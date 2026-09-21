// Harness around the actual maintenance translation unit, with core/database APIs doubled.
#include "CompanionGemPlanner.h"
#include "CompanionGemCatalog.h"
#include "DataMap.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using uint8 = uint8_t;
using uint32 = uint32_t;
constexpr int ITEM_CLASS_GEM = 3, ITEM_QUALITY_RARE = 3, ITEM_QUALITY_EPIC = 4;
constexpr int NO_BIND = 0, ITEM_FLAG_UNIQUE_EQUIPPABLE = 1, EQUIP_ERR_OK = 0;
constexpr int SOCKET_COLOR_META = 1, SOCKET_COLOR_RED = 2, SOCKET_COLOR_YELLOW = 4, SOCKET_COLOR_BLUE = 8;
constexpr int MAX_GEM_SOCKETS = 3, EQUIPMENT_SLOT_START = 0, EQUIPMENT_SLOT_END = 19, INVENTORY_SLOT_BAG_0 = 0;
constexpr int BOT_STATE_COMBAT = 0;
constexpr int PLAYERHOOK_ON_AFTER_UPDATE = 0, PLAYERHOOK_ON_EQUIP = 1, PLAYERHOOK_ON_STORE_NEW_ITEM = 2;
enum EnchantmentSlot { PERM_ENCHANTMENT_SLOT, TEMP_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT,
                       SOCK2, SOCK3, BONUS_ENCHANTMENT_SLOT, PRISMATIC_ENCHANTMENT_SLOT };
