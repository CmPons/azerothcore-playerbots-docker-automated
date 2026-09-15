#pragma once
// Explicit ground API doubles. Actual native splines, handoff/expiry and exact stop bodies are compiled separately.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <G3D/Vector3.h>
#include <G3D/Matrix3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include "MoveSpline.h"
#include "ObjectGuid.h"
#include "CthunPolicyScope.h"
#include "RaidCombatState.h"
#include <atomic>
#include <stdexcept>

constexpr uint32 MOVEMENTFLAG_FORWARD=1, MOVEMENTFLAG_BACKWARD=2, MOVEMENTFLAG_WALKING=4,
    MOVEMENTFLAG_SPLINE_ENABLED=8, MOVEMENTFLAG_ONTRANSPORT=16, MOVEMENTFLAG_ROOT=32,
    MOVEMENTFLAG_CAN_FLY=64, MOVEMENTFLAG_DISABLE_GRAVITY=128, MOVEMENTFLAG_FLYING=256,
    MOVEMENTFLAG_SWIMMING=512, MOVEMENTFLAG_FALLING=1024, MOVEMENTFLAG_FALLING_FAR=2048, MOVEMENTFLAG_MASK_MOVING=3;
using MovementFlags = uint32;
using AnimTier = uint8;
enum UnitMoveType { MOVE_RUN, MOVE_SWIM, MOVE_FLIGHT };
constexpr uint32 UNIT_STATE_LOST_CONTROL=1, UNIT_FLAG_DISABLE_MOVE=1;
constexpr int IDLE_MOTION_TYPE=0, NULL_MOTION_TYPE=-1, FOLLOW_MOTION_TYPE=1, FLEEING_MOTION_TYPE=2,
    MOTION_SLOT_IDLE=0, MOTION_SLOT_ACTIVE=1, MOTION_SLOT_CONTROLLED=2, EFFECT_MOTION_TYPE=3, CHASE_MOTION_TYPE=4;
