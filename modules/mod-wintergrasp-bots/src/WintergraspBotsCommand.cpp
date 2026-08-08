#include "WintergraspBotsDirector.h"
#include "Chat.h"
#include "CommandScript.h"
#include "ScriptMgr.h"
#include "BattlefieldMgr.h"
#include "BattlefieldWG.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "AiObjectContext.h"
#include "PositionValue.h"
#include "Map.h"
#include "MapMgr.h"
#include "ModelIgnoreFlags.h"
#include "PathGenerator.h"
#include "StringFormat.h"
#include <cmath>

using namespace Acore::ChatCommands;

void AddWintergraspBotsCommandScripts();   // fwd decl (called from the loader)

namespace
{
    constexpr uint32 WG_ZONE_ID = 4197;

    // Phase 3 SPIKE helpers (console-driven; auto-select bots since the server console has no
    // selected target). Removed/demoted after the untested-path verification.

    // First attacker-team bot in WG not already in a vehicle.
    Player* FindAttackerBotToBuild(Battlefield* bf)
    {
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (!b || !b->IsInWorld() || !b->IsAlive())
                continue;
            if (b->GetZoneId() != WG_ZONE_ID || b->GetTeamId() != bf->GetAttackerTeam())
                continue;
            if (b->GetVehicleBase())
                continue;
            return b;
        }
        return nullptr;
    }

    // First bot in WG currently seated in a vehicle.
    Player* FindBotInVehicle()
    {
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (!b || !b->IsInWorld() || b->GetZoneId() != WG_ZONE_ID)
                continue;
            if (b->GetVehicleBase())
                return b;
        }
        return nullptr;
    }

    // Fortress door approach (just outside GO 190375 at 5162.99,2841.23).
    constexpr float DOOR_X = 5152.0f, DOOR_Y = 2841.2f, DOOR_Z = 410.16f;

    // Nearest destructible WG fortification to a unit (walls, fortress door, last door, relic).
    GameObject* FindNearestFortification(Player* b)
    {
        for (uint32 e : { 190375u, 191810u, 190219u, 190220u, 190369u, 190370u, 192829u })
            if (GameObject* go = b->FindNearestGameObject(e, 200.0f))
                return go;
        return nullptr;
    }

    // Attacker bot closest to the fortress door (the one siegeprep parked there).
    Player* FindAttackerBotNearDoor(Battlefield* bf)
    {
        Player* best = nullptr; float bestD = 1e9f;
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (!b || !b->IsInWorld() || !b->IsAlive())
                continue;
            if (b->GetZoneId() != WG_ZONE_ID || b->GetTeamId() != bf->GetAttackerTeam())
                continue;
            float dx = b->GetPositionX() - DOOR_X, dy = b->GetPositionY() - DOOR_Y;
            float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = b; }
        }
        return best;
    }
}

