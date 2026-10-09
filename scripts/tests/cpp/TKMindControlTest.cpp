// Production MC helpers/actions with explicit game-service doubles. No worldserver is started.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <set>
#include <tuple>
#include <string>
#include <vector>
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t; using int32 = int32_t;
using ObjectGuid = uint32;
constexpr uint8 EQUIPMENT_SLOT_MAINHAND=15, EQUIPMENT_SLOT_OFFHAND=16, INVENTORY_SLOT_BAG_0=255, NULL_SLOT=255;
constexpr int CLASS_WARRIOR=1, CLASS_PALADIN=2, CLASS_HUNTER=3, CLASS_ROGUE=4, CLASS_SHAMAN=7;
constexpr int ROGUE_TAB_COMBAT=1, SHAMAN_TAB_ENHANCEMENT=1, WARRIOR_TAB_ARMS=0;
constexpr int TEMPEST_KEEP_MAP_ID=550, ITEM_INFINITY_BLADE=30312, SPELL_KAELTHAS_MIND_CONTROL=36797;
constexpr int NPC_NETHERSTRAND_LONGBOW=21268, NPC_COSMIC_INFUSER=21270, NPC_DEVASTATION=21269;
constexpr int NPC_INFINITY_BLADES=21271, NPC_WARP_SLICER=21272, NPC_STAFF_OF_DISINTEGRATION=21274;
constexpr int NPC_PHASESHIFT_BULWARK=21273, ITEM_NETHERSTRAND_LONGBOW=30318, ITEM_COSMIC_INFUSER=30316;
constexpr int ITEM_DEVASTATION=30317, ITEM_WARP_SLICER=30311, ITEM_STAFF_OF_DISINTEGRATION=30313;
constexpr int ITEM_PHASESHIFT_BULWARK=30314, UNIT_STATE_LOST_CONTROL=1, EQUIP_ERR_OK=0;
constexpr float INTERACTION_DISTANCE=5.0f;
constexpr int CMSG_AUTOSTORE_LOOT_ITEM=1;
struct Event {};
struct Map {uint32 id=550;};
struct Aura {int duration=25000; int GetDuration() const {return duration;}};
struct Unit {virtual ~Unit()=default; bool alive=true, world=true, hostile=true; Map* map=nullptr;
    uint32 guid=10, auraCaster=19622; bool mc=false; float distance=2;
    std::string name="member"; Aura aura;
    bool IsAlive() const {return alive;} bool IsInWorld() const {return world;}
    Map* GetMap() const {return map;} uint32 GetMapId() const {return map ? map->id : 0;}
    ObjectGuid GetGUID() const {return guid;} std::string GetName() const {return name;}
    Unit* GetVictim() const {return nullptr;}
    bool HasAura(uint32 id, ObjectGuid caster=0) const {return id==36797 && mc && (!caster || caster==auraCaster);}
    Aura* GetAura(uint32 id) {return HasAura(id) ? &aura : nullptr;}
    float GetPositionX() const {return distance;} float GetPositionY() const {return 0;}
    float GetPositionZ() const {return 0;}
};
struct Creature : Unit {bool combat=true; Unit* victim=nullptr;
    bool IsInCombat() const {return combat;} Unit* GetVictim() const {return victim;}};
struct Item {uint32 entry=30312; uint16 pos=uint16((255<<8)|23); bool broken=false;
    uint32 GetEntry() const {return entry;} bool IsBroken() const {return broken;} uint16 GetPos() const {return pos;}};
struct Player;
struct GroupReference {Player* player=nullptr; GroupReference* following=nullptr;
    Player* GetSource() const {return player;} GroupReference* next() const {return following;}};
