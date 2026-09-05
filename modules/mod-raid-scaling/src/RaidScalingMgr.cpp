#include "RaidScalingMgr.h"

#include "CellImpl.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PoolMgr.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Unit.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace
{
    std::vector<RaidBossResetRecipe> const EmptyBosses;

    std::vector<RaidBossResetRecipe> const& BossesForMap(uint32 mapId)
    {
        // display id is the small id printed to the player; encounter id is InstanceScript boss state id.
        static std::vector<RaidBossResetRecipe> const MC =
        {
            {1, 0, "Lucifron", "respawnable", {12118, 12119}, {}},
            {2, 1, "Magmadar", "respawnable", {11982}, {}},
            {3, 2, "Gehennas", "respawnable", {12259, 11661}, {}},
            {4, 3, "Garr", "respawnable", {12057, 12099}, {}},
            {5, 4, "Shazzrah", "respawnable", {12264}, {}},
            {6, 5, "Baron Geddon", "respawnable", {12056}, {}},
            {7, 6, "Sulfuron Harbinger", "respawnable", {12098, 11662}, {}},
            {8, 7, "Golemagg", "respawnable", {11988, 11672}, {}},
            {9, 8, "Majordomo Executus", "partial/event", {12018, 11663, 11664}, {}, true},
            {10, 9, "Ragnaros", "partial/event", {11502}, {}, true},
        };

        static std::vector<RaidBossResetRecipe> const ZG =
        {
            {1, 0, "High Priestess Jeklik", "respawnable", {14517}, {}},
            {2, 1, "High Priest Venoxis", "respawnable", {14507}, {}},
            {3, 2, "High Priestess Mar'li", "respawnable", {14510, 15041}, {}},
            {4, 3, "High Priestess Arlokk", "partial/event", {14515, 15101}, {180497, 180526}, true},
            {5, 4, "High Priest Thekal", "respawnable/adds", {14509, 11347, 11348}, {}},
            {6, 5, "Hakkar", "respawnable", {14834}, {}},
            {7, 6, "Bloodlord Mandokir", "respawnable/adds", {11382, 14988}, {}},
            {8, 7, "Jin'do the Hexxer", "respawnable/adds", {11380, 14986, 15112}, {}},
            {9, 8, "Gahz'ranka", "respawnable", {15114}, {}},
            {10, 9, "Edge of Madness", "partial/random", {15082, 15083, 15084, 15085}, {}, true},
        };

        static std::vector<RaidBossResetRecipe> const AQ20 =
        {
            {1, 0, "Kurinnaxx", "respawnable", {15348}, {}},
            {2, 1, "General Rajaxx", "partial/waves", {15341, 15391, 15392, 15389, 15390, 15386, 15388, 15385, 15471, 15473}, {}, true},
            {3, 2, "Moam", "respawnable", {15340}, {}},
            {4, 3, "Buru the Gorger", "partial/event", {15370, 15514}, {}, true},
            {5, 4, "Ayamiss the Hunter", "respawnable", {15369}, {}},
            {6, 5, "Ossirian the Unscarred", "partial/crystals", {15339, 15590}, {180619}, true},
        };

        static std::vector<RaidBossResetRecipe> const BWL =
        {
            {1, 0, "Razorgore", "partial/egg event", {12435, 12557, 14459, 12422, 12458, 12416, 12459, 12420}, {177807, 175946, 176964}, true},
            {2, 1, "Vaelastrasz", "respawnable", {13020}, {175185}},
            {3, 2, "Broodlord Lashlayer", "respawnable", {12017}, {179365}},
            {4, 3, "Firemaw", "respawnable", {11983}, {}},
            {5, 4, "Ebonroc", "respawnable", {14601}, {}},
            {6, 5, "Flamegor", "respawnable", {11981}, {}},
            {7, 6, "Chromaggus", "partial/doors", {14020}, {179116, 179117}, true},
            {8, 7, "Nefarian", "partial/event", {10162, 11583, 14307, 14309, 14310, 14311, 14312, 14605}, {176966}, true},
        };

        static std::vector<RaidBossResetRecipe> const AQ40 =
        {
            {1, 1, "The Prophet Skeram", "respawnable", {15263}, {180636}},
            {2, 2, "Bug Trio", "respawnable/adds", {15544, 15511, 15543}, {}},
            {3, 3, "Battleguard Sartura", "respawnable/adds", {15516, 15984}, {}},
            {4, 4, "Fankriss", "respawnable", {15510}, {}},
            {5, 5, "Viscidus", "partial/splits", {15299, 15667}, {}, true},
            {6, 6, "Princess Huhuran", "respawnable", {15509}, {}},
            {7, 7, "Twin Emperors", "partial/shared health", {15275, 15276, 15963}, {180634, 180635}, true},
            {8, 8, "Ouro", "partial/submerge", {15517, 15957}, {} , true},
            {9, 9, "C'Thun", "partial/multi-phase", {15589, 15727, 15809, 15725, 15726, 15728, 15334}, {180745}, true},
        };

        switch (mapId)
        {
            case 409: return MC;
            case 309: return ZG;
            case 509: return AQ20;
            case 469: return BWL;
            case 531: return AQ40;
            default: return EmptyBosses;
        }
    }

    std::string FormatFloat(float value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3) << value;
        return out.str();
    }

    char const* StateName(uint32 state)
    {
        switch (state)
        {
            case NOT_STARTED: return "NOT_STARTED";
            case IN_PROGRESS: return "IN_PROGRESS";
            case FAIL: return "FAIL";
            case DONE: return "DONE";
            case SPECIAL: return "SPECIAL";
            case TO_BE_DECIDED: return "TO_BE_DECIDED";
            default: return "UNKNOWN";
        }
    }
}

