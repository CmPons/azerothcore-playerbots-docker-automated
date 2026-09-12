#include "RaidScalingMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
    std::vector<std::string> Tokenize(char const* args)
    {
        std::vector<std::string> tokens;
        if (!args)
            return tokens;

        std::istringstream in(args);
        std::string token;
        while (in >> token)
            tokens.push_back(token);
        return tokens;
    }

    bool ParseUInt(std::string const& s, uint32& out)
    {
        char* end = nullptr;
        unsigned long value = std::strtoul(s.c_str(), &end, 10);
        if (!end || *end != '\0')
            return false;
        out = uint32(value);
        return true;
    }

    bool ParseFloat(std::string const& s, float& out)
    {
        char* end = nullptr;
        float value = std::strtof(s.c_str(), &end);
        if (!end || *end != '\0')
            return false;
        out = value;
        return true;
    }
}

class RaidScaleCommandScript : public CommandScript
{
public:
    RaidScaleCommandScript() : CommandScript("RaidScaleCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable root =
        {
            { "raidscale", HandleRaidScale, sRaidScalingMgr.CommandSecurity(), Console::No },
            { "raidinstance", HandleRaidInstance, sRaidScalingMgr.CommandSecurity(), Console::No },
            { "raidtwinsreset", HandleTwinsReset, sRaidScalingMgr.CommandSecurity(), Console::Yes },
        };
        return root;
    }

    static bool HandleTwinsReset(ChatHandler* handler, char const* args)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("Use .raidinstance boss reset 7 in-world; raidtwinsreset is console-only.");
            return true;
        }
        std::vector<std::string> tokens = Tokenize(args);
        if (tokens.size() != 2 || tokens[1] != "confirm" || tokens[0].empty() ||
            tokens[0].find_first_not_of("0123456789") != std::string::npos || tokens[0].size() > 10)
        {
            handler->SendSysMessage("Usage: raidtwinsreset <existing-AQ40-instance-id> confirm");
            return true;
        }
        unsigned long id = std::strtoul(tokens[0].c_str(), nullptr, 10);
        if (!id || id > std::numeric_limits<uint32>::max())
        {
            handler->SendSysMessage("Twins reset refused: invalid instance ID.");
            return true;
        }
        sRaidScalingMgr.RequestTwinsReset(handler, uint32(id));
        return true;
    }

    static bool HandleRaidScale(ChatHandler* handler, char const* args)
    {
        if (!sRaidScalingMgr.Enabled())
        {
            handler->SendSysMessage("RaidScaling is disabled.");
            return true;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("Run this in-world as a player.");
            return true;
        }

        Map* map = player->GetMap();
        std::vector<std::string> tokens = Tokenize(args);
        if (tokens.empty() || tokens[0] == "status")
        {
            sRaidScalingMgr.SendStatus(handler, map);
            return true;
        }

        if (tokens[0] == "off" || tokens[0] == "disable")
        {
            sRaidScalingMgr.DisableForMap(map, handler);
            return true;
        }

        if (tokens[0] == "export")
        {
            sRaidScalingMgr.Export(handler, map);
            return true;
        }

        if (tokens.size() == 3)
        {
            float value = 0.0f;
            if (!ParseFloat(tokens[2], value))
            {
                handler->SendSysMessage("Usage: .raidscale boss|trash hp|damage <multiplier>");
                return true;
            }

            sRaidScalingMgr.SetMultiplier(map, tokens[0], tokens[1], value, handler);
            return true;
        }

        uint32 targetPlayers = 0;
        if (ParseUInt(tokens[0], targetPlayers))
        {
            sRaidScalingMgr.EnableForMap(map, targetPlayers, handler);
            return true;
        }

        handler->SendSysMessage("Usage: .raidscale 10 | off | status | boss|trash hp|damage <multiplier> | export");
        return true;
    }

    static bool HandleRaidInstance(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("Run this in-world as a player.");
            return true;
        }

        std::vector<std::string> tokens = Tokenize(args);
        if (tokens.size() >= 2 && tokens[0] == "boss" && tokens[1] == "list")
        {
            InstanceMap* map = player->GetMap() ? player->GetMap()->ToInstanceMap() : nullptr;
            if (!map || !map->IsRaid())
            {
                handler->SendSysMessage("Stand inside a raid instance first.");
                return true;
            }

            sRaidScalingMgr.SendBossList(handler, map);
            return true;
        }

        if (tokens.size() >= 3 && tokens[0] == "boss" && tokens[1] == "reset")
        {
            uint32 id = 0;
            if (!ParseUInt(tokens[2], id))
            {
                handler->SendSysMessage("Usage: .raidinstance boss reset <id>");
                return true;
            }

            InstanceMap* map = player->GetMap() ? player->GetMap()->ToInstanceMap() : nullptr;
            if (!map || !map->IsRaid())
            {
                handler->SendSysMessage("Stand inside a raid instance first.");
                return true;
            }

            sRaidScalingMgr.ResetBoss(handler, map, id);
            return true;
        }

        if (tokens.size() >= 2 && tokens[0] == "reset" && tokens[1] == "all")
        {
            bool confirm = tokens.size() >= 3 && tokens[2] == "confirm";
            sRaidScalingMgr.ResetGroupBinds(handler, player, confirm);
            return true;
        }

        handler->SendSysMessage("Usage: .raidinstance boss list | boss reset <id> | reset all [confirm]");
        return true;
    }
};

void AddRaidScalingCommandScripts()
{
    new RaidScaleCommandScript();
}
