#include "WintergraspBotsDirector.h"
#include "WintergraspBotsConfig.h"
#include "AiObjectContext.h"   // EngageNearbyEnemy: prioritized/current target values
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "BattlefieldWG.h"     // BATTLEFIELD_WG type tag, CanInteractWithRelic, vehicle entries
#include "AreaDefines.h"       // AREA_THE_SUNKEN_RING / _BROKEN_TEMPLE / _WESTSPARK/_EASTSPARK_WORKSHOP
#include "Creature.h"          // EngageNearbyEnemy
#include "GameObject.h"        // breach stage: keep-building health
#include "Group.h"             // adopt: battlefield raid groups (isBFGroup)
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"            // PathTest: FindMap(571)
#include "MotionMaster.h"      // MovePoint
#include "ObjectAccessor.h"
#include "PathGenerator.h"     // PathTest + StepBot debug path probe
#include "StringFormat.h"
#include "Player.h"
#include "ModelIgnoreFlags.h"  // VMAP::ModelIgnoreFlags for wall LOS checks
#include "Playerbots.h"
#include "Pet.h"         // ScrubPetTargets: Pet/Guardian complete types
#include "CharmInfo.h"   // ScrubPetTargets: SetIsCommandAttack
#include "RandomPlayerbotMgr.h"
#include "Vehicle.h"           // TrySeatCannon: seat availability
#include "SpellMgr.h"          // TryMountBot: resolve mount spells
#include "SpellInfo.h"         // TryMountBot: Effects[].ApplyAuraName / BasePoints
#include "SpellAuraDefines.h"  // TryMountBot: SPELL_AURA_MOUNTED / flight-speed aura
#include <algorithm>
#include <cmath>
#include <list>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32 WG_ZONE_ID = 4197;   // Wintergrasp
    constexpr uint32 WG_MAP_ID  = 571;    // Northrend
    constexpr uint32 WG_MOVE_ID = 987201; // MotionMaster point id for director-issued moves

    // The core cannot build a walkable path longer than MAX_POINT_PATH_LENGTH(74) smooth-path
    // steps of SMOOTH_PATH_STEP_SIZE(4yd) ≈ 296yd — beyond that PathGenerator returns
    // SHORTCUT|NOPATH and MovePoint falls back to a straight 3D spline (bots glide through the
    // air and through walls; the WG floating/clipping bug). Objectives here are 300–1100yd out,
    // so all travel is issued in legs comfortably under that ceiling.
    constexpr float WG_MAX_LEG = 220.0f;

    // Unstick stop-short geometry: never teleport a bot INTO its objective (a defender rally
    // objective can be the enemy vehicle cluster itself). Below the min distance the teleport
    // is withheld entirely — until the escalation counter overrides (see StepBot).
    constexpr float WG_UNSTICK_MIN_DIST = 60.0f;
    constexpr float WG_UNSTICK_SHORT    = 40.0f;

    // Battle-center fallback (kept for populate entry + as a last-resort objective). The
    // mod-playerbots BF patch auto-accepts the invite once the bot is in the zone.
    constexpr float WG_X = 5314.51f;
    constexpr float WG_Y = 2789.83f;
    constexpr float WG_Z = 409.13f;
    constexpr float WG_O = 3.14f;

    // The 4 capturable outer workshops: capture-banner position + the area id GetData() keys on.
    // Coords are the actual "Wintergrasp * Factory Banner" GO spawns (acore_world.gameobject,
    // entries 190475/190487/194959/194962) so bots stand IN the capture radius, not at the
    // (offset) graveyard. Area ids follow the core workshop->area map (Sunken Ring=NE,
    // Broken Temple=NW, Eastspark=SE, Westspark=SW).
    struct WorkshopDef { float x, y, z; uint32 areaId; };
    const WorkshopDef WG_WORKSHOPS[4] =
    {
        { 4949.34f, 2432.59f, 320.18f, AREA_THE_SUNKEN_RING   }, // NE  banner 190475
        { 4948.52f, 3342.34f, 376.88f, AREA_THE_BROKEN_TEMPLE }, // NW  banner 190487
        { 4398.08f, 2356.50f, 376.19f, AREA_EASTSPARK_WORKSHOP}, // SE  banner 194959
        { 4390.78f, 3304.09f, 372.43f, AREA_WESTSPARK_WORKSHOP}, // SW  banner 194962
    };

    // Defender keep-approach anchor (no-siege fallback).
    const Position WG_DEFENDER_ANCHOR = { 5345.0f,  2842.0f,  410.0f,  3.14f };

    // Sortie muster: interior staging spot in the west courtyard near the gatehouse ramp.
    // Rally waves gather here until quorum, then exit together (jump/portal crossings).
    const Position WG_MUSTER = { 5185.0f, 2855.0f, 409.0f, 0.0f };
    constexpr float  WG_MUSTER_RADIUS   = 20.0f;   // counted as "at the muster" within this
    constexpr float  WG_RALLY_NEAR      = 80.0f;   // released wave counted as "at the rally" within this
    constexpr uint32 WG_SORTIE_GRACE_MS = 30000;   // released wave gets this long to exit before wipe checks

    // Attacker staging by breach stage along the keep's central axis (gate X~5163 -> mid wall
    // 191805 X~5279 -> last door 191810 X~5397): just west of the fortress door until it falls
    // (NOT on the door line — a leg ending on the door gets LOS-truncated forever), then the
    // courtyard short of the mid wall, then short of the last door. Foot bots push WITH the
    // siege instead of parking at the gate all battle, and every advance walks through an
    // already-destroyed opening (visually legitimate — no intact-wall clipping).
    const Position WG_ATTACKER_PUSH[3] =
    {
        { 5145.0f, 2841.0f, 409.0f, 0.0f },   // stage 0: outside, short of the intact fortress door
        { 5240.0f, 2841.0f, 409.0f, 0.0f },   // stage 1: door down -> courtyard before the mid wall
        { 5360.0f, 2841.0f, 414.0f, 0.0f },   // stage 2: mid wall down -> before the last door
    };

    // The keep's REAL defender crossings — NO teleports across walls (user directive; the old
    // gate-side teleport hop read as bots blinking to the front of the keep). OUT: run up the
    // gatehouse rampart and JUMP off the wall (the keep's intended defender exit; the mesh has
    // no jump-down links, so the drop itself is a scripted MoveJump from the rampart edge).
    // IN: run to the nearest Defender's Portal spawn (GO 190763 — WGPortalDefenderData in
    // BattlefieldWG.h rings the outside walls with them) and take it into the west courtyard.
    // Probe-verified (pathtest rampart probe): the courtyard mesh connects to a z~422 rampart
    // tier at (5156,2862) with a NORMAL path — the z~438 wall tops are NOT mesh-reachable from
    // inside (paths to them end on the ground, type 0x84). Bots climb to the 422 tier and jump
    // the ~13y down from there.
    const Position WG_RAMP_TOP     = { 5156.0f, 2862.0f, 422.0f, 0.0f };  // gatehouse rampart tier, north of the gate
    const Position WG_JUMP_LANDING = { 5142.0f, 2862.0f, 409.0f, 3.14f };  // ground outside, below the rampart
    const Position WG_PORTAL_DEST  = { 5240.0f, 2841.0f, 409.0f, 0.0f };  // portal exit: west courtyard
    // Mid-wall archway router. The keep's interior mid wall (x~5279) is only passable at its
    // axis archway: an off-axis approach paths into the wall face beside the opening and pins
    // there (runtime-observed both for exits AND for interior moves like east-side bots heading
    // to the west cannon posts). Any defender leg that crosses the mid-wall line stages onto
    // the axis on its own side, then crosses to the far-side point.
    constexpr float WG_MIDWALL_X = 5272.0f;
    const Position WG_ARCH_W = { 5250.0f, 2841.0f, 409.0f, 0.0f };   // west of the arch, on axis
    const Position WG_ARCH_E = { 5292.0f, 2841.0f, 409.0f, 0.0f };   // east of the arch, on axis
    // Breach-frontier line for the last door (191810 at x~5397): attackers may not be east of
    // this while the door stands. Gate/mid-wall frontiers reuse IsInsideKeep / WG_MIDWALL_X.
    constexpr float WG_LASTDOOR_X = 5390.0f;

    // The keep breach path is a central corridor on y~2841; every legal opening (fortress gate,
    // mid-wall arch, last door) is on that axis, while the flanking wall segments stand at
    // |y-2841|~42. Attackers are corridor-contained inside the keep (all their in-keep objectives
    // are on-axis); a bot off the corridor got there by clipping a standing flanking wall.
    constexpr float WG_KEEP_CORRIDOR_HW = 30.0f;   // attacker central-corridor half-width (Fix 8)
    // x-band around a standing interior wall LINE within which an off-axis unit counts as
    // "crossing" it (used for defenders, who legitimately hold off-axis interior spots like the
    // cannon posts far from these lines, so they can't be blanket corridor-contained). (Fix 8)
    constexpr float WG_WALL_CLIP_BAND = 25.0f;
    // Half-height of the interior walls (they span y~2683-2995). The defender wall-crossing check
    // fires only WITHIN this span, so it can't reach the two off-axis Defender's Portal spawns at
    // y~2666/3013 (just past the wall ends) and yank an approaching defender inside. (Fix 8)
    constexpr float WG_WALL_Y_HALFSPAN = 120.0f;
    // Mount only for travel legs longer than this (avoids mount/dismount churn on short hops). (Fix 7)
    constexpr float WG_MOUNT_MIN_DIST = 50.0f;
    const Position WG_DEFENDER_PORTALS[] =
    {
        { 5153.41f, 2901.35f, 409.19f, 0.0f },
        { 5268.70f, 2666.42f, 409.10f, 0.0f },
        { 5197.05f, 2944.81f, 409.19f, 0.0f },
        { 5196.67f, 2737.34f, 409.19f, 0.0f },
        { 5314.58f, 3055.85f, 408.86f, 0.0f },
        { 5391.28f, 2828.09f, 418.68f, 0.0f },
        { 5153.93f, 2781.67f, 409.25f, 0.0f },
        { 5311.44f, 2618.93f, 409.09f, 0.0f },
        { 5269.21f, 3013.84f, 408.83f, 0.0f },
        { 5401.62f, 2853.66f, 418.67f, 0.0f },
    };

    // Field garrison posts — AzerothCore's WG script never implemented the workshop/road guard
    // NPCs (only the keep + attack-tower guards exist), which also starves attackers of rank
    // kills. Layout per user direction: 4 guards tight on the workshop flag + 2 at each yard
    // entrance (spread out — a blob of 8 on the flag was the old, wrong look). Entrance
    // coordinates are INITIAL ESTIMATES from the road network; refine against the live map
    // (see the verification step) — z is ground-snapped at spawn either way. Each post belongs
    // to the controlling faction of its associated workshop area and its squad respawns when
    // the post flips (dead guards stay dead until then — not farmable in place). Guard entries
    // 30739/30740 pass BattlefieldWG::IsKeepNpc, so killing them PROMOTES nearby war
    // participants exactly like keep guards.
    struct GarrisonPostDef
    {
        uint32 areaId;      // owning workshop area (WorkshopController keys on it)
        uint8  count;       // used slots in slots[]
        float  slots[8][2]; // x,y per guard; z comes from GetHeight at the flag/post z seed
        float  zSeed;       // height-search seed for the whole post
    };
    constexpr GarrisonPostDef WG_GARRISON_POSTS[] =
    {
        // NE Sunken Ring — flag 4949.34,2432.59
        { AREA_THE_SUNKEN_RING, 8, { {4953,2437},{4945,2437},{4953,2428},{4945,2428},
                                     {4990,2470},{4984,2462},{4905,2400},{4913,2394} }, 320.18f },
        // NW Broken Temple — flag 4948.52,3342.34
        { AREA_THE_BROKEN_TEMPLE, 8, { {4952,3346},{4944,3346},{4952,3338},{4944,3338},
                                       {4995,3310},{4989,3318},{4900,3375},{4908,3369} }, 376.88f },
        // SE Eastspark — flag 4398.08,2356.50
        { AREA_EASTSPARK_WORKSHOP, 8, { {4402,2360},{4394,2360},{4402,2352},{4394,2352},
                                        {4440,2395},{4434,2387},{4355,2320},{4363,2314} }, 376.19f },
        // SW Westspark — flag 4390.78,3304.09
        { AREA_WESTSPARK_WORKSHOP, 8, { {4395,3308},{4387,3308},{4395,3300},{4387,3300},
                                        {4435,3270},{4429,3278},{4345,3340},{4353,3334} }, 372.43f },
        // Bridge/road posts (3 guards each; positions from the core's "Flag near bridge" data)
        { AREA_WESTSPARK_WORKSHOP, 3, { {4577,3479},{4568,3479},{4573,3470} }, 363.01f },
        { AREA_EASTSPARK_WORKSHOP, 3, { {4530,2814},{4522,2814},{4526,2805} }, 391.20f },
        { AREA_THE_SUNKEN_RING,   3, { {5085,2291},{5077,2291},{5081,2282} }, 365.00f },
    };
    constexpr uint8 WG_GARRISON_POST_COUNT = 7;
    constexpr uint8 WG_ROAD_POST_FIRST = 4;   // posts 0-3 = workshops, 4+ = road/bridge posts
    constexpr uint32 WG_NPC_GUARD_H = 30739;   // = BATTLEFIELD_WG_NPC_GUARD_H
    constexpr uint32 WG_NPC_GUARD_A = 30740;   // = BATTLEFIELD_WG_NPC_GUARD_A

    // Stray-audit geometry: search wide enough to cover a post's whole slot layout, but only
    // despawn a guard standing near one of THIS post's authored slots — never a core guard
    // that merely chased into the area (nearest core guard spawn is 58y from any post).
    constexpr float WG_GARRISON_AUDIT_SEARCH     = 80.0f;
    constexpr float WG_GARRISON_AUDIT_SLOT_RANGE = 15.0f;

    constexpr uint32 GO_TITAN_RELIC = 192829;

    // The attacker's GROUND siege vehicles (turrets/tower cannons are tower-mounted and handled
    // separately). Used by the engage layer and cannon target curation.
    constexpr uint32 WG_GROUND_VEHICLES[4] =
    {
        NPC_WINTERGRASP_CATAPULT,
        NPC_WINTERGRASP_DEMOLISHER,
        NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE,
        NPC_WINTERGRASP_SIEGE_ENGINE_HORDE,
    };

    // Cannon posts: ground positions at the INSIDE foot of the two west gatehouse tower pairs
    // (towers 190373 north / 190377 south, turrets 28366 at Z~440 on top). A defender walks to
    // the post, then is seated into the turret remotely (players climb the tower stairs; the
    // navmesh doesn't reach the tower top, so the seat click is the compromise).
    // Post spots are pulled ~20y off the tower feet: legs ENDING against the tower model get
    // LOS-truncated to nothing and pin the bot just short (runtime-observed at 5196,2927 /
    // 5181,2775 — which are themselves reachable, so the posts sit there). Both spots remain
    // within TrySeatCannon's 60y turret search.
    const Position WG_CANNON_POSTS[2] =
    {
        { 5190.0f, 2920.0f, 409.0f, 0.0f },   // north gatehouse (turrets at 5137,2935 / 5164,2961)
        { 5178.0f, 2772.0f, 409.0f, 0.0f },   // south gatehouse (turrets at 5164,2722 / 5138,2748)
    };

    // Rough keep footprint (contains the relic room, courtyards, battle center; excludes the
    // gate approach and all workshops). Used to partition defender assignments while their gate
    // is intact: bots inside can't reach outside jobs on foot (and vice versa), so cross-door
    // assignments only produce LOS-blocked legs and unstick teleports.
    bool IsInsideKeep(float x, float y)
    {
        return x > 5160.0f && x < 5500.0f && y > 2630.0f && y < 3050.0f;
    }

    // Fully remove a module-spawned guard from the map. NOT DespawnOrUnsummon: for a raw
    // (spawnId-less) battlefield creature that only flips the object to the Dead state — the
    // husk stays in the grid forever, RemoveCorpse no-ops on it, and every later audit sweep
    // re-finds and re-counts it (runtime-verified: 100+ "stray" audits that were the same
    // invisible husks). AddObjectToRemoveList is the real removal.
    void RemoveGuard(Creature* c)
    {
        c->CombatStop();
        c->AddObjectToRemoveList();
    }

    // True when a GO model roofs the position — the unit stands inside/under a structure
    // (workshop bay, gate arch). Gates the first-segment escape exemption in leg validation:
    // WITHOUT the roof condition the exemption rolls forward as the mover re-paths each tick,
    // and a vehicle skirting a tower eventually "exempts" its way straight inside it
    // (runtime-observed clipping).
    bool UnderStructure(Unit* u)
    {
        return !u->GetMap()->isInLineOfSight(
            u->GetPositionX(), u->GetPositionY(), u->GetPositionZ() + 0.5f,
            u->GetPositionX(), u->GetPositionY(), u->GetPositionZ() + 40.0f,
            u->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO, VMAP::ModelIgnoreFlags::Nothing);
    }
    // Relic room (all converge here once the last door is down).
    const Position WG_RELIC_ANCHOR    = { 5440.379f, 2840.493f, 430.282f, 4.45f };

    const Position WG_KEEP_GATE = { 5163.0f, 2841.0f, 409.0f, 0.0f };
    constexpr uint32 WG_AXIS_STRUCTURES[3] = { 190375, 191805, 191810 };
}

