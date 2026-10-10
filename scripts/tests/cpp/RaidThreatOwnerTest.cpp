// Injected production selection and hold bodies; external world API doubles only.
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
using uint8 = uint8_t;
struct Unit;
struct Player;
struct ThreatManager
{
    bool enabled = true;
    std::map<Unit*, float> values;
    float GetThreat(Unit* p) { return values[p]; }
    bool CanHaveThreatList() { return enabled; }
};
struct Unit
{
    bool alive = true, world = true, phase = true, friendly = true, controlled = false, gm = false;
    bool boss = true, combat = true, twins = false;
    int map = 1;
    Unit* victim = nullptr;
    ThreatManager threat;
    bool IsAlive() { return alive; }
    bool IsInWorld() { return world; }
    int GetMap() { return map; }
    bool InSamePhase(Unit*) { return phase; }
    bool GetCharmerGUID() { return controlled; }
    bool IsGameMaster() { return gm; }
    bool IsFriendlyTo(Unit* p) { return p->friendly; }
    bool IsInCombat() { return combat; }
    ThreatManager& GetThreatMgr() { return threat; }
};
struct GroupReference
{
    Player* player = nullptr;
    GroupReference* following = nullptr;
    Player* GetSource() { return player; }
    GroupReference* next() { return following; }
};
struct Group
{
    bool raid = true;
    std::vector<GroupReference> refs;
    bool isRaidGroup() { return raid; }
    GroupReference* GetFirstMember() { return refs.empty() ? nullptr : &refs[0]; }
    void Set(std::initializer_list<Player*> players)
    {
        refs.clear();
        for (auto* p : players) refs.push_back({p, nullptr});
        for (unsigned i = 1; i < refs.size(); ++i) refs[i-1].following = &refs[i];
    }
};
struct Player : Unit
{
    Group* group = nullptr;
    bool tank = false;
    Group* GetGroup() { return group; }
};
struct PlayerbotAI
{
    Player *bot = nullptr, *mt = nullptr, *twinsOwner = nullptr;
    Player* GetBot() { return bot; }
    static bool IsTank(Player* p) { return p->tank; }
};
namespace TempleOfAhnQirajHelpers
{
constexpr int AQT_DATA_VEKLOR = 1, AQT_DATA_VEKNILASH = 2;
bool IsTwinsBossTarget(Player*, Unit* target) { return target->twins; }
Unit* GetTwin(Player*, int) { return nullptr; }
Unit* GetTwinsTank(Player* p, int);
}
Player* twinOwner = nullptr;
Unit* TempleOfAhnQirajHelpers::GetTwinsTank(Player*, int) { return twinOwner; }
namespace ai::threat
{
[[maybe_unused]] Unit* GetMainTank(PlayerbotAI* ai) { return ai->mt; }
bool IsTauntImmuneRaidBoss(Unit* u) { return u && u->boss; }
Unit* GetThreatVictim(Unit* u) { return u->victim; }
// PRODUCTION
}
int main()
{
    Player mt, ot, dps, healer, stranger;
    Group group;
    for (auto* p : {&mt, &ot, &dps, &healer}) p->group = &group;
    mt.tank = ot.tank = stranger.tank = true;
    group.Set({&mt, &ot, &dps, &healer, nullptr});
    PlayerbotAI ai{&dps, &mt, nullptr};
    Unit boss;
    boss.victim = &ot;
    boss.threat.values = {{&mt, 1}, {&ot, 1000}, {&dps, 100}, {&healer, 2000}, {&stranger, 9000}};
    using namespace ai::threat;
    assert(GetHighestThreatTank(&ai, &boss) == &ot);
    // Original MT-only gate holds against one point despite Ari's established ownership.
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.threat.values[&dps] = 699;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.threat.values[&dps] = 700;
    assert(ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.threat.values[&dps] = 100;
    boss.threat.values[&mt] = 2000;
    assert(GetHighestThreatTank(&ai, &boss) == &mt);
    boss.threat.values[&mt] = 1;
    for (int mode = 0; mode < 7; ++mode)
    {
        ot.alive = mode != 0; ot.world = mode != 1; ot.map = mode == 2 ? 2 : 1;
        ot.phase = mode != 3; ot.controlled = mode == 4; ot.friendly = mode != 5; ot.gm = mode == 6;
        assert(GetHighestThreatTank(&ai, &boss) == &mt);
    }
    ot.alive = ot.world = ot.phase = ot.friendly = true; ot.controlled = ot.gm = false; ot.map = 1;
    boss.victim = &dps;
    assert(ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.victim = &ot;
    boss.threat.values.clear();
    assert(ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    // Preserve existing non-Twins zero-tank/positive-bot branch; no invented ownership.
    boss.threat.values[&dps] = 1;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    mt.alive = ot.alive = false;
    assert(!GetHighestThreatTank(&ai, &boss));
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    mt.alive = ot.alive = true;
    dps.tank = true;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    dps.tank = false;
    boss.boss = false;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.boss = true;
    group.raid = false;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    group.raid = true;
    // Caster-tank Twins exception must not be replaced by ordinary physical tanks.
    boss.twins = true; twinOwner = &healer;
    boss.threat.values = {{&mt, 9000}, {&ot, 8000}, {&healer, 100}, {&dps, 70}};
    assert(ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    boss.threat.values[&dps] = 69;
    assert(!ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    twinOwner = nullptr;
    assert(ShouldHoldDamageOnTauntImmuneBoss(&ai, &boss, 70));
    assert(!GetHighestThreatTank(nullptr, &boss));
    assert(!GetHighestThreatTank(&ai, nullptr));
    std::cout << "Per-target tank ownership, threshold, invalid owners, opening and Twins guards passed\n";
}
