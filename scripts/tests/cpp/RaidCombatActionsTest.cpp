#include "ActionFixture.h"
#include "RaidCombatActions.inc"
#include "CurrentTargetGet.inc"

int main(int argc, char** argv)
{
    PlayerbotAI ai;
    ai.bot.guid = ObjectGuid(1);
    CthunPolicy::Scope scope("531-1-1", "/missing", "/missing");
    ai.bot.map.CustomData.scope = &scope;
    scope.active = true;
    scope.api = 2;
    scope.generation = 9;
    scope.plannedAt = 100;
    Unit target;
    Spell cast(&target, &manager.info, 0);
    target.current = &cast;
    units[2] = &target;
    RaidCombat::Snapshot snapshot;
    snapshot.count = 1;
    snapshot.entityCount = 1;
    snapshot.sequence = 1;
    snapshot.entities[0].guid = 2;
    snapshot.entities[0].cast = 7;
    snapshot.entities[0].castSpell = 1;
    RaidCombat::Intent intent;
    intent.operation = RaidCombat::Operation::Interrupt;
    intent.spell = 1;
    intent.actionTarget = 1;
    auto apply = [&]
    {
        ++snapshot.sequence;
        RaidCombat::ApplyCombatAction(ai, snapshot, intent, 9, 100);
    };
    if (argc > 1 && std::string(argv[1]) == "engagement")
    {
        target.engaged = false;
        apply();
        assert(admitted == 0); // Original P1: legal harmful requests must not initiate an unengaged pull.
        std::cout << "no-new-pull admission regression passed\n";
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "expiry")
    {
        clockNow = 1099;
        ai.bot.visibility = [&] { clockNow = 1100; };
        apply();
        assert(admitted == 0); // Original P1: visibility callback crosses expiry before final admission.
        std::cout << "post-callback expiry admission regression passed\n";
        return 0;
    }
    Unit manual;
    manual.guid = ObjectGuid(3);
    units[3] = &manual;
    scope.snapshot.raid.count = 1;
    scope.snapshot.raid.entityCount = 1;
    scope.snapshot.raid.members[0].unit.guid = 1;
    scope.snapshot.raid.entities[0].guid = 2;
    scope.plan.raid.intents[0].target = 1;
    CurrentTargetValue current{&ai, &ai.bot, ObjectGuid(3)};
    ai.raidCombat.scheduled = true;
    assert(current.Get() == &target);
    ai.context.priority.value.push_back(ObjectGuid(3));
    assert(current.Get() == &manual); // Manual/raid-marker native target priorities win.
    ai.context.priority.value.clear();
    ai.raidCombat.scheduled = false;
    assert(current.Get() == &manual);
    apply();
    assert(admitted == 1 && effects == 1 && ai.raidCombat.actionReceipt == 12);
    RaidCombat::ApplyCombatAction(ai, snapshot, intent, 9, 100);
    assert(admitted == 1); // Same-frame duplicate is retired before callbacks.
    ai.bot.visibility = [&] { ++ai.bot.control; };
    apply();
    assert(admitted == 1);
    ai.bot.visibility = [&] { ++target.control; };
    apply();
    assert(admitted == 1);
    ai.bot.visibility = [&] { scope.generation = 10; };
    apply();
    assert(admitted == 1);
    ai.bot.visibility = {};
    scope.generation = 9;
    ai.bot.current = &cast;
    apply();
    assert(admitted == 1); // No manual/native cast cancellation.
    ai.bot.current = nullptr;
    ai.bot.cooldown.busy = true;
    apply();
    assert(admitted == 1);
    ai.bot.cooldown.busy = false;
    ai.eligible = false;
    apply();
    assert(admitted == 1); // Manual/stay/passive/CC/lease eligibility remains authoritative.
    ai.eligible = true;
    ai.bot.healer = true;
    snapshot.members[0].unit.alive = true;
    snapshot.members[0].unit.health = 20;
    apply();
    assert(admitted == 1 && ai.raidCombat.actionReceipt == 11);
    ai.bot.healer = false;
    cast.id = 8;
    apply();
    assert(admitted == 1); // Same spell ID with a replacement native cast is not the observed interrupt.
    cast.id = 7;
    intent.operation = RaidCombat::Operation::Dispel;
    intent.aura = 99;
    manager.info.Effects[0].Effect = SPELL_EFFECT_DISPEL;
    apply();
    assert(admitted == 2 && effects == 2);
    target.aura = false;
    apply();
    assert(admitted == 2);
    std::cout << "exact combat action adapter identity/replay/busy/manual/triage/dispel tests passed\n";
}