// File-local: gather up to `count` random bots of `team` that are safe to pull into Wintergrasp.
static std::vector<Player*> SelectEligibleBots(TeamId team, int count);

// Compute the next walkable leg toward `tgt`: lerp a point <= WG_MAX_LEG yards out, snap it to
// the map height, and take the endpoint of a REAL generated path to it (on-mesh, ground-level).
// Halves the leg twice before giving up, so a rejected midpoint (over a wall line, off-mesh,
// unreachable) still yields a shorter valid leg. Returns false when no walkable leg exists.
// `truncated` reports that the returned leg was cut short by a standing wall/door — callers use
// it to detect "route exists on the mesh but is barred by a structure" (e.g. the closed gate).
static bool ComputeLegWaypoint(Player* bot, Position const& tgt, Position& out, bool& truncated)
{
    truncated = false;
    float dist = bot->GetExactDist2d(&tgt);
    float frac = dist > WG_MAX_LEG ? WG_MAX_LEG / dist : 1.0f;
    for (int attempt = 0; attempt < 3; ++attempt, frac *= 0.5f)
    {
        float x = bot->GetPositionX() + (tgt.GetPositionX() - bot->GetPositionX()) * frac;
        float y = bot->GetPositionY() + (tgt.GetPositionY() - bot->GetPositionY()) * frac;
        float zSeed = bot->GetPositionZ() + (tgt.GetPositionZ() - bot->GetPositionZ()) * frac;
        float z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, zSeed + 30.0f, true, 200.0f);
        if (z <= INVALID_HEIGHT + 1.0f)
            z = zSeed;   // no ground found (over a void): let the path query's own clamping decide

        PathGenerator pg(bot);
        pg.CalculatePath(x, y, z, false);
        if ((pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH)) ||
            pg.GetPath().size() < 2)
            continue;

        // Intact walls/doors are dynamic GAMEOBJECT WMO models: they block line of sight but are
        // absent from the baked navmesh, so a mesh path can cut straight through a standing wall.
        // Truncate the leg at the first blocked segment — the bot advances to the obstacle and
        // holds instead of ghosting through it (destroyed structures stop blocking LOS). The
        // FIRST segment may be exempted ONLY when the bot is roofed by a structure (escaping a
        // workshop bay/arch it stands in) — a blanket exemption rolls forward with re-pathing
        // and lets movers walk into towers.
        auto const& pts = pg.GetPath();
        size_t lastClear = pts.size() - 1;
        bool escapeOk = UnderStructure(bot);
        for (size_t i = 1; i < pts.size(); ++i)
        {
            bool clear = bot->GetMap()->isInLineOfSight(pts[i - 1].x, pts[i - 1].y, pts[i - 1].z + 2.0f,
                                                        pts[i].x, pts[i].y, pts[i].z + 2.0f,
                                                        bot->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO,
                                                        VMAP::ModelIgnoreFlags::Nothing);
            if (!clear && i == 1 && escapeOk)
                continue;   // stepping OUT of the structure the bot stands in
            if (!clear)
            {
                lastClear = i - 1;
                truncated = true;
                break;
            }
        }
        if (lastClear == 0)
            continue;   // wall right in front: try a shorter leg, else hold this tick

        // Endpoint of the (possibly truncated) path: on-mesh, ground-correct, and short enough
        // that MovePoint's internal path build reproduces it instead of shortcutting.
        G3D::Vector3 const& last = pts[lastClear];
        out.Relocate(last.x, last.y, last.z, 0.0f);
        return true;
    }
    return false;
}

WintergraspBotsDirector* WintergraspBotsDirector::s_instance = nullptr;

WintergraspBotsDirector::WintergraspBotsDirector() : WorldScript("WintergraspBotsDirector")
{
    s_instance = this;
}

WintergraspBotsDirector* WintergraspBotsDirector::instance()
{
    return s_instance;
}

void WintergraspBotsDirector::OnStartup()
{
    sWintergraspBotsConfig->Load();
}

void WintergraspBotsDirector::OnUpdate(uint32 diff)
{
    // GUARD FIRST: a registered hook fires even when the module is disabled. Bail before
    // touching any state (known segfault lesson in this project).
    if (!sWintergraspBotsConfig->enable)
        return;

    // Two-speed clock: the full reconcile (population, jobs, engagement, garrison) runs every
    // tickMs; a light movement sub-tick runs every second so a bot whose leg ended between
    // reconciles gets its next leg immediately. Waiting out the 5s tick after every short
    // wall-truncated leg produced a run-2s/stand-3s shuffle (user-reported stutter).
    _accumMs += diff;
    _moveAccumMs += diff;
    bool fullTick = _accumMs >= sWintergraspBotsConfig->tickMs;
    bool moveTick = _moveAccumMs >= 1000;
    if (!fullTick && !moveTick)
        return;
    _moveAccumMs = 0;
    if (fullTick)
        _accumMs = 0;

    Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
    if (!bf || !bf->IsEnabled())
        return;

    if (bf->IsWarTime())
    {
        if (!_battleActive)
            _breachStage = 0;   // fresh battle: the latch must never inherit a prior battle's stage
        _battleActive = true;
        if (fullTick)
            Reconcile();
        else
            MovementTick();
    }
    else if (_battleActive)
    {
        // Release on the 1s move-tick too (not just the 5s full tick): a back-to-back
        // battle stop/start inside one full-tick window would otherwise never observe
        // the war-off state, and the breach-stage latch (line above) needs that
        // transition to clear. ReleaseAll is idempotent and fires once (it drops
        // _battleActive).
        _battleActive = false;
        ReleaseAll();
    }
}

// Fix 1: strip any FRIENDLY target from a managed bot's combat pets. Core PetAI can acquire a
// friendly siege vehicle/player (runtime-observed: allied warlock/hunter pets destroying allied
// vehicles), and playerbots does NOT re-command pets in ordinary combat — so we clear the bad
// target directly. Enemy targets are left untouched, so pets keep DPSing real hostiles. Also
// drops an AGGRESSIVE pet to DEFENSIVE so it stops proximity-acquiring new friendly targets.
void WintergraspBotsDirector::ScrubPetTargets(Player* bot) const
{
    // Iterate EVERY controlled unit, not just GetPet()/GetGuardianPet() — the latter returns only
    // ONE guardian, so a DK's Army of the Dead (up to 8 ghouls) or a mage's mirror images would
    // leave all-but-one still hammering an ally. m_Controlled covers pet + all guardians + minions.
    for (Unit* ctrl : bot->m_Controlled)
    {
        Creature* pet = ctrl ? ctrl->ToCreature() : nullptr;   // skip charmed players
        if (!pet || !pet->IsAlive())
            continue;
        if (pet->GetReactState() == REACT_AGGRESSIVE)
            pet->SetReactState(REACT_DEFENSIVE);
        Unit* victim = pet->GetVictim();
        // Drop any target friendly to the owner (same faction) — a pet must never attack an ally.
        // IsFriendlyTo is the decisive same-team test; keep the IsValidAttackTarget guard too.
        if (victim && (bot->IsFriendlyTo(victim) || !bot->IsValidAttackTarget(victim)))
        {
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module",
                    "[WGBots] pet-scrub: {} (owner {}) dropping target {} (friendlyToOwner={} validAtk={})",
                    pet->GetName(), bot->GetName(), victim->GetName(),
                    bot->IsFriendlyTo(victim), bot->IsValidAttackTarget(victim));
            pet->AttackStop();
            pet->SetTarget(ObjectGuid::Empty);
            if (CharmInfo* ci = pet->GetCharmInfo())
                ci->SetIsCommandAttack(false);
        }
    }
}

