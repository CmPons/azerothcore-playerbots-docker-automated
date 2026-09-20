// API doubles; policies, packet hooks, provenance assignments and cancellation are production code.
#include <array>
#include <cassert>
#include <compare>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
enum BotState { BOT_STATE_COMBAT, BOT_STATE_NON_COMBAT };
using BattlegroundTypeId = uint32;
using BattlegroundQueueTypeId = uint32;
enum BattlegroundBracketId : uint8 { BRACKET_6 = 6 };
using TeamId = uint32;
enum GroupJoinBattlegroundResult { ERR_GROUP_JOIN_BATTLEGROUND_FAIL = 0, SUCCESS = 1 };
constexpr uint32 STATUS_NONE = 0, STATUS_WAIT_QUEUE = 1, STATUS_WAIT_JOIN = 2, STATUS_IN_PROGRESS = 3;
constexpr uint32 CMSG_BATTLEFIELD_PORT = 4, BATTLEGROUND_QUEUE_NONE = 0, TEAM_NEUTRAL = 2;
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value = 0;
    uint32 GetCounter() const { return value; }
    auto operator<=>(ObjectGuid const&) const = default;
    static ObjectGuid const Empty;
};
ObjectGuid const ObjectGuid::Empty{};
using GuidSet = std::set<ObjectGuid>;
// QUEUE_INFO
struct Group
{
    bool raid = false, leader = false;
    ObjectGuid guid;
    GuidSet members;
    Group(bool r = false, bool l = false) : raid(r), leader(l), guid{++next} {}
    static inline uint32 next = 5000;
    ObjectGuid GetGUID() const { return guid; }
    bool IsMember(ObjectGuid g) const { return members.contains(g); }
};
struct WorldPacket
{
    std::vector<uint32> fields;
    WorldPacket() = default;
    WorldPacket(uint32, uint32) {}
    template<typename T> WorldPacket& operator<<(T v) { fields.push_back(v); return *this; }
};
struct Session
{
    bool bot = true;
    std::vector<WorldPacket> packets;
    bool IsBot() const { return bot; }
    void QueuePacket(WorldPacket* p) { packets.push_back(*p); delete p; }
};
struct PlayerbotAI;
struct Player
{
    uint32 guid;
    PlayerbotAI* ai = nullptr;
    Group* group = nullptr;
    Group* original = nullptr;
    Group* invitation = nullptr;
    bool overworld = true, wintergrasp = false, inBG = false;
    uint32 bgId = 0, slot = 0;
    std::set<uint32> queues;
    Session session{};
    explicit Player(uint32 id) : guid(id) {}
    ObjectGuid GetGUID() const { return {guid}; }
    Group* GetGroup() const { return group; }
    Group* GetOriginalGroup() const { return original; }
    Group* GetGroupInvite() const { return invitation; }
    bool InBattleground() const { return inBG; }
    uint32 GetBattlegroundId() const { return bgId; }
    uint32 GetBattlegroundQueueIndex(uint32) const { return slot; }
    void RemoveBattlegroundQueueId(uint32 q) { queues.erase(q); }
    void SendDirectMessage(WorldPacket* p) { session.packets.push_back(*p); }
    Session* GetSession() { return &session; }
};
namespace ObjectAccessor
{
    std::map<ObjectGuid, Player*> connected;
    Player* FindConnectedPlayer(ObjectGuid guid)
    {
        auto it = connected.find(guid);
        return it == connected.end() ? nullptr : it->second;
    }
}
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
    bool CanAcceptBattlegroundQueue(Player*, BattlegroundQueueTypeId);
    void CancelCompanionBattlegroundQueue(Player*, BattlegroundQueueTypeId);
} sRandomPlayerbotMgr;
struct BattlegroundQueue
{
    std::map<ObjectGuid, GroupQueueInfo> rows;
    bool GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* info)
    {
        auto it = rows.find(guid);
        if (it == rows.end()) return false;
        *info = it->second;
        return true;
    }
    void RemovePlayer(ObjectGuid guid, bool decrease)
    {
        assert(decrease);
        rows.erase(guid);
        for (auto& [id, info] : rows) info.Players.erase(guid);
    }
};
struct BattlegroundMgr
{
    std::map<uint32, BattlegroundQueue> queues;
    std::vector<std::array<uint32, 5>> updates;
    static uint8 BGArenaType(BattlegroundQueueTypeId type) { return type == 99 ? 2 : 0; }
    static uint32 BGQueueTypeId(uint32 type, uint8 arena) { return arena ? 99 : type; }
    static uint32 BGTemplateId(uint32 type) { return type; }
    BattlegroundQueue& GetBattlegroundQueue(uint32 type) { return queues[type]; }
    void BuildBattlegroundStatusPacket(WorldPacket* packet, void* bg, uint32 slot, uint32 status,
                                      uint32, uint32, uint8, uint32)
    {
        assert(!bg && status == STATUS_NONE);
        packet->fields = {slot, status};
    }
    void ScheduleQueueUpdate(uint32 a, uint32 b, uint32 q, uint32 bg, BattlegroundBracketId bracket)
    {
        updates.push_back({a,b,q,bg,bracket});
    }
} battlegroundMgr;
auto* sBattlegroundMgr = &battlegroundMgr;
struct PlayerbotOperation
{
    virtual ~PlayerbotOperation() = default;
    virtual bool Execute() = 0;
    virtual ObjectGuid GetBotGuid() const = 0;
    virtual std::string GetName() const = 0;
};
struct PlayerbotWorldThreadProcessor
{
    std::vector<std::unique_ptr<PlayerbotOperation>> operations;
    static PlayerbotWorldThreadProcessor& instance() { static PlayerbotWorldThreadProcessor p; return p; }
    void QueueOperation(std::unique_ptr<PlayerbotOperation> operation) { operations.push_back(std::move(operation)); }
    void Drain()
    {
        for (auto& op : operations) op->Execute();
        operations.clear();
    }
};
// CANCEL_OPERATION
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
    [[maybe_unused]] BattlegroundTypeId _bgTypeId = 1;
    // STATUS_GUARD
    return false;
}
void Queue(Player& bot, Player& leader, Group* group = nullptr, uint32 type = 1)
{
    ObjectAccessor::connected[bot.GetGUID()] = &bot;
    auto& queue = battlegroundMgr.queues[type];
    GroupQueueInfo info{};
    auto* ginfo = &info;
    Player* leaderPtr = &leader;
    // QUEUE_PROVENANCE
    info.Players = group ? group->members : GuidSet{bot.GetGUID()};
    info.ArenaType = BattlegroundMgr::BGArenaType(type);
    info.BgTypeId = type;
    info.BracketId = 6;
    for (auto guid : info.Players) queue.rows[guid] = info;
    bot.queues.insert(type);
}
void HumanGroupTests()
{
    // No hard-coded player GUID, and both parties and eligible raid groups work.
    for (uint32 owner : {1501, 9001}) for (bool raid : {false, true})
        for (uint32 guid : {142,815,1118,1159,1180,1274,1297,1315,1433})
        {
            Player bot(guid), human(owner), sibling(2000); PlayerbotAI ai; bot.ai = &ai;
            Group group(raid), bgRaid(true), other;
            bot.group = human.group = sibling.group = &group;
            group.members = {bot.GetGUID(), human.GetGUID(), sibling.GetGUID()};
            human.session.bot = false;
            ObjectAccessor::connected[human.GetGUID()] = &human;
            auto& mgr = sRandomPlayerbotMgr;
            Hooks hooks;
            ai.master = &human;
            ai.strategies[BOT_STATE_NON_COMBAT].insert("follow");
            auto strategies = ai.strategies;
            Queue(bot, human, &group);
            assert(!mgr.CanAutoJoinBattleground(&bot)); // Never grant autonomous submission.
            GroupJoinBattlegroundResult err = SUCCESS;
            assert(hooks.OnPlayerCanJoinInBattlegroundQueue(&human, {}, 1, 1, err));
            assert(!hooks.OnPlayerCanJoinInBattlegroundQueue(&bot, {}, 1, 1, err));
            // This assertion fails on the deployed September 20 policy.
            assert(!StatusGuard(&bot, STATUS_WAIT_QUEUE));
            assert(!StatusGuard(&bot, STATUS_WAIT_JOIN));
            assert(hooks.OnPlayerCanBattleFieldPort(&bot, 0, 1, 1));
            assert(bot.session.packets.empty() && ai.strategies == strategies);
#ifndef LEGACY_POLICY
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 0));
            assert(!mgr.CanAcceptBattlegroundQueue(nullptr, 1));
            // Explicit player consent does not depend on the autonomous-BG enable switch.
            sPlayerbotAIConfig.randomBotJoinBG = false;
            ai.strategies[BOT_STATE_NON_COMBAT].insert("stay");
            assert(mgr.CanAcceptBattlegroundQueue(&bot, 1));
            sPlayerbotAIConfig.randomBotJoinBG = true;
            ai.strategies = strategies;
            auto original = battlegroundMgr.queues[1].rows[bot.GetGUID()];
            auto& info = battlegroundMgr.queues[1].rows[bot.GetGUID()];
            info.QueuedGroupGuid = {};
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1)); // Old solo queue + human party isn't consent.
            info = original; info.QueuedGroupGuid = other.guid;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            info = original; info.QueuedLeaderGuid = sibling.GetGUID();
            ObjectAccessor::connected[sibling.GetGUID()] = &sibling;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1)); // Bot-led group isn't human intent.
            ObjectAccessor::connected.erase(sibling.GetGUID());
            info = original; info.Players.erase(human.GetGUID());
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1)); // Neither queued nor inside invited BG.
            info = original; info.IsRated = true;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            info = original; info.ArenaType = 2;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            info = original;
            bot.invitation = &other;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            bot.invitation = nullptr;
            human.group = &other;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            human.group = &group;
            group.members.erase(human.GetGUID());
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            group.members.insert(human.GetGUID());
            ObjectAccessor::connected.erase(human.GetGUID());
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1));
            ObjectAccessor::connected[human.GetGUID()] = &human;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 2)); // Wrong/missing queue.
            Queue(bot, human, &group, 99);
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 99)); // No arena permission added.
            // Human accepts first, leaving the queued Players set, but keeping the original party.
            info.IsInvitedToBGInstanceGUID = 42;
            battlegroundMgr.queues[1].RemovePlayer(human.GetGUID(), true);
            human.original = &group; human.group = &bgRaid; human.bgId = 42; human.inBG = true;
            assert(mgr.CanAcceptBattlegroundQueue(&bot, 1));
            assert(!StatusGuard(&bot, STATUS_WAIT_QUEUE)); // Force-join recovery uses same consent.
            assert(!StatusGuard(&bot, STATUS_WAIT_JOIN));
            assert(hooks.OnPlayerCanBattleFieldPort(&bot, 0, 1, 1));
            human.bgId = 43;
            assert(!mgr.CanAcceptBattlegroundQueue(&bot, 1)); // Unrelated BG is not authorization.
            human.bgId = 42;
            bot.inBG = true;
            assert(!StatusGuard(&bot, STATUS_WAIT_JOIN)); // Already-accepted bot is not recalled.
            bot.inBG = false;
            // Party changes between action selection and actual port consumption.
            bot.group = &other;
            assert(!hooks.OnPlayerCanBattleFieldPort(&bot, 0, 1, 1));
            assert(hooks.OnPlayerCanBattleFieldPort(&bot, 0, 1, 0));
            // A deferred cancellation must not invalidate a new authorized group queue.
            assert(StatusGuard(&bot, STATUS_WAIT_JOIN));
            bot.group = &group;
            human.group = &group; human.original = nullptr; human.bgId = 0;
            Queue(bot, human, &group);
            PlayerbotWorldThreadProcessor::instance().Drain();
            assert(battlegroundMgr.queues[1].rows.contains(bot.GetGUID()));
            assert(bot.session.packets.empty());
            // Entry or logout before the world-thread operation runs is also safe.
            bot.group = &other;
            assert(StatusGuard(&bot, STATUS_WAIT_JOIN));
            bot.inBG = true;
            PlayerbotWorldThreadProcessor::instance().Drain();
            assert(battlegroundMgr.queues[1].rows.contains(bot.GetGUID()));
            bot.inBG = false;
            assert(StatusGuard(&bot, STATUS_WAIT_JOIN));
            ObjectAccessor::connected.erase(bot.GetGUID());
            PlayerbotWorldThreadProcessor::instance().Drain();
            assert(battlegroundMgr.queues[1].rows.contains(bot.GetGUID()));
            ObjectAccessor::connected[bot.GetGUID()] = &bot;
            // Policy cancellation removes ONLY the bot, not the human/sibling queued with it.
            bot.slot = 1;
            assert(StatusGuard(&bot, STATUS_WAIT_JOIN));
            assert(battlegroundMgr.queues[1].rows.contains(bot.GetGUID())); // Not mutated on the AI/map thread.
            PlayerbotWorldThreadProcessor::instance().Drain();
            assert(!battlegroundMgr.queues[1].rows.contains(bot.GetGUID()));
            assert(battlegroundMgr.queues[1].rows.contains(human.GetGUID()));
            assert(battlegroundMgr.queues[1].rows.contains(sibling.GetGUID()));
            assert(bot.session.packets.back().fields == (std::vector<uint32>{1, STATUS_NONE}));
            assert(!bot.queues.contains(1) && bot.group == &other && ai.strategies == strategies);
            assert(battlegroundMgr.updates.back() == (std::array<uint32,5>{0,0,1,1,6}));