struct ObjectGuid
{
    uint32 value = 0;
    uint32 GetCounter() const { return value; }
    bool operator==(ObjectGuid const&) const = default;
};
struct ItemTemplate
{
    struct SocketEntry { uint8 Color = 0; } Socket[3];
    uint32 ItemId = 0, Class = ITEM_CLASS_GEM, Quality = 3, Bonding = 0, RequiredSkill = 0;
    uint32 ItemLimitCategory = 0, Duration = 0, GemProperties = 0, RequiredLevel = 0, socketBonus = 900;
    bool unique = false, usable = true;
    bool HasFlag(int) const { return unique; }
};
struct Enchant
{
    uint32 GemID = 0, requiredSkill = 0, requiredLevel = 0, EnchantmentCondition = 0;
    std::array<double, 3> scores{};
};
struct Properties { uint32 spellitemenchantement = 0, color = 0; };
struct Condition
{
    uint8 Color[5]{}, Comparator[5]{}, CompareColor[5]{};
    uint32 Value[5]{};
};
template<class T> struct Store
{
    std::map<uint32, T> rows;
    T const* LookupEntry(uint32 id) const
    {
        auto it = rows.find(id);
        return it == rows.end() ? nullptr : &it->second;
    }
};
Store<Enchant> sSpellItemEnchantmentStore;
Store<Properties> sGemPropertiesStore;
Store<Condition> sSpellItemEnchantmentConditionStore;
struct ObjectManager
{
    std::map<uint32, ItemTemplate> items;
    ItemTemplate const* GetItemTemplate(uint32 id) const
    {
        auto it = items.find(id);
        return it == items.end() ? nullptr : &it->second;
    }
} objectManager;
auto* sObjectMgr = &objectManager;
struct Session
{
    bool bot = true, logout = false;
    bool IsBot() const { return bot; }
    bool isLogingOut() const { return logout; }
};
struct PlayerbotAI
{
    bool real = false;
    int state = 1;
    bool IsRealPlayer() const { return real; }
    int GetState() const { return state; }
};
struct Item
{
    ObjectGuid guid{100}, owner{815};
    uint8 slot = 0;
    ItemTemplate proto;
    std::array<uint32, 7> enchants{};
    bool equipped = true, broken = false, trading = false, refundable = false, tradable = false;
    int updates = 0, saves = 0;
    ObjectGuid GetGUID() const { return guid; }
    ObjectGuid GetOwnerGUID() const { return owner; }
    bool IsEquipped() const { return equipped; }
    bool IsBroken() const { return broken; }
    bool IsInTrade() const { return trading; }
    bool IsRefundable() const { return refundable; }
    bool IsBOPTradable() const { return tradable; }
    uint8 GetSlot() const { return slot; }
    ItemTemplate const* GetTemplate() const { return &proto; }
    bool HasSocket() const { return proto.Socket[0].Color || enchants[PRISMATIC_ENCHANTMENT_SLOT]; }
    uint32 GetEnchantmentId(EnchantmentSlot index) const { return enchants[index]; }
    void SetEnchantment(EnchantmentSlot index, uint32 id, int, int, ObjectGuid)
    {
        enchants[index] = id;
        ++saves;
    }
    bool GemsFitSockets() const
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!proto.Socket[i].Color)
                continue;
            auto const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchants[2 + i]);
            auto const* item = enchant ? sObjectMgr->GetItemTemplate(enchant->GemID) : nullptr;
            auto const* props = item ? sGemPropertiesStore.LookupEntry(item->GemProperties) : nullptr;
            if (!props || !(props->color & proto.Socket[i].Color))
                return false;
        }
        return true;
    }
    void SendUpdateSockets() { ++updates; }
};
struct Player
{
    DataMap CustomData;
    ObjectGuid guid{815};
    Session session;
    PlayerbotAI ai;
    int role = 0;
    uint32 level = 70;
    bool world = true, removing = false, teleporting = false, alive = true, combat = false, trade = false, cast = false;
    std::array<Item*, 19> equipment{};
    std::vector<std::string> calls;
    Session* GetSession() { return &session; }
    ObjectGuid GetGUID() const { return guid; }
    uint32 GetLevel() const { return level; }
    std::string GetName() const { return "Fixture"; }
    bool IsInWorld() const { return world; }
    bool IsDuringRemoveFromWorld() const { return removing; }
    bool IsBeingTeleported() const { return teleporting; }
    bool IsAlive() const { return alive; }
    bool IsInCombat() const { return combat; }
    void* GetTradeData() const { return trade ? (void*)this : nullptr; }
    bool IsNonMeleeSpellCast(bool) const { return cast; }
    Item* GetItemByPos(int, uint8 slot) const { return equipment[slot]; }
    Item* GetItemByGuid(ObjectGuid id) const
    {
        for (auto* item : equipment)
            if (item && item->guid == id)
                return item;
        return nullptr;
    }
    int CanUseItem(ItemTemplate const* proto) const { return proto->usable ? 0 : 1; }
    bool EnchantmentFitsRequirements(uint32 condition, int) const;
    void ToggleMetaGemsActive(uint8, bool on) { calls.push_back(on ? "meta+" : "meta-"); }
    void ApplyEnchantment(Item*, EnchantmentSlot slot, bool on)
    {
        calls.push_back(std::to_string(slot) + (on ? "+" : "-"));
    }
};
#define GET_PLAYERBOT_AI(player) (&(player)->ai)
int logCount = 0;
template<class... T> void Log(T const&...) { ++logCount; }
#define LOG_INFO(...) Log(__VA_ARGS__)
struct StatsWeightCalculator
{
    Player* player;
    explicit StatsWeightCalculator(Player* value) : player(value) { }
    double CalculateEnchant(uint32 id) const { return sSpellItemEnchantmentStore.rows.at(id).scores[player->role]; }
};
struct ConfigManager
{
    std::map<std::string, uint32> values;
    template<class T> T GetOption(std::string key, T fallback) const
    {
        auto it = values.find(key);
        return it == values.end() ? fallback : T(it->second);
    }
} configManager;
auto* sConfigMgr = &configManager;
struct Field
{
    uint32 value;
    template<class T> T Get() const { return T(value); }
};
struct Result
{
    std::vector<std::array<Field, 2>> rows;
    size_t index = 0;
    Field* Fetch() { return rows[index].data(); }
    bool NextRow() { return ++index < rows.size(); }
};
using QueryResult = std::shared_ptr<Result>;
struct Database
{
    std::vector<std::array<Field, 2>> friends;
    unsigned queries = 0;
    QueryResult Query(char const*)
    {
        ++queries;
        return friends.empty() ? nullptr : std::make_shared<Result>(Result{friends, 0});
    }
} CharacterDatabase, PlayerbotsDatabase;
struct BotConfig
{
    std::unordered_set<uint32> accounts{100};
    bool IsInRandomAccountList(uint32 account) const { return accounts.count(account); }
} sPlayerbotAIConfig;
namespace RaidRosterStore
{
    std::unordered_set<uint32> roster;
    std::unordered_set<uint32> AllPinnedBots() { return roster; }
}
struct WorldScript
{
    explicit WorldScript(char const*) { }
    virtual ~WorldScript() = default;
    virtual void OnAfterConfigLoad(bool) { }
    virtual void OnStartup() { }
    virtual void OnUpdate(uint32) { }
};
struct PlayerScript
{
    PlayerScript(char const*, std::initializer_list<int>) { }
    virtual ~PlayerScript() = default;
    virtual void OnPlayerEquip(Player*, Item*, uint8, uint8, bool) { }
    virtual void OnPlayerStoreNewItem(Player*, Item*, uint32) { }
    virtual void OnPlayerAfterUpdate(Player*, uint32) { }
};

