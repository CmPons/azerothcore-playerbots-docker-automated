// Offline API doubles exercising extracted production StatsAction methods; no server or DB.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
constexpr uint8 EQUIPMENT_SLOT_START = 0;
constexpr uint8 EQUIPMENT_SLOT_END = 19;
constexpr uint8 INVENTORY_SLOT_ITEM_START = 23;
constexpr uint8 INVENTORY_SLOT_ITEM_END = 39;
constexpr uint8 INVENTORY_SLOT_BAG_0 = 255;
constexpr uint32 ITEM_FIELD_DURABILITY = 0;
constexpr uint32 ITEM_FIELD_MAXDURABILITY = 1;

struct Item
{
    uint32 current = 0;
    uint32 maximum = 0;
    uint32 GetUInt32Value(uint32 field) const { return field == ITEM_FIELD_DURABILITY ? current : maximum; }
};
struct Player
{
    std::array<Item*, 64> slots{};
    Item* GetItemByPos(uint16 pos) { return slots.at(pos & 255); }
};
struct ChatHelper
{
    std::string formatMoney(uint32 value) { return std::to_string(value) + "c"; }
};
struct StatsAction
{
    Player player;
    ChatHelper helper;
    Player* bot = &player;
    ChatHelper* chat = &helper;
    std::array<uint32, 64> costs{};
    std::array<uint32, 64> estimates{};
    uint32 EstRepair(uint16 pos) { ++estimates.at(pos & 255); return costs.at(pos & 255); }
    void ListRepairCost(std::ostringstream& out);
    double RepairPercent(uint16 pos);
    std::string Render() { std::ostringstream out; ListRepairCost(out); return out.str(); }
};

/* LIST_REPAIR_COST */
/* REPAIR_PERCENT */

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string const scenario = argv[1];
    StatsAction action;
    Item full{100, 100}, half{50, 100}, broken{0, 100}, ring{0, 0};
    if (scenario == "full-empty")
    {
        assert(action.Render() == "|cff00ff00100% (0c)|cffffffff Dur");
        action.player.slots[0] = &ring;
        assert(action.Render() == "|cff00ff00100% (0c)|cffffffff Dur");
        action.player.slots[1] = &full;
        action.player.slots[2] = &full;
        assert(action.Render() == "|cff00ff00100% (0c)|cffffffff Dur");
    }
    else if (scenario == "mixed-broken")
    {
        action.player.slots[0] = &full;
        action.player.slots[1] = &half;
        action.player.slots[10] = &ring;
        assert(action.Render() == "|cff00ff0075% (0c)|cffffffff Dur");
        action.player.slots[0] = &broken;
        assert(action.Render() == "|cffffff0025% (0c)|cffffffff Dur");
        action.player.slots[1] = &broken;
        assert(action.Render() == "|cffff00000% (0c)|cffffffff Dur");
        // Each repairable equipped item has equal weight, not one vote per durability point.
        Item shortFull{10, 10}, longHalf{100, 200};
        action.player.slots[0] = &shortFull;
        action.player.slots[1] = &longHalf;
        assert(action.Render() == "|cff00ff0075% (0c)|cffffffff Dur");
        Item fractional{2, 3};
        action.player.slots[0] = &fractional;
        action.player.slots[1] = nullptr;
        assert(action.Render() == "|cff00ff0067% (0c)|cffffffff Dur");
    }
    else if (scenario == "inventory-cost")
    {
        action.player.slots[EQUIPMENT_SLOT_END - 1] = &full;
        action.player.slots[EQUIPMENT_SLOT_END] = &broken;
        action.player.slots[INVENTORY_SLOT_ITEM_START] = &broken;
        action.player.slots[INVENTORY_SLOT_ITEM_END - 1] = &broken;
        action.costs[EQUIPMENT_SLOT_END - 1] = 1;
        action.costs[INVENTORY_SLOT_ITEM_START] = 20;
        action.costs[INVENTORY_SLOT_ITEM_END - 1] = 30;
        action.costs[INVENTORY_SLOT_ITEM_END] = 999;
        assert(action.Render() == "|cff00ff00100% (51c)|cffffffff Dur");
        for (uint32 i = 0; i < INVENTORY_SLOT_ITEM_END; ++i)
            assert(action.estimates[i] == 1);
        assert(action.estimates[INVENTORY_SLOT_ITEM_END] == 0);
        assert(full.current == 100 && broken.current == 0); // rendering does not repair anything
    }
    else
        return 1;
    std::cout << "Durability display: " << scenario << " passed\n";
}
