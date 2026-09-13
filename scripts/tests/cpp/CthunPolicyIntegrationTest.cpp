// Actual policy/action/scope + real Lua and checked-launch body; game APIs are explicit doubles.
#include "Aq40Cthun.h"
#include "CthunRoom.h"
#include "CthunPolicyScope.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace CthunPositioning;
namespace fs = std::filesystem;
struct FollowRecorder : MovementAction
{
    using MovementAction::MovementAction;
    void Record(bool direct) { verbose = direct; RecordCthunFollow(); }
};

struct Raid
{
    Map map;
    Group group;
    Creature eye, body;
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<PlayerbotAI>> ais;
    std::vector<std::unique_ptr<Aq40CthunPositionAction>> actions;
    std::string directory;
    Raid(std::string root, int count = 10) : directory(std::move(root))
    {
        eye.map = body.map = &map;
        eye.guid = 10001; body.guid = 10002; eye.reach = 15;
        eye.Relocate(CthunRoom::X, CthunRoom::Y, 100.304f);
        body.Relocate(CthunRoom::X, CthunRoom::Y, 100.304f);
        map.script.creatures[DATA_EYE] = &eye;
        map.script.creatures[DATA_CTHUN] = &body;
        for (int i = 0; i < count; ++i)
        {
            auto p = std::make_unique<Player>();
            p->guid = i + 1; p->map = &map; p->group = &group; p->healer = i == 2;
            p->melee = i < 4 && !p->healer;
            p->Relocate(-8632, 1970, 100.713f);
            group.Add(p.get());
            ais.push_back(std::make_unique<PlayerbotAI>(p.get()));
            actions.push_back(std::make_unique<Aq40CthunPositionAction>(ais.back().get()));
            ais.back()->ctx.GetValue<Unit*>("current target")->Set(&eye);
            players.push_back(std::move(p));
        }
        ais[0]->real = true;
        for (auto& ai : ais) ai->master = players[0].get();
        players[0]->Relocate(-8625, 1970, 100.713f);
    }
    void Plan()
    {
        std::vector<Player*> roster;
        for (auto& p : players) roster.push_back(p.get());
        UpdatePolicy(&map, roster, 250, directory, directory + "/status");
    }
    CthunPolicy::Scope* Scope() { return PolicyScope(&map); }
    void Load(std::string const& source)
    {
        map.script.state = 0;
        Plan();
        assert(Scope());
        std::string const rev = CthunPolicy::Digest(source);
        std::ofstream(directory + "/revisions/" + rev + ".lua") << source;
        std::ofstream(directory + "/requests/" + Scope()->id + ".txt")
            << "0123456789abcdef native " << rev << '\n';
        Scope()->Poll();
        Plan();
        assert(Scope()->active);
        map.script.state = IN_PROGRESS;
        Plan();
    }
    void Tick()
    {
        // Represents the PREVIOUS full map's after-player hook; next bot updates consume it.
        Plan();
        for (std::size_t i = 1; i < players.size(); ++i) actions[i]->Execute({});
        assert(Scope()->paths <= 64); // aggregate bound includes executing-spline retention checks
        for (std::size_t i = 1; i < players.size(); ++i)
        {
            Player* p = players[i].get();
            if (!p->moving) continue;
            auto const& goal = ais[i]->ctx.GetValue<LastMovement&>("last movement")->Get().lastMoveShort;
            float const distance = p->GetExactDist(goal.x, goal.y, goal.z);
            float const t = distance > 0 ? std::min(1.0f, 1.75f / distance) : 1.0f;
            p->Relocate(p->x + (goal.x-p->x)*t, p->y + (goal.y-p->y)*t, p->z + (goal.z-p->z)*t);
            p->spline.position = {p->x, p->y, p->z};
            if (t == 1) {p->moving = false; p->spline.done = true;}
        }
        clockMs += 250;
    }
};

