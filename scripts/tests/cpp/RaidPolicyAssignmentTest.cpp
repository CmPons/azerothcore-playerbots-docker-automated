// Production resolver + TK coexistence adapters, with bounded external-world doubles.
#include "RaidCombatData.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
struct ObjectGuid { uint64_t value; explicit ObjectGuid(uint64_t v=0):value(v){} };
constexpr unsigned UNIT_FLAG_NOT_SELECTABLE=1, UNIT_FLAG_NON_ATTACKABLE=2, UNIT_STATE_DIED=4;
struct Player;
struct GroupReference
{
    Player* player=nullptr; GroupReference* following=nullptr;
    Player* GetSource(){return player;} GroupReference* next(){return following;}
};
struct Group
{
    bool raid=true;
    GroupReference ref[3];
    bool isRaidGroup(){return raid;}
    GroupReference* GetFirstMember(){return ref;}
    bool IsAssistant(ObjectGuid){return false;}
};
namespace CthunPolicy
{
struct Scope
{
    int api=2; bool fresh=true;
    struct {RaidCombat::Snapshot raid;} snapshot;
    struct {RaidCombat::Plan raid;} plan;
    bool Fresh(unsigned){return fresh;}
};
}
struct Map
{
    struct Data
    {
        CthunPolicy::Scope* scope=nullptr;
        template<class T>T* Get(char const*){return scope;}
    } CustomData;
};
struct Unit
{
    bool world=true,alive=true,phase=true,friendly=true,controlled=false,gm=false,engaged=true;
    unsigned flags=0,entry=20062,state=0;
    Map* map=nullptr;
    ObjectGuid guid;
    bool IsInWorld(){return world;} bool IsAlive(){return alive;} Map* GetMap(){return map;}
    unsigned GetEntry(){return entry;} bool InSamePhase(Unit*){return phase;}
    bool HasUnitFlag(unsigned f){return flags&f;} bool HasUnitState(unsigned f){return state&f;}
    bool GetCharmerGUID(){return controlled;} bool IsGameMaster(){return gm;}
    bool IsFriendlyTo(Unit* p){return p->friendly;} ObjectGuid GetGUID(){return guid;}
};
struct Player : Unit
{
    Group* group=nullptr; bool tank=false,heal=false,bot=true; unsigned cls=1;
    Group* GetGroup(){return group;} unsigned getClass(){return cls;}
};
using GuidVector=std::vector<int>;
struct PlayerbotAI
{
    Player* bot=nullptr; Unit* current=nullptr; Unit* preferred=nullptr; GuidVector locks;
    static bool IsTank(Player* p){return p->tank;} static bool IsHeal(Player* p){return p->heal;}
    template<class T>T value();
};
template<> Unit* PlayerbotAI::value<Unit*>(){return current;}
template<> GuidVector PlayerbotAI::value<GuidVector>(){return locks;}
#define AI_VALUE(type,name) botAI->value<type>()
#define GET_PLAYERBOT_AI(player) ((player)->bot)
constexpr unsigned CLASS_WARLOCK=9,NPC_CAPERNIAN=20062;
unsigned getMSTime(){return 1;}
namespace ObjectAccessor
{
std::map<uint64_t,Unit*> units;
std::map<uint64_t,Player*> players;
Player* FindConnectedPlayer(ObjectGuid g){return players[g.value];}
Unit* GetUnit(Player&,ObjectGuid g){return units[g.value];}
}
namespace RaidCombat
{
bool Engaged(Player const&,Unit const& u){return u.engaged;}
Unit* PreferredTarget(PlayerbotAI& ai){return ai.preferred;}
}
// RESOLVER
namespace TempestKeepHelpers
{
// CAPERNIAN
}
struct KaelthasSunstriderAssignAdvisorDpsPriorityAction
{
    PlayerbotAI* botAI; Player* bot; Unit* attacked=nullptr; bool fallback=false;
    bool Attack(Unit* u){attacked=u;return true;}
    bool Execute()
    {
// DPS_PREFIX
    fallback=true;return false;
    }
};
int main()
{
    Map map,otherMap; Group group,otherGroup; CthunPolicy::Scope scope; map.CustomData.scope=&scope;
    Player human,warlock,bot;
    human.guid=ObjectGuid(1);human.bot=false;human.tank=true;
    warlock.guid=ObjectGuid(2);warlock.cls=CLASS_WARLOCK;bot.guid=ObjectGuid(3);
    for (auto* p:{&human,&warlock,&bot}) {p->map=&map;p->group=&group;}
    group.ref[0]={&human,&group.ref[1]};group.ref[1]={&warlock,&group.ref[2]};group.ref[2]={&bot,nullptr};
    Unit cap;cap.guid=ObjectGuid(100);cap.map=&map;
    ObjectAccessor::players[1]=&human;ObjectAccessor::units[100]=&cap;
    scope.snapshot.raid.count=1;scope.snapshot.raid.entityCount=1;
    scope.snapshot.raid.members[0].unit.guid=1;
    scope.snapshot.raid.entities[0].guid=100;scope.snapshot.raid.entities[0].entry=20062;
    auto& intent=scope.plan.raid.intents[0];intent.tankTargetCount=1;intent.tankTargets[0]=1;
    using RaidCombat::AssignedTank;
    assert(AssignedTank(&bot,20062)==&human);
    assert(TempestKeepHelpers::GetCapernianTank(&bot)==&human);
    scope.fresh=false;assert(!AssignedTank(&bot,20062));
    assert(TempestKeepHelpers::GetCapernianTank(&bot)==&warlock);scope.fresh=true;
    assert(!AssignedTank(&bot,999));
    for (int mode=0;mode<10;++mode)
    {
        human.alive=mode!=0;human.world=mode!=1;human.map=mode==2?&otherMap:&map;
        human.group=mode==3?&otherGroup:&group;human.tank=mode!=4;human.controlled=mode==5;
        human.gm=mode==6;human.friendly=mode!=7;human.phase=mode!=8;scope.api=mode==9?1:2;
        assert(!AssignedTank(&bot,20062));
    }
    human.alive=human.world=human.tank=human.friendly=human.phase=true;
    human.map=&map;human.group=&group;human.controlled=human.gm=false;scope.api=2;
    for(int mode=0;mode<8;++mode)
    {
        cap.alive=mode!=0;cap.world=mode!=1;cap.map=mode==2?&otherMap:&map;
        cap.entry=mode==3?99:20062;cap.phase=mode!=4;cap.flags=mode==5?UNIT_FLAG_NOT_SELECTABLE:0;
        cap.state=mode==6?UNIT_STATE_DIED:0;cap.engaged=mode!=7;
        assert(!AssignedTank(&bot,20062));
    }
    cap.alive=cap.world=cap.phase=cap.engaged=true;cap.map=&map;cap.entry=20062;cap.flags=cap.state=0;
    assert(AssignedTank(&bot,20062)==&human);
    intent.tankTargets[0]=2;assert(!AssignedTank(&bot,20062));intent.tankTargets[0]=1;
    assert(!AssignedTank(nullptr,20062));
    PlayerbotAI ai;ai.bot=&bot;ai.preferred=&cap;
    KaelthasSunstriderAssignAdvisorDpsPriorityAction action{&ai,&bot};
    assert(action.Execute() && action.attacked==&cap && !action.fallback);
    action.attacked=nullptr;ai.current=&cap;
    assert(!action.Execute() && !action.fallback && !action.attacked);
    ai.current=nullptr;ai.locks={42};
    assert(!action.Execute() && !action.fallback && !action.attacked);
    ai.locks.clear();ai.preferred=nullptr;action.Execute();assert(action.fallback);
    ai.preferred=&cap;bot.tank=true;action.fallback=false;action.Execute();assert(action.fallback);
    bot.tank=false;bot.heal=true;action.fallback=false;action.Execute();assert(action.fallback);
    std::cout<<"Fresh policy ownership, invalid-owner/target fallback, manual locks and native coexistence passed\n";
}