constexpr unsigned UNIT_STATE_MOVING=2;
using MovementGeneratorType=int;
using MovementSlot=int;
constexpr unsigned MMCF_UPDATE=1, MMCF_INUSE=4;
class Player;
class PlayerbotAI;
constexpr int LINEOFSIGHT_ALL_CHECKS=7;
constexpr float INVALID_HEIGHT=-100000.0f;
constexpr uint32 LIQUID_MAP_NO_WATER=0, LIQUID_MAP_IN_WATER=1, LIQUID_MAP_UNDER_WATER=2;
namespace VMAP { enum class ModelIgnoreFlags : uint32 { Nothing }; }
struct Position
{
    float x=0,y=0,z=0,o=0;
    static float NormalizeOrientation(float f) { return std::fmod(f+float(2*M_PI),float(2*M_PI)); }
    float GetPositionX() const { return x; } float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; } float GetOrientation() const { return o; }
};
struct LiquidData { uint32 Status=LIQUID_MAP_NO_WATER; float DepthLevel=0, Level=10; };
struct MeshTriangle { uint32 idx0,idx1,idx2; };
namespace VMAP
{
using ::MeshTriangle;
bool IntersectTriangle(MeshTriangle const&, std::vector<G3D::Vector3>::const_iterator,
    G3D::Ray const&, float&);
}
struct Mesh
{
    std::vector<G3D::Vector3> vertices;
    std::vector<MeshTriangle> triangles;
    void Quad(G3D::Vector3 a, G3D::Vector3 b, G3D::Vector3 c, G3D::Vector3 d)
    {
        uint32 n=vertices.size(); vertices.insert(vertices.end(), {a,b,c,d});
        triangles.push_back({n,n+1,n+2}); triangles.push_back({n,n+2,n+3});
    }
    bool IntersectRay(G3D::Ray const& ray, float& distance, bool stop, VMAP::ModelIgnoreFlags) const
    {
        bool hit=false;
        for (auto const& tri : triangles)
            if (VMAP::IntersectTriangle(tri, vertices.begin(), ray, distance))
            { hit=true; if (stop) return true; }
        return hit;
    }
};
struct ModelOwner { bool spawned=true; bool IsSpawned() const { return spawned; } };
struct GameObjectModel
{
    uint32 phasemask=1;
    ModelOwner storage; ModelOwner* owner=&storage;
    G3D::AABox iBound{G3D::Vector3(-10000,-10000,-10000),G3D::Vector3(10000,10000,10000)};
    G3D::Vector3 iPos{0,0,0}; G3D::Matrix3 iInvRot=G3D::Matrix3::identity();
    float iInvScale=1, iScale=1;
    Mesh mesh; Mesh* iModel=&mesh;
    bool isEnabled() const { return phasemask; }
    bool intersectRay(G3D::Ray const&,float&,bool,uint32,VMAP::ModelIgnoreFlags) const;
};
struct Map
{
    struct Data
    {
        CthunPolicy::Scope* scope=nullptr;
        template<class T> T* Get(char const*) { return static_cast<T*>(scope); }
    } CustomData;
    uint32 id=1, instance=1;
    bool loaded=true, water=false, blocked=false;
    float floor=0;
    Mesh walls;
    std::vector<G3D::Vector3> queries;
    bool IsGridLoaded(float,float) const { return loaded; }
    float GetHeight(uint32,float,float,float,bool) const { return floor; }
    LiquidData GetLiquidData(uint32,float,float,float z,float,int)
    { return {water && z<10 ? LIQUID_MAP_IN_WATER:LIQUID_MAP_NO_WATER, floor, 10}; }
    bool isInLineOfSight(float x,float y,float z,float a,float b,float c,uint32,int,VMAP::ModelIgnoreFlags)
    {
        (void)x; (void)y; (void)z;
        queries.emplace_back(a,b,c);
        return !blocked;
    }
};
class MovementGenerator
{
public:
    MovementGenerator();
    virtual ~MovementGenerator() = default;
    uint64 GetIdentity() const { return _identity; }
    virtual void Initialize(Unit*) = 0;
    virtual void Finalize(Unit*) = 0;
    virtual void Reset(Unit*) = 0;
    virtual bool Update(Unit*,uint32) = 0;
    virtual int GetMovementGeneratorType() = 0;
    virtual uint32 GetSplineId() const { return 0; }
private:
    uint64 _identity=0;
};
class BasicMotion : public MovementGenerator
{
public:
    explicit BasicMotion(int type) : type(type) { }
    int type;
    std::function<void()> finalizer;
    void Initialize(Unit*) override { }
    void Finalize(Unit*) override { if (finalizer) finalizer(); }
    void Reset(Unit*) override { }
    bool Update(Unit*,uint32) override { return true; }
    int GetMovementGeneratorType() override { return type; }
};
inline BasicMotion idle(IDLE_MOTION_TYPE);
inline bool isStatic(MovementGenerator* current) { return current==&idle; }
struct MotionMaster
{
    using _Ty=MovementGenerator*;
    using ExpireList=std::vector<_Ty>;
    explicit MotionMaster(Unit* owner=nullptr) : _owner(owner) { }
    ~MotionMaster() { for(int i=1;i<3;++i) if (Impl[i]) DirectDelete(Impl[i]); delete _expList; }
    _Ty Impl[3]{&idle,nullptr,nullptr};
    bool _needInit[3]{};
    int _top=0;
    Unit* _owner;
    unsigned _cleanFlag=0;
    ExpireList* _expList=nullptr;
    bool empty() const { return _top<0; }
    int size() const { return _top+1; }
    _Ty top() const { return Impl[_top]; }
    bool needInitTop() const { return _needInit[_top]; }
    void Initialize() { Impl[0]=&idle; _top=0; }
    void InitTop();
    void DirectDelete(_Ty);
    void DelayedDelete(_Ty);
    void DirectExpireSlot(MovementSlot,bool);
    void Mutate(MovementGenerator*,MovementSlot);
    bool InstallCheckedMovement(uint64,MovementGenerator*);
    bool ExpireOwnedMovement(uint64);
    _Ty GetMotionSlot(int slot) const { return Impl[slot]; }
    int GetCurrentMovementGeneratorType() const { return top()->GetMovementGeneratorType(); }
    int GetMotionSlotType(int slot) const { return Impl[slot] ? Impl[slot]->GetMovementGeneratorType() : NULL_MOTION_TYPE; }
};
struct MovementInfo
{
    uint32 flags=0;
    struct { Position pos; } transport;
    uint32 GetMovementFlags() const { return flags; }
    bool HasMovementFlag(uint32 mask) const { return flags&mask; }
    void SetMovementFlags(uint32 f) { flags=f; }
    void RemoveMovementFlag(uint32 f) { flags &= ~f; }
    static UnitMoveType GetSpeedType(uint32 f)
    { return f & MOVEMENTFLAG_FLYING ? MOVE_FLIGHT : f&MOVEMENTFLAG_SWIMMING ? MOVE_SWIM : MOVE_RUN; }
};
struct TransportBase
{
    Position pose;
    float GetPositionX() const { return pose.x; } float GetPositionY() const { return pose.y; }
    float GetPositionZ() const { return pose.z; } float GetOrientation() const { return pose.o; }
    static void CalculatePassengerPosition(float&,float&,float&,float*,float,float,float,float);
    static void CalculatePassengerOffset(float&,float&,float&,float*,float,float,float,float);
    void CalculatePassengerPosition(float& x,float& y,float& z,float* o=nullptr) const
    { CalculatePassengerPosition(x,y,z,o,pose.x,pose.y,pose.z,pose.o); }
    void CalculatePassengerOffset(float& x,float& y,float& z,float* o=nullptr) const
    { CalculatePassengerOffset(x,y,z,o,pose.x,pose.y,pose.z,pose.o); }
};
struct Transport : TransportBase
{
    ObjectGuid guid{9}; Map* map=nullptr; GameObjectModel* m_model=nullptr;
    ObjectGuid GetGUID() const { return guid; } bool IsInWorld() const { return true; }
    Map* GetMap() const { return map; }
    void Move(float x,float y,float z,float o)
    {
        pose={x,y,z,o}; m_model->iPos={x,y,z};
        m_model->iInvRot=G3D::Matrix3(std::cos(o),std::sin(o),0,-std::sin(o),std::cos(o),0,0,0,1);
    }
};
struct VehicleSeatEntry { bool control=true; bool CanControl() const { return control; } };
struct Vehicle;
struct FixtureAI
{
    int movementInforms=0;
    void MovementInform(int,uint32) { ++movementInforms; }
};
struct Unit : Position
{
    Unit() : mm(this) { }
    virtual ~Unit() = default;
    virtual Player* ToPlayer() { return nullptr; }
    uint64 control=1;
    uint64 GetControlIdentity() const { return control; }
    bool casting=false;
    bool IsNonMeleeSpellCast(bool) const { return casting; }
    unsigned statesCleared=0;
    void ClearUnitState(uint32) { ++statesCleared; }
    float GetExactDist(float a,float b,float c) const { return G3D::Vector3(x-a,y-b,z-c).length(); }
    void UpdateSplinePosition()
    {
        auto position=movespline->ComputePosition();
        x=position.x; y=position.y; z=position.z;
    }
    bool StopOwnedSpline(uint32);
    bool creature=false;
    FixtureAI brain;
    bool IsCreature() const { return creature; }
    Unit* ToCreature() { return creature ? this : nullptr; }
    FixtureAI* AI() { return &brain; }
    void RemoveUnitMovementFlag(uint32 f) { m_movementInfo.flags &= ~f; }
    ObjectGuid guid{1}, charmer;
    Map* map=nullptr; uint32 phase=1; bool alive=true,inWorld=true, rooted=false,lost=false,disable=false;
    MovementInfo m_movementInfo;
    Movement::MoveSpline spline; Movement::MoveSpline* movespline=&spline;
    MotionMaster mm;
    Transport* transport=nullptr; Vehicle* vehicle=nullptr;
    Unit* vehicleBase=nullptr;
    int8 seat=-1;
    float speeds[3]={7,4,7}, width=0.5f,height=1.8f;
    int stops=0,packets=0;
    ObjectGuid GetGUID() const { return guid; }
    ObjectGuid GetCharmerGUID() const { return charmer; }
    bool IsCharmed() const { return bool(charmer); }
    bool IsAlive() const { return alive; } bool IsInWorld() const { return inWorld; }
    bool IsRooted() const { return rooted; } bool IsImmobilizedState() const { return rooted; }
    bool HasUnitState(uint32) const { return lost; } bool HasUnitFlag(uint32) const { return disable; }
    bool HasUnitMovementFlag(uint32 f) const { return m_movementInfo.flags&f; }
    bool IsFlying() const { return HasUnitMovementFlag(MOVEMENTFLAG_FLYING); }
    bool isSwimming() const { return HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING); }
    Map* GetMap() const { return map; } uint32 GetMapId() const { return map->id; }
    uint32 GetInstanceId() const { return map->instance; } uint32 GetPhaseMask() const { return phase; }
    float GetSpeed(UnitMoveType t) const { return speeds[t]; }
    float GetCollisionWidth() const { return width; } float GetCollisionHeight() const { return height; }
    Transport* GetTransport() const { return transport; }
    TransportBase* GetDirectTransport() const { return transport; }
    ObjectGuid GetTransGUID() const { return transport ? transport->guid : vehicleBase ? vehicleBase->guid : ObjectGuid{}; }
    int8 GetTransSeat() const { return seat; }
    Vehicle* GetVehicle() const { return vehicle; } Unit* GetVehicleBase() const { return vehicleBase; }
    MotionMaster* GetMotionMaster() { return &mm; }
    uint32 GetPackGUID() const { return 0; } uint32 GetEntry() const { return 0; }
    template<class T> void SendMessageToSet(T*,bool) { ++packets; }
    void StopMoving();
};
struct Player : Unit
{
    PlayerbotAI* ai=nullptr;
    Player* ToPlayer() override { return this; }
    Unit* m_mover=this;
    bool teleport=false,taxi=false;
    bool IsBeingTeleported() const { return teleport; } bool IsInFlight() const { return taxi; }
};
struct Vehicle
{
    Unit* base=nullptr; VehicleSeatEntry seat;
    Unit* GetBase() const { return base; }
    VehicleSeatEntry const* GetSeatForPassenger(Unit*) { return &seat; }
};
namespace ObjectAccessor
{
inline std::map<uint64,Unit*> objects;
inline Unit* GetUnit(Unit& requester,ObjectGuid guid)
{
    auto it=objects.find(guid.raw);
    return it!=objects.end() && it->second->map==requester.map ? it->second : nullptr;
}
}
constexpr int PATHFIND_NORMAL=1, PATHFIND_SHORTCUT=2, PATHFIND_NOPATH=8, PATHFIND_SHORT=32;
struct PathGenerator
{
    // Constants and setter copied from the actual core header by the test runner.
#include "GroundPathLimit.inc"
    uint32 _pointPathLimit=MAX_POINT_PATH_LENGTH;
    int type=PATHFIND_NORMAL;
    Unit* actor; Movement::PointsArray route;
    inline static bool valid=true;
    inline static Movement::PointsArray custom;
    inline static unsigned calculations=0;
    inline static bool samePolygon=false;
    explicit PathGenerator(Unit* u):actor(u) { }
    bool CalculatePath(float x,float y,float z,bool)
    {
        ++calculations;
        route=custom.empty() ? Movement::PointsArray{{actor->x,actor->y,actor->z},{x,y,z}} : custom;
        type=valid ? PATHFIND_NORMAL : PATHFIND_NOPATH;
        // Capacity classification double, separately verified with real BuildPointPath/Detour.
        // Its one-poly/single-point special case bypasses the saturated-buffer rejection.
        if (valid && route.size() >= _pointPathLimit && !(samePolygon && _pointPathLimit == 1))
            type=PATHFIND_SHORTCUT | (_pointPathLimit < 2 ? PATHFIND_NOPATH : PATHFIND_SHORT);
        return valid;
    }
    bool CalculatePath(float,float,float,float x,float y,float z,bool force)
    { return CalculatePath(x,y,z,force); }
    int GetPathType() const { return type; }
    Movement::PointsArray const& GetPath() const { return route; }
};
constexpr int SMSG_MONSTER_MOVE=1, SMSG_MONSTER_MOVE_TRANSPORT=2;
struct WorldPacket
{
    WorldPacket(int,int) { } void SetOpcode(int) { }
    template<class T> WorldPacket& operator<<(T const&) { return *this; }
};
namespace Movement
{
struct PacketBuilder
{
    static void WriteMonsterMove(MoveSpline const&,WorldPacket&) { }
    static void WriteStopMovement(Location const&,uint32,WorldPacket&) { }
};
}

