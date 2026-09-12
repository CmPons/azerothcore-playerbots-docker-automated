// Offline production-command/API-double tests; never connect to a live server.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
struct ObjectGuid
{
    uint32 value = 0;
    explicit operator bool() const { return value != 0; }
    bool operator==(ObjectGuid const&) const = default;
};
using GuidVector = std::vector<ObjectGuid>;
using ItemIds = std::vector<uint32>;
constexpr uint8 INVENTORY_SLOT_BAG_0 = 255, INVENTORY_SLOT_ITEM_START = 23,
    INVENTORY_SLOT_ITEM_END = 39, INVENTORY_SLOT_BAG_START = 19, INVENTORY_SLOT_BAG_END = 23;
constexpr uint32 UNIT_NPC_FLAG_VENDOR = 128;
constexpr float INTERACTION_DISTANCE = 5.5f;
constexpr uint32 ITEM_CLASS_CONSUMABLE = 0, ITEM_CLASS_REAGENT = 5, ITEM_CLASS_PROJECTILE = 6;
enum Classes { CLASS_WARRIOR=1, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
    CLASS_DEATH_KNIGHT, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID=11 };
struct WorldObject
{
    uint32 phase = 1, map = 1;
    float x = 0, size = 0;
    uint32 GetPhaseMask() const { return phase; }
    bool InSamePhase(uint32 other) const { return (phase & other) != 0; }
};
struct Creature : WorldObject
{
    ObjectGuid guid{1};
    bool vendor = true, friendly = true, alive = true;
    ObjectGuid GetGUID() const { return guid; }
};
std::vector<Creature*> creatures;
template<class T> struct GridReference { T* source; T* GetSource() { return source; } };
template<class T> using GridRefMgr = std::vector<GridReference<T>>;
using CreatureMapType = GridRefMgr<Creature>;
namespace Acore
{
/* SEARCHER */
}
/* SEARCHER_VISIT */
namespace Cell
{
    inline uint32 visits = 0;
    template<class Searcher> void VisitObjects(WorldObject* player, Searcher& searcher, float range)
    {
        ++visits;
        CreatureMapType grid;
        for (Creature* c : creatures)
            if (c->map == player->map && std::fabs(c->x - player->x) <= range)
                grid.push_back({c});
        searcher.Visit(grid);
    }
}
struct ItemTemplate { uint32 Class = 15; };
struct Item
{
    uint32 id, count;
    ItemTemplate proto;
    uint32 GetCount() const { return count; }
    ItemTemplate const* GetTemplate() const { return &proto; }
    uint8 GetBagSlot() const { return 0; }
    uint8 GetSlot() const { return 0; }
};
struct Bag
{
    std::vector<Item> items;
    uint32 GetBagSize() const { return items.size(); }
    Item* GetItemByPos(uint32 slot) { return &items.at(slot); }
};
struct Player;
struct GroupReference
{
    Player* player;
    GroupReference* nextRef = nullptr;
    Player* GetSource() { return player; }
    GroupReference* next() { return nextRef; }
};
struct Group
{
    std::vector<GroupReference> refs;
    GroupReference* GetFirstMember() { return refs.empty() ? nullptr : &refs.front(); }
    void Set(std::initializer_list<Player*> players)
    {
        refs.clear(); for (Player* p : players) refs.push_back({p});
        for (size_t i=0; i+1<refs.size(); ++i) refs[i].nextRef=&refs[i+1];
    }
};
template<class T> struct Value { T data; T Get() { return data; } };
struct Context
{
    Value<GuidVector> npcs;
    template<class T> Value<T>* GetValue(char const*)
    {
        static_assert(std::is_same_v<T, GuidVector>); return &npcs;
    }
};
struct PlayerbotAI
{
    bool real = false;
    Context context;
    bool IsRealPlayer() { return real; }
    Context* GetAiObjectContext() { return &context; }
};
struct Player : WorldObject
{
    bool inWorld = true, alive = true, inFlight = false, room = true;
    uint8 cls = CLASS_PALADIN, level = 60;
    uint32 saves = 0, money = 1000;
    Group* group = nullptr;
    PlayerbotAI* ai = nullptr;
    std::vector<Item> items;
    Bag bag;
    std::vector<std::string> calls;
    bool IsInWorld() { return inWorld; }
    Group* GetGroup() { return group; }
    uint8 GetLevel() { return level; }
    uint8 getClass() { return cls; }
    Creature* GetNPCIfCanInteractWith(ObjectGuid guid, uint32 flags)
    {
        assert(flags == UNIT_NPC_FLAG_VENDOR);
        if (!inWorld || !alive || inFlight) return nullptr;
        for (Creature* c : creatures)
            if (c->guid == guid && c->map == map && c->vendor && c->friendly && c->alive &&
                std::fabs(c->x-x) <= INTERACTION_DISTANCE + size + c->size)
                return c;
        return nullptr;
    }
    Item* GetItemByPos(uint8, uint8 slot)
    {
        size_t i = slot - INVENTORY_SLOT_ITEM_START;
        return i < items.size() ? &items[i] : nullptr;
    }
    Bag* GetBagByPos(uint8 slot) { return slot == INVENTORY_SLOT_BAG_START ? &bag : nullptr; }
    uint32 GetItemCount(uint32 id)
    {
        uint32 n=0; for (auto const& item : items) if (item.id==id) n+=item.count;
        for (auto const& item : bag.items) if (item.id==id) n+=item.count;
        return n;
    }
    void SaveToDB(bool a, bool b) { assert(!a && !b); ++saves; }
    void DestroyItem(uint8, uint8, bool) { assert(false && "unexpected item destruction"); }
};
#define GET_PLAYERBOT_AI(player) ((player)->ai)
struct Session { Player* player; Player* GetPlayer() { return player; } };
struct ChatHandler
{
    Session* session;
    std::string message;
    std::vector<uint32> summary;
    Session* GetSession() { return session; }
    void SendSysMessage(char const* text) { message=text; }
    void PSendSysMessage(char const* text, uint32 stocked, uint32 reagentOnly, uint32 sold,
                        uint32 added, char const* skipped)
    { message=std::string(text)+skipped; summary={stocked,reagentOnly,sold,added}; }
};
bool g_RaidRosterEnable = true;
struct RaidRosterCommand { static bool HandleStockUp(ChatHandler* handler); };
// Only needed to compile the untouched Shaman branch of the real factory method.
constexpr uint32 ITEM_SUBCLASS_ARMOR_TOTEM = 0;
enum IterateItemsMask { ITERATE_ITEMS_IN_BAGS=1, ITERATE_ITEMS_IN_EQUIP=2, ITERATE_ITEMS_IN_BANK=4 };
struct HasRelicBySubclassVisitor { explicit HasRelicBySubclassVisitor(uint32) {} bool found=false; };
struct FindItemByIdsVisitor
{
    explicit FindItemByIdsVisitor(ItemIds const&) {}
    std::vector<Item*> GetResult() { return {}; }
};
struct PlayerbotFactory
{
    Player* bot; uint8 level;
    PlayerbotFactory(Player* p, uint8 l) : bot(p), level(l) {}
    template<class T> void IterateItems(T*, IterateItemsMask) {}
    void InitReagents();
    void InitAmmo() { bot->calls.push_back("ammo"); }
    void InitConsumables() { bot->calls.push_back("consumables"); }
    void InitPotions() { bot->calls.push_back("potions"); }
    void StoreItem(uint32 id, uint32 count)
    {
        bot->calls.push_back("reagent");
        if (!bot->room) return;
        for (auto& item : bot->items) if (item.id==id) { item.count+=count; return; }
        bot->items.push_back({id,count,{15}});
    }
};
/* FOR_EACH_ITEM */
/* FIND_BOT_VENDOR */
/* FIND_PLAYER_VENDOR */
/* COUNT_ITEMS */
uint32 LegacyCountRaidStockItems(Player* bot)
{
    uint32 total = 0;
    ForEachBagItem(bot, [&](Item* item)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (proto && (proto->Class == ITEM_CLASS_CONSUMABLE || proto->Class == ITEM_CLASS_REAGENT ||
                      proto->Class == ITEM_CLASS_PROJECTILE))
            total += item->GetCount();
    });
    return total;
}
/* INIT_REAGENTS */
uint32 SellVendorItems(Player* bot, PlayerbotAI*, ObjectGuid vendor)
{
    assert(bot->GetNPCIfCanInteractWith(vendor, UNIT_NPC_FLAG_VENDOR));
    bot->calls.push_back("sell"); ++bot->money;
    std::erase_if(bot->items, [](Item const& item) { return item.id==999; });
    return 1;
}
uint32 TopOffPotions(Player* bot) { bot->calls.push_back("topoff"); return 0; }
/* HANDLE_STOCKUP */

