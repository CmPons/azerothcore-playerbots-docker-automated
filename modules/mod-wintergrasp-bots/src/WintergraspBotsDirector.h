#ifndef MOD_WINTERGRASP_BOTS_DIRECTOR_H
#define MOD_WINTERGRASP_BOTS_DIRECTOR_H

#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"   // TeamId
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

class Battlefield;
class Player;
class Unit;

// World-thread director: during a Wintergrasp battle it keeps both factions populated with
// eligible random bots, assigns each bot an objective (workshop to capture / role anchor / relic
// room), and moves it there under its own feet — teleporting only when a bot is genuinely stuck.
// Registered once via `new` in the loader and owned by ScriptMgr.
class WintergraspBotsDirector : public WorldScript
{
public:
    WintergraspBotsDirector();

    void OnStartup() override;
    void OnUpdate(uint32 diff) override;

    // Returns the SAME object the loader `new`s (self-pointer set in the ctor), so the command
    // reads live director state — NOT a second, unregistered instance.
    static WintergraspBotsDirector* instance();

    std::string StatusText() const;
    std::string PathTest() const;   // DIAGNOSTIC: ground-Z + PathGenerator report for every objective

private:
    // Per-bot travel bookkeeping for the movement engine + stuck detector. Shared by the
    // foot path (_travel, StepBot) and the driven-vehicle watchdog (_vehTravel, WatchVehicle):
    // patrolIdx/dwellMs are foot-path-only, and unstickHolds means "consecutive withheld
    // (<min-dist) unstick attempts" on the foot path but "consecutive failed escape attempts
    // (fan-widening)" on the vehicle path.
    struct TravelState
    {
        Position last;          // last sampled position while traveling
        uint32   stuckMs = 0;   // accumulated no-progress time while traveling
        bool     has     = false;
        uint8  patrolIdx = 0;   // next patrol-ring point (foot only)
        uint32 dwellMs   = 0;   // time held at the objective since the last patrol move (foot only)
        uint8  unstickHolds = 0;   // withheld unsticks (foot) / failed escape attempts (vehicle)
    };

    void Reconcile();
    void MovementTick();   // 1s sub-tick: re-leg traveling bots the moment a leg ends (anti-stutter)
    void ReleaseAll();

    // WG-state readers (module holds a base Battlefield*; see plan for the GetData/cast facts).
    TeamId WorkshopController(Battlefield* bf, uint32 areaId) const;  // controlling team of one workshop
    bool   RelicOpen(Battlefield* bf) const;                         // last door down => relic clickable
    int    CountInZoneBots(TeamId team) const;                       // all in-zone bots of a team (over-fill fix)

    // Objective planning + execution. Jobs are priority-ordered posts with a headcount; bots are
    // assigned nearest-first, so the force composition reacts to the battle every tick (siege
    // near the keep -> rally the threat cluster; wall falls -> push deeper; workshop flips -> re-task).
    enum WgRankFilter : uint8 { WG_RANK_ANY = 0, WG_RANK_LIEUTENANT = 1, WG_RANK_RANKLESS = 2 };
    enum WgJobType    : uint8 { WG_JOB_GENERIC = 0, WG_JOB_CREW = 1, WG_JOB_PRODUCTION = 2 };
    struct WgJob
    {
        Position pos;
        int want;
        WgRankFilter rank = WG_RANK_ANY;    // who may take this job
        WgJobType    type = WG_JOB_GENERIC; // crew seats a cannon; production/crew stand still
    };

    // Live battle snapshot, rebuilt at the top of every Reconcile — job generation reads THIS
    // instead of fixed anchors, which is what makes the force reactive.
    struct WgBattleState
    {
        bool     hasVanguard = false;
        Position vanguard;              // crewed attacker vehicle closest to the keep gate
        uint32   threatCount = 0;       // enemy vehicles within ThreatRadius of the gate
        Position threat;                // their centroid (ground-snapped)
        bool     wallUnderFire = false; // an axis structure lost health since last tick
        Position firedWall;             // rally fallback: outside face of that structure
        int      lieutenants = 0;       // managed attackers with SPELL_LIEUTENANT
        int      lieutenantsWanted = 0; // vehicle cap - human reserve
    };
    void BuildBattleState(Battlefield* bf, std::vector<Position> const& convoy);

