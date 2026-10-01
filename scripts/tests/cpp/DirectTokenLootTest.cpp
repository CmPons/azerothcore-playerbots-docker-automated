// Offline fixture: production loot hook, scoring bodies and flat stat/aura collector.
// World/DBC lookups are doubles; item data is a read-only installed T4 snapshot.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using int32 = int32_t;
using Milliseconds = std::chrono::milliseconds;
using namespace std::chrono_literals;
// PRODUCTION_ENUMS
constexpr uint8 MAX_CLASSES = 12;
constexpr uint32 CLASSMASK_ALL_PLAYABLE = 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 1024;
constexpr int MAX_COMBAT_RATING = 25;
constexpr int MAX_ITEM_PROTO_SPELLS = 5;
constexpr int MAX_GEM_SOCKETS = 3;
constexpr int SOCK_ENCHANTMENT_SLOT = 2;
constexpr int EQUIPMENT_SLOT_END = 19;
constexpr uint8 NULL_SLOT = 255;
constexpr int EQUIP_ERR_OK = 0;
constexpr int ITEM_FLAG_MULTI_DROP = 1;
constexpr int ITEM_FLAGS_CU_FOLLOW_LOOT_RULES = 2;
#define ITEM_SUBCLASS_MASK_WEAPON_RANGED                                                                               \
    ((1 << ITEM_SUBCLASS_WEAPON_BOW) | (1 << ITEM_SUBCLASS_WEAPON_GUN) | (1 << ITEM_SUBCLASS_WEAPON_CROSSBOW))