struct Group {GroupReference* first=nullptr; GroupReference* GetFirstMember() const {return first;}};
struct WorldPacket {WorldPacket(int,int) {} template<class T> WorldPacket& operator<<(T const&) {return *this;}};
struct Session {unsigned queued=0; void QueuePacket(WorldPacket* p) {++queued; delete p;}};
struct Player : Unit {
    int cls=CLASS_ROGUE, tab=1; bool controlled=false, bankBlade=false, equipAllowed=true, swapAllowed=true;
    bool lootAllowed=true, bowPresent=true; Group* group=nullptr; Creature* boss=nullptr; Creature* corpse=nullptr;
    std::map<uint16,Item*> items; Session session; unsigned swaps=0;
    int getClass() const {return cls;} bool HasUnitState(int) const {return controlled;}
    Group* GetGroup() const {return group;}
    bool IsValidAttackTarget(Unit* t) const {return t->hostile;}
    float GetExactDist2d(Unit* t) const {return t->distance;}
    bool IsWithinMeleeRange(Unit* t) const {return t->distance <= 5;}
    float GetDistance(Unit* t) const {return t->distance;}
    Creature* FindNearestCreature(uint32 entry,float,bool living) const {
        if(entry==NPC_NETHERSTRAND_LONGBOW && !bowPresent)return nullptr;
        return entry==19622 && living ? boss : corpse;
    }
    Item* GetItemByPos(uint8 bag,uint8 slot) const {auto i=items.find(uint16((bag<<8)|slot)); return i==items.end()?nullptr:i->second;}
    Item* GetItemByEntry(uint32 entry) const {for(auto [p,i]:items) if(i && i->entry==entry) return i; return nullptr;}
    bool HasItemCount(uint32 id,uint32,bool bank) const {return GetItemByEntry(id) || (id==30312 && bank && bankBlade);}
    int CanEquipItem(uint8 slot,uint16& dst,Item*,bool) const {dst=uint16((255<<8)|slot); return equipAllowed?0:1;}
    void SwapItem(uint16 src,uint16 dst) {++swaps;if(!swapAllowed)return;std::swap(items[src],items[dst]);
        if(items[src])items[src]->pos=src;
        if(items[dst])items[dst]->pos=dst;
    }
    bool HasSpell(uint32 id) const {return id!=0;}
    Session* GetSession() {return &session;} void SetLootGUID(ObjectGuid) {}
};
namespace AiFactory {int GetPlayerSpecTab(Player* p) {return p->tab;}}
struct LootObject {Player* bot=nullptr; ObjectGuid guid=0; LootObject()=default; LootObject(Player* p,ObjectGuid g):bot(p),guid(g){}
    bool IsLootPossible(Player*) const {return bot->lootAllowed;}};
struct Action;
template<class T> struct Value {T data{}; T Get() const{return data;} void Set(T v){data=v;}};
struct Context {Value<LootObject> loot;Value<uint32> spell;Action* equip=nullptr;
    template<class T> Value<T>* GetValue(std::string const&,std::string const& = "") {
        if constexpr(std::is_same_v<T,uint32>)return &spell;else return &loot;}
    Action* GetAction(std::string const&) {return equip;}
};
struct PlayerbotAI {Player* bot;Context context;bool canCast=true, castResult=true, removeMC=true;
    unsigned casts=0,moves=0;std::string lastSpell;Unit* legacyBoss=nullptr;
    explicit PlayerbotAI(Player* p):bot(p){}
    Player* GetBot() const{return bot;}Context* GetAiObjectContext(){return &context;}
    bool CanCastSpell(char const*,Player*) const{return canCast;}
    bool CastSpell(char const* name,Player* target){++casts;lastSpell=name;if(castResult&&removeMC)target->mc=false;return castResult;}
    bool IsTank(Player*) const{return false;}
};
struct Action {PlayerbotAI* botAI;Player* bot;Context* context;
    Action(PlayerbotAI* ai,std::string const&):botAI(ai),bot(ai->bot),context(&ai->context){}
    virtual ~Action()=default;virtual bool Execute(Event){return false;}};
