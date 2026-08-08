#ifndef MOD_ARENA_ROSTER_GEAR_H
#define MOD_ARENA_ROSTER_GEAR_H
#include <cstdint>
class Player;

// WotLK PvP season tiers, keyed by the master's average item level so partners are geared
// "to you": a fresh-80 master gets Savage-tier partners, an ICC-geared one gets Wrathful.
enum ArenaSeason : uint8_t
{
    SEASON_SAVAGE     = 0,  // S5 honor tier   (~ilvl 200)
    SEASON_DEADLY     = 1,  // S5 arena        (~ilvl 213)
    SEASON_FURIOUS    = 2,  // S6              (~ilvl 232)
    SEASON_RELENTLESS = 3,  // S7              (~ilvl 245)
    SEASON_WRATHFUL   = 4,  // S8              (~ilvl 264)
};

namespace ArenaRosterGear
{
    ArenaSeason SeasonForItemLevel(float avgIlvl);
    char const* SeasonPrefix(ArenaSeason season);   // "Deadly Gladiator" etc. (log/message use too)

    // Strip the bot's equipped gear and re-equip it in `season` PvP set/off-set pieces for its
    // class — armor/jewelry and weapons/ranged/relic are all ranked by the playerbots stat
    // calculator, then the PlayerbotFactory enchant/gem pass runs. The scoring reads the bot's
    // ACTIVE talents, so force talents BEFORE calling; `specTab` is informational (log line
    // only). Bot must be in-world and level 80: every candidate the engine can produce
    // requires level 80, so a lower-level bot would be stripped and re-gear nothing.
    // Returns true when the result looks like a real set (>= 8 pieces equipped).
    bool EquipSeason(Player* bot, uint8_t specTab, ArenaSeason season);
}
#endif