struct _ItemStat
{
    uint32 ItemStatType = 0;
    int32 ItemStatValue = 0;
};
struct ItemTemplate
{
    uint32 ItemId = 0, Class = 4, SubClass = 4, Quality = 4, InventoryType = 7;
    uint32 AllowableClass = CLASSMASK_ALL_PLAYABLE, ItemLevel = 120, RequiredLevel = 70;
    uint32 RequiredHonorRank = 0, StatsCount = 0, Armor = 0, Block = 0, socketBonus = 0, ItemSet = 0;
    uint32 skill = 0;
    bool usable = true;
    std::string Name1;
    std::array<_ItemStat, 10> ItemStat{};
    struct Spell
    {
        int32 SpellId = 0;
        uint32 SpellTrigger = 1;
        float SpellPPMRate = 0;
        int32 SpellCooldown = 0;
    };
    std::array<Spell, 5> Spells{};
    struct SocketEntry
    {
        uint8 Color = 0;
    };
    std::array<SocketEntry, 3> Socket{};
    struct DamageEntry
    {
        float DamageMin = 0, DamageMax = 0;
    };
    std::array<DamageEntry, 2> Damage{};
    uint32 Delay = 2000;
    bool IsRangedWeapon() const
    {
        return false;
    }
    bool IsWeapon() const
    {
        return Class == ITEM_CLASS_WEAPON;
    }
    bool HasFlag(int) const
    {
        return true;
    }
    bool HasFlagCu(int) const
    {
        return true;
    }
    uint32 GetSkill() const
    {
        return skill;
    }
};
struct SpellEffectInfo
{
    uint32 Effect, ApplyAuraName;
    int32 BasePoints, DieSides, MiscValue;
    uint32 TriggerSpell;
};
struct SpellItemEnchantmentEntry
{
    std::array<uint32, 3> type{}, amount{}, spellid{};
};
struct ItemExtendedCostEntry
{
    uint32 reqhonorpoints = 0, reqarenapoints = 0, reqpersonalarenarating = 0;
};
template <class T> struct Store
{
    std::map<uint32, T> rows;
    T const* LookupEntry(uint32 id) const
    {
        auto i = rows.find(id);
        return i == rows.end() ? nullptr : &i->second;
    }
};
Store<SpellItemEnchantmentEntry> sSpellItemEnchantmentStore;
Store<ItemExtendedCostEntry> sItemExtendedCostStore;
std::map<uint32, std::vector<SpellEffectInfo>> spells;
struct Quest
{
    uint32 classes = 0;
    bool pvp = false;
    uint32 GetRequiredClasses() const
    {
        return classes;
    }
    bool IsPVPQuest() const
    {
        return pvp;
    }
};
struct ObjectMgr
{
    std::map<uint32, ItemTemplate> items;
    std::map<uint32, Quest> quests;
    ItemTemplate const* GetItemTemplate(uint32 id)
    {
        auto i = items.find(id);
        return i == items.end() ? nullptr : &i->second;
    }
    auto* GetItemTemplateStore()
    {
        return &items;
    }
    Quest const* GetQuestTemplate(uint32 id)
    {
        auto i = quests.find(id);
        return i == quests.end() ? nullptr : &i->second;
    }
} objects;
auto* sObjectMgr = &objects;
struct Group;
struct Player
{
    uint8 cls = 1, tab = 2;
    uint32 level = 70, map = 1;
    bool inWorld = true, tank = false, heal = false, caster = false, melee = true;
    bool aura = false, skilled = true;
    float rating = 0;
    Group* group = nullptr;
    uint8 getClass() const
    {
        return cls;
    }
    uint32 getClassMask() const
    {
        return 1u << (cls - 1);
    }
    uint32 GetLevel() const
    {
        return level;
    }
    bool IsInWorld() const
    {
        return inWorld;
    }
    bool IsInMap(Player const* other) const
    {
        return map == other->map;
    }
    Group* GetGroup() const
    {
        return group;
    }
    int CanUseItem(ItemTemplate const* p) const
    {
        return p->usable && level >= p->RequiredLevel ? 0 : 1;
    }
    bool HasSkill(uint32) const
    {
        return skilled;
    }
    bool CanDualWield() const
    {
        return true;
    }
    bool CanTitanGrip() const
    {
        return false;
    }
    bool HasAura(uint32) const
    {
        return aura;
    }
    bool HasSpell(uint32) const
    {
        return true;
    }
    float GetRatingBonusValue(uint32) const
    {
        return rating;
    }
};
struct GroupReference
{
    Player* player = nullptr;
    GroupReference* following = nullptr;
    Player* GetSource()
    {
        return player;
    }
    GroupReference* next()
    {
        return following;
    }
};
struct Group
{
    std::vector<GroupReference> members;
    void Set(std::vector<Player*> const& players)
    {
        members.clear();
        members.resize(players.size());
        for (size_t i = 0; i < players.size(); ++i)
        {
            members[i] = {players[i], i + 1 < players.size() ? &members[i + 1] : nullptr};
            if (players[i])
                players[i]->group = this;
        }
    }
    GroupReference* GetFirstMember()
    {
        return members.empty() ? nullptr : &members.front();
    }
};
struct AiFactory
{
    static uint8 GetPlayerSpecTab(Player* p)
    {
        return p->tab;
    }
};
struct PlayerbotAI
{
    static bool IsHeal(Player* p)
    {
        return p->heal;
    }
    static bool IsCaster(Player* p)
    {
        return p->caster;
    }
    static bool IsTank(Player* p)
    {
        return p->tank;
    }
    static bool IsMelee(Player* p)
    {
        return p->melee;
    }
    static float GetItemScoreMultiplier(ItemQualities);
};
struct PlayerbotFactory
{
    static uint32 CalcMixedGearScore(uint32, uint32);
};
struct
{
    bool preferredSpecWeapons = false;
} sPlayerbotAIConfig;
class StatsCollector
{
  public:
    StatsCollector(CollectorType t, int32) : type_(t)
    {
        Reset();
    }
    void Reset();
    void CollectItemStats(ItemTemplate const*);
    void CollectByItemStatType(uint32, int32);
    void HandleApplyAura(SpellEffectInfo const&, float, bool, Milliseconds);
    float AverageValue(SpellEffectInfo const&);
    void CollectSpellStats(uint32 id, float multiplier, Milliseconds cooldown)
    {
        (void)cooldown;
        if (!id)
            return;
        for (auto const& e : spells.at(id))
        {
            assert(e.Effect == SPELL_EFFECT_APPLY_AURA);
            // Eligible T4 pieces use flat, permanent, positive equip effects.
            if (!(e.ApplyAuraName == SPELL_AURA_MOD_DAMAGE_DONE || e.ApplyAuraName == SPELL_AURA_MOD_HEALING_DONE ||
                  e.ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER ||
                  e.ApplyAuraName == SPELL_AURA_MOD_RANGED_ATTACK_POWER ||
                  e.ApplyAuraName == SPELL_AURA_MOD_SHIELD_BLOCKVALUE ||
                  e.ApplyAuraName == SPELL_AURA_MOD_POWER_REGEN || e.ApplyAuraName == SPELL_AURA_MOD_TARGET_RESISTANCE))
            {
                std::cerr << "Unsupported fixture spell " << id << " aura " << e.ApplyAuraName << '\n';
                std::abort();
            }
            HandleApplyAura(e, multiplier, false, 0ms);
        }
    }
    void CollectEnchantStats(SpellItemEnchantmentEntry const* e)
    {
        for (int i = 0; i < 3; ++i)
        {
            assert(e->type[i] == 0 || e->type[i] == 5);
            if (e->type[i] == 5)
                CollectByItemStatType(e->spellid[i], e->amount[i]);
        }
    }
    float stats[STATS_TYPE_MAX]{};
    CollectorType type_;
};
// PRODUCTION_SCORER_HEADER
int overflowCalls = 0, setCalls = 0;
void StatsWeightCalculator::ApplyOverflowPenalty(Player*)
{
    ++overflowCalls;
}
void StatsWeightCalculator::CalculateItemSetMod(Player*, ItemTemplate const*)
{
    ++setCalls;
}
void StatsWeightCalculator::CalculateRandomProperty(int32, uint32)
{
    assert(false);
}
float StatsWeightCalculator::ApplyPreferredSpecWeapons(ItemTemplate const*, int32)
{
    assert(false);
    return 0;
}
// PRODUCTION_SCORER
// PRODUCTION_CANDIDATE_HEADER
struct TokenItemResolver
{
    static inline std::map<uint32, std::vector<TokenRewardCandidate>> rewards;
    static std::vector<TokenRewardCandidate> FindTokenRewards(uint32 id)
    {
        return rewards[id];
    }
};
struct Config
{
    bool enabled = true;
    uint32 mode = 1;
    template <class T> T GetOption(std::string const& key, T fallback)
    {
        if (key.ends_with(".Enable"))
            return T(enabled);
        if (key.ends_with(".Mode"))
            return T(mode);
        return fallback;
    }
} config;
auto* sConfigMgr = &config;
std::vector<uint32> rolls;
std::vector<std::pair<uint32, uint32>> rollBounds;
uint32 urand(uint32 low, uint32 high)
{
    rollBounds.push_back({low, high});
    uint32 value = rolls.empty() ? low : rolls.front();
    if (!rolls.empty())
        rolls.erase(rolls.begin());
    assert(low <= value && value <= high);
    return value;
}
struct LootStore
{
};
LootStore LootTemplates_Creature, LootTemplates_Gameobject, otherStore;
struct LootTemplate
{
};
struct LootItem
{
    uint32 itemid = 0, count = 1;
    bool is_looted = false, needs_quest = false;
    uint32 randomSuffix = 0, randomPropertyId = 0;
    bool freeforall = false, follow_loot_rules = false;
};
struct Loot
{
    std::vector<LootItem> items;
};
uint32 GenerateEnchSuffixFactor(uint32)
{
    return 17;
}
struct Item
{
    static uint32 GenerateItemRandomPropertyId(uint32)
    {
        return 19;
    }
};
constexpr int MISCHOOK_ON_AFTER_LOOT_TEMPLATE_PROCESS = 0;
struct MiscScript
{
    MiscScript(char const*, std::initializer_list<int>)
    {
    }
    virtual ~MiscScript() = default;
    virtual void OnAfterLootTemplateProcess(Loot*, LootTemplate const*, LootStore const&, Player*, bool, bool, uint16)
    {
    }
};
#define LOG_INFO(...) ((void)0)
// PRODUCTION_LOOT
// HISTORICAL_POOL
void LoadFixture()
{
    // FIXTURE_DATA
}

