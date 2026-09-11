// Offline API doubles for production Twins coordination code, NOT a combat simulator.
#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
using uint8 = uint8_t;
using uint32 = uint32_t;
using int32 = int32_t;
constexpr int CLASS_WARRIOR=1, CLASS_PALADIN=2, CLASS_HUNTER=3, CLASS_ROGUE=4, CLASS_PRIEST=5,
    CLASS_DEATH_KNIGHT=6, CLASS_SHAMAN=7, CLASS_MAGE=8, CLASS_WARLOCK=9, CLASS_DRUID=11;
constexpr int IN_PROGRESS=1, DONE=3, BOT_STATE_COMBAT=1, UNIT_STATE_MELEE_ATTACKING=1, UNIT_STATE_FOLLOW=2,
    UNIT_STATE_ROOT=4, CURRENT_AUTOREPEAT_SPELL=0, REACT_PASSIVE=0, REACT_DEFENSIVE=1, CREATURE_FLAG_EXTRA_NO_TAUNT=1,
    FLEEING_MOTION_TYPE=1, TIMED_FLEEING_MOTION_TYPE=2;
template<class... T> void TestLog(T const&...) {}
#define LOG_DEBUG(...) TestLog(__VA_ARGS__)
struct ObjectGuid
{
    uint32 id=0;
    ObjectGuid()=default;
    ObjectGuid(uint32 id):id(id){}
    bool operator<(ObjectGuid b) const {return id<b.id;}
    bool operator==(ObjectGuid b) const {return id==b.id;}
    explicit operator bool() const {return id!=0;}
    bool IsPlayer() const {return id<10000;}
};
namespace std {template<> struct hash<ObjectGuid> {size_t operator()(ObjectGuid a) const{return a.id;}};}
using GuidVector=std::vector<ObjectGuid>;
struct Position
{
    float x=0,y=0,z=0;
    void Relocate(float xx,float yy,float zz=0){x=xx;y=yy;z=zz;}
    float GetPositionX() const{return x;}
    float GetPositionY() const{return y;}
    float GetPositionZ() const{return z;}
    float GetExactDist2d(float xx,float yy) const{return std::hypot(x-xx,y-yy);}
    float GetExactDist2d(Position const* p) const{return GetExactDist2d(p->x,p->y);}
    float GetExactDist(float xx,float yy,float zz) const{return std::hypot(GetExactDist2d(xx,yy),z-zz);}
};
class Unit; class Player; class PlayerbotAI; class Creature; class Guardian; class Pet; struct Group;
struct ThreatManager
{
    std::unordered_map<Unit*,float> threat;
    Unit* current=nullptr;
    bool canHave=true;
    bool CanHaveThreatList() const{return canHave;}
    Unit* GetCurrentVictim(){return current;}
    float GetThreat(Unit* u){return threat[u];}
};
struct InstanceScript
{
    int state=IN_PROGRESS;
    std::map<uint32,Creature*> creatures;
    int GetBossState(uint32){return state;}
    Creature* GetCreature(uint32 id){return creatures[id];}
};
struct Map
{
    uint32 id=531,instance=1;
    InstanceScript script;
    std::vector<Creature*> bugs;
    int searches=0;
    uint32 GetInstanceId(){return instance;}
};
struct SpellInfo {uint32 Id=0;};
struct Spell
{
    struct Targets {Unit* unit=nullptr;Unit* GetUnitTarget() const{return unit;}} m_targets;
};
struct MotionMaster {int type=0;int GetCurrentMovementGeneratorType(){return type;}};
class Unit : public Position
{
public:
    virtual ~Unit()=default;
    ObjectGuid guid;
    std::string name="unit";
    Map* map=nullptr;
    bool alive=true,inWorld=true,los=true,moving=false,charmed=false,casting=false;
    uint32 phase=1,flags=0;
    float hp=100;
    Unit* victim=nullptr;
    ThreatManager tm;
    MotionMaster motion;
    Spell* autoSpell=nullptr;
    std::set<uint32> auras;
    int attackStops=0,moveStops=0,interrupts=0;
    ObjectGuid GetGUID() const{return guid;}
    std::string const& GetName() const{return name;}
    Map* GetMap() const{return map;}
    uint32 GetMapId() const{return map?map->id:0;}
    bool IsAlive() const{return alive;}
    bool isDead() const{return !alive;}
    bool IsInWorld() const{return inWorld;}
    bool InSamePhase(Unit const* other) const{return phase==other->phase;}
    Unit* GetVictim(){return victim;}
    virtual bool IsInCombat() const{return map && map->script.state==IN_PROGRESS;}
    bool IsWithinLOSInMap(Unit const* other) const{return los && other->los;}
    bool IsWithinLOS(float,float,float) const{return los;}
    float GetDistance2d(Unit const* p) const{return GetExactDist2d(p);}
    bool IsWithinMeleeRange(Unit const* p) const{return GetExactDist2d(p)<=7.33f;}
    float GetHealthPct() const{return hp;}
    bool isMoving() const{return moving;}
    void UpdateAllowedPositionZ(float,float,float&){}
    bool HasAura(uint32 aura) const{return auras.contains(aura);}
    ThreatManager& GetThreatMgr(){return tm;}
    bool IsCharmed() const{return charmed;}
    MotionMaster* GetMotionMaster(){return &motion;}
    virtual Player* ToPlayer(){return nullptr;}
    virtual Creature* ToCreature(){return nullptr;}
    void AttackStop(){victim=nullptr;flags&=~UNIT_STATE_MELEE_ATTACKING;++attackStops;}
    void StopMovingOnCurrentPos(){moving=false;++moveStops;}
    bool HasUnitState(uint32 flag) const{return flags&flag;}
    void ClearUnitState(uint32 flag){flags&=~flag;}
    void SetTarget(ObjectGuid){}
    virtual ObjectGuid GetTarget(){return {};}
    Spell const* GetCurrentSpell(int) const{return autoSpell;}
    void InterruptSpell(int){autoSpell=nullptr;++interrupts;}
    bool IsNonMeleeSpellCast(bool) const{return casting;}
    void InterruptNonMeleeSpells(bool){casting=false;++interrupts;}
};
class DynamicObject : public Position {};
struct CharmInfo
{
    bool attack=false;
    void SetIsCommandAttack(bool b){attack=b;}
    bool IsCommandAttack() const{return attack;}
    void SetIsAtStay(bool){} void SetIsFollowing(bool){} void SetIsCommandFollow(bool){} void SetIsReturning(bool){}
};
struct CreatureAI {Creature* owner;void AttackStart(Unit*);};
class Creature : public Unit
{
public:
    bool boss=true,IsAIEnabled=true;
    int react=REACT_DEFENSIVE;
    DynamicObject* blizzard=nullptr;
    Position home;
    Position const& GetHomePosition() const{return home;}
    CreatureAI ai{this};
    CharmInfo charm;
    Creature* ToCreature() override{return this;}
    bool IsDungeonBoss() const{return boss;}
    bool isWorldBoss() const{return false;}
    bool HasFlagsExtra(int) const{return boss;}
    bool IsImmunedToSpell(SpellInfo const*,void*) const{return boss;}
    DynamicObject* GetDynObject(uint32){return blizzard;}
    CreatureAI* AI(){return &ai;}
    bool IsTotem() const{return false;}
    int GetReactState() const{return react;}
    void SetReactState(int r){react=r;}
    CharmInfo* GetCharmInfo(){return &charm;}
};
inline void CreatureAI::AttackStart(Unit* t){owner->victim=t;}
class Guardian : public Creature {};
class Pet : public Guardian {};
struct GroupReference
{
    Player* member=nullptr;
    GroupReference* following=nullptr;
    Player* GetSource(){return member;}
    GroupReference* next(){return following;}
};
struct Group
{
    bool raid=true;
    std::deque<GroupReference> refs;
    bool isRaidGroup() const{return raid;}
    GroupReference* GetFirstMember(){return refs.empty()?nullptr:&refs.front();}
    void Add(Player* p){if(!refs.empty())refs.back().following=nullptr;refs.push_back({p,nullptr});
        if(refs.size()>1)refs[refs.size()-2].following=&refs.back();}
};
class Player : public Unit
{
public:
    PlayerbotAI* ai=nullptr;
    Group* group=nullptr;
    bool tankSpec=false,healer=false,melee=false,gm=false;
    int cls=CLASS_WARRIOR;
    Pet* pet=nullptr;
    std::vector<Unit*> m_Controlled;
    ObjectGuid selected;
    Player* ToPlayer() override{return this;}
    Group* GetGroup(){return group;}
    bool IsGameMaster() const{return gm;}
    int getClass() const{return cls;}
    InstanceScript* GetInstanceScript(){return map?&map->script:nullptr;}
    bool IsValidAttackTarget(Unit* u){return u && u->ToCreature() && u->alive && u->inWorld && u->map==map;}
    void GetCreatureListWithEntryInGrid(std::list<Creature*>& out,std::vector<uint32> const&,float radius)
    {++map->searches;for(auto* bug:map->bugs)if(GetExactDist2d(bug)<=radius)out.push_back(bug);}
    Pet* GetPet(){return pet;}
    Guardian* GetGuardianPet(){return pet;}
    ObjectGuid GetTarget(){return selected;}
    Unit* GetCharm(){return nullptr;}
    bool IsInSameGroupWith(Player* p){return group==p->group;}
};
#define GET_PLAYERBOT_AI(p) ((p)->ai)
inline uint32 clockMs=1000;
inline uint32 getMSTime(){return clockMs;}
inline uint32 getMSTimeDiff(uint32 from,uint32 to){return to-from;}
enum class MovementPriority {MOVEMENT_NORMAL=0,MOVEMENT_COMBAT=20};
struct LastMovement
{
    Position lastMoveShort;
    MovementPriority priority=MovementPriority::MOVEMENT_NORMAL;
    void clear(){lastMoveShort={};priority=MovementPriority::MOVEMENT_NORMAL;}
};
struct AnyValue {virtual ~AnyValue()=default;};
template<class T> struct TestValue : AnyValue
{
    std::remove_reference_t<T> value{};
    T Get(){return value;}
    void Set(T v){value=v;}
};
struct AiObjectContext
{
    std::map<std::string,std::unique_ptr<AnyValue>> values;
    template<class T> TestValue<T>* GetValue(std::string name,std::string qualifier="")
    {name+=qualifier;auto& v=values[name];if(!v)v=std::make_unique<TestValue<T>>();return dynamic_cast<TestValue<T>*>(v.get());}
};
class PlayerbotAI
{
public:
    Player* bot;
    Player* master=nullptr;
    AiObjectContext ctx;
    bool real=false,canMove=true,castAllowed=true,focus=false,pathAllowed=true;
    std::set<std::string> known={"shadow ward","searing pain","frostbolt","smite"},buffs;
    std::vector<std::string> casts,messages;
    std::vector<Position> moves;
    explicit PlayerbotAI(Player* p):bot(p){p->ai=this;ctx.GetValue<bool>("group")->Set(true);}
    Player* GetBot(){return bot;}
    AiObjectContext* GetAiObjectContext(){return &ctx;}
    bool IsRealPlayer() const{return real;}
    static bool IsTank(Player* p,bool=false){return p && p->tankSpec;}
    static bool IsHeal(Player* p,bool=false){return p && p->healer;}
    static bool IsMelee(Player* p){return p && p->melee;}
    float GetRange(std::string const&) const{return 38.5f;}
    bool HasStrategy(std::string const& name,int){return focus && name=="focus heal targets";}
    bool CanMove() const{return canMove;}
    bool HasAura(std::string const& n,Unit*){return buffs.contains(n);}
    bool CanCastSpell(std::string const& n,Unit* t){return castAllowed && known.contains(n) && t && bot->GetDistance2d(t)<=34;}
    bool CastSpell(std::string const& n,Unit*){casts.push_back(n);if(n=="shadow ward")buffs.insert(n);return true;}
    void TellMaster(std::string const& s){messages.push_back(s);}
    Unit* GetUnit(ObjectGuid g){for(auto& [_,c]:bot->map->script.creatures)if(c->guid==g)return c;return nullptr;}
};
#define AI_VALUE(type,name) (context->GetValue<type>(name)->Get())
#define AI_VALUE2(type,name,q) (context->GetValue<type>(name,q)->Get())
struct Event {};
class Action
{
public:
    enum class ActionThreatType{None,Single,Aoe};
    PlayerbotAI* botAI;Player* bot;AiObjectContext* context;std::string name;
    Action(PlayerbotAI* a,std::string n="action"):botAI(a),bot(a->bot),context(&a->ctx),name(n){}
    virtual ~Action()=default;
    virtual bool Execute(Event){return false;}
    virtual bool isUseful(){return true;}
    virtual bool isPossible(){return true;}
    virtual ActionThreatType getThreatType(){return ActionThreatType::None;}
    std::string const& getName(){return name;}
};
class MovementAction : public Action
{
public:
    using Action::Action;
protected:
    bool MoveTo(uint32,float x,float y,float z,bool,bool,bool,bool,MovementPriority p,bool=false,bool=false)
    {if(!botAI->pathAllowed)return false;Position pos;pos.Relocate(x,y,z);botAI->moves.push_back(pos);bot->moving=true;
        auto& last=AI_VALUE(LastMovement&,"last movement");last.lastMoveShort=pos;last.priority=p;return true;}
};
class AttackAction : public MovementAction
{
public:
    using MovementAction::MovementAction;
    bool Attack(Unit* u){context->GetValue<Unit*>("current target")->Set(u);bot->victim=u;return true;}
};
#define TEST_ACTION(name,base) class name : public base {public:using base::base;}
TEST_ACTION(MeleeAction,AttackAction);
TEST_ACTION(DpsAssistAction,AttackAction);TEST_ACTION(DpsAoeAction,AttackAction);TEST_ACTION(TankAssistAction,AttackAction);
TEST_ACTION(AggressiveTargetAction,AttackAction);TEST_ACTION(AttackAnythingAction,AttackAction);
TEST_ACTION(AttackLeastHpTargetAction,AttackAction);TEST_ACTION(AttackRtiTargetAction,AttackAction);
TEST_ACTION(FollowAction,MovementAction);TEST_ACTION(CombatFormationMoveAction,MovementAction);
TEST_ACTION(RearFlankAction,MovementAction);TEST_ACTION(ReachTargetAction,MovementAction);
TEST_ACTION(FleeAction,MovementAction);TEST_ACTION(FleeWithPetAction,MovementAction);
TEST_ACTION(FleeToGroupLeaderAction,MovementAction);TEST_ACTION(RunAwayAction,MovementAction);
TEST_ACTION(MoveOutOfEnemyContactAction,MovementAction);
TEST_ACTION(PetAttackAction,Action);
class CastSpellAction : public Action
{
public:
    using Action::Action;
    Unit* recipient=nullptr;
    Unit* GetTarget(){return recipient?recipient:AI_VALUE(Unit*,"current target");}
    ActionThreatType getThreatType() override{return ActionThreatType::Single;}
};
class CastHealingSpellAction : public CastSpellAction
{
public:
    using CastSpellAction::CastSpellAction;
    ActionThreatType getThreatType() override{return ActionThreatType::Aoe;}
};
TEST_ACTION(CastDebuffSpellOnAttackerAction,CastSpellAction);
class Multiplier : public Action
{
public:using Action::Action;virtual float GetValue(Action*)=0;
};
struct NextAction {NextAction(std::string,float){}};
struct TriggerNode {};
class Strategy
{
public:
    PlayerbotAI* botAI;
    explicit Strategy(PlayerbotAI* ai):botAI(ai){}
    virtual ~Strategy()=default;
    virtual std::string const getName(){return {};}
    virtual void InitMultipliers(std::vector<Multiplier*>&){}
    virtual void InitTriggers(std::vector<TriggerNode*>&){}
    virtual std::vector<NextAction> getDefaultActions(){return {};}
};
struct TestConfig
{
    template<class T> T GetOption(std::string const&,T value){return value;}
};
inline TestConfig config;
inline auto* sConfigMgr=&config;
struct TestSpellMgr {SpellInfo const* GetSpellInfo(uint32){return nullptr;}};
inline TestSpellMgr spellMgr;
inline auto* sSpellMgr=&spellMgr;
struct TestAiConfig {float healDistance=38.5f,mediumHealth=50;};
inline TestAiConfig sPlayerbotAIConfig;
struct MinValueCalculator
{
    float minValue;void* param=nullptr;
    explicit MinValueCalculator(float v):minValue(v){}
    void probe(float v,void* p){minValue=v;param=p;}
};
namespace ObjectAccessor {inline Player* FindPlayer(ObjectGuid){return nullptr;}}
