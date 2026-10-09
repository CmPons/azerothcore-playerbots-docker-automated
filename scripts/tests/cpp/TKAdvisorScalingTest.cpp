// Native aura handler, stat calculation, scaling manager and advisor methods are injected.
// World objects, cast delivery, death application and the scheduler are bounded doubles.
#include <chrono>
using namespace std::chrono_literals;
using SpellSchoolMask = uint32;
constexpr uint32 SPELL_RESURRECTION = 36450, SPELL_PERMANENT_FEIGN_DEATH = 29266,
    SPELL_KAEL_PHASE_TWO = 36709, NPC_KAELTHAS = 19622;
constexpr uint32 UNIT_STAND_STATE_STAND = 0, REACT_AGGRESSIVE = 2;
enum class SelectTargetMethod { Random };
struct SpellInfo { uint32 Id; };
struct TaskContext {};
struct Scheduler
{
    unsigned delay = 0;
    std::function<void(TaskContext)> pending;
    void CancelAll() { pending = {}; }
    template<class Fn> void Schedule(std::chrono::seconds after, Fn fn)
    {
        delay = after.count();
        pending = fn;
    }
    void Run()
    {
        assert(pending);
        auto action = std::move(pending);
        pending = {};
        action({});
    }
};
struct advisor_baseAI
{
    Creature* me;
    Scheduler scheduler{};
    bool _preventDeath = true, _feigning = false;
    unsigned scheduled = 0, threatResets = 0;
    uint32 phaseSignal = 0, feignSpell = 0;
    void DoCastAOE(uint32 spell, bool) { phaseSignal = spell; }
    void DoCastSelf(uint32 spell, bool) { feignSpell = spell; }
    void DoResetThreatList() { ++threatResets; }
    Unit* SelectTarget(SelectTargetMethod, uint32) { return nullptr; }
    void AttackStart(Unit*) { assert(false); }
    void ScheduleEvents() { ++scheduled; }
    void DamageTaken(Unit*, uint32&, DamageEffectType, SpellSchoolMask);
    void SpellHit(Unit*, SpellInfo const*);
};
/* ADVISOR_METHODS */