void SelectionTests()
{
    Player prot;
    Player fury;
    fury.tab = 1;
    Player arms;
    arms.tab = 0;
    Player priest;
    priest.cls = 5;
    priest.tab = 0;
    Player druid;
    druid.cls = 11;
    druid.tab = 2;
    auto rewards = FindDirectRewards(29767, 3);
    assert(BestSpecRewards(rewards, {&prot}, 1) == std::vector<uint32>{29015});
    assert(BestSpecRewards(rewards, {&fury}, 1) == std::vector<uint32>{29022});
    assert((BestSpecRewards(rewards, {&prot, &fury}, 1) == std::vector<uint32>{29015, 29022}));
    assert((BestSpecRewards(rewards, {&prot, &fury, &fury, &arms, &priest}, 1) ==
            BestSpecRewards(rewards, {&prot, &fury}, 1)));
    assert(BestSpecRewards(rewards, {&fury, &arms}, 1) == std::vector<uint32>{29022});
    assert(BestSpecRewards(rewards, {&priest, &druid}, 1).empty());
    for (uint32 i = 0; i < 3; ++i)
    {
        rolls = {i};
        rollBounds.clear();
        assert((PickRewardClass(rewards) == std::array<uint8, 3>{1, 5, 11}[i]));
        assert((rollBounds == std::vector<std::pair<uint32, uint32>>{{0, 2}}));
    }
    // More vendor paths or warrior pieces cannot alter class-roll cardinality.
    auto copies = TokenItemResolver::rewards[29767];
    TokenItemResolver::rewards[29767].insert(TokenItemResolver::rewards[29767].end(), copies.begin(), copies.end());
    assert(FindDirectRewards(29767, 3).size() == rewards.size());
    rolls = {2};
    assert(PickRewardClass(FindDirectRewards(29767, 3)) == 11);
    TokenItemResolver::rewards[29767] = copies;
    // Duplicate avoidance operates only among winners; it never admits inferior/PvP gear.
    assert(PickReward({29015}, {29015}) == 29015);
    assert(PickReward({29015, 29022}, {29015}) == 29022);
    assert(PickReward({}, {}) == 0);
    assert(PickRewardClass({}) == 0);
}

