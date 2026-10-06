// Production validation, item-usage and roll methods are injected by the Python harness.
// Item scores are doubles: this fixture tests eligibility/voting, not stat-weight accuracy.
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using int32 = int32_t;
using HolidayIds = uint32;
#define LOG_DEBUG(...) ((void)0)
// CONSTANTS
struct Player;
struct Group;
struct Guid
{
    uint32 value = 1297;
    uint32 GetCounter() const { return value; }
    bool operator<(Guid const& other) const { return value < other.value; }
};
using ObjectGuid = Guid;
struct ItemTemplate
{
    uint32 ItemId = 30189, Class = ITEM_CLASS_ARMOR, SubClass = 3, InventoryType = 10;
    uint32 Flags = 0, Flags2 = 0, Quality = ITEM_QUALITY_EPIC, Bonding = 1;
    uint32 AllowableClass = 64, AllowableRace = 1, RequiredSkill = 0, RequiredSkillRank = 0;
    uint32 RequiredSpell = 0, RequiredLevel = 70, HolidayId = 0;
    uint32 RequiredReputationFaction = 0, RequiredReputationRank = 0;
    uint32 ScalingStatDistribution = 0, ContainerSlots = 0, ItemSet = 0;
    uint32 proficiency = 413;
    uint8 slot = 9;
    float score = 220;
    bool HasFlag(uint32 value) const { return (Flags & value) != 0; }
    bool HasFlag2(uint32 value) const { return (Flags2 & value) != 0; }
};
std::map<uint32, ItemTemplate> templates;
struct Item
{
    ItemTemplate const* proto;
    Player* owner;
    bool m_lootGenerated = false;
    static inline unsigned allocations = 0, removals = 0;
    static Item* CreateItem(uint32 id, uint32, Player* owner, bool, uint32, bool)
    {
        ++allocations;
        return new Item{&templates.at(id), owner};
    }
    ItemTemplate const* GetTemplate() const { return proto; }
    uint32 GetEntry() const { return proto->ItemId; }
    uint32 GetCount() const { return 1; }
    uint8 GetSlot() const { return proto->slot; }
    uint32 GetSkill() const { return proto->proficiency; }
    bool IsBindedNotWith(Player const* player) const { return owner != player; }
    void RemoveFromUpdateQueueOf(Player*) { ++removals; }
    int32 GetInt32Value(uint32) const { return 0; }
    uint32 GetUInt32Value(uint32) const { return 50; }
};
using Bag = Item;
using ItemPosCountVec = std::vector<int>;
struct Player
{
    bool alive = true, dualWield = false, titanGrip = false, twoHand = false, disarmed = false;
    bool uniqueEquipped = false, storageFull = false, canUnequip = true;
    uint32 level = 70, classMask = 64, raceMask = 1, team = TEAM_ALLIANCE;
    InventoryResult countResult = EQUIP_ERR_OK, uniqueResult = EQUIP_ERR_OK;
    std::map<uint32, uint32> skills{{413, 1}};
    std::map<uint8, Item*> equipped;
    bool IsAlive() const { return alive; }
    uint32 GetLevel() const { return level; }
    uint32 GetTeamId(bool) const { return team; }
    uint32 getClassMask() const { return classMask; }
    uint32 getRaceMask() const { return raceMask; }
    uint32 getClass() const { return CLASS_SHAMAN; }
    Guid GetGUID() const { return {}; }
    Group* GetGroup() const;
    bool HasSpell(uint32) const { return false; }
    bool HasSkill(uint32 id) const { return GetSkillValue(id) != 0; }
    uint32 GetSkillValue(uint32 id) const
    {
        auto it = skills.find(id); return it == skills.end() ? 0 : it->second;
    }
    bool IsClass(uint32, uint32) const { return false; }
    uint32 GetReputationRank(uint32) const { return 0; }
    InventoryResult CanUseItem(Item*, bool not_loading = true) const;
    InventoryResult CanUseItem(ItemTemplate const*) const;
    InventoryResult BotCanUseItem(ItemTemplate const* proto) const { return CanUseItem(proto); }
    InventoryResult CanTakeMoreSimilarItems(Item*) const { return countResult; }
    uint32 GetAttackBySlot(uint8) const { return 0; }
    bool CanUseAttackType(uint32) const { return !disarmed; }
    bool CanDualWield() const { return dualWield; }
    bool CanTitanGrip() const { return titanGrip; }
    bool IsTwoHandUsed() const { return twoHand; }
    InventoryResult CanEquipUniqueItem(Item*, uint8) const { return uniqueResult; }
    Item* GetItemByPos(uint8, uint8 slot) const
    {
        auto it = equipped.find(slot); return it == equipped.end() ? nullptr : it->second;
    }
    Item* GetItemByPos(uint16 pos) const { return GetItemByPos(INVENTORY_SLOT_BAG_0, pos & 255); }
    InventoryResult CanUnequipItem(uint16, bool) const
    {
        return canUnequip ? EQUIP_ERR_OK : EQUIP_ERR_CANT_DO_RIGHT_NOW;
    }
    InventoryResult CanStoreItem(uint8, uint8, ItemPosCountVec&, Item*, bool) const
    {
        return storageFull ? EQUIP_ERR_INVENTORY_FULL : EQUIP_ERR_OK;
    }
    bool HasItemOrGemWithIdEquipped(uint32, uint32) const { return uniqueEquipped; }
    uint32 GetItemCount(uint32, bool) const { return 0; }
};
struct ScriptMgr
{
    bool equipAllowed = true, useAllowed = true;
    unsigned equipCalls = 0;
    bool OnPlayerCanEquipItem(Player*, uint8, uint16&, Item*, bool, bool notLoading)
    {
        ++equipCalls;
        assert(notLoading); // Valuation must NOT masquerade as character loading here.
        return equipAllowed;
    }
    bool OnPlayerCanUseItem(Player*, ItemTemplate const*, InventoryResult& result)
    {
        if (!useAllowed) result = EQUIP_ERR_CANT_DO_RIGHT_NOW;
        return useAllowed;
    }
} scripts;
ScriptMgr* sScriptMgr = &scripts;
struct ScalingStatDistributionEntry { uint32 MaxLevel = 80; };
struct ScalingStore
{
    ScalingStatDistributionEntry const* LookupEntry(uint32) { return nullptr; }
} sScalingStatDistributionStore;
bool IsHolidayActive(uint32) { return false; }
struct Group;
struct PlayerbotAI
{
    Player* bot;
    Group* group = nullptr;
    // EQUIP_DECLARATION
    uint8 FindEquipSlot(ItemTemplate const* proto, uint32, bool) const { return proto->slot; }
};
struct StatsWeightCalculator
{
    explicit StatsWeightCalculator(Player*) {}
    void SetItemSetBonus(bool) {}
    void SetOverflowPenalty(bool) {}
    void SetPvpSpec(bool) {}
    void SetItemSetComparisonSlot(uint8) {}
    float CalculateItem(uint32 id, int32) { return templates.at(id).score; }
};
struct RandomPlayerbotMgr { bool IsSpecPvp(uint32, uint32) const { return false; } } sRandomPlayerbotMgr;
struct RandomItemMgr
{
    bool CanEquipWeapon(ItemTemplate const*, uint32) const { return true; }
    bool CanEquipArmor(ItemTemplate const*, uint32, uint32) const { return true; }
} sRandomItemMgr;
struct Config
{
    float equipUpgradeThreshold = 1.1f;
    uint32 lootNeedRollLevel = 2;
    bool lootGreedRollLevel = false, lootRollDisenchant = false, lootRollRecipe = false;
} sPlayerbotAIConfig;
struct ItemUsageValue
{
    Player* bot;
    PlayerbotAI* botAI;
    ItemUsage QueryItemUsageForEquip(ItemTemplate const*, int32 = 0, Item* = nullptr);
    uint8 GetSmallestBagSize() const { return 16; }
    Item* CurrentItem(ItemTemplate const*) const { return nullptr; }
};
struct Event {};
struct Roll
{
    ObjectGuid itemGUID;
    uint32 itemid = 30189;
    int32 itemRandomPropId = 0, itemRandomSuffix = 0;
    std::map<Guid, RollVote> playerVote{{Guid{}, NOT_EMITED_YET}};
};
struct Group
{
    Roll roll;
    uint32 method = GROUP_LOOT;
    RollVote last = NOT_EMITED_YET;
    std::vector<Roll*> GetRolls() { return {&roll}; }
    uint32 GetLootMethod() const { return method; }
    void CountRollVote(Guid, Guid, uint8 vote) { last = static_cast<RollVote>(vote); }
};
struct ObjectMgr
{
    ItemTemplate const* GetItemTemplate(uint32 id) { return &templates.at(id); }
} objects;
ObjectMgr* sObjectMgr = &objects;
namespace TokenItemResolver
{
std::vector<int> FindTokenRewards(uint32) { return {}; }
}
struct StoreLootAction { static bool IsLootAllowed(uint32, PlayerbotAI*) { return true; } };
struct LootRollAction
{
    Player* bot;
    PlayerbotAI* botAI;
    Group* GetGroup() const { return botAI->group; }
    bool Execute(Event);
    RollVote CalculateRollVote(ItemTemplate const*, ItemUsage = ITEM_USAGE_NONE);
    ItemUsage Usage(std::string const& text)
    {
        ItemUsageValue value{bot, botAI};
        ItemUsage result = value.QueryItemUsageForEquip(&templates.at(std::stoul(text)));
        // For this class-usable tier item, the production outer evaluator falls back to KEEP.
        return result == ITEM_USAGE_NONE ? ITEM_USAGE_KEEP : result;
    }
};
// The production action obtains the group from Player; bridge the fixture's one group.
Group* activeGroup;
Group* Player::GetGroup() const { return activeGroup; }
#define AI_VALUE2(type, name, qualifier) Usage(qualifier)
#define GET_PLAYERBOT_AI(player) botAI
bool CanBotUseToken(ItemTemplate const*, Player*) { return true; }
bool RollUniqueCheck(ItemTemplate const*, Player*);

