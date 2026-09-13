// Production code against API doubles: geometry/decision tests, not live navigation or combat proof.
#include "Aq40Cthun.h"
#include <iostream>
#include <memory>

using namespace CthunPositioning;
constexpr float PI = 3.141592654f;

struct Raid
{
    Map map;
    Group group;
    Creature eye, body;
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<PlayerbotAI>> ais;
    std::vector<std::unique_ptr<Aq40CthunPositionAction>> actions;

    Raid(int count = 10)
    {
        clockMs = 1000;
        eye.map = body.map = &map;
        eye.guid = 10001;
        body.guid = 10002;
        eye.reach = 15;
        eye.Relocate(0, 0, 100.304f);
        body.Relocate(0, 0, 100.304f);
        map.script.creatures[DATA_EYE] = &eye;
        map.script.creatures[DATA_CTHUN] = &body;
        for (int i = 0; i < count; ++i)
        {
            auto p = std::make_unique<Player>();
            p->guid = i + 1;
            p->map = &map;
            p->group = &group;
            p->melee = i < 4;
            p->healer = i == 4 || i == 7 || i == 8;
            p->Relocate(150, 0, 100.304f);
            group.Add(p.get());
            ais.push_back(std::make_unique<PlayerbotAI>(p.get()));
            players.push_back(std::move(p));
            ais.back()->ctx.GetValue<Unit*>("current target")->Set(&eye);
            actions.push_back(std::make_unique<Aq40CthunPositionAction>(ais.back().get()));
        }
        ais[0]->real = true;
        for (auto& ai : ais)
            ai->master = players[0].get();
    }

    void Place(int i, float x, float y)
    {
        players[i]->Relocate(x, y, eye.z);
        players[i]->moving = false;
        ais[i]->ctx.GetValue<LastMovement&>("last movement")->Get().clear();
    }

    bool Think(int i)
    {
        int const before = map.pathCalls;
        bool const moved = actions[i]->Execute({});
        assert(map.pathCalls - before <= 8);
        if (moved)
        {
            auto& last = ais[i]->ctx.GetValue<LastMovement&>("last movement")->Get();
            last.msTime = clockMs;
            last.lastdelayTime = 700;
            assert(ais[i]->exactWaypoint);
            assert(players[i]->GetExactDist(last.lastMoveShort.x, last.lastMoveShort.y,
                                            last.lastMoveShort.z) <= STEP + 0.01f);
        }
        return moved;
    }

    void Advance(float seconds = 0.25f)
    {
        for (std::size_t i = 1; i < players.size(); ++i)
        {
            auto* p = players[i].get();
            if (!p->moving)
                continue;
            Position goal = ais[i]->ctx.GetValue<LastMovement&>("last movement")->Get().lastMoveShort;
            float const distance = p->GetExactDist(goal.x, goal.y, goal.z);
            float const t = distance > 0 ? std::min(1.0f, 7.0f * seconds / distance) : 1.0f;
            p->Relocate(p->x + (goal.x - p->x) * t, p->y + (goal.y - p->y) * t, p->z);
            if (t == 1)
                p->moving = false;
        }
        clockMs += uint32(seconds * 1000);
    }

    void Ticks(int count)
    {
        for (int tick = 0; tick < count; ++tick)
        {
            for (std::size_t i = 1; i < players.size(); ++i)
                Think(i);
            Advance();
        }
    }

    float Closest() const
    {
        float closest = 10000;
        for (std::size_t i = 0; i < players.size(); ++i)
            for (std::size_t j = i + 1; j < players.size(); ++j)
                closest = std::min(closest, players[i]->GetExactDist2d(players[j].get()));
        return closest;
    }
};

