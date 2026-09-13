#ifndef MOD_RAID_CREATURE_ELIGIBILITY_H
#define MOD_RAID_CREATURE_ELIGIBILITY_H

#include "Creature.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "TemporarySummon.h"
#include <array>

namespace RaidCreatureEligibility
{
    // Object-owned, ephemeral provenance: a child can outlive its player-owned summoner.
    struct Origin : DataMap::Base
    {
        bool excluded = false;
    };

    inline constexpr char OriginKey[] = "mod-raid-scaling.origin";

    inline bool HasExcludedOrigin(WorldObject const* object, std::array<WorldObject const*, 16>& path,
        uint32 depth, uint32& budget)
    {
        if (!object)
            return false; // NPC summoners routinely despawn before their encounter hazards.
        if (depth == path.size() || !budget)
            return true;
        --budget;
        for (uint32 i = 0; i < depth; ++i)
            if (path[i] == object)
                return true;
        path[depth] = object;

        if (Origin const* origin = object->CustomData.Get<Origin>(OriginKey))
            if (origin->excluded)
                return true;

        std::array<ObjectGuid, 4> sources{};
        if (Unit const* unit = object->ToUnit())
        {
            if (unit->IsPlayer() || unit->IsPet() || unit->IsControlledByPlayer() || unit->IsCreatedByPlayer())
                return true;
            sources[0] = unit->GetCharmerGUID();
            sources[1] = unit->GetOwnerGUID();
            sources[2] = unit->GetCreatorGUID();
            if (TempSummon const* summon = unit->ToTempSummon())
                sources[3] = summon->GetSummonerGUID();
        }
        else if (GameObject const* gameObject = object->ToGameObject())
            sources[0] = gameObject->GetOwnerGUID();

        for (uint32 i = 0; i < sources.size(); ++i)
        {
            ObjectGuid guid = sources[i];
            if (!guid)
                continue;
            if (guid.IsPlayer())
                return true; // Works even after the player leaves the map.
            bool duplicate = false;
            for (uint32 j = 0; j < i; ++j)
                duplicate |= sources[j] == guid;
            if (!duplicate && HasExcludedOrigin(ObjectAccessor::GetWorldObject(*object, guid), path, depth + 1, budget))
                return true;
        }
        return false;
    }

    inline bool HasExcludedOrigin(Creature const* creature)
    {
        std::array<WorldObject const*, 16> path{};
        uint32 budget = 64;
        return HasExcludedOrigin(creature, path, 0, budget);
    }

    inline void CaptureSummonOrigin(Creature* creature)
    {
        if (!creature->IsSummon())
            return;
        bool const excluded = HasExcludedOrigin(creature);
        creature->CustomData.GetDefault<Origin>(OriginKey)->excluded = excluded;
    }
}

#endif