// PRODUCTION_METHODS

int main()
{
    Player player;
    PlayerbotAI ai{&player};
    ItemUsageValue value{&player, &ai};
    templates[30189] = ItemTemplate{};
    templates[28827] = ItemTemplate{};
    templates[28827].ItemId = 28827;
    templates[28827].score = 100;
    Item old{&templates.at(28827), &player};
    Item candidate{&templates.at(30189), &player};
    player.equipped[9] = &old;
    Group group;
    activeGroup = ai.group = &group;
    LootRollAction roll{&player, &ai};
    auto usage = [&] { return value.QueryItemUsageForEquip(candidate.GetTemplate()); };
    auto vote = [&]
    {
        group.roll.playerVote[Guid{}] = NOT_EMITED_YET;
        assert(roll.Execute({}));
        return group.last;
    };
    uint16 dest = 0;
    assert(usage() == ITEM_USAGE_REPLACE && vote() == NEED);
    player.alive = false;
    ItemUsage const deadUsage = usage();
    RollVote const deadVote = vote();
    std::cout << "Dead-bot upgrade usage=" << deadUsage << " vote=" << unsigned(deadVote) << std::endl;
    assert(deadUsage == ITEM_USAGE_REPLACE && deadVote == NEED);
    assert(player.CanUseItem(&candidate, true) == EQUIP_ERR_YOU_ARE_DEAD);
    assert(ai.CanEquipItem(NULL_SLOT, dest, &candidate, true, true) == EQUIP_ERR_YOU_ARE_DEAD);
    assert(player.equipped.at(9) == &old && !player.alive);

    // Death never promotes a sidegrade/non-upgrade into Need.
    templates[30189].score = 105;
    assert(usage() == ITEM_USAGE_NONE && vote() == PASS);
    templates[30189].score = 220;
    sPlayerbotAIConfig.lootNeedRollLevel = 0;
    assert(vote() == PASS);
    sPlayerbotAIConfig.lootNeedRollLevel = 1;
    assert(vote() == GREED);
    sPlayerbotAIConfig.lootNeedRollLevel = 2;
    group.method = MASTER_LOOT; assert(vote() == PASS);
    group.method = FREE_FOR_ALL; assert(vote() == PASS);
    group.method = GROUP_LOOT;

    // All non-death eligibility controls remain effective while evaluating a dead bot.
    auto rejected = [&] { assert(usage() == ITEM_USAGE_NONE && vote() == PASS); };
    player.level = 69; rejected(); player.level = 70;
    player.classMask = 1; rejected(); player.classMask = 64;
    player.raceMask = 2; rejected(); player.raceMask = 1;
    player.skills.clear(); rejected(); player.skills[413] = 1;
    templates[30189].RequiredSkill = 999; rejected(); templates[30189].RequiredSkill = 0;
    templates[30189].RequiredSpell = 999; rejected(); templates[30189].RequiredSpell = 0;
    templates[30189].RequiredReputationFaction = 1;
    templates[30189].RequiredReputationRank = 5; rejected();
    templates[30189].RequiredReputationFaction = 0;
    templates[30189].HolidayId = 1; rejected(); templates[30189].HolidayId = 0;
    scripts.equipAllowed = false; rejected(); scripts.equipAllowed = true;
    scripts.useAllowed = false; rejected(); scripts.useAllowed = true;
    player.disarmed = true; rejected(); player.disarmed = false;
    player.uniqueResult = EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE; rejected(); player.uniqueResult = EQUIP_ERR_OK;
    templates[30189].Flags = ITEM_FLAG_UNIQUE_EQUIPPABLE;
    player.uniqueEquipped = true; rejected(); player.uniqueEquipped = false;
    templates[30189].Flags = 0;
    templates[30189].slot = NULL_SLOT; rejected(); templates[30189].slot = 9;
    player.countResult = EQUIP_ERR_INVENTORY_FULL; rejected(); player.countResult = EQUIP_ERR_OK;

    // Two-handed/offhand checks retain not_loading=true semantics, including storage checks.
    templates[30189].Class = ITEM_CLASS_WEAPON;
    templates[30189].InventoryType = INVTYPE_2HWEAPON;
    templates[30189].SubClass = ITEM_SUBCLASS_WEAPON_AXE2;
    templates[30189].slot = EQUIPMENT_SLOT_MAINHAND;
    player.equipped[EQUIPMENT_SLOT_OFFHAND] = &old;
    assert(usage() == ITEM_USAGE_EQUIP);
    player.storageFull = true; rejected(); player.storageFull = false;
    player.canUnequip = false; rejected(); player.canUnequip = true;
    assert(usage() == ITEM_USAGE_EQUIP);

    // Resurrection keeps the same valuation and restores ordinary equip eligibility.
    player.alive = true;
    assert(ai.CanEquipItem(NULL_SLOT, dest, &candidate, true, true) == EQUIP_ERR_OK);
    Player other;
    Item foreign{candidate.GetTemplate(), &other};
    assert(ai.CanEquipItem(NULL_SLOT, dest, &foreign, true, true) == EQUIP_ERR_DONT_OWN_THAT_ITEM);
    assert(Item::allocations == Item::removals);
    std::cout << "Alive/dead upgrade votes, normal equip rejection and eligibility controls passed\n";
}
