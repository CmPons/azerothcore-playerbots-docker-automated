#include "ArenaRosterGear.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "PlayerbotFactory.h"
#include "Mgr/Item/StatsWeightCalculator.h"
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

// Best (endgame-correct) armor subclass per class — a level-80 hunter/shaman wears mail,
// never "trains down" to leather; the season sets exist in exactly these subclasses per class.
uint8 BestArmorSubclassFor(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT: return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:       return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_ROGUE:
        case CLASS_DRUID:        return ITEM_SUBCLASS_ARMOR_LEATHER;
        default:                 return ITEM_SUBCLASS_ARMOR_CLOTH;  // mage/warlock/priest
    }
}

// Body-armor inventory types where the armor-subclass filter applies. Cloaks are cloth for
// everyone and neck/finger/trinket/weapons are subclass-misc, so they must NOT be filtered.
bool IsBodyArmorInvType(uint32 invType)
{
    switch (invType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
        case INVTYPE_LEGS:
        case INVTYPE_HANDS:
        case INVTYPE_WAIST:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
            return true;
        default:
            return false;
    }
}

// Raw (bot-independent) season-set fetch, cached per (season, invType): item_template is
// immutable at runtime, and poolinit gears 60 bots back-to-back — uncached, that's ~2.4k
// synchronous world-DB queries on the world thread. Ordering inside one cache entry matches
// the old per-bot query exactly: season prefix first, then the Hateful fallback, each block
// best item level first. World-thread-only (no locking), like every caller in this module.
// NOTE: an empty result is latched for the process lifetime like any other — intended for the
// immutable item_template (a legitimately empty slot/season combo shouldn't be re-queried per
// bot), but it also means a transient DB error at first touch sticks until restart.
std::vector<uint32> const& RawCandidatesFor(uint32 invType, ArenaSeason season)
{
    static std::unordered_map<uint32, std::vector<uint32>> cache;
    uint32 key = (uint32(season) << 8) | invType;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    std::vector<uint32> out;
    std::vector<char const*> prefixes = { ArenaRosterGear::SeasonPrefix(season) };
    if (season <= SEASON_DEADLY)
        prefixes.push_back("Hateful Gladiator");
    for (char const* prefix : prefixes)
    {
        // ItemLevel >= 190 excludes legacy items that share the naming (classic "Savage
        // Gladiator Chain" at ilvl 57, TBC leftovers at ilvl 70) — verified in item_template.
        QueryResult r = WorldDatabase.Query(
            "SELECT entry FROM item_template WHERE name LIKE '{}%' AND InventoryType = {} "
            "AND ItemLevel >= 190 ORDER BY ItemLevel DESC", prefix, invType);
        if (!r)
            continue;
        do { out.push_back(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
    }
    return cache.emplace(key, std::move(out)).first->second;
}

// Season-set item entries of one inventory type this bot can actually use, best item level
// first. Seasons <= Deadly also fall back to the "Hateful Gladiator" (S5 off-set) pieces for
// slots the main set doesn't cover (belt/boots/bracers/rings...). Per-bot filters run here,
// over the cached raw list.
std::vector<uint32> CandidatesFor(Player* bot, uint32 invType, ArenaSeason season)
{
    std::vector<uint32> out;
    uint32 classMask = 1u << (bot->getClass() - 1);
    for (uint32 entry : RawCandidatesFor(invType, season))
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto)
            continue;
        if (!(proto->AllowableClass & classMask))
            continue;
        if (proto->Class == ITEM_CLASS_ARMOR && IsBodyArmorInvType(invType) &&
            proto->SubClass != BestArmorSubclassFor(bot->getClass()))
            continue;
        if (bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;
        out.push_back(entry);
    }
    return out;
}

// No season-prefixed PvP trinkets exist (verified: InventoryType 12 returns zero rows for every
// Gladiator prefix) — WotLK's CC-break trinket is the faction "Medallion of the ..." line, in
// ilvl 128/200/226/264 versions. Cap by the season's item level and let CanUseItem pick the
// faction/level-legal one (AllowableRace masks are per-faction).
std::vector<uint32> TrinketCandidatesFor(Player* bot, ArenaSeason season)
{
    // Same cache rationale as RawCandidatesFor: the query is bot-independent (per season);
    // only the CanUseItem faction/level filter is per-bot.
    static std::unordered_map<uint8, std::vector<uint32>> cache;
    auto it = cache.find(uint8(season));
    if (it == cache.end())
    {
        static constexpr uint32 seasonIlvlCap[] = { 200, 213, 232, 245, 264 };
        static_assert(std::size(seasonIlvlCap) == SEASON_WRATHFUL + 1,
                      "seasonIlvlCap must have one entry per ArenaSeason");
        std::vector<uint32> raw;
        QueryResult r = WorldDatabase.Query(
            "SELECT entry FROM item_template WHERE name IN "
            "('Medallion of the Alliance', 'Medallion of the Horde') AND InventoryType = {} "
            "AND ItemLevel <= {} ORDER BY ItemLevel DESC", uint32(INVTYPE_TRINKET), seasonIlvlCap[season]);
        if (r) do { raw.push_back(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
        it = cache.emplace(uint8(season), std::move(raw)).first;
    }

    std::vector<uint32> out;
    for (uint32 entry : it->second)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (proto && bot->CanUseItem(proto) == EQUIP_ERR_OK)
            out.push_back(entry);
    }
    return out;
}

// Probe + equip one item entry via the core's own leak-free pair: CanEquipNewItem creates a
// temporary Item internally and deletes it itself (PlayerStorage.cpp:1884), EquipNewItem then
// creates the real one only on success — no manual Item cleanup path exists to get wrong.
// (PlayerbotFactory::CanEquipUnseenItem + EquipNewItem is the same idiom, hand-rolled.)
bool EquipEntry(Player* bot, uint32 entry)
{
    uint16 dest = 0;
    if (bot->CanEquipNewItem(NULL_SLOT, dest, entry, false) != EQUIP_ERR_OK)
        return false;
    return bot->EquipNewItem(dest, entry, true) != nullptr;
}

} // namespace

namespace ArenaRosterGear
{

ArenaSeason SeasonForItemLevel(float avgIlvl)
{
    if (avgIlvl < 206.f) return SEASON_SAVAGE;
    if (avgIlvl < 226.f) return SEASON_DEADLY;
    if (avgIlvl < 245.f) return SEASON_FURIOUS;
    if (avgIlvl < 258.f) return SEASON_RELENTLESS;
    return SEASON_WRATHFUL;
}

char const* SeasonPrefix(ArenaSeason season)
{
    switch (season)
    {
        case SEASON_SAVAGE:     return "Savage Gladiator";
        case SEASON_DEADLY:     return "Deadly Gladiator";
        case SEASON_FURIOUS:    return "Furious Gladiator";
        case SEASON_RELENTLESS: return "Relentless Gladiator";
        case SEASON_WRATHFUL:   return "Wrathful Gladiator";
    }
    return "Deadly Gladiator";
}

bool EquipSeason(Player* bot, uint8_t specTab, ArenaSeason season)
{
    if (!bot)
        return false;
    // Gate BEFORE the strip, and at 80 (not 70): the ilvl >= 190 candidate floor means every
    // item this engine can produce requires level 80 — a lower gate would let a 70-79 bot get
    // fully stripped, then fail every equip probe and end up naked.
    if (!bot->IsInWorld() || bot->GetLevel() < 80)
    {
        LOG_WARN("playerbots", "[ArenaRoster] Not gearing {}: {} (level {}).", bot->GetName(),
                 !bot->IsInWorld() ? "not in world" : "below level 80", bot->GetLevel());
        return false;
    }

    // Re-gear from scratch (factory second_chance style): destroy everything equipped except
    // the cosmetic shirt/tabard, then fill each slot with the best usable season piece.
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
            continue;
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
    }

    uint32 equipped = 0;

    // Spec-aware scorer shared by the armor/jewelry AND weapon paths. Constructed before any
    // candidate is picked, and used only AFTER HandleSync forced the pinned talents (the
    // calculator weights by the bot's active talent tab; SetPvpSpec adds resilience weighting).
    // Scoring is mandatory even for armor: the season sets ship multiple same-ilvl, same-slot,
    // class-legal stat variants (paladin "Ornamented" healer plate vs "Scaled" melee plate,
    // shaman Ringmail vs Mail, priest Mooncloth vs Satin, AGI- vs AP-profile rings) — plain
    // ItemLevel DESC order would hand a Ret paladin the Holy chest on an alphabetical accident.
    StatsWeightCalculator calc(bot);
    calc.SetPvpSpec(true);

    // All usable season candidates across the given inventory types, best spec-score first.
    auto rankedCandidates = [&calc, bot, season](std::initializer_list<uint32> invTypes)
    {
        std::vector<std::pair<float, uint32>> out;
        for (uint32 invType : invTypes)
            for (uint32 entry : CandidatesFor(bot, invType, season))
                out.push_back({ calc.CalculateItem(entry), entry });
        std::sort(out.begin(), out.end(),
                  [](std::pair<float, uint32> const& a, std::pair<float, uint32> const& b)
                  { return a.first > b.first; });
        return out;
    };

    // Armor + jewelry: best-scored candidate per slot, equip-probe fallback down the ranking.
    // CHEST and ROBE fill the same equipment slot, so they are ranked as ONE pool — a class
    // whose set piece is a robe must not lose the slot to a lower-scored chest-typed off-piece,
    // and vice versa. Failures are logged (HandleSync's warning message promises the log is
    // diagnosable): an EMPTY candidate pool means the season/naming assumption broke for this
    // class+slot — loud WARN; a non-empty pool where every probe failed is usually benign
    // slot contention — quiet DEBUG.
    auto equipBestOf = [&](char const* slotName, std::initializer_list<uint32> invTypes)
    {
        std::vector<std::pair<float, uint32>> ranked = rankedCandidates(invTypes);
        if (ranked.empty())
        {
            LOG_WARN("playerbots", "[ArenaRoster] {}: no usable {} candidate in {} gear.",
                     bot->GetName(), slotName, SeasonPrefix(season));
            return;
        }
        for (auto const& [score, entry] : ranked)
            if (EquipEntry(bot, entry))
            {
                ++equipped;
                return;
            }
        LOG_DEBUG("playerbots", "[ArenaRoster] {}: all {} {} candidate(s) failed the equip probe.",
                  bot->GetName(), ranked.size(), slotName);
    };
    equipBestOf("head",      { INVTYPE_HEAD });
    equipBestOf("shoulders", { INVTYPE_SHOULDERS });
    equipBestOf("chest",     { INVTYPE_CHEST, INVTYPE_ROBE });
    equipBestOf("legs",      { INVTYPE_LEGS });
    equipBestOf("hands",     { INVTYPE_HANDS });
    equipBestOf("waist",     { INVTYPE_WAIST });
    equipBestOf("feet",      { INVTYPE_FEET });
    equipBestOf("wrists",    { INVTYPE_WRISTS });
    equipBestOf("cloak",     { INVTYPE_CLOAK });
    equipBestOf("neck",      { INVTYPE_NECK });

    // Fingers: the two best-scored distinct rings (the season ships AGI- and AP-profile
    // "Band of ..." variants — score and take the top two, not the first two by name).
    uint32 fingersWanted = 2;
    for (auto const& [score, entry] : rankedCandidates({ INVTYPE_FINGER }))
    {
        if (EquipEntry(bot, entry))
        {
            ++equipped;
            if (--fingersWanted == 0)
                break;
        }
    }
    if (fingersWanted)
        LOG_WARN("playerbots", "[ArenaRoster] {}: {} ring slot(s) left unfilled in {} gear.",
                 bot->GetName(), fingersWanted, SeasonPrefix(season));

    // Trinkets: the faction CC-break medallion (unique — the second slot legitimately stays
    // open for whatever the bot picks up later; equipping two medallions is impossible anyway).
    {
        std::vector<uint32> medallions = TrinketCandidatesFor(bot, season);
        if (medallions.empty())
            LOG_WARN("playerbots", "[ArenaRoster] {}: no usable CC-break medallion found.",
                     bot->GetName());
        bool gotMedallion = false;
        for (uint32 entry : medallions)
            if (EquipEntry(bot, entry))
            {
                ++equipped;
                gotMedallion = true;
                break;
            }
        if (!medallions.empty() && !gotMedallion)
            LOG_DEBUG("playerbots", "[ArenaRoster] {}: all {} medallion candidate(s) failed the "
                      "equip probe.", bot->GetName(), medallions.size());
    }

    // Weapons / ranged / relic. We deliberately do NOT call PlayerbotFactory::InitEquipment for
    // this: read at PlayerbotFactory.cpp:2103-2392, incremental=false unconditionally unequips
    // the old item and equips its own best random-loot pick in EVERY slot — it would replace the
    // set pieces above, not fill around them. Season weapons are named "<Season> Gladiator's
    // <weapon>" so the same prefix query covers them; rank all candidates with the shared
    // spec-aware scorer and equip greedily best-first. A pick blocked by an earlier one
    // (occupied main hand, 2H vs off-hand) just fails the equip probe and falls through, which
    // naturally yields 2H for arms, main+off for rogues, weapon+shield/held for casters, and a
    // class-legal relic/wand/bow in the ranged slot.
    for (auto const& [score, entry] : rankedCandidates({
             INVTYPE_WEAPON, INVTYPE_2HWEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_WEAPONOFFHAND,
             INVTYPE_SHIELD, INVTYPE_HOLDABLE, INVTYPE_RANGED, INVTYPE_THROWN, INVTYPE_RANGEDRIGHT,
             INVTYPE_RELIC }))
    {
        if (score <= 0.f)   // sorted descending: nothing useful past this point
            break;
        if (EquipEntry(bot, entry))
            ++equipped;
    }
    bot->AutoUnequipOffhandIfNeed();

    // Enchant/gem pass over the equipped set (public factory entry point), plus ammo so
    // hunters can actually shoot. Both operate on the CURRENT gear — they don't replace it.
    PlayerbotFactory factory(bot, bot->GetLevel(), ITEM_QUALITY_EPIC, 0);
    factory.ApplyEnchantAndGemsNew();
    factory.InitAmmo();

    LOG_INFO("playerbots", "[ArenaRoster] Geared {} (spec tab {}) in {} gear: {} piece(s) equipped.",
             bot->GetName(), uint32(specTab), SeasonPrefix(season), equipped);
    return equipped >= 8;
}

} // namespace ArenaRosterGear
