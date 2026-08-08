// mod-wintergrasp-bots — playerbots join Wintergrasp battles.
// AGPL v3, same as AzerothCore.
#include "Log.h"
#include "WintergraspBotsDirector.h"

void AddWintergraspBotsCommandScripts();   // fwd decl (defined in WintergraspBotsCommand.cpp)

void Addmod_wintergrasp_botsScripts()
{
    LOG_INFO("server.loading", "[WintergraspBots] Registering scripts.");
    new WintergraspBotsDirector();       // WorldScript self-registers
    AddWintergraspBotsCommandScripts();
}
