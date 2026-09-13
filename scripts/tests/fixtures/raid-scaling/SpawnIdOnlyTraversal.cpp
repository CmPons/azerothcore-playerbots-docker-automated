// Exact reviewed map walkers, retained solely for regression/mutant evidence.

void RaidScalingMgr::ApplyToMap(Map* map, ChatHandler* handler)
{
    if (!map || !HasScaling(map))
        return;

    uint32 count = 0;
    for (auto const& pair : map->GetCreatureBySpawnIdStore())
    {
        if (Creature* creature = pair.second)
        {
            if (!IsScalableCreature(creature) || creature->isDead())
                continue;
            ApplyToCreature(creature);
            ++count;
        }
    }

    if (handler)
        handler->PSendSysMessage("RaidScale applied to {} loaded creatures.", count);
}

void RaidScalingMgr::RestoreMap(Map* map, ChatHandler* handler)
{
    if (!map)
        return;

    uint32 count = 0;
    std::vector<Creature*> creatures;
    for (auto const& pair : map->GetCreatureBySpawnIdStore())
        if (pair.second)
            creatures.push_back(pair.second);

    for (Creature* creature : creatures)
    {
        RestoreCreature(creature);
        ++count;
    }

    if (handler)
        handler->PSendSysMessage("RaidScale restored {} loaded creatures.", count);
}
