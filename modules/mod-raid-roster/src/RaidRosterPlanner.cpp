#include "RaidRosterPlanner.h"

#include <algorithm>

RaidRosterRoleCounts RaidRosterDesiredTotal(uint32_t tanks, uint32_t heals, uint32_t dpsExcludingPlayer)
{
    return RaidRosterRoleCounts{ tanks, heals, dpsExcludingPlayer + 1 };
}

RaidRosterFillPlan PlanRaidRosterFill(uint32_t targetSize,
                                      RaidRosterRoleCounts desiredTotal,
                                      RaidRosterRoleCounts fixed,
                                      RaidRosterRoleCounts rosterAvailable)
{
    RaidRosterFillPlan plan;
    uint32_t const fixedCount = fixed.Total();
    plan.slotsForRoster = targetSize > fixedCount ? targetSize - fixedCount : 0;

    plan.bots.tanks = fixed.tanks >= desiredTotal.tanks ? 0 : desiredTotal.tanks - fixed.tanks;
    plan.bots.heals = fixed.heals >= desiredTotal.heals ? 0 : desiredTotal.heals - fixed.heals;
    plan.bots.dps = fixed.dps >= desiredTotal.dps ? 0 : desiredTotal.dps - fixed.dps;

    // If fixed members already overfill one or more roles, reduce roster needs
    // to the actual free slots. Prefer trimming DPS first to preserve tank/heal
    // coverage where possible.
    while (plan.bots.Total() > plan.slotsForRoster)
    {
        if (plan.bots.dps)
            --plan.bots.dps;
        else if (plan.bots.heals)
            --plan.bots.heals;
        else if (plan.bots.tanks)
            --plan.bots.tanks;
        else
            break;
    }

    plan.bots.tanks = std::min(plan.bots.tanks, rosterAvailable.tanks);
    plan.bots.heals = std::min(plan.bots.heals, rosterAvailable.heals);
    plan.bots.dps = std::min(plan.bots.dps, rosterAvailable.dps);

    // If the exact role mix is impossible, still fill up to target size with
    // available roster bots. DPS first keeps emergency overfill least disruptive.
    while (plan.bots.Total() < plan.slotsForRoster && plan.bots.dps < rosterAvailable.dps)
        ++plan.bots.dps;
    while (plan.bots.Total() < plan.slotsForRoster && plan.bots.heals < rosterAvailable.heals)
        ++plan.bots.heals;
    while (plan.bots.Total() < plan.slotsForRoster && plan.bots.tanks < rosterAvailable.tanks)
        ++plan.bots.tanks;

    return plan;
}
