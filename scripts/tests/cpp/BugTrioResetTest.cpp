// Production method bodies are injected by test_bug_trio_reset.py.
// Map/AI doubles model despawn and native respawn scheduling, not the full game engine.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
using namespace std::chrono_literals;
using uint32 = std::uint32_t;
/* ENUMS */
enum EncounterState { NOT_STARTED, IN_PROGRESS, FAIL, DONE };
enum EvadeReason { EVADE_REASON_OTHER };
constexpr uint32 MOVE_RUN = 0;
constexpr uint32 UNIT_DYNFLAG_LOOTABLE = 1;
struct Unit {};
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value;
    bool operator!=(ObjectGuid const& other) const { return value != other.value; }
};
namespace GameTime
{
    std::chrono::seconds GetGameTime() { return std::chrono::seconds(1000); }
}
struct CreatureData { uint32 id; };
struct ObjectMgr
{
    std::unordered_map<uint32, CreatureData> data;
    CreatureData const* GetCreatureData(uint32 id) const
    {
        auto it = data.find(id);
        return it == data.end() ? nullptr : &it->second;
    }
} objectMgr;
auto sObjectMgr = &objectMgr;
struct Creature;
struct Map
{
    bool compatibility = false;
    std::unordered_map<uint32, time_t> respawns;
    std::vector<uint32> scheduled;
    auto const& GetCreatureRespawnTimes() const { return respawns; }
    void SaveCreatureRespawnTime(uint32 id, time_t& time)
    {
        respawns[id] = time;
        scheduled.push_back(id);
    }
};
struct InstanceBase
{
    virtual ~InstanceBase() = default;
    virtual void Initialize() = 0;
    virtual uint32 GetData(uint32) const = 0;
    virtual void SetData(uint32, uint32) = 0;
};
struct Instance : InstanceBase
{
    uint32 BugTrioDeathCount = 0;
    uint32 BugTrioConsumeTarget = 0;
    EncounterState state = NOT_STARTED;
    EncounterState skeram = DONE;
    std::map<uint32, Creature*> creatures;
    Creature* GetCreature(uint32 type);
    EncounterState GetBossState(uint32) const { return state; }
    /* INSTANCE_METHODS */
};
struct Motion
{
    void MoveWaypoint(uint32, bool) {}
};
struct CreatureAI
{
    virtual ~CreatureAI() = default;
    virtual void Reset() = 0;
    virtual void EnterEvadeMode(EvadeReason) = 0;
    virtual void JustDied(Unit*) = 0;
};
struct Creature : Unit
{
    Map* map;
    uint32 entry;
    uint32 spawn;
    bool alive = true;
    bool inWorld = true;
    bool evading = false;
    uint32 health = 1000;
    uint32 respawnCalls = 0;
    bool lootable = true;
    bool despawnScheduled = false;
    CreatureAI* ai = nullptr;
    Motion motion;
    bool IsAlive() const { return alive; }
    bool IsInEvadeMode() const { return evading; }
    uint32 GetEntry() const { return entry; }
    ObjectGuid GetGUID() const { return {spawn}; }
    Map* GetMap() const { return map; }
    CreatureAI* AI() { return ai; }
    Motion* GetMotionMaster() { return &motion; }
    void SetSpeed(uint32, float) {}
    void RemoveDynamicFlag(uint32) { lootable = false; }
    void DespawnOrUnsummon(std::chrono::seconds delay)
    {
        assert(delay == 3s);
        despawnScheduled = true;
    }
    void Respawn(bool force = false)
    {
        assert(!alive); // Already-evading living peers must be skipped, not respawned.
        assert(force);
        ++respawnCalls;
        if (map->compatibility)
        {
            map->respawns.erase(spawn);
            alive = inWorld = true;
            evading = false;
            health = 1000;
            ai->Reset();
        }
        else
        {
            time_t now = GameTime::GetGameTime().count();
            map->SaveCreatureRespawnTime(spawn, now);
            inWorld = false;
        }
    }
};
Creature* Instance::GetCreature(uint32 type)
{
    auto it = creatures.find(type);
    return it != creatures.end() && it->second->inWorld ? it->second : nullptr;
}
struct Scheduler { void CancelAll() {} };
struct BossAI : CreatureAI
{
    Creature* me;
    Instance* instance;
    BossAI(Creature* c, Instance* i) : me(c), instance(i) {}
    void Reset() override { instance->state = NOT_STARTED; }
    void EnterEvadeMode(EvadeReason) override
    {
        if (!me->alive || me->evading) return;
        me->evading = true;
        me->health = 1000;
        Reset();
    }
    void JustDied(Unit*) override { instance->state = DONE; }
};
struct Trio : BossAI
{
    Scheduler _scheduler;
    bool _dying = false;
    bool _isEating = false;
    uint32 finalSpells = 0;
    Trio(Creature* c, Instance* i) : BossAI(c, i) { c->ai = this; Reset(); }
    void Talk(uint32) {}
    void DoFinalSpell() { ++finalSpells; }
    /* TRIO_METHODS */
    /* LEGACY_EVADE */
};
struct Fixture
{
    Map map;
    Instance instance;
    std::map<uint32, std::unique_ptr<Creature>> creatures;
    std::map<uint32, std::unique_ptr<Trio>> ais;
    Fixture(bool compatibility = false)
    {
        map.compatibility = compatibility;
        instance.Initialize();
        for (auto const& [type, entry] : std::map<uint32, uint32>{{DATA_KRI, NPC_KRI}, {DATA_YAUJ, NPC_YAUJ}, {DATA_VEM, NPC_VEM}})
        {
            auto c = std::make_unique<Creature>();
            c->map = &map; c->entry = entry; c->spawn = type + 80000;
            objectMgr.data[c->spawn] = {entry};
            instance.creatures[type] = c.get();
            ais[type] = std::make_unique<Trio>(c.get(), &instance);
            creatures[type] = std::move(c);
        }
        instance.state = IN_PROGRESS;
        map.respawns[90000] = 999999; // Unrelated Skeram respawn must not be touched.
        objectMgr.data[90000] = {NPC_SKERAM};
        map.respawns[90001] = 999999; // Missing ObjectMgr record must be ignored.
    }
    void Kill(uint32 type)
    {
        auto& c = *creatures.at(type);
        c.alive = false; c.health = 0;
        map.respawns[c.spawn] = 1000 + 604800;
        ais.at(type)->JustDied(nullptr);
    }
    void RemoveConsumed()
    {
        for (auto& [type, c] : creatures)
        {
            (void)type;
            if (!c->alive && c->despawnScheduled && !map.compatibility) c->inWorld = false;
        }
    }
    void ProcessDue()
    {
        // Model native Map::ProcessCreatureRespawn: new creature for due original
        // spawn, skipping already-live spawns. No hand-made encounter duplicates.
        for (auto& [type, c] : creatures)
        {
            auto it = map.respawns.find(c->spawn);
            if (it == map.respawns.end() || it->second > 1000) continue;
            map.respawns.erase(it);
            if (c->alive) continue;
            c->alive = c->inWorld = true; c->health = 1000; c->evading = false;
            c->despawnScheduled = false; c->lootable = true;
            ais.at(type)->Reset();
        }
    }
};
int main()
{
    // The exact previous production lookup-only method cannot recover removed bugs.
    {
        Fixture old;
        old.Kill(DATA_KRI); old.Kill(DATA_YAUJ); old.RemoveConsumed();
        old.ais.at(DATA_VEM)->BossAI::EnterEvadeMode(EVADE_REASON_OTHER);
        old.ais.at(DATA_VEM)->LegacyEvadeAllBosses(EVADE_REASON_OTHER);
        old.ProcessDue();
        assert(old.instance.GetData(DATA_BUG_TRIO_DEATH) == 0);
        assert(!old.creatures.at(DATA_KRI)->alive && !old.creatures.at(DATA_YAUJ)->alive);
    }
    // All survivors and kill orders; retained corpses/dynamic removal/compat mode.
    for (uint32 survivor : {DATA_KRI, DATA_YAUJ, DATA_VEM})
        for (unsigned deaths : {0u, 1u, 2u})
            for (bool compatibility : {false, true})
                for (bool removed : {false, true})
                for (bool reversed : {false, true})
                {
                    Fixture f(compatibility);
                    unsigned n = 0;
                    std::vector<uint32> order{DATA_KRI, DATA_YAUJ, DATA_VEM};
                    if (reversed) std::reverse(order.begin(), order.end());
                    for (uint32 type : order)
                        if (type != survivor && n++ < deaths) f.Kill(type);
                    assert(f.instance.GetData(DATA_BUG_TRIO_DEATH) == deaths);
                    if (removed) f.RemoveConsumed();
                    f.ais.at(survivor)->_dying = true;
                    f.ais.at(survivor)->_isEating = true;
                    f.ais.at(survivor)->EnterEvadeMode(EVADE_REASON_OTHER);
                    f.ProcessDue();
                    assert(f.instance.state == NOT_STARTED && f.instance.skeram == DONE);
                    assert(f.instance.GetData(DATA_BUG_TRIO_DEATH) == 0);
                    for (auto const& [type, c] : f.creatures)
                    {
                        assert(c->alive && c->inWorld && c->health == 1000);
                        assert(!f.ais.at(type)->_dying && !f.ais.at(type)->_isEating);
                        assert(!f.map.respawns.count(c->spawn));
                    }
                    assert(f.map.respawns.at(90000) == 999999 && f.map.respawns.at(90001) == 999999);
                    auto scheduled = f.map.scheduled;
                    f.ais.at(survivor)->EnterEvadeMode(EVADE_REASON_OTHER);
                    assert(scheduled == f.map.scheduled); // Already evading: no repeated reset loop.
                    // Once home, the next attempt counts all three deaths normally.
                    f.instance.state = IN_PROGRESS;
                    for (uint32 type : order)
                    {
                        f.creatures.at(type)->evading = false;
                        if (type != survivor) f.Kill(type);
                    }
                    f.Kill(survivor);
                    assert(f.instance.state == DONE && f.instance.GetData(DATA_BUG_TRIO_DEATH) == 3);
                    assert(f.creatures.at(survivor)->lootable && f.instance.skeram == DONE);
                }
    // A stale timer for a living bug must not create another creature.
    {
        Fixture stale;
        auto* kri = stale.creatures.at(DATA_KRI).get();
        stale.map.respawns[kri->spawn] = 999999;
        stale.ais.at(DATA_VEM)->EnterEvadeMode(EVADE_REASON_OTHER);
        stale.ProcessDue();
        assert(stale.instance.GetCreature(DATA_KRI) == kri && kri->respawnCalls == 0);
        assert(stale.map.respawns.count(kri->spawn) == 0);
    }
    // Raid copies use independent consumption targets, counters and respawn queues.
    {
        Fixture a, b;
        b.instance.SetData(DATA_BUG_TRIO_CONSUME_TARGET, DATA_KRI);
        b.Kill(DATA_KRI); b.RemoveConsumed();
        a.Kill(DATA_KRI); a.Kill(DATA_YAUJ); a.RemoveConsumed();
        a.ais.at(DATA_VEM)->EnterEvadeMode(EVADE_REASON_OTHER);
        a.ProcessDue();
        assert(b.instance.GetData(DATA_BUG_TRIO_CONSUME_TARGET) == DATA_KRI);
        assert(b.instance.GetData(DATA_BUG_TRIO_DEATH) == 1 && b.instance.state == IN_PROGRESS);
        assert(b.map.respawns.at(b.creatures.at(DATA_KRI)->spawn) > 1000);
    }
    // Last-bug death still completes the encounter and leaves the final boss lootable.
    {
        Fixture done;
        done.Kill(DATA_KRI); done.Kill(DATA_YAUJ); done.Kill(DATA_VEM);
        assert(done.instance.state == DONE && done.instance.GetData(DATA_BUG_TRIO_DEATH) == 3);
        assert(!done.creatures.at(DATA_KRI)->lootable && !done.creatures.at(DATA_YAUJ)->lootable);
        assert(done.creatures.at(DATA_VEM)->lootable && !done.creatures.at(DATA_VEM)->despawnScheduled);
        auto timers = done.map.respawns;
        done.ais.at(DATA_VEM)->EnterEvadeMode(EVADE_REASON_OTHER);
        assert(done.instance.state == DONE && timers == done.map.respawns);
    }
    // A stray evade callback must not reset a completed encounter, even if a live
    // creature object remains (e.g. during an external administrative operation).
    {
        Fixture completed;
        completed.instance.state = DONE;
        completed.instance.SetData(DATA_BUG_TRIO_CONSUME_TARGET, DATA_KRI);
        completed.ais.at(DATA_VEM)->EnterEvadeMode(EVADE_REASON_OTHER);
        assert(completed.instance.state == DONE);
        assert(completed.instance.GetData(DATA_BUG_TRIO_CONSUME_TARGET) == DATA_KRI);
        assert(!completed.creatures.at(DATA_VEM)->evading && completed.map.scheduled.empty());
    }
    std::cout << "Production Bug Trio reset/state regression tests passed\n";
}
