// Actual equipped-hand dispatch, built-in item proc prefix and EffectDispel.
// Aura storage, packets, spell lookup and RNG are controlled game-service doubles.
#include <array>
#include <cassert>
#include <cstdint>
#include <list>
#include <utility>
using uint8=uint8_t;using uint32=uint32_t;using int32=int32_t;
using ObjectGuid=uint32;using SpellEffIndex=int;using DispelType=int;using TriggerCastFlags=uint32;
enum WeaponAttackType {BASE_ATTACK,OFF_ATTACK,RANGED_ATTACK};
enum EquipmentSlots {EQUIPMENT_SLOT_START=0,EQUIPMENT_SLOT_MAINHAND=15,EQUIPMENT_SLOT_OFFHAND=16,
    EQUIPMENT_SLOT_RANGED=17,EQUIPMENT_SLOT_END=19};
constexpr int INVENTORY_SLOT_BAG_0=255,FORM_GHOSTWOLF=16,ITEM_CLASS_WEAPON=2,MAX_ITEM_SPELLS=5;
constexpr int ITEM_SPELLTRIGGER_CHANCE_ON_HIT=2,PROC_FLAG_TAKEN_DAMAGE=1;
constexpr uint32 TRIGGERED_FULL_MASK=0xffff,TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD=2;
constexpr int SPELL_EFFECT_HANDLE_HIT_TARGET=1,SMSG_DISPEL_FAILED=1,SMSG_SPELLDISPELLOG=2;
constexpr int SPELLFAMILY_WARLOCK=5,SPELLCATEGORY_DEVOUR_MAGIC=12,EFFECT_1=1;
struct SpellEffect {int MiscValue=8;int CalcValue()const{return 0;}};
struct SpellInfo {uint32 Id=36478,ProcChance=101,SpellFamilyName=0;std::array<SpellEffect,3> Effects;
 static uint32 GetDispelMask(DispelType t){return 1u<<t;}int GetCategory()const{return 0;}};
struct _Spell {uint32 SpellId=0,SpellTrigger=2;float SpellPPMRate=0;};
struct ItemTemplate {int Class=ITEM_CLASS_WEAPON;std::array<_Spell,5> Spells;};
struct Item {ItemTemplate proto;bool broken=false;bool IsBroken()const{return broken;}
 ItemTemplate const* GetTemplate()const{return &proto;}};
struct WorldPacket {WorldPacket(int,int){} template<class T> WorldPacket& operator<<(T const&){return *this;}};
struct Unit;
struct Aura {uint32 id=36797;bool active=true;int resistanceChance=100;
 int CalcDispelChance(Unit*,bool)const{return resistanceChance;}
 uint32 GetId()const{return id;}ObjectGuid GetCasterGUID()const{return 19622;}};
using DispelChargesList=std::list<std::pair<Aura*,int>>;
struct ThreatMgr {void ForwardThreatForAssistingMe(Unit*,float,SpellInfo const*){}};
struct Unit {bool alive=true;Aura mc;ThreatMgr threat;unsigned removals=0;
 bool IsAlive()const{return alive;}bool IsFriendlyTo(Unit*)const{return false;}
 ObjectGuid GetGUID()const{return 2;}ObjectGuid GetPackGUID()const{return 2;}
 ThreatMgr& GetThreatMgr(){return threat;}
 void GetDispellableAuraList(Unit*,uint32 mask,DispelChargesList& list,SpellInfo const*) {
   if(mc.active && (mask&(1u<<8)))list.push_back({&mc,1});
 }
 void RemoveAurasDueToSpellByDispel(uint32 id,uint32 spell,ObjectGuid caster,Unit*,int count) {
   assert(id==36797&&spell==36478&&caster==19622&&count==1);mc.active=false;++removals;
 }
 void SendMessageToSet(WorldPacket*,bool){}
 Unit* GetOwner(){return nullptr;}Aura* GetAura(uint32){return nullptr;}
 void CastCustomSpell(Unit*,uint32,int32*,void*,void*,bool){}
};
struct Spell {Unit* m_caster;Unit* unitTarget;SpellInfo const* m_spellInfo;int damage=1;
 int effectHandleMode=SPELL_EFFECT_HANDLE_HIT_TARGET;void EffectDispel(SpellEffIndex);};