struct Party
{
    Creature vendor;
    Player master, ari, mel, human, offline;
    PlayerbotAI ariAI, melAI, selfAI, offlineAI;
    Group group;
    Session session{&master};
    ChatHandler handler{&session, {}, {}};
    Party()
    {
        creatures={&vendor}; vendor.x=4;
        ari.x=100; mel.x=1000; mel.map=2; // reagent delivery need not cross a vendor threshold per bot
        ari.ai=&ariAI; mel.ai=&melAI; mel.cls=CLASS_PRIEST;
        human.ai=&selfAI; selfAI.real=true; offline.ai=&offlineAI; offline.inWorld=false;
        group.Set({&master,&ari,nullptr,&mel,&human,&offline}); master.group=&group;
        ariAI.context.npcs.data={vendor.guid}; melAI.context.npcs.data={vendor.guid};
    }
    void Run() { handler.summary.clear(); assert(RaidRosterCommand::HandleStockUp(&handler)); }
};
int main(int argc, char** argv)
{
    assert(argc==2); std::string scenario=argv[1];
    if (scenario=="reagents")
    {
        Party p;
        p.ari.items={{21177,7,{15}},{999,4,{15}}};
        p.mel.bag.items={{17029,3,{15}}};
        p.Run();
        assert(p.ari.GetItemCount(21177)==100 && p.mel.GetItemCount(17029)==40);
        assert((p.handler.summary==std::vector<uint32>{2,2,0,130}));
        assert(p.ari.money==1000 && p.mel.money==1000 && p.ari.GetItemCount(999)==4);
        assert(p.ari.calls==std::vector<std::string>{"reagent"});
        assert(p.mel.calls==std::vector<std::string>{"reagent"});
        assert(p.human.saves==0 && p.offline.saves==0 && p.master.saves==0);
        assert(LegacyCountRaidStockItems(&p.ari)==0); // Kings/candles are Misc in the actual DB
        p.Run(); assert(p.handler.summary[3]==0); // no duplication on repeated stockup
        p.ari.items[0].count=120; p.Run(); assert(p.ari.GetItemCount(21177)==120); // never truncate surplus
        p.mel.items.clear(); p.mel.bag.items.clear(); p.mel.room=false;
        p.Run(); assert(p.mel.GetItemCount(17029)==0 && p.handler.summary[3]==0);
    }
    else if (scenario=="vendor-gate")
    {
        Party p; p.vendor.vendor=false; p.Run(); assert(p.ari.saves==0 && p.mel.saves==0);
        p.vendor.vendor=true; p.vendor.friendly=false; p.Run(); assert(p.ari.saves==0);
        p.vendor.friendly=true; p.vendor.alive=false; p.Run(); assert(p.ari.saves==0);
        p.vendor.alive=true; p.vendor.x=6; p.Run(); assert(p.ari.saves==0);
        p.vendor.x=4; p.vendor.phase=2; p.Run(); assert(p.ari.saves==0);
        p.vendor.phase=1; p.master.inFlight=true; p.Run(); assert(p.ari.saves==0);
        p.master.inFlight=false; p.vendor.x=8; p.vendor.size=3; p.Run(); assert(p.ari.saves==1);
        // No class/trade inventory check: this could be a food vendor, not a reagent merchant.
        assert(p.ari.GetItemCount(21177)==100);
    }
    else if (scenario=="legacy-stockup")
    {
        Party p; p.master.x=100; p.ari.x=0; p.ari.items={{999,5,{15}}};
        p.Run();
        assert((p.ari.calls==std::vector<std::string>{"sell","ammo","reagent","consumables","potions","topoff"}));
        assert(p.ari.GetItemCount(999)==0 && p.ari.money==1001 && p.ari.GetItemCount(21177)==100);
        assert(p.mel.saves==0);
        assert((p.handler.summary==std::vector<uint32>{1,0,1,100}));
    }
    else if (scenario=="guards-levels")
    {
        Party p; g_RaidRosterEnable=false; p.Run(); assert(p.ari.saves==0);
        g_RaidRosterEnable=true; p.handler.session=nullptr; p.Run(); assert(p.ari.saves==0);
        p.handler.session=&p.session; p.master.group=nullptr; p.Run(); assert(p.ari.saves==0);
        p.master.group=&p.group; p.ari.level=51; p.mel.level=59; p.Run();
        assert(p.ari.GetItemCount(21177)==0);
        assert(p.mel.GetItemCount(17028)==20 && p.mel.GetItemCount(17029)==20);
    }
    else return 1;
    std::cout << "Stockup production regression passed: " << scenario << '\n';
}
