#include "SpellCastAdmission.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
using uint8=uint8_t; using uint32=uint32_t; using uint64=uint64_t;
constexpr uint32 UNIT_STATE_IN_FLIGHT=1, UNIT_STATE_LOST_CONTROL=2;
// FLIGHT_CONSTANTS
struct MovementInfo {bool const* fly;uint32 flags=0;
 bool HasMovementFlag(uint32 mask)const{return ((flags|(*fly?MOVEMENTFLAG_FLYING:0))&mask)!=0;}};
constexpr int FORM_FLIGHT=1, FORM_FLIGHT_EPIC=2, CURRENT_CHANNELED_SPELL=1, POWER_MANA=0;
constexpr int UNIT_STAND_STATE_STAND=0, CAST_ANGLE_IN_FRONT=1, SPELL_FACING_FLAG_INFRONT=1;
constexpr int TARGET_FLAG_ITEM=1, TARGET_FLAG_GAMEOBJECT_ITEM=2, TARGET_FLAG_DEST_LOCATION=4, TARGET_FLAG_SOURCE_LOCATION=8;
constexpr int SPELL_EFFECT_OPEN_LOCK=33, SPELL_EFFECT_SKINNING=34, TRADE_SLOT_NONTRADED=0;
constexpr int TRIGGERED_NONE=0, TRIGGERED_IGNORE_POWER_AND_REAGENT_COST=1, BOT_STATE_NON_COMBAT=0, CMSG_GAMEOBJ_USE=0;
enum SpellCastResult { SPELL_CAST_OK, SPELL_FAILED_NOT_INFRONT, SPELL_FAILED_NOT_STANDING,
 SPELL_FAILED_UNIT_NOT_INFRONT, SPELL_FAILED_MOVING, SPELL_FAILED_TRY_AGAIN, SPELL_FAILED_NOT_SHAPESHIFT,
 SPELL_FAILED_OUT_OF_RANGE, SPELL_FAILED_NOT_READY, TEST_LOS, TEST_POWER, TEST_CONTROL };
struct ObjectGuid {uint64 id=0; ObjectGuid(uint64 x=0):id(x){}; uint64 GetRawValue() const{return id;}
 std::string ToString() const{return std::to_string(id);} explicit operator bool()const{return id!=0;}
 uint32 GetCounter()const{return id;} uint32 GetHigh()const{return 0;}};
struct Map {bool raid=true; bool IsRaid()const{return raid;}} world;
struct Group {bool raid=true; bool isRaidGroup()const{return raid;}} group;
struct WorldObject {virtual ~WorldObject()=default;};
struct SpellInfo {uint32 Id=100, Targets=0, FacingCasterFlags=0; bool positive=true, channel=false, repeat=false;
 uint32 castTime=0; struct EffectInfo {int Effect=0;} Effects[3];
 bool IsPositive()const{return positive;} bool IsChanneled()const{return channel;}
 uint32 CalcCastTime(void*)const{return castTime;} uint32 GetDuration()const{return 1000;}
 bool IsAutoRepeatRangedSpell()const{return repeat;}} info;
struct SpellMgr {SpellInfo const* GetSpellInfo(uint32 id){return id==info.Id?&info:nullptr;}} spellMgr;
auto* sSpellMgr=&spellMgr;
struct Unit:WorldObject {bool inWorld=true,friendly=true,immune=false; ObjectGuid guid{2}; float hp=6,x=10,y=0,z=0;
 bool IsInWorld()const{return inWorld;} Map* GetMap()const{return &world;}
 ObjectGuid GetGUID()const{return guid;} float GetHealthPct()const{return hp;} float GetPositionZ()const{return z;}
 std::string GetName()const{return "recipient";} bool IsImmunedToSpell(SpellInfo const*){return immune;}} recipient;
struct Item:WorldObject {uint32 GetEntry(){return 1;} ObjectGuid GetGUID(){return {1};}};
struct GameObject:WorldObject {bool isSpawned(){return true;} ObjectGuid GetGUID(){return {1};}};
struct TradeData {void SetSpell(uint32){} TradeData* GetTraderData(){return this;} Item* GetItem(int){return nullptr;}};
struct Pet {bool known=false; unsigned toggles=0; std::vector<unsigned> m_autospells;
 bool HasSpell(uint32){return known;} void ToggleAutocast(SpellInfo const*,bool){++toggles;}};