void PvpTests()
{
    auto old = OldFindDirectRewardItemIds(29767, 3);
    assert(std::find(old.begin(), old.end(), 24547) != old.end());
    auto rewards = FindDirectRewards(29767, 3);
    assert(std::none_of(rewards.begin(), rewards.end(), [](auto r) { return r.itemId == 24547; }));
    for (auto const& [token, paths] : TokenItemResolver::rewards)
    {
        (void)paths;
        for (auto const& r : FindDirectRewards(token, 3))
            assert(objects.items.at(r.itemId).Name1.find("Gladiator") == std::string::npos);
    }
    ItemTemplate item = objects.items.at(29015);
    TokenRewardCandidate c;
    assert(!IsPvpReward(&item, c));
    item.RequiredHonorRank = 1;
    assert(IsPvpReward(&item, c));
    item.RequiredHonorRank = 0;
    c.extendedCost = 99999;
    assert(IsPvpReward(&item, c));
    sItemExtendedCostStore.rows[99999] = {1, 0, 0};
    assert(IsPvpReward(&item, c));
    sItemExtendedCostStore.rows[99999] = {0, 1, 0};
    assert(IsPvpReward(&item, c));
    sItemExtendedCostStore.rows[99999] = {0, 0, 1};
    assert(IsPvpReward(&item, c));
    c.extendedCost = 0;
    c.questId = 1;
    assert(IsPvpReward(&item, c));
    objects.quests[1] = {1, true};
    assert(IsPvpReward(&item, c));
    objects.quests[1].pvp = false;
    assert(!IsPvpReward(&item, c));
}

void ContextTests()
{
    Player owner, other, remote, offline;
    remote.map = 2;
    offline.inWorld = false;
    Group group;
    group.Set({&owner, &other, &remote, &offline, nullptr});
    assert(PresentPlayers(&owner, false) == (std::vector<Player*>{&owner, &other}));
    assert(PresentPlayers(&owner, true) == std::vector<Player*>{&owner});
    assert(PresentPlayers(nullptr, false).empty());
    assert(PresentPlayers(&offline, false).empty());
    owner.group = nullptr;
    assert(PresentPlayers(&owner, false) == std::vector<Player*>{&owner});
    owner.level = 69;
    assert(BestSpecRewards(FindDirectRewards(29767, 3), {&owner}, 1).empty());
    owner.level = 70;
    owner.tab = 255;
    assert(BestSpecRewards(FindDirectRewards(29767, 3), {&owner}, 1).empty());
    owner.tab = 2;
    objects.items[29015].skill = 1;
    owner.skilled = false;
    auto r = BestSpecRewards(FindDirectRewards(29767, 3), {&owner}, 1);
    assert(std::find(r.begin(), r.end(), 29015) == r.end());
    objects.items[29015].skill = 0;
}