// Created by the Python runner from the real .cpp with only #include directives removed.
#include "CompanionGemMaintenance.production.inc"

bool Player::EnchantmentFitsRequirements(uint32 condition, int) const
{
    CompanionGems::Counts counts{};
    for (auto* item : equipment)
    {
        if (!item || item->broken)
            continue;
        for (int i = 2; i < 5; ++i)
        {
            auto const* enchant = sSpellItemEnchantmentStore.LookupEntry(item->enchants[i]);
            auto const* proto = enchant ? sObjectMgr->GetItemTemplate(enchant->GemID) : nullptr;
            auto const* props = proto ? sGemPropertiesStore.LookupEntry(proto->GemProperties) : nullptr;
            if (props)
                counts = CompanionGems::AddColor(counts, props->color);
        }
    }
    std::vector<CompanionGems::Requirement> rules;
    return CompanionGems::ReadRequirements(condition, rules) && CompanionGems::Fits(counts, rules);
}

void Gem(uint32 item, uint32 enchant, uint8 color, uint32 quality, std::array<double, 3> scores, uint32 condition = 0)
{
    ItemTemplate proto;
    proto.ItemId = item;
    proto.GemProperties = item;
    proto.Quality = quality;
    objectManager.items[item] = proto;
    sGemPropertiesStore.rows[item] = {enchant, color};
    sSpellItemEnchantmentStore.rows[enchant] = {item, 0, 0, condition, scores};
}

void ResetSettings(uint32 epic = 0)
{
    configManager.values = {{"CompanionMaintenance.SocketGems.Enable", 1},
                            {"CompanionMaintenance.SocketGems.EpicPercent", epic}};
    CharacterDatabase.friends = {std::array<Field, 2>{Field{815}, Field{1}},
                                 std::array<Field, 2>{Field{999}, Field{100}},
                                 std::array<Field, 2>{Field{555}, Field{500}}};
    PlayerbotsDatabase.friends = {std::array<Field, 2>{Field{500}, Field{2}}};
    RaidRosterStore::roster = {1180};
    CompanionGems::RefreshSettings();
}

