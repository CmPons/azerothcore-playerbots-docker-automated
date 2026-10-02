// Complete production Creature::Respawn body with storage/AI/world doubles.
// Verifies hook timing and branch coverage, not a complete map simulation.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using time_t = std::int64_t;
constexpr int DAY = 86400, MINUTE = 60, SPELL_AURA_TRANSFORM = 56;
constexpr int CREATURE_FLAG_EXTRA_HARD_RESET = 1, CONDITION_SOURCE_TYPE_CREATURE_RESPAWN = 1;
enum class DeathState { Alive, JustDied, Corpse, Dead, JustRespawned };
enum class HighGuid { Unit };
struct ObjectGuid
{
    uint64 id;
    template<HighGuid> static ObjectGuid Create(uint32 entry, uint32 spawn) { return {uint64(entry) << 32 | spawn}; }
    bool operator==(ObjectGuid const&) const = default;
};
struct CreatureData { uint32 id = 21964, id2 = 0, id3 = 0, movementType = 0, equipmentId = 0; } data;
struct CreatureTemplate { bool hardReset = false; bool HasFlagsExtra(int) const { return hardReset; } } info;
struct CreatureModel
{
    uint32 CreatureDisplayID;
    float DisplayScale;
    CreatureModel(uint32 id, float scale, float) : CreatureDisplayID(id), DisplayScale(scale) {}
};
struct CreatureModelInfo {};
using MovementGeneratorType = uint32;
using ConditionList = std::vector<int>;
struct Creature;
struct ConditionMgr
{
    bool allow = true;
    ConditionList GetConditionsForNotGroupedEntry(int, uint32) { return {}; }
    bool IsObjectMeetToConditions(Creature*, ConditionList const&) { return allow; }
} conditions;
struct Map
{
    time_t linked = 0;
    int removed = 0, scheduled = 0;
    time_t GetLinkedRespawnTime(ObjectGuid) { return linked; }
    void RemoveCreatureRespawnTime(uint32) { ++removed; }
    void SaveCreatureRespawnTime(uint32, time_t) { ++scheduled; }
};
struct ObjectMgr
{
    ObjectGuid target{0};
    CreatureTemplate const* GetCreatureTemplate(uint32) { return &info; }
    CreatureData const* GetCreatureData(uint32) { return &data; }
    ObjectGuid GetLinkedRespawnGuid(ObjectGuid) { return target; }
    CreatureModelInfo const* GetCreatureModelRandomGender(CreatureModel*, CreatureTemplate const*) { return nullptr; }
} objects;
struct PoolMgr
{
    template<class T> uint32 IsPartOfAPool(uint32) { return 0; }
    template<class T> void UpdatePool(uint32, uint32) {}
} pools;
struct AI
{
    bool allow = true;
    bool reset = false;
    bool CanRespawn() { return allow; }
    void Reset() { reset = true; }
};
struct Motion { void InitDefault() {} };
struct Position { void Relocate(float, float, float, float) {} };
struct ScriptMgr { int calls = 0; void OnCreatureRespawn(Creature*); } scripts;
#define sConditionMgr (&::conditions)
#define sObjectMgr (&objects)
#define sPoolMgr (&pools)
#define sScriptMgr (&scripts)
#define LOG_DEBUG(...) ((void)0)
namespace GameTime { struct Time { time_t count() const { return 1000; } }; inline Time GetGameTime() { return {}; } }
static uint32 GetRandomId(uint32 a, uint32, uint32) { return a; }
static uint32 urand(uint32 a, uint32) { return a; }

struct Creature
{
    bool _respawnCompatibilityMode = true, IsAIEnabled = true, TriggerJustRespawned = false;
    bool react = false, visible = false, removal = false;
    uint32 m_spawnId = 7, m_originalEntry = 21964, m_defaultMovementType = 0;
    CreatureData const* m_creatureData = &data;
    time_t m_respawnTime = 0, m_respawnDelay = 60, m_respawnedTime = 0;
    Position m_last_notify_position;
    std::vector<int> loot;
    DeathState state = DeathState::Dead;
    uint32 health = 0, maximum = 241248;
    Map map;
    struct AI ai;
    Motion motion;
    bool IsAlive() const { return state == DeathState::Alive; }
    DeathState getDeathState() const { return state; }
    void setDeathState(DeathState s)
    {
        state = s == DeathState::JustRespawned ? DeathState::Alive :
            s == DeathState::JustDied ? DeathState::Corpse : s;
    }
    uint32 GetEntry() const { return 21964; }
    Map* GetMap() { return &map; }
    struct AI* AI() { return &ai; }
    void RemoveCorpse(bool, bool) { if (state == DeathState::Corpse) state = DeathState::Dead; }
    void UpdateEntry(uint32, CreatureData const* = nullptr, bool = true) {}
    void LoadEquipment(uint32) {}
    void AIM_Initialize() {}
    void ResetPickPocketLootTime() {}
    void SelectLevel() { maximum = health = 603120; }
    std::vector<int> GetAuraEffectsByType(int) { return {}; }
    uint32 GetNativeDisplayId() const { return 1; }
    float GetNativeObjectScale() const { return 1; }
    CreatureTemplate const* GetCreatureTemplate() const { return &info; }
    void SetDisplayId(uint32, float) {}
    void SetNativeDisplayId(uint32, float = 1) {}
    Motion* GetMotionMaster() { return &motion; }
    void InitializeReactState() { react = true; }
    void UpdateObjectVisibility(bool) { visible = true; }
    void AddObjectToRemoveList() { removal = true; }
    void SetRespawnTime(uint32 t) { m_respawnTime = 1000 + t; }
    void SaveRespawnTime() {}
    void Respawn(bool force);
};
void ScriptMgr::OnCreatureRespawn(Creature* c)
{
    ++calls;
    assert(c->IsAlive() && c->health == 603120 && c->maximum == 603120);
    assert(!c->IsAIEnabled || c->ai.reset);
    assert(c->react && !c->visible);
    // A consuming module can now safely normalize the newly rebuilt native pool.
    c->health = c->maximum = 241248;
}
/* PRODUCTION */

int main()
{
    for (int scenario = 0; scenario < 10; ++scenario)
    {
        Creature c;
        scripts.calls = 0;
        conditions.allow = true;
        info.hardReset = false;
        objects.target = {0};
        data.id2 = 0;
        bool force = false;
        bool expected = scenario == 0 || scenario == 5 || scenario == 7 || scenario == 8 || scenario == 9;
        switch (scenario)
        {
            case 1: c.state = DeathState::Alive; break;
            case 2: conditions.allow = false; break;
            case 3: c.ai.allow = false; break;
            case 4: c.map.linked = 3000; break;
            case 5: conditions.allow = c.ai.allow = false; force = true; c.state = DeathState::Alive; break;
            case 6: c._respawnCompatibilityMode = false; break;
            case 7: c.IsAIEnabled = false; break;
            case 8: data.id2 = 1; break;
            case 9: c.map.linked = 3000; info.hardReset = true; break;
        }
        c.Respawn(force);
        assert(scripts.calls == int(expected));
        if (expected) assert(c.IsAlive() && c.health == 241248 && c.visible);
        if (scenario == 6) assert(c.removal && c.map.scheduled == 1 && !c.ai.reset);
    }
    std::cout << "Passed native respawn hook: 10 branches\n";
}