struct AdvisorData { uint32 entry, native, scaled, revivedScaled; };
AdvisorData const Advisors[] = {
/* ADVISOR_DATA */
};
void ResurrectionAura(Creature& advisor, bool apply = true)
{
    AuraApplication application{&advisor};
    AuraEffect{100}.HandleAuraModIncreaseHealthPercent(&application, AURA_EFFECT_HANDLE_STAT, apply);
}
void NativeBaseline(Creature& creature, uint32 hp)
{
    creature.deathState = DeathState::Alive;
    creature.SetCreateHealth(hp);
    creature.SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(hp));
    creature.SetFullHealth();
}
struct Scene
{
    Map map;
    Creature advisor{&map}, kael{&map};
    advisor_baseAI ai{&advisor};
    RaidScalingMgr& mgr = RaidScalingMgr::Instance();
    explicit Scene(AdvisorData row, bool enabled = true)
    {
        static uint32 nextInstance = 100;
        map.id = 550;
        mgr._originalSizes[550] = 25;
        map.instance = nextInstance++;
        advisor.entry = row.entry;
        advisor.data.id = row.entry;
        advisor.spawnId = row.entry;
        advisor.proto.rank = CREATURE_ELITE_WORLDBOSS;
        kael.entry = NPC_KAELTHAS;
        NativeBaseline(advisor, row.native);
        map.spawns[advisor.spawnId] = &advisor;
        map.GetObjectsStore().Insert<Creature>(advisor.GetGUID(), &advisor);
        if (enabled)
            Enable(mgr, map, mgr.MakeSettings(25, 10, true));
    }
    void Revive()
    {
        ResurrectionAura(advisor);
        SpellInfo spell{SPELL_RESURRECTION};
        ai.SpellHit(&kael, &spell);
    }
};
// SCENARIOS
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    if (scenario == "resurrection")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            Creature& advisor = s.advisor;
            s.mgr.OnCreatureAddWorld(&advisor);
            assert(advisor.GetMaxHealth() == row.scaled);
            uint32 hit = advisor.GetHealth();
            s.ai.DamageTaken(nullptr, hit, 0, 0);
            assert(hit == row.scaled - 1);
            advisor.SetHealth(advisor.GetHealth() - hit);
            assert(advisor.IsAlive() && advisor.GetHealth() == 1 && s.ai._feigning);
            assert(s.ai.phaseSignal == SPELL_KAEL_PHASE_TWO && s.ai.feignSpell == SPELL_PERMANENT_FEIGN_DEATH);
            assert(advisor.HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE));
            s.Revive();
            assert(advisor.GetMaxHealth() == row.revivedScaled);
            assert(advisor.GetHealth() == row.revivedScaled);
            assert(advisor.removedAura == SPELL_PERMANENT_FEIGN_DEATH);
            assert(s.ai.scheduler.delay == 6 && s.ai._preventDeath && s.ai._feigning);
            s.ai.scheduler.Run();
            assert(!s.ai._preventDeath && !s.ai._feigning);
            assert(!advisor.HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE));
            assert(advisor.zoneCombat && advisor.reactState == REACT_AGGRESSIVE);
            assert(s.ai.threatResets == 1 && s.ai.scheduled == 1);
            hit = advisor.GetHealth();
            s.ai.DamageTaken(nullptr, hit, 0, 0);
            assert(hit == advisor.GetHealth()); // Native second-phase lethal damage is not clamped.
            std::cout << row.entry << ": " << row.scaled << " -> " << advisor.GetMaxHealth() << '\n';
        }
    }
    else if (scenario == "reapply")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            s.mgr.OnCreatureAddWorld(&s.advisor);
            s.Revive();
            s.advisor.SetHealth(row.revivedScaled / 3);
            uint32 injured = s.advisor.GetHealth();
            for (unsigned i = 0; i < 20; ++i)
            {
                assert(s.mgr.SetMultiplier(&s.map, "boss", "hp", .4f, nullptr));
                assert(s.advisor.GetMaxHealth() == row.revivedScaled);
                assert(s.advisor.GetHealth() == injured);
                assert(s.advisor.GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT) == 2);
            }
            ResurrectionAura(s.advisor, false);
            assert(s.advisor.GetMaxHealth() == row.scaled);
            assert(std::abs(int(s.advisor.GetHealth()) - int(injured / 2)) <= 1);
            s.mgr.ApplyToMap(&s.map);
            assert(s.advisor.GetMaxHealth() == row.scaled);
        }
    }
    else if (scenario == "settings")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            s.mgr.OnCreatureAddWorld(&s.advisor);
            s.Revive();
            s.advisor.SetHealth(row.revivedScaled / 2);
            assert(s.mgr.SetMultiplier(&s.map, "boss", "hp", .5f, nullptr));
            assert(s.advisor.GetMaxHealth() == row.native && s.advisor.GetHealth() == row.native / 2);
            assert(s.mgr.SetMultiplier(&s.map, "trash", "hp", .1f, nullptr));
            assert(s.advisor.GetMaxHealth() == row.native); // Boss, not trash, multiplier.
            s.mgr.DisableForMap(&s.map);
            assert(s.advisor.GetMaxHealth() == row.native * 2 && s.advisor.GetHealth() == row.native);
            assert(s.advisor.GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT) == 2);
            assert(s.mgr.EnableForMap(&s.map, 10, nullptr));
            assert(s.advisor.GetMaxHealth() == row.revivedScaled);
            assert(s.advisor.GetHealth() == row.revivedScaled / 2);
            s.advisor.deathState = DeathState::Dead;
            s.advisor.SetHealth(0);
            s.mgr.DisableForMap(&s.map);
            assert(s.advisor.GetMaxHealth() == row.native * 2 && s.advisor.GetHealth() == 0);
        }
    }
    else if (scenario == "respawn")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            s.mgr.OnCreatureAddWorld(&s.advisor);
            for (unsigned i = 0; i < 20; ++i)
            {
                s.Revive();
                assert(s.advisor.GetMaxHealth() == row.revivedScaled);
                ResurrectionAura(s.advisor, false);
                NativeBaseline(s.advisor, row.native);
                s.mgr.OnCreatureRespawn(&s.advisor);
                assert(s.advisor.GetMaxHealth() == row.scaled && s.advisor.GetHealth() == row.scaled);
            }
            NativeBaseline(s.advisor, 200000); // Same GUID, genuinely rebuilt native base.
            s.mgr.OnCreatureRespawn(&s.advisor);
            assert(s.advisor.GetMaxHealth() == 80000);
            s.Revive();
            assert(s.advisor.GetMaxHealth() == 160000);
            // Existing aura during a rebuild also survives; no phase-dependent special case.
            NativeBaseline(s.advisor, 250000);
            s.mgr.OnCreatureRespawn(&s.advisor);
            assert(s.advisor.GetMaxHealth() == 200000);
        }
    }
    else if (scenario == "scope")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            s.advisor.flags = UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE;
            s.mgr.OnCreatureAddWorld(&s.advisor);
            assert(s.advisor.GetMaxHealth() == row.scaled); // Scaled before activation.
            s.mgr.DisableForMap(&s.map);
            for (bool Unit::* flag : {&Unit::controlled, &Unit::created})
            {
                s.advisor.*flag = true;
                Enable(s.mgr, s.map, s.mgr.MakeSettings(25, 10, true));
                s.mgr.OnCreatureAddWorld(&s.advisor);
                assert(s.advisor.GetMaxHealth() == row.native);
                s.advisor.*flag = false;
            }
            s.mgr._enabled = false;
            s.mgr.OnCreatureAddWorld(&s.advisor);
            assert(s.advisor.GetMaxHealth() == row.native);
            s.mgr._enabled = true;
            s.map.raid = false;
            s.mgr.OnCreatureAddWorld(&s.advisor);
            assert(s.advisor.GetMaxHealth() == row.native);
            s.map.raid = true;
            s.map.instance = 0;
            s.mgr.OnCreatureAddWorld(&s.advisor);
            assert(s.advisor.GetMaxHealth() == row.native);
        }
        Scene unrelated(Advisors[0]);
        unrelated.advisor.entry = NPC_KAELTHAS;
        unrelated.mgr.OnCreatureAddWorld(&unrelated.advisor);
        assert(unrelated.advisor.flat[BASE_VALUE] == Advisors[0].native);
        Scene wrongMap(Advisors[0]);
        wrongMap.map.id = 548;
        Enable(wrongMap.mgr, wrongMap.map, wrongMap.mgr.MakeSettings(25, 10, true));
        wrongMap.mgr.OnCreatureAddWorld(&wrongMap.advisor);
        assert(wrongMap.advisor.flat[BASE_VALUE] == Advisors[0].native);
    }
    else if (scenario == "late")
    {
        for (auto row : Advisors)
        {
            Scene s(row, false);
            s.Revive();
            assert(s.advisor.GetMaxHealth() == row.native * 2);
            s.advisor.SetHealth(row.native);
            assert(s.mgr.EnableForMap(&s.map, 10, nullptr));
            assert(s.advisor.GetMaxHealth() == row.revivedScaled);
            assert(s.advisor.GetHealth() == row.revivedScaled / 2);
            Scene untouched(row, false);
            untouched.mgr.OnCreatureAddWorld(&untouched.advisor);
            assert(untouched.advisor.GetMaxHealth() == row.native);
            ResurrectionAura(s.advisor, false);
            assert(s.advisor.GetMaxHealth() == row.scaled);
            s.mgr.DisableForMap(&s.map);
            assert(s.advisor.GetMaxHealth() == row.native); // Do not restore a cached, now-expired buff.
        }
    }
    else if (scenario == "old")
    {
        for (auto row : Advisors)
        {
            Scene s(row);
            s.mgr.OnCreatureAddWorld(&s.advisor);
            assert(s.advisor.GetMaxHealth() == row.scaled);
            s.Revive();
            assert(s.advisor.GetMaxHealth() == row.native * 2); // Unscaled base used by native aura.
            assert(s.mgr.SetMultiplier(&s.map, "boss", "hp", .4f, nullptr));
            assert(s.advisor.GetMaxHealth() == row.scaled); // Old workaround clobbers the buff's HP.
            assert(s.advisor.GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT) == 2);
        }
    }
    else
        return 1;
    std::cout << "Passed " << scenario << '\n';
    return 0;
}