// Mount the bot INSTANTLY via a triggered cast of its own best ground mount (Fix 7 v2). The prior
// approach (DoSpecificAction -> playerbots' normal ~1.5s interruptible cast, attempted only once
// per 5s reconcile) almost never completed in the contested zone: a lone attempt landed mid-combat
// and the cast broke on damage. A triggered cast applies the mount aura immediately and cannot be
// interrupted, so a bot that reaches here (out of combat, long open leg) mounts every time.
// Returns true if now mounted; false if the bot has no ground-mount spell (druids / mount-item
// users) so the caller can fall back to playerbots' own form/item path.
bool WintergraspBotsDirector::TryMountBot(Player* bot) const
{
    if (bot->IsMounted())
        return true;
    if (!bot->IsOutdoors())   // core forbids mounting indoors; a triggered cast would bypass that
        return false;

    uint32 best = 0;
    int32 bestSpeed = -1;
    for (auto const& entry : bot->GetSpellMap())
    {
        if (entry.second->State == PLAYERSPELL_REMOVED || !entry.second->Active)
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(entry.first);
        if (!si || si->IsPassive() || si->Effects[0].ApplyAuraName != SPELL_AURA_MOUNTED)
            continue;
        // Ground mounts only — WG is a no-fly zone (a flying mount here just roots the rider).
        if (si->Effects[1].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED ||
            si->Effects[2].ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
            continue;
        int32 speed = std::max(si->Effects[1].BasePoints, si->Effects[2].BasePoints);
        if (speed > bestSpeed)
        {
            bestSpeed = speed;
            best = entry.first;
        }
    }
    if (!best)
        return false;

    bot->CastSpell(bot, best, true);   // triggered = instant, uninterruptible
    return bot->IsMounted();
}

// Fast movement sub-tick: re-leg any traveling, idle-on-its-feet bot whose spline has ended.
// Assignment, engagement, stuck detection, and population stay on the full reconcile tick.
void WintergraspBotsDirector::MovementTick()
{
    for (int team = 0; team < 2; ++team)
        for (ObjectGuid const& g : _managed[team])
        {
            Player* p = ObjectAccessor::FindPlayer(g);
            if (!p || !p->IsInWorld() || !p->IsAlive())
                continue;
            if (p->GetZoneId() != WG_ZONE_ID)
                continue;
            ScrubPetTargets(p);   // Fix 1: keep pets off friendlies — even for seated bots
            if (p->GetVehicle())
                continue;
            // Containment runs BEFORE the combat/moving skip — a bot clipping through a wall is by
            // definition moving (and usually chasing something past it). Both teams (see the fn).
            if (EnforceBreachFrontier(p))
                continue;
            // Fast-tick vehicle intercept: a defender running alongside an attacker siege vehicle
            // must engage it NOW, not run right past for a whole 5s reconcile (user-reported).
            // Vehicle scan only (no player list on the sub-tick); gated to when siege vehicles are
            // actually in play so the extra grid scan isn't paid otherwise.
            if (_siegeVehiclesActive && TeamId(team) != _attackerTeam && !p->IsInCombat())
            {
                std::vector<Player*> const noHostiles;
                Position chase;
                if (EngageNearbyEnemy(p, noHostiles, chase) == 1)
                    continue;   // now attacking the vehicle
            }
            if (p->IsInCombat() || p->isMoving())
                continue;
            auto it = _objective.find(g);
            if (it == _objective.end())
                continue;
            StepBot(p, it->second, /*fullTick=*/false);
        }
}

TeamId WintergraspBotsDirector::WorkshopController(Battlefield* bf, uint32 areaId) const
{
    // WG's GetData(areaId) returns the workshop graveyard's controlling TeamId (BattlefieldWG.cpp:943).
    return TeamId(bf->GetData(areaId));
}

bool WintergraspBotsDirector::RelicOpen(Battlefield* bf) const
{
    // CanInteractWithRelic() is WG-only + non-const; reach it via the type tag then cast.
    if (!bf || bf->GetTypeId() != BATTLEFIELD_WG)
        return false;
    return static_cast<BattlefieldWG*>(bf)->CanInteractWithRelic();
}

int WintergraspBotsDirector::CountInZoneBots(TeamId team) const
{
    int n = 0;
    for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
    {
        Player* bot = itr.second;
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;
        if (bot->GetZoneId() != WG_ZONE_ID)
            continue;
        if (bot->GetTeamId() != team)
            continue;
        ++n;
    }
    return n;
}

// The faction's posts for this tick, in priority order, each with a headcount. Assignment is
// nearest-first per job, and the LAST job absorbs all leftovers (every list ends with the
// faction's main-effort post). Reactive by construction:
// - Relic open: everyone converges on the relic room (attackers click, defenders contest).
// - Defender with enemy siege in play: the majority rally AT the live threat cluster (scaled
//   to its size); small pickets hold owned workshops (retake them if all are lost —
//   workshops feed the enemy's vehicle cap).
// - Attacker: small squads capture missing workshops (vehicle cap + rank); the rest push the
//   keep alongside the siege, advancing as walls fall (WG_ATTACKER_PUSH).
std::vector<WintergraspBotsDirector::WgJob> WintergraspBotsDirector::BuildJobs(Battlefield* bf, TeamId team, int aliveCount,
                                                                               std::vector<Position> const& convoy) const
{
    std::vector<WgJob> jobs;
    bool isAttacker = (team == bf->GetAttackerTeam());

    if (RelicOpen(bf))
    {
        jobs.push_back({ WG_RELIC_ANCHOR, aliveCount });
        return jobs;
    }

    if (!sWintergraspBotsConfig->workshopCapture)
    {
        jobs.push_back({ isAttacker ? WG_ATTACKER_PUSH[_breachStage] : WG_DEFENDER_ANCHOR, aliveCount });
        return jobs;
    }

    if (isAttacker)
    {
        // Vehicle production: Lieutenants only — anyone else standing at a factory is wasted.
        uint32 myVeh = bf->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_VEHICLE_A
                                                         : BATTLEFIELD_WG_DATA_VEHICLE_H);
        uint32 myMax = bf->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A
                                                         : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H);
        bool wantVehicles = myVeh + sWintergraspBotsConfig->humanReserve < myMax;
        bool rankPhase = _state.lieutenants < _state.lieutenantsWanted;

        // Split into two passes so ALL production jobs are pushed before ANY capture job — a
        // WG_RANK_ANY capture job earlier in the list could otherwise draft the Lieutenant a
        // later factory needs.
        for (WorkshopDef const& w : WG_WORKSHOPS)
            if (WorkshopController(bf, w.areaId) == team && wantVehicles)
                jobs.push_back({ Position(w.x, w.y, w.z, 0.0f), 2, WG_RANK_LIEUTENANT, WG_JOB_PRODUCTION });
        for (WorkshopDef const& w : WG_WORKSHOPS)
            if (WorkshopController(bf, w.areaId) != team)
            {
                // Capture squads double as the RANK-UP play: the garrison squad guarding an
                // enemy workshop is exactly the promotion fodder, so while the Lieutenant
                // census is short the squads go in bigger and rankless-first. Post-rank
                // squads are knob-sized (3 stragglers trickling back into an 8-guard
                // garrison die retail — production-observed). ALL capture squads
                // (rank-phase included) surge +2 while the attacker holds NO workshop:
                // that starves vehicle production outright.
                int want = rankPhase ? 4 : int(sWintergraspBotsConfig->captureSquad);
                if (_controlled[team] == 0)
                    want += 2;
                jobs.push_back({ Position(w.x, w.y, w.z, 0.0f), want,
                                 rankPhase ? WG_RANK_RANKLESS : WG_RANK_ANY, WG_JOB_GENERIC });
            }

        // Road-post hunts: more promotion camps while ranking (posts 4-6 are the road posts).
        if (rankPhase)
            for (uint8 i = WG_ROAD_POST_FIRST; i < WG_GARRISON_POST_COUNT; ++i)
                if (WorkshopController(bf, WG_GARRISON_POSTS[i].areaId) != team)
                    jobs.push_back({ Position(WG_GARRISON_POSTS[i].slots[0][0],
                                              WG_GARRISON_POSTS[i].slots[0][1],
                                              WG_GARRISON_POSTS[i].zSeed, 0.0f),
                                     3, WG_RANK_RANKLESS, WG_JOB_GENERIC });

        // Escorts: infantry fire teams run WITH the siege convoy (its jobs track the vehicles'
        // live positions each tick) — they screen the engines from sortieing defenders.
        int escorts = 0;
        for (Position const& v : convoy)
        {
            if (escorts >= 2)
                break;
            jobs.push_back({ v, 3, WG_RANK_ANY, WG_JOB_GENERIC });
            ++escorts;
        }

        // Main effort. WITH a live siege vanguard, mass WHERE THE SIEGE IS (advancing with it).
        // WITHOUT any vehicle, massing at the keep gate is pointless — the walls can't fall
        // without siege — so leave only a small screen there and push the MASS onto the
        // workshops that gate vehicle production (Fix 3: split effort).
        if (_state.hasVanguard)
        {
            jobs.push_back({ _state.vanguard, aliveCount, WG_RANK_ANY, WG_JOB_GENERIC });
        }
        else
        {
            jobs.push_back({ WG_ATTACKER_PUSH[_breachStage], int(sWintergraspBotsConfig->keepScreen),
                             WG_RANK_ANY, WG_JOB_GENERIC });

            // Leftovers (last job = the absorber) push the workshops: first unclaimed
            // (retake -> restart production), else hold an owned one, else the keep anchor.
            Position field = WG_ATTACKER_PUSH[_breachStage];
            bool found = false;
            for (WorkshopDef const& w : WG_WORKSHOPS)
                if (WorkshopController(bf, w.areaId) != team)
                {
                    field.Relocate(w.x, w.y, w.z, 0.0f);
                    found = true;
                    break;
                }
            if (!found)
                for (WorkshopDef const& w : WG_WORKSHOPS)
                    if (WorkshopController(bf, w.areaId) == team)
                    {
                        field.Relocate(w.x, w.y, w.z, 0.0f);
                        break;
                    }
            jobs.push_back({ field, aliveCount, WG_RANK_ANY, WG_JOB_GENERIC });
        }
    }
    else
    {
        // Gatehouse cannon crews: only while the keep is actually threatened, capped, and
        // typed CREW so ONLY these assignees seat cannons (fixes turret squatting). A 0 max
        // disables crews outright; otherwise at least 1 gunner per post even at max=1.
        bool keepThreat = _state.threatCount > 0 || _state.wallUnderFire;
        if (keepThreat && sWintergraspBotsConfig->turretCrewMax > 0)
            for (Position const& post : WG_CANNON_POSTS)
                jobs.push_back({ post, std::max(1, int(sWintergraspBotsConfig->turretCrewMax / 2)),
                                 WG_RANK_ANY, WG_JOB_CREW });

        // Rally to the LIVE threat: mass at the enemy vehicle cluster, scaled to its size —
        // never a fixed line. Shelling with no visible shooter -> rally at the wall under fire.
        // Muster-then-sortie: un-released waves gather at the interior muster point instead
        // of dribbling out the single rampart exit solo (see UpdateSortie).
        bool mustering = sWintergraspBotsConfig->sortieQuorum > 0 && !_sortieReleased;
        if (_state.threatCount > 0)
            jobs.push_back({ mustering ? WG_MUSTER : _state.threat, RallyWant(aliveCount),
                             WG_RANK_ANY, WG_JOB_GENERIC });
        else if (_state.wallUnderFire)
            jobs.push_back({ mustering ? WG_MUSTER : _state.firedWall, 6, WG_RANK_ANY, WG_JOB_GENERIC });

        // Field work: always contest up to two lost workshops; picket the ones we hold.
        int retakes = 0;
        for (WorkshopDef const& w : WG_WORKSHOPS)
            if (WorkshopController(bf, w.areaId) != team && retakes < 2)
            {
                jobs.push_back({ Position(w.x, w.y, w.z, 0.0f), 2, WG_RANK_ANY, WG_JOB_GENERIC });
                ++retakes;
            }
        for (WorkshopDef const& w : WG_WORKSHOPS)
            if (WorkshopController(bf, w.areaId) == team)
                jobs.push_back({ Position(w.x, w.y, w.z, 0.0f), 2, WG_RANK_ANY, WG_JOB_GENERIC });

        // Small keep watch always stays home.
        jobs.push_back({ WG_DEFENDER_ANCHOR, 3, WG_RANK_ANY, WG_JOB_GENERIC });

        // Main effort (absorbs leftovers): the rally while threatened; otherwise push the
        // FIELD — mass on the first enemy-held workshop, or stand watch if we hold everything.
        // NOTE: one vehicle oscillating across ThreatRadius re-targets these leftovers between
        // the rally and the field push each tick — same seam as the gunner cycle; debounce
        // here if it ever reads as ping-ponging in game.
        if (keepThreat && _state.threatCount > 0)
            jobs.push_back({ mustering ? WG_MUSTER : _state.threat, aliveCount, WG_RANK_ANY, WG_JOB_GENERIC });
        else
        {
            // Deliberate SWARM: all leftovers push ONE enemy workshop (mass to retake it,
            // then the next flip re-targets them) rather than spreading thin across the map.
            Position fieldPush = WG_DEFENDER_ANCHOR;
            for (WorkshopDef const& w : WG_WORKSHOPS)
                if (WorkshopController(bf, w.areaId) != team)
                {
                    fieldPush.Relocate(w.x, w.y, w.z, 0.0f);
                    break;
                }
            jobs.push_back({ fieldPush, aliveCount, WG_RANK_ANY, WG_JOB_GENERIC });
        }
    }

    return jobs;
}

// Idle-combat layer: WG siege vehicles besiege BUILDINGS, so they generate no threat against any
// player — and playerbots' target selection is threat-based, so idle defenders stand and watch a
// demolisher pound the keep (runtime-observed: 5 bots sitting beside an engine on the last door).
// The same threat blindness applies to enemy PLAYERS who aren't hitting the bot itself: one
// killed a friendly tower cannon while a defender squad stood and watched (user-reported), so
// the scan covers hostile players too — WG is open war, any enemy in range is a fight.
// The attack order uses the same sequence as playerbots' own AttackAction: "prioritized
// targets" keeps a no-threat target valid, "current target" + selection + Attack() start the
// fight, and the combat engine's normal DPS/reach logic takes over.
// Returns 0 = nothing around, 1 = attack ordered (target in radius with line of sight),
// 2 = chase (`chase` = a vehicle within DOUBLE radius but out of LOS — e.g. shelling the wall
// the bot stands behind; the caller walks the bot toward it and the next tick's LOS pass
// converts the chase into an attack). Without the chase tier, bots stand oblivious beside a
// wall that is being bombarded (runtime-observed).
int WintergraspBotsDirector::EngageNearbyEnemy(Player* bot, std::vector<Player*> const& hostiles,
                                               Position& chase) const
{
    float radius = sWintergraspBotsConfig->engageVehicleRadius;
    if (radius <= 0.0f)
        return 0;

    // Ranged bots also engage hostile manned TOWER CANNONS — the turrets sit ~30y up where
    // melee can't reach, but casters/hunters can burn them down from the ground.
    std::vector<uint32> entries(std::begin(WG_GROUND_VEHICLES), std::end(WG_GROUND_VEHICLES));
    if (!PlayerbotAI::IsMelee(bot))
        entries.push_back(NPC_WINTERGRASP_TOWER_CANNON);

    Unit* bestLos = nullptr;          // nearest in-LOS non-ground target (tower cannon or player)
    Unit* bestGroundVeh = nullptr;    // nearest in-LOS hostile GROUND siege vehicle — absolute priority
    Creature* bestAny = nullptr;
    float bestLosDist = radius + 1.0f;
    float bestGroundDist = radius + 1.0f;
    float bestAnyDist = radius * 2.0f + 1.0f;
    for (uint32 entry : entries)
    {
        Creature* c = bot->FindNearestCreature(entry, radius * 2.0f);
        if (!c || !c->IsAlive())
            continue;
        if (!bot->IsValidAttackTarget(c))   // hostile-only; own faction's vehicles are friendly
            continue;
        bool isCannon = entry == NPC_WINTERGRASP_TOWER_CANNON;
        float d = bot->GetDistance(c);
        // Tower cannons are engaged only from INSIDE firing range (~35y): for anything
        // farther the combat engine repositions toward the turret, and the navmesh routes
        // that chase straight through the intact gate below it — destructible walls/doors
        // aren't in the mesh (runtime-observed: attackers walking through the closed
        // fortress door and up the gatehouse ramp after engaging a wall-top turret).
        float losRange = isCannon ? std::min(radius, 35.0f) : radius;
        if (d <= losRange && bot->IsWithinLOSInMap(c))
        {
            // Ground siege vehicles get ABSOLUTE priority — they break the keep and nothing else
            // will ever target them (they threaten no one). Tower cannons DO damage players, so
            // they compete normally (nearest-in-LOS) against enemy players below.
            if (!isCannon)
            {
                if (d < bestGroundDist) { bestGroundVeh = c; bestGroundDist = d; }
            }
            else if (d < bestLosDist) { bestLos = c; bestLosDist = d; }
        }
        // Chase tier is for GROUND vehicles only — a tower cannon sits ~30y up on a tower top
        // with no walkable route to it, so chasing one just pins the bot (engage on LOS only).
        if (!isCannon && d < bestAnyDist)
        {
            bestAny = c;
            bestAnyDist = d;
        }
    }

    // Enemy players on foot (drivers are skipped — their vehicle is the target and is covered
    // above). Skipped entirely when a ground siege vehicle is in LOS (that takes absolute
    // priority); otherwise a player competes nearest-in-LOS against any tower cannon found above.
    if (!bestGroundVeh)
        for (Player* h : hostiles)
        {
            if (!h->IsAlive() || h->GetVehicle() || !bot->IsValidAttackTarget(h))
                continue;
            float d = bot->GetDistance(h);
            if (d <= radius && d < bestLosDist && bot->IsWithinLOSInMap(h))
            {
                bestLos = h;
                bestLosDist = d;
            }
        }

    Unit* target = bestGroundVeh ? bestGroundVeh : bestLos;
    if (target)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (!ai)
            return 0;

        AiObjectContext* ctx = ai->GetAiObjectContext();
        ctx->GetValue<GuidVector>("prioritized targets")->Set({ target->GetGUID() });
        ctx->GetValue<Unit*>("current target")->Set(target);
        bot->SetSelection(target->GetGUID());
        ai->ChangeEngine(BOT_STATE_COMBAT);
        bot->Attack(target, bot->IsWithinMeleeRange(target) || PlayerbotAI::IsMelee(bot));

        if (sWintergraspBotsConfig->debug)
            LOG_INFO("module", "[WGBots] engage {} -> {} ({:.0f}y)", bot->GetName(),
                     target->GetName(), bestGroundVeh ? bestGroundDist : bestLosDist);
        return 1;
    }

    if (bestAny)
    {
        chase.Relocate(bestAny->GetPositionX(), bestAny->GetPositionY(), bestAny->GetPositionZ(), 0.0f);
        return 2;
    }
    return 0;
}