RaidScalingMgr& RaidScalingMgr::Instance()
{
    static RaidScalingMgr instance;
    return instance;
}

void RaidScalingMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("RaidScaling.Enable", true);
    _commandSecurity = sConfigMgr->GetOption<uint32>("RaidScaling.CommandSecurity", 2);
    _blizzardLikeDefault = sConfigMgr->GetOption<bool>("RaidScaling.BlizzardLikeDefault", true);
    _damageExponent = sConfigMgr->GetOption<float>("RaidScaling.DamageExponent", 0.6f);
    _minHealth = sConfigMgr->GetOption<float>("RaidScaling.MinHealthMultiplier", 0.05f);
    _minDamage = sConfigMgr->GetOption<float>("RaidScaling.MinDamageMultiplier", 0.05f);
    _maxHealth = sConfigMgr->GetOption<float>("RaidScaling.MaxHealthMultiplier", 5.0f);
    _maxDamage = sConfigMgr->GetOption<float>("RaidScaling.MaxDamageMultiplier", 5.0f);

    _originalSizes =
    {
        {249, 40}, {309, 20}, {409, 40}, {469, 40}, {509, 20}, {531, 40}, {533, 40},
        {532, 10}, {565, 25}, {544, 25}, {548, 25}, {550, 25}, {534, 25}, {564, 25}, {580, 25},
    };

    for (auto& [mapId, size] : _originalSizes)
        size = sConfigMgr->GetOption<uint32>("RaidScaling.OriginalSize." + std::to_string(mapId), size);
}

RaidScalingMgr::InstanceKey RaidScalingMgr::MakeKey(Map const* map) const
{
    return map ? InstanceKey{map->GetId(), map->GetInstanceId()} : InstanceKey{};
}

uint32 RaidScalingMgr::GetOriginalSize(uint32 mapId) const
{
    auto itr = _originalSizes.find(mapId);
    return itr != _originalSizes.end() ? itr->second : 0;
}

float RaidScalingMgr::ClampHealth(float value) const
{
    return std::min(_maxHealth, std::max(_minHealth, value));
}

float RaidScalingMgr::ClampDamage(float value) const
{
    return std::min(_maxDamage, std::max(_minDamage, value));
}

bool RaidScalingMgr::EnableForMap(Map* map, uint32 targetPlayers, ChatHandler* handler)
{
    if (!_enabled)
    {
        if (handler) handler->SendSysMessage("RaidScaling is disabled.");
        return false;
    }

    if (!map || !map->IsRaid() || !map->GetInstanceId())
    {
        if (handler) handler->SendSysMessage("Stand inside a raid instance first.");
        return false;
    }

    uint32 original = GetOriginalSize(map->GetId());
    if (!original)
    {
        if (handler) handler->PSendSysMessage("No original raid size configured for map {}.", map->GetId());
        return false;
    }

    targetPlayers = std::max<uint32>(1, targetPlayers);
    float ratio = float(targetPlayers) / float(original);
    float damage = _blizzardLikeDefault ? std::pow(ratio, _damageExponent) : ratio;

    RaidScaleSettings settings;
    settings.targetPlayers = targetPlayers;
    settings.originalPlayers = original;
    settings.bossHealth = ClampHealth(ratio);
    settings.trashHealth = ClampHealth(ratio);
    settings.bossDamage = ClampDamage(damage);
    settings.trashDamage = ClampDamage(damage);

    _instances[MakeKey(map)] = settings;
    ApplyToMap(map, handler);

    if (handler)
        handler->PSendSysMessage("RaidScale enabled for {} #{}: {} -> {} players (hp {}, damage {}).",
            map->GetMapName(), map->GetInstanceId(), original, targetPlayers, FormatFloat(settings.bossHealth), FormatFloat(settings.bossDamage));
    return true;
}

