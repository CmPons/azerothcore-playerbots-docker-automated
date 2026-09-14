/* Native API doubles for the exact production RaidCombatActions.cpp adapter. */
#include "RaidCombatPolicy.h"
#include "RaidCombatAdmission.h"
#include "CthunPolicyScope.h"
#include "RaidCombatState.h"
#include <array>
#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <iostream>
#include <vector>
using uint32 = uint32_t;
using uint64 = uint64_t;
constexpr unsigned MAX_SPELL_EFFECTS = 3;
enum { CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_AUTOREPEAT_SPELL };
enum { SPELL_EFFECT_INTERRUPT_CAST = 1, SPELL_EFFECT_DISPEL, SPELL_EFFECT_SCHOOL_DAMAGE, TRIGGERED_NONE = 0 };
static uint32 clockNow = 100;
uint32 getMSTime() { return clockNow; }
struct ObjectGuid
{
    uint64 id = 0;
    ObjectGuid() = default;
    explicit ObjectGuid(uint64 value) : id(value) { }
    uint64 GetRawValue() const { return id; }
    explicit operator bool() const { return id != 0; }
    bool IsEmpty() const { return id == 0; }
};
struct SpellInfo
{
    struct EffectData { unsigned Effect = 0; };
    uint32 Id = 1;
    bool positive = false, passive = false;
    std::array<EffectData, 3> Effects{{{SPELL_EFFECT_INTERRUPT_CAST}, {}, {}}};
    bool IsPositive() const { return positive; }
    bool IsPositiveEffect(unsigned) const { return positive; }
    bool IsPassive() const { return passive; }
    bool HasEffect(unsigned value) const
    {
        for (auto const& effect : Effects) if (effect.Effect == value) return true;
        return false;
    }
};
struct SpellManager
{
    SpellInfo info;
    SpellInfo const* GetSpellInfo(uint32 id) { return id == info.Id ? &info : nullptr; }
};
static SpellManager manager;
static SpellManager* sSpellMgr = &manager;
class Player;
class Spell;
class Unit
{
public:
    ObjectGuid guid{2};
    uint64 control = 1;
    bool alive = true, engaged = true, aura = true, sameMap = true, phase = true;
    Spell* current = nullptr;
    virtual ~Unit() = default;
    virtual Player* ToPlayer() { return nullptr; }
    uint64 GetControlIdentity() const { return control; }
    ObjectGuid GetGUID() const { return guid; }
    bool IsAlive() const { return alive; }
    bool IsInWorld() const { return alive; }
    bool IsInMap(Unit*) const { return sameMap; }
    bool InSamePhase(Unit*) const { return phase; }
    Spell* GetCurrentSpell(unsigned slot) { return slot == CURRENT_GENERIC_SPELL ? current : nullptr; }
    void* GetAuraApplication(uint32) { return aura ? this : nullptr; }
};
struct Cooldowns { bool busy = false; bool HasGlobalCooldown(SpellInfo const*) { return busy; } };
class Map
{
public:
    struct Data
    {
        CthunPolicy::Scope* scope = nullptr;
        template<class T> T* Get(char const*) { return static_cast<T*>(scope); }
    } CustomData;
};
class Player : public Unit
{
public:
    Map map;
    std::list<int> SpellQueue;
    Player* m_mover = this;
    Cooldowns cooldown;
    bool known = true, charmed = false, attackable = true, los = true, healer = false;
    std::function<void()> visibility;
    Player* ToPlayer() override { return this; }
    ObjectGuid GetCharmerGUID() const { return ObjectGuid(charmed); }
    ObjectGuid GetTransGUID() const { return {}; }
    void* GetVehicle() const { return nullptr; }
    void* GetGroup() const { return reinterpret_cast<void*>(1); }
    Map* GetMap() { return &map; }
    bool HasActiveSpell(uint32) { return known; }
    bool CanSeeOrDetect(Unit*) { if (visibility) visibility(); return true; }
    bool IsWithinLOSInMap(Unit*) { return los; }
    bool IsValidAttackTarget(Unit*) { return attackable; }
    uint32 GetSpellCooldownDelay(uint32) { return 0; }
    Cooldowns& GetGlobalCooldownMgr() { return cooldown; }
};
using GuidVector = std::vector<ObjectGuid>;
struct PriorityValue { GuidVector value; GuidVector& Get() { return value; } };
struct Context
{
    PriorityValue priority;
    template<class T> PriorityValue* GetValue(char const*) { return &priority; }
};
class PlayerbotAI
{
public:
    Context context;
    Context* GetAiObjectContext() { return &context; }
    Player bot;
    RaidCombat::State raidCombat;
    bool eligible = true;
    Player* GetBot() { return &bot; }
    static bool IsHeal(Player* player) { return player->healer; }
};
static std::map<uint64, Unit*> units;
namespace ObjectAccessor
{
Unit* GetUnit(Player&, ObjectGuid guid) { auto it = units.find(guid.id); return it == units.end() ? nullptr : it->second; }
}
struct SpellCastTargets { Unit* target = nullptr; void SetUnitTarget(Unit* unit) { target = unit; } };
struct TargetInfo { ObjectGuid targetGUID; unsigned effectMask = 1; };
static unsigned admitted = 0, effects = 0;
class Spell
{
public:
    Spell(Unit* caster, SpellInfo const* info, unsigned) : caster(caster), info(info) { }
    Unit* caster;
    SpellInfo const* info;
    bool (*check)(Spell const&) = nullptr;
    std::list<TargetInfo> targets;
    uint64 id = 7;
    uint64 GetCastIdentity() const { return id; }
    Unit* GetCaster() const { return caster; }
    SpellInfo const* GetSpellInfo() const { return info; }
    auto const& GetCollectedCombatTargets() const { return targets; }
    void SetCombatAdmissionCheck(bool (*value)(Spell const&)) { check = value; }
    void prepare(SpellCastTargets* value)
    {
        ++admitted;
        targets.push_back({value->target->GetGUID(), 1});
        if (!check || check(*this)) ++effects;
        delete this;
    }
};
class CurrentTargetValue
{
public:
    PlayerbotAI* botAI;
    Player* bot;
    ObjectGuid selection;
    Unit* Get();
};
namespace RaidCombat
{
bool Eligible(PlayerbotAI& ai) { return ai.eligible && !ai.bot.charmed && ai.bot.m_mover == &ai.bot; }
bool Engaged(Player const&, Unit const& target) { return target.engaged; }
}
