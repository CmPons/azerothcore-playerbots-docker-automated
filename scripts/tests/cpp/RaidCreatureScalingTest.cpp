// Appended to the Twins health harness: production manager/hook/helper bodies, native faction
// calculations and DataMap, with object lookup/storage doubles. Not a worldserver simulation.
void Hostile(Creature& creature)
{
    creature.faction.hostileMask = FACTION_MASK_PLAYER;
}
void Summoner(TempSummon& summon, Creature& parent)
{
    summon.summoner = parent.GetGUID();
    ObjectAccessor::objects[parent.GetGUID()] = &parent;
}
void ExpectUnscaled(RaidScalingMgr& mgr, Creature& creature, Unit& victim)
{
    assert(mgr.GetDamageScale(&creature, &victim) == 1.0f);
    assert(!mgr.IsScalableCreature(&creature));
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    RaidScalingMgr& mgr = RaidScalingMgr::Instance();
    Map map;
    Enable(mgr, map);
    Unit player;
    player.player = true;
    player.faction.faction = 2;
    player.faction.ourMask = FACTION_MASK_PLAYER;
    RaidScalingUnitScript hook;
    TempSummon add(&map);
    add.entry = 15712; // observed Dirt Mound rank 0; no production entry exception
    Hostile(add);
    auto damage = [&] { return hook.DealDamage(&add, &player, 750, 0); };
    if (scenario == "mound")
    {
        Creature ouro(&map);
        ouro.entry = 15517;
        ouro.proto.rank = 3;
        Summoner(add, ouro);
        add.flags = UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE;
        mgr.OnCreatureAddWorld(&add);
        assert(!mgr.IsScalableCreature(&add));
        assert(add.GetMaxHealth() == 3052);
        assert(damage() == 326); // 750 * pow(10/40, .6), unchanged truncation
        ObjectAccessor::objects.clear(); // native Ouro despawns 1s after creating mounds
        assert(damage() == 326);
        add.trigger = true; // damaging triggers must not inherit health's exclusion
        assert(damage() == 326);
        add.faction.hostileMask = 0; // neutral is not friendly
        assert(damage() == 326);
        assert(hook.DealDamage(&add, &player, 0, 0) == 0);
    }
    else if (scenario == "health")
    {
        // Dirt Mound, Ouro Scarab (15718), and an arbitrary normal add: no production allowlist.
        for (uint32 entry : {15712u, 15718u, 900001u})
        {
            Creature normal(&map);
            normal.entry = entry;
            normal.spawnId = entry;
            Hostile(normal);
            mgr.OnCreatureAddWorld(&normal);
            assert(normal.GetMaxHealth() == 763 && normal.GetCreateHealth() == 763);
            normal.SetHealth(381);
            mgr.ApplyToCreature(&normal);
            assert(normal.GetMaxHealth() == 763 && normal.GetHealth() == 381);
            assert(hook.DealDamage(&normal, &player, 750, 0) == 326);
            mgr.RestoreCreature(&normal);
            assert(normal.GetMaxHealth() == 3052 && normal.GetHealth() == 1524);
        }
        auto settings = Default();
        settings.bossHealth = .5f;
        settings.bossDamage = .2f;
        mgr._state.Set(mgr.MakeKey(&map), settings);
        for (uint32 rank : {1u, 2u, 3u})
        {
            add.proto.rank = rank;
            add.flags = UNIT_FLAG_NON_ATTACKABLE; // preserve scripted elite/boss intro behavior
            add.faction.hostileMask = 0;
            assert(mgr.IsScalableCreature(&add));
            assert(std::fabs(mgr.GetDamageScale(&add, &player) -
                (rank == 3 ? .2f : settings.trashDamage)) < .00001f);
        }
        add.proto.rank = 0;
        add.boss = true;
        assert(mgr.IsScalableCreature(&add));
        assert(mgr.GetDamageScale(&add, &player) == .2f);
        add.boss = false;
        add.worldBoss = true;
        assert(mgr.IsScalableCreature(&add));
    }
    else if (scenario == "exclusions")
    {
        for (bool Creature::*flag : {&Creature::pet, &Creature::critter, &Creature::civilian})
        {
            add.*flag = true;
            ExpectUnscaled(mgr, add, player);
            add.*flag = false;
        }
        add.faction.hostileMask = 0;
        add.faction.friendlyMask = FACTION_MASK_PLAYER;
        ExpectUnscaled(mgr, add, player);
        add.faction.friendlyMask = 0; // neutral normal health is intentionally conservative
        assert(!mgr.IsScalableCreature(&add));
        assert(damage() == 326);
        Hostile(add);
        sFactionStore.entries[add.faction.faction].reputationListID = 0;
        assert(!mgr.IsScalableCreature(&add)); // native IsHostileToPlayers excludes reputation factions
        sFactionStore.entries.clear();
        for (uint32 flag : {UNIT_FLAG_NON_ATTACKABLE, UNIT_FLAG_NOT_SELECTABLE,
            UNIT_FLAG_NOT_ATTACKABLE_1, UNIT_FLAG_NON_ATTACKABLE_2})
        {
            add.flags = flag;
            assert(!mgr.IsScalableCreature(&add));
            assert(damage() == 326);
        }
        add.flags = 0;
        add.immunePC = true;
        assert(!mgr.IsScalableCreature(&add));
        assert(damage() == 326);
        add.immunePC = false;
        add.trigger = true; // utility trigger receives no health, zero damage stays zero
        assert(!mgr.IsScalableCreature(&add));
        assert(hook.DealDamage(&add, &player, 0, 0) == 0);
        Creature npc(&map);
        assert(hook.DealDamage(&add, &npc, 750, 0) == 750);
        Unit pet;
        pet.controlled = true;
        pet.faction = player.faction;
        assert(hook.DealDamage(&add, &pet, 750, 0) == 326);
        assert(hook.DealDamage(&player, &pet, 750, 0) == 750);
    }
    else if (scenario == "origins")
    {
        ObjectGuid playerGuid{42}; // unresolved player: GUID alone is enough
        for (ObjectGuid Unit::*field : {&Unit::owner, &Unit::charmer, &Unit::creator})
        {
            add.*field = playerGuid;
            ExpectUnscaled(mgr, add, player);
            add.*field = {};
        }
        add.summoner = playerGuid;
        ExpectUnscaled(mgr, add, player);
        add.summoner = {};
        for (bool Unit::*flag : {&Unit::controlled, &Unit::created})
        {
            add.*flag = true;
            ExpectUnscaled(mgr, add, player);
            add.*flag = false;
        }
        Creature guardian(&map);
        guardian.entry = 100001;
        guardian.owner = playerGuid;
        add.owner = guardian.GetGUID();
        ObjectAccessor::objects[guardian.GetGUID()] = &guardian;
        ExpectUnscaled(mgr, add, player);
        add.owner = {};
        Summoner(add, guardian); // pet/guardian's temporary child, not a direct player summon
        mgr._enabled = false;
        mgr._state.Disable(mgr.MakeKey(&map));
        mgr.OnCreatureAddWorld(&add); // provenance captured while off, no HP change
        assert(add.GetMaxHealth() == 3052);
        ObjectAccessor::objects.clear();
        mgr._enabled = true;
        Enable(mgr, map);
        ExpectUnscaled(mgr, add, player);
        TempSummon child(&map);
        child.entry = 100002;
        Hostile(child);
        Summoner(child, add); // cached ancestry propagates to another generation
        mgr.OnCreatureAddWorld(&child);
        ObjectAccessor::objects.clear();
        ExpectUnscaled(mgr, child, player);
        // No GUID registry: a distinct object with the same test GUID cannot inherit cached data.
        TempSummon independent(&map);
        independent.entry = add.entry;
        independent.spawnId = add.spawnId;
        independent.runtimeId = add.runtimeId;
        Hostile(independent);
        assert(!RaidCreatureEligibility::HasExcludedOrigin(&independent));
        assert(mgr.GetDamageScale(&independent, &player) == Default().trashDamage);
        mgr.OnCreatureAddWorld(&independent);
        independent.controlled = true; // control acquired after the spawn snapshot
        ExpectUnscaled(mgr, independent, player);
        independent.controlled = false;
        assert(!RaidCreatureEligibility::HasExcludedOrigin(&independent));
        GameObject object;
        object.owner = playerGuid;
        independent.summoner = ObjectGuid::Create<HighGuid::Unit>(200001, 1);
        ObjectAccessor::objects[independent.summoner] = &object;
        ExpectUnscaled(mgr, independent, player);
        bool destroyed = false;
        struct OriginProbe : RaidCreatureEligibility::Origin
        {
            bool& destroyed;
            explicit OriginProbe(bool& value) : destroyed(value) { }
            ~OriginProbe() override { destroyed = true; }
        };
        {
            TempSummon scoped(&map);
            scoped.CustomData.Set(RaidCreatureEligibility::OriginKey, new OriginProbe(destroyed));
        }
        assert(destroyed); // native DataMap releases provenance with the object's lifetime
    }
    else if (scenario == "bounds")
    {
        Creature parent(&map);
        parent.entry = 100003;
        Summoner(add, parent);
        parent.owner = add.GetGUID();
        ObjectAccessor::objects[add.GetGUID()] = &add;
        ExpectUnscaled(mgr, add, player); // cycle
        std::vector<std::unique_ptr<TempSummon>> chain;
        for (uint32 i = 0; i < 18; ++i)
        {
            auto node = std::make_unique<TempSummon>(&map);
            node->entry = 200000 + i;
            if (!chain.empty())
                Summoner(*chain.back(), *node);
            chain.push_back(std::move(node));
        }
        assert(RaidCreatureEligibility::HasExcludedOrigin(chain.front().get()));
        chain[1]->summoner = {}; // shallow NPC chain is permitted
        assert(!RaidCreatureEligibility::HasExcludedOrigin(chain.front().get()));
    }
    else if (scenario == "runtime-lifecycle")
    {
        mgr._originalSizes[map.id] = 40;
        assert(add.GetSpawnId() == 0 && add.GetGUID());
        map.GetObjectsStore().Insert<Creature>(add.GetGUID(), &add);
        assert(map.GetCreatureBySpawnIdStore().empty());
        auto expect = [&](uint32 max, uint32 hp, uint32 hit)
        {
            assert(add.GetCreateHealth() == max && add.GetMaxHealth() == max);
            assert(add.GetHealth() == hp && damage() == hit);
            assert(add.GetFlatModifierValue(UNIT_MOD_HEALTH, BASE_VALUE) == 3052);
        };
        mgr.OnCreatureAddWorld(&add);
        expect(763, 763, 326);
        add.SetHealth(381);
        expect(763, 381, 326);
        assert(mgr.SetMultiplier(&map, "trash", "hp", .5f));
        expect(1526, 762, 326);
        mgr.ApplyToMap(&map);
        expect(1526, 762, 326);
        assert(mgr.DisableForMap(&map));
        expect(3052, 1524, 750);
        assert(mgr.EnableForMap(&map, 10));
        expect(763, 381, 326);
        mgr.ApplyToMap(&map);
        expect(763, 381, 326);
        map.GetObjectsStore().Remove<Creature>(add.GetGUID());

        assert(mgr.DisableForMap(&map));
        TempSummon offSpawn(&map);
        Hostile(offSpawn);
        map.GetObjectsStore().Insert<Creature>(offSpawn.GetGUID(), &offSpawn);
        mgr.OnCreatureAddWorld(&offSpawn);
        assert(offSpawn.GetMaxHealth() == 3052 && offSpawn.GetHealth() == 3052);
        assert(mgr.EnableForMap(&map, 10));
        assert(offSpawn.GetMaxHealth() == 763 && offSpawn.GetHealth() == 763);
        offSpawn.SetHealth(1);
        assert(mgr.SetMultiplier(&map, "trash", "hp", .05f));
        assert(offSpawn.IsAlive() && offSpawn.GetMaxHealth() == 153 && offSpawn.GetHealth() == 1);
        mgr.ApplyToMap(&map);
        assert(offSpawn.IsAlive() && offSpawn.GetHealth() == 1);
        mgr.DisableForMap(&map);
        assert(offSpawn.IsAlive() && offSpawn.GetHealth() == 20);
        assert(offSpawn.flat[BASE_VALUE] == 3052);
        map.GetObjectsStore().Remove<Creature>(offSpawn.GetGUID());

        for (DeathState state : {DeathState::Corpse, DeathState::Dead})
        {
            assert(mgr.EnableForMap(&map, 10));
            TempSummon dead(&map);
            Hostile(dead);
            map.GetObjectsStore().Insert<Creature>(dead.GetGUID(), &dead);
            mgr.OnCreatureAddWorld(&dead);
            dead.SetHealth(0);
            dead.deathState = state;
            assert(mgr.SetMultiplier(&map, "trash", "hp", .5f));
            assert(dead.GetHealth() == 0 && dead.GetMaxHealth() == 763);
            assert(mgr.DisableForMap(&map));
            assert(dead.GetCreateHealth() == 3052 && dead.GetMaxHealth() == 3052);
            assert(dead.GetHealth() == 0 && dead.deathState == state);
            assert(mgr.EnableForMap(&map, 10));
            assert(dead.GetHealth() == 0 && dead.deathState == state);
            assert(dead.flat[BASE_VALUE] == 3052);
            map.GetObjectsStore().Remove<Creature>(dead.GetGUID());
        }

        auto removed = std::make_unique<TempSummon>(&map);
        Hostile(*removed);
        map.GetObjectsStore().Insert<Creature>(removed->GetGUID(), removed.get());
        mgr.OnCreatureAddWorld(removed.get());
        ObjectGuid gone = removed->GetGUID();
        map.GetObjectsStore().Remove<Creature>(gone);
        removed.reset();
        assert(!map.GetCreature(gone));
        mgr.ApplyToMap(&map);
        mgr.DisableForMap(&map); // stale stats cache must not become an object pointer
    }
    else if (scenario == "runtime-removal")
    {
        // Remove/destroy a later snapshot member during both apply and restore. ASan catches
        // retained pointers; native lookup safely returns nullptr when the later GUID is used.
        for (bool restore : {false, true})
        {
            Enable(mgr, map);
            std::vector<std::unique_ptr<TempSummon>> creatures;
            for (uint32 i = 0; i < 2; ++i)
            {
                creatures.push_back(std::make_unique<TempSummon>(&map));
                Hostile(*creatures.back());
                map.GetObjectsStore().Insert<Creature>(creatures.back()->GetGUID(), creatures.back().get());
                mgr.OnCreatureAddWorld(creatures.back().get());
            }
            auto guids = GetLoadedCreatureGuids(&map);
            assert(guids.size() == 2);
            Creature* first = map.GetCreature(guids.front());
            uint32 removed = 0;
            first->afterHealthChange = [&]
            {
                for (auto& creature : creatures)
                    if (creature && creature.get() != first)
                    {
                        map.GetObjectsStore().Remove<Creature>(creature->GetGUID());
                        creature.reset();
                        ++removed;
                    }
            };
            if (restore)
                mgr.RestoreMap(&map);
            else
                mgr.ApplyToMap(&map);
            assert(removed == 1 && !map.GetCreature(guids.back()));
            first->afterHealthChange = {};
            map.GetObjectsStore().Remove<Creature>(first->GetGUID());
        }
    }
    else if (scenario == "settings")
    {
        assert(damage() == 326);
        auto settings = Default();
        settings.trashDamage = .6f;
        settings.bossDamage = .2f;
        mgr._state.Set(mgr.MakeKey(&map), settings);
        mgr.OnCreatureAddWorld(&add);
        assert(damage() == 450); // spawn doesn't overwrite manual settings
        assert(mgr.GetSettings(&map)->trashDamage == .6f);
        mgr._enabled = false;
        assert(damage() == 750);
        mgr._enabled = true;
        map.raid = false;
        assert(damage() == 750);
        map.raid = true;
        map.instance = 0;
        assert(damage() == 750);
        map.instance = 2;
        assert(damage() == 750);
        map.instance = 1;
        mgr.DisableForMap(&map);
        mgr.OnCreatureAddWorld(&add);
        assert(damage() == 750 && !mgr.GetSettings(&map));
        assert(mgr.GetDamageScale(nullptr, &player) == 1);
        assert(mgr.GetDamageScale(&add, nullptr) == 1);
    }
    else
        return 1;
    std::cout << "Passed " << scenario << '\n';
}