bool RaidScalingMgr::DisableForMap(Map* map, ChatHandler* handler)
{
    if (!map)
        return false;

    RestoreMap(map, handler);
    _instances.erase(MakeKey(map));

    if (handler)
        handler->PSendSysMessage("RaidScale disabled for {} #{}.", map->GetMapName(), map->GetInstanceId());
    return true;
}

bool RaidScalingMgr::HasScaling(Map const* map) const
{
    return GetSettings(map) != nullptr;
}

RaidScaleSettings const* RaidScalingMgr::GetSettings(Map const* map) const
{
    if (!map)
        return nullptr;

    auto itr = _instances.find(MakeKey(map));
    return itr != _instances.end() ? &itr->second : nullptr;
}

bool RaidScalingMgr::IsScalableCreature(Creature const* creature) const
{
    if (!creature || creature->IsPet() || creature->IsTrigger() || creature->IsCritter() || creature->IsCivilian())
        return false;

    CreatureTemplate const* proto = creature->GetCreatureTemplate();
    if (!proto)
        return false;

    return creature->IsDungeonBoss() || creature->isWorldBoss() || proto->rank == CREATURE_ELITE_ELITE ||
        proto->rank == CREATURE_ELITE_RAREELITE || proto->rank == CREATURE_ELITE_WORLDBOSS;
}

RaidScaleCreatureKind RaidScalingMgr::ClassifyCreature(Creature const* creature) const
{
    if (!creature)
        return RaidScaleCreatureKind::Trash;

    CreatureTemplate const* proto = creature->GetCreatureTemplate();
    if (creature->IsDungeonBoss() || creature->isWorldBoss() || (proto && proto->rank == CREATURE_ELITE_WORLDBOSS))
        return RaidScaleCreatureKind::Boss;

    return RaidScaleCreatureKind::Trash;
}

float RaidScalingMgr::HealthScaleFor(Creature const* creature, RaidScaleSettings const& settings) const
{
    return ClassifyCreature(creature) == RaidScaleCreatureKind::Boss ? settings.bossHealth : settings.trashHealth;
}

float RaidScalingMgr::DamageScaleFor(Creature const* creature, RaidScaleSettings const& settings) const
{
    return ClassifyCreature(creature) == RaidScaleCreatureKind::Boss ? settings.bossDamage : settings.trashDamage;
}

uint32 RaidScalingMgr::ScaleHealth(uint32 value, float scale) const
{
    return std::max<uint32>(1, uint32(std::round(float(value) * scale)));
}

void RaidScalingMgr::OnCreatureAddWorld(Creature* creature)
{
    if (!_enabled || !creature || !creature->GetMap() || !HasScaling(creature->GetMap()))
        return;

    ApplyToCreature(creature);
}

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

void RaidScalingMgr::ApplyToCreature(Creature* creature)
{
    if (!creature || !creature->GetMap() || !IsScalableCreature(creature) || creature->isDead())
        return;

    RaidScaleSettings const* settings = GetSettings(creature->GetMap());
    if (!settings)
        return;

    ObjectGuid guid = creature->GetGUID();
    OriginalCreatureStats& original = _originalCreatureStats[guid];
    if (!original.maxHealth)
    {
        original.createHealth = creature->GetCreateHealth();
        original.maxHealth = creature->GetMaxHealth();
        original.health = creature->GetHealth();
    }

    float scale = HealthScaleFor(creature, *settings);
    uint32 newCreate = ScaleHealth(original.createHealth ? original.createHealth : original.maxHealth, scale);
    uint32 newMax = ScaleHealth(original.maxHealth, scale);
    float pct = original.maxHealth ? std::min(1.0f, float(creature->GetHealth()) / float(creature->GetMaxHealth())) : 1.0f;
    if (creature->GetMaxHealth() == original.maxHealth)
        pct = original.maxHealth ? std::min(1.0f, float(original.health) / float(original.maxHealth)) : 1.0f;

    creature->SetCreateHealth(newCreate);
    creature->SetMaxHealth(newMax);
    creature->SetHealth(std::max<uint32>(1, uint32(std::round(float(newMax) * pct))));
    creature->ResetPlayerDamageReq();
}

