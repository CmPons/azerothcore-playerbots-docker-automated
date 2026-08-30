#include "RaidRosterPlanner.h"

#include "gtest/gtest.h"

TEST(RaidRosterPlannerTest, FillsOnlyMissingSlotsWhenCompanionsAreAlreadyGrouped)
{
    RaidRosterFillPlan plan = PlanRaidRosterFill(
        10,
        RaidRosterDesiredTotal(2, 2, 5),
        RaidRosterRoleCounts{ 0, 0, 3 },   // player + two DPS companions
        RaidRosterRoleCounts{ 4, 9, 27 });

    EXPECT_EQ(plan.slotsForRoster, 7u);
    EXPECT_EQ(plan.bots.tanks, 2u);
    EXPECT_EQ(plan.bots.heals, 2u);
    EXPECT_EQ(plan.bots.dps, 3u);
    EXPECT_EQ(plan.bots.Total(), 7u);
}

TEST(RaidRosterPlannerTest, CountsTankPlayerAgainstTankQuota)
{
    RaidRosterFillPlan plan = PlanRaidRosterFill(
        10,
        RaidRosterDesiredTotal(2, 2, 5),
        RaidRosterRoleCounts{ 1, 0, 2 },   // tank player + two DPS companions
        RaidRosterRoleCounts{ 4, 9, 27 });

    EXPECT_EQ(plan.slotsForRoster, 7u);
    EXPECT_EQ(plan.bots.tanks, 1u);
    EXPECT_EQ(plan.bots.heals, 2u);
    EXPECT_EQ(plan.bots.dps, 4u);
    EXPECT_EQ(plan.bots.Total(), 7u);
}

TEST(RaidRosterPlannerTest, AddsNoBotsWhenFixedGroupAlreadyMeetsTargetSize)
{
    RaidRosterFillPlan plan = PlanRaidRosterFill(
        5,
        RaidRosterDesiredTotal(1, 1, 2),
        RaidRosterRoleCounts{ 1, 1, 3 },
        RaidRosterRoleCounts{ 4, 9, 27 });

    EXPECT_EQ(plan.slotsForRoster, 0u);
    EXPECT_EQ(plan.bots.Total(), 0u);
}

TEST(RaidRosterPlannerTest, TrimsDpsFirstWhenFixedRolesOverfillComposition)
{
    RaidRosterFillPlan plan = PlanRaidRosterFill(
        5,
        RaidRosterDesiredTotal(1, 1, 2),
        RaidRosterRoleCounts{ 0, 3, 0 },   // too many fixed healers, only two slots left
        RaidRosterRoleCounts{ 4, 9, 27 });

    EXPECT_EQ(plan.slotsForRoster, 2u);
    EXPECT_EQ(plan.bots.tanks, 1u);
    EXPECT_EQ(plan.bots.heals, 0u);
    EXPECT_EQ(plan.bots.dps, 1u);
    EXPECT_EQ(plan.bots.Total(), 2u);
}

TEST(RaidRosterPlannerTest, FillsUnavailableIdealRolesWithOtherAvailableRoles)
{
    RaidRosterFillPlan plan = PlanRaidRosterFill(
        10,
        RaidRosterDesiredTotal(2, 2, 5),
        RaidRosterRoleCounts{ 1, 0, 0 },
        RaidRosterRoleCounts{ 0, 9, 27 }); // no tanks available

    EXPECT_EQ(plan.slotsForRoster, 9u);
    EXPECT_EQ(plan.bots.tanks, 0u);
    EXPECT_EQ(plan.bots.heals, 2u);
    EXPECT_EQ(plan.bots.dps, 7u);
    EXPECT_EQ(plan.bots.Total(), 9u);
}