class WintergraspBotsCommandScript : public CommandScript
{
public:
    WintergraspBotsCommandScript() : CommandScript("WintergraspBotsCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        // GM-only (SEC_GAMEMASTER); Console::Yes allows the worldserver console.
        static ChatCommandTable sub =
        {
            { "status",    HandleStatus,    SEC_GAMEMASTER, Console::Yes },
            { "buildtest", HandleBuildTest, SEC_GAMEMASTER, Console::Yes },
            { "hittest",   HandleHitTest,   SEC_GAMEMASTER, Console::Yes },
            { "hitcast",   HandleHitCast,   SEC_GAMEMASTER, Console::Yes },
            { "siegeprep",   HandleSiegePrep,   SEC_GAMEMASTER, Console::Yes },
            { "siegetest",   HandleSiegeTest,   SEC_GAMEMASTER, Console::Yes },
            { "siegefire",   HandleSiegeFire,   SEC_GAMEMASTER, Console::Yes },
            { "doorhp",      HandleDoorHp,      SEC_GAMEMASTER, Console::Yes },
            { "spelleffect", HandleSpellEffect, SEC_GAMEMASTER, Console::Yes },
            { "grantrank",   HandleGrantRank,   SEC_GAMEMASTER, Console::Yes },
            { "pathtest",    HandlePathTest,    SEC_GAMEMASTER, Console::Yes },
            { "pathprobe",   HandlePathProbe,   SEC_GAMEMASTER, Console::Yes },
            { "pathdump",    HandlePathDump,    SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable root = { { "wgbots", sub } };
        return root;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        WintergraspBotsDirector* dir = WintergraspBotsDirector::instance();
        if (!dir)
        {
            handler->SendSysMessage("WintergraspBots director is not loaded.");
            return true;
        }
        handler->SendSysMessage(dir->StatusText());
        return true;
    }

    // SPIKE: build a catapult on an attacker bot to exercise the "untested" OnCreatureCreate path.
    // Casts the catapult CREATE spell (56663) directly on the bot (summons 27881 + seats the bot as
    // driver, bot is the summoner so the core resolves a Player). Run right after `.bf start 1`
    // (attacker holds its 2 start workshops → vehicle cap 8). NOTE: PSendSysMessage is fmt-style ({}).
    static bool HandleBuildTest(ChatHandler* handler)
    {
        Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
        if (!bf || !bf->IsWarTime())
        {
            handler->SendSysMessage("buildtest: no active WG battle (use .bf start 1).");
            return true;
        }
        handler->PSendSysMessage("buildtest: attacker team={} | capA {}/{} capH {}/{}",
            uint32(bf->GetAttackerTeam()),
            bf->GetData(BATTLEFIELD_WG_DATA_VEHICLE_A), bf->GetData(BATTLEFIELD_WG_DATA_MAX_VEHICLE_A),
            bf->GetData(BATTLEFIELD_WG_DATA_VEHICLE_H), bf->GetData(BATTLEFIELD_WG_DATA_MAX_VEHICLE_H));

        Player* bot = FindAttackerBotToBuild(bf);
        if (!bot)
        {
            handler->SendSysMessage("buildtest: no eligible attacker bot in WG.");
            return true;
        }

        uint32 create = 56663; // catapult "Build" spell -> summons 27881 + HandleSpellClick(caster)
        bot->CastSpell(bot, create, true);
        handler->PSendSysMessage("buildtest: {} cast create {} at ({:.0f},{:.0f})",
                                 bot->GetName(), create, bot->GetPositionX(), bot->GetPositionY());

        if (Creature* v = bot->FindNearestCreature(27881, 60.0f))
            handler->PSendSysMessage("  -> catapult spawned entry={} hordeFlag={} allianceFlag={} botSeated={}",
                                     v->GetEntry(), v->HasAura(14267) ? 1 : 0, v->HasAura(14268) ? 1 : 0,
                                     bot->GetVehicleBase() ? 1 : 0);
        else
            handler->SendSysMessage("  -> no catapult spawned within 60y (see worldserver log)");
        return true;
    }

    // SPIKE: inspect the vehicle a bot just built — faction flag + weapon spell ids + nearest wall.
    static bool HandleHitTest(ChatHandler* handler)
    {
        Player* bot = FindBotInVehicle();
        if (!bot)
        {
            handler->SendSysMessage("hittest: no bot currently in a vehicle.");
            return true;
        }
        Unit* veh = bot->GetVehicleBase();
        handler->PSendSysMessage("hittest: {} in vehicle entry={} hordeFlag={} allianceFlag={}",
                                 bot->GetName(), veh->GetEntry(),
                                 veh->HasAura(14267) ? 1 : 0, veh->HasAura(14268) ? 1 : 0);
        if (Creature* c = veh->ToCreature())
            for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
                if (c->m_spells[i])
                    handler->PSendSysMessage("   m_spells[{}] = {}", i, c->m_spells[i]);

        if (GameObject* go = FindNearestFortification(bot))
            handler->PSendSysMessage("   nearest fortification {} health={} (dist {:.1f})",
                                     go->GetEntry(), go->GetGOValue()->Building.Health, bot->GetDistance(go));
        else
            handler->SendSysMessage("   no wall/door/relic GO nearby (drive closer).");
        return true;
    }

    // SPIKE: have the vehicle bot cast <spellId> at the nearest wall; report health delta.
    static bool HandleHitCast(ChatHandler* handler, uint32 spellId)
    {
        Player* bot = FindBotInVehicle();
        if (!bot)
        {
            handler->SendSysMessage("hitcast: no bot currently in a vehicle.");
            return true;
        }
        GameObject* go = FindNearestFortification(bot);
        if (!go)
        {
            handler->SendSysMessage("hitcast: no fortification GO nearby.");
            return true;
        }
        uint32 before = go->GetGOValue()->Building.Health;
        bot->CastSpell(go, spellId, false);
        handler->PSendSysMessage("hitcast: {} cast {} at GO {}  health {} -> {}",
                                 bot->GetName(), spellId, go->GetEntry(),
                                 before, go->GetGOValue()->Building.Health);
        return true;
    }

    // SPIKE: park an attacker bot at the fortress door so the next-tick siegetest builds beside a wall.
    static bool HandleSiegePrep(ChatHandler* handler)
    {
        Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
        if (!bf || !bf->IsWarTime()) { handler->SendSysMessage("siegeprep: no active WG battle."); return true; }
        Player* bot = FindAttackerBotToBuild(bf);
        if (!bot) { handler->SendSysMessage("siegeprep: no attacker bot."); return true; }
        bot->TeleportTo(571, DOOR_X, DOOR_Y, DOOR_Z, 0.0f);
        handler->PSendSysMessage("siegeprep: teleported {} to fortress door approach.", bot->GetName());
        return true;
    }

    // SPIKE step 1: build a DEMOLISHER on an attacker bot and NearTeleport its base right up to the
    // fortress door (NearTeleportTo is synchronous, so the bot+vehicle are positioned this tick).
    static bool HandleSiegeTest(ChatHandler* handler)
    {
        Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
        if (!bf || !bf->IsWarTime()) { handler->SendSysMessage("siegetest: no active WG battle."); return true; }
        Player* bot = FindAttackerBotToBuild(bf);
        if (!bot) { handler->SendSysMessage("siegetest: no attacker bot."); return true; }

        if (!bot->GetVehicleBase())
            bot->CastSpell(bot, 56575, true); // build DEMOLISHER (summons 28094 + seats)
        Unit* veh = bot->GetVehicleBase();
        if (!veh) { handler->SendSysMessage("siegetest: build failed, bot not seated."); return true; }

        veh->NearTeleportTo(DOOR_X, DOOR_Y, DOOR_Z, 3.0f); // move the demolisher up to the door (sync)

        GameObject* go = bot->FindNearestGameObject(190375, 150.0f);
        if (!go) go = bot->FindNearestGameObject(191810, 150.0f);
        if (!go) { handler->SendSysMessage("siegetest: positioned, but no door within 150y."); return true; }

        // Fire NOW via the IOC-proven path: set "bg siege" = wall pos, then CastVehicleSpell with the
        // VEHICLE BASE as target so spellTarget==vehicleBase and the projectile dest = bg-siege (wall).
        handler->PSendSysMessage("siegetest: {} in demolisher at door {} health={} dist={:.0f}; firing HurlBoulder.",
                                 bot->GetName(), go->GetEntry(), go->GetGOValue()->Building.Health, bot->GetDistance(go));
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (!ai) { handler->SendSysMessage("siegetest: no PlayerbotAI."); return true; }

        PositionMap& pm = ai->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
        PositionInfo sp = pm["bg siege"];
        sp.Set(go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), bot->GetMapId());
        pm["bg siege"] = sp;

        bool r1 = ai->CastVehicleSpell(50896, veh); // Hurl Boulder -> dest = bg siege (wall)
        bool r2 = ai->CastVehicleSpell(50896, veh); // 2nd call in case the 1st only faced/prepared
        handler->PSendSysMessage("siegetest: CastVehicleSpell(50896) returned {}/{}. Check .wgbots doorhp.",
                                 r1 ? 1 : 0, r2 ? 1 : 0);
        return true;
    }

    // SPIKE step 2: fire the demolisher's candidate weapons at the door via the REAL vehicle path
    // (PlayerbotAI::CastVehicleSpell = ballistic arc). Projectiles have travel time — check .wgbots
    // doorhp a few seconds later for the health drop.
    static bool HandleSiegeFire(ChatHandler* handler)
    {
        Player* bot = FindBotInVehicle();
        if (!bot) { handler->SendSysMessage("siegefire: no bot in a vehicle."); return true; }
        GameObject* go = bot->FindNearestGameObject(190375, 120.0f);
        if (!go) go = bot->FindNearestGameObject(191810, 120.0f);
        if (!go) { handler->PSendSysMessage("siegefire: no door within 120y (dist issue)."); return true; }

        handler->PSendSysMessage("siegefire: door {} health={} dist={:.0f}; firing Hurl Boulder (dest) + Ram (GO)",
                                 go->GetEntry(), go->GetGOValue()->Building.Health, bot->GetDistance(go));
        // Hurl Boulder (50896, effect 32) is dest-targeted -> cast at the door's ground location.
        bot->CastSpell(go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), 50896, false);
        // Ram (54107, effect 87) targets the structure -> cast at the GO.
        bot->CastSpell(go, 54107, false);
        return true;
    }

