// Offline lifecycle test double. The complete production boss class is injected below.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <vector>

using uint32 = uint32_t;
using int32 = int32_t;
using namespace std::chrono_literals;
using Duration = std::chrono::milliseconds;
struct TaskContext;
struct Job
{
    Duration due;
    uint32 group;
    std::function<void(TaskContext)> callback;
};
struct Scheduler
{
    Duration now{0};
    std::vector<Job> jobs;
    template<class F> Scheduler& Schedule(Duration delay, F callback)
    {
        jobs.push_back({now + delay, 0, callback});
        return *this;
    }
    template<class F> Scheduler& Schedule(Duration delay, uint32 group, F callback)
    {
        jobs.push_back({now + delay, group, callback});
        return *this;
    }
    template<class F> Scheduler& Schedule(Duration lo, Duration, uint32 group, F callback)
    {
        return Schedule(lo, group, callback);
    }
    void CancelAll() { jobs.clear(); }
    void CancelGroup(uint32 group) { std::erase_if(jobs, [=](Job const& job) { return job.group == group; }); }
    void RescheduleGroup(uint32 group, Duration delay)
    {
        for (auto& job : jobs)
            if (job.group == group)
                job.due = now + delay;
    }
    void DelayAll(Duration delay) { for (auto& job : jobs) job.due += delay; }
    void Update(uint32 diff);
};
struct TaskContext
{
    Scheduler* scheduler;
    Job job;
    void Repeat(Duration lo, Duration) { scheduler->Schedule(lo, job.group, job.callback); }
    void Repeat(Duration delay) { scheduler->Schedule(delay, job.group, job.callback); }
};
void Scheduler::Update(uint32 diff)
{
    now += Duration(diff);
    for (unsigned count = 0; count < 100; ++count)
    {
        auto next = std::min_element(jobs.begin(), jobs.end(), [](auto const& a, auto const& b)
        {
            return a.due < b.due;
        });
        if (next == jobs.end() || next->due > now)
            return;
        Job job = *next;
        jobs.erase(next);
        job.callback(TaskContext{this, job});
    }
    assert(false);
}
constexpr uint32 DATA_LEOTHERAS_THE_BLIND = 2;
constexpr uint32 REACT_PASSIVE = 0, REACT_AGGRESSIVE = 2;
constexpr uint32 UNIT_STAND_STATE_STAND = 0, UNIT_STAND_STATE_KNEEL = 8;
constexpr uint32 UNIT_FLAG_NOT_SELECTABLE = 1, UNIT_STATE_CASTING = 2;
constexpr uint32 SPELLVALUE_MAX_TARGETS = 0, BASE_ATTACK = 0, SPELL_CAST_OK = 0;
struct Unit { uint32 GetGUID() const { return 42; } };
struct FormationMember
{
    uint32 GetEntry() const { return 21806; }
};
struct CreatureGroup
{
    unsigned livingBinders = 3, knownBinders = 3;
    unsigned respawns = 0;
    FormationMember binders[3];
    auto GetMembers()
    {
        std::vector<std::pair<FormationMember*, unsigned>> result;
        for (unsigned i = 0; i < knownBinders; ++i)
            result.push_back({&binders[i], 0});
        return result;
    }
    bool IsAnyMemberAlive(bool ignoreLeader)
    {
        assert(ignoreLeader);
        return livingBinders != 0;
    }
};
struct Motion
{
    unsigned chases = 0;
    void Clear() {}
    void MoveChase(Unit* target, float, float) { assert(target); ++chases; }
};
struct Creature : Unit
{
    CreatureGroup* formation = nullptr;
    Unit* victim = nullptr;
    std::set<uint32> auras;
    std::vector<uint32> casts;
    Motion motion;
    uint32 react = REACT_PASSIVE, stand = UNIT_STAND_STATE_KNEEL, flags = 0;
    bool alive = true, engaged = false, evading = false;
    unsigned zoneCalls = 0, equipmentLoads = 0, threatResets = 0;
    std::function<void()> onZone;
    bool IsAlive() const { return alive; }
    bool IsEngaged() const { return engaged; }
    bool IsInEvadeMode() const { return evading; }
    CreatureGroup* GetFormation() { return formation; }
    bool HasAura(uint32 id) const { return auras.contains(id); }
    void RemoveAurasDueToSpell(uint32 id) { auras.erase(id); }
    void RemoveAllAuras() { auras.clear(); }
    void SetReactState(uint32 value) { react = value; }
    uint32 GetReactState() const { return react; }
    void SetStandState(uint32 value) { stand = value; }
    void SetUnitFlag(uint32 value) { flags |= value; }
    void RemoveUnitFlag(uint32 value) { flags &= ~value; }
    void LoadEquipment(int = 1, bool = false) { ++equipmentLoads; }
    uint32 GetNativeDisplayId() const { return 1; }
    uint32 GetDisplayId() const { return HasAura(37673) ? 2 : 1; }
    void SetInCombatWithZone() { ++zoneCalls; engaged = true; if (onZone) onZone(); }
    void ClearTarget() {}
    void SendMeleeAttackStop() {}
    void SendMeleeAttackStart(Unit*) {}
    Motion* GetMotionMaster() { return &motion; }
    void StopMoving() {}
    void ResumeChasingVictim() { if (victim) ++motion.chases; }
    Unit* GetVictim() { return victim; }
    void SetTarget(uint32) {}
    void InterruptNonMeleeSpells(bool) {}
    void CastCustomSpell(uint32 spell, uint32, uint32, Creature*, bool) { casts.push_back(spell); }
    bool IsWithinDistInMap(Unit* target, float) { assert(target); return false; }
    void AddThreat(Unit*, float) {}
    bool HasUnitState(uint32) const { return false; }
    bool isAttackReady(uint32) const { return true; }
    void setAttackTimer(uint32, uint32) {}
};
struct ScriptedAI
{
    virtual ~ScriptedAI() = default;
    virtual void Reset() {}
    virtual void AttackStart(Unit*) {}
    virtual void DoAction(int32) {}
    virtual void JustEngagedWith(Unit*) {}
    virtual void UpdateAI(uint32) {}
};
struct BossAI : ScriptedAI
{
    Creature* me;
    Scheduler scheduler;
    std::function<void()> healthCheck;
    unsigned talks = 0, melees = 0, engagements = 0, resets = 0;
    BossAI(Creature* creature, uint32) : me(creature) {}
    void Reset() override { ++resets; scheduler.CancelAll(); healthCheck = {}; }
    void JustEngagedWith(Unit*) override { ++engagements; me->engaged = true; }
    void ScheduleHealthCheckEvent(uint32 pct, std::function<void()> callback)
    {
        assert(pct == 15); healthCheck = callback;
    }
    void DoCastSelf(uint32 spell, bool = false) { me->casts.push_back(spell); me->auras.insert(spell); }
    uint32 DoCastVictim(uint32 spell) { me->casts.push_back(spell); return SPELL_CAST_OK; }
    void Talk(uint32) { ++talks; }
    void DoResetThreatList() { ++me->threatResets; }
    void AttackStartCaster(Unit*, float) {}
    bool UpdateVictim() { return me->victim != nullptr; }
    void DoMeleeAttackIfReady() { ++melees; }
};