void HookTests()
{
    Player owner;
    PlayerbotsDirectTokenLootScript script;
    auto run = [&](Loot& loot, LootStore const& store = LootTemplates_Creature, Player* p = nullptr)
    { script.OnAfterLootTemplateProcess(&loot, nullptr, store, p ? p : &owner, false, false, 1); };
    Loot loot{{{29767}, {29015}, {29767, 2}, {29767, 1, true}, {29767, 1, false, true}}};
    rolls = {0, 0};
    run(loot);
    assert(loot.items.size() == 5 && loot.items[0].itemid == 29015);
    assert(loot.items[0].randomSuffix == 17 && loot.items[0].randomPropertyId == 19);
    assert(loot.items[0].freeforall && loot.items[0].follow_loot_rules);
    for (int i = 2; i < 5; ++i)
        assert(loot.items[i].itemid == 29767);
    loot = {{{29767}}};
    rolls = {1};
    run(loot); // absent priest: retain token, no class reroll
    assert(loot.items[0].itemid == 29767 && rolls.empty());
    rolls = {0, 0};
    run(loot, LootTemplates_Gameobject);
    assert(loot.items[0].itemid == 29015);
    loot = {{{29767}}};
    rolls.clear();
    run(loot, otherStore);
    assert(loot.items[0].itemid == 29767);
    config.enabled = false;
    run(loot);
    assert(loot.items[0].itemid == 29767);
    config.enabled = true;
    config.mode = 2;
    run(loot);
    assert(loot.items[0].itemid == 29767);
    config.mode = 1;
    script.OnAfterLootTemplateProcess(&loot, nullptr, LootTemplates_Creature, nullptr, false, false, 1);
    assert(loot.items[0].itemid == 29767);
    script.OnAfterLootTemplateProcess(nullptr, nullptr, LootTemplates_Creature, &owner, false, false, 1);
}

void QuestAndSafetyTests()
{
    ItemTemplate token;
    token.ItemId = 100;
    objects.items[100] = token;
    ItemTemplate reward;
    reward.ItemId = 101;
    objects.items[101] = reward;
    TokenRewardCandidate c;
    c.tokenItemId = 100;
    c.rewardItemId = 101;
    c.questId = 1000;
    objects.quests[1000] = {1, false};
    TokenItemResolver::rewards[100] = {c};
    auto r = FindDirectRewards(100, 3);
    assert(r.size() == 1 && r[0].classes == 1);
    c.questId = 1001;
    objects.quests[1001] = {16, false};
    TokenItemResolver::rewards[100].push_back(c);
    r = FindDirectRewards(100, 3);
    assert(r.size() == 1 && r[0].classes == 17);
    objects.items[100].AllowableClass = 1;
    r = FindDirectRewards(100, 3);
    assert(r[0].classes == 1);
    objects.items[101].InventoryType = INVTYPE_NON_EQUIP;
    assert(FindDirectRewards(100, 3).empty());
    objects.items[101].InventoryType = 7;
    objects.items[101].Quality = 2;
    assert(FindDirectRewards(100, 3).empty());
    assert(FindDirectRewards(999999, 3).empty());
    objects.items[101].Quality = 4;
    objects.items[101].Class = ITEM_CLASS_CONSUMABLE;
    assert(FindDirectRewards(100, 3).empty());
    objects.items[101].Class = ITEM_CLASS_ARMOR;
    // Equal scores have stable ID tie-breaking; negative finite scores remain rankable.
    objects.items[102] = objects.items[101];
    objects.items[102].ItemId = 102;
    Player owner;
    assert(BestSpecRewards({{102, 1}, {101, 1}}, {&owner}, 1) == std::vector<uint32>{101});
    objects.items[101].StatsCount = 1;
    objects.items[101].ItemStat[0] = {ITEM_MOD_SPELL_POWER, 100};
    assert(BestSpecRewards({{101, 1}}, {&owner}, 1) == std::vector<uint32>{101});
    // Invalid weapon data must not inject NaN/Inf into selection.
    objects.items[103] = objects.items[102];
    objects.items[103].ItemId = 103;
    objects.items[103].Class = ITEM_CLASS_WEAPON;
    objects.items[103].Delay = 0;
    assert(BestSpecRewards({{103, 1}}, {&owner}, 1).empty());
    objects.items[103].Damage[0].DamageMin = 1;
    assert(BestSpecRewards({{103, 1}}, {&owner}, 1).empty());
}