    // SPIKE: report the fortress/last door health (any in-zone bot as search origin).
    static bool HandleDoorHp(ChatHandler* handler)
    {
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (!b || !b->IsInWorld() || b->GetZoneId() != WG_ZONE_ID) continue;
            GameObject* go = b->FindNearestGameObject(190375, 1500.0f);
            if (!go) go = b->FindNearestGameObject(191810, 1500.0f);
            if (go) { handler->PSendSysMessage("doorhp: GO {} health={}", go->GetEntry(), go->GetGOValue()->Building.Health); return true; }
        }
        handler->SendSysMessage("doorhp: no door GO found.");
        return true;
    }

    // SPIKE: grant the Lieutenant rank aura to all in-zone attacker bots so the siege strategy can
    // build demolishers without waiting for the full battle-long rank grind. Re-run as bots arrive.
    static bool HandleGrantRank(ChatHandler* handler)
    {
        Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(WG_ZONE_ID);
        if (!bf || !bf->IsWarTime()) { handler->SendSysMessage("grantrank: no active WG battle."); return true; }
        uint32 n = 0;
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (!b || !b->IsInWorld() || b->GetZoneId() != WG_ZONE_ID) continue;
            if (b->GetTeamId() != bf->GetAttackerTeam()) continue;
            if (!b->HasAura(55629)) { b->AddAura(55629, b); ++n; } // SPELL_LIEUTENANT
        }
        handler->PSendSysMessage("grantrank: granted Lieutenant to {} attacker bots.", n);
        return true;
    }

    // DIAGNOSTIC (pathing investigation): ground-Z + PathGenerator report for every objective.
    static bool HandlePathTest(ChatHandler* handler)
    {
        WintergraspBotsDirector* dir = WintergraspBotsDirector::instance();
        if (!dir)
        {
            handler->SendSysMessage("WintergraspBots director is not loaded.");
            return true;
        }
        handler->SendSysMessage(dir->PathTest());
        return true;
    }

    // DIAGNOSTIC (road-route verification): arbitrary point-to-point navmesh probe with the SAME
    // acceptance rules the siege strategy's MoveByLeg applies (path type, per-4y-step climb guard
    // >2.5, deep-water guard at 1.5 wade depth, GO-WMO wall ray). Authored road waypoints MUST be
    // verified with this before shipping — the original Fix 4b coords were unverified estimates
    // and pinned every Eastspark vehicle at the middle bridge (runtime-observed oscillation).
    // Usage: .wgbots pathprobe x1 y1 x2 y2   (z is ground-snapped; grids are force-loaded, so no
    // battle or nearby player is needed — only ANY bot on map 571 to own the PathGenerator).
    static bool HandlePathProbe(ChatHandler* handler, float x1, float y1, float x2, float y2)
    {
        Map* map = sMapMgr->FindMap(571, 0);
        if (!map)
        {
            handler->SendSysMessage("pathprobe: map 571 not loaded");
            return true;
        }
        // PathGenerator needs an owner ON the map (for mmap tile context); any Northrend bot works.
        Player* owner = nullptr;
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (b && b->IsInWorld() && b->GetMapId() == 571)
            {
                owner = b;
                break;
            }
        }
        if (!owner)
        {
            handler->SendSysMessage("pathprobe: no bot on map 571 to own the path query");
            return true;
        }

        // Force-load grids along the probe line (segments can span several 533y grids).
        float dx = x2 - x1, dy = y2 - y1;
        float dist = std::sqrt(dx * dx + dy * dy);
        int steps = std::max(1, int(dist / 200.0f));
        for (int i = 0; i <= steps; ++i)
            map->LoadGrid(x1 + dx * i / steps, y1 + dy * i / steps);

        float z1 = map->GetHeight(PHASEMASK_NORMAL, x1, y1, 600.0f, true, 400.0f);
        float z2 = map->GetHeight(PHASEMASK_NORMAL, x2, y2, 600.0f, true, 400.0f);
        if (z1 <= INVALID_HEIGHT + 1.0f || z2 <= INVALID_HEIGHT + 1.0f)
        {
            handler->PSendSysMessage("pathprobe: no ground at an endpoint (z1={:.1f} z2={:.1f})", z1, z2);
            return true;
        }

        PathGenerator pg(owner);
        bool ok = pg.CalculatePath(x1, y1, z1, x2, y2, z2, false);
        auto const& pts = pg.GetPath();
        handler->PSendSysMessage(
            "pathprobe ({:.0f},{:.0f},{:.1f}) -> ({:.0f},{:.0f},{:.1f}) d2d={:.0f}: ok={} type=0x{:02x} pts={} end=({:.0f},{:.0f},{:.1f})",
            x1, y1, z1, x2, y2, z2, dist, ok ? 1 : 0, uint32(pg.GetPathType()), pts.size(),
            pg.GetActualEndPosition().x, pg.GetActualEndPosition().y, pg.GetActualEndPosition().z);
        if (pts.size() < 2)
            return true;

        // Mirror the siege strategy's SegmentClearWide checks along the whole path — PLUS a
        // swim-poly detector: the PathGenerator owner is a PLAYER (bots can swim), so paths may
        // legally cross deep water on NAV_WATER surface polys that a ground VEHICLE can never
        // use. Signature: path z floats far above the real ground (mesh z = water surface,
        // GetHeight = lakebed). This exact blind spot blessed the east lake channel as
        // "DRIVABLE" and stalled the first verified-chain battle at (4687,2282).
        float maxClimb = 0.0f;
        int firstClimb = -1, firstWater = -1, firstWall = -1, firstSwim = -1;
        for (size_t i = 1; i < pts.size(); ++i)
        {
            G3D::Vector3 const& a = pts[i - 1];
            G3D::Vector3 const& b = pts[i];
            float climb = b.z - a.z;
            if (climb > maxClimb)
                maxClimb = climb;
            if (firstClimb < 0 && climb > 2.5f)
                firstClimb = int(i);
            if (firstWater < 0 && map->IsUnderWater(PHASEMASK_NORMAL, b.x, b.y, b.z, 3.0f))
                firstWater = int(i);
            if (firstSwim < 0)
            {
                float gz = map->GetHeight(PHASEMASK_NORMAL, b.x, b.y, b.z + 2.0f, true, 60.0f);
                if (gz > INVALID_HEIGHT + 1.0f && b.z - gz > 2.5f)
                    firstSwim = int(i);   // floating above real ground: swim surface / no deck
            }
            if (firstWall < 0 && !map->isInLineOfSight(a.x, a.y, a.z + 2.0f, b.x, b.y, b.z + 2.0f,
                                                       PHASEMASK_NORMAL, LINEOFSIGHT_CHECK_GOBJECT_WMO,
                                                       VMAP::ModelIgnoreFlags::Nothing))
                firstWall = int(i);
        }
        auto at = [&pts](int i) { return Acore::StringFormat("({:.0f},{:.0f},{:.1f})", pts[i].x, pts[i].y, pts[i].z); };
        std::string out = Acore::StringFormat("  maxClimb/step={:.2f}", maxClimb);
        if (firstClimb >= 0) out += " CLIMB-BLOCK@" + std::to_string(firstClimb) + " " + at(firstClimb);
        if (firstWater >= 0) out += " WATER-BLOCK@" + std::to_string(firstWater) + " " + at(firstWater);
        if (firstSwim  >= 0) out += " SWIM-BLOCK@"  + std::to_string(firstSwim)  + " " + at(firstSwim);
        if (firstWall  >= 0) out += " WALL-BLOCK@"  + std::to_string(firstWall)  + " " + at(firstWall);
        bool badType = pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH | PATHFIND_INCOMPLETE);
        out += (firstClimb < 0 && firstWater < 0 && firstSwim < 0 && firstWall < 0 && !badType)
                   ? " => DRIVABLE" : " => BLOCKED";
        handler->SendSysMessage(out);
        return true;
    }

    // DIAGNOSTIC: like pathprobe, but prints every smooth-path point with per-step climb/water
    // flags. The navmesh's NORMAL paths hug the real roads, so dumping a path between two known
    // road points REVEALS the road line — used to author verified road waypoints without
    // guess-and-bisect probing.
    static bool HandlePathDump(ChatHandler* handler, float x1, float y1, float x2, float y2)
    {
        Map* map = sMapMgr->FindMap(571, 0);
        if (!map)
        {
            handler->SendSysMessage("pathdump: map 571 not loaded");
            return true;
        }
        Player* owner = nullptr;
        for (auto const& itr : sRandomPlayerbotMgr.GetAllBots())
        {
            Player* b = itr.second;
            if (b && b->IsInWorld() && b->GetMapId() == 571)
            {
                owner = b;
                break;
            }
        }
        if (!owner)
        {
            handler->SendSysMessage("pathdump: no bot on map 571 to own the path query");
            return true;
        }
        float dx = x2 - x1, dy = y2 - y1;
        float dist = std::sqrt(dx * dx + dy * dy);
        int steps = std::max(1, int(dist / 200.0f));
        for (int i = 0; i <= steps; ++i)
            map->LoadGrid(x1 + dx * i / steps, y1 + dy * i / steps);
        float z1 = map->GetHeight(PHASEMASK_NORMAL, x1, y1, 600.0f, true, 400.0f);
        float z2 = map->GetHeight(PHASEMASK_NORMAL, x2, y2, 600.0f, true, 400.0f);
        PathGenerator pg(owner);
        pg.CalculatePath(x1, y1, z1, x2, y2, z2, false);
        auto const& pts = pg.GetPath();
        handler->PSendSysMessage("pathdump type=0x{:02x} pts={}", uint32(pg.GetPathType()), pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
        {
            float climb = i ? pts[i].z - pts[i - 1].z : 0.0f;
            bool water = map->IsUnderWater(PHASEMASK_NORMAL, pts[i].x, pts[i].y, pts[i].z, 3.0f);
            // Ground beneath the path point: a large gap means a swim-surface poly (see
            // pathprobe's SWIM-BLOCK) — print it so road reads can't mistake water for road.
            float gz = map->GetHeight(PHASEMASK_NORMAL, pts[i].x, pts[i].y, pts[i].z + 2.0f, true, 60.0f);
            bool swim = gz > INVALID_HEIGHT + 1.0f && pts[i].z - gz > 2.5f;
            handler->PSendSysMessage("  [{}] ({:.0f},{:.0f},{:.1f}) gz={:.1f} climb={:.2f}{}{}{}",
                                     uint32(i), pts[i].x, pts[i].y, pts[i].z, gz, climb,
                                     climb > 2.5f ? " CLIMB" : "", water ? " WATER" : "", swim ? " SWIM" : "");
        }
        return true;
    }

    // SPIKE: print a spell's 3 effect ids + name from the core's loaded SpellInfo (effect 87 =
    // SPELL_EFFECT_GAMEOBJECT_DAMAGE identifies the wall weapon). No bot needed.
    static bool HandleSpellEffect(ChatHandler* handler, uint32 id)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(id);
        if (!si) { handler->PSendSysMessage("spelleffect: spell {} not found", id); return true; }
        char const* nm = si->SpellName[0];
        handler->PSendSysMessage("spelleffect: {} '{}' eff = {} / {} / {}",
                                 id, nm ? nm : "?",
                                 uint32(si->GetEffect(EFFECT_0).Effect),
                                 uint32(si->GetEffect(EFFECT_1).Effect),
                                 uint32(si->GetEffect(EFFECT_2).Effect));
        return true;
    }
};

void AddWintergraspBotsCommandScripts()
{
    new WintergraspBotsCommandScript();
}