#endif
            ObjectAccessor::connected.clear();
            battlegroundMgr.queues.clear();
        }
}
int main()
{
    HumanGroupTests();
    auto& mgr = sRandomPlayerbotMgr;
    assert(!mgr.CanAutoJoinBattleground(nullptr));
    for (uint32 guid : {142,815,1118,1159,1180,1274,1297,1315,1433})
    {
        Player p(guid); PlayerbotAI ai; p.ai = &ai; Hooks hooks;
        assert(mgr.IsBattlegroundCompanion(guid));
        assert(mgr.CanAutoJoinBattleground(&p));
        assert(!mgr.CanAutoJoinBattleground(&p, true));
        GroupJoinBattlegroundResult err = SUCCESS;
        assert(hooks.OnPlayerCanJoinInBattlegroundQueue(&p, {}, 1, 0, err));
        assert(!hooks.OnPlayerCanJoinInArenaQueue(&p, {}, 0, 1, 0, 0, err));
        assert(err == ERR_GROUP_JOIN_BATTLEGROUND_FAIL);
        assert(hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 1));
        assert(!hooks.OnPlayerCanBattleFieldPort(&p, 2, 1, 1));
        for (bool raid : {false, true}) for (bool leader : {false, true})
        {
            Group group(raid, leader); p.group = &group;
            assert(!mgr.CanAutoJoinBattleground(&p));
            err = SUCCESS;
            assert(!hooks.OnPlayerCanJoinInBattlegroundQueue(&p, {}, 1, leader, err));
            assert(err == ERR_GROUP_JOIN_BATTLEGROUND_FAIL);
            assert(!hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 1));
            assert(hooks.OnPlayerCanBattleFieldPort(&p, 0, 1, 0));
            for (uint32 state : {STATUS_WAIT_QUEUE, STATUS_WAIT_JOIN})
            {
                Queue(p, p); // Solo enrollment, then recruitment into this group.
                auto preferences = ai.strategies;
                auto oldCount = p.session.packets.size();
                assert(StatusGuard(&p, state));
                PlayerbotWorldThreadProcessor::instance().Drain();
                assert(p.session.packets.size() == oldCount + 1);
                assert(p.session.packets.back().fields == (std::vector<uint32>{0, STATUS_NONE}));
                assert(p.group == &group && ai.strategies == preferences);
                assert(ai.context.value.v == 0);
            }
            assert(!StatusGuard(&p, STATUS_IN_PROGRESS));
            p.inBG = true;
            assert(!StatusGuard(&p, STATUS_WAIT_JOIN));
            p.inBG = false; p.group = nullptr;
        }
        Group invite; p.invitation = &invite;
        assert(!mgr.CanAutoJoinBattleground(&p));
        Queue(p, p);
        assert(StatusGuard(&p, STATUS_WAIT_JOIN));
        PlayerbotWorldThreadProcessor::instance().Drain();
        p.invitation = nullptr;
        Player owner(1501); ai.master = &owner;
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
        Queue(p, p, nullptr, 99);
        assert(StatusGuard(&p, STATUS_WAIT_JOIN, 99));
        PlayerbotWorldThreadProcessor::instance().Drain();
        mgr.protectedFriends.insert(guid);
        assert(mgr.CanAutoJoinBattleground(&p));
    }
    Player filler(2000), human(1501), possessed(1180);
    PlayerbotAI fillerAI, humanAI; humanAI.real = true;
    filler.ai = &fillerAI; human.ai = &humanAI; possessed.ai = &humanAI;
    Group raid(true,true); filler.group = human.group = possessed.group = &raid;
    for (auto* p : {&filler, &human, &possessed})
    {
        assert(mgr.CanAutoJoinBattleground(p, true));
        assert(!StatusGuard(p, STATUS_WAIT_JOIN));
    }
    possessed.ai = nullptr; assert(mgr.CanAutoJoinBattleground(&possessed));
    std::cout << "human-led BG queues, entry ordering, recruitment races, bot-only cancellation and solo policy passed\n";
}