struct MovementAction : Action {using Action::Action;template<class... T> bool MoveTo(T...){++botAI->moves;return true;}};
struct AttackAction : MovementAction {using MovementAction::MovementAction;};
using ItemIds=std::set<uint32>;
struct EquipAction : Action {using Action::Action;void EquipItems(ItemIds const&) {}};
struct Trigger {PlayerbotAI* botAI;Player* bot;Trigger(PlayerbotAI* a,std::string const&):botAI(a),bot(a->bot){}
    virtual bool IsActive(){return false;}};
namespace MovementPriority {constexpr int MOVEMENT_COMBAT=1;}
namespace ObjectAccessor {Player* resolved=nullptr;Player* FindPlayer(ObjectGuid g){return resolved&&resolved->guid==g?resolved:nullptr;}}
template<class... T> void Log(T const&...) {}
#define LOG_INFO(...) Log(__VA_ARGS__)
#define AI_VALUE2(type,key,qualifier) (botAI->legacyBoss)
struct Config {float lootDistance=15;} config;
Config sPlayerbotAIConfig;
struct OpenLootAction {PlayerbotAI* ai;explicit OpenLootAction(PlayerbotAI* a):ai(a){}
    bool Execute(Event){return ai->bot->corpse && ai->bot->GetDistance(ai->bot->corpse)<=3;}};