void ScoringTests()
{
    Player owner;
    float before = StatsWeightCalculator(&owner, true).CalculateItem(29015);
    owner.tank = true;
    owner.heal = true;
    owner.caster = true;
    owner.aura = true;
    owner.rating = 100;
    float after = StatsWeightCalculator(&owner, true).CalculateItem(29015);
    assert(before == after && overflowCalls == 0 && setCalls == 0);
    StatsWeightCalculator ordinary(&owner);
    ordinary.CalculateItem(29015);
    assert(overflowCalls == 1 && setCalls == 1); // existing equipment mode defaults preserved
    Player feral;
    feral.cls = 11;
    feral.tab = 1;
    auto cat = BestSpecRewards(FindDirectRewards(29767, 3), {&feral}, 11);
    feral.tank = true;
    feral.aura = true;
    assert(cat == BestSpecRewards(FindDirectRewards(29767, 3), {&feral}, 11));
}

void TierMatrixTests()
{
    // All 5 slots, all 27 pre-Wrath class talent tabs, using installed item stats/equip spells/socket bonuses.
    std::map<uint8, std::array<uint32, 3>> sets = {{1, {655, 655, 654}}, {2, {624, 625, 626}}, {3, {651, 651, 651}},
                                                   {4, {621, 621, 621}}, {5, {663, 663, 664}}, {7, {632, 633, 631}},
                                                   {8, {648, 648, 648}}, {9, {645, 645, 645}}, {11, {639, 640, 638}}};
    // Scoring is not a set-label classifier. These are the measured existing-weight
    // crossovers; keep them explicit rather than pretending all healer tiers win.
    std::map<std::tuple<uint8, uint8, uint32>, uint32> crossovers = {
        {{5, 0, 29758}, 29057},  {{5, 0, 29767}, 29059}, {{5, 1, 29758}, 29057}, {{5, 1, 29767}, 29059},
        {{7, 2, 29754}, 29033},  {{7, 2, 29760}, 29035}, {{7, 2, 29763}, 29037}, {{11, 2, 29758}, 29092},
        {{11, 2, 29764}, 29095}, {{11, 2, 29767}, 29094}};
    unsigned count = 0, crossed = 0;
    for (auto const& [cls, expected] : sets)
        for (uint8 tab = 0; tab < 3; ++tab)
            for (uint32 token = 29753; token <= 29767; ++token)
            {
                if (!(objects.items[token].AllowableClass & (1u << (cls - 1))))
                    continue;
                Player p;
                p.cls = cls;
                p.tab = tab;
                auto winners = BestSpecRewards(FindDirectRewards(token, 3), {&p}, cls);
                assert(winners.size() == 1);
                auto crossover = crossovers.find({cls, tab, token});
                if (crossover != crossovers.end())
                {
                    assert(winners[0] == crossover->second);
                    ++crossed;
                }
                else
                    assert(objects.items[winners[0]].ItemSet == expected[tab]);

                StatsWeightCalculator scorer(&p, true);
                float const bestScore = scorer.CalculateItem(winners[0]);
                for (auto reward : FindDirectRewards(token, 3))
                    if (reward.classes & p.getClassMask())
                        assert(scorer.CalculateItem(reward.itemId) <= bestScore);
                ++count;
            }
    assert(count == 135 && crossed == crossovers.size());
    std::cout << count << " real T4 class/spec/slot cases passed (" << crossed << " documented score crossovers)\n";
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    LoadFixture();
    std::string which = argv[1];
    if (which == "selection")
        SelectionTests();
    else if (which == "pvp")
        PvpTests();
    else if (which == "context")
        ContextTests();
    else if (which == "hook")
        HookTests();
    else if (which == "safety")
        QuestAndSafetyTests();
    else if (which == "scoring")
        ScoringTests();
    else if (which == "tier")
        TierMatrixTests();
    else
        assert(false);
    std::cout << which << " passed\n";
}
