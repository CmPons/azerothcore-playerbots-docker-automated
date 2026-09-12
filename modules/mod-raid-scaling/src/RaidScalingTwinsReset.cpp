#include "RaidScalingMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "GameObject.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PoolMgr.h"
#include "ThreatManager.h"
#include <algorithm>

namespace
{
    constexpr uint32 TwinsMap = 531;
    constexpr uint32 TwinsEncounter = 7;
    constexpr uint32 TwinsIntroTrigger = 4047;
    constexpr uint32 Veknilash = 15275;
    constexpr uint32 Veklor = 15276;
    constexpr uint32 MastersEye = 15963;

    bool TwinsResetCombatActive(InstanceMap* map)
    {
        if (map->GetInstanceScript()->IsEncounterInProgress())
            return true;
        for (auto const& reference : map->GetPlayers())
            if (Player* player = reference.GetSource())
                if (player->IsInCombat())
                    return true;
        for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
            if (creature && creature->IsInCombat())
                return true;
        return false;
    }

    bool IsTwinsResetBugEntry(uint32 entry)
    {
        return entry == 15316 || entry == 15317;
    }

    bool IsTwinsResetBug(ObjectGuid::LowType spawnId, CreatureData const& data)
    {
        if (!IsTwinsResetBugEntry(data.id))
            return false;
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Unit>(data.id, spawnId);
        return sObjectMgr->GetLinkedRespawnGuid(guid).GetEntry() == Veklor;
    }
}

bool RaidScalingMgr::RequestTwinsReset(ChatHandler* handler, uint32 instanceId)
{
    if (!handler || !instanceId)
        return false;
    auto ids = sMapMgr->GetInstanceIDs();
    if (instanceId >= ids.size() || !ids[instanceId])
    {
        handler->SendSysMessage("Twins reset refused: instance ID is not allocated.");
        return false;
    }
    uint32 expected = 0;
    if (!_pendingTwinsReset.compare_exchange_strong(expected, instanceId))
    {
        handler->PSendSysMessage("Twins reset refused: instance {} already has a pending request.", expected);
        return false;
    }
    if (Map* map = sMapMgr->FindMap(TwinsMap, instanceId))
    {
        bool success = ProcessPendingTwinsReset(map);
        handler->PSendSysMessage("Twins reset for instance {}: {}. See RaidScaling log for details.",
            instanceId, success ? "completed" : "failed; do not pull");
        return success;
    }
    handler->PSendSysMessage("Twins reset queued once for AQ40 instance {} on its next load. "
        "Not reset yet; request is lost on server restart. See RaidScaling log before pulling.", instanceId);
    return true;
}

bool RaidScalingMgr::ProcessPendingTwinsReset(Map* map)
{
    if (!map || map->GetId() != TwinsMap || !map->GetInstanceId() || !map->ToInstanceMap() ||
        !map->ToInstanceMap()->GetInstanceScript())
        return false;
    uint32 expected = map->GetInstanceId();
    if (!_pendingTwinsReset.compare_exchange_strong(expected, 0))
        return false;
    // Consume before callbacks: no automatic retry on combat/refusal or after a partial recovery.
    CliHandler handler(nullptr, [](void*, std::string_view message)
    {
        LOG_INFO("module", "[RaidScaling] Twins reset: {}", message);
    });
    bool success = ResetTwins(&handler, map->ToInstanceMap());
    LOG_INFO("module", "[RaidScaling] Twins reset for instance {} {}.", map->GetInstanceId(),
        success ? "completed" : "failed; do not pull, inspect the preceding message");
    return success;
}

