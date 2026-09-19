// API doubles; repository loading, strategy repair, factory solo choices and
// group/master transition bodies are extracted verbatim from production source.
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using uint32 = std::uint32_t;
enum BotState { BOT_STATE_COMBAT, BOT_STATE_NON_COMBAT, BOT_STATE_DEAD };
struct ObjectGuid
{
    using LowType = uint32;
    uint32 value;
    uint32 GetCounter() const { return value; }
};
struct Group { bool battlefield = false; bool isBFGroup() const { return battlefield; } };
struct PlayerbotAI;
struct Player
{
    uint32 guid;
    PlayerbotAI* ai = nullptr;
    Group* group = nullptr;
    bool overworld = true, bg = false, queue = false, wintergrasp = false;
    ObjectGuid GetGUID() const { return {guid}; }
    Group* GetGroup() const { return group; }
    bool InBattleground() const { return bg; }
    bool InBattlegroundQueue() const { return queue; }
};
#define GET_PLAYERBOT_AI(p) ((p)->ai)
struct WorldPosition
{
    Player* player;
    explicit WorldPosition(Player* p) : player(p) {}
    bool isOverworld() const { return player->overworld; }
};
bool IsInWintergraspWar(Player* p) { return p->wintergrasp; }
struct Config
{
    bool enableNewRpgStrategy = true, autoDoQuests = true, randomBotJoinBG = true;
} sPlayerbotAIConfig;
struct Context
{
    std::vector<std::string> saved;
    void GetUntypedValue(char const*) {}
    void Load(std::vector<std::string> const& v) { saved = v; }
};
struct PlayerbotAI
{
    Player* bot;
    Player* master = nullptr;
    Player* nextMaster = nullptr;
    bool real = false;
    Context context;
    std::array<std::set<std::string>, 3> strategies;
    explicit PlayerbotAI(Player* p) : bot(p) { p->ai = this; }
    Player* GetBot() { return bot; }
    Player* GetMaster() { return master; }
    Player* GetGroupLeader() { return master; }
    Player* FindNewMaster() { return nextMaster; }
    void SetMaster(Player* p) { master = p; }
    bool IsRealPlayer() const { return real; }
    Context* GetAiObjectContext() { return &context; }
    void ClearStrategies(BotState state) { strategies[state].clear(); }
    bool HasStrategy(std::string const& name, BotState state) const { return strategies[state].contains(name); }
    void ChangeStrategy(std::string const& names, BotState state)
    {
        std::istringstream input(names);
        for (std::string token; std::getline(input, token, ',');)
        {
            if (token.empty()) continue;
            if (token[0] == '+') strategies[state].insert(token.substr(1));
            else if (token[0] == '-') strategies[state].erase(token.substr(1));
            else assert(false);
        }
    }
    void ResetStrategies();
    void Reset(bool) {}
    void TellMaster(std::string const&) {}
    void UpdateAIGroupMaster();
};
struct EngineDouble
{
    PlayerbotAI* ai;
    void addStrategy(char const* name, bool) { ai->strategies[BOT_STATE_NON_COMBAT].insert(name); }
};
void PlayerbotAI::ResetStrategies()
{
    strategies = {};
    strategies[BOT_STATE_NON_COMBAT] = {"chat", "follow", "quest", "nc"};
    if (!bot->group && !master)
    {
        EngineDouble engine{this};
        auto* nonCombatEngine = &engine;
        // FACTORY_SOLO_CHOICES
    }
}
struct RandomPlayerbotMgr
{
    std::set<uint32> selected{1118, 1159, 1180, 1274, 1297, 1315, 1433};
    bool IsWorldBot(uint32 guid) const { return selected.contains(guid); }
    bool IsRandomBot(Player* p) const { return p && IsWorldBot(p->guid); }
    void RestoreWorldBotSoloStrategies(Player* bot);
} sRandomPlayerbotMgr;
struct PlayerbotTextMgr
{
    static PlayerbotTextMgr& instance() { static PlayerbotTextMgr mgr; return mgr; }
    std::string GetBotTextOrDefault(char const*, char const*, std::initializer_list<int>) { return "hello"; }
};
struct Field
{
    std::string value;
    template<typename T> T Get() const { return value; }
};
using Row = std::array<Field, 2>;
struct QueryRows
{
    std::vector<Row> rows;
    std::size_t index = 0;
    Field* Fetch() { return rows[index].data(); }
    bool NextRow() { return ++index < rows.size(); }
};
using PreparedQueryResult = std::shared_ptr<QueryRows>;
struct PlayerbotsDatabasePreparedStatement
{
    uint32 guid = 0;
    void SetData(int, uint32 value) { guid = value; }
};
constexpr int PLAYERBOTS_SEL_DB_STORE = 0;
struct Database
{
    std::map<uint32, std::vector<Row>> rows;
    PlayerbotsDatabasePreparedStatement stmt;
    PlayerbotsDatabasePreparedStatement* GetPreparedStatement(int) { return &stmt; }
    PreparedQueryResult Query(PlayerbotsDatabasePreparedStatement* s)
    {
        if (!rows.contains(s->guid)) return nullptr;
        auto result = std::make_shared<QueryRows>();
        result->rows = rows.at(s->guid);
        return result;
    }
    // No write APIs: a new DB write in the tested code must fail compilation.
} PlayerbotsDatabase;
struct PlayerbotRepository
{
    static PlayerbotRepository& instance() { static PlayerbotRepository repo; return repo; }
    void Load(PlayerbotAI*);
};
// PRODUCTION_METHODS
void LoginProfile(Player* bot)
{
    PlayerbotAI* botAI = bot->ai;
    botAI->ResetStrategies();
    // LOGIN_LOAD_AND_REPAIR
}
void Profile(uint32 guid, std::string extra = "")
{
    // Ailina's observed saved NC profile, with preference/value preservation checks.
    PlayerbotsDatabase.rows[guid] = {
        Row{Field{"co"}, Field{"+resto,+cc,+threat,+tranquility"}},
        Row{Field{"nc"}, Field{"+aq40,+buff,+chat,+cure,+default,+dps assist,+duel,+emote,+food,+gather,+loot,+mount,+nc,+pvp,+quest,+save mana" + extra}},
        Row{Field{"dead"}, Field{"+chat,+dead,+default,+follow"}},
        Row{Field{"value"}, Field{"formation>chaos"}},
        Row{Field{"value"}, Field{"rti>skull"}},
    };
}
void AssertPreferences(PlayerbotAI const& ai)
{
    assert(ai.HasStrategy("resto", BOT_STATE_COMBAT));
    assert(ai.HasStrategy("cc", BOT_STATE_COMBAT));
    assert(ai.HasStrategy("threat", BOT_STATE_COMBAT));
    assert(ai.HasStrategy("save mana", BOT_STATE_NON_COMBAT));
    assert(ai.HasStrategy("quest", BOT_STATE_NON_COMBAT));
    assert(ai.HasStrategy("dead", BOT_STATE_DEAD));
    assert((ai.context.saved == std::vector<std::string>{"formation>chaos", "rti>skull"}));
}
void AssertNoRoaming(PlayerbotAI const& ai)
{
    for (auto name : {"grind", "new rpg", "rpg", "move random"})
        assert(!ai.HasStrategy(name, BOT_STATE_NON_COMBAT));
}
int main()
{
    // Reproduce the actual bug: repository load clears factory solo defaults.
    Player bot{1180}; PlayerbotAI ai(&bot); Profile(bot.guid);
    ai.ResetStrategies();
    assert(ai.HasStrategy("grind", BOT_STATE_NON_COMBAT));
    assert(ai.HasStrategy("new rpg", BOT_STATE_NON_COMBAT));
    PlayerbotRepository::instance().Load(&ai);
    AssertNoRoaming(ai);
    LoginProfile(&bot);
    assert(ai.HasStrategy("grind", BOT_STATE_NON_COMBAT));
    assert(ai.HasStrategy("new rpg", BOT_STATE_NON_COMBAT));
    AssertPreferences(ai);
    assert(ai.HasStrategy("bg", BOT_STATE_NON_COMBAT));
    sPlayerbotAIConfig.randomBotJoinBG = false;
    LoginProfile(&bot);
    assert(!ai.HasStrategy("bg", BOT_STATE_NON_COMBAT));
    sPlayerbotAIConfig.randomBotJoinBG = true;
    LoginProfile(&bot);
    auto repaired = ai.strategies;
    sRandomPlayerbotMgr.RestoreWorldBotSoloStrategies(&bot);
    assert(ai.strategies == repaired);

    // All opt-ins, no saved profile, and factory's ordinary NC follow default.
    for (auto guid : sRandomPlayerbotMgr.selected)
    {
        Player p{guid}; PlayerbotAI a(&p);
        Profile(guid, ",+follow"); LoginProfile(&p);
        assert(a.HasStrategy("grind", BOT_STATE_NON_COMBAT));
        assert(a.HasStrategy("new rpg", BOT_STATE_NON_COMBAT));
        assert(a.HasStrategy("follow", BOT_STATE_NON_COMBAT));
        PlayerbotsDatabase.rows.erase(guid); LoginProfile(&p);
        assert(a.HasStrategy("new rpg", BOT_STATE_NON_COMBAT));
    }

    // Unselected bots and humans (even artificially opted in) are untouched.
    for (auto guid : {142u, 815u, 1501u, 1180u})
    {
        Player p{guid}; PlayerbotAI a(&p); a.real = guid == 1180;
        Profile(guid, ",+rpg"); a.ResetStrategies();
        PlayerbotRepository::instance().Load(&a);
        auto before = a.strategies;
        sRandomPlayerbotMgr.RestoreWorldBotSoloStrategies(&p);
        assert(a.strategies == before);
    }
    sRandomPlayerbotMgr.RestoreWorldBotSoloStrategies(nullptr);
    Player missingAI{1180}; sRandomPlayerbotMgr.RestoreWorldBotSoloStrategies(&missingAI);

    // Saved solo profiles cannot leak into grouped/mastered/PvP/instance activity.
    for (int gate = 0; gate < 7; ++gate)
    {
        Player p{1180}, owner{1501}; PlayerbotAI a(&p), ownerAI(&owner); Group group;
        if (gate == 0) p.group = &group; // also covers group leaders: no solo roaming in any group
        if (gate == 1 || gate == 2) { a.master = &owner; ownerAI.real = gate == 1; }
        if (gate == 3) p.bg = true;
        if (gate == 4) p.queue = true;
        if (gate == 5) p.overworld = false;
        if (gate == 6) p.wintergrasp = true;
        Profile(p.guid, ",+grind,+new rpg,+rpg,+move random"); LoginProfile(&p);
        AssertNoRoaming(a); AssertPreferences(a);
    }

    // Explicit manual orders survive. Neither state gets wiped or rewritten.
    for (auto state : {BOT_STATE_NON_COMBAT, BOT_STATE_COMBAT})
        for (auto order : {"stay", "passive", "runaway", "move from group", "follow"})
        {
            if (state == BOT_STATE_NON_COMBAT && std::string(order) == "follow") continue;
            Profile(bot.guid, ",+new rpg,+grind");
            PlayerbotRepository::instance().Load(&ai);
            ai.ChangeStrategy(std::string("+") + order, state);
            sRandomPlayerbotMgr.RestoreWorldBotSoloStrategies(&bot);
            AssertNoRoaming(ai); AssertPreferences(ai);
            assert(ai.HasStrategy(order, state));
            ai.ChangeStrategy(std::string("-") + order, state);
        }

    // Respect the existing configured fallback rather than forcing new RPG globally.
    for (int mode = 0; mode < 3; ++mode)
    {
        sPlayerbotAIConfig.enableNewRpgStrategy = mode == 0;
        sPlayerbotAIConfig.autoDoQuests = mode != 2;
        Profile(bot.guid, ",+rpg,+move random,+new rpg"); LoginProfile(&bot);
        assert(ai.HasStrategy("grind", BOT_STATE_NON_COMBAT));
        assert(ai.HasStrategy("new rpg", BOT_STATE_NON_COMBAT) == (mode == 0));
        assert(ai.HasStrategy("rpg", BOT_STATE_NON_COMBAT) == (mode == 1));
        assert(ai.HasStrategy("move random", BOT_STATE_NON_COMBAT) == (mode == 2));
    }
    sPlayerbotAIConfig = {};

    // Execute the unchanged production master-transition method: invitation drops
    // solo choices; leaving restores the ordinary factory choices.
    Player owner{1501}; PlayerbotAI ownerAI(&owner); ownerAI.real = true; Group group;
    bot.group = &group; ai.nextMaster = &owner; ai.UpdateAIGroupMaster();
    assert(ai.master == &owner); AssertNoRoaming(ai);
    bot.group = nullptr; ai.UpdateAIGroupMaster();
    assert(!ai.master && ai.HasStrategy("new rpg", BOT_STATE_NON_COMBAT));
    // No per-tick reassertion that would defeat later manual instructions.
    ai.ChangeStrategy("-grind,-new rpg,+stay", BOT_STATE_NON_COMBAT);
    ai.UpdateAIGroupMaster(); AssertNoRoaming(ai);
    assert(ai.HasStrategy("stay", BOT_STATE_NON_COMBAT));
    std::cout << "saved profile and solo strategy cases passed\n";
}
