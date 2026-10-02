// Extracted production manager/loader methods; native respawn stat generation is doubled here.
class AllCreatureScript
{
public:
    explicit AllCreatureScript(char const*) {}
    virtual ~AllCreatureScript() = default;
    virtual void OnCreatureAddWorld(Creature*) {}
    virtual void OnCreatureRespawn(Creature*) {}
};
/* LOADER */

static void NativeRebuild(Creature& creature, uint32 health)
{
    creature.createHealth = creature.maxHealth = creature.health = health;
    creature.flat[BASE_VALUE] = float(health);
    creature.flat[TOTAL_VALUE] = 0;
    creature.pct[BASE_PCT] = creature.pct[TOTAL_PCT] = 1;
    creature.deathState = DeathState::Alive;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    RaidScalingMgr& mgr = sRaidScalingMgr;
    RaidScalingCreatureScript hook;
    Map map;
    map.id = 548;
    mgr._originalSizes[548] = 25;
    Enable(mgr, map, mgr.MakeSettings(25, 10, true));
    Creature guard(&map);
    guard.entry = guard.data.id = 21964;
    guard.proto.rank = CREATURE_ELITE_ELITE;
    NativeRebuild(guard, 603120);
    map.GetObjectsStore().Insert<Creature>(guard.GetGUID(), &guard);
    hook.OnCreatureAddWorld(&guard);
    assert(guard.GetMaxHealth() == 241248);

    if (scenario == "cycles")
    {
        Creature lord(&map);
        lord.entry = lord.data.id = 21214;
        lord.boss = true;
        NativeRebuild(lord, 1274700);
        hook.OnCreatureAddWorld(&lord);
        lord.SetHealth(254940);
        for (int cycle = 0; cycle < 20; ++cycle)
        {
            guard.SetHealth(60000); // Must not carry an injured previous life into respawn.
            guard.deathState = DeathState::Dead;
            NativeRebuild(guard, 603120); // Creature::SelectLevel + JustRespawned.
            hook.OnCreatureRespawn(&guard);
            assert(guard.GetMaxHealth() == 241248 && guard.GetHealth() == 241248);
            mgr.ApplyToCreature(&guard); // An additional AddWorld/manual refresh cannot compound scaling.
            assert(guard.GetHealth() == 241248 && guard.GetCreateHealth() == 241248);
            assert(lord.GetMaxHealth() == 509880 && lord.GetHealth() == 254940);
        }
        assert(mgr.GetSettings(&map)->fromDefault);
    }
    else if (scenario == "baseline")
    {
        // Same GUID, new native level/stats. A stale original-stat cache would restore 603120.
        NativeRebuild(guard, 900000);
        guard.SetHealth(450000); // AI Reset can choose a starting fraction.
        hook.OnCreatureRespawn(&guard);
        assert(guard.GetMaxHealth() == 360000 && guard.GetHealth() == 180000);
        mgr.RestoreCreature(&guard);
        assert(guard.GetMaxHealth() == 900000 && guard.GetHealth() == 450000);
    }
    else if (scenario == "settings")
    {
        auto settings = mgr.MakeSettings(25, 10, false);
        settings.trashHealth = .2f;
        settings.trashDamage = .3f;
        settings.bossHealth = .6f;
        Enable(mgr, map, settings);
        NativeRebuild(guard, 603120);
        hook.OnCreatureRespawn(&guard);
        assert(guard.GetMaxHealth() == 120624 && guard.GetHealth() == 120624);
        assert(mgr.GetSettings(&map)->trashDamage == .3f && !mgr.GetSettings(&map)->fromDefault);
        mgr.DisableForMap(&map);
        NativeRebuild(guard, 900000);
        hook.OnCreatureRespawn(&guard);
        assert(!mgr.HasScaling(&map) && guard.GetHealth() == 900000);
        mgr.EnableForMap(&map, 10);
        assert(guard.GetMaxHealth() == 360000);
        mgr._enabled = false;
        NativeRebuild(guard, 1000000);
        hook.OnCreatureRespawn(&guard);
        assert(guard.GetMaxHealth() == 1000000);
        mgr._enabled = true;
        mgr.ApplyToCreature(&guard);
        assert(guard.GetMaxHealth() == 400000); // Disabled respawn also invalidates old baseline.
    }
    else if (scenario == "isolation")
    {
        for (int kind = 0; kind < 6; ++kind)
        {
            Map other;
            other.id = 548;
            other.instance = 100 + kind;
            if (kind == 1) other.raid = false;
            if (kind == 2) other.instance = 0;
            Creature excluded(&other);
            excluded.proto.rank = CREATURE_ELITE_ELITE;
            excluded.entry = excluded.data.id = 21965;
            if (kind != 0) Enable(mgr, other, mgr.MakeSettings(25, 10, true));
            if (kind == 3) excluded.pet = true;
            if (kind == 4) excluded.owner = ObjectGuid{42};
            if (kind == 5) excluded.controlled = true;
            NativeRebuild(excluded, 603120);
            hook.OnCreatureRespawn(&excluded);
            assert(excluded.GetHealth() == 603120);
            if (kind == 0) assert(!mgr.HasScaling(&other));
        }
        hook.OnCreatureRespawn(nullptr);
        NativeRebuild(guard, 603120);
        guard.deathState = DeathState::Dead; // AI Reset despawned it: do not resurrect.
        guard.health = 0;
        hook.OnCreatureRespawn(&guard);
        assert(guard.isDead() && guard.GetHealth() == 0);
    }
    else if (scenario == "dynamic")
    {
        for (uint32 life = 1; life <= 10; ++life)
        {
            Creature recreated(&map);
            recreated.proto.rank = CREATURE_ELITE_ELITE;
            recreated.entry = recreated.data.id = 21966;
            recreated.runtimeId = 500 + life;
            NativeRebuild(recreated, 603120);
            hook.OnCreatureAddWorld(&recreated); // Dynamic path still uses world-entry only.
            assert(recreated.GetHealth() == 241248);
        }
    }
    else if (scenario == "mutation-respawn")
    {
        Map aq;
        aq.instance = 99;
        Enable(mgr, aq);
        Creature bug(&aq);
        Link(bug);
        hook.OnCreatureAddWorld(&bug);
        for (int cycle = 0; cycle < 10; ++cycle)
        {
            NativeRebuild(bug, 3052);
            Mutate(bug); // AI reset applied the native +300% aura before the callback.
            hook.OnCreatureRespawn(&bug);
            assert(bug.GetCreateHealth() == 763 && bug.GetMaxHealth() == 3052);
            Mutate(bug, false);
            assert(bug.GetMaxHealth() == 763);
        }
    }
    else if (scenario == "percent")
    {
        for (float scale : {.4f, 1.0f, 2.0f})
        {
            auto settings = mgr.MakeSettings(25, 10, false);
            settings.trashHealth = scale;
            Enable(mgr, map, settings);
            NativeRebuild(guard, 600000);
            hook.OnCreatureRespawn(&guard);
            guard.SetHealth(guard.GetMaxHealth() / 4);
            uint32 hp = guard.GetHealth();
            for (int repeat = 0; repeat < 10; ++repeat) mgr.ApplyToCreature(&guard);
            assert(guard.GetHealth() == hp); // Especially scale == 1: do not heal to first snapshot.
        }
    }
    else
        assert(false);
    std::cout << "Passed " << scenario << '\n';
}
