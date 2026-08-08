#ifndef MOD_ARENA_ROSTER_COMP_H
#define MOD_ARENA_ROSTER_COMP_H
#include "SharedDefines.h"
#include "PlayerbotAI.h"   // *_TAB_* spec enums
#include <array>
#include <cstdint>

struct ArenaPartnerDef { uint8 cls; uint8 specTab; };

// Default PvP spec per class for the partner pool (overridable via .arena spec).
inline constexpr std::array<ArenaPartnerDef, 10> ARENA_PARTNER_DEFAULTS = {{
    { CLASS_WARRIOR,      WARRIOR_TAB_ARMS         },
    { CLASS_PALADIN,      PALADIN_TAB_HOLY         },
    { CLASS_HUNTER,       HUNTER_TAB_MARKSMANSHIP  },
    { CLASS_ROGUE,        ROGUE_TAB_COMBAT         },
    { CLASS_PRIEST,       PRIEST_TAB_DISCIPLINE    },
    { CLASS_DEATH_KNIGHT, DEATH_KNIGHT_TAB_UNHOLY  },
    { CLASS_SHAMAN,       SHAMAN_TAB_RESTORATION   },
    { CLASS_MAGE,         MAGE_TAB_FROST           },
    { CLASS_WARLOCK,      WARLOCK_TAB_AFFLICTION   },
    { CLASS_DRUID,        DRUID_TAB_RESTORATION    },
}};

struct ArenaPoolSlot { uint8 cls; uint8 specTab; bool healer; };

// One tier's 15-bot roster: 4 healers (slots 0-3) + 11 DPS (slots 4-14).
inline constexpr std::array<ArenaPoolSlot, 15> ARENA_POOL_ROSTER = {{
    /* 0*/ { CLASS_PRIEST,       PRIEST_TAB_DISCIPLINE,   true  },  // Disc
    /* 1*/ { CLASS_PALADIN,      PALADIN_TAB_HOLY,        true  },  // Holy
    /* 2*/ { CLASS_DRUID,        DRUID_TAB_RESTORATION,   true  },  // Resto
    /* 3*/ { CLASS_SHAMAN,       SHAMAN_TAB_RESTORATION,  true  },  // Resto
    /* 4*/ { CLASS_WARRIOR,      WARRIOR_TAB_ARMS,        false },  // Arms
    /* 5*/ { CLASS_ROGUE,        ROGUE_TAB_COMBAT,        false },  // Combat
    /* 6*/ { CLASS_MAGE,         MAGE_TAB_FROST,          false },  // Frost
    /* 7*/ { CLASS_WARLOCK,      WARLOCK_TAB_AFFLICTION,  false },  // Affliction
    /* 8*/ { CLASS_HUNTER,       HUNTER_TAB_MARKSMANSHIP, false },  // MM
    /* 9*/ { CLASS_DEATH_KNIGHT, DEATH_KNIGHT_TAB_UNHOLY, false },  // Unholy
    /*10*/ { CLASS_WARRIOR,      WARRIOR_TAB_ARMS,        false },
    /*11*/ { CLASS_MAGE,         MAGE_TAB_FROST,          false },
    /*12*/ { CLASS_ROGUE,        ROGUE_TAB_COMBAT,        false },
    /*13*/ { CLASS_DEATH_KNIGHT, DEATH_KNIGHT_TAB_UNHOLY, false },
    /*14*/ { CLASS_PRIEST,       PRIEST_TAB_SHADOW,       false },  // Shadow
}};

// Team membership matrix (indices into ARENA_POOL_ROSTER). Disjoint within a bracket.
inline constexpr std::array<std::array<uint8, 2>, 7> ARENA_POOL_2V2 = {{
    {{0, 4}},   // Disc + Arms
    {{1, 9}},   // Holy + Unholy DK
    {{2, 5}},   // Resto druid + Rogue
    {{3, 10}},  // Resto shaman + Warrior
    {{6, 12}},  // Frost mage + Rogue (double DPS)
    {{7, 14}},  // Affli + Shadow (double DPS caster)
    {{8, 13}},  // MM + DK (double DPS)
}};                                            // slot 11 benched in 2v2

inline constexpr std::array<std::array<uint8, 3>, 5> ARENA_POOL_3V3 = {{
    {{0, 4, 6}},    // Disc + Arms + Frost
    {{1, 9, 11}},   // Holy + DK + Mage
    {{2, 5, 8}},    // Resto + Rogue + MM
    {{3, 10, 14}},  // Resto sham + Warrior + Shadow
    {{7, 12, 13}},  // Affli + Rogue + DK (triple DPS)
}};

inline constexpr std::array<std::array<uint8, 5>, 3> ARENA_POOL_5V5 = {{
    {{0, 1, 4, 6, 9}},     // double-heal + Arms/Frost/DK
    {{2, 5, 8, 11, 14}},   // Resto + Rogue/MM/Mage/Shadow
    {{3, 7, 10, 12, 13}},  // Resto sham + Affli/Warrior/Rogue/DK cleave
}};

// Team names: "T<tier> <name>" keeps them identifiable and unique per bracket.
inline constexpr std::array<char const*, 7> ARENA_POOL_2V2_NAMES =
    { "Blade Pact", "Grave Oath", "Wild Court", "Storm Call", "Cold Snap", "Dark Bargain", "Long Shot" };
inline constexpr std::array<char const*, 5> ARENA_POOL_3V3_NAMES =
    { "Iron Accord", "Bone Legion", "Thorn Circle", "Tide Watch", "Night Shift" };
inline constexpr std::array<char const*, 3> ARENA_POOL_5V5_NAMES =
    { "War Council", "Free Company", "Last Call" };
#endif
