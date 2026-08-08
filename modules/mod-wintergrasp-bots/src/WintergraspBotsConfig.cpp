#include "WintergraspBotsConfig.h"
#include "Config.h"
#include "Log.h"

WintergraspBotsConfig* WintergraspBotsConfig::instance()
{
    static WintergraspBotsConfig instance;
    return &instance;
}

void WintergraspBotsConfig::Load()
{
    enable     = sConfigMgr->GetOption<bool>("WintergraspBots.Enable", false);
    perFaction = sConfigMgr->GetOption<uint32>("WintergraspBots.PerFaction", 15);
    minLevel   = sConfigMgr->GetOption<uint32>("WintergraspBots.MinLevel", 75);
    tickMs     = sConfigMgr->GetOption<uint32>("WintergraspBots.TickMs", 5000);
    debug      = sConfigMgr->GetOption<bool>("WintergraspBots.Debug", false);

    workshopCapture = sConfigMgr->GetOption<bool>("WintergraspBots.WorkshopCapture", true);
    arriveRadius    = sConfigMgr->GetOption<float>("WintergraspBots.ArriveRadius", 8.0f);
    stuckSeconds    = sConfigMgr->GetOption<uint32>("WintergraspBots.StuckSeconds", 12);
    stuckEpsilon    = sConfigMgr->GetOption<float>("WintergraspBots.StuckEpsilon", 3.0f);
    reassertMove    = sConfigMgr->GetOption<bool>("WintergraspBots.ReassertMove", true);
    engageVehicleRadius = sConfigMgr->GetOption<float>("WintergraspBots.EngageVehicleRadius", 80.0f);
    defenseShare    = sConfigMgr->GetOption<uint32>("WintergraspBots.DefenseShare", 60);
    fieldGarrison   = sConfigMgr->GetOption<bool>("WintergraspBots.FieldGarrison", true);
    turretCrewMax   = sConfigMgr->GetOption<uint32>("WintergraspBots.TurretCrewMax", 4);
    rallyPerVehicle = sConfigMgr->GetOption<uint32>("WintergraspBots.RallyPerVehicle", 3);
    threatRadius    = sConfigMgr->GetOption<float>("WintergraspBots.ThreatRadius", 350.0f);
    patrolRadius    = sConfigMgr->GetOption<float>("WintergraspBots.PatrolRadius", 12.0f);
    sortieQuorum    = sConfigMgr->GetOption<uint32>("WintergraspBots.SortieQuorum", 4);
    captureSquad    = sConfigMgr->GetOption<uint32>("WintergraspBots.CaptureSquad", 5);
    recrewRadius       = sConfigMgr->GetOption<float>("WintergraspBots.RecrewRadius", 40.0f);
    vehicleReapSeconds = sConfigMgr->GetOption<uint32>("WintergraspBots.VehicleReapSeconds", 30);
    keepScreen         = sConfigMgr->GetOption<uint32>("WintergraspBots.KeepScreen", 4);
    siegeEnable     = sConfigMgr->GetOption<bool>("WintergraspBots.SiegeEnable", true);
    humanReserve    = sConfigMgr->GetOption<uint32>("WintergraspBots.HumanReserve", 2);

    LOG_INFO("server.loading",
        "[WintergraspBots] Enable={} PerFaction={} MinLevel={} TickMs={} Debug={} "
        "WorkshopCapture={} ArriveRadius={} StuckSeconds={} StuckEpsilon={} ReassertMove={} "
        "EngageVehicleRadius={} DefenseShare={} FieldGarrison={} TurretCrewMax={} RallyPerVehicle={} "
        "ThreatRadius={} PatrolRadius={} SortieQuorum={} CaptureSquad={} SiegeEnable={} HumanReserve={} "
        "RecrewRadius={} VehicleReapSeconds={} KeepScreen={}",
        enable ? 1 : 0, perFaction, minLevel, tickMs, debug ? 1 : 0,
        workshopCapture ? 1 : 0, arriveRadius, stuckSeconds, stuckEpsilon, reassertMove ? 1 : 0,
        engageVehicleRadius, defenseShare, fieldGarrison ? 1 : 0, turretCrewMax, rallyPerVehicle,
        threatRadius, patrolRadius, sortieQuorum, captureSquad, siegeEnable ? 1 : 0, humanReserve,
        recrewRadius, vehicleReapSeconds, keepScreen);
}
