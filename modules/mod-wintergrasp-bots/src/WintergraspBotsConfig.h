#ifndef MOD_WINTERGRASP_BOTS_CONFIG_H
#define MOD_WINTERGRASP_BOTS_CONFIG_H
#include "Define.h"

class WintergraspBotsConfig
{
public:
    static WintergraspBotsConfig* instance();

    void Load();

    bool   enable     = false;
    uint32 perFaction = 15;
    uint32 minLevel   = 75;
    uint32 tickMs     = 5000;
    bool   debug      = false;

    // Phase 2b (objectives)
    bool   workshopCapture = true;    // master toggle for the objective layer (false = P2a behavior)
    float  arriveRadius    = 8.0f;    // yards: within this of the objective, stop moving and hold
    uint32 stuckSeconds    = 12;      // traveling + not-in-combat + no progress this long => stuck
    float  stuckEpsilon    = 3.0f;    // yards of 2D movement below which a sample counts as "no progress"
    bool   reassertMove    = true;    // re-issue MovePoint each tick for idle bots (false = debug)
    float  engageVehicleRadius = 80.0f; // idle bots attack a hostile siege vehicle within this range (0 = off)
    uint32 defenseShare = 60;         // % of defenders reserved for the priority rally at the live threat (leftovers may join too)
    bool   fieldGarrison = true;      // spawn faction guard squads at workshops + bridge roads (core omits them)

    uint32 turretCrewMax   = 4;       // max defenders manning keep tower cannons at once (threat-gated)
    uint32 rallyPerVehicle = 3;       // defenders sent to the rally per enemy vehicle in the threat cluster
    float  threatRadius    = 350.0f;  // yards from the keep gate within which enemy vehicles count as a keep threat
    float  patrolRadius    = 12.0f;   // yards: patrol-loop radius around a held objective (0 = statues)
    uint32 sortieQuorum    = 4;       // defenders mustered inside before the rally wave exits the keep (0 = no muster)
    uint32 captureSquad    = 5;       // attackers per enemy-workshop capture squad after the rank phase (+2 at zero owned workshops)

    // Idle-vehicle recovery (Fix 2) + no-siege effort split (Fix 3)
    float  recrewRadius       = 40.0f; // yards: an attacker foot bot re-crews an idle friendly siege vehicle this close
    uint32 vehicleReapSeconds = 30;    // seconds an unmanned unreachable vehicle survives before being killed to free the cap (0 = off)
    uint32 keepScreen         = 4;     // attackers left screening the keep gate when no siege vehicle is in play

    // Phase 3 (siege vehicles) — read directly via sConfigMgr by the fork siege strategy; loaded
    // here too for the startup log. Keys must exist in the conf so the strategy's GetOption finds them.
    bool   siegeEnable  = true;       // master toggle for the siege layer
    uint32 humanReserve = 2;          // vehicle slots per faction reserved for human players
};

#define sWintergraspBotsConfig WintergraspBotsConfig::instance()
#endif
