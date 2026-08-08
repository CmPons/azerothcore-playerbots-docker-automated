#include "ScriptMgr.h"
#include "Log.h"
#include "ArenaRosterConfig.h"
#include "ArenaRosterComp.h"  // comp tables (ARENA_PARTNER_DEFAULTS, ARENA_POOL_* rosters/teams)
#include "ArenaRosterCommand.h"
#include "ArenaRosterDirector.h"

class ArenaRosterWorldBootstrap : public WorldScript
{
public:
    ArenaRosterWorldBootstrap() : WorldScript("ArenaRosterWorldBootstrap") { }
    void OnAfterConfigLoad(bool /*reload*/) override { ArenaRosterLoadConfig(); }
};

void Addmod_arena_rosterScripts()
{
    // Canary token "AR-v3" — bump per code drop; grep it in the startup log to prove
    // the running build contains this copy of the module (stale-copy trap).
    LOG_INFO("server.loading", "[ArenaRoster] Registering scripts (AR-v3).");
    new ArenaRosterWorldBootstrap();
    new ArenaRosterCommand();
    new ArenaRosterDirector();   // poolinit state machine + (Task 8) queue watcher
}
