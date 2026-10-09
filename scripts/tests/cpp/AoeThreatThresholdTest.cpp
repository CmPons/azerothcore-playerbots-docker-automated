// Complete production ThreatMultiplier body; bounded context/config/encounter helper doubles.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include "RaidThreatControl.h"
struct Map {};
Map raid;
using uint8 = uint8_t;
using uint32 = uint32_t;
struct Unit
{
    bool hostile = false, alive = true, inWorld = true;
    float health = 100;
    Map* map = &raid;
    bool IsAlive() const { return alive; }
    bool IsInWorld() const { return inWorld; }
    Map* GetMap() const { return map; }
    float GetHealthPct() const { return health; }
};
struct Player : Unit
{
    bool IsValidAttackTarget(Unit* target) { return target && target->hostile; }
    bool IsFriendlyTo(Unit* target) { return target && !target->hostile; }
};
struct PlayerbotAI {};
struct Action
{
    enum class ActionThreatType { None, Single, Aoe };
    virtual ~Action() = default;
    std::string getName() const { return "test action"; }
    virtual ActionThreatType getThreatType() { return ActionThreatType::None; }
};
struct CastSpellAction : Action
{
    Unit* target = nullptr;
    Unit* GetTarget() { return target; }
    ActionThreatType getThreatType() override { return ActionThreatType::Single; }
};
struct CastHealingSpellAction : CastSpellAction
{
    // HEAL_THREAT_TYPE
};
struct AttackAction : Action {};
struct PetAttackAction : Action {};
struct AoeDamage : CastSpellAction
{
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};
struct State
{
    bool neglect = false, group = true, twinsActive = false, twinsBoss = false, bossHold = false;
    uint8 aoe = 0, current = 0, bossLimitSeen = 0;
    Unit enemy{true};
    unsigned stopped = 0;
    template<class T> T Value(std::string const& name)
    {
        if constexpr (std::is_same_v<T, bool>)
            return name == "neglect threat" ? neglect : group;
        else if constexpr (std::is_same_v<T, Unit*>)
            return &enemy;
        else
            return name == "aoe" ? aoe : current;
    }
} state;
#define AI_VALUE(type, name) state.Value<type>(name)
#define AI_VALUE2(type, name, qualifier) state.Value<type>(qualifier)
struct Config
{
    bool discipline = true;
    template<class T> T GetOption(char const*, T fallback)
    {
        if constexpr (std::is_same_v<T, bool>) return discipline;
        return fallback;
    }
} config;
auto* sConfigMgr = &config;
namespace TempleOfAhnQirajHelpers
{
    bool IsTwinsEncounterActive(Player*) { return state.twinsActive; }
    bool IsTwinsBossTarget(Player*, Unit*) { return state.twinsBoss; }
}
namespace ai::threat
{
    bool ShouldHoldDamageOnTauntImmuneBoss(PlayerbotAI*, Unit*, uint8 limit)
    {
        assert(limit >= 1 && limit <= 100);
        state.bossLimitSeen = limit;
        return state.bossHold;
    }
    void StopDirectDamage(PlayerbotAI*, Unit*) { ++state.stopped; }
}
namespace ai::threat::control
{
    Settings settings;
    std::optional<Decision> last;
    Settings GetSettings(Map const*) { return settings; }
    void Record(Player*, Unit* target, std::string const& action, Reason reason,
        Settings const& policy, int aoe, int current)
    {
        last = Decision{};
        last->action = action;
        last->health = target ? target->health : 0;
        last->reason = reason;
        last->settings = policy;
        last->aoe = aoe;
        last->target = current;
    }
}
struct ThreatMultiplier
{
    Player player;
    PlayerbotAI ai;
    Player* bot = &player;
    PlayerbotAI* botAI = &ai;
    float GetValue(Action*);
};
// PRODUCTION
int main(int argc, char** argv)
{
    assert(argc == 2);
    using namespace ai::threat::control;
    settings.healing = HealingMode::Normal;
    std::string scenario = argv[1];
    ThreatMultiplier multiplier;
    CastHealingSpellAction heal;
    AoeDamage damage;
    CastSpellAction single;
    AttackAction attack;
    PetAttackAction pet;
    Action neutral;
    Unit friendly;
    heal.target = &friendly;
    single.target = &state.enemy;
    if (scenario == "boundaries")
    {
        for (unsigned aoe = 0; aoe <= 255; ++aoe)
            for (unsigned current = 0; current <= 255; ++current)
            {
                state.aoe = aoe;
                state.current = current;
                float expected = aoe >= 90 || current >= 80 ? 0.0f : 1.0f;
                assert(multiplier.GetValue(&heal) == expected);
                assert(multiplier.GetValue(&damage) == expected);
                assert(multiplier.GetValue(&single) == (current >= 80 ? 0.0f : 1.0f));
                assert(multiplier.GetValue(&attack) == (current >= 80 ? 0.0f : 1.0f));
                assert(multiplier.GetValue(&pet) == (current >= 80 ? 0.0f : 1.0f));
                assert(multiplier.GetValue(&neutral) == 1.0f);
            }
    }
    else if (scenario == "limitations")
    {
        // Recorded inputs: Keilmere's veto lifts; Meliah and Ailina still encounter other limits.
        state.aoe = state.current = 73;
        assert(multiplier.GetValue(&heal) == 1.0f);
        state.aoe = state.current = 80;
        assert(multiplier.GetValue(&heal) == 0.0f);
        state.aoe = 136;
        state.current = 9;
        assert(multiplier.GetValue(&heal) == 0.0f);
        assert(multiplier.GetValue(&single) == 1.0f); // Wanding can still be allowed when heals are held.
    }
    else if (scenario == "guards")
    {
        state.aoe = state.current = 255;
        assert(multiplier.GetValue(nullptr) == 1.0f);
        state.neglect = true;
        assert(multiplier.GetValue(&heal) == 1.0f);
        state.neglect = false;
        state.group = false;
        assert(multiplier.GetValue(&heal) == 1.0f);
        state.group = true;
        state.aoe = state.current = 0;
        state.bossHold = true;
        assert(multiplier.GetValue(&heal) == 0.0f);
        assert(state.stopped == 1);
        config.discipline = false;
        assert(multiplier.GetValue(&heal) == 1.0f);
        state.twinsBoss = true;
        assert(multiplier.GetValue(&heal) == 0.0f); // Twins boss gate independent of generic config.
        state.twinsActive = true;
        assert(multiplier.GetValue(&heal) == 1.0f);
        single.target = &friendly;
        assert(multiplier.GetValue(&single) == 1.0f);
        single.target = &state.enemy;
        assert(multiplier.GetValue(&single) == 0.0f);
        state.bossHold = false;
        state.aoe = state.current = 255;
        assert(multiplier.GetValue(&single) == 1.0f); // Twins owner check bypasses generic gates as before.
    }
    else if (scenario == "control")
    {
        settings.healing = HealingMode::Emergency;
        settings.boss = 85;
        damage.target = single.target = &friendly; // A friendly target alone does not make an action a heal.
        friendly.health = 29.9f;
        state.aoe = state.current = 255;
        state.bossHold = true;
        assert(multiplier.GetValue(&heal) == 1.0f);
        assert(last->reason == Reason::Emergency && state.stopped == 0);
        assert(multiplier.GetValue(&damage) == 0.0f && state.stopped == 1);
        assert(state.bossLimitSeen == 85);
        assert(multiplier.GetValue(&single) == 0.0f);
        friendly.health = 30;
        assert(multiplier.GetValue(&heal) == 0.0f && last->reason == Reason::Boss);
        assert(last->aoe == -1 && last->target == -1);
        state.bossHold = false;
        state.current = 10;
        assert(multiplier.GetValue(&heal) == 0.0f && last->reason == Reason::Aoe);
        assert(last->aoe == 255 && last->target == -1);
        state.aoe = 10;
        state.current = 80;
        assert(multiplier.GetValue(&heal) == 0.0f && last->reason == Reason::Target);
        settings.target = 90;
        assert(multiplier.GetValue(&heal) == 1.0f && last->reason == Reason::Allowed);
        settings.aoe = 60;
        state.aoe = 60;
        assert(multiplier.GetValue(&heal) == 0.0f);
        settings.healing = HealingMode::Exempt;
        friendly.health = 100;
        assert(multiplier.GetValue(&heal) == 1.0f && last->reason == Reason::Exempt);
        assert(multiplier.GetValue(&damage) == 0.0f);
        for (int invalid = 0; invalid < 4; ++invalid)
        {
            friendly.hostile = invalid == 0;
            friendly.alive = invalid != 1;
            friendly.inWorld = invalid != 2;
            friendly.map = invalid == 3 ? nullptr : &raid;
            assert(multiplier.GetValue(&heal) == 0.0f);
        }
        heal.target = nullptr;
        assert(multiplier.GetValue(&heal) == 0.0f);
    }
    else if (scenario == "old")
    {
        state.aoe = 49;
        assert(multiplier.GetValue(&heal) == 1.0f);
        state.aoe = 50;
        assert(multiplier.GetValue(&heal) == 0.0f);
        state.aoe = state.current = 73;
        assert(multiplier.GetValue(&heal) == 0.0f);
    }
    else
        return 1;
    std::cout << "Passed " << scenario << '\n';
}