// Anti-clip containment. The keep's destructible gate/walls are dynamic gameobjects and are NOT
// in the navmesh: director-issued legs are wall-LOS-truncated and respect them, but any NATIVE
// move (playerbots combat movement closing on an engaged target, threat fight-back chase) paths
// straight through the closed fortress door (runtime-observed: attackers walking through the
// intact gate and up the gatehouse ramp). This backstop catches an attacker on foot past the
// current breach frontier within ~1s of crossing, drops its fight so the chase doesn't
// immediately re-enter, and snaps it back to the stage push anchor — at that latency the bot is
// only a few yards inside, so the correction reads as bumping off the door.
bool WintergraspBotsDirector::EnforceBreachFrontier(Player* bot)
{
    if (_relicOpen)   // last door down: the whole keep is legitimately open
        return false;

    float x = bot->GetPositionX();
    float y = bot->GetPositionY();
    bool offAxis = std::fabs(y - 2841.0f) > WG_KEEP_CORRIDOR_HW;   // off the central breach corridor
    bool attacker = (bot->GetTeamId() == _attackerTeam);
    Position snapTo;

    if (attacker)
    {
        // Attackers: the whole in-keep breach path is the central corridor, and they may only be
        // as deep as the current breach stage allows. Being INSIDE the keep either off-corridor
        // (clipped a flanking wall) or past the open depth (clipped/ran through a standing wall on
        // the axis line) is illegal. The x-line alone let off-axis clips through into a "legal"
        // x-band; the corridor gate closes that.
        if (!IsInsideKeep(x, y))
            return false;
        float openX = _breachStage >= 2 ? WG_LASTDOOR_X : _breachStage >= 1 ? WG_MIDWALL_X : 0.0f;
        if (!offAxis && x <= openX)
            return false;   // in the corridor and not past the open depth: legal
        snapTo = WG_ATTACKER_PUSH[_breachStage];
    }
    else
    {
        // Defenders legitimately hold off-axis interior spots (keep cannon posts at x~5178-5190),
        // so they are NOT corridor-contained. Only correct an off-axis CROSSING of a still-standing
        // interior wall LINE — the mid wall (until stage 2) and the last door — both far from the
        // cannon posts, so those are never touched. This is the "running through the inner walls"
        // case. (The fortress line is skipped for defenders: the cannon posts sit within its band.)
        // The y-span bound keeps this from reaching the off-axis defender portals past the wall ends.
        bool inWallSpan   = offAxis && std::fabs(y - 2841.0f) < WG_WALL_Y_HALFSPAN;
        bool crossingMid  = _breachStage < 2 && std::fabs(x - WG_MIDWALL_X)  < WG_WALL_CLIP_BAND;
        bool crossingLast =                      std::fabs(x - WG_LASTDOOR_X) < WG_WALL_CLIP_BAND;
        if (!inWallSpan || (!crossingMid && !crossingLast))
            return false;
        snapTo = WG_DEFENDER_ANCHOR;   // inner, on-axis: a legal defender spot to re-route from
    }

    bot->CombatStop(true);
    bot->SetSelection(ObjectGuid::Empty);
    if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
    {
        AiObjectContext* ctx = ai->GetAiObjectContext();
        ctx->GetValue<GuidVector>("prioritized targets")->Set(GuidVector());
        ctx->GetValue<Unit*>("current target")->Set(nullptr);
        ai->ChangeEngine(BOT_STATE_NON_COMBAT);
    }
    bot->GetMotionMaster()->Clear();
    bot->TeleportTo(WG_MAP_ID, snapTo.GetPositionX(), snapTo.GetPositionY(),
                    snapTo.GetPositionZ(), snapTo.GetOrientation());
    _travel.erase(bot->GetGUID());
    if (sWintergraspBotsConfig->debug)
        LOG_INFO("module", "[WGBots] frontier: pulled {} {} back off a standing wall (stage {})",
                 bot->GetName(), attacker ? "(atk)" : "(def)", _breachStage);
    return true;
}

// Bay-wedge watchdog. Driven vehicles have NO native stuck handling — StepBot skips seated
// bots — so a demolisher wedged leaving its workshop bay stays wedged for the whole battle
// and starves the faction vehicle cap (runtime-observed: permanent pile-up in one bay).
// Only vehicles that OUGHT to be traveling are watched: parked-and-firing at a wall is
// legal, and the strategy's dry-fire feedback owns that case. The no-watch radius must sit
// ABOVE the strategy's 45y fire gate and its ~32y dry-fire reposition ring (both in
// WintergraspSiegeStrategy.cpp — keep in sync) but tight enough that a vehicle wedged on
// the mid-range approach still has an owner; 60y covers both. Workshops are far outside it,
// so bay wedges always qualify. Sampled every reconcile (tickMs).
void WintergraspBotsDirector::WatchVehicle(Player* bot, Unit* veh)
{
    for (uint32 entry : WG_AXIS_STRUCTURES)
        if (GameObject* g = bot->FindNearestGameObject(entry, 60.0f))
            if (g->GetGOValue()->Building.Health > 0)
            {
                _vehTravel.erase(bot->GetGUID());
                return;
            }

    TravelState& ts = _vehTravel[bot->GetGUID()];
    if (ts.has && veh->GetExactDist2d(&ts.last) < sWintergraspBotsConfig->stuckEpsilon)
        ts.stuckMs += sWintergraspBotsConfig->tickMs;
    else
        ts.stuckMs = 0;
    ts.last = veh->GetPosition();
    ts.has = true;
    if (ts.stuckMs < sWintergraspBotsConfig->stuckSeconds * 1000)
        return;
    ts.stuckMs = 0;

    // Stop-short teleport toward the keep approach — first candidate angle with valid ground
    // and a wall-clear line wins (mirrors the battle-tested foot unstick).
    float bx = veh->GetPositionX(), by = veh->GetPositionY();
    // = the fork strategy's WG_APPROACH_X/Y (WintergraspSiegeStrategy.cpp) — keep in sync
    float angBase = std::atan2(2841.0f - by, 5100.0f - bx);
    // Escape fan: keep-bearing first, widening with each failed attempt (unstickHolds) —
    // a bay wedge can have its direct line blocked; ±90° and the reverse bearing cover it.
    std::vector<float> fan = { 0.0f, float(M_PI) / 4.0f, -float(M_PI) / 4.0f };
    if (ts.unstickHolds >= 1)
    {
        fan.push_back(float(M_PI) / 2.0f);
        fan.push_back(-float(M_PI) / 2.0f);
    }
    if (ts.unstickHolds >= 2)
        fan.push_back(float(M_PI));
    for (float off : fan)
    {
        float nx = bx + std::cos(angBase + off) * 25.0f;
        float ny = by + std::sin(angBase + off) * 25.0f;
        float nz = veh->GetMap()->GetHeight(veh->GetPhaseMask(), nx, ny, veh->GetPositionZ() + 30.0f);
        if (nz <= INVALID_HEIGHT + 1.0f)
            continue;
        if (!veh->GetMap()->isInLineOfSight(bx, by, veh->GetPositionZ() + 2.0f, nx, ny, nz + 2.0f,
                                            veh->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO,
                                            VMAP::ModelIgnoreFlags::Nothing))
            continue;
        if (sWintergraspBotsConfig->debug)
            LOG_INFO("module", "[WGBots] vehicle unstick: {} ({:.0f},{:.0f}) -> ({:.0f},{:.0f})",
                     bot->GetName(), bx, by, nx, ny);
        veh->NearTeleportTo(nx, ny, nz + 0.5f, veh->GetOrientation());
        ts.unstickHolds = 0;
        return;
    }

    // All candidates blocked: remember the failure so the next cycle widens the fan, and log
    // it so verification can see a wedge the watchdog cannot yet clear.
    ++ts.unstickHolds;
    if (sWintergraspBotsConfig->debug)
        LOG_INFO("module", "[WGBots] vehicle unstick: {} at ({:.0f},{:.0f}) — no clear candidate (attempt {})",
                 bot->GetName(), bx, by, ts.unstickHolds);
}

// Fix 2b: an attacker foot bot within RecrewRadius of an ALIVE, UNMANNED, friendly ground siege
// vehicle seats into it (remote spell-click, like the cannon crews) so an abandoned engine becomes
// active siege again at zero vehicle-cap cost. Returns true if seated. Any rank may drive — the
// siege strategy fires on IsInVehicle(true), not on the Lieutenant aura.
bool WintergraspBotsDirector::TryRecrewVehicle(Player* bot) const
{
    float radius = sWintergraspBotsConfig->recrewRadius;
    if (radius <= 0.0f)
        return false;
    for (uint32 entry : WG_GROUND_VEHICLES)
    {
        Creature* v = bot->FindNearestCreature(entry, radius);
        if (!v || !v->IsAlive() || !v->IsFriendlyTo(bot))
            continue;
        Vehicle* kit = v->GetVehicleKit();
        if (!kit || !kit->GetAvailableSeatCount())
            continue;
        if (kit->GetPassenger(0))   // driver seat already crewed
            continue;
        v->HandleSpellClick(bot);
        if (bot->IsOnVehicle(v))
        {
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module", "[WGBots] {} re-crews idle {} at ({:.0f},{:.0f})",
                         bot->GetName(), v->GetName(), v->GetPositionX(), v->GetPositionY());
            return true;
        }
    }
    return false;
}

// Seat an idle defender at a cannon post into a free friendly keep tower cannon nearby. The
// turret sits ~30y above the post on the tower top (unreachable by pathing), so the seat click
// is remote — the stand-in for a player climbing the tower stairs. Firing comes from the wg
// siege strategy's in-vehicle "fire cannon" trigger + MaintainCannonTarget below.
bool WintergraspBotsDirector::TrySeatCannon(Player* bot) const
{
    Creature* cannon = bot->FindNearestCreature(NPC_WINTERGRASP_TOWER_CANNON, 60.0f);
    if (!cannon || !cannon->IsAlive() || !cannon->IsFriendlyTo(bot))
        return false;
    if (!cannon->GetVehicleKit() || !cannon->GetVehicleKit()->GetAvailableSeatCount() ||
        cannon->GetVehicleKit()->IsVehicleInUse())
        return false;

    cannon->HandleSpellClick(bot);
    if (!bot->IsOnVehicle(cannon))
        return false;

    if (sWintergraspBotsConfig->debug)
        LOG_INFO("module", "[WGBots] seated {} in tower cannon at ({:.0f},{:.0f})",
                 bot->GetName(), cannon->GetPositionX(), cannon->GetPositionY());
    return true;
}

// A bot manning a keep cannon needs its "current target" curated every tick — the strategy's
// fire-cannon action shoots at that value, and nothing else will ever pick a siege vehicle
// (vehicles threaten nobody, so playerbots' threat-based targeting is blind to them).
bool WintergraspBotsDirector::MaintainCannonTarget(Player* bot) const
{
    Unit* vb = bot->GetVehicleBase();
    if (!vb || vb->GetEntry() != NPC_WINTERGRASP_TOWER_CANNON)
        return false;

    Creature* best = nullptr;
    float bestDist = 121.0f;   // the fire-cannon action range is 120y
    for (uint32 entry : WG_GROUND_VEHICLES)
    {
        Creature* c = bot->FindNearestCreature(entry, 120.0f);
        if (!c || !c->IsAlive() || !bot->IsValidAttackTarget(c))
            continue;
        float d = bot->GetDistance(c);
        if (d < bestDist)
        {
            best = c;
            bestDist = d;
        }
    }
    if (!best)
        return false;

    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    if (!ai)
        return false;

    AiObjectContext* ctx = ai->GetAiObjectContext();
    ctx->GetValue<GuidVector>("prioritized targets")->Set({ best->GetGUID() });
    ctx->GetValue<Unit*>("current target")->Set(best);
    bot->SetSelection(best->GetGUID());
    return true;
}