    std::vector<WgJob> BuildJobs(Battlefield* bf, TeamId team, int aliveCount,
                                 std::vector<Position> const& convoy) const;   // convoy: crewed attacker engines
    void StepBot(Player* bot, Position const& tgt, bool fullTick = true, bool holdStill = false);   // move toward tgt / unstick
    int  EngageNearbyEnemy(Player* bot, std::vector<Player*> const& hostiles,
                           Position& chase) const;                  // 0 none / 1 attacking / 2 chase(out)
    bool TrySeatCannon(Player* bot) const;                           // defender at a cannon post -> man the turret
    bool MaintainCannonTarget(Player* bot) const;            // manned cannon -> keep a vehicle targeted (false = nothing in range)
    bool EnforceBreachFrontier(Player* bot);                 // attacker clipped past an intact gate/wall -> snap back outside
    void WatchVehicle(Player* bot, Unit* veh);               // driven vehicle: bay-wedge watchdog + unstick
    int  RallyWant(int aliveCount) const;                    // rally headcount from the battle state
    void UpdateSortie(Battlefield* bf);                      // muster-then-sortie release/regroup state
    void UpdateGarrison(Battlefield* bf);                            // field guard posts (workshops/bridges)
    void ScrubPetTargets(Player* bot) const;                 // Fix 1: strip FRIENDLY targets from a bot's combat pets
    bool TryMountBot(Player* bot) const;                     // Fix 7: instant, uninterruptible ground mount from spellbook
    bool TryRecrewVehicle(Player* bot) const;                // Fix 2b: seat a foot bot into an idle friendly siege vehicle
    void ReapIdleVehicles(Battlefield* bf);                  // Fix 2c: kill long-abandoned unmanned vehicles (frees the cap)

    static WintergraspBotsDirector* s_instance;

    uint32 _accumMs = 0;
    uint32 _moveAccumMs = 0;   // fast movement sub-tick accumulator
    std::unordered_set<ObjectGuid> _managed[2];               // [TEAM_ALLIANCE], [TEAM_HORDE]
    std::unordered_set<ObjectGuid> _activated;                // bots we've ResetStrategies'd after WG arrival
    std::unordered_map<ObjectGuid, Position> _objective;      // bot -> assigned objective point
    std::unordered_map<ObjectGuid, TravelState> _travel;      // bot -> movement/stuck bookkeeping
    std::unordered_map<uint8, std::vector<ObjectGuid>> _garrison;   // field post id -> guard squad
    std::unordered_map<uint8, TeamId> _garrisonOwner;               // field post id -> squad faction
    std::unordered_map<ObjectGuid, uint8> _gunnerIdle;       // seated gunner -> consecutive no-target reconciles
    std::unordered_map<ObjectGuid, TravelState> _vehTravel;  // seated driver -> vehicle stuck bookkeeping
    std::unordered_map<ObjectGuid, uint32> _idleVeh;         // unmanned friendly vehicle GUID -> ms idle (reap timer)

    bool   _sortieReleased = false;  // muster-then-sortie: the rally wave has been released
    uint32 _sortieAgeMs    = 0;      // ms since release (wave-wipe checks wait out the exit)
    uint8  _sortieLowTicks = 0;      // consecutive reconciles with the released wave absent from the rally

    WgBattleState _state;                                    // rebuilt each Reconcile
    std::unordered_map<uint32, uint32> _wallHealth;          // axis GO entry -> last seen health

    // Status snapshot, refreshed each reconcile for the .wgbots status command.
    int  _inZone[2]     = {0, 0};
    int  _controlled[2] = {0, 0};
    bool _relicOpen     = false;
    bool _battleActive  = false;
    uint8 _breachStage  = 0;    // 0 fortress door intact / 1 door down / 2 mid wall down (attacker push depth)
    TeamId _attackerTeam = TEAM_NEUTRAL;   // refreshed each Reconcile; StepBot's sally hop is defender-only
    bool _siegeVehiclesActive = false;     // crewed attacker siege vehicles exist -> defenders hunt them on the fast tick
};

#endif
