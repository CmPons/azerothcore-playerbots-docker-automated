#ifndef MOD_ARENA_ROSTER_CONFIG_H
#define MOD_ARENA_ROSTER_CONFIG_H
#include <cstdint>
#include <array>

extern bool   g_ArEnable;
extern bool   g_ArDebug;
extern std::array<uint32_t, 3> g_ArTierCeilings;  // upper bound of tiers 1..3
extern std::array<uint32_t, 4> g_ArTierRatings;   // pinned rating per tier 1..4
extern uint32_t g_ArGraceLogoutSecs;

void ArenaRosterLoadConfig();
// Tier index 0..3 for a team rating.
uint8_t ArenaRosterTierFor(uint32_t rating);
#endif