void GuardsAndPolicy()
{
    Raid r(3);
    auto* p = r.players[1].get();
    auto* ai = r.ais[1].get();
    r.Place(0, 30, 0);
    r.Place(1, 30, 0);
    r.Place(2, 30, 0);
    assert(ControlsMovement(p, ai));
    assert(NeedsMovement(p, ai));
    assert(!ControlsMovement(r.players[0].get(), r.ais[0].get()));
    ai->passive = true;
    assert(!ControlsMovement(p, ai));
    ai->passive = false;
    p->flags = UNIT_STATE_ROOT;
    assert(!ControlsMovement(p, ai));
    p->flags = 0;
    p->charmed = true;
    assert(!ControlsMovement(p, ai));
    p->charmed = false;
    p->alive = false;
    assert(!ControlsMovement(p, ai));
    p->alive = true;
    p->auras.insert(DIGESTIVE_ACID);
    assert(!ControlsMovement(p, ai));
    p->auras.clear();
    p->z = -98;
    assert(!ControlsMovement(p, ai));
    p->z = r.eye.z;
    r.map.script.state = DONE;
    assert(!ControlsMovement(p, ai));
    r.map.script.state = IN_PROGRESS;
    r.map.id = 469;
    assert(!ControlsMovement(p, ai));
    r.map.id = 531;
    r.group.raid = false;
    assert(!ControlsMovement(p, ai));
    r.group.raid = true;
    r.eye.phase = r.body.phase = 2;
    assert(!ControlsMovement(p, ai));
    r.eye.phase = r.body.phase = 1;

    CthunMovementMultiplier policy(ai);
    FollowAction follow(ai);
    ReachTargetAction reach(ai);
    RearFlankAction flank(ai);
    MeleeAction melee(ai);
    CastHealingSpellAction heal(ai);
    CastSpellAction spell(ai);
    CastSpellAction blink(ai, "blink"), charge(ai, "charge"), sprint(ai, "sprint");
    MovementAction manual(ai, "chat shortcut stay");
    assert(policy.GetValue(&follow) == 0);
    assert(policy.GetValue(&reach) == 0);
    assert(policy.GetValue(&flank) == 0);
    assert(policy.GetValue(&melee) == 1);
    assert(policy.GetValue(&heal) == 1);
    assert(policy.GetValue(&spell) == 1);
    assert(policy.GetValue(&blink) == 0);
    assert(policy.GetValue(&charge) == 0);
    assert(policy.GetValue(&sprint) == 1);
    assert(policy.GetValue(&manual) == 1);
    assert(policy.GetValue(r.actions[1].get()) == 1);
    assert(policy.GetValue(nullptr) == 1);
    auto& last = ai->ctx.GetValue<LastMovement&>("last movement")->Get();
    last.priority = MovementPriority::MOVEMENT_FORCED;
    last.msTime = clockMs;
    last.lastdelayTime = 1000;
    int stops = p->moveStops;
    assert(!r.Think(1));
    assert(p->moveStops == stops);
    clockMs += 1001;
    assert(r.Think(1));
    stops = p->moveStops;
    assert(!r.Think(1));
    assert(p->moveStops == stops); // continue the same checked waypoint, do not stutter-reissue
    Aq40CthunStatusAction status(ai);
    assert(status.Execute({}));
    assert(ai->messages.back().find("wanted=15") != std::string::npos);
}