// CLASSES
bool KaelthasSunstriderLootLegendaryWeaponsAction::ShouldBotLootWeapon(uint32 entry)
{return entry==NPC_NETHERSTRAND_LONGBOW || entry==NPC_INFINITY_BLADES;}
// HELPERS
using namespace TempestKeepHelpers;
// METHODS
struct Scene {
    Map map,other;Player bot,member;Creature boss,corpse;Item dagger,oldWeapon;
    GroupReference a,b;Group group;PlayerbotAI ai;KaelthasSunstriderBreakMindControlAction rescue;
    KaelthasSunstriderLootLegendaryWeaponsAction loot;KaelthasSunstriderRaidMemberIsMindControlledTrigger trigger;
    Scene():ai{&bot},rescue(&ai),loot(&ai),trigger(&ai) {
        bot.map=member.map=boss.map=corpse.map=&map;boss.guid=19622;bot.guid=1;member.guid=2;
        bot.boss=&boss;bot.corpse=&corpse;corpse.alive=false;corpse.distance=8;
        member.mc=true;member.name="healer";a.player=&bot;a.following=&b;b.player=&member;group.first=&a;bot.group=&group;
        bot.items[dagger.pos]=&dagger;oldWeapon.entry=999;oldWeapon.pos=uint16((255<<8)|16);bot.items[oldWeapon.pos]=&oldWeapon;
        ai.context.spell.data=5938;ObjectAccessor::resolved=&member;ai.legacyBoss=&boss;
    }
};
void cases(std::string const& scenario) {
    if(scenario=="loot" || scenario=="all") {
        Scene s;s.bot.items.clear();s.corpse.distance=8;
        // Failed early bow opportunity must not strand later dagger acquisition. Shared corpse double
        // also proves the actual LootWeapon approach obeys OpenLootAction's smaller radius.
        assert(s.loot.Execute({}));assert(s.ai.moves==1);assert(s.bot.session.queued==0);
        s.corpse.distance=2;s.bot.bowPresent=false;
        assert(s.loot.Execute({}));assert(s.bot.session.queued==1);
        s.bot.lootAllowed=false;assert(!s.loot.Execute({}));
    }
    if(scenario=="hand" || scenario=="all") {
        Scene s;s.bot.equipAllowed=false;assert(!s.rescue.Execute({}));assert(s.ai.casts==0);
        s.bot.equipAllowed=true;s.bot.swapAllowed=false;assert(!s.rescue.Execute({}));assert(s.ai.casts==0);
        s.bot.swapAllowed=true;assert(s.rescue.Execute({}));assert(HasReadyInfinityBlade(&s.ai));assert(s.ai.casts==0);
        assert(s.bot.GetItemByEntry(999)); // earned weapon preserved, not deleted
        assert(s.rescue.Execute({}));assert(s.ai.lastSpell=="shiv" && !s.member.mc);
        assert(!s.trigger.IsActive());assert(!s.rescue.Execute({})); // no attacks after release
    }
    if(scenario=="discovery" || scenario=="all") {
        Scene s;s.ai.legacyBoss=nullptr;assert(s.trigger.IsActive());
        s.boss.victim=&s.bot;assert(!s.trigger.IsActive());s.boss.victim=nullptr;
        s.boss.combat=false;assert(!s.trigger.IsActive());s.boss.combat=true;
        s.member.auraCaster=123;assert(!s.trigger.IsActive());s.member.auraCaster=19622;
        s.member.alive=false;assert(!s.trigger.IsActive());s.member.alive=true;
        s.member.hostile=false;assert(!s.trigger.IsActive());s.member.hostile=true;
        s.member.map=&s.other;assert(!s.trigger.IsActive());s.member.map=&s.map;
        s.member.world=false;assert(!s.trigger.IsActive());s.member.world=true;
        s.bot.controlled=true;assert(!s.trigger.IsActive());s.bot.controlled=false;
        s.bot.mc=true;assert(!s.trigger.IsActive());s.bot.mc=false;
        s.map.id=1;assert(!s.trigger.IsActive());s.map.id=550;
        s.bot.items.clear();s.bot.bankBlade=true;assert(!s.trigger.IsActive());
    }
    if(scenario=="all") {
        for(auto [cls,tab,hand,spell] : std::vector<std::tuple<int,int,int,std::string>>{
            {CLASS_ROGUE,1,16,"shiv"},{CLASS_ROGUE,0,15,"sinister strike"},{CLASS_ROGUE,2,15,"sinister strike"},
            {CLASS_HUNTER,0,15,"wing clip"},{CLASS_SHAMAN,1,15,"stormstrike"},{CLASS_WARRIOR,2,15,"hamstring"}}) {
            Scene s;s.bot.cls=cls;s.bot.tab=tab;assert(InfinityBladeSlot(&s.ai)==hand);
            assert(s.rescue.Execute({}));assert(HasReadyInfinityBlade(&s.ai));s.member.distance=20;
            assert(s.rescue.Execute({}));assert(s.ai.moves==1 && s.ai.casts==0);
            s.member.distance=2;s.ai.canCast=false;assert(!s.rescue.Execute({}));assert(s.ai.casts==0);
            s.ai.canCast=true;s.ai.castResult=false;assert(!s.rescue.Execute({}));assert(s.member.mc);
            s.ai.castResult=true;s.ai.removeMC=false;assert(s.rescue.Execute({}));assert(s.member.mc); // no forced dispel
            s.ai.removeMC=true;assert(s.rescue.Execute({}));assert(!s.member.mc && s.ai.lastSpell==spell);
        }
        for(auto [cls,tab]:std::vector<std::pair<int,int>>{{CLASS_PALADIN,1},{CLASS_SHAMAN,2},{CLASS_WARRIOR,0}}) {
            Scene s;s.bot.cls=cls;s.bot.tab=tab;assert(!s.trigger.IsActive());assert(!s.rescue.Execute({}));
        }
        Scene s;s.dagger.broken=true;assert(!EquipInfinityBlade(&s.ai));assert(!s.rescue.Execute({}));
        s.dagger.broken=false;assert(EquipInfinityBlade(&s.ai));
        auto status=DescribeKaelthasMindControl(&s.ai);
        assert(status.find("blade_owned=1 ready=1")!=std::string::npos);
        assert(status.find("mc=healer:25000ms")!=std::string::npos);
        assert(status.find("rescue_target=healer")!=std::string::npos);
        // The actual class hierarchy must not label this support operation as a boss attack.
        Action* action=&s.rescue;assert(dynamic_cast<AttackAction*>(action)==nullptr);
    }
}
int main(int argc,char** argv){cases(argc>1?argv[1]:"all");std::cout<<"PASS production MC support\n";}
