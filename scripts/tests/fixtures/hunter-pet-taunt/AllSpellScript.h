// Minimal core API doubles for compiling the complete production spell hook.
#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using uint32 = uint32_t;
using uint16 = uint16_t;
enum { ALLSPELLHOOK_ON_SPELL_CHECK_CAST, ALLSPELLHOOK_CAN_PREPARE };
enum SpellCastResult : uint8_t { SPELL_CAST_OK, SPELL_FAILED_DONT_REPORT, SPELL_FAILED_LINE_OF_SIGHT };
enum { HUNTER_PET, SUMMON_PET, CLASS_HUNTER, CLASS_WARLOCK, SPELL_EFFECT_ATTACK_ME, SPELL_AURA_MOD_TAUNT };
class Spell;
class SpellCastTargets;
class AuraEffect {};
class AllSpellScript
{
public:
    inline static AllSpellScript* registered = nullptr;
    std::string name;
    std::vector<uint16> hooks;
    AllSpellScript(char const* name, std::vector<uint16> hooks) : name(name), hooks(hooks) { registered = this; }
    virtual ~AllSpellScript() = default;
    virtual void OnSpellCheckCast(Spell*, bool, SpellCastResult&) {}
    virtual bool CanPrepare(Spell*, SpellCastTargets const*, AuraEffect const*) { return true; }
};
class Group {};
class Unit;
class Player;
class Pet;
class ThreatManager
{
public:
    bool enabled = true;
    Unit* victim = nullptr;
    bool CanHaveThreatList() const { return enabled; }
    Unit* GetCurrentVictim() const { return victim; }
};
class Unit
{
public:
    virtual ~Unit() = default;
    Unit* victim = nullptr;
    ThreatManager threat;
    bool alive = true, world = true;
    virtual bool IsPet() const { return false; }
    virtual Pet* ToPet() { return nullptr; }
    virtual Player* ToPlayer() { return nullptr; }
    Unit* GetVictim() const { return victim; }
    ThreatManager& GetThreatMgr() { return threat; }
    bool IsAlive() const { return alive; }
    bool IsInWorld() const { return world; }
};
class Player : public Unit
{
public:
    Group* group = nullptr;
    int playerClass = CLASS_HUNTER;
    bool tankSpec = false;
    bool bot = true;
    bool runtimeTankStrategy = false;
    Player* ToPlayer() override { return this; }
    Group* GetGroup() const { return group; }
    bool IsClass(int value) const { return playerClass == value; }
};
class Pet : public Unit
{
public:
    Player* owner = nullptr;
    int type = HUNTER_PET;
    bool IsPet() const override { return true; }
    Pet* ToPet() override { return this; }
    int getPetType() const { return type; }
    Player* GetOwner() const { return owner; }
};
class PlayerbotAI
{
public:
    static bool IsTank(Player* player, bool bySpec = false)
    {
        assert(bySpec); // Must not depend on the victim's current bot strategy.
        return player->tankSpec;
    }
};
class SpellInfo
{
public:
    uint32 Id = 2649;
    SpellInfo const* firstRank = nullptr;
    bool tauntEffect = false, tauntAura = false;
    SpellInfo const* GetFirstRankSpell() const { return firstRank ? firstRank : this; }
    bool HasEffect(int) const { return tauntEffect; }
    bool HasAura(int) const { return tauntAura; }
};
class SpellCastTargets
{
public:
    Unit* unit = nullptr;
    Unit* GetUnitTarget() const { return unit; }
};
class Spell
{
public:
    Unit* caster = nullptr;
    SpellInfo const* info = nullptr;
    SpellCastTargets m_targets;
    Unit* GetCaster() const { return caster; }
    SpellInfo const* GetSpellInfo() const { return info; }
};