// Maintain the field garrison posts (see WG_GARRISON_POSTS): raise the controlling faction's
// squad at each post, flip it on capture, leave casualties dead until the flip.
void WintergraspBotsDirector::UpdateGarrison(Battlefield* bf)
{
    if (!sWintergraspBotsConfig->fieldGarrison)
        return;

    Map* map = sMapMgr->FindMap(WG_MAP_ID, 0);
    if (!map)
        return;

    for (uint8 i = 0; i < WG_GARRISON_POST_COUNT; ++i)
    {
        GarrisonPostDef const& post = WG_GARRISON_POSTS[i];
        TeamId owner = WorkshopController(bf, post.areaId);
        if (owner != TEAM_ALLIANCE && owner != TEAM_HORDE)
            continue;

        std::vector<ObjectGuid>& squad = _garrison[i];

        // Same owner: leave the squad as-is (alive or fallen) — it only re-raises on a flip.
        auto ownerItr = _garrisonOwner.find(i);
        if (ownerItr != _garrisonOwner.end() && ownerItr->second == owner)
            continue;

        // Resolve through the MAP, never Battlefield::GetCreature — the core assigns BfMap
        // once at server startup when Northrend isn't loaded yet, so bf->GetCreature() is a
        // silent nullptr for the server's whole lifetime (runtime-verified: every tracked
        // despawn no-opped; only the audit's live pointers ever cleaned anything).
        for (ObjectGuid const& g : squad)
            if (Creature* c = map->GetCreature(g))
                RemoveGuard(c);
        squad.clear();

        uint32 entry = owner == TEAM_HORDE ? WG_NPC_GUARD_H : WG_NPC_GUARD_A;
        Creature* refGuard = nullptr;
        for (uint8 k = 0; k < post.count; ++k)
        {
            float x = post.slots[k][0];
            float y = post.slots[k][1];
            float z = map->GetHeight(PHASEMASK_NORMAL, x, y, post.zSeed + 10.0f, true, 60.0f);
            if (z <= INVALID_HEIGHT + 1.0f)
                z = post.zSeed;
            if (Creature* c = bf->SpawnCreature(entry, x, y, z, 0.0f, owner))
            {
                // Battlefield::SpawnCreature makes a RAW creature that keeps its template
                // respawn timer — killed guards were RESPAWNING minutes later as untracked
                // strays (the true engine behind the guard accumulation the user reported).
                // A week-long delay disables that for any battle's lifetime; corpses linger
                // a few minutes (dead-stay-dead look), then never return.
                c->SetRespawnDelay(WEEK);
                c->SetCorpseDelay(5 * MINUTE);
                squad.push_back(c->GetGUID());
                if (!refGuard)
                    refGuard = c;
            }
        }
        _garrisonOwner[i] = owner;

        // Spawn audit: the capture-flip path leaks guards (runtime-confirmed: 13 strays over
        // two battles) — force-clean anything on this post's slots that isn't the squad just
        // raised. The per-stray log stays UNGATED on purpose: strays should now be rare, and
        // this line is the only signal if the leak ever recurs.
        if (refGuard)
        {
            std::list<Creature*> strays;
            refGuard->GetCreatureListWithEntryInGrid(strays,
                std::vector<uint32>{ WG_NPC_GUARD_A, WG_NPC_GUARD_H }, WG_GARRISON_AUDIT_SEARCH);
            for (Creature* s : strays)
            {
                if (std::find(squad.begin(), squad.end(), s->GetGUID()) != squad.end())
                    continue;
                bool onSlot = false;
                for (uint8 k = 0; k < post.count && !onSlot; ++k)
                    onSlot = s->GetExactDist2d(post.slots[k][0], post.slots[k][1]) <= WG_GARRISON_AUDIT_SLOT_RANGE;
                if (!onSlot)
                    continue;
                RemoveGuard(s);
                LOG_INFO("module", "[WGBots] garrison audit: removed stray guard at post {}", i);
            }
        }

        if (sWintergraspBotsConfig->debug)
            LOG_INFO("module", "[WGBots] garrison post {} raised for team {} ({} guards at {:.0f},{:.0f})",
                     i, int(owner), squad.size(), post.slots[0][0], post.slots[0][1]);
    }
}

