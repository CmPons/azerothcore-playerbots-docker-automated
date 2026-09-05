#ifndef MOD_PB_CHATTER_PERSONA_H
#define MOD_PB_CHATTER_PERSONA_H

#include <cstdint>
#include <string>

class Player;

namespace PBChatterPersona
{
    void Initialize();
    std::string BuildPromptBlock(Player* bot);
}

#endif