void RaidScalingMgr::RestoreCreature(Creature* creature)
{
    if (!creature)
        return;

    auto itr = _originalCreatureStats.find(creature->GetGUID());
    if (itr == _originalCreatureStats.end())
        return;

    OriginalCreatureStats const original = itr->second;
    float pct = creature->GetMaxHealth() ? std::min(1.0f, float(creature->GetHealth()) / float(creature->GetMaxHealth())) : 1.0f;

    creature->SetCreateHealth(original.createHealth);
    creature->SetMaxHealth(original.maxHealth);
    creature->SetHealth(std::max<uint32>(1, uint32(std::round(float(original.maxHealth) * pct))));
    creature->ResetPlayerDamageReq();
    _originalCreatureStats.erase(itr);
}

float RaidScalingMgr::GetDamageScale(Unit* attacker, Unit* victim) const
{
    if (!_enabled || !attacker || !victim)
        return 1.0f;

    Creature* creature = attacker->ToCreature();
    if (!creature || !IsScalableCreature(creature))
        return 1.0f;

    RaidScaleSettings const* settings = GetSettings(creature->GetMap());
    if (!settings)
        return 1.0f;

    // Only scale hostile raid damage against the player party/raid. Leave creature-vs-creature
    // scripted event combat alone (e.g. escorts, friendly NPCs, add interactions).
    if (!victim->IsPlayer() && !victim->IsControlledByPlayer())
        return 1.0f;

    return DamageScaleFor(creature, *settings);
}

bool RaidScalingMgr::SetMultiplier(Map* map, std::string const& creatureKind, std::string const& statKind, float value, ChatHandler* handler)
{
    RaidScaleSettings* settings = nullptr;
    auto itr = _instances.find(MakeKey(map));
    if (itr != _instances.end())
        settings = &itr->second;

    if (!settings)
    {
        if (handler) handler->SendSysMessage("RaidScale is not enabled for this instance. Use .raidscale 10 first.");
        return false;
    }

    bool boss = creatureKind == "boss" || creatureKind == "bosses";
    bool trash = creatureKind == "trash" || creatureKind == "mob" || creatureKind == "mobs";
    bool hp = statKind == "hp" || statKind == "health";
    bool dmg = statKind == "damage" || statKind == "dmg";

    if ((!boss && !trash) || (!hp && !dmg))
    {
        if (handler) handler->SendSysMessage("Usage: .raidscale boss|trash hp|damage <multiplier>");
        return false;
    }

    value = hp ? ClampHealth(value) : ClampDamage(value);
    if (boss && hp) settings->bossHealth = value;
    if (boss && dmg) settings->bossDamage = value;
    if (trash && hp) settings->trashHealth = value;
    if (trash && dmg) settings->trashDamage = value;

    if (hp)
        ApplyToMap(map, nullptr);

    if (handler)
        handler->PSendSysMessage("RaidScale {} {} set to {}.", creatureKind, statKind, FormatFloat(value));
    return true;
}

void RaidScalingMgr::SendStatus(ChatHandler* handler, Map* map) const
{
    if (!handler)
        return;

    if (!map || !map->IsRaid() || !map->GetInstanceId())
    {
        handler->SendSysMessage("Stand inside a raid instance first.");
        return;
    }

    RaidScaleSettings const* s = GetSettings(map);
    if (!s)
    {
        handler->PSendSysMessage("RaidScale inactive for {} #{} (original size {}).", map->GetMapName(), map->GetInstanceId(), GetOriginalSize(map->GetId()));
        return;
    }

    handler->PSendSysMessage("RaidScale active for {} #{}: original {}, target {}, boss hp {}, boss damage {}, trash hp {}, trash damage {}.",
        map->GetMapName(), map->GetInstanceId(), s->originalPlayers, s->targetPlayers,
        FormatFloat(s->bossHealth), FormatFloat(s->bossDamage), FormatFloat(s->trashHealth), FormatFloat(s->trashDamage));
}