// PRODUCTION_BOSS

int main()
{
    Unit player;
    CreatureGroup formation;
    Creature creature;
    creature.formation = &formation;
    creature.victim = &player;
    boss_leotheras_the_blind ai(&creature);
    creature.onZone = [&] { ai.JustEngagedWith(&player); };

    // Fresh grid loading must not treat an empty/partial formation as a defeated gate.
    formation.knownBinders = formation.livingBinders = 0;
    ai.Reset();
    ai.JustEngagedWith(&player);
    assert(creature.react == REACT_PASSIVE && ai.scheduler.jobs.empty());
    creature.engaged = false;
    formation.knownBinders = 2;
    ai.Reset();
    ai.JustEngagedWith(&player);
    assert(creature.react == REACT_PASSIVE && ai.scheduler.jobs.empty());
    creature.engaged = false;
    formation.knownBinders = formation.livingBinders = 3;

    // Initial gate and partial guard wipes must not be bypassed.
    ai.Reset();
    assert(creature.react == REACT_PASSIVE && ai.scheduler.jobs.empty());
    ai.DoAction(ACTION_CHECK_SPELLBINDERS);
    assert(creature.zoneCalls == 0 && ai.scheduler.jobs.empty());
    formation.livingBinders = 1;
    ai.JustEngagedWith(&player);
    assert(ai.scheduler.jobs.empty());
    creature.engaged = false;
    ai.Reset();
    assert(creature.react == REACT_PASSIVE);

    // Final guard death starts once, including a synchronous zone-combat callback.
    formation.livingBinders = 0;
    ai.DoAction(ACTION_CHECK_SPELLBINDERS);
    assert(creature.react == REACT_AGGRESSIVE && creature.stand == UNIT_STAND_STATE_STAND);
    assert(creature.zoneCalls == 1 && ai.scheduler.jobs.size() == 3 && ai.talks == 1);
    ai.DoAction(ACTION_CHECK_SPELLBINDERS);
    ai.JustEngagedWith(&player);
    assert(creature.zoneCalls == 1 && ai.scheduler.jobs.size() == 3 && ai.talks == 1);
    ai.UpdateAI(25050);
    assert(creature.HasAura(SPELL_WHIRLWIND) && ai.melees == 1);

    // Exercise the actual demon phase and a wipe during it.
    ai.DemonTime();
    assert(creature.HasAura(SPELL_METAMORPHOSIS));
    creature.engaged = false;
    ai.Reset();
    assert(!creature.HasAura(SPELL_METAMORPHOSIS));
    assert(!creature.HasAura(SPELL_WHIRLWIND));
    assert(creature.react == REACT_AGGRESSIVE && ai.scheduler.jobs.empty());
    assert(creature.zoneCalls == 1 && !creature.engaged);
    ai.JustEngagedWith(&player);
    assert(ai.scheduler.jobs.size() == 3 && ai.talks == 2);

    // Wipe in the final split's kneeling/non-selectable window.
    ai.healthCheck();
    assert(creature.react == REACT_PASSIVE && creature.flags & UNIT_FLAG_NOT_SELECTABLE);
    creature.engaged = false;
    ai.Reset();
    assert(creature.react == REACT_AGGRESSIVE && creature.stand == UNIT_STAND_STATE_STAND);
    assert(!(creature.flags & UNIT_FLAG_NOT_SELECTABLE) && ai.scheduler.jobs.empty());
    ai.JustEngagedWith(&player);
    assert(ai.scheduler.jobs.size() == 3);
    ai.healthCheck();
    ai.scheduler.Update(4000);
    assert(creature.HasAura(SPELL_SUMMON_SHADOW_OF_LEOTHERAS));
    ai.scheduler.Update(2000);
    assert(creature.react == REACT_AGGRESSIVE && !(creature.flags & UNIT_FLAG_NOT_SELECTABLE));

    // A prior real release remains valid when the dead guards unload.
    formation.knownBinders = 0;
    // Repeated repulls neither need another binder death nor duplicate schedules.
    for (unsigned i = 0; i < 20; ++i)
    {
        creature.engaged = false;
        ai.Reset();
        assert(ai.scheduler.jobs.empty() && !creature.engaged);
        ai.JustEngagedWith(&player);
        ai.DoAction(ACTION_CHECK_SPELLBINDERS);
        assert(ai.scheduler.jobs.size() == 3);
        assert(std::count_if(ai.scheduler.jobs.begin(), ai.scheduler.jobs.end(), [&](Job const& job)
        {
            return job.due - ai.scheduler.now == 10min;
        }) == 1);
    }
    assert(formation.respawns == 0);

    // Refused resets, a missing formation, and an evade-time notification fail safely.
    unsigned resetCount = ai.resets;
    ai.Reset();
    assert(ai.resets == resetCount);
    creature.engaged = false;
    creature.alive = false;
    ai.Reset();
    assert(ai.resets == resetCount);
    creature.alive = true;
    creature.formation = nullptr;
    ai.Reset();
    ai.JustEngagedWith(&player);
    assert(creature.react == REACT_PASSIVE && ai.scheduler.jobs.empty());
    creature.engaged = false;
    creature.formation = &formation;
    ai.Reset();
    creature.evading = true;
    ai.DoAction(ACTION_CHECK_SPELLBINDERS);
    ai.JustEngagedWith(&player);
    assert(ai.scheduler.jobs.empty());
    ai.MoveToTargetIfOutOfRange(nullptr);

    // Recreated AI can recognize three loaded dead guards without a remembered release.
    Creature fresh;
    formation.knownBinders = 3;
    fresh.formation = &formation;
    fresh.victim = &player;
    boss_leotheras_the_blind recreated(&fresh);
    recreated.Reset();
    assert(fresh.react == REACT_AGGRESSIVE && recreated.scheduler.jobs.empty());
    recreated.JustEngagedWith(&player);
    assert(recreated.scheduler.jobs.size() == 3);
    std::cout << "Leotheras initial gate, phases, 20 repulls and refusal cases passed\n";
}
