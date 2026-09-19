// API doubles only: tested method bodies are extracted verbatim from native source.
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
using uint32 = std::uint32_t;
constexpr int CLASS_DEATH_KNIGHT = 6;
constexpr int BOT_STATE_NON_COMBAT = 0;
enum class HighGuid { Player };
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value;
    template<HighGuid> static ObjectGuid Create(uint32 v) { return {v}; }
    uint32 GetCounter() const { return value; }
    auto operator<=>(ObjectGuid const&) const = default;
};
struct CharacterCacheEntry { int Class = 1; uint32 account = 20; };
struct CharacterCache
{
    std::map<uint32, CharacterCacheEntry> entries;
    uint32 GetCharacterAccountIdByGuid(ObjectGuid g) { return entries.count(g.value) ? entries[g.value].account : 0; }
    CharacterCacheEntry const* GetCharacterCacheByGuid(ObjectGuid g)
    { return entries.count(g.value) ? &entries[g.value] : nullptr; }
} cache;
auto* sCharacterCache = &cache;
struct Config
{
    std::string guids;
    template<typename T> T GetOption(char const*, char const*) { return guids; }
} config;
auto* sConfigMgr = &config;
struct AIConfig
{
    bool enabled = true, randomBotAutologin = true, disableDeathKnightLogin = false;
    bool enablePeriodicOnlineOffline = false;
    uint32 minRandomBotInWorldTime = 10, maxRandomBotInWorldTime = 20, permanentlyInWorldTime = 1000;
    uint32 minRandomBotReviveTime = 2, maxRandomBotReviveTime = 5;
    uint32 minRandomBotRandomizeTime = 10, maxRandomBotRandomizeTime = 50;
    uint32 minRandomBotTeleportInterval = 15, maxRandomBotTeleportInterval = 60;
    uint32 randomBotsPerInterval = 50;
    bool IsInRandomAccountList(uint32 a) { return a == 10 || a == 20; }
} sPlayerbotAIConfig;
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
uint32 urand(uint32 a, uint32) { return a; }
enum class TravelState { TRAVEL_STATE_IDLE, TRAVEL_STATE_TRAVEL };
struct TravelTarget
{
    TravelState state = TravelState::TRAVEL_STATE_IDLE;
    TravelState getTravelState() { return state; }
};
struct Context
{
    TravelTarget target;
    struct Value { TravelTarget* ptr; TravelTarget* Get() { return ptr; } } value{&target};
    template<typename T> Value* GetValue(char const*) { return &value; }
};
struct Player;
struct AI
{
    Player* bot = nullptr;
    Player* master = nullptr;
    Player* nextMaster = nullptr;
    int resets = 0;
    void UpdateAIGroupMaster();
    void SetMaster(Player* p) { master = p; }
    Player* GetMaster() { return master; }
    Player* GetGroupLeader() { return master; }
    Player* FindNewMaster() { return nextMaster; }
    bool IsRealPlayer() { return false; }
    void Reset(bool) { ++resets; }
    void ResetStrategies() { ++resets; }
    void ChangeStrategy(char const*, int) {}
    void TellMaster(std::string const&) {}
    Context context;
    Context* GetAiObjectContext() { return &context; }
    void LeaveOrDisbandGroup() {}
};
using PlayerbotAI = AI;
struct Group
{
    bool battlefield = false;
    bool isBFGroup() { return battlefield; }
    bool isLFGGroup() { return true; }
    uint32 GetLeader() { return 0; }
};
struct Player
{
    uint32 guid;
    AI ai;
    bool bg = false, queue = false, dead = false;
    Group* group = nullptr;
    bool InBattleground() { return bg; }
    bool InBattlegroundQueue() { return queue; }
    bool isDead() { return dead; }
    Group* GetGroup() { return group; }
    ObjectGuid GetGUID() { return {guid}; }
    std::string GetName() { return "fixture"; }
    int GetTeamId() { return 0; }
    int GetLevel() { return 12; }
};
constexpr int TEAM_ALLIANCE = 0;
#define GET_PLAYERBOT_AI(p) (&(p)->ai)
bool IsInWintergraspWar(Player*) { return false; }
namespace ObjectAccessor
{
    std::map<ObjectGuid, Player*> connected;
    Player* FindConnectedPlayer(ObjectGuid g) { return connected.count(g) ? connected[g] : nullptr; }
}
struct Field
{
    uint32 value;
    template<typename T> T Get() { return value; }
};
struct QueryRows
{
    std::vector<Field> rows;
    std::size_t index = 0;
    Field* Fetch() { return &rows[index]; }
    bool NextRow() { return ++index < rows.size(); }
};
using PreparedQueryResult = std::shared_ptr<QueryRows>;
struct PlayerbotsDatabasePreparedStatement
{
    template<typename T> void SetData(int, T) {}
};
constexpr int PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_EVENT = 0;
struct Database
{
    PlayerbotsDatabasePreparedStatement statement;
    PreparedQueryResult result;
    PlayerbotsDatabasePreparedStatement* GetPreparedStatement(int) { return &statement; }
    PreparedQueryResult Query(PlayerbotsDatabasePreparedStatement*) { return result; }
} PlayerbotsDatabase;
using PlayerBotMap = std::map<ObjectGuid, Player*>;
class RandomPlayerbotMgr
{
public:
    std::unordered_set<uint32> worldBotGuids;
    bool worldBotGuidsLoaded = false;
    std::vector<uint32> addClassTypeAccounts{20};
    std::list<uint32> currentBots;
    std::map<ObjectGuid, uint32> botLoading;
    PlayerBotMap owned;
    std::vector<Player*> players;
    PlayerBotMap::const_iterator GetPlayerBotsBegin() { return owned.begin(); }
    PlayerBotMap::const_iterator GetPlayerBotsEnd() { return owned.end(); }
    void DisablePlayerBot(ObjectGuid) {}
    void OnPlayerLogout(Player*);
    std::map<std::pair<uint32, std::string>, uint32> events;
    uint32 lastLifetime = 0;
    int randomized = 0, refreshed = 0, teleported = 0, revived = 0;
    bool protectedFriend = false;
    void LoadWorldBotGuids();
    void GetBots();
    bool IsWorldBot(uint32) const;
    bool AddWorldBot(uint32);
    bool IsRandomBot(uint32);
    bool IsRandomBot(Player* p) { return p && IsRandomBot(p->guid); }
    uint32 GetMaxAllowedBotCount();
    bool ProcessBot(Player*);
    Player* GetPlayerBot(ObjectGuid g) { return owned.count(g) ? owned[g] : nullptr; }
    uint32 GetEventValue(uint32 g, std::string e) { return events[{g, e}]; }
    void SetEventValue(uint32 g, std::string e, uint32 v, uint32 lifetime)
    { events[{g, e}] = v; if (e == "add") lastLifetime = lifetime; }
    bool IsPersistentCompanion(Player*) { return protectedFriend; }
    void ScheduleRandomize(uint32 g, uint32 t) { SetEventValue(g, "randomize", 1, t); }
    void ScheduleTeleport(uint32 g, uint32 t) { SetEventValue(g, "teleport", 1, t); }
    void Randomize(Player*) { ++randomized; }
    void Refresh(Player*) { ++refreshed; }
    void RandomTeleportForLevel(Player*) { ++teleported; }
    void Revive(Player*) { ++revived; }
    uint32 RemainingOrdinaryBudget(uint32 maxAllowedBotCount)
    {
        // POPULATION_BUDGET
        return maxAllowedBotCount;
    }
};
// PRODUCTION_METHODS
RandomPlayerbotMgr sRandomPlayerbotMgr;
struct PlayerbotsMgr
{
    static PlayerbotsMgr& instance() { static PlayerbotsMgr mgr; return mgr; }
    PlayerbotAI* GetPlayerbotAI(Player* p) { return &p->ai; }
};
struct PlayerbotTextMgr
{
    static PlayerbotTextMgr& instance() { static PlayerbotTextMgr mgr; return mgr; }
    std::string GetBotTextOrDefault(char const*, char const*, std::initializer_list<int>) { return "hello"; }
};
// GROUP_TRANSITIONS
uint32 routedAccount = 999;
void AuthorizedLogin(ObjectGuid playerGuid, uint32 masterAccountId)
{
    // AUTHORIZED_ROUTING
    routedAccount = masterAccountId;
}
int main()
{
    for (uint32 g : {1118, 1159, 1180, 1274, 1297, 1315, 1433, 1500})
        cache.entries[g] = {};
    cache.entries[142].account = 10; // ordinary world bot; cannot opt in
    cache.entries[815].account = 10;
    cache.entries[1501].account = 1501; // human
    cache.entries[1600].account = 30; // type2 but not a random account
    config.guids = "";
    RandomPlayerbotMgr empty;
    empty.LoadWorldBotGuids();
    assert(empty.worldBotGuids.empty() && !empty.AddWorldBot(1118));
    // Default-empty must still restore any valid historical add event, including type2.
    PlayerbotsDatabase.result = std::make_shared<QueryRows>();
    PlayerbotsDatabase.result->rows = {{1500}, {142}, {815}};
    empty.events = {{{0, "bot_count"}, 2}, {{1500, "add"}, 1}, {{142, "add"}, 1}, {{815, "add"}, 1}};
    empty.GetBots();
    assert((empty.currentBots == std::list<uint32>{1500, 142}));
    assert(empty.IsRandomBot(1500)); // pre-existing behavior, not a new config opt-in
    empty.GetBots();
    assert(empty.currentBots.size() == 2); // already-populated pool is not reread
    config.guids = "1118,1159,1180,1274,1297,1315,1433";
    sPlayerbotAIConfig.enabled = false;
    RandomPlayerbotMgr disabled;
    disabled.LoadWorldBotGuids();
    assert(disabled.worldBotGuids.empty());
    sPlayerbotAIConfig.enabled = true;
    sPlayerbotAIConfig.randomBotAutologin = false;
    RandomPlayerbotMgr noAutologin;
    noAutologin.LoadWorldBotGuids();
    assert(noAutologin.worldBotGuids.empty());
    sPlayerbotAIConfig.randomBotAutologin = true;
    config.guids += ", 1118 ,0,-1,+1,1118oops,4294967296,9999,1501,142,815,1600, ,";
    auto& mgr = sRandomPlayerbotMgr;
    mgr.addClassTypeAccounts.push_back(30);
    mgr.LoadWorldBotGuids();
    assert(mgr.worldBotGuids.size() == 7);
    assert(!mgr.IsWorldBot(142) && !mgr.IsWorldBot(815) && !mgr.IsWorldBot(1500));
    config.guids = "";
    mgr.LoadWorldBotGuids(); // no hot removal
    assert(mgr.worldBotGuids.size() == 7);
    RandomPlayerbotMgr removed;
    removed.LoadWorldBotGuids(); // next startup removes opt-in
    assert(removed.worldBotGuids.empty());

    Player bot{1118, {}};
    assert(!mgr.IsRandomBot(1118));
    ObjectAccessor::connected[{1118}] = &bot; // personal holder: do not steal
    assert(!mgr.AddWorldBot(1118) && !mgr.IsRandomBot(1118));
    ObjectAccessor::connected.clear();
    mgr.botLoading[{1118}] = 1501;
    assert(!mgr.AddWorldBot(1118));
    mgr.botLoading.clear();
    mgr.events[{1118, "logout"}] = 1;
    assert(!mgr.AddWorldBot(1118));
    mgr.events.clear();
    cache.entries[1118].Class = CLASS_DEATH_KNIGHT;
    sPlayerbotAIConfig.disableDeathKnightLogin = true;
    assert(!mgr.AddWorldBot(1118));
    sPlayerbotAIConfig.disableDeathKnightLogin = false;
    assert(mgr.AddWorldBot(1118) && mgr.lastLifetime == 1000);
    assert(mgr.AddWorldBot(1118) && mgr.currentBots.size() == 1 && mgr.IsRandomBot(1118));
    assert(!mgr.AddWorldBot(1500) && !mgr.IsRandomBot(1500));
    sPlayerbotAIConfig.enablePeriodicOnlineOffline = true;
    assert(mgr.AddWorldBot(1159) && mgr.lastLifetime == 10);
    sPlayerbotAIConfig.enablePeriodicOnlineOffline = false;
    mgr.events[{0, "bot_count"}] = 2000;
    for (uint32 g = 5000; g < 7000; ++g) mgr.currentBots.push_back(g);
    assert(mgr.GetMaxAllowedBotCount() == 2007);
    assert(mgr.RemainingOrdinaryBudget(2000) == 0); // full ordinary population
    for (uint32 g : mgr.worldBotGuids) assert(mgr.AddWorldBot(g));
    assert(mgr.currentBots.size() == 2007 && mgr.RemainingOrdinaryBudget(2000) == 0);
    mgr.currentBots.remove(5000);
    assert(mgr.RemainingOrdinaryBudget(2000) == 1); // selected slots never displace normal bots
    AuthorizedLogin({1118}, 1501);
    assert(routedAccount == 0);
    AuthorizedLogin({1500}, 1501);
    assert(routedAccount == 1501);

    mgr.events.clear();
    assert(!mgr.ProcessBot(&bot) && mgr.randomized == 0); // expired randomize is deferred
    assert(mgr.GetEventValue(1118, "randomize") == 1);
    mgr.ProcessBot(&bot); // ordinary refresh and teleport remain enabled
    assert(mgr.refreshed == 1 && mgr.teleported == 1);
    bot.dead = true;
    mgr.ProcessBot(&bot); // existing death mark then expiry
    mgr.events[{1118, "revive"}] = 0;
    mgr.ProcessBot(&bot);
    assert(mgr.revived == 1);
    Player human{1501, {}};
    bot.dead = false;
    bot.ai.bot = &bot;
    bot.ai.master = &human;
    bot.ai.UpdateAIGroupMaster();
    assert(!bot.ai.master && bot.ai.resets == 2); // leave group resumes existing random AI
    Player personal{1500, {}};
    personal.ai.bot = &personal;
    personal.ai.master = &human;
    personal.ai.UpdateAIGroupMaster();
    assert(personal.ai.master == &human && personal.ai.resets == 0);
    Group party;
    bot.group = &party;
    bot.ai.nextMaster = &human;
    bot.ai.UpdateAIGroupMaster();
    assert(bot.ai.master == &human); // grouped acquisition is unchanged
    mgr.owned[{1118}] = &bot;
    mgr.players.push_back(&human);
    mgr.OnPlayerLogout(&human);
    assert(!bot.ai.master && mgr.owned.size() == 1 && mgr.players.empty());
    party.battlefield = true;
    bot.ai.UpdateAIGroupMaster();
    assert(!bot.ai.master); // battlefield raids do not grant ownership
    Player normal{815, {}};
    assert(mgr.ProcessBot(&normal) && mgr.randomized == 1);
    mgr.protectedFriend = true;
    normal.guid = 142;
    assert(!mgr.ProcessBot(&normal) && mgr.randomized == 1);
    std::cout << "all lifecycle cases passed\n";
}