bool RaidScalingMgr::ResetTwins(ChatHandler* handler, InstanceMap* map)
{
    if (!handler || !map || map->GetId() != TwinsMap || !map->GetInstanceScript())
        return false;
    InstanceScript* instance = map->GetInstanceScript();
    if (TwinsResetCombatActive(map))
    {
        handler->SendSysMessage("Twins reset refused: an encounter, player or creature is still in combat.");
        return false;
    }

    // Completion-mask indices are DBC encounter indices, NOT the script's boss-state indices.
    uint32 clearMask = 0;
    if (DungeonEncounterList const* encounters = sObjectMgr->GetDungeonEncounterList(TwinsMap, map->GetDifficulty()))
        for (DungeonEncounter const* encounter : *encounters)
            if (encounter && encounter->creditType == ENCOUNTER_CREDIT_KILL_CREATURE &&
                (encounter->creditEntry == Veknilash || encounter->creditEntry == Veklor))
            {
                if (!encounter->dbcEntry || encounter->dbcEntry->mapId != TwinsMap ||
                    encounter->dbcEntry->encounterIndex >= 32)
                {
                    handler->SendSysMessage("Twins reset refused: invalid encounter-credit mapping.");
                    return false;
                }
                clearMask |= uint32(1) << encounter->dbcEntry->encounterIndex;
            }
    if (!clearMask)
    {
        handler->SendSysMessage("Twins reset refused: missing encounter-credit mapping; no state was changed.");
        return false;
    }

    // Use the complete spawn catalogue, not just corpses still loaded near the caller.
    std::vector<ObjectGuid::LowType> bosses, eyes, bugs;
    uint32 meleeCount = 0, casterCount = 0, scarabCount = 0, scorpionCount = 0;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if (data.mapid != TwinsMap || !(data.spawnMask & (uint32(1) << map->GetSpawnMode())))
            continue;
        bool const bug = IsTwinsResetBug(spawnId, data);
        if (!bug && data.id != Veknilash && data.id != Veklor && data.id != MastersEye)
            continue;
        if (sPoolMgr->IsPartOfAPool<Creature>(spawnId))
        {
            handler->PSendSysMessage("Twins reset refused: spawn {} (entry {}) is pooled.", spawnId, data.id);
            return false;
        }
        if (!map->IsSpawnGroupActive(data.spawnGroupId))
        {
            handler->PSendSysMessage("Twins reset refused: spawn {} (entry {}) has inactive group {}.",
                spawnId, data.id, data.spawnGroupId);
            return false;
        }
        // Native room spawns randomly choose scarab or scorpion from creature_multispawn.
        // Permit that pair only on Vek'lor-linked bugs; bosses/controller remain single-entry.
        if ((data.id2 && (!bug || !IsTwinsResetBugEntry(data.id2))) ||
            (data.id3 && (!bug || !IsTwinsResetBugEntry(data.id3))))
        {
            handler->PSendSysMessage("Twins reset refused: spawn {} has unsupported entries {}/{}/{}.",
                spawnId, data.id, data.id2, data.id3);
            return false;
        }
        if (bug)
        {
            bugs.push_back(spawnId);
            data.id == 15316 ? ++scarabCount : ++scorpionCount;
        }
        else if (data.id == MastersEye)
            eyes.push_back(spawnId);
        else
        {
            bosses.push_back(spawnId);
            data.id == Veknilash ? ++meleeCount : ++casterCount;
        }
    }
    if (meleeCount != 1 || casterCount != 1 || eyes.size() != 1 || !scarabCount || !scorpionCount)
    {
        handler->SendSysMessage("Twins reset refused: incomplete or ambiguous encounter spawn catalogue.");
        return false;
    }

    std::vector<ObjectGuid::LowType> doors;
    uint32 entranceCount = 0, exitCount = 0;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
        if (data.mapid == TwinsMap && (data.spawnMask & (uint32(1) << map->GetSpawnMode())) &&
            (data.id == 180634 || data.id == 180635))
        {
            if (sPoolMgr->IsPartOfAPool<GameObject>(spawnId) || !map->IsSpawnGroupActive(data.spawnGroupId))
            {
                handler->SendSysMessage("Twins reset refused: an encounter door is pooled or inactive.");
                return false;
            }
            doors.push_back(spawnId);
            data.id == 180634 ? ++entranceCount : ++exitCount;
        }
    if (entranceCount != 1 || exitCount != 1)
    {
        handler->SendSysMessage("Twins reset refused: incomplete or ambiguous encounter doors.");
        return false;
    }

    // Recreate emperors before bugs so linked_respawn cannot postpone bugs behind a dead master.
    std::vector<ObjectGuid::LowType> spawns = bosses;
    spawns.insert(spawns.end(), eyes.begin(), eyes.end());
    spawns.insert(spawns.end(), bugs.begin(), bugs.end());
    for (ObjectGuid::LowType spawnId : spawns)
    {
        CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);
        map->LoadGrid(data->posX, data->posY);
    }
    for (ObjectGuid::LowType spawnId : doors)
    {
        GameObjectData const* data = sObjectMgr->GetGameObjectData(spawnId);
        map->LoadGrid(data->posX, data->posY);
    }
    if (TwinsResetCombatActive(map))
    {
        handler->SendSysMessage("Twins reset refused: combat was detected after loading the encounter room.");
        return false;
    }
    for (ObjectGuid::LowType spawnId : spawns)
    {
        auto bounds = map->GetCreatureBySpawnIdStore().equal_range(spawnId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (Creature* creature = itr->second)
                if (creature->GetCharmerOrOwnerGUID())
                {
                    handler->SendSysMessage("Twins reset refused: release control of encounter creatures first.");
                    return false;
                }
    }

    instance->SetBossState(TwinsEncounter, NOT_STARTED);
    if (instance->GetBossState(TwinsEncounter) != NOT_STARTED)
    {
        handler->SendSysMessage("Twins reset refused: the instance script did not accept NOT_STARTED.");
        return false;
    }
    instance->SetCompletedEncountersMask(instance->GetCompletedEncounterMask() & ~clearMask, true);
    instance->ResetAreaTriggerDone(TwinsIntroTrigger);

    uint32 restored = 0;
    for (ObjectGuid::LowType spawnId : spawns)
    {
        // Respawn can alter the spawn store. Snapshot GUIDs and re-resolve within this call.
        std::vector<ObjectGuid> loaded;
        auto bounds = map->GetCreatureBySpawnIdStore().equal_range(spawnId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (itr->second)
                loaded.push_back(itr->second->GetGUID());
        for (ObjectGuid guid : loaded)
            if (Creature* creature = map->GetCreature(guid))
            {
                creature->InterruptNonMeleeSpells(true);
                creature->m_Events.KillAllEvents(false);
                creature->GetThreatMgr().ClearAllThreat();
                creature->RemoveAllAuras();
                // Native forced respawn, NOT Unit::Kill: no new kill reward or shared-health death callback.
                creature->Respawn(true);
                if (creature->IsAlive() && !creature->AIM_Initialize())
                {
                    handler->SendSysMessage("Twins reset incomplete: AI reinitialization failed. Retry out of combat.");
                    return false;
                }
            }

        // Non-compat corpses are queued for removal; recreate this original spawn through the
        // native map path. Do not drain unrelated respawn/removal queues or summon duplicate adds.
        time_t now = GameTime::GetGameTime().count();
        map->SaveCreatureRespawnTime(spawnId, now);
        map->ProcessCreatureRespawn(spawnId);
        uint32 alive = 0;
        bounds = map->GetCreatureBySpawnIdStore().equal_range(spawnId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (Creature* creature = itr->second)
                if (creature->IsAlive())
                {
                    ++alive;
                    ApplyToCreature(creature);
                }
        if (alive != 1)
        {
            handler->SendSysMessage("Twins reset incomplete: a native respawn is not ready. "
                "Do not pull; retry the reset.");
            return false;
        }
        ++restored;
    }

    // SetBossState updates normal loaded doors. Also repair despawned doors and repeat the state
    // assignment explicitly for idempotent NOT_STARTED resets, without touching other raid doors.
    RespawnGameObjectEntries(map, {180634, 180635});
    for (ObjectGuid::LowType spawnId : doors)
    {
        map->ProcessGameObjectRespawn(spawnId);
        uint32 ready = 0;
        auto bounds = map->GetGameObjectBySpawnIdStore().equal_range(spawnId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (GameObject* go = itr->second)
                if (go->isSpawned())
                {
                    ++ready;
                    instance->HandleGameObject(go->GetGUID(), go->GetEntry() == 180634, go);
                }
        if (ready != 1)
        {
            handler->SendSysMessage("Twins reset incomplete: an encounter door is not ready. "
                "Retry before pulling.");
            return false;
        }
    }

    handler->PSendSysMessage("Twin Emperors reset: restored {} original spawns ({} room bugs), "
        "cleared their completion credit and reset the intro/doors. Other kills, binds and reset deadline retained. "
        "Leave and re-enter the "
        "room entrance to replay the intro before pulling.", restored, bugs.size());
    return true;
}
