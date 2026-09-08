#ifndef MOD_PB_CHATTER_CHANNEL_POLICY_H
#define MOD_PB_CHATTER_CHANNEL_POLICY_H

#include "PBChatterAmbient.h"

namespace PBChatterChannelPolicy
{
    // roll is in [0, 99]. This is a preference on public ambient opportunities,
    // not a promise that this percentage of all delivered lines will be in raid.
    inline bool PreferRaid(uint8_t kind, bool groupEnabled, bool humanRaid,
        uint32_t chance, uint32_t roll)
    {
        return (kind == AMB_ZONE || kind == AMB_GUILD) && groupEnabled && humanRaid &&
            roll < (chance > 100 ? 100 : chance);
    }

    inline bool SameGroup(uint64_t expected, uint64_t botGroup, uint64_t anchorGroup)
    {
        return expected != 0 && expected == botGroup && expected == anchorGroup;
    }
}

#endif
