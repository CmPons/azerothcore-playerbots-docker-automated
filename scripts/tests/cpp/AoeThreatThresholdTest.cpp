// Complete production ThreatMultiplier body; bounded context/config/encounter helper doubles.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
using uint8 = uint8_t;
using uint32 = uint32_t;
struct Unit { bool hostile = false; };
struct Player : Unit
{
    bool IsValidAttackTarget(Unit* target) { return target && target->hostile; }
};
struct PlayerbotAI {};
struct Action
{
    enum class ActionThreatType { None, Single, Aoe };
    virtual ~Action() = default;
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
    uint8 aoe = 0, current = 0;
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
        assert(limit == 70); // Existing configured default is unchanged.
        return state.bossHold;
    }
    void StopDirectDamage(PlayerbotAI*, Unit*) { ++state.stopped; }
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
