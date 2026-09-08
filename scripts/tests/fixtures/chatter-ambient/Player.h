// Minimal game API doubles for compiling the production ambient director.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
using uint32 = uint32_t;
enum class HighGuid { Player };
struct ObjectGuid
{
    using LowType = uint32_t;
    uint64_t value = 0;
    uint64_t GetCounter() const { return value; }
    uint64_t GetRawValue() const { return value; }
    template<HighGuid> static ObjectGuid Create(LowType value) { return {value}; }
};
class PlayerbotAI
{
public:
    bool real = false;
    bool IsRealPlayer() const { return real; }
};
class Group;
class Player
{
public:
    ObjectGuid guid;
    bool online = true;
    bool alive = true;
    Group* group = nullptr;
    PlayerbotAI* ai = nullptr;
    uint32_t zone = 10;
    uint32_t guild = 20;
    ObjectGuid GetGUID() const { return guid; }
    bool IsInWorld() const { return online; }
    bool IsAlive() const { return alive; }
    Group* GetGroup() const { return group; }
    uint32_t GetZoneId() const { return zone; }
    uint32_t GetGuildId() const { return guild; }
};
struct GroupReference
{
    Player* player;
    GroupReference* following;
    Player* GetSource() const { return player; }
    GroupReference* next() const { return following; }
};
class Group
{
public:
    ObjectGuid guid{99};
    bool raid = true;
    bool bg = false;
    bool bf = false;
    GroupReference* first = nullptr;
    bool isRaidGroup() const { return raid; }
    bool isBGGroup() const { return bg; }
    bool isBFGroup() const { return bf; }
    ObjectGuid GetGUID() const { return guid; }
    GroupReference* GetFirstMember() const { return first; }
};
namespace ObjectAccessor
{
    inline std::unordered_map<uint64_t, Player*> players;
    inline auto const& GetPlayers() { return players; }
    inline Player* FindPlayer(ObjectGuid guid)
    {
        auto it = players.find(guid.value);
        return it == players.end() ? nullptr : it->second;
    }
}
#define GET_PLAYERBOT_AI(player) ((player)->ai)
inline uint32_t testRoll = 0;
inline uint32_t urand(uint32_t low, uint32_t high)
{
    return low == 0 && high == 99 ? testRoll : low;
}
