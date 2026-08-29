#ifndef MOD_PB_CHATTER_EVENTS_H
#define MOD_PB_CHATTER_EVENTS_H

#include <cstdint>
#include <string>
#include <vector>

class Group;
class PlayerScript;

namespace PBChatterEvents
{
    // World thread. Processes event-triggered LLM chatter and scans real-player groups
    // for newly visible opposing-faction players.
    void Tick(uint32_t diff);

    // World thread. If the bot has a fresh (<=120s) event seed, copies its phrase into
    // outHint and returns true; consumes the seed either way. botGuidCounter = counter.
    bool Take(uint64_t botGuidCounter, uint32_t nowMs, std::string& outHint);

    // World thread. Recent party/raid happenings for prompt grounding: normal mob kills,
    // notable kills, quest turn-ins, level-ups, and safe item-store notifications. Returns
    // newest relevant events in chronological order; expired events are forgotten.
    std::vector<std::string> RecentForGroup(Group* group, uint32_t nowMs, uint32_t maxItems = 6);
}

// Registered by the loader. Stamps a short-lived "last event" phrase on bots.
PlayerScript* PBChatterMakeEventScript();

#endif
