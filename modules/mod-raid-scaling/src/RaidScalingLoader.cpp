#include "RaidScalingMgr.h"

#include "Creature.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "Unit.h"

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
    new RaidScalingCreatureScript();
    new RaidScalingUnitScript();
    AddRaidScalingCommandScripts();
}
