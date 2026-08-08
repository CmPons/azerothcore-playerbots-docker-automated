#ifndef MOD_ARENA_ROSTER_COMMAND_H
#define MOD_ARENA_ROSTER_COMMAND_H
#include "CommandScript.h"
#include "Chat.h"

class ArenaRosterCommand : public CommandScript
{
public:
    ArenaRosterCommand() : CommandScript("ArenaRosterCommand") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleCreate(ChatHandler* handler);
    static bool HandleGo(ChatHandler* handler, std::string bracket, Acore::ChatCommands::Tail classes);
    static bool HandleSync(ChatHandler* handler);
    static bool HandleSpec(ChatHandler* handler, std::string className, std::string specName);
    static bool HandleLogout(ChatHandler* handler);
    static bool HandleStatus(ChatHandler* handler);
    static bool HandleRemove(ChatHandler* handler, Optional<std::string> confirm);
    static bool HandlePoolInit(ChatHandler* handler);
    static bool HandlePoolStatus(ChatHandler* handler);
    static bool HandleForceQueue(ChatHandler* handler, uint8 tier, Optional<uint8> type);   // TEST-ONLY
};
#endif
