#include "CthunPolicyRuntime.h"
#include "RaidCombatAdmission.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using CthunPolicy::Runtime;
namespace
{
CthunPolicy::Snapshot Realistic()
{
    CthunPolicy::Snapshot snapshot;
    auto& raid = snapshot.raid;
    raid.map = 531;
    raid.instance = 3;
    raid.sequence = 1;
    raid.sampledAt = 100;
    raid.combat = true;
    raid.count = RaidCombat::MaxRoster;
    raid.entityCount = RaidCombat::MaxEntities;
    for (unsigned i = 0; i < raid.count; ++i)
    {
        auto& member = raid.members[i];
        member.eligible = i != 0;
        member.human = i == 0;
        member.healer = i % 4 == 0;
        member.unit.guid = i + 1;
        member.unit.x = -8625.0f + float(i);
        member.unit.y = 1974;
        member.unit.z = 100.713f;
        member.unit.alive = true;
        member.unit.health = 100;
        member.unit.auraCount = RaidCombat::MaxAuras;
    }
    for (unsigned i = 0; i < raid.entityCount; ++i)
    {
        auto& unit = raid.entities[i];
        unit.guid = (uint64_t(1) << 60) + i;
        unit.x = -8578.79f;
        unit.y = 1986.18f;
        unit.z = 100.446f;
        unit.alive = true;
        unit.health = 100;
        unit.entry = i == 0 ? 15589 : 15726;
        unit.attackable = unit.engaged = i != 95;
        unit.auraCount = RaidCombat::MaxAuras;
        for (unsigned j = 0; j < unit.auraCount; ++j)
            unit.auras[j].spell = 100 + j;
    }
    return snapshot;
}
std::string const release = "{movement=0,target=0,operation=0,spell=0,aura=0,action_target=0}";
}
int main(int argc, char** argv)
{
    auto snapshot = Realistic();
    CthunPolicy::Plan plan;
    if (argc > 1)
    {
        std::ifstream input(argv[1], std::ios::binary);
        std::string source((std::istreambuf_iterator<char>(input)), {});
        Runtime runtime;
        if (!runtime.Load(source) || runtime.Api() != 2 || !runtime.Evaluate(snapshot, plan))
        {
            std::cerr << argv[1] << ": " << runtime.Error() << " (requires combat API2)\n";
            return 1;
        }
        for (unsigned i = 0; i < 100; ++i)
            assert(runtime.Evaluate(snapshot, plan));
        if (argc > 2)
        {
            assert(plan.raid.intents[0].movement == RaidCombat::Positioning::Release);
            assert(plan.raid.intents[1].movement == RaidCombat::Positioning::Ground);
        }
        CthunPolicy::Snapshot empty;
        assert(runtime.Evaluate(empty, plan));
        // Visible unattackable hazard survives marshalling; it cannot be selected as an attack priority.
        std::cout << "API2 40-member/96-entity/8-aura real Lua budget passed; memory=" << runtime.Memory() << '\n';
        return 0;
    }
    std::string const module = "return {api=2,plan=function(s) local out={} "
        "for i,m in ipairs(s.members) do out[i]=" + release + " end ";
    for (std::string const body : {"out[2].movement=2;out[2].x=0/0;out[2].y=0;out[2].z=0",
        "out[2].target=96", "out[2].target=97", "out[1].movement=1", "out[2].surprise=true",
        "out[2].movement='1'", "out[2].operation=3", "out[2].operation=1",
        "out[2].spell=100001", "out[2].action_target=-1", "out[41]={}"})
    {
        Runtime runtime;
        assert(runtime.Load(module + body + ";return out end}"));
        plan.raid.intents[0].target = 777;
        assert(!runtime.Evaluate(snapshot, plan));
        assert(plan.raid.intents[0].target == 777);
        assert(!runtime.Error().empty());
    }
    for (std::string const body : {"while true do end", "local x='xxxxxxxx' for i=1,30 do x=x..x end"})
    {
        Runtime runtime;
        assert(runtime.Load(module + body + ";return out end}"));
        assert(!runtime.Evaluate(snapshot, plan));
        assert(runtime.Memory() <= CthunPolicy::MEMORY_LIMIT);
    }
    Runtime observed;
    assert(observed.Load(module + "assert(#s.entities==96 and s.entities[96].alive and "
        "not s.entities[96].attackable);return out end}"));
    assert(observed.Evaluate(snapshot, plan));
    std::cout << "API2 marshal, transactional parser, invalid output and finite budgets passed\n";
}
