// API doubles; policy, packet-boundary hooks and queue cancellation are production code.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
enum BotState { BOT_STATE_COMBAT, BOT_STATE_NON_COMBAT };
using BattlegroundTypeId = uint32;
using BattlegroundQueueTypeId = uint32;
enum GroupJoinBattlegroundResult { ERR_GROUP_JOIN_BATTLEGROUND_FAIL = 0, SUCCESS = 1 };
constexpr uint32 STATUS_WAIT_QUEUE = 1, STATUS_WAIT_JOIN = 2, STATUS_IN_PROGRESS = 3;
constexpr uint32 CMSG_BATTLEFIELD_PORT = 4;
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value = 0;
    uint32 GetCounter() const { return value; }
};
struct Group { bool raid = false, leader = false; };
struct WorldPacket
{
    std::vector<uint32> fields;
    WorldPacket(uint32, uint32) {}
    template<typename T> WorldPacket& operator<<(T v) { fields.push_back(v); return *this; }
};
struct Session
{
    std::vector<WorldPacket> packets;
    void QueuePacket(WorldPacket* p) { packets.push_back(*p); delete p; }
};
struct PlayerbotAI;
struct Player
{
    uint32 guid;
    PlayerbotAI* ai = nullptr;
    Group* group = nullptr;
    Group* invitation = nullptr;
    bool overworld = true, wintergrasp = false, inBG = false;
    Session session{};
    ObjectGuid GetGUID() const { return {guid}; }
    Group* GetGroup() const { return group; }
    Group* GetGroupInvite() const { return invitation; }
    bool InBattleground() const { return inBG; }
    Session* GetSession() { return &session; }
};
struct Context
{
    struct Value { uint32 v = 99; void Set(uint32 n) { v = n; } } value;
    template<typename T> Value* GetValue(char const*) { return &value; }
};
struct PlayerbotAI
{
    Player* master = nullptr;
    bool real = false;
    std::array<std::set<std::string>, 2> strategies;
    Context context;
    bool IsRealPlayer() const { return real; }
    Player* GetMaster() const { return master; }
    bool HasStrategy(std::string const& name, BotState state) const { return strategies[state].contains(name); }
    Context* GetAiObjectContext() { return &context; }
    // No reset API: cancellation must not replace the player's current AI settings.
};
#define GET_PLAYERBOT_AI(p) ((p)->ai)
struct WorldPosition
{
    Player* p;
    explicit WorldPosition(Player* value) : p(value) {}
    bool isOverworld() const { return p->overworld; }
};
bool IsInWintergraspWar(Player* p) { return p->wintergrasp; }
struct Config { bool randomBotJoinBG = true; } sPlayerbotAIConfig;
struct RandomPlayerbotMgr
{
    std::set<uint32> selected{1118,1159,1180,1274,1297,1315,1433};
    std::set<uint32> protectedFriends{142,815,1118,1159,1180};
    bool IsWorldBot(uint32 guid) const { return selected.contains(guid); }
    bool IsPersistentCompanion(uint32 guid) const { return protectedFriends.contains(guid); }
    bool IsBattlegroundCompanion(uint32 guid);
    bool CanAutoJoinBattleground(Player*, bool arena = false);
} sRandomPlayerbotMgr;
struct BattlegroundMgr
{
    static uint8 BGArenaType(BattlegroundQueueTypeId type) { return type == 99 ? 2 : 0; }
};
// POLICY_METHODS
struct PlayerScript
{
    virtual bool OnPlayerCanJoinInBattlegroundQueue(Player*, ObjectGuid, BattlegroundTypeId, uint8,
                                                   GroupJoinBattlegroundResult&) = 0;
    virtual bool OnPlayerCanJoinInArenaQueue(Player*, ObjectGuid, uint8, BattlegroundTypeId, uint8, uint8,
                                           GroupJoinBattlegroundResult&) = 0;
    virtual bool OnPlayerCanBattleFieldPort(Player*, uint8, BattlegroundTypeId, uint8) = 0;
};
struct Hooks : PlayerScript
{
    // PACKET_HOOKS
};
bool StatusGuard(Player* bot, uint32 statusid, BattlegroundQueueTypeId queueTypeId = 1)
{
    auto* botAI = bot->ai;
    BattlegroundTypeId _bgTypeId = 1;
    // STATUS_GUARD
    return false;
}
int main()
{
    auto& mgr = sRandomPlayerbotMgr;
    assert(!mgr.CanAutoJoinBattleground(nullptr));
    for (uint32 guid : {142,815,1118,1159,1180,1274,1297,1315,1433})
    {
        Player p{guid}; PlayerbotAI ai; p.ai = &ai; Hooks hooks;
        assert(mgr.IsBattlegroundCompanion(guid));
        assert(mgr.CanAutoJoinBattleground(&p));
        assert(!mgr.CanAutoJoinBattleground(&p, true)); // BGs, not arena team recruitment
        GroupJoinBattlegroundResult err = SUCCESS;
        assert(hooks.OnPlayerCanJoinInBattlegroundQueue(&p, {}, 1, 0, err));
        assert(!hooks.OnPlayerCanJoinInArenaQueue(&p, {}, 0, 1, 0, 0, err));
        assert(err == ERR_GROUP_JOIN_BATTLEGROUND_FAIL);
        assert(hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 1));
        assert(!hooks.OnPlayerCanBattleFieldPort(&p, 2, 1, 1));
        for (bool raid : {false, true}) for (bool leader : {false, true})
        {
            Group group{raid, leader}; p.group = &group;
            assert(!mgr.CanAutoJoinBattleground(&p));
            err = SUCCESS;
            assert(!hooks.OnPlayerCanJoinInBattlegroundQueue(&p, {}, 1, leader, err));
            assert(err == ERR_GROUP_JOIN_BATTLEGROUND_FAIL);
            // Recheck at final packet consumption, even if AI selected/queued while solo.
            assert(!hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 1));
            assert(hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 0)); // leaving is always allowed
            for (uint32 state : {STATUS_WAIT_QUEUE, STATUS_WAIT_JOIN})
            {
                auto preferences = ai.strategies;
                auto oldCount = p.session.packets.size();
                assert(StatusGuard(&p, state));
                assert(p.session.packets.size() == oldCount + 1);
                assert((p.session.packets.back().fields == std::vector<uint32>{0,0,1,0x1F90,0}));
                assert(p.group == &group && ai.strategies == preferences);
                assert(ai.context.value.v == 0);
            }
            assert(!StatusGuard(&p, STATUS_IN_PROGRESS));
            p.inBG = true;
            assert(!StatusGuard(&p, STATUS_WAIT_JOIN)); // do not kick an active BG's auto-raid
            p.inBG = false; p.group = nullptr;
        }
        Group invite; p.invitation = &invite;
        assert(!mgr.CanAutoJoinBattleground(&p));
        assert(StatusGuard(&p, STATUS_WAIT_JOIN));
        p.invitation = nullptr;
        Player owner{1501}; ai.master = &owner;
        assert(!mgr.CanAutoJoinBattleground(&p));
        ai.master = nullptr;
        for (auto state : {BOT_STATE_COMBAT, BOT_STATE_NON_COMBAT})
            for (auto order : {"stay","passive","runaway","move from group","follow"})
            {
                ai.strategies[state].insert(order);
                assert(mgr.CanAutoJoinBattleground(&p) ==
                       (state == BOT_STATE_NON_COMBAT && std::string(order) == "follow"));
                ai.strategies[state].erase(order);
            }
        p.overworld = false; assert(!mgr.CanAutoJoinBattleground(&p)); p.overworld = true;
        p.wintergrasp = true; assert(!mgr.CanAutoJoinBattleground(&p)); p.wintergrasp = false;
        sPlayerbotAIConfig.randomBotJoinBG = false;
        assert(!mgr.CanAutoJoinBattleground(&p));
        sPlayerbotAIConfig.randomBotJoinBG = true;
        assert(!StatusGuard(&p, STATUS_WAIT_JOIN));
        assert(StatusGuard(&p, STATUS_WAIT_JOIN, 99)); // decline arena invitation
        // Pending cap 9 must not reintroduce the old blanket BG exclusion.
        mgr.protectedFriends.insert(guid);
        assert(mgr.CanAutoJoinBattleground(&p));
    }
    // Ordinary filler groups/arenas and human control keep their existing eligibility.
    Player filler{2000}, human{1501}, possessed{1180};
    PlayerbotAI fillerAI, humanAI; humanAI.real = true;
    filler.ai = &fillerAI; human.ai = &humanAI; possessed.ai = &humanAI;
    Group raid{true,true}; filler.group = human.group = possessed.group = &raid;
    for (auto* p : {&filler, &human, &possessed})
    {
        assert(mgr.CanAutoJoinBattleground(p, true));
        assert(!StatusGuard(p, STATUS_WAIT_JOIN));
    }
    possessed.ai = nullptr; assert(mgr.CanAutoJoinBattleground(&possessed));
    std::cout << "solo BG policy, party races, packet hooks and queue cancellation passed\n";
}
