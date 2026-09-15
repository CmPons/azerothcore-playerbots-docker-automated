#include "GroundFixture.h"
#include "MoveSplineInit.h"
#include "RaidCombatPolicy.h"
#include "RaidCombatGround.inc"
#include <iostream>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(#x)+":"+std::to_string(__LINE__)); } while (0)

class MovementAction
{
public:
    explicit MovementAction(PlayerbotAI& ai) : botAI(&ai), bot(ai.GetBot()) { }
    PlayerbotAI* botAI;
    Player* bot;
    bool verbose = false;
    std::string getName() const { return "follow"; }
    void RecordCthunFollow();
};
#define AI_VALUE(type, name) botAI->context.last.value
#include "RecordCthunFollow.inc"
#undef AI_VALUE

int main()
{
    Map map;
    CthunPolicy::Scope scope("531-1-1", "/missing", "/missing");
    map.CustomData.scope = &scope;
    scope.api = 2;
    scope.active = true;
    scope.generation = 9;
    scope.plannedAt = clockNow;
    PlayerbotAI ai;
    Player& bot = ai.bot;
    bot.map = &map;
    auto& mm = bot.mm;
    RaidCombat::Intent goal;
    goal.movement = RaidCombat::Positioning::Ground;
    goal.x = 20;
    auto frame = [&]
    {
        clockNow += 250;
        scope.plannedAt = clockNow;
        RaidCombat::MaintainGround(ai, goal, 9, clockNow);
    };
    auto follow = [&]
    {
        RaidCombat::ReleaseGround(ai);
        mm.Mutate(new BasicMotion(FOLLOW_MOTION_TYPE), MOTION_SLOT_ACTIVE);
        Movement::MoveSplineInit init(&bot);
        init.MoveTo(5, 0, 0, false);
        init.Launch();
        ai.raidCombat.scheduled = true;
        RaidCombat::RecordScheduledMotion(ai);
        return mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity();
    };
    uint64 scheduled = follow();
    map.blocked = true;
    frame();
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled);
    CHECK(!ai.raidCombat.claim); // Validate geometry BEFORE handoff.
    unsigned paths = PathGenerator::calculations;
    RaidCombat::MaintainGround(ai, goal, 9, clockNow);
    CHECK(PathGenerator::calculations == paths); // Bounded failed-frame retry.
    map.blocked = false;
    PathGenerator::custom = {{0, 0, 0}, {0, 4, 0}, {2.8f, 0, 0}};
    frame();
    CHECK(!ai.raidCombat.claim && ai.raidCombat.movementReceipt == 4);
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled); // Still bounded to six actual yards.
    PathGenerator::custom.clear();
    map.floor = 2;
    frame();
    CHECK(!ai.raidCombat.claim && ai.raidCombat.movementReceipt == 4); // Unsupported floor still rejects.
    map.floor = 0;
    map.water = true;
    frame();
    CHECK(!ai.raidCombat.claim && ai.raidCombat.movementReceipt == 4); // No swimming fallback.
    map.water = false;
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled);
    frame();
    CHECK(ai.raidCombat.claim && ai.raidCombat.ownedMotion != scheduled);
    CHECK(RaidCombat::HasMovementClaim(ai));
    CHECK(!bot.movespline->Finalized() && bot.movespline->Duration() < 1000);
    uint32 ownedSpline = bot.movespline->GetId();
    mm.Mutate(new BasicMotion(FLEEING_MOTION_TYPE), MOTION_SLOT_CONTROLLED);
    unsigned states = bot.statesCleared;
    frame();
    CHECK(bot.movespline->Finalized()); // Exact owned cleanup, not waiting out spline TTL.
    CHECK(mm.GetMotionSlotType(MOTION_SLOT_CONTROLLED) == FLEEING_MOTION_TYPE);
    CHECK(bot.statesCleared == states); // CC state is not blanket-cleared by spline retirement.
    CHECK(!ai.raidCombat.claim);
    mm.DirectExpireSlot(MOTION_SLOT_CONTROLLED, false);
    scheduled = follow();
    ai.raidCombat.automaticMotion = 0;
    frame();
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled); // Unknown/manual follow wins.
    ai.raidCombat.automaticMotion = scheduled;
    ai.context.last.value.msTime = clockNow;
    ai.context.last.value.lastdelayTime = 10000;
    map.id = 531;
    MovementAction recorder(ai);
    recorder.RecordCthunFollow();
    CHECK(ai.context.last.value.lastdelayTime == 10000); // Legacy follow tagging cannot erase API2 leases.
    frame();
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled); // Every nonpolicy lease wins.
    ai.context.last.value.lastdelayTime = 0;
    bot.casting = true;
    frame();
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE)->GetIdentity() == scheduled); // No native/manual heal cancellation.
    bot.casting = false;
    frame();
    CHECK(ai.raidCombat.claim);
    ownedSpline = bot.movespline->GetId();
    mm.Mutate(new BasicMotion(FOLLOW_MOTION_TYPE), MOTION_SLOT_ACTIVE);
    Movement::MoveSplineInit replacement(&bot);
    replacement.MoveTo(8, 0, 0, false);
    replacement.Launch();
    uint32 replacementSpline = bot.movespline->GetId();
    CHECK(replacementSpline != ownedSpline);
    RaidCombat::ReleaseGround(ai);
    CHECK(bot.movespline->GetId() == replacementSpline && !bot.movespline->Finalized());
    CHECK(mm.GetMotionSlotType(MOTION_SLOT_ACTIVE) == FOLLOW_MOTION_TYPE); // Replacement motion survives cleanup.
    scheduled = follow();
    auto* current = static_cast<BasicMotion*>(mm.GetMotionSlot(MOTION_SLOT_ACTIVE));
    current->finalizer = [&] { ++bot.control; ai.enabled = false; };
    frame();
    CHECK(!ai.raidCombat.claim); // Post-handoff authority/control revalidation.
    ai.enabled = true;
    follow();
    frame();
    CHECK(ai.raidCombat.claim);
    clockNow += 1000;
    RaidCombat::MaintainGround(ai, goal, 9, scope.plannedAt);
    CHECK(!ai.raidCombat.claim && bot.movespline->Finalized());
    follow();
    goal.movement = RaidCombat::Positioning::Hold;
    frame();
    CHECK(ai.raidCombat.claim && bot.movespline->Finalized());
    CHECK(mm.GetMotionSlot(MOTION_SLOT_ACTIVE) == nullptr);
    CHECK(RaidCombat::HasMovementClaim(ai)); // Targetless combat hold does not return to follow.
    bot.rooted = true;
    frame();
    CHECK(!ai.raidCombat.claim);
    bot.rooted = false;
    ai.enabled = false;
    frame();
    CHECK(!ai.raidCombat.claim); // Passive/stay/manual eligibility exclusion.
    std::cout << "actual ground adapter + native spline/MM/owned-stop: handoff, cleanup, replacements, TTL, holds, cast/CC/leases passed\n";
}