void RaidScalingMgr::Export(ChatHandler* handler, Map* map) const
{
    if (!handler)
        return;

    RaidScaleSettings const* s = GetSettings(map);
    if (!map || !s)
    {
        handler->SendSysMessage("RaidScale is not enabled for this instance.");
        return;
    }

    handler->PSendSysMessage("# RaidScale export for map {} ({})", map->GetId(), map->GetMapName());
    handler->PSendSysMessage("RaidScaling.OriginalSize.{} = {}", map->GetId(), s->originalPlayers);
    handler->PSendSysMessage("# TargetPlayers = {}", s->targetPlayers);
    handler->PSendSysMessage("# BossHealth = {}", FormatFloat(s->bossHealth));
    handler->PSendSysMessage("# BossDamage = {}", FormatFloat(s->bossDamage));
    handler->PSendSysMessage("# TrashHealth = {}", FormatFloat(s->trashHealth));
    handler->PSendSysMessage("# TrashDamage = {}", FormatFloat(s->trashDamage));
}

std::vector<RaidBossResetRecipe> const& RaidScalingMgr::GetBosses(uint32 mapId) const
{
    return BossesForMap(mapId);
}

void RaidScalingMgr::SendBossList(ChatHandler* handler, InstanceMap* map) const
{
    if (!handler || !map)
        return;

    InstanceScript* instance = map->GetInstanceScript();
    if (!instance)
    {
        handler->SendSysMessage("This instance has no boss state script.");
        return;
    }

    std::vector<RaidBossResetRecipe> const& bosses = GetBosses(map->GetId());
    if (bosses.empty())
    {
        handler->PSendSysMessage("No boss reset recipes for map {} yet.", map->GetId());
        return;
    }

    handler->PSendSysMessage("Bosses for {} #{}:", map->GetMapName(), map->GetInstanceId());
    for (RaidBossResetRecipe const& boss : bosses)
    {
        uint32 state = instance->GetBossState(boss.encounterId);
        handler->PSendSysMessage("[{}] {}  {}  {}", boss.displayId, boss.name, StateName(state), boss.support);
    }
}

uint32 RaidScalingMgr::RespawnCreatureEntries(Map* map, std::vector<uint32> const& entries) const
{
    if (!map)
        return 0;

    std::unordered_set<uint32> wanted(entries.begin(), entries.end());
    time_t now = GameTime::GetGameTime().count();
    uint32 count = 0;

    std::vector<Creature*> deadCreatures;
    for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
    {
        CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);
        if (!data || !wanted.count(data->id) || !creature || !creature->isDead())
            continue;
        deadCreatures.push_back(creature);
    }

    for (Creature* creature : deadCreatures)
    {
        creature->Respawn(true);
        ++count;
    }

    std::vector<ObjectGuid::LowType> toRespawn;
    for (auto const& [spawnId, respawnTime] : map->GetCreatureRespawnTimes())
    {
        CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);
        if (!data || !wanted.count(data->id))
            continue;
        if (sPoolMgr->IsPartOfAPool<Creature>(spawnId))
            continue;
        toRespawn.push_back(spawnId);
    }

    for (ObjectGuid::LowType spawnId : toRespawn)
    {
        map->SaveCreatureRespawnTime(spawnId, now);
        ++count;
    }

    return count;
}

uint32 RaidScalingMgr::RespawnGameObjectEntries(Map* map, std::vector<uint32> const& entries) const
{
    if (!map)
        return 0;

    std::unordered_set<uint32> wanted(entries.begin(), entries.end());
    time_t now = GameTime::GetGameTime().count();
    uint32 count = 0;

    std::vector<GameObject*> inactiveGOs;
    for (auto const& [spawnId, go] : map->GetGameObjectBySpawnIdStore())
    {
        GameObjectData const* data = sObjectMgr->GetGameObjectData(spawnId);
        if (!data || !wanted.count(data->id) || !go || go->isSpawned())
            continue;
        inactiveGOs.push_back(go);
    }

    for (GameObject* go : inactiveGOs)
    {
        go->Respawn();
        ++count;
    }

    std::vector<ObjectGuid::LowType> toRespawn;
    for (auto const& [spawnId, respawnTime] : map->GetGORespawnTimes())
    {
        GameObjectData const* data = sObjectMgr->GetGameObjectData(spawnId);
        if (!data || !wanted.count(data->id))
            continue;
        if (sPoolMgr->IsPartOfAPool<GameObject>(spawnId))
            continue;
        toRespawn.push_back(spawnId);
    }

    for (ObjectGuid::LowType spawnId : toRespawn)
    {
        map->SaveGORespawnTime(spawnId, now);
        ++count;
    }

    return count;
}

