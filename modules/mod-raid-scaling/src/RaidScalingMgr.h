#ifndef MOD_RAID_SCALING_MGR_H
#define MOD_RAID_SCALING_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>
#include <vector>

class ChatHandler;
class Creature;
class InstanceMap;
class Map;
class Player;
class Unit;

enum class RaidScaleCreatureKind : uint8
{
    Trash,
    Boss
};

struct RaidScaleSettings
{
    uint32 targetPlayers = 0;
    uint32 originalPlayers = 0;
    float bossHealth = 1.0f;
    float bossDamage = 1.0f;
    float trashHealth = 1.0f;
    float trashDamage = 1.0f;
};

struct RaidBossResetRecipe
{
    uint32 displayId = 0;
    uint32 encounterId = 0;
    char const* name = "";
    char const* support = "respawnable";
    std::vector<uint32> creatureEntries;
    std::vector<uint32> gameObjectEntries;
    bool partial = false;
};

class RaidScalingMgr
{
public:
    static RaidScalingMgr& Instance();

    void LoadConfig();

    bool Enabled() const { return _enabled; }
    uint32 CommandSecurity() const { return _commandSecurity; }

    bool EnableForMap(Map* map, uint32 targetPlayers, ChatHandler* handler = nullptr);
    bool DisableForMap(Map* map, ChatHandler* handler = nullptr);
    bool HasScaling(Map const* map) const;
    RaidScaleSettings const* GetSettings(Map const* map) const;
    float GetDamageScale(Unit* attacker, Unit* victim) const;

    void OnCreatureAddWorld(Creature* creature);
    void ApplyToMap(Map* map, ChatHandler* handler = nullptr);
    void RestoreMap(Map* map, ChatHandler* handler = nullptr);
    void ApplyToCreature(Creature* creature);
    void RestoreCreature(Creature* creature);

    bool SetMultiplier(Map* map, std::string const& creatureKind, std::string const& statKind, float value, ChatHandler* handler = nullptr);
    void SendStatus(ChatHandler* handler, Map* map) const;
    void Export(ChatHandler* handler, Map* map) const;

    std::vector<RaidBossResetRecipe> const& GetBosses(uint32 mapId) const;
    void SendBossList(ChatHandler* handler, InstanceMap* map) const;
    bool ResetBoss(ChatHandler* handler, InstanceMap* map, uint32 bossDisplayId);
    bool ResetGroupBinds(ChatHandler* handler, Player* player, bool confirm);

private:
    struct InstanceKey
    {
        uint32 mapId = 0;
        uint32 instanceId = 0;

        bool operator==(InstanceKey const& other) const
        {
            return mapId == other.mapId && instanceId == other.instanceId;
        }
    };

    struct InstanceKeyHash
    {
        std::size_t operator()(InstanceKey const& key) const
        {
            return (std::size_t(key.mapId) << 32) ^ key.instanceId;
        }
    };

    struct OriginalCreatureStats
    {
        uint32 createHealth = 0;
        uint32 maxHealth = 0;
        uint32 health = 0;
    };

    InstanceKey MakeKey(Map const* map) const;
    uint32 GetOriginalSize(uint32 mapId) const;
    bool IsScalableCreature(Creature const* creature) const;
    RaidScaleCreatureKind ClassifyCreature(Creature const* creature) const;
    float HealthScaleFor(Creature const* creature, RaidScaleSettings const& settings) const;
    float DamageScaleFor(Creature const* creature, RaidScaleSettings const& settings) const;
    float ClampHealth(float value) const;
    float ClampDamage(float value) const;
    uint32 ScaleHealth(uint32 value, float scale) const;

    uint32 RespawnCreatureEntries(Map* map, std::vector<uint32> const& entries) const;
    uint32 RespawnGameObjectEntries(Map* map, std::vector<uint32> const& entries) const;
    bool IsGroupMemberInMap(Player* leader, uint32 mapId, uint32 instanceId) const;

    bool _enabled = true;
    bool _blizzardLikeDefault = true;
    uint32 _commandSecurity = 2;
    float _damageExponent = 0.6f;
    float _minHealth = 0.05f;
    float _minDamage = 0.05f;
    float _maxHealth = 5.0f;
    float _maxDamage = 5.0f;

    std::unordered_map<uint32, uint32> _originalSizes;
    std::unordered_map<InstanceKey, RaidScaleSettings, InstanceKeyHash> _instances;
    std::unordered_map<ObjectGuid, OriginalCreatureStats> _originalCreatureStats;
};

#define sRaidScalingMgr RaidScalingMgr::Instance()

#endif