// Move one bot toward its assigned objective under normal movement. Teleport ONLY when the bot
// is genuinely stuck (traveling, not fighting, no forward progress for StuckSeconds) — and the
// defender portal click, which is the game's own re-entry mechanic.
void WintergraspBotsDirector::StepBot(Player* bot, Position const& tgt, bool fullTick, bool holdStill)
{
    TravelState& ts = _travel[bot->GetGUID()];

    // In combat: let the WG PvP loadout drive movement; not "traveling", so reset the stuck timer.
    if (bot->IsInCombat())
    {
        ts.stuckMs = 0;
        ts.has = false;
        return;
    }

    // Arrived: stop and hold (let workshop capture tick / stand the defensive line).
    if (bot->GetExactDist2d(tgt.GetPositionX(), tgt.GetPositionY()) <= sWintergraspBotsConfig->arriveRadius)
    {
        ts.stuckMs = 0;
        ts.unstickHolds = 0;
        ts.has = false;

        // Patrol + fidget: a held post is walked, not statued. Every dwell period (staggered
        // per bot so squads don't march in step) the bot strolls a short ring point around
        // the objective and drifts back. Crews/production stand at their task (holdStill).
        float r = sWintergraspBotsConfig->patrolRadius;
        if (fullTick && !holdStill && r > 0.0f && !bot->isMoving() && !bot->HasUnitState(UNIT_STATE_CASTING))
        {
            ts.dwellMs += sWintergraspBotsConfig->tickMs;
            uint32 dwell = 8000 + (bot->GetGUID().GetCounter() % 5) * 2000;   // 8-16s
            if (ts.dwellMs >= dwell)
            {
                ts.dwellMs = 0;
                static float const DIR[4][2] = { {1,0}, {0,1}, {-1,0}, {0,-1} };
                uint8 k = uint8(ts.patrolIdx++ + bot->GetGUID().GetCounter()) % 4;
                Position pat(tgt.GetPositionX() + DIR[k][0] * r,
                             tgt.GetPositionY() + DIR[k][1] * r, tgt.GetPositionZ(), 0.0f);
                Position leg;
                bool trunc = false;
                if (ComputeLegWaypoint(bot, pat, leg, trunc) && !trunc)
                    bot->GetMotionMaster()->MovePoint(WG_MOVE_ID, leg.GetPositionX(),
                                                      leg.GetPositionY(), leg.GetPositionZ());
            }
        }
        return;
    }

    // Mid-cast (mount-up, drink): a new move order would interrupt it and the retry cycle reads
    // as stutter-walking. Let the cast finish; the next tick moves the bot (mounted, ideally).
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    // Mount for long open-field travel (Fix 7). The WG combat loadout keeps bots out of playerbots'
    // autonomous mount path (they hold a pvp target / combat state), and the director's continuously
    // re-issued MovePoint prevents a mount from ever casting on its own — so mount explicitly on a
    // long, out-of-combat leg via playerbots' own selection (it picks the bot's ground mount; WG is
    // a no-fly zone). Dismount is left to playerbots (its combat engine dismounts on engage). Bot is
    // already known out of combat (StepBot's early return) and not casting (just checked above).
    if (fullTick && !bot->IsMounted() && !holdStill &&
        bot->GetExactDist2d(tgt.GetPositionX(), tgt.GetPositionY()) > WG_MOUNT_MIN_DIST)
    {
        if (bot->isMoving())
            bot->StopMoving();   // stop first; the MovePoint below re-issues travel already mounted

        // Instant, uninterruptible ground mount from the bot's own spellbook. Guaranteed to land
        // because we only reach here out of combat (StepBot's early return) — no cast window for
        // battle damage to break, so it works on the first attempt instead of ~1-in-5.
        bool mounted = TryMountBot(bot);
        if (!mounted)
        {
            if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
            {
                // No SPELL_AURA_MOUNTED spell (druid travel form, mount-item users) — fall back to
                // playerbots' own selection, which handles forms/items. Interruptible, but it's the
                // only path for those and they're a minority.
                ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(nullptr);
                ai->ChangeEngine(BOT_STATE_NON_COMBAT);
                ai->DoSpecificAction("check mount state");
                mounted = bot->IsMounted();
            }
        }
        if (sWintergraspBotsConfig->debug)
            LOG_INFO("module", "[WGBots] mount {} d2d={:.0f} -> mounted={} casting={}",
                     bot->GetName(), bot->GetExactDist2d(tgt.GetPositionX(), tgt.GetPositionY()),
                     mounted, bot->HasUnitState(UNIT_STATE_CASTING));
        // Mounted (or the fallback cast is mid-flight): don't re-issue movement this tick. Next tick
        // travels the bot mounted; the sub-tick casting guard protects an in-progress fallback cast.
        if (mounted || bot->HasUnitState(UNIT_STATE_CASTING))
            return;
    }

    // Traveling: (re)issue the move so playerbots' idle-AI can't silently clear it. Travel is
    // issued as <=WG_MAX_LEG walkable legs (see ComputeLegWaypoint) — the reassert tick chains
    // legs until the arrive check above stops the bot at the objective.
    if (sWintergraspBotsConfig->reassertMove || !ts.has || !bot->isMoving())
    {
        Position leg;
        bool truncated = false;
        bool haveLeg = ComputeLegWaypoint(bot, tgt, leg, truncated);

        // Defender crossing the keep boundary with no route through: use the keep's REAL exits,
        // never a teleport (the old gate hop read as bots blinking to the front of the keep —
        // user-reported). Outbound: run up the gatehouse rampart and JUMP off the wall (the
        // keep's designed defender exit; the drop is a scripted MoveJump because the mesh has
        // no jump-down links). Inbound: run to the nearest Defender's Portal spawn and take it
        // (the game's own mechanic — the port fires only within click range of the portal).
        // Rampart jump is PROXIMITY-keyed, not box-keyed: the rampart tier (x~5156) sits just
        // OUTSIDE the IsInsideKeep box (x>5160), so a box-gated jump disengages at the top of
        // the ramp and the bot runs back down (runtime-observed loop). Any defender on the
        // tier whose objective is out west simply jumps.
        if (bot->GetTeamId() != _attackerTeam &&
            bot->GetPositionZ() > 412.0f && bot->GetExactDist2d(&WG_RAMP_TOP) <= 10.0f &&
            !IsInsideKeep(tgt.GetPositionX(), tgt.GetPositionY()))
        {
            bot->GetMotionMaster()->MoveJump(WG_JUMP_LANDING.GetPositionX(),
                                             WG_JUMP_LANDING.GetPositionY(),
                                             WG_JUMP_LANDING.GetPositionZ(), 24.0f, 10.0f);
            ts.stuckMs = 0;
            ts.has = false;
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module", "[WGBots] {} jumps off the rampart", bot->GetName());
            return;
        }

        if ((!haveLeg || truncated) && bot->GetTeamId() != _attackerTeam)
        {
            bool botInside = IsInsideKeep(bot->GetPositionX(), bot->GetPositionY());
            bool tgtInside = IsInsideKeep(tgt.GetPositionX(), tgt.GetPositionY());
            bool botEast = bot->GetPositionX() > WG_MIDWALL_X;
            bool tgtEast = tgt.GetPositionX() > WG_MIDWALL_X;
            if (botInside && botEast != tgtEast)
            {
                // Crossing the interior mid wall (either direction; also the first stage of the
                // rampart exit): align with the arch axis on this side, then cross.
                Position const& align = botEast ? WG_ARCH_E : WG_ARCH_W;
                Position const& cross = botEast ? WG_ARCH_W : WG_ARCH_E;
                Position const& via = std::fabs(bot->GetPositionY() - 2841.0f) > 8.0f ? align : cross;
                haveLeg = ComputeLegWaypoint(bot, via, leg, truncated);
            }
            else if (botInside && !tgtInside)
            {
                haveLeg = ComputeLegWaypoint(bot, WG_RAMP_TOP, leg, truncated);
            }
            else if (!botInside && tgtInside)
            {
                Position const* portal = nullptr;
                float best = 100000.0f;
                for (Position const& p : WG_DEFENDER_PORTALS)
                {
                    float d = bot->GetExactDist2d(&p);
                    if (d < best)
                    {
                        best = d;
                        portal = &p;
                    }
                }
                if (portal && best <= 5.0f)
                {
                    bot->TeleportTo(WG_MAP_ID, WG_PORTAL_DEST.GetPositionX(), WG_PORTAL_DEST.GetPositionY(),
                                    WG_PORTAL_DEST.GetPositionZ(), WG_PORTAL_DEST.GetOrientation());
                    ts.stuckMs = 0;
                    ts.has = false;
                    if (sWintergraspBotsConfig->debug)
                        LOG_INFO("module", "[WGBots] {} takes the defender portal into the keep", bot->GetName());
                    return;
                }
                if (portal)
                    haveLeg = ComputeLegWaypoint(bot, *portal, leg, truncated);
            }
        }

        if (sWintergraspBotsConfig->debug && fullTick)
            LOG_INFO("module",
                "[WGBots] leg {}: ({:.0f},{:.0f},{:.0f}) -> obj ({:.0f},{:.0f},{:.0f}) d2d={:.0f} leg=({:.0f},{:.0f},{:.0f}) walkable={} trunc={} moving={} mounted={} motion={}",
                bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                tgt.GetPositionX(), tgt.GetPositionY(), tgt.GetPositionZ(), bot->GetExactDist2d(&tgt),
                leg.GetPositionX(), leg.GetPositionY(), leg.GetPositionZ(),
                haveLeg ? 1 : 0, truncated ? 1 : 0, bot->isMoving() ? 1 : 0, bot->IsMounted() ? 1 : 0,
                uint32(bot->GetMotionMaster()->GetCurrentMovementGeneratorType()));

        // Still no leg: locally obstructed (workshop bay wall, ruin corner — runtime-observed:
        // a bot pinned at the Broken Temple graveyard 70y from its banner, with the structure
        // blocking every direct leg AND the unstick's wall-clear line). Probe a lateral
        // side-step so the bot walks AROUND the obstruction; the side is stable per bot so
        // successive ticks keep working the same way around.
        if (!haveLeg)
        {
            float base = bot->GetAngle(tgt.GetPositionX(), tgt.GetPositionY());
            float first = bot->GetGUID().GetCounter() % 2 ? float(M_PI) / 2.0f : -float(M_PI) / 2.0f;
            float const offs[3] = { first, -first, float(M_PI) };   // perp, other perp, back out
            for (int k = 0; k < 3 && !haveLeg; ++k)
            {
                Position side(bot->GetPositionX() + 30.0f * std::cos(base + offs[k]),
                              bot->GetPositionY() + 30.0f * std::sin(base + offs[k]),
                              bot->GetPositionZ(), 0.0f);
                bool sideTrunc = false;
                haveLeg = ComputeLegWaypoint(bot, side, leg, sideTrunc);
            }
        }

        // No walkable leg even sideways: the bot is standing somewhere off-mesh (runtime-
        // verified: a bot that strayed onto the boundary mountains). Do NOT fall back to a raw
        // MovePoint — that relaunches the straight-line glide this fix removes. Hold position
        // instead: the stuck sampler below accumulates, and the wall-clear unstick teleport
        // (stop-short, escalating inside the min distance) recovers it.
        if (haveLeg)
            bot->GetMotionMaster()->MovePoint(WG_MOVE_ID, leg.GetPositionX(), leg.GetPositionY(), leg.GetPositionZ());
    }

    // Fast sub-tick: movement only — stuck sampling stays on the full tick (its accumulator
    // is calibrated in full-tick periods).
    if (!fullTick)
        return;

    // Stuck sampling: compare against the last sample taken while traveling.
    if (ts.has)
    {
        float moved = bot->GetExactDist2d(ts.last.GetPositionX(), ts.last.GetPositionY());
        if (moved < sWintergraspBotsConfig->stuckEpsilon)
            ts.stuckMs += sWintergraspBotsConfig->tickMs;
        else
        {
            ts.stuckMs = 0;
            ts.unstickHolds = 0;   // real progress ends the stuck episode — don't carry holds over
        }
    }
    ts.last.Relocate(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    ts.has = true;

    if (ts.stuckMs >= sWintergraspBotsConfig->stuckSeconds * 1000)
    {
        // Unstick must NEVER move a bot through a standing wall (user directive: no through-wall
        // movement of any kind). Only teleport when the straight hop to the objective crosses no
        // structure; otherwise keep the bot where it is and let re-pathing/combat/reassignment
        // resolve it. (Defender boundary crossings are handled by the gate hop above instead.)
        bool wallClear = bot->GetMap()->isInLineOfSight(
            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ() + 2.0f,
            tgt.GetPositionX(), tgt.GetPositionY(), tgt.GetPositionZ() + 2.0f,
            bot->GetPhaseMask(), LINEOFSIGHT_CHECK_GOBJECT_WMO, VMAP::ModelIgnoreFlags::Nothing);
        if (wallClear)
        {
            // Stop SHORT of the objective — never blink a bot into the middle of it. Within
            // the min distance the teleport is withheld (side-step + legs usually resolve
            // local snags), but ESCALATES after 3 withheld cycles so an off-mesh bot close to
            // its objective cannot be a statue forever (Task 8 review finding).
            float dist = bot->GetExactDist2d(&tgt);
            bool escalate = false;
            if (dist <= WG_UNSTICK_MIN_DIST)
            {
                ts.unstickHolds = uint8(ts.unstickHolds + 1);
                escalate = ts.unstickHolds >= 3 && dist > sWintergraspBotsConfig->arriveRadius + 5.0f;
            }
            if (dist > WG_UNSTICK_MIN_DIST || escalate)
            {
                // Landing floor: never closer than arriveRadius to the objective (an escalated
                // teleport at dist 13-18y would otherwise land 3-8y from, e.g., the enemy
                // vehicle cluster — the exact blink the stop-short exists to prevent).
                float shortBy = std::clamp(dist - sWintergraspBotsConfig->arriveRadius - 2.0f,
                                           std::min(dist * 0.5f, sWintergraspBotsConfig->arriveRadius),
                                           WG_UNSTICK_SHORT);
                float frac = (dist - shortBy) / dist;
                float dx = bot->GetPositionX() + (tgt.GetPositionX() - bot->GetPositionX()) * frac;
                float dy = bot->GetPositionY() + (tgt.GetPositionY() - bot->GetPositionY()) * frac;
                float dz = bot->GetMap()->GetHeight(bot->GetPhaseMask(), dx, dy,
                                                    tgt.GetPositionZ() + 20.0f, true, 100.0f);
                if (dz <= INVALID_HEIGHT + 1.0f)
                    dz = tgt.GetPositionZ();
                bot->TeleportTo(WG_MAP_ID, dx, dy, dz, tgt.GetOrientation());
                ts.unstickHolds = 0;
                if (sWintergraspBotsConfig->debug)
                    LOG_INFO("module", "[WGBots] unstuck {} -> {:.0f}y short of objective ({:.0f},{:.0f})",
                             bot->GetName(), shortBy, tgt.GetPositionX(), tgt.GetPositionY());
            }
        }
        ts.stuckMs = 0;
        ts.has = false;
    }
}

// Rebuild the live battle snapshot: where the siege is (vanguard), what threatens the keep
// (vehicle cluster near the gate + walls that just took damage), and the attacker rank census.
void WintergraspBotsDirector::BuildBattleState(Battlefield* bf, std::vector<Position> const& convoy)
{
    _state = WgBattleState();

    // Vanguard: the crewed attacker vehicle closest to the keep gate.
    float best = 100000.0f;
    for (Position const& v : convoy)
    {
        float d = v.GetExactDist2d(&WG_KEEP_GATE);
        if (d < best)
        {
            best = d;
            _state.vanguard = v;
            _state.hasVanguard = true;
        }
    }

    // Threat cluster: ALL hostile ground vehicles near the gate (grid search from the defender
    // bot nearest the keep — also catches human-driven and freshly built ones the convoy list
    // misses). Centroid + count drive the defender rally job.
    TeamId defTeam = _attackerTeam == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    Player* ref = nullptr;
    float refBest = 100000.0f;
    for (ObjectGuid const& g : _managed[defTeam])
        if (Player* p = ObjectAccessor::FindPlayer(g))
            if (p->IsInWorld() && p->IsAlive() && p->GetZoneId() == WG_ZONE_ID)
            {
                float d = p->GetExactDist2d(&WG_KEEP_GATE);
                if (d < refBest)
                {
                    refBest = d;
                    ref = p;
                }
            }
    // No living defender near the keep -> no ref: threat/wall-fire detection degrades to zero
    // for the tick. Acceptable: those signals only feed defender behaviors, which have no
    // actors in that scenario (vanguard + census above don't depend on the ref).
    if (ref)
    {
        std::list<Creature*> vehicles;
        ref->GetCreatureListWithEntryInGrid(vehicles,
            std::vector<uint32>(std::begin(WG_GROUND_VEHICLES), std::end(WG_GROUND_VEHICLES)), 800.0f);
        float cx = 0, cy = 0;
        for (Creature* c : vehicles)
        {
            if (!c->IsAlive() || !ref->IsValidAttackTarget(c))
                continue;
            if (c->GetExactDist2d(&WG_KEEP_GATE) > sWintergraspBotsConfig->threatRadius)
                continue;
            cx += c->GetPositionX();
            cy += c->GetPositionY();
            ++_state.threatCount;
        }
        if (_state.threatCount)
        {
            cx /= _state.threatCount;
            cy /= _state.threatCount;
            float cz = ref->GetMap()->GetHeight(PHASEMASK_NORMAL, cx, cy, 440.0f, true, 100.0f);
            if (cz <= INVALID_HEIGHT + 1.0f)
                cz = 409.0f;
            _state.threat.Relocate(cx, cy, cz, 0.0f);
        }

        // Walls under fire: any axis structure whose health dropped since last tick. Lets the
        // defense react to shelling even before it has line of sight to the shooter.
        for (uint32 entry : WG_AXIS_STRUCTURES)
            if (GameObject* go = ref->FindNearestGameObject(entry, 1500.0f))
            {
                uint32 hp = go->GetGOValue()->Building.Health;
                auto it = _wallHealth.find(entry);
                if (it != _wallHealth.end() && hp < it->second)
                {
                    _state.wallUnderFire = true;
                    float fx = go->GetPositionX() - 30.0f;
                    float fz = ref->GetMap()->GetHeight(PHASEMASK_NORMAL, fx, go->GetPositionY(),
                                                        go->GetPositionZ() + 15.0f, true, 100.0f);
                    if (fz <= INVALID_HEIGHT + 1.0f)
                        fz = 409.0f;
                    _state.firedWall.Relocate(fx, go->GetPositionY(), fz, 0.0f);
                }
                _wallHealth[entry] = hp;
            }
    }

    // Attacker rank census vs demand (enough Lieutenants to crew the buildable vehicles).
    if (_attackerTeam == TEAM_ALLIANCE || _attackerTeam == TEAM_HORDE)
    {
        for (ObjectGuid const& g : _managed[_attackerTeam])
            if (Player* p = ObjectAccessor::FindPlayer(g))
                if (p->IsInWorld() && p->IsAlive() && p->HasAura(SPELL_LIEUTENANT))
                    ++_state.lieutenants;
        uint32 myMax = bf->GetData(_attackerTeam == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A
                                                                  : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H);
        _state.lieutenantsWanted = std::max(0, int(myMax) - int(sWintergraspBotsConfig->humanReserve));
    }
}

// Rally headcount: scale to the enemy cluster, floor a fireteam, cap by DefenseShare.
int WintergraspBotsDirector::RallyWant(int aliveCount) const
{
    int want = std::max(4, int(_state.threatCount * sWintergraspBotsConfig->rallyPerVehicle));
    want = std::min(want, (aliveCount * int(sWintergraspBotsConfig->defenseShare)) / 100);
    return std::max(2, want);
}

// Muster-then-sortie: defenders leave the keep as a wave or not at all. While un-released,
// the rally job sits at WG_MUSTER (inside); when enough gather there the wave releases and
// the job flips to the real outside rally point. A wiped wave (after it had time to exit)
// regroups at muster instead of dribbling replacements out the single rampart exit one at
// a time into the attacker mob (user-reported trickle). Below quorum the force holds
// inside indefinitely — an interior stand at the gate, by construction.
void WintergraspBotsDirector::UpdateSortie(Battlefield* bf)
{
    bool threat = _state.threatCount > 0 || _state.wallUnderFire;
    if (!threat || sWintergraspBotsConfig->sortieQuorum == 0)
    {
        _sortieReleased = false;
        _sortieAgeMs = 0;
        _sortieLowTicks = 0;
        return;
    }

    TeamId def = bf->GetAttackerTeam() == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    Position rally = _state.threatCount > 0 ? _state.threat : _state.firedWall;
    int muster = 0, atRally = 0, alive = 0;
    for (ObjectGuid const& g : _managed[def])
    {
        Player* p = ObjectAccessor::FindPlayer(g);
        if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetVehicle() || p->GetZoneId() != WG_ZONE_ID)
            continue;
        ++alive;
        // NOTE: counts ANY defender near the point, not just rally assignees — fine while no
        // other defender job anchors in the west courtyard (nearest is a cannon post, 65y+).
        if (p->GetExactDist2d(&WG_MUSTER) < WG_MUSTER_RADIUS)
            ++muster;
        if (!IsInsideKeep(p->GetPositionX(), p->GetPositionY()) && p->GetExactDist2d(&rally) < WG_RALLY_NEAR)
            ++atRally;
    }

    if (_sortieReleased)
    {
        _sortieAgeMs += sWintergraspBotsConfig->tickMs;
        // Wave wiped: require SUSTAINED absence (2 reconciles) after the exit grace — the
        // rally anchor MOVES with the siege (and flips to the fired wall), so a single-tick
        // undercount can be the reference drifting away from a live wave, not a wipe.
        if (_sortieAgeMs > WG_SORTIE_GRACE_MS && atRally < 2)
        {
            if (++_sortieLowTicks >= 2)
            {
                _sortieReleased = false;
                _sortieAgeMs = 0;
                _sortieLowTicks = 0;
                if (sWintergraspBotsConfig->debug)
                    LOG_INFO("module", "[WGBots] sortie wave gone ({} at rally) — regrouping at muster", atRally);
            }
        }
        else
            _sortieLowTicks = 0;
        return;
    }

    int quorum = std::min<int>(int(sWintergraspBotsConfig->sortieQuorum), RallyWant(alive));
    if (muster >= quorum)
    {
        _sortieReleased = true;
        _sortieAgeMs = 0;
        _sortieLowTicks = 0;
        if (sWintergraspBotsConfig->debug)
            LOG_INFO("module", "[WGBots] sortie wave released ({} mustered, quorum {})", muster, quorum);
    }
    else if (sWintergraspBotsConfig->debug)
        LOG_INFO("module", "[WGBots] mustering {}/{}", muster, quorum);
}

// Fix 2c: kill alive-but-unmanned, unreachable friendly siege vehicles that no one has re-crewed
// for VehicleReapSeconds (stuck in terrain/water). The core frees the vehicle cap ONLY on death
// (OnUnitDeath); a despawn leaks the slot. KillSelf() reaches setDeathState(JUST_DIED) ->
// OnUnitDeath -> the cap decrements. Searches around each managed attacker bot (the ~533y grid
// clamp means one search center can't see the whole field). Runs once per full reconcile.
void WintergraspBotsDirector::ReapIdleVehicles(Battlefield* /*bf*/)
{
    uint32 reapMs = sWintergraspBotsConfig->vehicleReapSeconds * 1000;
    if (reapMs == 0 || (_attackerTeam != TEAM_ALLIANCE && _attackerTeam != TEAM_HORDE))
    {
        _idleVeh.clear();
        return;
    }

    std::unordered_set<ObjectGuid> seen;
    std::vector<Creature*> idle;
    for (ObjectGuid const& g : _managed[_attackerTeam])
    {
        Player* p = ObjectAccessor::FindPlayer(g);
        if (!p || !p->IsInWorld() || p->GetZoneId() != WG_ZONE_ID)
            continue;
        std::list<Creature*> vs;
        p->GetCreatureListWithEntryInGrid(vs,
            std::vector<uint32>(std::begin(WG_GROUND_VEHICLES), std::end(WG_GROUND_VEHICLES)), 400.0f);
        for (Creature* v : vs)
        {
            if (!v->IsAlive() || !v->IsFriendlyTo(p))
                continue;
            if (!seen.insert(v->GetGUID()).second)
                continue;
            Vehicle* kit = v->GetVehicleKit();
            // Reap only a FULLY empty wreck: any occupied seat (a gunner, or a real player in a
            // passenger seat) means it's not abandoned — killing it would strand the rider on the
            // corpse until deferred removal. A missing kit isn't a real vehicle either.
            if (!kit || kit->IsVehicleInUse())
                continue;
            idle.push_back(v);
        }
    }

    // Age idle vehicles; kill past the threshold. Timers for no-longer-idle vehicles drop by
    // omission from the rebuilt map.
    std::unordered_map<ObjectGuid, uint32> next;
    for (Creature* v : idle)
    {
        uint32 t = _idleVeh[v->GetGUID()] + sWintergraspBotsConfig->tickMs;
        if (t >= reapMs)
        {
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module", "[WGBots] reaping abandoned vehicle {} at ({:.0f},{:.0f}) — frees the cap",
                         v->GetName(), v->GetPositionX(), v->GetPositionY());
            v->KillSelf();
        }
        else
            next[v->GetGUID()] = t;
    }
    _idleVeh.swap(next);
}