inline uint32 clockNow=100;
inline uint32 getMSTime() { return clockNow; }
inline uint32 getMSTimeDiff(uint32 a,uint32 b) { return b-a; }
struct LastMovement
{
    uint64 cthunOwner=0;
    uint32 msTime=0, cthunManual=0, cthunAutomatic=0;
    float lastdelayTime=0;
    void clear() { cthunOwner=0; msTime=cthunManual=cthunAutomatic=0; lastdelayTime=0; }
};
struct LastValue { LastMovement value; LastMovement& Get() { return value; } };
struct Context
{
    LastValue last;
    template<class T> LastValue* GetValue(char const*) { return &last; }
};
class PlayerbotAI
{
public:
    Player bot;
    Context context;
    RaidCombat::State raidCombat;
    bool enabled=true;
    PlayerbotAI() { bot.ai=this; }
    Player* GetBot() { return &bot; }
    Context* GetAiObjectContext() { return &context; }
};
#define GET_PLAYERBOT_AI(player) ((player)->ai)
namespace RaidCombat
{
inline bool Eligible(PlayerbotAI& ai)
{
    auto& bot=ai.bot;
    return ai.enabled && bot.IsAlive() && !bot.IsRooted() && !bot.lost && !bot.IsCharmed() && !bot.teleport;
}
}
