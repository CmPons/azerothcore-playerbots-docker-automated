#include "RaidScalingMgr.h"

#include "AllMapScript.h"
#include "Chat.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Unit.h"
#include "WorldSession.h"

void AddRaidScalingCommandScripts();

class RaidScalingWorldScript : public WorldScript
{
public:
    RaidScalingWorldScript() : WorldScript("RaidScalingWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sRaidScalingMgr.LoadConfig();
    }
};

class RaidScalingMapScript : public AllMapScript
{
public:
    RaidScalingMapScript() : AllMapScript("RaidScalingMapScript",
        {ALLMAPHOOK_ON_CREATE_MAP, ALLMAPHOOK_ON_DESTROY_MAP, ALLMAPHOOK_ON_PLAYER_ENTER_ALL}) { }

    void OnCreateMap(Map* map) override
    {
        sRaidScalingMgr.OnMapCreate(map);
    }

    void OnDestroyMap(Map* map) override
    {
        sRaidScalingMgr.OnMapDestroy(map);
    }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        if (!sRaidScalingMgr.Enabled() || !map || !map->IsRaid() || !map->GetInstanceId() || !player)
            return;
        WorldSession* session = player->GetSession();
        if (!session || session->IsBot())
            return;

        // Report only. Never change difficulty when someone joins or reconnects.
        ChatHandler handler(session);
        if (auto settings = sRaidScalingMgr.GetSettings(map))
            handler.PSendSysMessage("RaidScale: {} #{} scaled for {} players ({}).",
                map->GetMapName(), map->GetInstanceId(), settings->targetPlayers,
                settings->fromDefault ? "server default" : "manual override");
        else
            sRaidScalingMgr.SendStatus(&handler, map);
    }
};

class RaidScalingCreatureScript : public AllCreatureScript
{
public:
    RaidScalingCreatureScript() : AllCreatureScript("RaidScalingCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        sRaidScalingMgr.OnCreatureAddWorld(creature);
    }
};

class RaidScalingUnitScript : public UnitScript
{
public:
    RaidScalingUnitScript() : UnitScript("RaidScalingUnitScript") { }

    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType damageType) override
    {
        (void)damageType;
        if (!damage)
            return damage;

        float scale = sRaidScalingMgr.GetDamageScale(attacker, victim);
        if (scale >= 0.999f && scale <= 1.001f)
            return damage;

        return std::max<uint32>(1, uint32(float(damage) * scale));
    }
};

void Addmod_raid_scalingScripts()
{
    LOG_INFO("server.loading", "[RaidScaling] Registering scripts.");
    new RaidScalingWorldScript();
    new RaidScalingMapScript();
    new RaidScalingCreatureScript();
    new RaidScalingUnitScript();
    AddRaidScalingCommandScripts();
}
