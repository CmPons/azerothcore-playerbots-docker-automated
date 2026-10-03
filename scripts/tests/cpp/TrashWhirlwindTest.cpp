// Offline doubles around the complete production hazard helper and extracted avoidance methods.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

using uint32 = uint32_t;
using ObjectGuid = uint32;
using GuidVector = std::vector<ObjectGuid>;
enum BotState { BOT_STATE_COMBAT };
enum MovementPriority { MOVEMENT_COMBAT, MOVEMENT_FORCED };
constexpr int EFFECT_0 = 0;
constexpr int SPELL_EFFECT_WEAPON_DAMAGE = 58;
uint32 now = 10000;
uint32 getMSTime() { return now; }

struct Position
{
    float x = 0, y = 0, z = 0;
    Position() = default;
    Position(float x, float y, float z) : x(x), y(y), z(z) { }
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; }
    float GetExactDist(Position const* p) const
    {
        return std::sqrt((x - p->x) * (x - p->x) + (y - p->y) * (y - p->y) + (z - p->z) * (z - p->z));
    }
    bool operator==(Position const&) const = default;
};
struct WorldObject : Position
{
    float reach = 1.5f;
    float GetCombatReach() const { return reach; }
    Position const& GetPosition() const { return *this; }
};
struct Aura
{
    bool removed = false, expired = false;
    bool IsRemoved() const { return removed; }
    bool IsExpired() const { return expired; }
};
struct Unit : WorldObject
{
    bool creature = true, world = true, alive = true, combat = true, hostile = true, controlled = false;
    uint32 entry = 20031, mapId = 550;
    void* map = reinterpret_cast<void*>(1);
    Unit* victim = nullptr;
    std::map<uint32, Aura> auras;
    bool IsCreature() const { return creature; }
    bool IsInWorld() const { return world; }
    bool IsAlive() const { return alive; }
    bool IsInCombat() const { return combat; }
    bool IsControlledByPlayer() const { return controlled; }
    uint32 GetMapId() const { return mapId; }
    void* GetMap() const { return map; }
    uint32 GetEntry() const { return entry; }
    Unit* GetVictim() const { return victim; }
    Aura const* GetAura(uint32 id) const
    {
        auto it = auras.find(id);
        return it == auras.end() ? nullptr : &it->second;
    }
};
struct Player : Unit
{
    bool IsHostileTo(Unit const* u) const { return u->hostile; }
};
struct SpellEffect
{
    int Effect = SPELL_EFFECT_WEAPON_DAMAGE;
    float radius = 8.0f;
    float CalcRadius(Unit*) const { return radius; }
};
struct SpellInfo { SpellEffect Effects[1]; };
struct SpellMgr
{
    std::map<uint32, SpellInfo> spells{{15578, {}}, {15589, {}}};
    SpellInfo const* GetSpellInfo(uint32 id) const
    {
        auto it = spells.find(id);
        return it == spells.end() ? nullptr : &it->second;
    }
} spellMgr;
auto* sSpellMgr = &spellMgr;
struct Config
{
    std::unordered_set<uint32> aoeAvoidSpellWhitelist;
    float maxAoeAvoidRadius = 15.0f;
} sPlayerbotAIConfig;
template<class T> struct Value
{
    T value{};
    T& Get() { return value; }
};
struct Context
{
    Value<GuidVector> candidates, traps, triggers;
    Value<Aura*> debuff;
    Value<Unit*> target;
    template<class T> Value<T>* GetValue(std::string const& name)
    {
        if constexpr (std::is_same_v<T, GuidVector>)
        {
            if (name == "possible targets no los")
                return &candidates;
            return name == "possible triggers" ? &triggers : &traps;
        }
        else if constexpr (std::is_same_v<T, Aura*>)
            return &debuff;
        else
            return &target;
    }
};
struct PlayerbotAI
{
    Player player;
    Context context;
    std::map<ObjectGuid, Unit*> units;
    bool avoid = true, stay = false, tank = false, melee = true, claim = false;
    int interruptions = 0;
    struct { bool scheduled = true; } raidCombat;
    Player* GetBot() { return &player; }
    Context* GetAiObjectContext() { return &context; }
    Unit* GetUnit(ObjectGuid guid) { return units.contains(guid) ? units[guid] : nullptr; }
    bool HasStrategy(std::string const& name, BotState) const { return name == "stay" ? stay : avoid; }
    BotState GetState() const { return BOT_STATE_COMBAT; }
    bool IsTank(Player*) const { return tank; }
    bool IsMelee(Player*) const { return melee; }
    void InterruptSpell() { ++interruptions; }
};
namespace RaidCombat
{
    bool HasMovementClaim(PlayerbotAI const& ai) { return ai.claim; }
}