void PathGuards()
{
    Raid r(2);
    r.Place(0, 30, 0);
    r.Place(1, 30, 0);
    Position spot;
    auto* p = r.players[1].get();
    auto* ai = r.ais[1].get();
    assert(FindPosition(p, ai, spot));
    assert(spot.GetExactDist2d(p) <= 6.01f);
    r.map.pathType = PATHFIND_NORMAL | PATHFIND_SHORTCUT;
    assert(!FindPosition(p, ai, spot));
    r.map.pathType = PATHFIND_NORMAL;
    ai->pathAllowed = false;
    assert(!FindPosition(p, ai, spot));
    ai->pathAllowed = true;
    p->los = false;
    assert(!FindPosition(p, ai, spot));
    p->los = true;
    r.map.fixedHeight = true;
    r.map.groundZ = p->z + 8;
    assert(!FindPosition(p, ai, spot));
    r.map.fixedHeight = false;
    Position detour;
    detour.Relocate(-30, 0, p->z);
    r.map.pathDetour = {detour};
    assert(!FindPosition(p, ai, spot));
    r.map.pathDetour.clear();
    // A short, normal path can still be unsafe between safe endpoints.
    r.Place(0, 45, 0);
    r.Place(1, 30, 0);
    detour.Relocate(32, 0, p->z);
    r.map.pathDetour = {detour};
    assert(!FindPosition(p, ai, spot)); // creates a new link to the previously spaced human
    r.Place(0, -30, 0);
    r.Place(1, 17, 6);
    r.eye.auras.insert(RED_COLORATION);
    detour.Relocate(17, 4, p->z);
    r.map.pathDetour = {detour};
    assert(!FindPosition(p, ai, spot)); // path enters the real beam before reaching safety
    p->moving = true;
    int const stops = p->moveStops;
    assert(!r.Think(1));
    assert(p->moveStops > stops && !p->moving);
    r.map.pathDetour.clear();
    r.eye.auras.clear();
    // Once separated, ordinary follow must not undo it or keep interrupting support casts.
    r.Place(0, -20, 0);
    r.Place(1, 17.4f, 0);
    assert(!NeedsMovement(p, ai));
    assert(!r.actions[1]->isUseful());
    p->casting = true;
    assert(!r.Think(1));
    assert(p->casting);
}

void EntranceAndDispersal()
{
    Raid r;
    r.map.script.state = 0;
    r.Place(0, 145, 0);
    for (int i = 1; i < 10; ++i)
        r.Place(i, 160, 0);
    r.Ticks(48);
    std::cerr << "prepull closest=" << r.Closest() << '\n';
    assert(r.Closest() >= 14.75f);
    for (int i = 1; i < 10; ++i)
        assert(r.players[i]->GetExactDist2d(&r.eye) >= 126.4f);
    r.Place(0, 106, 0);
    r.map.script.state = IN_PROGRESS;
    // Moving in must not recreate chains after the group has dispersed.
    for (int tick = 0; tick < 100; ++tick)
    {
        r.Ticks(1);
        assert(r.Closest() > 13.0f);
    }
    float nearest = 1000;
    for (int i = 1; i < 10; ++i)
        nearest = std::min(nearest, r.players[i]->GetExactDist2d(&r.eye));
    std::cerr << "combat nearest boss=" << nearest << " closest=" << r.Closest() << '\n';
    assert(nearest < 40);
}

void GlareBothDirections()
{
    for (float sign : {-1.0f, 1.0f})
    {
        Raid r(2);
        r.players[1]->melee = false;
        r.Place(0, -35, 0);
        r.Place(1, 30, 0);
        r.eye.auras.insert(RED_COLORATION);
        assert(GlareClearance(&r.eye, *r.players[1], 1.5f) < 0);
        r.Ticks(12); // native three-second red telegraph, before first lethal tick
        assert(GlareClearance(&r.eye, *r.players[1], 1.5f) >= 0);
        for (int tick = 0; tick <= 140; ++tick)
        {
            if (tick % 4 == 0)
                r.eye.orientation = sign * float(tick / 4) * PI / 35.0f;
            r.Ticks(1);
            // Test native five-yard forward line independently of the planner's warning cone.
            auto* p = r.players[1].get();
            float angle = std::remainder(std::atan2(p->y, p->x) - r.eye.orientation, 2 * PI);
            assert(std::abs(angle) > PI / 2 || std::abs(std::sin(angle)) * p->GetExactDist2d(&r.eye) >= 5.0f);
        }
        r.eye.auras.clear();
        assert(GlareClearance(&r.eye, *r.players[1], 1.5f) > 0);
        // Native Eye FAKE death keeps IsAlive true but sets health zero. Switch to the body's
        // smaller envelope, including when the ordinary target value still points at the Eye.
        r.eye.hp = 0;
        r.players[1]->melee = true;
        r.Place(1, 17.5f, 0);
        assert(ControlsMovement(r.players[1].get(), r.ais[1].get()));
        Position next;
        assert(FindPosition(r.players[1].get(), r.ais[1].get(), next));
        assert(next.GetExactDist2d(&r.body) < 16.7f);
        r.eye.alive = false;
        assert(ControlsMovement(r.players[1].get(), r.ais[1].get()));
        r.players[1]->z = -98;
        assert(!ControlsMovement(r.players[1].get(), r.ais[1].get()));
    }
}