bool RaidScalingMgr::ResetBoss(ChatHandler* handler, InstanceMap* map, uint32 bossDisplayId)
{
    if (!handler || !map)
        return false;

    InstanceScript* instance = map->GetInstanceScript();
    if (!instance)
    {
        handler->SendSysMessage("This instance has no boss state script.");
        return false;
    }

    std::vector<RaidBossResetRecipe> const& bosses = GetBosses(map->GetId());
    auto itr = std::find_if(bosses.begin(), bosses.end(), [bossDisplayId](RaidBossResetRecipe const& boss)
    {
        return boss.displayId == bossDisplayId;
    });

    if (itr == bosses.end())
    {
        handler->SendSysMessage("Unknown boss id. Use .raidinstance boss list.");
        return false;
    }

    RaidBossResetRecipe const& boss = *itr;
    if (boss.encounterId >= instance->GetEncounterCount())
    {
        handler->PSendSysMessage("Boss '{}' recipe encounter id {} is outside this instance encounter count {}.", boss.name, boss.encounterId, instance->GetEncounterCount());
        return false;
    }

    instance->SetBossState(boss.encounterId, NOT_STARTED);
    uint32 creatures = RespawnCreatureEntries(map, boss.creatureEntries);
    uint32 gameObjects = RespawnGameObjectEntries(map, boss.gameObjectEntries);

    ApplyToMap(map, nullptr);

    handler->PSendSysMessage("Reset {}: boss state -> NOT_STARTED, respawned/queued {} creatures and {} gameobjects. Support: {}{}",
        boss.name, creatures, gameObjects, boss.support, boss.partial ? " (may need fresh instance for perfect script state)" : "");
    return true;
}

bool RaidScalingMgr::IsGroupMemberInMap(Player* leader, uint32 mapId, uint32 instanceId) const
{
    if (!leader || !leader->GetGroup())
        return leader && leader->GetMapId() == mapId && leader->GetInstanceId() == instanceId;

    Group* group = leader->GetGroup();
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member)
            continue;
        if (member->GetMapId() == mapId && member->GetInstanceId() == instanceId)
            return true;
    }

    return false;
}

bool RaidScalingMgr::ResetGroupBinds(ChatHandler* handler, Player* player, bool confirm)
{
    if (!handler || !player)
        return false;

    Map* map = player->GetMap();
    if (!map || !map->IsRaid())
    {
        handler->SendSysMessage("Stand in the raid map/instance you want to reset, or outside it after everyone has left.");
        return false;
    }

    uint32 mapId = map->GetId();
    uint32 instanceId = map->GetInstanceId();
    Difficulty difficulty = player->GetDifficulty(true);

    if (instanceId && IsGroupMemberInMap(player, mapId, instanceId) && !confirm)
    {
        handler->PSendSysMessage("Group members are still inside {} #{}. Leave first, or use .raidinstance reset all confirm.", map->GetMapName(), instanceId);
        return true;
    }

    std::vector<ObjectGuid> memberGuids;
    if (Group* group = player->GetGroup())
    {
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
            memberGuids.push_back(slot.guid);
    }
    else
        memberGuids.push_back(player->GetGUID());

    uint32 unbound = 0;
    for (ObjectGuid memberGuid : memberGuids)
    {
        Player* onlineMember = ObjectAccessor::FindConnectedPlayer(memberGuid);
        BoundInstancesMap const& binds = sInstanceSaveMgr->PlayerGetBoundInstances(memberGuid, difficulty);
        auto bindItr = binds.find(mapId);
        if (bindItr == binds.end())
            continue;

        InstanceSave const* save = bindItr->second.save;
        if (!save)
            continue;

        if (instanceId && save->GetInstanceId() != instanceId)
            continue;

        sInstanceSaveMgr->PlayerUnbindInstance(memberGuid, mapId, difficulty, true, onlineMember);
        ++unbound;
    }

    if (instanceId)
        DisableForMap(map, nullptr);

    handler->PSendSysMessage("Unbound {} online group member(s) from map {}{}.", unbound, mapId, instanceId ? " current instance" : "");
    handler->SendSysMessage("Offline members may still need a future DB-backed unbind pass if they were saved but not online.");
    return true;
}