// PRODUCTION_HEADER
// PRODUCTION_SOURCE

struct Event { };
struct AvoidAoeAction
{
    PlayerbotAI* botAI;
    Player* bot;
    uint32 lastMoveTimer = 0, moveInterval = 1000;
    bool canMove = true, moveSuccess = true, legacyAura = false;
    int moves = 0, legacyCalls = 0;
    Position destination{16, 0, 0};
    explicit AvoidAoeAction(PlayerbotAI* ai) : botAI(ai), bot(ai->GetBot()) { }
    bool IsMovingAllowed() const { return canMove; }
    Position BestPositionForMeleeToFlee(Position, float) const { return destination; }
    Position BestPositionForRangedToFlee(Position, float) const { return destination; }
    bool MoveTo(uint32, float x, float y, float z, bool, bool, bool normalOnly, bool,
                MovementPriority priority, bool lessDelay)
    {
        assert(normalOnly && priority == MOVEMENT_FORCED && lessDelay);
        ++moves;
        return moveSuccess && TrashWhirlwind::AllowsMove(botAI, Position(x, y, z));
    }
    bool AvoidAuraWithDynamicObj() { ++legacyCalls; return legacyAura; }
    bool AvoidGameObjectWithDamage() { ++legacyCalls; return false; }
    bool AvoidUnitWithDamageAura() { ++legacyCalls; return false; }
    bool isUseful();
    bool Execute(Event);
    bool AvoidTrashWhirlwind();
};
#define AI_VALUE(type, name) botAI->GetAiObjectContext()->GetValue<type>(name)->Get()
// PRODUCTION_METHODS

