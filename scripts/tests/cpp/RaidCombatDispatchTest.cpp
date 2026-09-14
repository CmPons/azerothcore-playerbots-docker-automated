/* Exact generic Update body, real Scope/VM/mailbox; native adapters are tested separately. */
#include "CthunPolicyScope.h"
#include "RaidCombatState.h"
#include "RaidCombatPolicy.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using uint32 = uint32_t;
using uint64 = uint64_t;
static uint32 clockNow = 100;
uint32 getMSTime() { return clockNow; }
struct ObjectGuid
{
    uint64 id;
    uint64 GetRawValue() const { return id; }
};
class PlayerbotAI;
class Player
{
public:
    PlayerbotAI* ai = nullptr;
    uint64 id = 1;
    bool IsInWorld() const { return true; }
    ObjectGuid GetGUID() const { return {id}; }
};
class PlayerbotAI
{
public:
    Player bot;
    RaidCombat::State raidCombat;
    bool eligible = true;
    PlayerbotAI() { bot.ai = this; }
    bool IsRealPlayer() const { return false; }
};
#define GET_PLAYERBOT_AI(player) ((player)->ai)
class Map
{
public:
    struct Reference { Player* player; Player* GetSource() const { return player; } };
    DataMap CustomData;
    bool safe = true, combat = false;
    uint32 id = 469, instance = 1;
    unsigned sequence = 0;
    std::vector<Reference> players;
    bool IsRaid() const { return true; }
    uint32 GetId() const { return id; }
    uint32 GetInstanceId() const { return instance; }
    auto const& GetPlayers() const { return players; }
};
static unsigned dispatches = 0, releases = 0;
static RaidCombat::Positioning lastMovement = RaidCombat::Positioning::Release;
namespace RaidCombat
{
bool Eligible(PlayerbotAI& ai) { return ai.eligible; }
bool SafeBoundary(Map* map) { return map->safe; }
void Collect(Map* map, CthunPolicy::Snapshot& snapshot)
{
    auto& raid = snapshot.raid;
    raid.map = map->id;
    raid.instance = map->instance;
    raid.sequence = ++map->sequence;
    raid.combat = map->combat;
    raid.sampledAt = clockNow;
    for (auto const& ref : map->players)
    {
        auto& member = raid.members[raid.count++];
        member.unit.guid = ref.player->id;
        member.eligible = ref.player->ai && ref.player->ai->eligible;
        member.human = !ref.player->ai;
    }
}
void ReleaseGround(PlayerbotAI&) { ++releases; }
void MaintainGround(PlayerbotAI&, Intent const& intent, uint64, uint32)
{
    ++dispatches;
    lastMovement = intent.movement;
}
void ApplyCombatAction(PlayerbotAI&, Snapshot const&, Intent const&, uint64, uint32) { }
}
namespace CthunPositioning
{
void UpdatePolicy(Map*, std::vector<Player*> const&, uint32, std::string const&, std::string const&) { assert(false); }
}
#include "RaidCombatUpdate.inc"
int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs = std::filesystem;
    fs::path root(argv[1]);
    for (auto name : {"revisions", "defaults", "status"}) fs::create_directories(root / name);
    unsigned publication = 0;
    auto publish = [&](std::string const& source)
    {
        auto digest = CthunPolicy::Digest(source);
        std::ofstream(root / "revisions" / (digest + ".lua")) << source;
        std::ofstream(root / "defaults/raid.txt") << "1 1 " << "000000000000000" << ++publication << " " << digest;
        return digest;
    };
    std::string const good = "return {api=2,plan=function(s) local out={} for i,m in ipairs(s.members) do "
        "out[i]={movement=m.eligible and 1 or 0,target=0,operation=0,spell=0,aura=0,action_target=0} end return out end}";
    auto first = publish(good);
    Map map;
    PlayerbotAI ai;
    Player human;
    human.id = 2;
    map.players = {{&ai.bot}, {&human}};
    auto update = [&]
    {
        clockNow += 1000;
        RaidCombat::Update(&map, 1000, root.string(), (root / "status").string());
    };
    map.safe = false;
    update();
    auto* scope = map.CustomData.Get<CthunPolicy::Scope>("playerbots.cthun");
    assert(scope && !scope->active && scope->queued == first && releases);
    map.safe = true;
    update();
    assert(scope->active && scope->api == 2 && scope->revision == first);
    assert(dispatches == 1 && lastMovement == RaidCombat::Positioning::Hold);
    auto generation = scope->generation;
    update();
    assert(scope->generation == generation && scope->adopted == 1);
    publish("syntax bad!");
    map.safe = false;
    update();
    assert(scope->active && scope->revision == first && !scope->queued.empty());
    map.safe = true;
    update();
    assert(scope->active && scope->revision == first && scope->error.find("reload rejected") != std::string::npos);
    auto fault = publish("return {api=2,plan=function(s) if s.combat then error('combat failure') end "
        "local out={} for i,m in ipairs(s.members) do out[i]={movement=0,target=0,operation=0,spell=0,aura=0,action_target=0} end return out end}");
    update();
    assert(scope->active && scope->revision == fault && scope->generation != generation);
    map.safe = false;
    map.combat = true;
    update();
    assert(!scope->active && scope->faultedRevision == fault && scope->faultError.find("combat failure") != std::string::npos);
    map.safe = true;
    map.combat = false;
    update();
    assert(!scope->active && scope->queued.empty());
    auto corrected = publish(good + " -- corrected bytes");
    update();
    assert(scope->active && scope->revision == corrected);
    ai.eligible = false;
    update();
    assert(lastMovement == RaidCombat::Positioning::Release && scope->excluded == 2);
    ai.eligible = true;
    Map fresh;
    fresh.id = 533;
    fresh.instance = 2;
    fresh.players = map.players;
    clockNow += 1000;
    RaidCombat::Update(&fresh, 1000, root.string(), (root / "status").string());
    auto* second = fresh.CustomData.Get<CthunPolicy::Scope>("playerbots.cthun");
    assert(second && second != scope && second->active && second->revision == corrected);
    std::cout << "exact generic dispatch + real VM: new scopes, safe adoption, invalid candidate, last-good, fault/quarantine, status passed\n";
}