void FullRaidSweep()
{
    for (float sign : {-1.0f, 1.0f})
        for (float start : {0.0f, 1.3f, 6.2f})
        {
            Raid r;
            for (int i = 0; i < 10; ++i)
            {
                float angle = i < 4 ? float(i) * PI / 2.0f : float(i - 4) * PI / 3.0f + 0.3f;
                float radius = i < 4 ? 17.5f : 34.0f;
                r.Place(i, radius * std::cos(angle), radius * std::sin(angle));
            }
            assert(r.Closest() > 15.0f);
            r.eye.orientation = start;
            r.eye.auras.insert(RED_COLORATION);
            r.Ticks(12);
            for (int tick = 0; tick <= 140; ++tick)
            {
                if (tick % 4 == 0)
                {
                    r.eye.orientation = start + sign * float(tick / 4) * PI / 35.0f;
                    // The server casts immediately after turning, BEFORE bots can react this tick.
                    for (int i = 1; i < 10; ++i)
                    {
                        auto* p = r.players[i].get();
                        float angle = std::remainder(std::atan2(p->y, p->x) - r.eye.orientation, 2 * PI);
                        bool clear = std::abs(angle) > PI / 2 ||
                            std::abs(std::sin(angle)) * p->GetExactDist2d(&r.eye) >= 5.0f;
                        if (!clear)
                            std::cerr << "sweep hit sign=" << sign << " start=" << start << " tick=" << tick
                                      << " player=" << i << " pos=" << p->x << ',' << p->y << '\n';
                        assert(clear);
                    }
                }
                r.Ticks(1);
            }
        }
}

void NeighborAndHealingGuards()
{
    Raid r(3);
    r.Place(0, -25, 0);
    r.Place(1, 17.5f, 0);
    r.Place(2, 17.5f, 0);
    auto* p = r.players[1].get();
    auto* ai = r.ais[1].get();
    r.players[2]->alive = false;
    assert(!NeedsMovement(p, ai));
    r.players[2]->alive = true;
    r.players[2]->phase = 2;
    assert(!NeedsMovement(p, ai));
    r.players[2]->phase = 1;
    r.players[2]->z = -98;
    assert(!NeedsMovement(p, ai));
    r.players[2]->z = p->z;
    r.players[2]->reach = 4;
    r.Place(2, 34.5f, 0);
    assert(NeedsMovement(p, ai)); // enlarged player needs more than the usual 15y
    r.players[2]->reach = 1.5f;
    assert(!NeedsMovement(p, ai));
    r.Place(2, 17.5f, 20);
    auto& reservation = r.ais[2]->ctx.GetValue<LastMovement&>("last movement")->Get();
    reservation.lastMoveShort.Relocate(17.5f, 14, p->z);
    reservation.priority = MovementPriority::MOVEMENT_COMBAT;
    reservation.msTime = clockMs;
    r.players[2]->moving = true;
    assert(NeedsMovement(p, ai));
    r.players[2]->moving = false;
    assert(!NeedsMovement(p, ai));
    r.Place(2, 34.5f, 0);
    r.players[1]->healer = true;
    r.Place(0, 95, 0);
    r.players[0]->hp = 30;
    r.Place(1, 34, 15);
    assert(NeedsMovement(p, ai)); // out-of-range injured human: healer has a checked approach
    Position spot;
    assert(FindPosition(p, ai, spot));
    assert(spot.GetExactDist2d(r.players[0].get()) < p->GetExactDist2d(r.players[0].get()));
    r.map.script.state = 0;
    r.Place(0, 200, 0);
    assert(!ControlsMovement(p, ai)); // human withdrew from the approach
}

int main()
{
    GuardsAndPolicy();
    PathGuards();
    EntranceAndDispersal();
    GlareBothDirections();
    FullRaidSweep();
    NeighborAndHealingGuards();
    std::cout << "Cthun production positioning regressions passed\n";
}
