// Offline API doubles for actual instance methods; not a live encounter simulation.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
using uint32 = uint32_t;
using namespace std::chrono_literals;
constexpr uint32 MAP_AHN_QIRAJ_TEMPLE = 531;
/* ENUMS */
/* MOUND_ENTRY */
enum EncounterState { NOT_STARTED, IN_PROGRESS, FAIL, DONE, SPECIAL, TO_BE_DECIDED };
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value = 0;
    auto operator<=>(ObjectGuid const&) const = default;
};
using GuidVector = std::vector<ObjectGuid>;
using Milliseconds = std::chrono::milliseconds;
namespace GameTime
{
    inline time_t now = 1000;
    std::chrono::seconds GetGameTime() { return std::chrono::seconds(now); }
}
template<class... Args> void Log(Args const&...) {}
#define LOG_INFO(...) Log(__VA_ARGS__)
struct CreatureData { uint32 id = NPC_OURO_SPAWNER, id2 = 0, id3 = 0, mapid = 531; };
struct ObjectMgr
{
    std::map<uint32, CreatureData> data;
    CreatureData const* GetCreatureData(uint32 id)
    {
        auto it = data.find(id);
        return it == data.end() ? nullptr : &it->second;
    }
} objectMgr;
ObjectMgr* sObjectMgr = &objectMgr;
struct TaskContext;
struct TaskScheduler
{
    using Handler = std::function<void(TaskContext)>;
    struct Task { uint32 due; Handler handler; };
    uint32 now = 0;
    std::vector<Task> tasks;
    void Schedule(Milliseconds delay, Handler handler)
    {
        tasks.push_back({now + uint32(delay.count()), std::move(handler)});
    }
    void Update(uint32 diff);
};
struct TaskContext
{
    TaskScheduler* scheduler;
    TaskScheduler::Task task;
    void Repeat(Milliseconds delay)
    {
        task.due += uint32(delay.count());
        scheduler->tasks.push_back(task);
    }
};
void TaskScheduler::Update(uint32 diff)
{
    now += diff;
    GameTime::now += diff / 1000;
    // Dispatch without retaining vector iterators across callbacks.
    for (;;)
    {
        auto it = std::find_if(tasks.begin(), tasks.end(), [&](Task const& t) { return t.due <= now; });
        if (it == tasks.end())
            return;
        Task task = *it;
        tasks.erase(it);
        task.handler(TaskContext{this, task});
    }
}
struct Map;
struct TestInstance;
struct Creature
{
    uint32 entry = 0, spawn = 0;
    ObjectGuid guid;
    Map* map = nullptr;
    bool alive = true, compatibility = false, hasSummoned = false;
    uint32 respawns = 0;
    uint32 GetEntry() const { return entry; }
    ObjectGuid GetGUID() const { return guid; }
    bool IsAlive() const { return alive; }
    void Respawn(bool force = false);
};
struct Player {};
struct GameObject { void DespawnOrUnsummon(Milliseconds) {} };
struct Map
{
    uint32 id;
    TestInstance* script = nullptr;
    bool gridLoaded = true;
    std::unordered_map<uint32, time_t> timers;
    std::map<ObjectGuid, Creature*> creatures;
    std::vector<std::unique_ptr<Creature>> storage;
    std::vector<uint32> writes;
    explicit Map(uint32 id) : id(id) {}
    uint32 GetInstanceId() const { return id; }
    auto const& GetCreatureRespawnTimes() const { return timers; }
    Creature* GetCreature(ObjectGuid guid)
    {
        auto it = creatures.find(guid);
        return it == creatures.end() ? nullptr : it->second;
    }
    GameObject* GetGameObject(ObjectGuid) { return nullptr; }
    void SaveCreatureRespawnTime(uint32 spawn, time_t& time)
    {
        // Deliberately invalidate timer iterators: production must collect IDs before saving.
        timers.erase(spawn);
        timers.rehash(257);
        timers.emplace(spawn, time);
        writes.push_back(spawn);
    }
    Creature* Add(uint32 entry, uint32 spawn = 0);
    void Remove(Creature* creature);
    void Process();
};
struct InstanceScript
{
    Map* instance;
    TaskScheduler scheduler;
    std::array<EncounterState, 10> states{};
    std::map<uint32, ObjectGuid> objects;
    uint32 entered = 0, removed = 0, saves = 0;
    uint32 completed = 127, binds = 10, inventory = 42, deadline = 1789397072;
    explicit InstanceScript(Map* map) : instance(map)
    {
        for (uint32 i = 1; i <= 7; ++i)
            states[i] = DONE;
    }
    EncounterState GetBossState(uint32 id) const { return states[id]; }
    virtual bool SetBossState(uint32 id, EncounterState state)
    {
        if (states[id] == TO_BE_DECIDED) { states[id] = state; return false; }
        if (states[id] == state) return false;
        states[id] = state;
        ++saves;
        return true;
    }
    Creature* GetCreature(uint32 type)
    {
        auto it = objects.find(type);
        return it == objects.end() ? nullptr : instance->GetCreature(it->second);
    }
    virtual void OnPlayerEnter(Player*) { ++entered; }
    virtual void OnCreatureCreate(Creature* creature)
    {
        if (creature->entry == NPC_OURO) objects[DATA_OURO] = creature->guid;
        if (creature->entry == NPC_OURO_SPAWNER) objects[DATA_OURO_SPAWNER] = creature->guid;
        if (creature->entry == NPC_CTHUN) objects[DATA_CTHUN] = creature->guid;
    }
    virtual void OnCreatureRemove(Creature* creature)
    {
        ++removed;
        for (auto it = objects.begin(); it != objects.end();)
            if (it->second == creature->guid) it = objects.erase(it); else ++it;
    }
    virtual ~InstanceScript() = default;
};
struct TestInstance : InstanceScript
{
    using InstanceScript::InstanceScript;
    bool _ouroRecoveryPending = false;
    std::set<ObjectGuid> _ouroMounds;
    GuidVector CThunGraspGUIDs;
    /* METHODS */
    /* LEGACY */
};
Creature* Map::Add(uint32 entry, uint32 spawn)
{
    storage.push_back(std::make_unique<Creature>());
    Creature* creature = storage.back().get();
    creature->entry = entry;
    creature->spawn = spawn;
    creature->guid.value = uint32(storage.size());
    creature->map = this;
    creatures[creature->guid] = creature;
    script->OnCreatureCreate(creature);
    return creature;
}
void Map::Remove(Creature* creature)
{
    if (creature->spawn)
        timers[creature->spawn] = GameTime::now + 365 * 86400;
    creatures.erase(creature->guid);
    script->OnCreatureRemove(creature);
}
void Creature::Respawn(bool force)
{
    assert(!force); // recovery must not bypass conditions or kill a living spawner
    ++respawns;
    if (compatibility)
    {
        alive = true;
        hasSummoned = false;
        map->timers.erase(spawn);
    }
    else
    {
        map->Remove(this);
        time_t now = GameTime::now;
        map->SaveCreatureRespawnTime(spawn, now);
    }
}
void Map::Process()
{
    std::vector<uint32> due;
    for (auto const& [spawn, time] : timers)
        if (gridLoaded && time <= GameTime::now && objectMgr.GetCreatureData(spawn))
            due.push_back(spawn);
    for (uint32 spawn : due)
    {
        bool alive = false;
        for (auto const& [guid, creature] : creatures)
            alive |= creature->spawn == spawn && creature->alive;
        timers.erase(spawn);
        if (!alive)
            Add(objectMgr.data[spawn].id, spawn);
    }
}
struct Fixture
{
    Map map;
    TestInstance script;
    Player player;
    explicit Fixture(uint32 id = 5670) : map(id), script(&map)
    {
        map.script = &script;
        objectMgr.data.clear();
        objectMgr.data[88073] = {};
        objectMgr.data[88076] = {NPC_VEKNILASH, 0, 0, 531};
        objectMgr.data[12345] = {NPC_OURO_SPAWNER, 0, 0, 1};
        map.timers[88073] = GameTime::now + 365 * 86400;
        map.timers[88076] = GameTime::now + 500000;
    }
    void Tick() { script.scheduler.Update(2000); }
    void Preserved()
    {
        assert(script.completed == 127 && script.binds == 10 && script.inventory == 42);
        assert(script.deadline == 1789397072);
        for (uint32 i = 1; i <= 7; ++i) assert(script.states[i] == DONE);
        for (uint32 id : map.writes) assert(id == 88073);
    }
};
int main()
{
    {
        Fixture f;
        Creature* spawner = f.map.Add(NPC_OURO_SPAWNER, 88073);
        f.script.states[DATA_OURO] = IN_PROGRESS;
        f.map.Remove(spawner);
        assert(!f.script.GetCreature(DATA_OURO_SPAWNER));
        assert(f.script.LegacySetBossState(DATA_OURO, FAIL));
        assert(f.map.writes.empty()); // reproduce missing-GUID failure in the prior production handler
        f.script.OnPlayerEnter(&f.player);
        f.script.OnPlayerEnter(&f.player);
        assert(f.script.scheduler.tasks.size() == 1 && f.script.entered == 2);
        int unrelated = 0;
        f.script.scheduler.Schedule(1s, [&](TaskContext) { ++unrelated; });
        f.Tick();
        assert(unrelated == 1 && f.map.writes.size() == 1);
        f.map.Process();
        Creature* replacement = f.script.GetCreature(DATA_OURO_SPAWNER);
        assert(replacement && replacement != spawner && replacement->spawn == 88073);
        f.script.OnPlayerEnter(&f.player); f.Tick(); f.map.Process();
        assert(f.map.writes.size() == 1 && replacement->respawns == 0);
        assert(f.script.GetCreature(DATA_OURO_SPAWNER) == replacement);
        f.Preserved();
    }
    {
        Fixture f;
        f.script.states[DATA_OURO] = IN_PROGRESS;
        Creature* ouro = f.map.Add(NPC_OURO);
        Creature* a = f.map.Add(NPC_OURO_DIRT_MOUND);
        Creature* b = f.map.Add(NPC_OURO_DIRT_MOUND);
        f.map.Remove(ouro); // ordinary submerge must not restore the original spawning mound
        f.script.OnPlayerEnter(&f.player); f.Tick();
        assert(f.map.writes.empty() && !f.script._ouroRecoveryPending);
        f.script.SetBossState(DATA_OURO, FAIL);
        f.map.Remove(a); f.Tick();
        assert(f.map.writes.empty() && f.script._ouroRecoveryPending);
        f.map.Add(NPC_OURO); // surviving mound successfully re-emerges
        f.map.Remove(b);
        f.script.SetBossState(DATA_OURO, NOT_STARTED);
        f.script.SetBossState(DATA_OURO, IN_PROGRESS); f.Tick();
        assert(f.map.writes.empty() && !f.script._ouroRecoveryPending);
        f.Preserved();
    }
    {
        Fixture f;
        f.script.states[DATA_OURO] = IN_PROGRESS;
        Creature* ouro = f.map.Add(NPC_OURO);
        Creature* mound = f.map.Add(NPC_OURO_DIRT_MOUND);
        f.script.SetBossState(DATA_OURO, FAIL); f.Tick();
        assert(f.map.writes.empty());
        f.map.Remove(ouro); f.Tick();
        assert(f.map.writes.empty());
        f.map.Remove(mound); f.Tick();
        assert(f.map.writes.size() == 1);
        f.Preserved();
    }
    for (bool compatibility : {false, true})
    {
        Fixture f;
        Creature* spawner = f.map.Add(NPC_OURO_SPAWNER, 88073);
        spawner->alive = false; spawner->compatibility = compatibility; spawner->hasSummoned = true;
        f.script.SetBossState(DATA_OURO, FAIL); f.Tick(); f.map.Process();
        Creature* restored = f.script.GetCreature(DATA_OURO_SPAWNER);
        assert(restored && restored->alive && !restored->hasSummoned);
        assert(spawner->respawns == 1);
        f.Preserved();
    }
    {
        Fixture f;
        f.script.states[DATA_OURO] = TO_BE_DECIDED;
        assert(!f.script.SetBossState(DATA_OURO, NOT_STARTED)); // native load does not invoke the transition handler
        f.map.gridLoaded = false;
        f.script.OnPlayerEnter(&f.player); f.Tick(); f.map.Process();
        assert(!f.script.GetCreature(DATA_OURO_SPAWNER));
        assert(f.map.timers[88073] <= GameTime::now);
        f.map.gridLoaded = true; f.map.Process();
        assert(f.script.GetCreature(DATA_OURO_SPAWNER));
        f.Preserved();
    }
    {
        Fixture f;
        f.script.SetBossState(DATA_OURO, FAIL);
        f.script.SetBossState(DATA_OURO, DONE);
        assert(!f.script.SetBossState(DATA_OURO, FAIL));
        assert(!f.script.SetBossState(DATA_OURO, NOT_STARTED));
        f.script.OnPlayerEnter(&f.player); f.Tick();
        assert(f.script.GetBossState(DATA_OURO) == DONE && f.map.writes.empty());
        assert(f.script.SetBossState(DATA_CTHUN, IN_PROGRESS)); // unrelated encounter still uses native behavior
        Creature* cthun = f.map.Add(NPC_CTHUN);
        f.map.Remove(cthun);
        assert(!f.script.GetCreature(DATA_CTHUN));
        assert(f.script.GetBossState(DATA_CTHUN) == IN_PROGRESS && f.map.writes.empty());
        f.Preserved();
    }
    for (int invalid = 0; invalid < 5; ++invalid)
    {
        Fixture f;
        if (invalid == 0) objectMgr.data.erase(88073);
        if (invalid == 1) objectMgr.data[88073].id2 = NPC_OURO;
        if (invalid == 2) objectMgr.data[88073].mapid = 1;
        if (invalid == 3) f.map.timers.erase(88073);
        if (invalid == 4) { objectMgr.data[88074] = {}; f.map.timers[88074] = GameTime::now + 999999; }
        f.map.timers[12345] = GameTime::now + 99999;
        f.script.OnPlayerEnter(&f.player); f.Tick();
        assert(f.map.writes.empty());
        f.Preserved();
    }
    {
        Fixture first(5670), second(5671);
        time_t other = second.map.timers[88073];
        first.script.SetBossState(DATA_OURO, FAIL); first.Tick();
        assert(second.map.timers[88073] == other && second.map.writes.empty());
        first.Preserved(); second.Preserved();
    }
    std::cout << "Ouro production recovery regressions passed\n";
}