int main(int argc, char** argv)
{
    assert(argc == 3 || argc == 4);
    fs::create_directories(std::string(argv[1]) + "/revisions");
    fs::create_directories(std::string(argv[1]) + "/requests");
    fs::create_directories(std::string(argv[1]) + "/status");
    std::ifstream input(argv[2]);
    std::string source((std::istreambuf_iterator<char>(input)), {});
    if (argc == 4 && std::string(argv[3]) == "blocked-route")
    {
        Raid r(argv[1], 2);
        r.Load(source);
        auto* p = r.players[1].get();
        Position blocked;
        int const best = r.Scope()->plan.choices[1];
        assert(best > 0);
        auto const& candidate = r.Scope()->snapshot.members[1].candidates[best];
        blocked.Relocate(candidate.x, candidate.y, candidate.z);
        Position const origin = *p;
        unsigned blockedCalls = 0, alternativeCalls = 0;
        r.map.pathCheck = [&](float x, float y, float z)
        {
            if (blocked.GetExactDist(x, y, z) < 0.05f) { ++blockedCalls; return false; }
            ++alternativeCalls;
            return true; // mesh is valid; only the particular best route is obstructed
        };
        CthunMovementMultiplier multiplier(r.ais[1].get());
        FollowAction follow(r.ais[1].get(), "follow");
        bool progressed = false;
        for (unsigned tick = 0; tick < 20 && !progressed; ++tick)
        {
            assert(p->GetExactDist(origin.x, origin.y, origin.z) == 0); // no artificial observation changes
            r.Plan();
            assert(multiplier.GetValue(&follow) == 0); // never restore unchecked follow
            progressed = r.actions[1]->Execute({});
            if (!progressed) assert(r.ais[1]->moves.empty());
            clockMs += 250;
        }
        assert(progressed && blockedCalls == 1 && alternativeCalls > 0);
        auto const& goal = r.ais[1]->moves.back();
        assert(goal.GetExactDist2d(&r.eye) < origin.GetExactDist2d(&r.eye));
        assert(blocked.GetExactDist(goal.x, goal.y, goal.z) > 0.05f);
        std::cout << "specific blocked route -> safe Lua-directed alternative passed\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[3]) == "fear-root")
    {
        Raid r(argv[1], 2);
        r.Load(source);
        auto* p = r.players[1].get();
        auto& last = r.ais[1]->ctx.GetValue<LastMovement&>("last movement")->Get();
        assert(r.actions[1]->Execute({}) && last.cthunOwner);
        p->Relocate(last.lastMoveShort.x, last.lastMoveShort.y, last.lastMoveShort.z);
        p->moving = false; p->spline.done = true;
        uint32 const spline = p->spline.id;
        // MotionMaster controlled slot + FleeingMovementGenerator::DoInitialize/SetTargetLocation:
        // both StopMoving calls return for the finalized spline; root prevents a new fear spline.
        p->flags |= UNIT_STATE_ROOT;
        p->motion.type = FLEEING_MOTION_TYPE; p->motion.controlledFear = true; p->motion.states = &p->flags;
        p->StopMoving();
        p->flags |= UNIT_STATE_FLEEING;
        if (p->HasUnitState(UNIT_STATE_ROOT)) p->StopMoving();
        assert(p->spline.id == spline && !ControlsMovement(p, r.ais[1].get()));
        int const clears = p->motion.clears, stops = p->moveStops;
        assert(r.actions[1]->isUseful() && !r.actions[1]->Execute({}));
        assert(p->motion.controlledFear && p->HasUnitState(UNIT_STATE_FLEEING));
        assert(p->motion.type == FLEEING_MOTION_TYPE && p->HasUnitState(UNIT_STATE_ROOT));
        assert(p->motion.clears == clears && p->moveStops == stops && p->spline.id == spline);
        assert(!last.cthunOwner);
        std::cout << "finalized same-ID policy ownership preserves rooted controlled fear passed\n";
        return 0;
    }
    {
        CthunPolicy::Snapshot snapshot;
        snapshot.combat = true; snapshot.committed = true; snapshot.count = 1;
        auto& member = snapshot.members[0];
        member.eligible = true; member.entering = true; member.count = 2;
        member.candidates[0].goal = 10; member.candidates[0].glare = 3;
        member.candidates[1].goal = 4; member.candidates[1].glare = 3;
        member.candidates[1].crowding = 400; // doorway transit cannot obey a hard spread veto
        CthunPolicy::Runtime original, changed;
        CthunPolicy::Plan plan;
        assert(original.Load(source) && original.Evaluate(snapshot, plan));
        assert(plan.choices[0] == 1);
        std::string edited = source;
        auto const at = edited.find("if member.entering then");
        assert(at != std::string::npos);
        edited.replace(at, std::string("if member.entering then").size(), "if false then");
        assert(changed.Load(edited) && changed.Evaluate(snapshot, plan));
        assert(plan.choices[0] == 0); // same native binary, meaningful Lua-only behavior change
    }
    {
        CthunPolicy::Snapshot snapshot;
        snapshot.combat = snapshot.committed = true; snapshot.count = 2;
        for (auto& member : snapshot.members)
        {
            member.eligible = member.entering = true; member.count = 3; member.reach = 1.5f;
            member.candidates[0].goal = 10;
            member.candidates[1].x = 6; member.candidates[1].goal = 4;
            member.candidates[2].x = 3; member.candidates[2].goal = 7;
            for (auto& candidate : member.candidates) candidate.glare = 3;
        }
        CthunPolicy::Runtime runtime;
        CthunPolicy::Plan plan;
        assert(runtime.Load(source) && runtime.Evaluate(snapshot, plan));
        assert(plan.choices[0] == 1 && plan.choices[1] == 2); // distinct viable entry endpoints
        // Constrained doorway: even a penalized shared endpoint must beat exterior waiting.
        for (auto& member : snapshot.members)
        {
            member.count = 2; member.candidates[0].goal = 7;
            member.candidates[1].x = 3; member.candidates[1].goal = 4;
        }
        assert(runtime.Evaluate(snapshot, plan));
        assert(plan.choices[0] == 1 && plan.choices[1] == 1);
    }
    {
        Raid r(argv[1], 2);
        r.Load(source);
        auto const count = r.Scope()->snapshot.members[1].count;
        int const choice = r.Scope()->plan.choices[1];
        assert(choice > 0);
        auto const calls = r.map.pathCalls;
        r.Scope()->paths = 64;
        assert(!r.actions[1]->Execute({}));
        assert(r.map.pathCalls == calls && r.Scope()->plan.choices[1] == choice);
        r.Plan();
        assert(r.Scope()->snapshot.members[1].count == count && r.actions[1]->Execute({}));
    }
    {
        Raid r(argv[1]);
        r.Load(source);
        for (int tick = 0; tick < 160; ++tick) r.Tick();
        for (unsigned i = 1; i < r.players.size(); ++i)
            assert(CthunRoom::Interior(*r.players[i]));
        assert(r.Scope()->adopted == 9);
        float closest = 1000;
        for (unsigned i = 1; i < r.players.size(); ++i)
            for (unsigned j = i + 1; j < r.players.size(); ++j)
                closest = std::min(closest, r.players[i]->GetExactDist2d(r.players[j].get()));
        std::cout << "interior minimum spacing=" << closest << '\n';
        assert(closest > 14.5f);
        std::cout << "ten-player landing -> interior entry passed\n";
    }
    {
        Raid r(argv[1], 3);
        r.Load(source);
        auto* p = r.players[1].get(); auto* ai = r.ais[1].get();
        p->Relocate(CthunRoom::X-30, CthunRoom::Y, 100.304f);
        r.players[2]->Relocate(p->x, p->y, p->z);
        r.Plan();
        p->casting = true;
        assert(!r.actions[1]->Execute({}) && p->casting && p->interrupts == 0);
        CthunMovementMultiplier multiplier(ai);
        CastHealingSpellAction heal(ai, "heal"); MeleeAction attack(ai, "melee");
        FollowAction follow(ai, "follow");
        assert(multiplier.GetValue(&heal) == 1 && multiplier.GetValue(&attack) == 1);
        assert(multiplier.GetValue(&follow) == 0);
        p->casting = false;
        ai->stay = true; assert(!ControlsMovement(p, ai)); ai->stay = false;
        ai->combatFollow = true; assert(!ControlsMovement(p, ai)); // existing persistent explicit/PvP follow
        ai->combatFollow = false; assert(ControlsMovement(p, ai)); // existing go/strategy removal
        ai->passive = true; assert(!ControlsMovement(p, ai)); ai->passive = false;
        auto& last = ai->ctx.GetValue<LastMovement&>("last movement")->Get();
        last.Set(531, p->x+2, p->y, p->z, 0, 1000, MovementPriority::MOVEMENT_NORMAL);
        assert(!ControlsMovement(p, ai)); // even lower-priority explicit current leases win
        last.clear();
        FollowRecorder recorder(ai, "follow");
        p->spline.done = false;
        recorder.Record(true); // actual Engine direct-call marker, even without an Event owner
        assert(last.cthunManual == p->spline.id && !last.cthunAutomatic);
        recorder.Record(false);
        assert(last.cthunManual == p->spline.id); // same active manual motion is not reclassified
        ++p->spline.id;
        recorder.Record(false);
        assert(last.cthunAutomatic == p->spline.id && !last.cthunManual);
        last.clear();
        last.cthunManual = p->spline.id; p->spline.done = false;
        assert(!ControlsMovement(p, ai));
        p->spline.done = true; p->motion.type = FOLLOW_MOTION_TYPE;
        assert(ControlsMovement(p, ai)); // finalized explicit follow cannot disable tactics forever
        ++p->spline.id; p->motion.type = 0;
        assert(ControlsMovement(p, ai)); // no permanent PvE follow disable
        p->auras.insert(DIGESTIVE_ACID); assert(!ControlsMovement(p, ai)); p->auras.clear();
        r.map.script.state = DONE; assert(!ControlsMovement(p, ai)); r.map.script.state = IN_PROGRESS;
        ai->ctx.GetValue<Unit*>("current target")->Set(nullptr); // targetless transition, regardless of AI engine
        r.eye.hp = 0; assert(!ControlsMovement(p, ai)); r.Plan(); assert(ControlsMovement(p, ai));
        r.eye.hp = 100; r.Plan();
        r.map.pathType = PATHFIND_SHORTCUT;
        // A positively tagged old automatic follow must not keep drifting when no checked route exists.
        p->moving = true; p->spline.done = false; p->motion.type = FOLLOW_MOTION_TYPE;
        recorder.Record(false);
        assert(!r.actions[1]->Execute({}) && !p->moving && p->motion.type == 0);
        p->moving = true; p->spline.done = false; p->motion.type = CHASE_MOTION_TYPE; last.clear();
        int const ambiguousStops = p->moveStops;
        assert(!r.actions[1]->Execute({}) && p->moveStops == ambiguousStops); // unknown provenance survives
        p->moving = false; p->spline.done = true; p->motion.type = 0;
        r.map.pathType = PATHFIND_NORMAL;
        ai->pathAllowed = false; assert(!r.actions[1]->Execute({})); ai->pathAllowed = true;
        // A stale plan never moves, and cannot stop somebody else's replacement spline.
        clockMs += 1001;
        p->moving = true; last.cthunOwner = 123; last.cthunSpline = p->spline.id-1;
        int stops = p->moveStops;
        assert(!r.actions[1]->Execute({}) && p->moveStops == stops);
        r.Plan();
        last.clear();
        p->moving = false;
        assert(r.actions[1]->Execute({}));
        assert(last.cthunOwner && last.cthunSpline == p->spline.id);
        auto const& route = ai->executedPath;
        assert(route.size() >= 2 && std::abs(route.front().x-p->x) < .001f);
        // Every executed segment came from the validated path, not an endpoint re-path.
        assert(route.back().x == last.lastMoveShort.x && route.back().y == last.lastMoveShort.y);
        ++r.Scope()->generation;
        assert(!r.actions[1]->Execute({}) && !p->moving && !last.cthunOwner);
    }
    {
        Raid r(argv[1], 2);
        r.Load(source);
        Player* p = r.players[1].get();
        p->Relocate(CthunRoom::X-30, CthunRoom::Y, 100.304f);
        r.map.pathDetour = {{p->x+3, p->y+2, p->z}};
        r.Plan();
        assert(r.actions[1]->Execute({}));
        assert(p->spline.points.size() == 3);
        // A newly arrived human makes the executing detour unsafe, while the endpoint and
        // newly generated straight path stay clear. Endpoint-only retention misses this.
        r.players[0]->Relocate(p->x+3, p->y+16.5f, p->z);
        r.map.pathDetour.clear();
        r.Plan();
        int const stops = p->moveStops;
        r.actions[1]->Execute({});
        assert(p->moveStops > stops);
        assert(!p->moving || p->spline.points.size() == 2);
    }
    {
        Raid r(argv[1], 2);
        r.Load("return {api=1,plan=function(s) local out={} for i,m in ipairs(s.members) do "
               "out[i]=m.eligible and (#m.candidates>1 and 1 or 0) or -1 end return out end}");
        Player* p = r.players[1].get();
        p->Relocate(CthunRoom::X-17.5f, CthunRoom::Y, 100.304f);
        r.Plan();
        assert(!NeedsMovement(p, r.ais[1].get()));
        assert(r.actions[1]->isUseful() && r.actions[1]->Execute({})); // Lua, not hidden native Needs, decides
    }
    {
        Raid r(argv[1], 2);
        r.Load("return {api=1,plan=function(s) local out={} for i,m in ipairs(s.members) do "
               "out[i]=m.eligible and 0 or -1 end return out end}");
        Player* p = r.players[1].get();
        p->moving = true; p->spline.done = false; p->casting = true; p->motion.type = FOLLOW_MOTION_TYPE;
        auto& last = r.ais[1]->ctx.GetValue<LastMovement&>("last movement")->Get();
        last.cthunAutomatic = p->spline.id;
        r.Plan();
        assert(r.actions[1]->isUseful());
        assert(!r.actions[1]->Execute({}) && !p->moving && p->casting && p->interrupts == 0);
    }
    for (float direction : {-1.0f, 1.0f})
    {
        Raid r(argv[1], 2);
        r.Load(source);
        auto* p = r.players[1].get();
        p->Relocate(CthunRoom::X+30, CthunRoom::Y, 100.304f);
        r.eye.auras.insert(RED_COLORATION);
        r.eye.orientation = direction * .15f;
        r.Plan();
        float const clearance = GlareClearance(&r.eye, *p, p->reach);
        p->casting = true;
        assert(r.actions[1]->Execute({}));
        Position const& goal = r.ais[1]->ctx.GetValue<LastMovement&>("last movement")->Get().lastMoveShort;
        assert(GlareClearance(&r.eye, goal, p->reach) > clearance);
        assert(p->interrupts == 1);
    }
    {
        Raid r(argv[1], 2);
        r.map.script.state = 0;
        r.players[0]->Relocate(-8638, 1915, 109);
        r.players[1]->Relocate(-8640, 1915, 109);
        r.Plan();
        assert(!ControlsMovement(r.players[1].get(), r.ais[1].get()));
        r.players[1]->otherCombat = true;
        assert(!ControlsMovement(r.players[1].get(), r.ais[1].get()));
        r.players[1]->Relocate(-8396, 2051, 116); // stacked NE trash is not the doorway
        assert(!ControlsMovement(r.players[1].get(), r.ais[1].get()));
    }
    std::cout << "real Lua/native entry, phase, glare, casts, manual leases and checked route passed\n";
}
