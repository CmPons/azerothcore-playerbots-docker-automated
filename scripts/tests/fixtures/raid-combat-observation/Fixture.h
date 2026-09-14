// Explicit API/data doubles. Real linked containers, type visitors, coordinate functions and
// extracted core visibility/casting/encounter/Map::Visit bodies are NOT replaced by booleans.
#include "GenericObservation.h"
#include "GridDefines.h"
#include "Cell.h"
#include "MapRefMgr.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include "GridObject.inc"

std::string GetDebugInfo() { return {}; }
namespace Acore
{
[[noreturn]] void Assert(std::string_view, uint32, std::string_view, std::string_view, std::string_view, std::string_view)
{ std::abort(); }
}
class Map;
class BattlegroundMap;
class Unit;
class TempSummon;
struct Position { float x = 0, y = 0, z = 0; };
enum { SERVERSIDE_VISIBILITY_GHOST, SERVERSIDE_VISIBILITY_GM, GHOST_VISIBILITY_GHOST = 2,
    STATUS_IN_PROGRESS = 3, CURRENT_GENERIC_SPELL = 0, CURRENT_CHANNELED_SPELL = 1,
    CURRENT_AUTOREPEAT_SPELL = 2, SPELL_STATE_FINISHED = 0, SPELL_STATE_DELAYED = 1,
    SPELL_ATTR2_DO_NOT_RESET_COMBAT_TIMERS = 0, IN_PROGRESS = 1 };
struct Flags
{
    uint32 value[2]{1, 0};
    uint32 GetValue(unsigned i) const { return value[i]; }
    uint32 GetFlags() const { return value[0]; }
};
struct AIProbe { bool visible = true; bool CanBeSeen(Player const*) const { return visible; } };
struct Battleground { int status = 0; int GetStatus() const { return status; } };

class Map
{
public:
    MapRefMgr m_mapRefMgr; // Real MapRefMgr + extracted real MapReference lifecycle.
    std::unordered_map<ObjectGuid, WorldObject*> objects; // Explicit lookup double; force rehash in tests.
    std::map<uint32, std::unique_ptr<MapGridType>> grids; // Real native MapGrid/GridCell/TypeMapContainer.
    bool arena = false;
    virtual ~Map();
    bool IsBattleArena() const { return arena; }
    BattlegroundMap* ToBattlegroundMap() { return nullptr; }
    MapRefMgr const& GetPlayers() const { return m_mapRefMgr; }
    bool IsGridLoaded(GridCoord const& c) const { return grids.contains(c.GetId()); }
    MapGridType* GetMapGrid(uint16 x, uint16 y) { return grids.at(GridCoord(x,y).GetId()).get(); }
    template<class T, class C> void Visit(Cell const&, TypeContainerVisitor<T,C>&);
    template<class T> void Add(T& object);
};
class BattlegroundMap : public Map
{
public:
    Battleground* GetBG() { return nullptr; }
};
#include "MapReference.inc"
inline Map::~Map() { m_mapRefMgr.clearReferences(); }