struct Player : Unit {std::array<Item*,19> items{};bool usable=true;int form=0;unsigned submitted=0;
 bool CanUseAttackType(WeaponAttackType)const{return usable;}int GetShapeshiftForm()const{return form;}
 Item* GetItemByPos(int,uint8 slot){return items[slot];}
 uint32 GetAttackTime(WeaponAttackType)const{return 1500;}
 float GetPPMProcChance(uint32 speed,float ppm,SpellInfo const*)const{return speed*ppm/600.0f;}
 float GetWeaponProcChance()const{return 20;}
 void CastSpell(Unit*,uint32,TriggerCastFlags,Item*);
 void CastItemCombatSpell(Unit*,WeaponAttackType,uint32,uint32);
 void CastItemCombatSpell(Unit*,WeaponAttackType,uint32,uint32,Item*,ItemTemplate const*);
};
struct ScriptMgr {bool allow=true;template<class... T>bool OnPlayerCanCastItemCombatSpell(T...){return allow;}
 template<class... T>bool OnCastItemCombatSpell(T...){return allow;}} scripts;
ScriptMgr* sScriptMgr=&scripts;
struct SpellMgr {SpellInfo info;SpellInfo const* GetSpellInfo(uint32 id){return id==36478?&info:nullptr;}} spells;
SpellMgr* sSpellMgr=&spells;
bool procRoll=true,dispelRoll=true;
bool roll_chance_f(float){return procRoll;}bool roll_chance_i(int){return dispelRoll;}
uint32 urand(uint32 low,uint32){return low;}
#define LOG_ERROR(...) do {} while(false)
// PRODUCTION
void Player::CastSpell(Unit* target,uint32 id,TriggerCastFlags,Item*) {
 assert(id==36478);++submitted;Spell effect{this,target,&spells.info};effect.EffectDispel(0);
}
int main() {
 Player bot;Unit friendUnit;Item dagger;dagger.proto.Spells[1].SpellId=36478;
 dagger.proto.Spells[1].SpellPPMRate=60;
 bot.items[16]=&dagger;
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);
 assert(friendUnit.mc.active&&bot.submitted==0); // wrong hand cannot trigger the dagger
 procRoll=false;bot.CastItemCombatSpell(&friendUnit,OFF_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);
 assert(friendUnit.mc.active&&bot.submitted==0); // do not grant guaranteed procs
 procRoll=true;dispelRoll=false;bot.CastItemCombatSpell(&friendUnit,OFF_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);
 assert(friendUnit.mc.active&&bot.submitted==1);
 dispelRoll=true;bot.CastItemCombatSpell(&friendUnit,OFF_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);
 assert(!friendUnit.mc.active&&friendUnit.removals==1); // native EffectDispel removes spell36797
 friendUnit.mc.active=true;bot.items[16]=nullptr;bot.items[15]=&dagger;
 // Spell.cpp supplies TAKEN_DAMAGE for successful non-damaging melee abilities too (Wing Clip).
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);
 assert(!friendUnit.mc.active&&friendUnit.removals==2);
 friendUnit.mc.active=true;dagger.broken=true;
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);assert(friendUnit.mc.active);
 dagger.broken=false;bot.usable=false;
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);assert(friendUnit.mc.active);
 bot.usable=true;scripts.allow=false;
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);assert(friendUnit.mc.active);
 scripts.allow=true;friendUnit.mc.resistanceChance=0;
 bot.CastItemCombatSpell(&friendUnit,BASE_ATTACK,PROC_FLAG_TAKEN_DAMAGE,0);assert(friendUnit.mc.active);
}
