#ifndef MOD_RAID_SCALING_STATE_H
#define MOD_RAID_SCALING_STATE_H

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

struct RaidScaleSettings
{
    std::uint32_t targetPlayers = 0;
    std::uint32_t originalPlayers = 0;
    float bossHealth = 1.0f;
    float bossDamage = 1.0f;
    float trashHealth = 1.0f;
    float trashDamage = 1.0f;
    bool fromDefault = false;
};

inline std::uint64_t RaidScaleKey(std::uint32_t mapId, std::uint32_t instanceId)
{
    return (std::uint64_t(mapId) << 32) | instanceId;
}

// Different maps can update on different threads. Return value snapshots, never
// pointers into a registry which another map creation/destruction can mutate.
class RaidScalingState
{
public:
    bool Initialize(std::uint64_t key, RaidScaleSettings const& settings)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _instances.emplace(key, settings).second;
    }

    void Set(std::uint64_t key, RaidScaleSettings const& settings)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _instances[key] = settings;
    }

    void Disable(std::uint64_t key)
    {
        // Keep an explicit off entry so initialization cannot undo the override.
        Set(key, RaidScaleSettings{});
    }

    std::optional<RaidScaleSettings> Get(std::uint64_t key) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto itr = _instances.find(key);
        if (itr == _instances.end() || !itr->second.targetPlayers)
            return std::nullopt;
        return itr->second;
    }

    template<class Update>
    bool Modify(std::uint64_t key, Update update)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto itr = _instances.find(key);
        if (itr == _instances.end() || !itr->second.targetPlayers)
            return false;
        update(itr->second);
        return true;
    }

    void Erase(std::uint64_t key)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _instances.erase(key);
    }

private:
    mutable std::mutex _mutex;
    std::unordered_map<std::uint64_t, RaidScaleSettings> _instances;
};

#endif
