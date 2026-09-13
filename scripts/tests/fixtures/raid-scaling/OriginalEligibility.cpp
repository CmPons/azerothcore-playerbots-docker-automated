// Exact pre-change methods, manager SHA256 2f88d83cc578e1372e03a282c72fe6614762d191b5713141c361abea5255854d.
// Regression fixture only: demonstrates the old rank gate without depending on private git history.

bool RaidScalingMgr::IsScalableCreature(Creature const* creature) const
{
    if (!creature || creature->IsPet() || creature->IsTrigger() || creature->IsCritter() || creature->IsCivilian())
        return false;

    CreatureTemplate const* proto = creature->GetCreatureTemplate();
    if (!proto)
        return false;

    // Scale before mutation: waiting for aura 802 would miss the spawn-time health scaling.
    return IsTwinsEncounterBug(creature) || creature->IsDungeonBoss() || creature->isWorldBoss() ||
        proto->rank == CREATURE_ELITE_ELITE ||
        proto->rank == CREATURE_ELITE_RAREELITE || proto->rank == CREATURE_ELITE_WORLDBOSS;
}

float RaidScalingMgr::GetDamageScale(Unit* attacker, Unit* victim) const
{
    if (!_enabled || !attacker || !victim)
        return 1.0f;

    Creature* creature = attacker->ToCreature();
    if (!creature || !IsScalableCreature(creature))
        return 1.0f;

    auto settings = GetSettings(creature->GetMap());
    if (!settings)
        return 1.0f;

    // Only scale hostile raid damage against the player party/raid. Leave creature-vs-creature
    // scripted event combat alone (e.g. escorts, friendly NPCs, add interactions).
    if (!victim->IsPlayer() && !victim->IsControlledByPlayer())
        return 1.0f;

    return DamageScaleFor(creature, *settings);
}

void RaidScalingMgr::OnCreatureAddWorld(Creature* creature)
{
    if (!_enabled || !creature || !creature->GetMap() || !HasScaling(creature->GetMap()))
        return;

    ApplyToCreature(creature);
}
