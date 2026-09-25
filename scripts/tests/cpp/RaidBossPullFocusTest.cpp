// Production helper and DPS selector are injected; no server or database operations.
#include <cassert>
#include <map>
#include <string>
#include <iostream>

struct ObjectGuid
{
    int id = 0;
    explicit operator bool() const { return id != 0; }
};
struct Unit
{
    bool alive = true, inWorld = true, immuneBoss = false, combat = false;
    int map = 1;
    Unit* victim = nullptr;
    Unit const* engagedBy = nullptr;
    ObjectGuid selected;
    bool IsAlive() const { return alive; }
    bool IsInWorld() const { return inWorld; }
    int GetMap() const { return map; }
    Unit* GetVictim() { return victim; }
    ObjectGuid GetTarget() { return selected; }
    bool IsEngagedBy(Unit const* unit) const { return engagedBy == unit; }
};
struct Player : Unit { bool tank = false; };
struct PlayerbotAI
{
    Unit* mainTank = nullptr;
    std::map<int, Unit*> units;
    bool caster = false, combo = false;
    unsigned near = 0;
    Unit* GetUnit(ObjectGuid guid) { return units[guid.id]; }
    static bool IsTank(Player* player) { return player->tank; }
    unsigned GetNearGroupMemberCount() { return near; }
    bool IsCaster(Player*) { return caster; }
    bool IsCombo(Player*) { return combo; }
};
namespace ai::threat
{
Unit* GetMainTank(PlayerbotAI* ai) { return ai ? ai->mainTank : nullptr; }
bool IsTauntImmuneRaidBoss(Unit* unit) { return unit && unit->immuneBoss; }
// PRODUCTION_FOCUS
}
struct RtiTargetValue
{
    Unit* marked = nullptr;
    Unit* Calculate() { return marked; }
};
struct TargetValue
{
    Unit* ordinary = nullptr;
    template<class T> Unit* FindTarget(T*) { return ordinary; }
};
struct CasterFindTargetSmartStrategy { CasterFindTargetSmartStrategy(PlayerbotAI*, float) {} };
struct ComboFindTargetSmartStrategy { ComboFindTargetSmartStrategy(PlayerbotAI*, float) {} };
struct GeneralFindTargetSmartStrategy { GeneralFindTargetSmartStrategy(PlayerbotAI*, float) {} };
struct DpsTargetValue : RtiTargetValue, TargetValue
{
    PlayerbotAI* botAI;
    Player* bot;
    DpsTargetValue(PlayerbotAI* ai, Player* player) : botAI(ai), bot(player) {}
    Unit* Calculate();
};
#define AI_VALUE(type, name) type(1)
// PRODUCTION_SELECTOR

int main()
{
    Player tank, dps, otherGroup;
    Unit boss, trash;
    boss.immuneBoss = true;
    PlayerbotAI ai;
    ai.mainTank = &tank;
    ai.units[1] = &boss;
    tank.selected.id = 1;
    DpsTargetValue selector(&ai, &dps);

    // A mere selection/inspection of Hydross must not create a DPS target.
    assert(!selector.Calculate());
    // Combat elsewhere, including fighting nearby trash, is not permission either.
    tank.combat = true;
    assert(!selector.Calculate());
    boss.combat = true;
    boss.engagedBy = &otherGroup;
    assert(!selector.Calculate());
    selector.ordinary = &trash;
    assert(selector.Calculate() == &trash);
    // Even an autoattack selected before reaching the boss cannot launch ranged DPS early.
    tank.victim = &boss;
    assert(selector.Calculate() == &trash);

    // Actual MT engagement is enough, including zero-threat/combat-link cases.
    boss.engagedBy = &tank;
    assert(selector.Calculate() == &boss);
    tank.victim = nullptr;
    assert(selector.Calculate() == &boss);
    tank.combat = false; // Per-target engagement is authoritative, not an unrelated cached flag.
    assert(selector.Calculate() == &boss);
    // Threat reset/evade: immediately stop injecting the stale selected boss.
    boss.engagedBy = nullptr;
    assert(selector.Calculate() == &trash);
    boss.engagedBy = &tank;
    boss.alive = false;
    assert(selector.Calculate() == &trash);
    boss.alive = true;
    boss.inWorld = false;
    assert(selector.Calculate() == &trash);
    boss.inWorld = true;
    boss.map = 2;
    assert(selector.Calculate() == &trash);
    boss.map = 1;
    tank.alive = false;
    assert(selector.Calculate() == &trash);
    tank.alive = true;
    ai.mainTank = nullptr;
    assert(selector.Calculate() == &trash);
    ai.mainTank = &tank;
    tank.selected.id = 0;
    assert(selector.Calculate() == &trash);
    tank.selected.id = 999;
    assert(selector.Calculate() == &trash);
    tank.selected.id = 1;

    // Existing RTI/explicit-target arbitration and tank/non-boss paths stay separate.
    boss.engagedBy = nullptr;
    selector.marked = &boss;
    assert(selector.Calculate() == &boss);
    selector.marked = nullptr;
    boss.engagedBy = &tank;
    dps.tank = true;
    assert(selector.Calculate() == &trash);
    dps.tank = false;
    boss.immuneBoss = false;
    assert(selector.Calculate() == &trash);
    for (bool caster : {false, true})
        for (bool combo : {false, true})
        {
            ai.caster = caster; ai.combo = combo; ai.near = 5;
            assert(selector.Calculate() == &trash);
        }
    std::cout << "Raid boss focus requires actual MT engagement; no selection-only pulls passed\n";
}