int main()
{
    PlayerbotAI ai;
    Unit spin;
    spin.auras[33500] = {};
    ai.units[1] = &spin;
    ai.context.candidates.value = {999, 1}; // Stale candidates must not hide the next valid creature.
    ai.context.target.value = &spin;
    ai.player.x = 4;
    auto hazards = TrashWhirlwind::FindHazards(&ai);
    assert(hazards.size() == 1 && hazards[0].radius == 10.5f);
    AvoidAoeAction action(&ai);
    assert(action.isUseful());
    assert(action.Execute({}));
    assert(action.moves == 1 && ai.interruptions == 1 && action.lastMoveTimer == now);
    assert(action.legacyCalls == 0);
    assert(!action.isUseful()); // Successful escapes are throttled even with chat logging off.
    now += 1001;
    assert(action.isUseful());
    action.canMove = false;
    assert(!action.AvoidTrashWhirlwind());
    action.canMove = true;
    ai.claim = true;
    assert(!action.AvoidTrashWhirlwind());
    ai.claim = false;
    action.moveSuccess = false;
    assert(!action.AvoidTrashWhirlwind() && ai.interruptions == 1);
    action.moveSuccess = true;
    action.destination = Position(-16, 0, 0); // Must not flee through the center of the spin.
    assert(!action.AvoidTrashWhirlwind());
    action.destination = Position(16, 0, 0);
    ai.melee = false;
    assert(action.AvoidTrashWhirlwind()); // Healers/ranged use the same detection.

    ai.player.x = 16;
    assert(!TrashWhirlwind::AllowsMove(&ai, Position(4, 0, 0)));
    assert(!TrashWhirlwind::AllowsMove(&ai, Position(-16, 0, 0)));
    assert(TrashWhirlwind::AllowsMove(&ai, Position(20, 0, 0)));
    assert(!TrashWhirlwind::AllowsApproach(&ai, &spin, 0)); // Melee approach/gap closer.
    ai.player.x = 40;
    assert(TrashWhirlwind::AllowsApproach(&ai, &spin, 25)); // Safe spell/heal range remains available.
    assert(!TrashWhirlwind::AllowsApproach(&ai, &spin, 0));
    ai.raidCombat.scheduled = false;
    assert(TrashWhirlwind::AllowsMove(&ai, Position(0, 0, 0)));
    assert(TrashWhirlwind::AllowsApproach(&ai, &spin, 0));
    ai.raidCombat.scheduled = true;
    spin.auras[33500].expired = true;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    assert(TrashWhirlwind::AllowsApproach(&ai, &spin, 0));
    spin.auras[33500].expired = false;
    spin.auras[33500].removed = true;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    spin.auras[33500].removed = false;

    // Role exemption applies only to the tank who currently owns this particular enemy.
    ai.tank = true;
    spin.victim = &ai.player;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    assert(TrashWhirlwind::AllowsApproach(&ai, &spin, 0));
    spin.victim = nullptr;
    assert(TrashWhirlwind::FindHazards(&ai).size() == 1);
    ai.tank = false;
    spin.victim = &ai.player;
    assert(TrashWhirlwind::FindHazards(&ai).size() == 1); // A DPS with aggro still escapes.

    for (bool* flag : {&ai.player.world, &ai.player.alive, &ai.player.combat,
                       &spin.creature, &spin.world, &spin.alive, &spin.combat, &spin.hostile, &ai.avoid})
    {
        *flag = false;
        assert(TrashWhirlwind::FindHazards(&ai).empty());
        *flag = true;
    }
    for (bool* flag : {&spin.controlled, &ai.stay})
    {
        *flag = true;
        assert(TrashWhirlwind::FindHazards(&ai).empty());
        *flag = false;
    }
    ai.player.mapId = 548;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    ai.player.mapId = 550;
    spin.map = nullptr;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    spin.map = ai.player.map;
    spin.entry = 21215; // Bosses and arbitrary other whirlwinds are not added to this policy.
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    spin.entry = 20035;
    assert(TrashWhirlwind::FindHazards(&ai).empty()); // Wrong aura for this creature.
    spin.auras[36132] = {};
    assert(TrashWhirlwind::FindHazards(&ai).size() == 1);
    for (uint32 id : {36132u, 15589u})
    {
        sPlayerbotAIConfig.aoeAvoidSpellWhitelist.insert(id);
        assert(TrashWhirlwind::FindHazards(&ai).empty());
        sPlayerbotAIConfig.aoeAvoidSpellWhitelist.clear();
    }
    sPlayerbotAIConfig.maxAoeAvoidRadius = 7;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    sPlayerbotAIConfig.maxAoeAvoidRadius = 15;
    auto& effect = spellMgr.spells[15589].Effects[0];
    for (float radius : {0.0f, -1.0f, 16.0f, NAN, INFINITY})
    {
        effect.radius = radius;
        assert(TrashWhirlwind::FindHazards(&ai).empty());
    }
    effect.radius = 8;
    effect.Effect = 2;
    assert(TrashWhirlwind::FindHazards(&ai).empty());
    spellMgr.spells.erase(15589);
    assert(TrashWhirlwind::FindHazards(&ai).empty());

    using TrashWhirlwind::IsEscapeOrClear;
    TrashWhirlwind::Hazard hazard{Position(0, 0, 0), 10};
    assert(IsEscapeOrClear(Position(4, 0, 0), Position(6, 0, 0), hazard));
    assert(!IsEscapeOrClear(Position(4, 0, 0), Position(2, 0, 0), hazard));
    assert(!IsEscapeOrClear(Position(4, 0, 0), Position(4, 0, 0), hazard));
    assert(IsEscapeOrClear(Position(0, 0, 0), Position(12, 0, 0), hazard));
    assert(!IsEscapeOrClear(Position(12, 0, 0), Position(-12, 0, 0), hazard));
    assert(IsEscapeOrClear(Position(12, 0, 0), Position(12, 12, 0), hazard));
    assert(IsEscapeOrClear(Position(12, 0, 30), Position(-12, 0, 30), hazard));
    assert(!IsEscapeOrClear(Position(12, 0, 0), Position(NAN, 0, 0), hazard));
    for (int degrees = 0; degrees < 360; ++degrees)
    {
        float const angle = degrees * 3.14159265f / 180.0f;
        Position const inside(4 * std::cos(angle), 4 * std::sin(angle), 0);
        Position const outside(16 * std::cos(angle), 16 * std::sin(angle), 0);
        assert(IsEscapeOrClear(inside, outside, hazard));
        assert(!IsEscapeOrClear(outside, inside, hazard));
    }

    ai.context.traps.value = {1};
    action.legacyAura = true;
    now += 1001;
    assert(action.isUseful());
    assert(action.Execute({}));
    assert(action.legacyCalls > 0); // Existing generic hazards remain reachable without a trash spin.
    std::cout << "Trash whirlwind detection, escape, admission and 360-angle checks passed\n";
}
