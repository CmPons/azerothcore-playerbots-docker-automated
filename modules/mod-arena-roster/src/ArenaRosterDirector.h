#ifndef MOD_ARENA_ROSTER_DIRECTOR_H
#define MOD_ARENA_ROSTER_DIRECTOR_H

#include "ScriptMgr.h"
#include "ArenaRosterStore.h"
#include <array>
#include <string>
#include <vector>

class ArenaTeam;
class Player;

// World-thread director for the opponent ladder. Pool creation (ArenaTeam::Create needs the
// captain ONLINE and level >= 70 — 80 for our gear — and AddPlayerBot logins are asynchronous)
// cannot run inside one command handler, so `.arenaroster poolinit` starts a state machine
// here that advances across world ticks: per tier 1..4 it pins 15 addclass bots, logs them in,
// levels/talents/gears them, builds the 15 bracket teams, then logs the tier back out.
class ArenaRosterDirector : public WorldScript
{
public:
    ArenaRosterDirector();
    static ArenaRosterDirector* instance;

    void OnUpdate(uint32 diff) override;

    bool StartPoolInit(std::string& err);
    std::string PoolStatusText() const;

    // TEST-ONLY (`.arenaroster forcequeue`): field a tier team without a real player queueing.
    bool ForceQueue(uint8 tier, uint8 arenaType, std::string& err);

private:
    enum class PoolPhase : uint8
    {
        Idle,          // no job has run since startup
        PinBots,       // choose + persist this tier's 15 bots, submit logins
        LoginWave,     // wait for all 15 to be online (timeout -> Failed)
        PrepareBots,   // level 80 + talents + season gear, batched per tick
        CreateTeams,   // build the tier's 15 teams, one bracket per tick
        LogoutWave,    // log the tier out; next tier or Done
        Done,
        Failed,
    };

    // Brackets per tier: 2v2 / 3v3 / 5v5 — must match ArenaPoolRow::team[3] (both extents
    // static_assert'd in the constructor, where the private members are visible).
    static constexpr uint8 BRACKET_COUNT = 3;

    // One served opponent lineup, driven across world ticks by TickEngagement. One player on a
    // LAN — one engagement at a time (TickQueueWatch stops scanning while one is active).
    struct Engagement
    {
        bool   active = false;
        uint8  arenaType = 0;                // 2/3/5
        uint8  tier = 0;                     // 1-based tier served (cached at serve time so
                                             // renormalize never re-reads the pool table)
        uint32 poolTeamId = 0;
        ObjectGuid captainGuid;
        std::vector<ObjectGuid> bots;        // whole lineup, captain included
        enum class St : uint8 { LoggingIn, Queueing, Fighting, Draining } st = St::LoggingIn;
        uint32 stateMs = 0;                  // elapsed in the current state (timeout guard)
        uint32 graceMs = 0;                  // Draining: elapsed since the match ended
        ObjectGuid bmGuid;                   // resolved arena battlemaster (queue packet target)
        bool   bmTeleported = false;         // a battlemaster teleport is in flight/settled
        uint8  bmCacheIdx = 0;               // next battlemaster cache entry to try on failure
        bool   grouped = false;              // Queueing sub-step 2 done
        bool   queued = false;               // Queueing sub-step 3 done (packet sent)
    };

    // True while a poolinit job is mid-flight — the shared guard for the OnUpdate tick gate
    // and both sides of the poolinit <-> engagement mutual exclusion.
    bool PoolBusy() const;
    void TickPoolInit(uint32 elapsedMs);
    // Task 8: watch REAL players' rated-arena queues and field the matching tier team.
    void TickQueueWatch(uint32 elapsedMs);
    void TickEngagement(uint32 elapsedMs);
    bool ServeOpponents(uint8 tier, uint8 arenaType, std::string& err);
    void TickEngagementQueueing(Player* captain);
    bool GroupEngagementTeam(Player* captain);
    void TeleportCaptainToBattlemaster(Player* captain);
    void RenormalizeServedTeam();
    void AbortEngagement(std::string why);
    // Pin `rating` (clamped to uint16) on the team + every member's PersonalRating/MMR/MaxMMR
    // and persist — shared by CreateOneTeam and the post-match renormalize.
    static void PinTeamRating(ArenaTeam* team, uint32 rating);
    void PhasePinBots();
    void PhaseLoginWave(uint32 elapsedMs);
    void PhasePrepareBots(uint32 elapsedMs);
    void PhaseCreateTeams();
    void PhaseLogoutWave();
    void Fail(std::string msg);
    ArenaPoolRow& TierRow(uint8 rosterIdx);   // row of the tier in progress
    bool CreateOneTeam(uint8 type, uint8 bracketSlot, char const* baseName,
                       uint8 const* indices, size_t count, std::string& err);
    static char const* PhaseName(PoolPhase phase);
    // Log out every currently-online bot in `rows` (tierFilter 0 = all tiers). Shared by
    // LogoutWave, Fail() (a failed job must never strand its masterless bots online — they
    // sit outside random-bot churn) and StartPoolInit's partial rebuild. The engagement's
    // Draining state logs out its cached guid vector directly (same LogoutPlayerBot call).
    static void LogoutPoolBots(std::vector<ArenaPoolRow> const& rows, uint8 tierFilter);

    PoolPhase _poolPhase = PoolPhase::Idle;
    uint8  _tierInProgress = 0;    // 1..TIER_COUNT while a job runs
    uint32 _poolPhaseTimerMs = 0;  // elapsed in the current timed phase (login/prepare)
    uint8  _prepIdx = 0;           // next roster idx PrepareBots processes
    uint8  _bracketStep = 0;       // CreateTeams sub-step: 0=2v2, 1=3v3, 2=5v5
    std::string _poolError;
    std::vector<ArenaPoolRow> _rows;   // all tiers pinned so far this job
    // Per roster idx: team ids of the tier in progress (outer extent asserted against
    // ARENA_POOL_ROSTER.size() in the constructor).
    std::array<std::array<uint32, BRACKET_COUNT>, 15> _teamIds{};

    Engagement _eng;   // one engagement at a time
    // Per (tier, bracket): last lineup served, so the rotation survives alternating tiers in
    // one bracket (outer extent 4 asserted against TIER_COUNT in the constructor, same
    // pattern as _teamIds vs ARENA_POOL_ROSTER).
    std::array<std::array<uint32, BRACKET_COUNT>, 4> _lastServedTeam{};   // [tier-1][bracket]

    uint32 _accumMs = 0;           // 1s cadence accumulator
};

#endif
