// Production tank multiplier is injected below; game state and action types are test doubles.
#include <array>
#include <iostream>

struct Unit
{
    Unit* victim = nullptr;
    bool active = false;
    Unit* GetVictim() const { return victim; }
};

struct PlayerbotAI
{
    bool tank = false;
    bool mainTank = false;
    bool IsTank(Unit*) const { return tank; }
    bool IsMainTank(Unit*) const { return mainTank; }
};

struct Action { virtual ~Action() = default; };
struct CombatFormationMoveAction : Action {};
struct TankAssistAction : Action {};
struct AvoidAoeAction : Action {};
struct CastReachTargetSpellAction : Action {};
struct CastTauntAction : Action {};
struct CastGrowlAction : Action {};
struct CastHandOfReckoningAction : Action {};
struct CastDarkCommandAction : Action {};

Unit* foundMagtheridon = nullptr;
bool channelersAlive = false;
constexpr int SOUTH_CHANNELER = 0;
constexpr int WEST_CHANNELER = 1;
constexpr int EAST_CHANNELER = 2;
Unit* GetChanneler(Unit* bot, int) { return channelersAlive ? bot : nullptr; }
bool IsMagtheridonActive(Unit* unit) { return unit && unit->active; }
#define AI_VALUE2(type, name, qualifier) foundMagtheridon

struct MagtheridonControlTankActionsMultiplier
{
    Unit* bot;
    PlayerbotAI* botAI;
    float GetValue(Action* action);
};

// PRODUCTION_METHOD

int main()
{
    Unit bot;
    Unit magtheridon;
    Unit other;
    PlayerbotAI ai;
    MagtheridonControlTankActionsMultiplier multiplier{&bot, &ai};
    CombatFormationMoveAction formation;
    TankAssistAction assist;
    AvoidAoeAction avoid;
    CastReachTargetSpellAction reach;
    CastTauntAction taunt;
    CastGrowlAction growl;
    CastHandOfReckoningAction reckoning;
    CastDarkCommandAction darkCommand;
    Action ordinary;
    std::array<Action*, 9> actions{&formation, &assist, &avoid, &reach, &taunt,
                                  &growl, &reckoning, &darkCommand, &ordinary};
    unsigned checks = 0;
    for (bool tank : {false, true})
    for (bool mainTank : {false, true})
    for (bool hasVictim : {false, true})
    for (bool bossPresent : {false, true})
    for (bool bossActive : {false, true})
    for (bool addsAlive : {false, true})
    for (Unit* bossVictim : std::array<Unit*, 3>{nullptr, &bot, &other})
    {
        ai.tank = tank;
        ai.mainTank = mainTank;
        bot.victim = hasVictim ? &other : nullptr;
        foundMagtheridon = bossPresent ? &magtheridon : nullptr;
        magtheridon.active = bossActive;
        magtheridon.victim = bossVictim;
        channelersAlive = addsAlive;
        for (Action* action : actions)
        {
            // All channeler-phase actions are unrestricted. Preserve the original boss-phase policy.
            float expected = 1.0f;
            if (tank && hasVictim && bossPresent && bossActive)
            {
                if (action == &formation || action == &assist ||
                    (action == &avoid && (mainTank || bossVictim == &bot)))
                    expected = 0.0f;
            }
            if (multiplier.GetValue(action) != expected)
            {
                std::cerr << "Tank multiplier mismatch: tank=" << tank << " mt=" << mainTank
                          << " victim=" << hasVictim << " boss=" << bossPresent
                          << " active=" << bossActive << " adds=" << addsAlive << '\n';
                return 1;
            }
            ++checks;
        }
    }
    std::cout << checks << " tank multiplier checks passed\n";
}
