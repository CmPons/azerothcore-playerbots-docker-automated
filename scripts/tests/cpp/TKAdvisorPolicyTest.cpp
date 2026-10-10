// Executes the authored policy and parser in the actual production Lua runtime.
#include "CthunPolicyRuntime.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
using CthunPolicy::Runtime;
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::ifstream file(argv[1]);
    std::string source((std::istreambuf_iterator<char>(file)), {});
    Runtime runtime;
    assert(runtime.Load(source));
    CthunPolicy::Snapshot s;
    auto& r = s.raid;
    r.map = 550; r.instance = 42; r.combat = true; r.count = 10; r.entityCount = 5;
    for (unsigned i = 0; i < r.count; ++i)
    {
        auto& m = r.members[i];
        m.unit.guid = i + 1; m.unit.alive = true; m.unit.health = 100;
        m.human = i == 0; m.eligible = i != 0; m.tank = i < 2;
        m.mainTank = i == 0; m.healer = i >= 2 && i <= 4;
        m.melee = i < 2 || i == 5 || i == 6;
    }
    unsigned const entries[] = {20062,20060,20063,20064,19622};
    for (unsigned i = 0; i < r.entityCount; ++i)
    {
        auto& e = r.entities[i];
        e.guid = 100+i; e.entry = entries[i]; e.alive = e.attackable = e.engaged = e.selectable = true;
        e.health = 100; e.visibleTo = 1023;
    }
    CthunPolicy::Plan plan;
    auto evaluate = [&] { assert(runtime.Evaluate(s,plan)); };
    auto check = [&](unsigned target)
    {
        evaluate();
        for (unsigned i = 0; i < 5; ++i) assert(plan.raid.intents[i].target == 0);
        for (unsigned i = 5; i < 10; ++i) assert(plan.raid.intents[i].target == target);
        for (unsigned i = 0; i < 10; ++i)
        {
            assert(plan.raid.intents[i].movement == RaidCombat::Positioning::Release);
            assert(plan.raid.intents[i].operation == RaidCombat::Operation::None);
        }
    };
    check(4); // Thaladred, not Capernian (array deliberately in reverse order).
    assert(plan.raid.intents[0].tankTargetCount == 2);
    assert(plan.raid.intents[0].tankTargets[0] == 2); // Sanguinar
    assert(plan.raid.intents[0].tankTargets[1] == 1); // Capernian
    r.entities[3].selectable = false; // feigning alive advisor must not stay the target
    check(3); // Telonicus
    r.entities[2].health = 0;
    check(2); // Sanguinar
    r.entities[1].alive = false;
    check(1); // Capernian last, same preferred target for hunter/casters/melee
    r.entities[0].engaged = false;
    check(0); // never select untouched/unengaged units
    r.entities[0].engaged = true;
    r.members[0].mainTank = false;
    check(0); // no eligible human MT: fall back, never silently assign a bot
    r.members[0].mainTank = true;
    r.gaps = 2; check(0); r.gaps = 0;
    r.members[5].eligible = false; evaluate(); assert(!plan.raid.intents[5].target);
    r.members[5].eligible = true;
    r.combat = false; check(0); // wipe/reset clears the revived-phase latch
    r.combat = true; check(0); // single-advisor phase left alone
    r.entities[0].auraCount = 1; r.entities[0].auras[0].spell = 36450;
    check(1); // join/reload into late revived phase recognized from resurrection aura
    r.entities[4] = r.entities[0]; check(0); // duplicate advisor identity: no arbitrary choice
    r.entities[4].entry = 19622;
    for (unsigned n = 0; n < 100; ++n) check(1);
    // Capernian last: safe-distance melee handling only before Kael first activates.
    r.combat = false; check(0); r.combat = true;
    r.entities[4].attackable = false;
    evaluate();
    assert(plan.raid.intents[5].target == 1);
    assert(plan.raid.intents[5].movement == RaidCombat::Positioning::Ground);
    r.members[5].unit.x = 30;
    evaluate(); assert(plan.raid.intents[5].movement == RaidCombat::Positioning::Hold);
    r.gaps = 8; evaluate(); assert(plan.raid.intents[5].movement == RaidCombat::Positioning::Release);
    r.gaps = 0;
    r.entities[4].attackable = true; check(1); // release for MC and other active-Kael mechanics
    r.entities[4].attackable = false; check(1); // never reacquire during a later invulnerable transition

    // Optional extension is transactional and does not grant control of human actors.
    std::string base = "return {api=2,plan=function(s) local o={} for i=1,#s.members do "
        "o[i]={movement=0,target=0,operation=0,spell=0,aura=0,action_target=0} end ";
    for (std::string body : {"o[1].tank_targets={1,1}", "o[6].tank_targets={1}",
        "o[1].tank_targets={0}", "o[1].tank_targets={6}", "o[1].tank_targets={'1'}",
        "o[1].tank_targets={1,2,3,4,5}", "o[1].tank_targets={x=1}",
        "o[1].tank_targets={1};o[2].tank_targets={1}",
        "o[1].tank_targets={1};o[1].movement=1", "o[1].tank_targets={1};o[1].target=1"})
    {
        Runtime bad; assert(bad.Load(base+body+";return o end}"));
        plan.raid.intents[0].target = 777;
        assert(!bad.Evaluate(s,plan)); assert(plan.raid.intents[0].target == 777);
    }
    Runtime valid;
    assert(valid.Load(base+"o[1].tank_targets={1};return o end}"));
    assert(valid.Evaluate(s,plan)); assert(plan.raid.intents[0].tankTargetCount == 1);
    r.entities[0].selectable = false; assert(!valid.Evaluate(s,plan));
    r.entities[0].selectable = true; r.entities[0].engaged = false; assert(!valid.Evaluate(s,plan));
    r.entities[0].engaged = true; r.members[0].unit.alive = false; assert(!valid.Evaluate(s,plan));
    r.members[0].unit.alive = true;
    r.count = RaidCombat::MaxRoster; r.entityCount = RaidCombat::MaxEntities;
    for (unsigned i = 10; i < r.count; ++i)
    {
        auto& m = r.members[i];
        m.unit.guid = i+1; m.unit.alive = true; m.unit.health = 100; m.eligible = true;
    }
    for (unsigned i = 5; i < r.entityCount; ++i)
    {
        auto& e = r.entities[i];
        e.guid = 100+i; e.entry = 17000+i; e.alive = e.attackable = e.engaged = e.selectable = true;
        e.health = 100;
    }
    for (unsigned i = 0; i < 100; ++i) evaluate();
    for (unsigned i = 5; i < r.count; ++i) assert(plan.raid.intents[i].target == 1);
    std::cout << "TK shared order, human ownership, phase/fallback, finisher safety and schema guards passed\n";
}
