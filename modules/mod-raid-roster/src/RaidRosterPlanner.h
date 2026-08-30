#ifndef MOD_RAID_ROSTER_PLANNER_H
#define MOD_RAID_ROSTER_PLANNER_H

#include <cstdint>

struct RaidRosterRoleCounts
{
    uint32_t tanks = 0;
    uint32_t heals = 0;
    uint32_t dps = 0;

    uint32_t Total() const { return tanks + heals + dps; }
};

struct RaidRosterFillPlan
{
    uint32_t slotsForRoster = 0;
    RaidRosterRoleCounts bots;
};

// Convert a RAID_SUBCOMPS row into target whole-group composition. SubComp::dps
// historically counted only roster bots when the player is DPS, so the target
// total composition has one more DPS slot.
RaidRosterRoleCounts RaidRosterDesiredTotal(uint32_t tanks, uint32_t heals, uint32_t dpsExcludingPlayer);

// Plan how many roster bots of each role to add/keep for a requested target
// group size. `fixed` means the human player plus existing non-roster party/raid
// members. Fixed members satisfy role quotas first. If fixed members overfill a
// role, trim roster needs down to the remaining slots, preferring to trim DPS,
// then healers, then tanks. If an ideal role is unavailable in the roster, fill
// leftover slots with DPS, then healers, then tanks.
RaidRosterFillPlan PlanRaidRosterFill(uint32_t targetSize,
                                      RaidRosterRoleCounts desiredTotal,
                                      RaidRosterRoleCounts fixed,
                                      RaidRosterRoleCounts rosterAvailable);

#endif