class WorldObject : public Position
{
public:
    virtual ~WorldObject() = default;
    ObjectGuid guid;
    Map* map = nullptr;
    bool inWorld = true, always = false, despawn = false, detectable = true;
    uint32 phase = 1, entry = 0;
    Flags m_serverSideVisibility, m_serverSideVisibilityDetect, m_invisibility, m_stealth;
    virtual Unit const* ToUnit() const { return nullptr; }
    virtual Player const* ToPlayer() const { return nullptr; }
    virtual Creature const* ToCreature() const { return nullptr; }
    virtual GameObject const* ToGameObject() const { return nullptr; }
    bool IsPlayer() const { return ToPlayer(); }
    bool IsCreature() const { return ToCreature(); }
    ObjectGuid GetGUID() const { return guid; }
    uint32 GetEntry() const { return entry; }
    uint8 GetTypeId() const { return IsPlayer() ? TYPEID_PLAYER : (ToUnit() ? TYPEID_UNIT : TYPEID_GAMEOBJECT); }
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; }
    bool IsInWorld() const { return inWorld; }
    Map* GetMap() const { return map; }
    Map* FindMap() const { return map; }
    bool InSamePhase(WorldObject const* other) const { return phase & other->phase; }
    bool IsNeverVisible() const { return !inWorld; }
    bool IsAlwaysVisibleFor(WorldObject const*) const { return always; }
    bool CanAlwaysSee(WorldObject const*) const { return false; }
    bool IsInvisibleDueToDespawn() const { return despawn; }
    bool CanDetect(WorldObject const* other, bool ignoreStealth, bool, bool) const
    { return other->detectable || ignoreStealth; } // Detection sub-algorithms explicitly doubled.
    float GetSightRange(WorldObject const*) const { return 100; }
    bool IsWithinSightRange(Position const& p, float range) const
    { return std::hypot(x-p.x, y-p.y) <= range; }
    bool IsWithinDist(WorldObject const* other, float range, bool) const
    { return IsWithinSightRange(*other, range); }
    bool CanSeeOrDetect(WorldObject const*, bool = false, bool = false, bool = false) const;
    bool CanNeverSee(WorldObject const*) const;
};
struct SpellInfo { bool HasAttribute(int) const { return false; } };
struct Spell
{
    int state = 2;
    unsigned castTime = 1;
    SpellInfo info;
    SpellInfo* m_spellInfo = &info;
    int getState() const { return state; }
    unsigned GetCastTime() const { return castTime; }
};
class Unit : public WorldObject
{
public:
    std::set<Unit*> m_Controlled;
    Unit* vehicle = nullptr;
    ObjectGuid target, owner;
    bool combat = false, removing = false, pet = false;
    Spell* m_currentSpells[3]{};
    Unit const* ToUnit() const override { return this; }
    ObjectGuid GetTarget() const { return target; }
    bool IsInCombat() const { return combat; } // Native inline is HasUnitFlag(UNIT_FLAG_IN_COMBAT).
    bool IsDuringRemoveFromWorld() const { return removing; }
    bool IsNonMeleeSpellCast(bool, bool = false, bool = false, bool = false, bool = true) const;
    Unit* GetVehicleBase() const { return vehicle; }
    bool IsPet() const { return pet; }
    ObjectGuid GetOwnerGUID() const { return owner; }
};
class Corpse : public WorldObject, public GridObject<Corpse> {};
class DynamicObject : public WorldObject, public GridObject<DynamicObject> {};
class Creature : public Unit, public GridObject<Creature>
{
public:
    bool IsAIEnabled = true;
    AIProbe ai;
    TempSummon const* summon = nullptr;
    Creature const* ToCreature() const override { return this; }
    AIProbe const* AI() const { return &ai; }
    TempSummon const* ToTempSummon() const { return summon; }
};
class TempSummon : public Creature
{
public:
    bool onlySummoner = false;
    bool IsVisibleBySummonerOnly() const { return onlySummoner; }
    ObjectGuid GetSummonerGUID() const { return owner; }
};
class GameObject : public WorldObject, public GridObject<GameObject>
{
public:
    AIProbe ai;
    GameObject const* ToGameObject() const override { return this; }
    AIProbe const* AI() const { return &ai; }
};
class Player : public Unit, public GridObject<Player>
{
public:
    MapReference mapRef;
    Unit* m_mover = this; // SafeUnitPointer conversion/lifecycle is NOT simulated here.
    bool teleport = false, conditions = true, spectator = false, dead = false, groupVisible = true;
    uint32 health = 100, team = 0;
    Corpse* corpse = nullptr;
    std::set<ObjectGuid> client;
    Player const* ToPlayer() const override { return this; }
    bool IsBeingTeleported() const { return teleport; }
    bool CanSeeObjectByVisibilityConditions(WorldObject const*) const { return conditions; }
    bool IsSpectator() const { return spectator; }
    Position const& GetSightPosition() const { return *this; }
    bool isDead() const { return dead; }
    uint32 GetHealth() const { return health; }
    Corpse* GetCorpse() const { return corpse; }
    bool HaveAtClient(WorldObject const* other) const { return HaveAtClient(other->GetGUID()); }
    bool HaveAtClient(ObjectGuid id) const { return client.contains(id); }
    bool GetFarSightDistance() const { return false; }
    bool isInFront(WorldObject const*) const { return true; }
    uint32 GetTeamId() const { return team; }
    bool IsGroupVisibleFor(Player const*) const { return groupVisible; }
};
struct BossInfo { unsigned state = 0; };
class InstanceScript
{
public:
    std::vector<BossInfo> bosses;
    bool IsEncounterInProgress() const;
};
class InstanceMap : public Map
{
public:
    InstanceScript* script = nullptr;
    InstanceScript* GetInstanceScript() { return script; }
};
namespace ObjectAccessor
{
inline Player* GetPlayer(Map const* map, ObjectGuid const& id)
{
    auto it = map->objects.find(id);
    return it == map->objects.end() ? nullptr : dynamic_cast<Player*>(it->second);
}
inline WorldObject* GetWorldObject(WorldObject const& context, ObjectGuid const& id)
{
    auto it = context.GetMap()->objects.find(id);
    return it == context.GetMap()->objects.end() ? nullptr : it->second;
}
inline Unit* GetUnit(WorldObject const& context, ObjectGuid const& id)
{ return dynamic_cast<Unit*>(GetWorldObject(context, id)); }
}
template<class T> void Map::Add(T& object)
{
    object.map = this;
    objects[object.GetGUID()] = &object;
    auto coord = Acore::ComputeCellCoord(object.x, object.y);
    Cell cell(coord);
    auto& grid = grids[GridCoord(cell.GridX(), cell.GridY()).GetId()];
    if (!grid) grid = std::make_unique<MapGridType>(cell.GridX(), cell.GridY());
    grid->AddGridObject(cell.CellX(), cell.CellY(), &object);
    if constexpr (std::is_same_v<T,Player>) object.mapRef.link(this, &object);
}
