// Actual Ouro classes, native victim/engagement/threat methods and TaskScheduler with offline API doubles.
// Geometry, spell effects, combat references and instance storage are deliberately not a live simulation.
#include "TaskScheduler.h"
#include "Errors.h"
#include <cstdlib>
#include <cmath>
#include <numbers>
#include <optional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
using uint8 = uint8_t;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x " at " << __LINE__ << '\n'; std::exit(1); } } while (false)
#define LOG_ERROR(...) ((void)0)
/* ENUMS */
enum { UNIT_STATE_ROOT = 1, UNIT_STATE_CASTING = 2, UNIT_FLAG_NOT_SELECTABLE = 4,
       UNIT_FLAG_NON_ATTACKABLE = 8, UNIT_FLAG_PLAYER_CONTROLLED = 16, UNIT_FLAG_IMMUNE_TO_PC = 32,
       UNIT_FLAG_IMMUNE_TO_NPC = 64, REACT_AGGRESSIVE = 0, REACT_PASSIVE = 1 };
enum EncounterState { NOT_STARTED, IN_PROGRESS, FAIL, DONE };
enum DamageEffectType { DIRECT_DAMAGE };
enum SpellSchoolMask { SPELL_SCHOOL_MASK_NORMAL };
enum class SelectTargetMethod { MaxThreat, Random };
struct SpellInfo { uint32 Id; };
enum { CURRENT_GENERIC_SPELL, SPELL_STATE_PREPARING, SPELL_STATE_FINISHED };
struct Spell
{
    SpellInfo info;
    uint32 state = SPELL_STATE_PREPARING;
    uint32 remaining = 2000; // synthetic preparation interval; production cast time is not changed or simulated
    SpellInfo const* GetSpellInfo() const { return &info; }
    uint32 getState() const { return state; }
};
struct Unit;
struct Creature;
struct CreatureAI;
struct InstanceScript;
struct WorldObject { virtual Unit* ToUnit() { return nullptr; } virtual ~WorldObject() = default; };
struct Unit : WorldObject
{
    uint32 guid = 0, flags = 0;
    bool alive = true, player = true, visible = true, accessible = true, hostile = true, inMap = true, gm = false;
    float distance = 30.0f, bearing = 0.0f;
    Unit* ToUnit() override { return this; }
    bool IsPlayer() const { return player; }
    bool IsCreature() const { return !player; }
    Unit* ToPlayer() { return this; }
    Creature const* ToCreature() const;
    bool IsGameMaster() const { return gm; }
    bool HasUnitFlag(uint32 flag) const { return flags & flag; }
    bool IsHostileTo(Unit const*) const { return hostile; }
    uint32 GetGUID() const { return guid; }
};
struct ThreatManager;
struct ThreatReference
{
    enum TauntState : uint32 { TAUNT_STATE_DETAUNT = 0, TAUNT_STATE_NONE = 1, TAUNT_STATE_TAUNT = 2 };
    enum OnlineState { ONLINE_STATE_ONLINE = 2, ONLINE_STATE_SUPPRESSED = 1, ONLINE_STATE_OFFLINE = 0 };
    Creature* _owner;
    Unit* _victim;
    float amount;
    OnlineState _online = ONLINE_STATE_OFFLINE;
    TauntState _taunted = TAUNT_STATE_NONE;
    Unit* GetVictim() const { return _victim; }
    float GetThreat() const { return amount; }
    OnlineState GetOnlineState() const { return _online; }
    bool IsAvailable() const { return _online > ONLINE_STATE_OFFLINE; }
    bool IsOffline() const { return !IsAvailable(); }
    bool IsTaunting() const { return _taunted >= TAUNT_STATE_TAUNT; }
    TauntState GetTauntState() const { return IsTaunting() ? TAUNT_STATE_TAUNT : _taunted; }
    bool ShouldBeOffline() const;
    static bool FlagsAllowFighting(Unit const*, Unit const*);
    void UpdateOffline();
};
struct SortedThreat
{
    std::vector<ThreatReference const*> refs;
    bool empty() const { return refs.empty(); }
    auto top() const { return refs.front(); }
    auto ordered_begin() const { return refs.begin(); }
    auto ordered_end() const { return refs.end(); }
};
struct ThreatManager
{
    Creature* _owner;
    SortedThreat sorted;
    SortedThreat* _sortedThreatList = &sorted;
    std::map<uint32, ThreatReference*> _myThreatListEntries;
    std::vector<std::unique_ptr<ThreatReference>> storage;
    ThreatReference const* _currentVictimRef = nullptr;
    ThreatReference const* _fixateRef = nullptr;
    std::vector<Unit*> notify;
    explicit ThreatManager(Creature* owner) : _owner(owner) {}
    static bool CompareReferencesLT(ThreatReference const*, ThreatReference const*, float);
    void Sort()
    {
        std::sort(sorted.refs.begin(), sorted.refs.end(), [](auto a, auto b) { return CompareReferencesLT(b, a, 1.0f); });
    }
    auto const& GetSortedThreatList() { Sort(); return sorted.refs; }
    Unit* GetFixateTarget() const { return _fixateRef ? _fixateRef->GetVictim() : nullptr; }
    bool IsThreatListEmpty(bool includeOffline = false) const
    {
        return std::none_of(sorted.refs.begin(), sorted.refs.end(), [&](auto ref) { return includeOffline || ref->IsAvailable(); });
    }
    ThreatReference const* ReselectVictim();
    Unit* GetCurrentVictim();
    ThreatReference* Add(Unit* target, float amount);
    void ModifyThreatByPercent(Unit* target, int percent)
    {
        _myThreatListEntries.at(target->guid)->amount *= (100 + percent) * 0.01f;
        Sort();
    }
    void Refresh() { GetCurrentVictim(); }
    void Clear()
    {
        sorted.refs.clear(); _myThreatListEntries.clear(); storage.clear();
        _currentVictimRef = _fixateRef = nullptr; notify.clear();
    }
};
struct CompareThreatLessThan
{
    bool operator()(ThreatReference const* a, ThreatReference const* b) const
    {
        return ThreatManager::CompareReferencesLT(a, b, 1.0f);
    }
};
struct GameObject
{
    int uses = 0, despawns = 0;
    void Use(Creature*) { ++uses; }
    void DespawnOrUnsummon(std::chrono::milliseconds = 0ms) { ++despawns; }
};
struct EventQueue
{
    TaskScheduler scheduler;
    void AddEventAtOffset(std::function<void()> callback, std::chrono::milliseconds delay)
    {
        scheduler.Schedule(delay, [callback](TaskContext) { callback(); });
    }
};
struct Creature : Unit
{
    CreatureAI* ai = nullptr;
    InstanceScript* instance = nullptr;
    ThreatManager threat{this};
    Unit* victim = nullptr;
    Unit* facing = nullptr;
    float orientation = 0.0f;
    std::optional<Spell> currentSpell;
    std::vector<uint32> damageHits, stunHits;
    Spell* GetCurrentSpell(uint32) { return currentSpell ? &*currentSpell : nullptr; }
    float GetOrientation() const { return orientation; }
    void SetOrientation(float value) { orientation = value; }
    void SetFacingTo(float value) { orientation = value; }
    void AdvanceCast(uint32 diff);
    void InterruptCast()
    {
        if (currentSpell) currentSpell->state = SPELL_STATE_FINISHED;
        state &= ~UNIT_STATE_CASTING;
    }
    uint32 target = 0, health = 1000, maxHealth = 1000, entry = NPC_OURO, state = 0, react = REACT_AGGRESSIVE;
    uint32 lowered = 0;
    bool combatMovement = true, engaged = false, combat = true, charmed = false, focus = false;
    bool removed = false, wipeClearsThreat = false;
    int despawns = 0, attacks = 0, swings = 0, zoneCalls = 0, aura = 0;
    GameObject base;
    EventQueue m_Events;
    std::list<Creature*> mounds;
    std::vector<WorldObject*> nearby;
    explicit Creature(InstanceScript* script = nullptr) : instance(script) { player = false; }
    CreatureAI* AI() { return ai; }
    bool IsTrigger() const { return false; }
    bool IsWithinMeleeRange(Unit const* targetUnit) const { return targetUnit && targetUnit->distance <= 5.0f; }
    bool IsWithinDistInMap(Unit const* targetUnit, float distanceLimit) const { return targetUnit->distance <= distanceLimit; }
    bool CanSeeOrDetect(Unit const* targetUnit) const { return targetUnit->visible; }
    bool _IsTargetAcceptable(Unit const* targetUnit) const { return targetUnit->alive && targetUnit->hostile; }
    bool CanCreatureAttack(Unit const*) const;
    void SetCombatMovement(bool value) { combatMovement = value; }
    void SetControlled(bool, uint32 flag) { state |= flag; }
    bool HasUnitState(uint32 flag) const { return state & flag; }
    bool HasSpellFocus() const { return focus; }
    void SetInFront(Unit* who) { facing = who; orientation = who->bearing; }
    void SetTarget(uint32 who) { target = who; }
    void SetReactState(uint32 value) { react = value; }
    bool HasReactState(uint32 value) const { return react == value; }
    void SetUnitFlag(uint32 value) { flags |= value; }
    bool IsEngaged() const { return engaged; }
    bool IsAlive() const { return alive; }
    bool IsCharmed() const { return charmed; }
    bool IsInCombat() const { return combat; }
    Unit* GetVictim() { return victim; }
    ThreatManager& GetThreatMgr() { return threat; }
    Unit* SelectVictim();
    void AtEngage(Unit* who);
    void AtDisengage() { engaged = false; }
    void AttackStop() { victim = nullptr; target = 0; }
    void DespawnOrUnsummon(std::chrono::milliseconds = 0ms) { ++despawns; }
    GameObject* FindNearestGameObject(uint32, float) { return &base; }
    void GetCreatureListWithEntryInGrid(std::list<Creature*>& list, uint32, float) { list = mounds; }
    void AddAura(uint32 spell, Creature*) { aura = spell; }
    uint32 GetHealth() const { return health; }
    uint32 GetMaxHealth() const { return maxHealth; }
    void SetHealth(uint32 value) { health = value; }
    bool HealthBelowPctDamaged(uint32 pct, uint32 damage) const { return int(health) - int(damage) < int(maxHealth * pct / 100); }
    void LowerPlayerDamageReq(uint32 value) { lowered = value; }
    uint32 GetEntry() const { return entry; }
    InstanceScript* GetInstanceScript() { return instance; }
    void SetInCombatWithZone() { ++zoneCalls; }
    void AddThreat(Unit* who, float amount) { threat.Add(who, amount); }
};
Creature const* Unit::ToCreature() const { return static_cast<Creature const*>(this); }
struct InstanceScript
{
    EncounterState state = NOT_STARTED;
    Creature* ouro = nullptr;
    void SetBossState(uint32, EncounterState value) { state = value; }
    EncounterState GetBossState(uint32) { return state; }
    Creature* GetCreature(uint32) { return ouro; }
};
struct Cast { uint32 spell, target; bool triggered; float orientation = 0.0f; };
struct CreatureAI
{
    Creature* me;
    bool _isEngaged = false;
    explicit CreatureAI(Creature* creature) : me(creature) { me->ai = this; }
    virtual ~CreatureAI() = default;
    virtual bool CanAIAttack(Unit const*) const { return true; }
    virtual void Reset() {}
    virtual void DamageTaken(Unit*, uint32&, DamageEffectType, SpellSchoolMask) {}
    virtual void SpellHitTarget(Unit*, SpellInfo const*) {}
    virtual void JustEngagedWith(Unit*) {}
    virtual void JustSummoned(Creature*) {}
    virtual void MoveInLineOfSight(Unit*) {}
    virtual void SetData(uint32, uint32) {}
    virtual void UpdateAI(uint32) {}
    enum EvadeReason { EVADE_REASON_NO_HOSTILES };
    virtual void EnterEvadeMode(EvadeReason = EVADE_REASON_NO_HOSTILES) {}
    virtual void AttackStart(Unit* who)
    {
        ++me->attacks;
        me->victim = who;
        me->target = who->guid;
    }
    bool IsEngaged() const { return _isEngaged; }
    void JustStartedThreateningMe(Unit* who) { if (!IsEngaged()) EngagementStart(who); }
    void EngagementStart(Unit* who);
    void EngagementOver() { _isEngaged = false; me->AtDisengage(); }
    bool UpdateVictim();
};
struct ScriptedAI : CreatureAI
{
    TaskScheduler scheduler;
    std::vector<Cast> casts;
    using CreatureAI::CreatureAI;
    void DoCast(Unit* who, uint32 spell, bool triggered = false) { casts.push_back({spell, who->guid, triggered}); }
    void DoCastSelf(uint32 spell, bool triggered = false) { DoCast(me, spell, triggered); }
    void DoCastAOE(uint32 spell)
    {
        // Native DoCastAOE has no explicit target; GUID selection is independent of cone orientation.
        casts.push_back({spell, 0, false, me->GetOrientation()});
        if (spell == SPELL_SAND_BLAST)
        {
            me->currentSpell = Spell{{spell}};
            me->state |= UNIT_STATE_CASTING;
            CHECK(!me->focus); // untargeted negative cone has no native cast focus
        }
    }
    void DoCastVictim(uint32 spell) { if (me->victim) DoCast(me->victim, spell); }
    void DoZoneInCombat() { ++me->zoneCalls; }
    void DoResetThreatList() { for (auto& ref : me->threat.storage) ref->amount = 0; }
    Unit* SelectTarget(SelectTargetMethod, uint32 = 0, float = 0, bool playerOnly = false)
    {
        for (auto ref : me->threat.GetSortedThreatList())
            if (ref->IsAvailable() && (!playerOnly || ref->GetVictim()->IsPlayer()))
                return ref->GetVictim();
        return nullptr;
    }
    void DoSpellAttackToRandomTargetIfReady(uint32 spell)
    {
        if (!me->HasUnitState(UNIT_STATE_CASTING))
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0, true)) DoCast(target, spell);
    }
    void DoMeleeAttackIfReady()
    {
        if (!me->HasUnitState(UNIT_STATE_CASTING) && me->IsWithinMeleeRange(me->victim)) ++me->swings;
    }
};
struct BossAI : ScriptedAI
{
    InstanceScript* instance;
    BossAI(Creature* creature, uint32) : ScriptedAI(creature), instance(creature->instance)
    {
        scheduler.SetValidator([this] { return !me->HasUnitState(UNIT_STATE_CASTING); });
    }
    void JustEngagedWith(Unit*) override { DoZoneInCombat(); instance->SetBossState(DATA_OURO, IN_PROGRESS); }
};
namespace Acore
{
    struct AllWorldObjectsInRange { AllWorldObjectsInRange(Creature*, float) {} };
    template<class Checker> struct WorldObjectListSearcher
    {
        std::list<WorldObject*>& objects;
        WorldObjectListSearcher(Creature*, std::list<WorldObject*>& list, Checker&) : objects(list) {}
    };
    namespace Containers
    {
        template<class T> auto SelectRandomContainerElement(T const& list) { return list.front(); }
    }
}
namespace Cell
{
    template<class Searcher> void VisitObjects(Creature* me, Searcher& searcher, float range)
    {
        for (WorldObject* object : me->nearby)
            if (Unit* unit = object->ToUnit(); unit && unit->distance <= range) searcher.objects.push_back(object);
    }
}
void Creature::AdvanceCast(uint32 diff)
{
    if (!currentSpell || currentSpell->state == SPELL_STATE_FINISHED) return;
    if (diff < currentSpell->remaining)
    {
        currentSpell->remaining -= diff;
        return;
    }
    SpellInfo info = currentSpell->info;
    currentSpell->state = SPELL_STATE_FINISHED; // deliberately retain the finished slot until later reuse
    state &= ~UNIT_STATE_CASTING;
    if (info.Id == SPELL_SAND_BLAST)
    {
        // Independently approximate a frontal cone at EFFECT time, not the cast-start target GUID.
        // Spell.dbc 26102 damage and stun both use caster-relative target 54; no explicit target flags.
        for (auto const& ref : threat.storage)
        {
            Unit* unit = ref->GetVictim();
            float angle = std::remainder(unit->bearing - GetOrientation(), 2 * std::numbers::pi_v<float>);
            if (ref->IsAvailable() && unit->alive && std::abs(angle) <= std::numbers::pi_v<float> / 4)
            {
                damageHits.push_back(unit->guid);
                stunHits.push_back(unit->guid);
                ai->SpellHitTarget(unit, &info);
            }
        }
    }
}
/* NATIVE_METHODS */
bool Creature::CanCreatureAttack(Unit const* who) const
{
    return who->inMap && who->alive && who->hostile && who->accessible && !who->gm && ai->CanAIAttack(who);
}
void Creature::AtEngage(Unit* who) { engaged = true; ai->JustEngagedWith(who); }
void ThreatReference::UpdateOffline()
{
    bool offline = ShouldBeOffline();
    if (offline == IsOffline()) return;
    _online = offline ? ONLINE_STATE_OFFLINE : ONLINE_STATE_ONLINE;
    if (!offline) _owner->threat.notify.push_back(_victim);
    _owner->threat.Sort();
}
Unit* ThreatManager::GetCurrentVictim()
{
    Sort(); // production mutations update heap order; tests also change amounts/states directly
    _currentVictimRef = ReselectVictim();
    auto pending = std::move(notify);
    notify.clear();
    for (Unit* who : pending) _owner->ai->JustStartedThreateningMe(who);
    return _currentVictimRef ? _currentVictimRef->GetVictim() : nullptr;
}
ThreatReference* ThreatManager::Add(Unit* targetUnit, float amount)
{
    storage.push_back(std::make_unique<ThreatReference>(ThreatReference{_owner, targetUnit, amount}));
    auto ref = storage.back().get();
    _myThreatListEntries[targetUnit->guid] = ref;
    sorted.refs.push_back(ref);
    ref->UpdateOffline();
    GetCurrentVictim();
    return ref;
}
Unit* Creature::SelectVictim()
{
    Unit* selected = threat.GetCurrentVictim();
    if (selected && _IsTargetAcceptable(selected) && CanCreatureAttack(selected))
    {
        if (!HasSpellFocus()) SetInFront(selected);
        return selected;
    }
    ai->EnterEvadeMode();
    if (wipeClearsThreat) { AttackStop(); threat.Clear(); }
    return nullptr;
}
/* OURO_CLASSES */
struct TestOuro : boss_ouro
{
    using boss_ouro::boss_ouro;
    using boss_ouro::IsPlayerWithinMeleeRange;
    bool Submerged() const { return _submerged; }
    bool Enraged() const { return _enraged; }
    int NoMeleeTicks() const { return _submergeMelee; }
};
struct Fixture
{
    InstanceScript instance;
    Creature ouro;
    TestOuro ai;
    Unit hunter, tank, other;
    Creature mound;
    npc_dirt_mound moundAI;
    Fixture() : ouro(&instance), ai(&ouro), moundAI(&mound)
    {
        hunter.guid = 1; tank.guid = 2; other.guid = 3;
        tank.distance = other.distance = 3;
        hunter.bearing = std::numbers::pi_v<float> / 2;
        other.bearing = std::numbers::pi_v<float>;
        ouro.guid = 100;
        ouro.nearby = {&tank};
        ouro.mounds = {&mound};
        ai.Reset();
    }
    void Tick(uint32 diff = 1000)
    {
        ouro.threat.Refresh();
        ouro.m_Events.scheduler.Update(diff);
        ouro.AdvanceCast(diff);
        ai.UpdateAI(diff);
    }
    int Count(uint32 spell) const
    {
        return std::count_if(ai.casts.begin(), ai.casts.end(), [&](auto cast) { return cast.spell == spell; });
    }
    void StartMelee()
    {
        ouro.threat.Add(&tank, 100);
        Tick();
        CHECK(instance.state == IN_PROGRESS);
    }
    void StableMelee()
    {
        StartMelee();
        ouro.threat.Add(&hunter, 10000);
        Tick();
    }
};
void TestRanged()
{
    Fixture f;
    f.tank.distance = 30;
    f.ouro.threat.Add(&f.hunter, 10000);
    f.Tick();
    CHECK(f.instance.state == IN_PROGRESS);
    CHECK(f.Count(SPELL_BIRTH) == 1);
    CHECK(f.ouro.zoneCalls == 1);
    CHECK(!f.ouro.threat.IsThreatListEmpty());
    CHECK(f.ouro.threat._myThreatListEntries.at(1)->amount == 10000);
    CHECK(f.ouro.swings == 0 && !f.ouro.combatMovement && f.ouro.HasUnitState(UNIT_STATE_ROOT));
    f.Tick(2000);
    CHECK(f.ai.NoMeleeTicks() == 1);
    f.tank.distance = 3;
    f.ouro.threat.Add(&f.tank, 100);
    f.Tick();
    CHECK(f.ouro.victim == &f.tank && f.ai.NoMeleeTicks() == 0);
    CHECK(f.Count(SPELL_BIRTH) == 1 && f.ouro.despawns == 0);
}
void TestKnockback()
{
    Fixture f;
    f.StartMelee();
    CHECK(f.Count(SPELL_GROUND_RUPTURE) == 1);
    CHECK(f.ai.casts.at(2).spell == SPELL_GROUND_RUPTURE && f.ai.casts.at(2).triggered);
    f.ouro.threat.Add(&f.hunter, 10000);
    f.tank.distance = 30; // effect integration is not simulated: explicitly model the native knockback's range change
    f.Tick();
    CHECK(f.instance.state == IN_PROGRESS && f.ouro.despawns == 0);
    CHECK(!f.ouro.threat.IsThreatListEmpty());
    f.Tick(2000);
    CHECK(f.ai.NoMeleeTicks() > 0);
    f.tank.distance = 3;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank && f.ai.NoMeleeTicks() == 0);
    CHECK(f.Count(SPELL_GROUND_RUPTURE) == 1);
}
void TestMeleeSelection()
{
    Fixture f;
    f.StableMelee();
    CHECK(f.ouro.victim == &f.tank && f.ouro.facing == &f.tank);
    int attacks = f.ouro.attacks;
    auto other = f.ouro.threat.Add(&f.other, 105);
    f.Tick();
    CHECK(f.ouro.victim == &f.tank && f.ouro.attacks == attacks && f.ouro.facing == &f.tank);
    other->amount = 110;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank);
    other->amount = 111;
    f.Tick();
    CHECK(f.ouro.victim == &f.other && f.ouro.attacks == attacks + 1);
    f.ouro.focus = true;
    f.ouro.facing = &f.hunter;
    f.Tick();
    CHECK(f.ouro.facing == &f.hunter); // do not overwrite cast focus
    f.ouro.focus = false;
    f.other.distance = 30;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank && f.ouro.facing == &f.tank);
    f.tank.alive = false;
    f.Tick();
    CHECK(f.ouro.victim == &f.hunter);
    CHECK(!f.ai.IsPlayerWithinMeleeRange());
}
void TestOrdering()
{
    Fixture f;
    f.StableMelee();
    auto tank = f.ouro.threat._myThreatListEntries.at(2);
    auto other = f.ouro.threat.Add(&f.other, 1);
    other->_taunted = ThreatReference::TAUNT_STATE_TAUNT;
    f.Tick();
    CHECK(f.ouro.victim == &f.other);
    tank->_taunted = static_cast<ThreatReference::TauntState>(3);
    f.Tick();
    CHECK(f.ouro.victim == &f.tank); // newer taunt beats the older one, independent of amount
    tank->amount = 0;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank);
    tank->_taunted = ThreatReference::TAUNT_STATE_NONE;
    f.Tick();
    CHECK(f.ouro.victim == &f.other); // newer taunt expires
    other->_taunted = ThreatReference::TAUNT_STATE_DETAUNT;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank);
    tank->_online = ThreatReference::ONLINE_STATE_SUPPRESSED;
    f.Tick();
    CHECK(f.ouro.victim == &f.other); // online precedes suppressed, even when detaunted
    f.ouro.threat._fixateRef = tank;
    f.Tick();
    CHECK(f.ouro.victim == &f.tank); // available melee fixate overrides sorting
    f.tank.distance = 30;
    f.Tick();
    CHECK(f.ouro.victim == &f.other); // ranged fixate must not strand eligible melee
    f.tank.distance = 3;
    f.tank.visible = false;
    f.Tick();
    CHECK(f.ouro.victim == &f.other);
    f.ouro.threat._fixateRef = nullptr;
    f.other.accessible = false;
    f.Tick();
    CHECK(f.ouro.victim == &f.hunter && !f.ai.IsPlayerWithinMeleeRange());
    // Immediate helper revalidation between the periodic threat refreshes.
    f.other.accessible = true;
    f.ouro.threat.Refresh();
    CHECK(f.ai.IsPlayerWithinMeleeRange());
    f.other.inMap = false;
    CHECK(!f.ai.IsPlayerWithinMeleeRange());
    f.other.inMap = true;
    f.other.hostile = false;
    CHECK(!f.ai.IsPlayerWithinMeleeRange());
    f.other.hostile = true;
    f.other.player = false; // preserve native pet/guardian melee eligibility, despite the historical helper name
    CHECK(f.ai.IsPlayerWithinMeleeRange());
}
void AwaitSandBlast(Fixture& f, int count = 1)
{
    for (int i = 0; i < 25 && f.Count(SPELL_SAND_BLAST) < count; ++i) f.Tick();
    CHECK(f.Count(SPELL_SAND_BLAST) == count);
    CHECK(f.ouro.currentSpell && f.ouro.currentSpell->getState() == SPELL_STATE_PREPARING);
}
void TestSandBlastAndSweep()
{
    Fixture f;
    f.StableMelee();
    AwaitSandBlast(f);
    CHECK(f.ai.casts.back().spell == SPELL_SAND_BLAST && f.ai.casts.back().target == 0);
    CHECK(f.ouro.target == f.hunter.guid && f.ouro.victim == &f.tank);
    CHECK(f.ouro.GetOrientation() == f.hunter.bearing);
    CHECK(f.ouro.damageHits.empty() && f.ouro.stunHits.empty());
    auto tank = f.ouro.threat._myThreatListEntries.at(2);
    SpellInfo sand{SPELL_SAND_BLAST}, sweep{SPELL_SWEEP};
    f.ai.SpellHitTarget(&f.tank, &sweep);
    CHECK(tank->amount == 100);
    f.ai.SpellHitTarget(nullptr, &sand);
    int attacks = f.ouro.attacks;
    f.Tick(500); f.Tick(500); f.Tick(500);
    CHECK(f.ouro.GetOrientation() == f.hunter.bearing && f.ouro.attacks == attacks);
    CHECK(f.ouro.damageHits.empty());
    f.Tick(500); // effects resolve from orientation before the next AI tick
    CHECK(f.ouro.damageHits == std::vector<uint32>{f.hunter.guid});
    CHECK(f.ouro.stunHits == f.ouro.damageHits);
    CHECK(f.ouro.threat._myThreatListEntries.at(1)->amount == 0 && tank->amount == 100);
    CHECK(f.ouro.GetOrientation() == f.tank.bearing); // finished spell still occupies its slot: release anyway
    f.Tick();
    CHECK(f.ouro.target == f.tank.guid && f.Count(SPELL_SWEEP) == 1);
    for (auto cast : f.ai.casts)
        if (cast.spell == SPELL_SWEEP) CHECK(cast.target == f.tank.guid && !cast.triggered);
    f.ouro.threat._myThreatListEntries.at(1)->amount = 10000;
    f.hunter.bearing = -std::numbers::pi_v<float> / 2;
    AwaitSandBlast(f, 2);
    CHECK(f.ouro.GetOrientation() == f.hunter.bearing); // repeated cast takes a fresh bearing
    f.Tick(500); f.Tick(500); f.Tick(1000);
    CHECK(f.ouro.damageHits == (std::vector<uint32>{f.hunter.guid, f.hunter.guid}));
    CHECK(f.ouro.GetOrientation() == f.tank.bearing);
}
void TestSandBlastContinuity()
{
    Fixture f;
    f.StableMelee();
    AwaitSandBlast(f);
    float cone = f.hunter.bearing;
    Unit decoy;
    decoy.guid = 4; decoy.bearing = -std::numbers::pi_v<float> / 2;
    f.ouro.threat.Add(&decoy, 20000); // native SelectVictim now faces a third bearing, not the intended cone
    f.ouro.threat.Add(&f.other, 105);
    f.Tick(500);
    CHECK(f.ouro.GetOrientation() == cone && f.ouro.victim == &f.tank);
    f.ouro.threat._myThreatListEntries.at(3)->amount = 111;
    f.Tick(500);
    CHECK(f.ouro.GetOrientation() == cone && f.ouro.victim == &f.other);
    f.other.alive = false; // actual melee victim invalidation must still run while casting
    f.Tick(500);
    CHECK(f.ouro.GetOrientation() == cone && f.ouro.victim == &f.tank);
    f.Tick(500);
    CHECK(f.ouro.damageHits == std::vector<uint32>{f.hunter.guid});
    CHECK(f.ouro.GetOrientation() == f.tank.bearing);

    Fixture invalidTarget;
    invalidTarget.StableMelee();
    AwaitSandBlast(invalidTarget);
    cone = invalidTarget.hunter.bearing;
    invalidTarget.hunter.alive = false;
    invalidTarget.Tick(500);
    CHECK(invalidTarget.ouro.GetOrientation() == cone && invalidTarget.ouro.victim == &invalidTarget.tank);
    invalidTarget.Tick(1500);
    CHECK(invalidTarget.ouro.damageHits.empty() && invalidTarget.ouro.GetOrientation() == invalidTarget.tank.bearing);
}
void TestSandBlastRelease()
{
    Fixture f;
    f.StableMelee();
    AwaitSandBlast(f);
    f.Tick(500);
    f.ouro.InterruptCast();
    f.Tick(0);
    CHECK(f.ouro.GetOrientation() == f.tank.bearing && f.ouro.damageHits.empty());
    f.hunter.bearing = -std::numbers::pi_v<float> / 2;
    AwaitSandBlast(f, 2);
    CHECK(f.ouro.GetOrientation() == f.hunter.bearing);
    f.ouro.currentSpell.reset(); // native slot cleared directly, rather than retained FINISHED
    f.ouro.state &= ~UNIT_STATE_CASTING;
    f.Tick(0);
    CHECK(f.ouro.GetOrientation() == f.tank.bearing);
    AwaitSandBlast(f, 3);
    f.ai.Reset(); // Reset releases the script override even before native casting state is cleared
    f.Tick(0);
    CHECK(f.ouro.GetOrientation() == f.tank.bearing);
    f.ouro.InterruptCast();
    f.Tick(25000);
    CHECK(f.Count(SPELL_SAND_BLAST) == 3); // reset also retains native schedule cancellation

    Fixture wipe;
    wipe.StableMelee();
    AwaitSandBlast(wipe);
    wipe.tank.alive = wipe.hunter.alive = false;
    wipe.ouro.wipeClearsThreat = true;
    wipe.Tick(500);
    CHECK(wipe.instance.state == FAIL && wipe.ouro.despawns == 1 && wipe.ouro.base.despawns == 1);
    CHECK(wipe.ouro.threat.storage.empty() && wipe.ouro.damageHits.empty());
    CHECK(wipe.Count(SPELL_SAND_BLAST) == 1 && wipe.Count(SPELL_SWEEP) == 0);
}
void TestSubmergeAndHealth()
{
    Fixture f;
    f.tank.distance = 30;
    f.ouro.health = 432;
    f.ouro.threat.Add(&f.hunter, 10000);
    for (int i = 0; i < 12; ++i) f.Tick();
    CHECK(!f.ai.Submerged() && f.ai.NoMeleeTicks() == 10);
    f.Tick();
    CHECK(f.ai.Submerged() && f.ouro.despawns == 1 && f.instance.state == IN_PROGRESS);
    CHECK(f.ouro.victim == nullptr && f.ouro.react == REACT_PASSIVE);
    CHECK(f.ouro.HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE));
    CHECK(f.ouro.base.uses == 1 && f.ouro.base.despawns == 1);
    CHECK(f.mound.aura == SPELL_SUMMON_OURO_AURA);
    Creature replacement;
    f.moundAI.JustSummoned(&replacement);
    CHECK(replacement.health == 432 && replacement.lowered == 568 && replacement.zoneCalls == 1);
    f.ai.Submerge();
    CHECK(f.ouro.despawns == 1);
    Fixture timed;
    timed.StartMelee();
    for (int i = 1; i < 89; ++i) timed.Tick();
    CHECK(!timed.ai.Submerged());
    timed.Tick();
    CHECK(timed.ai.Submerged()); // native 90-second phase transition remains
}
void TestEnrageAndValidator()
{
    Fixture f;
    f.ouro.threat.Add(&f.hunter, 10000);
    f.ouro.health = 202;
    uint32 damage = 2;
    f.ai.DamageTaken(&f.hunter, damage, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);
    CHECK(!f.ai.Enraged()); // exactly 20% is not below 20%
    f.ouro.health = 201;
    f.ai.DamageTaken(&f.hunter, damage, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);
    CHECK(damage == 2 && f.ai.Enraged() && f.Count(SPELL_BERSERK) == 1);
    f.Tick();
    CHECK(f.Count(SPELL_BOULDER) == 1);
    f.ouro.threat.Add(&f.tank, 100);
    f.ouro.victim = &f.hunter; // helper must see eligible melee even before victim selection
    CHECK(f.ai.IsPlayerWithinMeleeRange());
    f.Tick();
    CHECK(f.Count(SPELL_BOULDER) == 1);
    f.tank.distance = 30;
    for (int i = 0; i < 100; ++i) f.Tick();
    CHECK(!f.ai.Submerged() && f.ouro.despawns == 0 && f.Count(SPELL_SUMMON_OURO_MOUNDS) == 5);
    f.ai.DamageTaken(&f.hunter, damage, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL);
    CHECK(f.Count(SPELL_BERSERK) == 1);
    Fixture casting;
    casting.StartMelee();
    casting.ouro.state |= UNIT_STATE_CASTING;
    casting.Tick(22000);
    CHECK(casting.Count(SPELL_SAND_BLAST) == 0 && casting.Count(SPELL_SWEEP) == 0);
    casting.ouro.state &= ~UNIT_STATE_CASTING;
    casting.Tick(0);
    CHECK(casting.Count(SPELL_SAND_BLAST) == 1 && casting.Count(SPELL_SWEEP) == 0);
    casting.Tick(2000);
    CHECK(casting.Count(SPELL_SWEEP) == 1);
}
void TestWipeAndGuards()
{
    Fixture f;
    f.StableMelee();
    f.tank.alive = f.hunter.alive = false;
    f.ouro.wipeClearsThreat = true;
    auto casts = f.ai.casts.size();
    f.Tick(20000);
    CHECK(f.instance.state == FAIL && f.ouro.despawns == 1 && f.ouro.base.despawns == 1);
    CHECK(f.ai.casts.size() == casts + 1 && f.ai.casts.back().spell == SPELL_OURO_SUBMERGE_VISUAL);
    CHECK(f.ouro.threat.storage.empty()); // no references may survive native no-target/evade handling
    Fixture idle;
    idle.Tick(100000);
    CHECK(idle.ai.casts.empty() && idle.instance.state == NOT_STARTED);
    Fixture charmed;
    charmed.StableMelee();
    charmed.ouro.charmed = true;
    charmed.ouro.victim = &charmed.hunter;
    charmed.Tick();
    CHECK(charmed.ouro.victim == &charmed.hunter);
    charmed.ouro.alive = false;
    charmed.Tick();
    CHECK(!charmed.ouro.engaged);
    Fixture passive;
    passive.StableMelee();
    passive.ouro.react = REACT_PASSIVE;
    passive.Tick();
    CHECK(passive.ouro.victim == nullptr && passive.ouro.despawns == 0);
}
int main(int argc, char** argv)
{
    if (argc == 2)
    {
        if (std::string(argv[1]) == "ranged") TestRanged();
        else if (std::string(argv[1]) == "knockback") TestKnockback();
        else if (std::string(argv[1]) == "sand-blast") TestSandBlastAndSweep();
        else if (std::string(argv[1]) == "sand-blast-continuity") TestSandBlastContinuity();
        else return 2;
        return 0;
    }
    TestRanged(); TestKnockback(); TestMeleeSelection(); TestOrdering(); TestSandBlastAndSweep();
    TestSandBlastContinuity(); TestSandBlastRelease();
    TestSubmergeAndHealth(); TestEnrageAndValidator(); TestWipeAndGuards();
    std::cout << "Ouro production combat regressions passed\n";
}
