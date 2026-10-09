#include <algorithm>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
using uint32=uint32_t;using int32=int32_t;
constexpr int ALLSPELLHOOK_ON_SPELL_CHECK_CAST=1,SPELLVALUE_MAX_TARGETS=0;
using SpellCastResult=int;
struct Map {bool raid=true;uint32 id=550,instance=1;};
struct Unit {bool creature=true;uint32 entry=19622;Map* map=nullptr;
 bool IsCreature()const{return creature;}uint32 GetEntry()const{return entry;}
 uint32 GetMapId()const{return map?map->id:0;}Map* GetMap()const{return map;}};
struct SpellInfo {uint32 Id=36797;};
struct SpellValue {uint32 MaxAffectedTargets=3;};
struct Spell {Unit* caster=nullptr;SpellInfo info;SpellValue value;unsigned writes=0;
 SpellInfo const* GetSpellInfo(){return &info;}Unit* GetCaster(){return caster;}
 SpellValue const* GetSpellValue(){return &value;}void SetSpellValue(int,uint32 v){value.MaxAffectedTargets=v;++writes;}};
struct Settings {uint32 originalPlayers=25,targetPlayers=10;};
struct Manager {bool enabled=true,scaled=true;Settings settings;
 bool Enabled()const{return enabled;}
 std::optional<Settings> GetSettings(Map* m)const{return scaled&&m&&m->raid&&m->instance?std::optional(settings):std::nullopt;}
} sRaidScalingMgr;
struct Config {int limit=1;template<class T>T GetOption(std::string const&,T,bool){return T(limit);}} config;
Config* sConfigMgr=&config;
struct AllSpellScript {AllSpellScript(char const*,std::initializer_list<int>){}
 virtual ~AllSpellScript()=default;virtual void OnSpellCheckCast(Spell*,bool,SpellCastResult&) {}};
// PRODUCTION
int main() {
 Map map;Unit unit;unit.map=&map;Spell spell;spell.caster=&unit;RaidScalingMindControlScript hook;int result=0;
 auto check=[&](uint32 expected){spell.value.MaxAffectedTargets=3;hook.OnSpellCheckCast(&spell,true,result);
    assert(spell.value.MaxAffectedTargets==expected);assert(result==0);};
 check(1);sRaidScalingMgr.settings.targetPlayers=1;check(1);
 sRaidScalingMgr.settings.targetPlayers=11;check(3);sRaidScalingMgr.settings.targetPlayers=25;check(3);
 sRaidScalingMgr.settings.targetPlayers=0;check(3);sRaidScalingMgr.settings.targetPlayers=10;
 config.limit=0;check(3);config.limit=2;check(2);config.limit=3;check(3);
 config.limit=-1;check(1);config.limit=4;check(1);config.limit=1;
 sRaidScalingMgr.enabled=false;check(3);sRaidScalingMgr.enabled=true;
 sRaidScalingMgr.scaled=false;check(3);sRaidScalingMgr.scaled=true;
 sRaidScalingMgr.settings.originalPlayers=40;check(3);sRaidScalingMgr.settings.originalPlayers=25;
 map.raid=false;check(3);map.raid=true;map.instance=0;check(3);map.instance=1;
 map.id=531;check(3);map.id=550;unit.entry=1;check(3);unit.entry=19622;
 unit.creature=false;check(3);unit.creature=true;spell.info.Id=1;check(3);spell.info.Id=36797;
 spell.caster=nullptr;check(3);spell.caster=&unit;hook.OnSpellCheckCast(nullptr,true,result);
 spell.value.MaxAffectedTargets=1;config.limit=3;hook.OnSpellCheckCast(&spell,false,result);
 assert(spell.value.MaxAffectedTargets==1); // never loosen an earlier cap
 config.limit=1;spell.value.MaxAffectedTargets=0;hook.OnSpellCheckCast(&spell,false,result);
 assert(spell.value.MaxAffectedTargets==1);
}