void WintergraspBotsDirector::Reconcile()
{
    Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
    if (!bf)
        return;

    // Refresh status snapshot (cheap; also used by .wgbots status).
    _attackerTeam = bf->GetAttackerTeam();
    _relicOpen = RelicOpen(bf);
    _controlled[TEAM_ALLIANCE] = _controlled[TEAM_HORDE] = 0;
    for (WorkshopDef const& w : WG_WORKSHOPS)
    {
        TeamId c = WorkshopController(bf, w.areaId);
        if (c == TEAM_ALLIANCE || c == TEAM_HORDE)
            ++_controlled[c];
    }

    // Breach stage for the attacker push anchors (0 = fortress door intact, 1 = door down,
    // 2 = mid wall down too). Keep buildings are battlefield spawns read via a live bot's
    // grid search — and Cell::Visit CLAMPS every search radius to SIZE_OF_GRIDS (~533y),
    // so the search center must be a bot that is actually near the keep: an arbitrary ref
    // (first in hash order — a workshop squad, a road post) silently finds NO door at all,
    // reads stage 0, and turns EnforceBreachFrontier against the open gate (production-
    // observed: attackers bounced out of a legal courtyard). Use the managed bot nearest
    // the gate, and LATCH the stage for the battle — walls never rebuild mid-battle, so a
    // detection hiccup may stall the stage but must never regress it. Reset in ReleaseAll.
    Player* stageRef = nullptr;
    float stageBest = 100000.0f;
    for (int t = 0; t < 2; ++t)
        for (ObjectGuid const& g : _managed[t])
            if (Player* q = ObjectAccessor::FindPlayer(g))
                if (q->IsInWorld() && q->GetZoneId() == WG_ZONE_ID)
                {
                    float d = q->GetExactDist2d(&WG_KEEP_GATE);
                    if (d < stageBest)
                    {
                        stageBest = d;
                        stageRef = q;
                    }
                }
    if (stageRef)
    {
        uint8 stage = 0;
        if (GameObject* door = stageRef->FindNearestGameObject(190375, 1500.0f))
            if (door->GetGOValue()->Building.Health == 0)
            {
                stage = 1;
                if (GameObject* mid = stageRef->FindNearestGameObject(191805, 1500.0f))
                    if (mid->GetGOValue()->Building.Health == 0)
                        stage = 2;
            }
        if (stage > _breachStage)
        {
            _breachStage = stage;
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module", "[WGBots] breach stage -> {}", _breachStage);
        }
    }

    // Pre-pass: collect the attacker convoy (crewed ground-vehicle positions) for the battle
    // snapshot — the per-team loop below re-collects it for escort jobs.
    std::vector<Position> stateConvoy;
    if (_attackerTeam == TEAM_ALLIANCE || _attackerTeam == TEAM_HORDE)
        for (ObjectGuid const& g : _managed[_attackerTeam])
            if (Player* p = ObjectAccessor::FindPlayer(g))
                if (p->IsInWorld() && p->IsAlive() && p->GetZoneId() == WG_ZONE_ID)
                    if (Unit* vb = p->GetVehicleBase())
                        for (uint32 e : WG_GROUND_VEHICLES)
                            if (vb->GetEntry() == e)
                            {
                                stateConvoy.push_back(vb->GetPosition());
                                break;
                            }
    BuildBattleState(bf, stateConvoy);
    UpdateSortie(bf);
    // Crewed attacker siege vehicles exist somewhere -> defenders hunt them on the fast (1s) tick
    // so they don't run right past a vehicle for a whole reconcile (gates that scan; see MovementTick).
    _siegeVehiclesActive = !stateConvoy.empty();

    // Field guard posts at the workshops and bridge roads (the core never spawns these).
    UpdateGarrison(bf);

    // Everyone (bot or human) fighting in the zone this tick — the engage layer scans the
    // OTHER faction's slice for idle-bot targets (threat-blind cases: see EngageNearbyEnemy).
    std::vector<Player*> zonePlayers;
    if (Map* map = sMapMgr->FindMap(WG_MAP_ID, 0))
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (auto itr = players.begin(); itr != players.end(); ++itr)
            if (Player* p = itr->GetSource())
                if (p->IsInWorld() && p->IsAlive() && p->GetZoneId() == WG_ZONE_ID && !p->IsGameMaster())
                    zonePlayers.push_back(p);
    }

    for (int team = 0; team < 2; ++team)
    {
        TeamId teamId = TeamId(team);

        std::vector<Player*> hostiles;
        for (Player* p : zonePlayers)
            if (p->GetTeamId() != teamId)
                hostiles.push_back(p);

        // ADOPT eligible bots already in the zone (organic arrivals via the Phase 1 invite
        // auto-accept) so EVERY in-zone bot pursues objectives — not just ones we teleported.
        // Without this the objective layer only steers the handful the director topped up, and
        // nobody masses on a banner to capture it.
        for (auto const& it : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = it.second;
            if (!b || !b->IsInWorld() || !b->IsAlive())
                continue;
            if (b->GetZoneId() != WG_ZONE_ID || b->GetTeamId() != teamId)
                continue;
            if (b->GetLevel() < sWintergraspBotsConfig->minLevel)
                continue;
            // Leave player/roster-grouped bots alone — but the WG battlefield raid-groups
            // EVERYONE it invites, so a plain group check turns adoption into a one-way
            // ratchet (a bot pruned once can never come back; runtime-observed: 3 managed
            // vs 26 in zone). Battlefield groups are automatic — adopt through them.
            if (b->GetGroup() && !b->GetGroup()->isBFGroup())
                continue;
            if (sRandomPlayerbotMgr.IsAddclassBot(b))   // leave roster/addclass bots
                continue;
            _managed[team].insert(b->GetGUID());
        }

        // Prune managed bots that logged out, left the world, or left the zone; collect the
        // assignable pool (alive, on foot) from the rest.
        std::vector<Player*> pool;
        std::vector<Position> convoy;   // attacker ground vehicles (positions of crewed engines)
        for (auto itr = _managed[team].begin(); itr != _managed[team].end();)
        {
            Player* p = ObjectAccessor::FindPlayer(*itr);
            if (!p || !p->IsInWorld() || p->GetZoneId() != WG_ZONE_ID)
            {
                _activated.erase(*itr);
                _objective.erase(*itr);
                _travel.erase(*itr);
                _gunnerIdle.erase(*itr);
                _vehTravel.erase(*itr);
                itr = _managed[team].erase(itr);
                continue;
            }

            // Apply the WG combat loadout once (ResetStrategies re-runs AiFactory: adds
            // pvp/dps-assist/attack-tagged, drops travel/rpg/grind for zone 4197 in war-time).
            if (!_activated.count(*itr))
            {
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(p))
                    ai->ResetStrategies();
                _activated.insert(*itr);
            }

            if (p->IsAlive())
                ScrubPetTargets(p);   // Fix 1: strip friendly pet targets on the full tick as well

            // Dead: let playerbots' own death handling (release, ghost corpse-run, resurrect) own
            // the bot's movement — steering a ghost with MovePoint fights the corpse-run and
            // walked one clear off-mesh into the boundary mountains (runtime-verified).
            // Seated: demolisher crews belong to the siege strategy; manned keep cannons get
            // their firing target curated here; attacker crews' vehicles form the convoy that
            // infantry escorts run with.
            if (!p->IsAlive() || p->GetVehicle())
            {
                if (p->IsAlive())
                {
                    if (Unit* vb = p->GetVehicleBase(); vb && vb->GetEntry() == NPC_WINTERGRASP_TOWER_CANNON)
                    {
                        // Gunner lifecycle: fire while targets exist; dismount once the keep
                        // threat clears or the gun has been dry two reconciles running.
                        bool found = MaintainCannonTarget(p);
                        uint8& idle = _gunnerIdle[*itr];
                        idle = found ? 0 : uint8(idle + 1);
                        bool keepThreat = _state.threatCount > 0 || _state.wallUnderFire;
                        // NOTE: keepThreat sees vehicles within ThreatRadius (350y) of the gate
                        // but the gun only reaches 120y — a siege camping between produces a
                        // seat/dry/dismount/re-seat cycle (~15s). Cosmetic; natural hysteresis
                        // point if it ever bothers anyone.
                        if (!keepThreat || idle >= 2)
                        {
                            p->ExitVehicle();
                            _gunnerIdle.erase(*itr);
                            if (sWintergraspBotsConfig->debug)
                                LOG_INFO("module", "[WGBots] {} dismounts the cannon ({})",
                                         p->GetName(), !keepThreat ? "threat clear" : "gun dry");
                        }
                    }
                    if (teamId == bf->GetAttackerTeam())
                        if (Unit* vb = p->GetVehicleBase())
                            for (uint32 e : WG_GROUND_VEHICLES)
                                if (vb->GetEntry() == e)
                                {
                                    convoy.push_back(vb->GetPosition());
                                    WatchVehicle(p, vb);
                                    break;
                                }
                }
                _travel.erase(*itr);
                ++itr;
                continue;
            }

            _gunnerIdle.erase(*itr);   // on foot: no live gunner counter by definition
            _vehTravel.erase(*itr);    // on foot: must not inherit stale vehicle samples
            pool.push_back(p);
            ++itr;
        }

        // Priority jobs with quotas, filled nearest-first among rank-eligible bots; the last
        // job absorbs the leftovers.
        std::vector<WgJob> jobs = BuildJobs(bf, teamId, int(pool.size()), convoy);
        struct Assign { Player* p; Position pos; WgJobType type; };
        std::vector<Assign> assignment;
        std::vector<Player*> remaining = pool;
        for (WgJob const& job : jobs)
        {
            if (remaining.empty())
                break;
            std::vector<Player*> cands;
            for (Player* b : remaining)
            {
                bool lt = b->HasAura(SPELL_LIEUTENANT);
                if (job.rank == WG_RANK_LIEUTENANT && !lt)
                    continue;
                if (job.rank == WG_RANK_RANKLESS && lt)
                    continue;
                cands.push_back(b);
            }
            std::sort(cands.begin(), cands.end(), [&job](Player* a, Player* b)
                      { return a->GetExactDist2d(&job.pos) < b->GetExactDist2d(&job.pos); });
            int take = std::min<int>(job.want, int(cands.size()));
            for (int i = 0; i < take; ++i)
            {
                assignment.push_back({ cands[i], job.pos, job.type });
                remaining.erase(std::find(remaining.begin(), remaining.end(), cands[i]));
            }
        }
        for (Player* p : remaining)   // leftovers join the faction's main effort (last job)
            assignment.push_back({ p, jobs.back().pos, jobs.back().type });

        for (auto& [p, tgt, jobType] : assignment)
        {
            _objective[p->GetGUID()] = tgt;

            // Contain any bot that native combat movement carried through a standing gate/wall
            // before issuing new orders (see EnforceBreachFrontier; both teams).
            if (EnforceBreachFrontier(p))
                continue;

            // Relic endgame: ANY attacker beside the open relic clicks it. (Foot bots without the
            // Lieutenant aura never run the siege strategy's relic action, so without this they
            // stand next to the relic and the battle stalls.)
            if (_relicOpen && teamId == bf->GetAttackerTeam())
                if (GameObject* relic = p->FindNearestGameObject(GO_TITAN_RELIC, 15.0f))
                {
                    relic->Use(p);
                    continue;
                }

            // Only a crew-job assignee may seat a cannon (a passing defender must NOT be
            // vacuumed into a turret — that was the turret-squatting bug).
            if (jobType == WG_JOB_CREW && !p->IsInCombat() && TrySeatCannon(p))
            {
                _travel.erase(p->GetGUID());
                continue;
            }

            // Idle FOOT bots pick a fight with any hostile siege vehicle or enemy player
            // around — neither threatens the bot until it's hit, so the playerbots engine
            // won't do it itself. In LOS: attack. Vehicles out of LOS (e.g. shelling the wall
            // this bot stands behind): chase — run around the structure toward it; the next
            // tick's LOS pass turns the chase into an attack.
            if (!p->IsInCombat())
            {
                Position chase;
                int eng = EngageNearbyEnemy(p, hostiles, chase);
                if (eng == 1)
                {
                    _travel.erase(p->GetGUID());
                    continue;
                }
                if (eng == 2)
                {
                    // Muster hold: the out-of-LOS chase tier (a vehicle shelling the wall) must
                    // not divert a gathering defender — the chase leg walks him up the rampart
                    // and out SOLO, exactly the trickle the muster exists to prevent. In-LOS
                    // fights (eng == 1) stay allowed: a visible enemy inside gets fought.
                    bool musterHold = teamId != _attackerTeam &&
                                      sWintergraspBotsConfig->sortieQuorum > 0 && !_sortieReleased &&
                                      IsInsideKeep(p->GetPositionX(), p->GetPositionY());
                    if (!musterHold)
                        tgt = chase;
                }
            }

            // Re-crew an abandoned friendly siege vehicle we're passing (Fix 2b) — reuse it
            // instead of stalling the cap. Attacker only; skip crew/production assignees.
            if (teamId == _attackerTeam && jobType == WG_JOB_GENERIC && !p->IsInCombat() &&
                TryRecrewVehicle(p))
            {
                _travel.erase(p->GetGUID());
                continue;
            }

            StepBot(p, tgt, true, jobType != WG_JOB_GENERIC);
        }

        // Over-fill fix: base need on the ACTUAL in-zone count of this faction (managed or not),
        // not on _managed.size() — the P2a bug over-filled to ~3x because unmanaged in-zone bots
        // weren't counted.
        _inZone[team] = CountInZoneBots(teamId);

        // DIAGNOSTIC: is the managed-vs-inzone gap just dead bots (normal) or bots stuck outside
        // the zone (a real churn/entry bug)? The prune loop already drops out-of-zone bots from
        // _managed, so managed should equal (alive-in-zone + dead-in-zone); this confirms it.
        if (sWintergraspBotsConfig->debug)
        {
            int mgAlive = 0, mgDead = 0;
            for (ObjectGuid const& g : _managed[team])
            {
                Player* mp = ObjectAccessor::FindPlayer(g);
                if (!mp)
                    continue;
                if (mp->IsAlive())
                    ++mgAlive;
                else
                    ++mgDead;
            }
            LOG_INFO("module", "[WGBots] team {} census: managed={} (alive={} dead={}) inZoneAlive={}",
                     int(team), _managed[team].size(), mgAlive, mgDead, _inZone[team]);
        }

        int need = int(sWintergraspBotsConfig->perFaction) - _inZone[team];
        if (need <= 0)
            continue;

        std::vector<Player*> picks = SelectEligibleBots(teamId, need);
        for (Player* bot : picks)
        {
            // Enter at the battle center; assignment + movement take over next tick.
            bot->TeleportTo(WG_MAP_ID, WG_X, WG_Y, WG_Z, WG_O);
            _managed[team].insert(bot->GetGUID());
            if (sWintergraspBotsConfig->debug)
                LOG_INFO("module", "[WGBots] sent {} (team {}) to Wintergrasp", bot->GetName(), team);
        }
    }

    // Fix 2c: reap abandoned vehicles across the field to free the cap for rebuilds.
    ReapIdleVehicles(bf);
}

