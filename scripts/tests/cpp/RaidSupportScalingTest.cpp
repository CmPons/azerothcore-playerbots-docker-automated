// Appended to the existing native-faction/provenance harness. Spell/log storage are doubles.
#include <array>
#include <limits>
using int32 = std::int32_t;
enum SupportEffects
{
    SPELL_EFFECT_HEAL = 10, SPELL_EFFECT_HEAL_MECHANICAL = 75,
    SPELL_EFFECT_HEAL_PCT = 136, SPELL_EFFECT_HEAL_MAX_HEALTH = 67, SPELL_EFFECT_HEALTH_LEECH = 9,
    SPELL_AURA_PERIODIC_HEAL = 8, SPELL_AURA_OBS_MOD_HEALTH = 20,
    SPELL_AURA_PERIODIC_LEECH = 53, SPELL_AURA_PERIODIC_HEALTH_FUNNEL = 62,
    SPELL_AURA_SCHOOL_ABSORB = 69, SPELL_AURA_MANA_SHIELD = 97,
    SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN = 87
};
struct SpellEffectInfo
{
    int effect = 0, aura = 0;
    bool area = false;
    bool IsAreaAuraEffect() const { return area; }
};
struct SpellInfo
{
    std::array<SpellEffectInfo, 3> Effects{};
    bool HasEffect(int type) const
    { return std::any_of(Effects.begin(), Effects.end(), [type](auto& e) { return e.effect == type; }); }
    bool HasAura(int type) const
    { return std::any_of(Effects.begin(), Effects.end(), [type](auto& e) { return e.aura == type; }); }
};
/* SUPPORT_HEADER */
/* SUPPORT_MANAGER */
constexpr int UNIT_AURA_TYPE = 0;
struct SupportAura
{
    Unit* owner;
    int type = UNIT_AURA_TYPE;
    int GetType() const { return type; }
    Unit* GetUnitOwner() const { return owner; }
};
struct SupportAuraEffect
{
    SupportAura* aura;
    SpellInfo* info;
    SupportAura* GetBase() const { return aura; }
    int GetAuraType() const { return info->Effects[0].aura; }
    SpellInfo const* GetSpellInfo() const { return info; }
    uint32 GetEffIndex() const { return 0; }
};
struct SupportScript
{
    /* SUPPORT_METHODS */
} supportScript;
struct SupportDispatcher
{
    void ModifyHealReceived(Unit* target, Unit* caster, uint32& amount, SpellInfo const* info)
    { supportScript.ModifyHealReceived(target, caster, amount, info); }
} supportDispatcher;
#define sScriptMgr (&supportDispatcher)
struct HealInfo
{
    Unit* caster;
    Unit* target;
    uint32 heal, effective = 0;
    SpellInfo* info;
    HealInfo(Unit* c, Unit* t, uint32 h, SpellInfo* i) : caster(c), target(t), heal(h), info(i) {}
    Unit* GetHealer() const { return caster; }
    Unit* GetTarget() const { return target; }
    SpellInfo const* GetSpellInfo() const { return info; }
    uint32 GetHeal() const { return heal; }
    void SetHeal(uint32 h) { heal = h; }
    void SetEffectiveHeal(uint32 h) { effective = h; }
};
struct SupportPipeline : Creature
{
    using Creature::Creature;
    uint32 absorb = 0, loggedHeal = 0, loggedEffective = 0;
    void CalcHealAbsorb(HealInfo& info) { info.heal -= std::min(info.heal, absorb); }
    static int32 DealHeal(Unit*, Unit* victim, uint32 amount)
    {
        uint32 gain = std::min(amount, victim->maxHealth - victim->health);
        victim->health += gain;
        return int32(gain);
    }
    void SendHealSpellLog(HealInfo& info, bool)
    { loggedHeal = info.heal; loggedEffective = info.effective; }
    int32 HealBySpell(HealInfo&, bool);
};
/* HEAL_PIPELINE */
void Enemy(Creature& creature, bool boss = false)
{
    creature.faction.hostileMask = FACTION_MASK_PLAYER;
    creature.proto.rank = boss ? CREATURE_ELITE_WORLDBOSS : CREATURE_ELITE_ELITE;
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    Map map, other;
    other.instance = 2;
    auto& mgr = sRaidScalingMgr;
    Enable(mgr, map, mgr.MakeSettings(25, 10, true));
    SupportPipeline healer(&map);
    Creature target(&map);
    Enemy(healer, true); Enemy(target, true);
    SpellInfo flat;
    flat.Effects[0].effect = SPELL_EFFECT_HEAL;
    if (scenario == "heals")
    {
        for (uint32 amount : {46250u, 53750u, 92500u, 107500u, 69375u, 80625u})
        {
            target.maxHealth = 100000; target.health = 0;
            HealInfo info(&healer, &target, amount, &flat);
            healer.HealBySpell(info, false);
            assert(info.heal == uint32(std::round(amount * 0.4)));
            assert(target.health == info.heal && healer.loggedEffective == info.heal);
        }
        // Asymmetric boss/trash factors expose reversed direct-heal hook arguments.
        auto settings = *mgr.GetSettings(&map);
        settings.trashHealth = .25f; settings.bossDamage = .2f;
        Enable(mgr, map, settings); target.proto.rank = CREATURE_ELITE_ELITE;
        target.maxHealth = 100000; target.health = 99900;
        healer.absorb = 1000;
        HealInfo info(&healer, &target, 80000, &flat);
        assert(healer.HealBySpell(info, true) == 100);
        assert(info.heal == 19000 && target.health == 100000 && healer.loggedEffective == 100);
        SpellInfo hot; hot.Effects[0].aura = SPELL_AURA_PERIODIC_HEAL;
        uint32 tick = 2000;
        supportScript.ModifyHealReceived(&target, &healer, tick, &hot);
        assert(tick == 500); // no double scaling at aura amount construction
        assert(RaidScalingSupport::ScaleAmount(0, .4f) == 0);
        assert(RaidScalingSupport::ScaleAmount(1, .05f) == 1);
        assert(RaidScalingSupport::ScaleAmount(2000000000u, 5.f) == 2147483647u);
    }
    else if (scenario == "shields")
    {
        SpellInfo shield; shield.Effects[0].aura = SPELL_AURA_SCHOOL_ABSORB;
        SupportAura aura{&target}; SupportAuraEffect effect{&aura, &shield};
        for (int type : {SPELL_AURA_SCHOOL_ABSORB, SPELL_AURA_MANA_SHIELD})
        {
            shield.Effects[0].aura = type;
            for (int stacks : {1, 3})
                for (int refresh = 0; refresh < 3; ++refresh)
                {
                    int32 amount = 25000 * stacks; // fresh native amount, not remaining capacity
                    supportScript.OnAuraEffectCalculateAmount(&effect, &healer, amount);
                    assert(amount == 10000 * stacks);
                    amount -= 3000; // consuming a shield does not recalculate its original amount
                    assert(amount == 10000 * stacks - 3000);
                }
        }
        for (int32 original : {-1, 0})
        {
            int32 amount = original;
            supportScript.OnAuraEffectCalculateAmount(&effect, &healer, amount);
            assert(amount == original);
        }
        for (int type : {SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, SPELL_AURA_PERIODIC_HEAL})
        {
            shield.Effects[0].aura = type; int32 amount = 75;
            supportScript.OnAuraEffectCalculateAmount(&effect, &healer, amount);
            assert(amount == 75);
        }
        shield.Effects[0].aura = SPELL_AURA_SCHOOL_ABSORB;
        for (int mode : {1, 2})
        {
            aura.type = mode == 1 ? 1 : UNIT_AURA_TYPE;
            shield.Effects[0].area = mode == 2;
            int32 amount = 25000;
            supportScript.OnAuraEffectCalculateAmount(&effect, &healer, amount);
            assert(amount == 25000);
        }
    }
    else if (scenario == "percent")
    {
        for (int type : {SPELL_EFFECT_HEAL_PCT, SPELL_EFFECT_HEAL_MAX_HEALTH, SPELL_EFFECT_HEALTH_LEECH})
        {
            auto info = flat; info.Effects[1].effect = type;
            assert(!RaidScalingSupport::IsFlatHealing(&info)); // mixed spells fail closed too
        }
        for (int type : {SPELL_AURA_OBS_MOD_HEALTH, SPELL_AURA_PERIODIC_LEECH,
            SPELL_AURA_PERIODIC_HEALTH_FUNNEL})
        {
            auto info = flat; info.Effects[1].aura = type;
            assert(!RaidScalingSupport::IsFlatHealing(&info));
        }
        SpellInfo unknown;
        assert(!RaidScalingSupport::IsFlatHealing(&unknown));
        assert(!RaidScalingSupport::IsFlatHealing(nullptr));
        unknown.Effects[0].effect = SPELL_EFFECT_HEAL_MECHANICAL;
        assert(RaidScalingSupport::IsFlatHealing(&unknown));
    }
    else if (scenario == "scope")
    {
        assert(std::abs(mgr.GetSupportScale(&healer, &target) - .4f) < .0001f);
        assert(std::abs(mgr.GetSupportScale(&healer, &healer) - .4f) < .0001f);
        assert(mgr.GetSupportScale(nullptr, &target) == 1);
        Unit player; player.player = true; player.unitMap = &map;
        assert(mgr.GetSupportScale(&player, &target) == 1);
        assert(mgr.GetSupportScale(&healer, &player) == 1);
        uint32 playerHeal = 80000;
        supportScript.ModifyHealReceived(&target, &player, playerHeal, &flat);
        assert(playerHeal == 80000);
        SpellInfo shield; shield.Effects[0].aura = SPELL_AURA_SCHOOL_ABSORB;
        SupportAura aura{&player}; SupportAuraEffect effect{&aura, &shield};
        int32 playerShield = 25000;
        supportScript.OnAuraEffectCalculateAmount(&effect, &healer, playerShield);
        assert(playerShield == 25000);
        for (Creature* actor : {static_cast<Creature*>(&healer), &target})
        {
            actor->controlled = true; assert(mgr.GetSupportScale(&healer, &target) == 1); actor->controlled = false;
            actor->pet = true; assert(mgr.GetSupportScale(&healer, &target) == 1); actor->pet = false;
            actor->created = true; assert(mgr.GetSupportScale(&healer, &target) == 1); actor->created = false;
            actor->owner = ObjectGuid{42}; assert(mgr.GetSupportScale(&healer, &target) == 1); actor->owner = {};
            actor->faction.hostileMask = 0; assert(mgr.GetSupportScale(&healer, &target) == 1); Enemy(*actor, true);
            actor->civilian = true; assert(mgr.GetSupportScale(&healer, &target) == 1); actor->civilian = false;
        }
        target.map = &other; assert(mgr.GetSupportScale(&healer, &target) == 1); target.map = &map;
        mgr._enabled = false; assert(mgr.GetSupportScale(&healer, &target) == 1); mgr._enabled = true;
        map.raid = false; assert(mgr.GetSupportScale(&healer, &target) == 1); map.raid = true;
        map.instance = 0; assert(mgr.GetSupportScale(&healer, &target) == 1); map.instance = 1;
        mgr.DisableForMap(&map); assert(mgr.GetSupportScale(&healer, &target) == 1);
        Enable(mgr, map, mgr.MakeSettings(10, 10, true));
        assert(mgr.GetSupportScale(&healer, &target) == 1);
        Enable(mgr, map, mgr.MakeSettings(40, 10, true));
        assert(mgr.GetSupportScale(&healer, &target) == .25f);
    }
    else
        assert(false);
    std::cout << "Passed support " << scenario << '\n';
}