struct WorldPacket {explicit WorldPacket(int,int=0){} template<class T>WorldPacket& operator<<(T const&){return *this;}};
struct Session {void HandleGameObjectUseOpcode(WorldPacket&) {}} session;
struct Player:Unit {bool flying=false,taxi=false,mounted=false,vehicle=false,aura=false,moving=false,known=true;
 bool standing=true,control=false,cooldown=false,combat=true,channel=false; int form=0; float mana=100;
 Pet* pet=nullptr; Unit* selected=nullptr;
 MovementInfo m_movementInfo{&flying};
 // IS_FLYING
 bool HasUnitState(uint32 state)const{return state==1?taxi:control;}
 bool IsMounted()const{return mounted;} void* GetVehicle()const{return vehicle?(void*)this:nullptr;}
 bool HasAuraType(int id)const{return id==SPELL_AURA_FLY&&aura;}
 // HAS_FLY_AURA
 int GetShapeshiftForm()const{return form;}
 bool IsInCombat()const{return combat;} Group* GetGroup()const{return &group;}
 bool HasSpell(uint32)const{return known;} uint32 GetMapId()const{return 550;} uint32 GetInstanceId()const{return 7;}
 bool isMoving()const{return moving;} uint32 GetUnitState()const{return (taxi?1:0)|(control?2:0);}
 float GetPowerPct(int)const{return mana;} float GetExactDist(Unit const* u)const{
 return std::sqrt((x-u->x)*(x-u->x)+(y-u->y)*(y-u->y)+(z-u->z)*(z-u->z));}
 bool IsFriendlyTo(Unit const* u)const{return u->friendly;} Pet* GetPet(){return pet;}
 void* GetCurrentSpell(int){return channel?(void*)this:nullptr;} bool HasSpellCooldown(uint32){return cooldown;}
 Unit* GetSelectedUnit(){return selected;} void SetSelection(ObjectGuid){}
 bool IsStandState(){return standing;} void SetStandState(int){standing=true;}
 bool HasInArc(int,WorldObject*){return true;} TradeData* GetTradeData(){return nullptr;}
 Player* GetTrader(){return nullptr;} Session* GetSession(){return &session;}
 float GetPositionX(){return x;} float GetPositionY(){return y;}
 float GetExactDist(float a,float b,float c){Unit t;t.x=a;t.y=b;t.z=c;return GetExactDist(&t);}
 float GetDistance(float a,float b,float c){return GetExactDist(a,b,c);}
 void SetFacingTo(float){} float GetAngle(float,float){return 0;}};
struct ServerFacade {static ServerFacade& instance(){static ServerFacade v;return v;}
 float GetDistance2d(Player* p,Unit* t){return p->GetExactDist(t);} void SetFacingTo(Player*,WorldObject*){}};
struct SpellCastTargets {Unit* unit=nullptr; GameObject* go=nullptr; Item* item=nullptr;
 void SetUnitTarget(Unit* u){unit=u;} void SetItemTarget(Item* i){item=i;}
 void SetDst(float,float,float,float){} void SetDst(Unit const&){} void SetGOTarget(GameObject* g){go=g;}
 void SetTradeItemTarget(Player*){} Unit* GetUnitTarget(){return unit;} GameObject* GetGOTarget(){return go;}
 Item* GetItemTarget(){return item;}};
SpellCastResult checkResult=SPELL_CAST_OK,prepareResult=SPELL_CAST_OK;
unsigned prepared=0,checked=0,logs=0; uint32 clockMs=100;
uint32 getMSTime(){return clockMs;}
struct Spell {static inline std::vector<Spell*> live; SpellInfo const* inf; SpellCastTargets m_targets; Item* m_CastItem=nullptr;
 Spell(Player*,SpellInfo const* i,int):inf(i){assert(i);live.push_back(this);}
 ~Spell(){live.erase(std::find(live.begin(),live.end(),this));}
 SpellCastResult CheckCast(bool){++checked;return checkResult;}
 SpellCastResult prepare(SpellCastTargets*){++prepared;return prepareResult;}
 uint32 GetCastTime(){return inf->castTime;} void cancel(){}};
