// Minimal live-state doubles for production social collection and ambient prompt output.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
using uint8 = uint8_t;
using uint32 = uint32_t;
constexpr uint8 CLASS_WARRIOR = 1, CLASS_PALADIN = 2, CLASS_HUNTER = 3, CLASS_ROGUE = 4,
    CLASS_PRIEST = 5, CLASS_DEATH_KNIGHT = 6, CLASS_SHAMAN = 7, CLASS_MAGE = 8,
    CLASS_WARLOCK = 9, CLASS_DRUID = 11;
constexpr uint8 RACE_HUMAN = 1, RACE_ORC = 2, RACE_DWARF = 3, RACE_NIGHTELF = 4,
    RACE_UNDEAD_PLAYER = 5, RACE_TAUREN = 6, RACE_GNOME = 7, RACE_TROLL = 8,
    RACE_BLOODELF = 10, RACE_DRAENEI = 11;
constexpr int POWER_MANA = 0, TEAM_ALLIANCE = 0, FLIGHT_MOTION_TYPE = 1, MAX_QUEST_LOG_SIZE = 25;
struct ObjectGuid
{
    uint32 value = 0;
    bool operator==(ObjectGuid const&) const = default;
};
struct PlayerSocial
{
    std::set<uint32> friends;
    bool HasFriend(ObjectGuid guid) const { return friends.contains(guid.value); }
};
struct Unit
{
    std::string name = "Test";
    bool alive = true;
    std::string const& GetName() const { return name; }
    bool IsAlive() const { return alive; }
    float GetHealthPct() const { return 100; }
};
struct MotionMaster
{
    int GetCurrentMovementGeneratorType() const { return 0; }
};
struct Taxi
{
    uint32 GetTaxiSource() const { return 0; }
    uint32 GetTaxiDestination() const { return 0; }
    std::vector<uint32> GetPath() const { return {}; }
};
class Group;
class Player : public Unit
{
public:
    ObjectGuid guid{1};
    uint32 guild = 0;
    PlayerSocial* social = nullptr;
    Group* group = nullptr;
    bool real = false;
    Unit* victim = nullptr;
    Taxi m_taxi;
    MotionMaster motion;
    ObjectGuid GetGUID() const { return guid; }
    uint32 GetGuildId() const { return guild; }
    PlayerSocial* GetSocial() { return social; }
    Group* GetGroup() const { return group; }
    uint8 GetLevel() const { return 70; }
    uint8 getClass() const { return CLASS_MAGE; }
    uint8 getRace() const { return RACE_HUMAN; }
    uint32 GetTeamId() const { return TEAM_ALLIANCE; }
    bool HasTankSpec() const { return false; }
    bool HasHealSpec() const { return false; }
    bool IsInCombat() const { return false; }
    bool IsInFlight() const { return false; }
    bool IsMounted() const { return false; }
    uint32 GetMaxPower(int) const { return 100; }
    float GetPowerPct(int) const { return 100; }
    Unit* GetVictim() const { return victim; }
    Unit* GetSelectedUnit() const { return nullptr; }
    std::vector<Unit*> getAttackers() const { return {}; }
    uint32 GetMapId() const { return 1; }
    uint32 GetZoneId() const { return 1; }
    uint32 GetAreaId() const { return 1; }
    uint32 GetQuestSlotQuestId(uint8) const { return 0; }
    MotionMaster* GetMotionMaster() { return &motion; }
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
    GroupReference* first = nullptr;
    GroupReference* GetFirstMember() const { return first; }
    bool isRaidGroup() const { return true; }
};
struct GuildMgr
{
    std::map<uint32, std::string> names;
    std::string GetGuildNameById(uint32 id) const
    {
        auto it = names.find(id);
        return it == names.end() ? "" : it->second;
    }
};
inline GuildMgr guildMgr;
inline GuildMgr* sGuildMgr = &guildMgr;
namespace ObjectAccessor
{
    inline std::map<std::string, Player*> players;
    inline Player* FindPlayerByName(std::string const& name)
    {
        auto it = players.find(name);
        return it == players.end() ? nullptr : it->second;
    }
}
struct Quest
{
    int GetZoneOrSort() const { return 0; }
    std::string GetTitle() const { return ""; }
};
struct QuestPOI { uint32 AreaId, MapId; };
using QuestPOIVector = std::vector<QuestPOI>;
struct ObjectMgr
{
    Quest const* GetQuestTemplate(uint32) const { return nullptr; }
    QuestPOIVector const* GetQuestPOIVector(uint32) const { return nullptr; }
};
inline ObjectMgr objectMgr;
inline ObjectMgr* sObjectMgr = &objectMgr;
struct World { int GetDefaultDbcLocale() const { return 0; } };
inline World world;
inline World* sWorld = &world;
struct AreaTableEntry { char const* area_name[1] = {"Test area"}; };
struct MapEntry
{
    char const* name[1] = {"Test map"};
    bool IsDungeon() const { return false; }
    bool IsRaid() const { return false; }
};
struct TaxiNodesEntry { char const* name[1] = {"Test taxi"}; };
template<class T> struct Store
{
    T entry;
    T const* LookupEntry(uint32) const { return &entry; }
};
inline Store<AreaTableEntry> sAreaTableStore;
inline Store<MapEntry> sMapStore;
inline Store<TaxiNodesEntry> sTaxiNodesStore;
inline uint32 urand(uint32 low, uint32) { return low; }
