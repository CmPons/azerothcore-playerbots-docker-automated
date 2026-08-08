#ifndef MOD_AHBOT_PRICE_COMMAND_H
#define MOD_AHBOT_PRICE_COMMAND_H
#include "CommandScript.h"
#include "Chat.h"

class AHPriceCommand : public CommandScript
{
public:
    AHPriceCommand() : CommandScript("AHPriceCommand") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleSearch(ChatHandler* handler, Acore::ChatCommands::Tail text);
    static bool HandleItem(ChatHandler* handler, uint32 itemId);
};
#endif