void WintergraspBotsDirector::ReleaseAll()
{
    // Stand the field garrison down (module-owned spawns: the battlefield's own cleanup does
    // not know about them and they must not persist into peacetime). The tracked-GUID loop
    // alone is NOT enough: a failed flip despawn orphans guards out of _garrison, and those
    // orphans used to survive into later battles (runtime-verified: ~40 stale guards swept at
    // one post on battle start after hours of test battles). Sweep each post's slot area with
    // the same per-slot filter the spawn audit uses, then despawn the tracked squad.
    if (Map* map = sMapMgr->FindMap(WG_MAP_ID, 0))
        for (auto const& [postId, squad] : _garrison)
        {
            // Map lookups, NOT Battlefield::GetCreature — see UpdateGarrison (BfMap is null
            // for the server's lifetime; bf->GetCreature() silently no-ops).
            Creature* ref = nullptr;
            for (ObjectGuid const& g : squad)
                if ((ref = map->GetCreature(g)))
                    break;
            if (ref && postId < WG_GARRISON_POST_COUNT)
            {
                GarrisonPostDef const& post = WG_GARRISON_POSTS[postId];
                std::list<Creature*> nearby;
                ref->GetCreatureListWithEntryInGrid(nearby,
                    std::vector<uint32>{ WG_NPC_GUARD_A, WG_NPC_GUARD_H }, WG_GARRISON_AUDIT_SEARCH);
                for (Creature* s : nearby)
                {
                    bool onSlot = false;
                    for (uint8 k = 0; k < post.count && !onSlot; ++k)
                        onSlot = s->GetExactDist2d(post.slots[k][0], post.slots[k][1]) <= WG_GARRISON_AUDIT_SLOT_RANGE;
                    if (onSlot)
                        RemoveGuard(s);
                }
            }
            for (ObjectGuid const& g : squad)
                if (Creature* c = map->GetCreature(g))
                    RemoveGuard(c);
        }
    _garrison.clear();
    _garrisonOwner.clear();
    _gunnerIdle.clear();

    // Battle over: forget everyone; bots resume normal random-bot activity once out of war-time.
    _managed[0].clear();
    _managed[1].clear();
    _activated.clear();
    _objective.clear();
    _travel.clear();
    _vehTravel.clear();
    _idleVeh.clear();
    _inZone[0] = _inZone[1] = 0;
    _controlled[0] = _controlled[1] = 0;
    _relicOpen = false;
    _breachStage = 0;   // stage latches during a battle (walls never rebuild mid-battle)
    _siegeVehiclesActive = false;
    _state = WgBattleState();
    _wallHealth.clear();
    _sortieReleased = false;
    _sortieAgeMs = 0;
    _sortieLowTicks = 0;
    if (sWintergraspBotsConfig->debug)
        LOG_INFO("module", "[WGBots] battle ended, released managed bots");
}

std::string WintergraspBotsDirector::StatusText() const
{
    uint32 vehA = 0, vehH = 0;
    if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID))
    {
        vehA = bf->GetData(BATTLEFIELD_WG_DATA_VEHICLE_A);
        vehH = bf->GetData(BATTLEFIELD_WG_DATA_VEHICLE_H);
    }
    return "WGBots: enable=" + std::to_string(sWintergraspBotsConfig->enable ? 1 : 0)
         + " active=" + std::to_string(_battleActive ? 1 : 0)
         + " | managed A=" + std::to_string(_managed[TEAM_ALLIANCE].size())
         + " H=" + std::to_string(_managed[TEAM_HORDE].size())
         + " | inzone A=" + std::to_string(_inZone[TEAM_ALLIANCE])
         + " H=" + std::to_string(_inZone[TEAM_HORDE])
         + " | workshops A=" + std::to_string(_controlled[TEAM_ALLIANCE])
         + " H=" + std::to_string(_controlled[TEAM_HORDE])
         + " | vehicles A=" + std::to_string(vehA)
         + " H=" + std::to_string(vehH)
         + " | breach=" + std::to_string(_breachStage)
         + " | relicOpen=" + std::to_string(_relicOpen ? 1 : 0)
         + " | lts=" + std::to_string(_state.lieutenants) + "/" + std::to_string(_state.lieutenantsWanted)
         + " thr=" + std::to_string(_state.threatCount)
         + " fire=" + std::to_string(_state.wallUnderFire ? 1 : 0);
}

// DIAGNOSTIC (pathing investigation): for every hardcoded objective, report the real ground
// height vs the coded Z, then run a live PathGenerator from the first managed bot of each team
// to each objective and report the path type. Type flags: 0x01 NORMAL, 0x02 SHORTCUT,
// 0x04 INCOMPLETE, 0x08 NOPATH, 0x10 NOT_USING_PATH, 0x20 SHORT, 0x40/0x80 FARFROMPOLY start/end.
std::string WintergraspBotsDirector::PathTest() const
{
    Map* map = sMapMgr->FindMap(WG_MAP_ID, 0);
    if (!map)
        return "pathtest: map 571 not loaded";

    struct Target { char const* name; float x, y, z; };
    Target const targets[] =
    {
        { "ws NE SunkenRing", WG_WORKSHOPS[0].x, WG_WORKSHOPS[0].y, WG_WORKSHOPS[0].z },
        { "ws NW BrokenTmpl", WG_WORKSHOPS[1].x, WG_WORKSHOPS[1].y, WG_WORKSHOPS[1].z },
        { "ws SE Eastspark",  WG_WORKSHOPS[2].x, WG_WORKSHOPS[2].y, WG_WORKSHOPS[2].z },
        { "ws SW Westspark",  WG_WORKSHOPS[3].x, WG_WORKSHOPS[3].y, WG_WORKSHOPS[3].z },
        { "defender anchor",  WG_DEFENDER_ANCHOR.GetPositionX(), WG_DEFENDER_ANCHOR.GetPositionY(), WG_DEFENDER_ANCHOR.GetPositionZ() },
        { "attacker push 0",  WG_ATTACKER_PUSH[0].GetPositionX(), WG_ATTACKER_PUSH[0].GetPositionY(), WG_ATTACKER_PUSH[0].GetPositionZ() },
        { "relic anchor",     WG_RELIC_ANCHOR.GetPositionX(),    WG_RELIC_ANCHOR.GetPositionY(),    WG_RELIC_ANCHOR.GetPositionZ() },
        { "battle center",    WG_X, WG_Y, WG_Z },
    };

    std::string out = "pathtest: coded Z vs ground Z:";
    for (Target const& t : targets)
    {
        float gzNear = map->GetHeight(PHASEMASK_NORMAL, t.x, t.y, t.z + 5.0f,  true, 120.0f);
        float gzHigh = map->GetHeight(PHASEMASK_NORMAL, t.x, t.y, t.z + 60.0f, true, 200.0f);
        out += Acore::StringFormat("\n  {:<16} coded={:>6.1f} ground(z+5)={:>6.1f} ground(z+60)={:>6.1f}",
                                   t.name, t.z, gzNear, gzHigh);
    }

    for (int team = 0; team < 2; ++team)
    {
        Player* src = nullptr;
        for (ObjectGuid const& g : _managed[team])
            if (Player* p = ObjectAccessor::FindPlayer(g))
                if (p->IsInWorld() && p->IsAlive() && p->GetZoneId() == WG_ZONE_ID)
                {
                    src = p;
                    break;
                }
        if (!src)
        {
            out += Acore::StringFormat("\n team {}: no live managed bot to path from", team);
            continue;
        }
        out += Acore::StringFormat("\n team {} paths from {} ({:.0f},{:.0f},{:.0f}) falling={}:",
                                   team, src->GetName(), src->GetPositionX(), src->GetPositionY(),
                                   src->GetPositionZ(), src->IsFalling() ? 1 : 0);
        for (Target const& t : targets)
        {
            PathGenerator pg(src);
            bool ok = pg.CalculatePath(t.x, t.y, t.z, false);
            out += Acore::StringFormat("\n   -> {:<16} ok={} type=0x{:02x} pts={} end=({:.0f},{:.0f},{:.0f})",
                                       t.name, ok ? 1 : 0, uint32(pg.GetPathType()), pg.GetPath().size(),
                                       pg.GetActualEndPosition().x, pg.GetActualEndPosition().y,
                                       pg.GetActualEndPosition().z);
        }
    }

    // Rampart probe: map the elevated mesh around the gatehouse from a defender INSIDE the keep
    // — verifies whether the courtyard connects to the wall top (the jump-exit route).
    Player* def = nullptr;
    if (_attackerTeam == TEAM_ALLIANCE || _attackerTeam == TEAM_HORDE)
    {
        int defTeam = _attackerTeam == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
        for (ObjectGuid const& g : _managed[defTeam])
            if (Player* p = ObjectAccessor::FindPlayer(g))
                if (p->IsInWorld() && p->IsAlive() && p->GetZoneId() == WG_ZONE_ID &&
                    IsInsideKeep(p->GetPositionX(), p->GetPositionY()))
                {
                    def = p;
                    break;
                }
    }
    if (!def)
        out += "\n rampart probe: no live defender inside the keep to path from";
    else
    {
        out += Acore::StringFormat("\n rampart probe from {} ({:.0f},{:.0f},{:.0f}):",
                                   def->GetName(), def->GetPositionX(), def->GetPositionY(), def->GetPositionZ());
        for (float x = 5150.0f; x <= 5210.0f; x += 15.0f)
            for (float y = 2790.0f; y <= 2895.0f; y += 25.0f)
            {
                float ez = map->GetHeight(PHASEMASK_NORMAL, x, y, 445.0f, true, 50.0f);
                if (ez <= 420.0f)
                    continue;   // ground level here — only elevated surfaces are interesting
                PathGenerator pg(def);
                bool ok = pg.CalculatePath(x, y, ez, false);
                out += Acore::StringFormat("\n   ({:.0f},{:.0f}) topZ={:.0f} ok={} type=0x{:02x} pts={} endZ={:.0f}",
                                           x, y, ez, ok ? 1 : 0, uint32(pg.GetPathType()), pg.GetPath().size(),
                                           pg.GetActualEndPosition().z);
            }
        PathGenerator pg(def);
        bool ok = pg.CalculatePath(WG_RAMP_TOP.GetPositionX(), WG_RAMP_TOP.GetPositionY(),
                                   WG_RAMP_TOP.GetPositionZ(), false);
        out += Acore::StringFormat("\n   RAMP_TOP ({:.0f},{:.0f},{:.0f}) ok={} type=0x{:02x} pts={} end=({:.0f},{:.0f},{:.0f})",
                                   WG_RAMP_TOP.GetPositionX(), WG_RAMP_TOP.GetPositionY(), WG_RAMP_TOP.GetPositionZ(),
                                   ok ? 1 : 0, uint32(pg.GetPathType()), pg.GetPath().size(),
                                   pg.GetActualEndPosition().x, pg.GetActualEndPosition().y,
                                   pg.GetActualEndPosition().z);
    }
    return out;
}

static std::vector<Player*> SelectEligibleBots(TeamId team, int count)
{
    std::vector<Player*> out;
    if (count <= 0)
        return out;

    // PlayerBotMap is std::map<ObjectGuid, Player*>; iterate value (.second).
    for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
    {
        Player* bot = itr.second;
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            continue;
        if (bot->GetTeamId() != team)
            continue;
        if (bot->GetLevel() < sWintergraspBotsConfig->minLevel)
            continue;
        if (bot->GetZoneId() == WG_ZONE_ID)          // already in Wintergrasp
            continue;
        if (bot->InBattleground())
            continue;
        if (!bot->GetMap() || bot->GetMap()->IsDungeon())   // not in a dungeon/instance
            continue;
        if (bot->GetGroup())                          // leave grouped bots (roster/player)
            continue;
        if (sRandomPlayerbotMgr.IsAddclassBot(bot))   // leave roster/addclass bots
            continue;

        out.push_back(bot);
        if (out.size() >= (size_t)count)
            break;
    }
    return out;
}