int main()
{
    Gem(24027, 101, 2, 3, {10, 1, 2}); // strength
    Gem(24030, 102, 2, 3, {1, 12, 1}); // spell power
    Gem(24033, 103, 8, 3, {1, 1, 12}); // stamina
    Gem(24054, 104, 10, 3, {7, 1, 7});
    Gem(24057, 105, 10, 3, {1, 9, 1});
    Gem(24050, 106, 4, 3, {3, 8, 1});
    Gem(24062, 107, 12, 3, {1, 1, 7});
    Gem(24058, 111, 6, 3, {8, 1, 1});
    Gem(32193, 108, 2, 4, {15, 1, 3});
    Gem(32409, 109, 1, 3, {8, 1, 1}, 1);
    Gem(25901, 110, 1, 3, {1, 12, 1}, 1);
    Condition condition;
    for (int i = 0; i < 3; ++i)
    {
        condition.Color[i] = i + 2;
        condition.Comparator[i] = 5;
        condition.Value[i] = 2; // Installed BC Relentless/Insightful conditions require two of each color.
    }
    sSpellItemEnchantmentConditionStore.rows[1] = condition;
    ResetSettings();
    auto config = CompanionGems::settings.load();
    assert(config->eligible.count(815) && config->eligible.count(1180));
    assert(!config->eligible.count(999) && !config->eligible.count(555));
    assert(config->gems.size() == 11);

    // Scope includes roster bots without friends. Humans, AI-controlled humans and fillers cannot be modified.
    Player player;
    assert(CompanionGems::IsCompanion(&player, *config));
    player.guid.value = 1180;
    assert(CompanionGems::IsCompanion(&player, *config));
    player.guid.value = 999;
    assert(!CompanionGems::IsCompanion(&player, *config));
    player.guid.value = 815;
    player.session.bot = false;
    assert(!CompanionGems::IsCompanion(&player, *config));
    player.session.bot = true;
    player.ai.real = true;
    assert(!CompanionGems::IsCompanion(&player, *config));
    player.ai.real = false;
    for (bool Player::* flag : {&Player::combat, &Player::cast, &Player::trade, &Player::teleporting, &Player::removing})
    {
        player.*flag = true;
        assert(!CompanionGems::SafeToMaintain(&player));
        player.*flag = false;
    }
    player.alive = false;
    assert(!CompanionGems::SafeToMaintain(&player));
    player.alive = true;
    player.ai.state = BOT_STATE_COMBAT;
    assert(!CompanionGems::SafeToMaintain(&player));
    player.ai.state = 1;
    player.session.logout = true;
    assert(!CompanionGems::SafeToMaintain(&player));
    player.session.logout = false;

    CompanionGemPlayer script;
    Item chest;
    chest.slot = 4;
    chest.proto.Socket[0].Color = 2;
    chest.proto.Socket[1].Color = 4;
    chest.proto.Socket[2].Color = 8;
    chest.enchants[0] = 777; // permanent enchant must survive unchanged
    player.equipment[4] = &chest;
    player.combat = true;
    script.OnPlayerStoreNewItem(&player, &chest, 1);
    script.OnPlayerAfterUpdate(&player, 3000);
    assert(chest.enchants[2] == 0);
    player.combat = false;
    script.OnPlayerAfterUpdate(&player, 1);
    assert(chest.enchants[2] == 101 && chest.enchants[3] && chest.enchants[4]);
    assert(chest.enchants[0] == 777 && chest.updates == 3);
    assert(player.calls.front() == "meta-" && player.calls.back() == "meta+");
    auto preserved = chest.enchants;
    script.OnPlayerAfterUpdate(&player, 400000);
    assert(chest.enchants == preserved && chest.updates == 3); // periodic is idempotent

    // Equipping newly received gear causes an early check, not a full-interval wait.
    Item shoulders;
    shoulders.guid.value = 101;
    shoulders.slot = 2;
    shoulders.proto.Socket[0].Color = 2;
    player.equipment[2] = &shoulders;
    script.OnPlayerEquip(&player, &shoulders, 0, 2, true);
    script.OnPlayerAfterUpdate(&player, 1999);
    assert(!shoulders.enchants[2]);
    script.OnPlayerAfterUpdate(&player, 1);
    assert(shoulders.enchants[2] == 101 && shoulders.enchants[BONUS_ENCHANTMENT_SLOT] == 900);
    std::vector<std::string> bonusOrder{"meta-", "2-", "3-", "4-", "2+", "3+", "4+", "5-", "5+", "meta+"};
    assert(std::vector<std::string>(player.calls.end() - 10, player.calls.end()) == bonusOrder);
    assert(shoulders.saves == 2); // socket and newly activated native bonus, normal item saving
    // Re-spec does not rewrite occupied sockets (whether manual or generated).
    player.role = 1;
    script.OnPlayerAfterUpdate(&player, 400000);
    assert(chest.enchants == preserved && shoulders.enchants[2] == 101);

    // Future empty sockets follow the NEW role. A filled socket on the same item stays intact.
    shoulders.proto.Socket[1].Color = 2;
    script.OnPlayerAfterUpdate(&player, 400000);
    assert(shoulders.enchants[3] == 102);
    // Tank role selects stamina, not caster or strength gems.
    player.role = 2;
    shoulders.proto.Socket[2].Color = 8;
    script.OnPlayerAfterUpdate(&player, 400000);
    assert(shoulders.enchants[4] == 103);

    // Do not alter bag-only, traded, refundable, broken, or foreign-owned gear.
    for (int test = 0; test < 6; ++test)
    {
        Item item;
        item.proto.Socket[0].Color = 2;
        if (test == 0) item.equipped = false;
        if (test == 1) item.trading = true;
        if (test == 2) item.refundable = true;
        if (test == 3) item.tradable = true;
        if (test == 4) item.broken = true;
        if (test == 5) item.owner.value = 42;
        assert(!CompanionGems::ApplyGem(&player, &item, 0, {24027, 101, 2, 10}));
        assert(item.saves == 0);
    }

    // Native socket type and prismatic-extension rules; no phantom sockets.
    Item belt;
    belt.slot = 5;
    belt.guid.value = 105;
    belt.enchants[PRISMATIC_ENCHANTMENT_SLOT] = 999;
    player.equipment[5] = &belt;
    assert(CompanionGems::SocketColor(&belt, 0) == 14 && CompanionGems::SocketColor(&belt, 1) == 0);
    assert(!CompanionGems::ApplyGem(&player, &belt, 0, {32409, 109, 1, 10}));
    assert(!CompanionGems::ApplyGem(&player, &belt, 1, {24027, 101, 2, 10}));
    assert(CompanionGems::ApplyGem(&player, &belt, 0, {24027, 101, 2, 10}));
    assert(!CompanionGems::ApplyGem(&player, &belt, 0, {24030, 102, 2, 10}));

    // A fresh setup must choose hybrid colors to activate a meta and apply it LAST.
    Player metaPlayer;
    Item head, body;
    head.proto.Socket[0].Color = 1;
    body.guid.value = 200;
    body.slot = 4;
    for (auto& socket : body.proto.Socket) socket.Color = 2;
    metaPlayer.equipment[0] = &head;
    metaPlayer.equipment[4] = &body;
    CompanionGems::FillSockets(&metaPlayer, *config);
    assert(head.enchants[2] == 109 && metaPlayer.EnchantmentFitsRequirements(1, -1));
    assert(body.enchants[2] && body.enchants[3] && body.enchants[4]);
    auto metaGems = body.enchants;
    CompanionGems::FillSockets(&metaPlayer, *config);
    assert(body.enchants == metaGems && head.updates == 1);
    Player impossible;
    Item emptyMeta;
    emptyMeta.proto.Socket[0].Color = 1;
    impossible.equipment[0] = &emptyMeta;
    CompanionGems::FillSockets(&impossible, *config);
    assert(!emptyMeta.enchants[2]); // do not install an inactive meta

    // Epic mix honors its stable per-socket policy; meta fallback stays blue.
    ResetSettings(100);
    Player epicPlayer;
    Item epicGear;
    epicGear.proto.Socket[0].Color = 2;
    epicPlayer.equipment[0] = &epicGear;
    CompanionGems::FillSockets(&epicPlayer, *CompanionGems::settings.load());
    assert(epicGear.enchants[2] == 108);
    Player epicMetaPlayer;
    Item epicHead, epicBody;
    epicHead.proto.Socket[0].Color = 1;
    epicBody.guid.value = 301;
    epicBody.slot = 4;
    for (auto& socket : epicBody.proto.Socket) socket.Color = 2;
    epicMetaPlayer.equipment[0] = &epicHead;
    epicMetaPlayer.equipment[4] = &epicBody;
    CompanionGems::FillSockets(&epicMetaPlayer, *CompanionGems::settings.load());
    assert(epicHead.enchants[2] == 109 && epicMetaPlayer.EnchantmentFitsRequirements(1, -1));
    assert(epicBody.enchants[2] != 108 && epicBody.enchants[3] != 108 && epicBody.enchants[4] != 108);
    ResetSettings(0);
    CompanionGems::FillSockets(&epicPlayer, *CompanionGems::settings.load());
    assert(epicGear.enchants[2] == 108); // no downgrade after policy change

    // Active manual relative-color metas cannot be disabled by a tempting yellow gem.
    // An incompatible, already inactive second manual meta must not block all useful maintenance.
    Gem(99901, 901, 1, 4, {10, 10, 10}, 2);
    Gem(99902, 902, 1, 4, {10, 10, 10}, 3);
    Condition moreBlue, moreYellow;
    moreBlue.Color[0] = 4; moreBlue.Comparator[0] = 3; moreBlue.CompareColor[0] = 3;
    moreYellow.Color[0] = 3; moreYellow.Comparator[0] = 3; moreYellow.CompareColor[0] = 4;
    sSpellItemEnchantmentConditionStore.rows[2] = moreBlue;
    sSpellItemEnchantmentConditionStore.rows[3] = moreYellow;
    sSpellItemEnchantmentStore.rows[106].scores = {50, 50, 50};
    Player manual;
    Item manualHead, manualOther, manualBody;
    manualHead.proto.Socket[0].Color = 1;
    manualHead.enchants[2] = 901;
    manualOther.proto.Socket[0].Color = 1;
    manualOther.enchants[2] = 902;
    manualOther.guid.value = 302;
    manualOther.slot = 1;
    manualBody.guid.value = 303;
    manualBody.slot = 4;
    manualBody.proto.Socket[0].Color = 8;
    manualBody.proto.Socket[1].Color = 4;
    manualBody.enchants[2] = 103;
    manual.equipment[0] = &manualHead;
    manual.equipment[1] = &manualOther;
    manual.equipment[4] = &manualBody;
    assert(manual.EnchantmentFitsRequirements(2, -1) && !manual.EnchantmentFitsRequirements(3, -1));
    CompanionGems::FillSockets(&manual, *CompanionGems::settings.load());
    assert(manualBody.enchants[3] && manualBody.enchants[3] != 106);
    assert(manualHead.enchants[2] == 901 && manualOther.enchants[2] == 902 && manualBody.enchants[2] == 103);
    assert(manual.EnchantmentFitsRequirements(2, -1));
    sSpellItemEnchantmentStore.rows[106].scores = {3, 8, 1};

    // Catalog filters reject unique, profession-restricted, limited, bound and temporary gems.
    ItemTemplate saved = objectManager.items[24027];
    for (int test = 0; test < 5; ++test)
    {
        auto& proto = objectManager.items[24027];
        proto = saved;
        if (test == 0) proto.unique = true;
        if (test == 1) proto.RequiredSkill = 755;
        if (test == 2) proto.ItemLimitCategory = 1;
        if (test == 3) proto.Bonding = 1;
        if (test == 4) proto.Duration = 1;
        ResetSettings();
        for (auto const& gem : CompanionGems::settings.load()->gems)
            assert(gem.option.item != 24027);
    }
    objectManager.items[24027] = saved;
    ResetSettings();

    // A due check must still enforce scope and minimum level; deletion before a check is harmless.
    Player low;
    Item lowGear;
    lowGear.proto.Socket[0].Color = 2;
    low.equipment[0] = &lowGear;
    low.level = 60;
    script.OnPlayerAfterUpdate(&low, 400000);
    assert(!lowGear.enchants[2]);
    low.level = 61;
    low.guid.value = 999;
    script.OnPlayerAfterUpdate(&low, 400000);
    assert(!lowGear.enchants[2]);
    low.guid.value = 815;
    script.OnPlayerEquip(&low, &lowGear, 0, 0, true);
    low.equipment[0] = nullptr;
    script.OnPlayerAfterUpdate(&low, 400000);
    assert(!lowGear.enchants[2]);

    // No map-thread SQL: settings snapshots are refreshed separately; disabled mode does no lookup.
    unsigned queries = CharacterDatabase.queries + PlayerbotsDatabase.queries;
    script.OnPlayerAfterUpdate(&player, 400000);
    assert(CharacterDatabase.queries + PlayerbotsDatabase.queries == queries);
    configManager.values["CompanionMaintenance.SocketGems.Enable"] = 0;
    CompanionGems::RefreshSettings();
    assert(CharacterDatabase.queries + PlayerbotsDatabase.queries == queries);
    assert(!CompanionGems::settings.load()->enabled);
    std::cout << "Companion gem runtime: all checks passed\n";
}