struct Config {bool logInGroupOnly=true;uint32 reactDelay=100,globalCoolDown=1500;float sightDistance=100;}sPlayerbotAIConfig;
#define LOG_DEBUG(...) ((void)0)
template<class... Args>void logInfo(Args const&...){++logs;}
#define LOG_INFO(...) logInfo(__VA_ARGS__)
struct ChatHelper {static std::string FormatSpell(SpellInfo const*){return "spell";}};
struct Position {float x=0,y=0,z=0;Position(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 float GetPositionX()const{return x;}float GetPositionY()const{return y;}float GetPositionZ()const{return z;}
 void Reset(){}}; using PositionMap=std::map<std::string,Position>;
struct LastSpellCast {void Set(uint32,ObjectGuid,time_t){}};
struct LootObject {ObjectGuid guid; bool IsLootPossible(Player*){return true;}};
template<class T>struct Value {using U=std::remove_reference_t<T>;U value{};U& Get(){return value;} operator U&(){return value;}};
struct Context {template<class T,class... Args>Value<T>* GetValue(Args const&...){static Value<T> v;return &v;}
 template<class T>Value<T>& operator[](char const*){static Value<T> v;return v;}};
// Native loot access uses unary * on its value wrapper.
template<class T>T& operator*(Value<T>& v){return v.Get();}
struct PlayerbotAI {Player* bot; Context context; Context* aiObjectContext=&context; ChatHelper chatHelper;
 ai::casting::Trace spellCastTrace; bool master=true;
 explicit PlayerbotAI(Player* p):bot(p){}
 bool HasRealPlayerMaster(){return master;} bool IsValidUnit(Unit* u){return u&&u->IsInWorld();}
 void TellMaster(std::ostringstream&){} void TellMasterNoFacing(std::ostringstream&){}
 void SetNextCheckDelay(uint32){} bool HasStrategy(char const*,int){return false;}
 GameObject* GetGameObject(ObjectGuid){return nullptr;} Unit* GetUnit(ObjectGuid){return nullptr;}
 bool IsSpellFlightAllowed(uint32,Unit*,ai::casting::Stage,Position const* = nullptr);
 void RecordSpellCastFailure(uint32,Unit*,ai::casting::Stage,ai::casting::Reason,int=-1,Position const* = nullptr);
 std::string DescribeSpellCastFailures()const;
 bool CanCastSpell(uint32,Unit*,bool=true,Item* = nullptr,Item* = nullptr);
 bool CanCastSpell(uint32,float,float,float,bool=true,Item* = nullptr);
 bool CastSpell(uint32,Unit*,Item* = nullptr);
 bool CastSpell(uint32,float,float,float,Item* = nullptr);
};
// HELPERS
// PRODUCTION
void cleanup(){while(!Spell::live.empty())delete Spell::live.back();}
int main(int argc,char** argv)
{
 std::string scenario=argc>1?argv[1]:"all";
 Player p;p.x=0;p.guid={1};PlayerbotAI ai(&p);
 if(scenario=="ground-reject"){
  info.Targets=TARGET_FLAG_DEST_LOCATION;prepareResult=TEST_LOS;
  assert(!ai.CastSpell(info.Id,10.f,0.f,0.f));cleanup();return 0;
 }
 for(int kind=0;kind<4;++kind){
  info.positive=kind<2;recipient.friendly=info.positive;info.castTime=kind%2?1500:0;
  p.flying=p.aura=true;
  info.Targets=0;
  assert(ai.CanCastSpell(info.Id,&recipient));
  unsigned before=prepared;assert(ai.CastSpell(info.Id,&recipient));assert(prepared==before+1);cleanup();
  info.Targets=TARGET_FLAG_DEST_LOCATION;
  assert(ai.CanCastSpell(info.Id,10.f,0.f,0.f));
  before=prepared;assert(ai.CastSpell(info.Id,10.f,0.f,0.f));assert(prepared==before+1);cleanup();
 }
 p.flying=false;p.m_movementInfo.flags=MOVEMENTFLAG_DISABLE_GRAVITY;
 assert(p.IsFlying());assert(ai.CanCastSpell(info.Id,&recipient));assert(ai.CastSpell(info.Id,&recipient));cleanup();
 assert(ai.CanCastSpell(info.Id,10.f,0.f,0.f));assert(ai.CastSpell(info.Id,10.f,0.f,0.f));cleanup();
 p.aura=false;assert(!ai.CastSpell(info.Id,&recipient));p.m_movementInfo.flags=0;p.flying=true;
 for(int denial=0;denial<6;++denial){
  p.aura=denial!=0;p.taxi=denial==1;p.mounted=denial==2;p.vehicle=denial==3;
  p.form=denial==4?FORM_FLIGHT:(denial==5?FORM_FLIGHT_EPIC:0);
  unsigned before=prepared;
  assert(!ai.CanCastSpell(info.Id,&recipient));assert(!ai.CastSpell(info.Id,&recipient));
  assert(!ai.CanCastSpell(info.Id,10.f,0.f,0.f));assert(!ai.CastSpell(info.Id,10.f,0.f,0.f));
  assert(prepared==before);
 }
 p.aura=true;p.taxi=p.mounted=p.vehicle=false;p.form=0;info.Targets=0;info.positive=true;recipient.friendly=true;
 for(auto reason:{TEST_LOS,TEST_POWER,TEST_CONTROL,SPELL_FAILED_OUT_OF_RANGE}){
  prepareResult=reason;assert(!ai.CastSpell(info.Id,&recipient));cleanup();
  info.Targets=TARGET_FLAG_DEST_LOCATION;assert(!ai.CastSpell(info.Id,10.f,0.f,0.f));cleanup();info.Targets=0;
  auto d=ai.spellCastTrace.Read();assert(d.last[1].result==reason && d.last[1].reason==ai::casting::Reason::Core);
  assert(d.last[1].destination && d.last[1].destX==10 && d.last[1].distance==10);
 }
 prepareResult=SPELL_CAST_OK;checkResult=TEST_LOS;
 assert(!ai.CanCastSpell(info.Id,&recipient));assert(!ai.CanCastSpell(info.Id,10.f,0.f,0.f));
 checkResult=SPELL_CAST_OK;p.control=true;assert(!ai.CanCastSpell(info.Id,&recipient));p.control=false;
 p.cooldown=true;assert(!ai.CanCastSpell(info.Id,&recipient));p.cooldown=false;
 p.known=false;assert(!ai.CanCastSpell(info.Id,&recipient));p.known=true;
 p.moving=true;info.castTime=1500;assert(!ai.CanCastSpell(info.Id,&recipient));
 assert(!ai.CastSpell(info.Id,&recipient));cleanup();
 info.castTime=0;assert(ai.CanCastSpell(info.Id,&recipient));assert(ai.CastSpell(info.Id,&recipient));cleanup();
 p.moving=false;p.standing=false;assert(!ai.CastSpell(info.Id,&recipient));assert(p.standing);
 assert(!ai.CastSpell(999,&recipient));assert(!ai.CastSpell(999,1.f,2.f,3.f));
 Pet pet;pet.known=true;p.pet=&pet;p.aura=false;assert(ai.CastSpell(info.Id,&recipient));assert(pet.toggles==1);p.pet=nullptr;
 p.flying=false;p.aura=false;assert(ai.CastSpell(info.Id,&recipient));cleanup();
 p.mounted=true;prepareResult=TEST_CONTROL;assert(!ai.CastSpell(info.Id,&recipient));cleanup();
 assert(!ai.CastSpell(info.Id,10.f,0.f,0.f));cleanup();p.mounted=false;prepareResult=SPELL_CAST_OK;
 p.taxi=true;assert(!ai.CastSpell(info.Id,&recipient));p.taxi=false;
 auto initial=ai.spellCastTrace.Read().count;ai.master=false;
 ai.RecordSpellCastFailure(info.Id,&recipient,ai::casting::Stage::Execute,ai::casting::Reason::Flight);
 assert(ai.spellCastTrace.Read().count==initial);ai.master=true;
 world.raid=false;ai.RecordSpellCastFailure(info.Id,&recipient,ai::casting::Stage::Execute,ai::casting::Reason::Flight);
 assert(ai.spellCastTrace.Read().count==initial);world.raid=true;
 assert(ai.DescribeSpellCastFailures().find("CASTFAIL")!=std::string::npos);
 // Truth table and diagnostic retention/rate limits use the actual production header.
 for(unsigned mask=0;mask<64;++mask){ai::casting::Flight s;
  s.flying=mask&1;s.taxi=mask&2;s.mounted=mask&4;s.vehicle=mask&8;s.aura=mask&16;s.form=mask&32;
  assert(ai::casting::FlightAllowed(s)==(!s.taxi&&(!s.flying||(s.aura&&!s.mounted&&!s.vehicle&&!s.form))));
 }
 ai::casting::Trace trace;ai::casting::Failure f;f.at=100;f.map=550;f.instance=1;
 assert(trace.Record(f));assert(!trace.Record(f));f.at=5099;assert(!trace.Record(f));f.at=5100;assert(trace.Record(f));
 f.positive=true;f.health=6;assert(trace.Record(f));assert(!trace.Record(f));
 auto d=trace.Read();assert(d.valid[2]&&d.last[2].health==6);
 f.positive=false;trace.Record(f);assert(trace.Read().last[2].health==6);
 f.instance=2;assert(trace.Record(f));assert(!trace.Read().valid[2]);
 ai::casting::Trace wrap;f.at=0xfffffff0;assert(wrap.Record(f));f.at=20;assert(!wrap.Record(f));
 f.at=4984;assert(wrap.Record(f));
 ai::casting::Trace boundary;f.positive=true;f.health=30;boundary.Record(f);assert(!boundary.Read().valid[2]);
 f.health=29.9f;boundary.Record(f);assert(boundary.Read().valid[2]);
 std::thread writer([&]{for(unsigned i=0;i<10000;++i){f.at=i;trace.Record(f);}});
 for(unsigned i=0;i<10000;++i){auto value=trace.Read();assert(value.last[0].instance==2);}writer.join();
 std::cout<<"Production unit/position casts, flight exclusions, core failures and bounded diagnostics passed\n";
}
