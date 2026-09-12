// Production command reset logic with isolated map/respawn API doubles. No server connection.
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include "RaidScalingMgr.h"

enum EncounterState { NOT_STARTED, IN_PROGRESS, FAIL, DONE };
constexpr uint32 ENCOUNTER_CREDIT_KILL_CREATURE = 0;
struct CreatureData
{
    uint32 id = 0, id2 = 0, id3 = 0, mapid = 531, spawnMask = 1, spawnGroupId = 0;
    float posX = 0, posY = 0;
};
using GameObjectData = CreatureData;
struct DungeonEncounterEntry { uint32 mapId = 531, encounterIndex = 6; };
struct DungeonEncounter
{
    DungeonEncounterEntry const* dbcEntry;
    uint32 creditType = 0, creditEntry = 15275;
};
using DungeonEncounterList = std::list<DungeonEncounter const*>;
struct ObjectMgr
{
    std::map<uint32, CreatureData> data;
    std::map<uint32, GameObjectData> doors;
    std::map<ObjectGuid, ObjectGuid> links;
    DungeonEncounterList encounters;
    auto const& GetAllCreatureData() const { return data; }
    auto const& GetAllGOData() const { return doors; }
    CreatureData const* GetCreatureData(uint32 id) const { return &data.at(id); }
    GameObjectData const* GetGameObjectData(uint32 id) const { return &doors.at(id); }
    ObjectGuid GetLinkedRespawnGuid(ObjectGuid id) const
    {
        auto i = links.find(id);
        return i == links.end() ? ObjectGuid{} : i->second;
    }
    DungeonEncounterList const* GetDungeonEncounterList(uint32, uint32) const { return &encounters; }
} objectMgr;
#define sObjectMgr (&objectMgr)
struct PoolMgr
{
    std::set<uint32> pooled;
    template<class T> bool IsPartOfAPool(uint32 id) const { return pooled.contains(id); }
} poolMgr;
#define sPoolMgr (&poolMgr)
namespace GameTime
{
    struct Time { time_t count() const { return 100; } };
    Time GetGameTime() { return {}; }
}
class ChatHandler
{
public:
    std::string message;
    void SendSysMessage(char const* text) { message = text; }
    template<class... Args> void PSendSysMessage(char const* text, Args...) { message = text; }
};
class Player
{
public:
    bool combat = false;
    bool IsInCombat() const { return combat; }
};
struct PlayerReference
{
    Player* player;
    Player* GetSource() const { return player; }
};
class GameObject
{
public:
    uint32 entry = 0, spawnId = 0;
    bool spawned = true, open = false;
    uint32 GetEntry() const { return entry; }
    ObjectGuid GetGUID() const { return ObjectGuid::Create<HighGuid::Unit>(entry, spawnId); }
    bool isSpawned() const { return spawned; }
};
class InstanceScript
{
public:
    std::array<EncounterState, 10> states{};
    uint32 mask = 127, saves = 0;
    bool reject = false;
    std::set<uint32> triggers{4047, 9999};
    uint64 deadline = 1789397072, extended = 1789656272;
    uint32 binds = 10;
    bool IsEncounterInProgress() const
    { return std::find(states.begin(), states.end(), IN_PROGRESS) != states.end(); }
    void SetBossState(uint32 i, EncounterState state) { if (!reject) { states.at(i) = state; ++saves; } }
    EncounterState GetBossState(uint32 i) const { return states.at(i); }
    uint32 GetEncounterCount() const { return states.size(); }
    void SetCompletedEncountersMask(uint32 m, bool save) { assert(save); mask = m; ++saves; }
    uint32 GetCompletedEncounterMask() const { return mask; }
    void ResetAreaTriggerDone(uint32 i) { triggers.erase(i); }
    void HandleGameObject(ObjectGuid, bool open, GameObject* go) { go->open = open; }
};
class Map
{
public:
    uint32 id = 531, instance = 5670, mode = 0, failSpawn = 0, generic = 0;
    bool activeGroup = true, combatOnLoad = false;
    InstanceScript script;
    Player player;
    std::vector<PlayerReference> players{{&player}};
    std::multimap<uint32, Creature*> creatures;
    std::multimap<uint32, GameObject*> gos;
    std::vector<std::unique_ptr<Creature>> owned;
    std::vector<std::unique_ptr<GameObject>> ownedGos;
    std::map<uint32, time_t> timers;
    std::vector<uint32> processed, scaled;
    uint32 guidCounter = 1000;
    uint32 GetId() const { return id; }
    uint32 GetInstanceId() const { return instance; }
    uint32 GetDifficulty() const { return mode; }
    uint32 GetSpawnMode() const { return mode; }
    InstanceScript* GetInstanceScript() { return &script; }
    auto const& GetPlayers() const { return players; }
    auto& GetCreatureBySpawnIdStore() { return creatures; }
    auto& GetGameObjectBySpawnIdStore() { return gos; }
    bool IsSpawnGroupActive(uint32) const { return activeGroup; }
    void LoadGrid(float, float) { if (combatOnLoad) player.combat = true; }
    Creature* Add(uint32 spawnId, bool alive = true, bool compat = false);
    Creature* GetCreature(ObjectGuid guid);
    void SaveCreatureRespawnTime(uint32 i, time_t& t) { timers[i] = t; }
    void ProcessCreatureRespawn(uint32 i);
    void ProcessGameObjectRespawn(uint32 i)
    {
        auto bounds = gos.equal_range(i);
        for (auto j = bounds.first; j != bounds.second; ++j)
            if (j->second->spawned) return;
        auto go = std::make_unique<GameObject>();
        go->entry = objectMgr.doors.at(i).id; go->spawnId = i;
        gos.emplace(i, go.get()); ownedGos.push_back(std::move(go));
    }
    Creature* Alive(uint32 i);
    uint32 AliveCount(uint32 i);
};
class InstanceMap : public Map {};
struct Events
{
    bool scheduled = true;
    void KillAllEvents(bool force) { assert(!force); scheduled = false; }
};
struct ThreatManager
{
    uint32 threat = 500;
    void ClearAllThreat() { threat = 0; }
};
class Creature
{
public:
    Map* map;
    uint32 spawnId = 0, entry = 0, hp = 12208;
    ObjectGuid guid, owner;
    bool alive = true, compat = false, combat = false, aura = true, casting = true, staleIntro = true;
    bool aiFailure = false;
    Events m_Events;
    ThreatManager threat;
    bool IsInCombat() const { return combat; }
    bool IsAlive() const { return alive; }
    ObjectGuid GetGUID() const { return guid; }
    ObjectGuid GetCharmerOrOwnerGUID() const { return owner; }
    void InterruptNonMeleeSpells(bool all) { assert(all); casting = false; }
    ThreatManager& GetThreatManager() { return threat; }
    void RemoveAllAuras() { aura = false; }
    void Respawn(bool force)
    {
        assert(force && !casting && !m_Events.scheduled && threat.threat == 0 && !aura);
        // Models native Respawn(true): compat reinitializes in place; non-compat removes/queues.
        alive = compat;
        if (compat) hp = 3052;
        else map->timers[spawnId] = 100;
    }
    bool AIM_Initialize() { staleIntro = false; return !aiFailure; }
};
Creature* Map::Add(uint32 i, bool alive, bool compat)
{
    auto c = std::make_unique<Creature>();
    c->map = this; c->entry = objectMgr.data.at(i).id; c->spawnId = i; c->alive = alive; c->compat = compat;
    c->guid = ObjectGuid::Create<HighGuid::Unit>(c->entry, ++guidCounter);
    Creature* raw = c.get(); creatures.emplace(i, raw); owned.push_back(std::move(c)); return raw;
}
Creature* Map::GetCreature(ObjectGuid guid)
{
    for (auto const& c : owned) if (c->guid == guid) return c.get();
    return nullptr;
}
uint32 Map::AliveCount(uint32 i)
{
    uint32 count = 0; auto bounds = creatures.equal_range(i);
    for (auto j = bounds.first; j != bounds.second; ++j) if (j->second->alive) ++count;
    return count;
}
Creature* Map::Alive(uint32 i)
{
    auto bounds = creatures.equal_range(i);
    for (auto j = bounds.first; j != bounds.second; ++j) if (j->second->alive) return j->second;
    return nullptr;
}
void Map::ProcessCreatureRespawn(uint32 i)
{
    processed.push_back(i);
    if (i == failSpawn) return;
    if (Alive(i)) { timers.erase(i); return; }
    auto const& d = objectMgr.data.at(i);
    auto linked = objectMgr.GetLinkedRespawnGuid(ObjectGuid::Create<HighGuid::Unit>(d.id, i));
    if (linked && timers.contains(linked.value & 0xffffffffu)) return;
    timers.erase(i);
    Creature* c = Add(i);
    c->aura = c->casting = c->staleIntro = c->m_Events.scheduled = false;
    c->threat.threat = 0;
    c->hp = 3052;
}
void RaidScalingMgr::ApplyToCreature(Creature* c)
{
    c->map->scaled.push_back(c->spawnId);
    auto settings = _state.Get(RaidScaleKey(c->map->id, c->map->instance));
    c->hp = settings ? uint32(3052 * settings->trashHealth) : 3052;
}
uint32 RaidScalingMgr::RespawnGameObjectEntries(Map*, std::vector<uint32> const& entries) const
{
    assert(entries.empty() || (entries == std::vector<uint32>{180634, 180635})); return 0;
}
uint32 RaidScalingMgr::RespawnCreatureEntries(Map* map, std::vector<uint32> const&) const
{ ++map->generic; return 0; }
void RaidScalingMgr::ApplyToMap(Map* map, ChatHandler*) { ++map->generic; }
std::vector<RaidBossResetRecipe> const& RaidScalingMgr::GetBosses(uint32) const
{
    static std::vector<RaidBossResetRecipe> recipes = {{1, 1, "Skeram", "", {}, {}},
        {7, 7, "Twin Emperors", "", {}, {}}};
    return recipes;
}
/* PRODUCTION */
/* DISPATCHER */

struct Scene
{
    InstanceMap map;
    RaidScalingMgr mgr;
    ChatHandler handler;
    DungeonEncounterEntry dbc;
    DungeonEncounter encounter{&dbc};
    Creature* corridor;
    Scene()
    {
        objectMgr.data.clear(); objectMgr.doors.clear(); objectMgr.links.clear(); poolMgr.pooled.clear();
        objectMgr.encounters = {&encounter};
        for (auto [spawn, entry] : std::map<uint32, uint32>{{1,15275},{2,15276},{3,15963},{4,15316},
                                                          {5,15317},{6,15316},{7,15509}})
            objectMgr.data[spawn].id = entry;
        for (uint32 i : {4u,5u})
            objectMgr.links[ObjectGuid::Create<HighGuid::Unit>(objectMgr.data[i].id,i)] =
                ObjectGuid::Create<HighGuid::Unit>(15276,2);
        objectMgr.doors[10].id=180634; objectMgr.doors[11].id=180635;
        map.script.states.fill(DONE); map.script.states[0]=NOT_STARTED;
        map.timers={{1,90000},{2,90000},{3,90000},{5,90000},{7,99999}};
        map.Add(1,false); map.Add(2,false); map.Add(4,true);
        corridor=map.Add(6); corridor->hp=11111;
        RaidScaleSettings settings; settings.targetPlayers=10; settings.trashHealth=.25;
        mgr._state.Set(RaidScaleKey(map.id,map.instance),settings);
    }
    void Untouched()
    {
        for (uint32 i : {1u,2u,3u,4u,5u,6u,8u,9u}) assert(map.script.states[i]==DONE);
        assert(map.script.deadline==1789397072 && map.script.extended==1789656272 && map.script.binds==10);
        assert(map.timers[7]==99999 && corridor->hp==11111 && corridor->aura);
        assert(map.generic==0);
    }
    void Reset() { assert(mgr.ResetBoss(&handler,&map,7)); }
};
int main(int argc,char** argv)
{
    assert(argc==2); std::string mode=argv[1];
    Scene s;
    if (mode=="complete")
    {
        InstanceMap other;
        other.instance=5671;
        Creature* otherBug=other.Add(4);
        otherBug->hp=9999;
        s.Reset(); s.Untouched();
        assert(otherBug->hp==9999 && other.processed.empty() && other.script.mask==127);
        assert(s.map.script.states[7]==NOT_STARTED && s.map.script.mask==63);
        assert(!s.map.script.triggers.contains(4047) && s.map.script.triggers.contains(9999));
        assert(s.map.gos.find(10)->second->open && !s.map.gos.find(11)->second->open);
        for (uint32 i=1;i<=5;++i)
        {
            assert(s.map.AliveCount(i)==1 && s.map.Alive(i)->hp==763);
            assert(!s.map.Alive(i)->aura && !s.map.Alive(i)->staleIntro && !s.map.timers.contains(i));
        }
        s.Reset(); s.Untouched(); // idempotent, including dead objects not yet removed from the map store
        for (uint32 i=1;i<=5;++i) assert(s.map.AliveCount(i)==1);
    }
    else if (mode=="compatibility")
    {
        s.map.GetCreatureBySpawnIdStore().find(1)->second->compat=true;
        s.map.GetCreatureBySpawnIdStore().find(4)->second->compat=true;
        s.mgr._state.Disable(RaidScaleKey(s.map.id,s.map.instance));
        s.Reset(); s.Untouched();
        assert(!s.map.Alive(1)->staleIntro && s.map.Alive(4)->hp==3052);
    }
    else if (mode=="guards")
    {
        s.map.player.combat=true;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.map.player.combat=false;
        s.map.script.states[8]=IN_PROGRESS;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.map.script.states[8]=DONE;
        s.corridor->combat=true;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.corridor->combat=false;
        s.map.Alive(4)->owner={42};
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.map.Alive(4)->owner={};
        s.map.combatOnLoad=true;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7));
        assert(s.map.script.saves==0 && s.map.script.mask==127 && s.map.processed.empty());
    }
    else if (mode=="catalogue")
    {
        objectMgr.encounters.clear(); assert(!s.mgr.ResetBoss(&s.handler,&s.map,7));
        objectMgr.encounters={&s.encounter}; s.dbc.encounterIndex=32;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.dbc.encounterIndex=6;
        auto links=objectMgr.links;
        objectMgr.links.clear(); assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); objectMgr.links=links;
        poolMgr.pooled.insert(4); assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); poolMgr.pooled.clear();
        s.map.activeGroup=false; assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); s.map.activeGroup=true;
        objectMgr.data[1].id2=999; assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); objectMgr.data[1].id2=0;
        auto exit=objectMgr.doors.at(11);
        objectMgr.doors.erase(11); assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); objectMgr.doors[11]=exit;
        objectMgr.data[2].mapid=1; assert(!s.mgr.ResetBoss(&s.handler,&s.map,7)); objectMgr.data[2].mapid=531;
        assert(s.map.script.saves==0 && s.map.processed.empty());
    }
    else if (mode=="mask-failure")
    {
        s.dbc.encounterIndex=4; // prove bit is resolved, not hardcoded to script index 7 or expected DBC index 6
        s.map.failSpawn=2;
        assert(!s.mgr.ResetBoss(&s.handler,&s.map,7));
        assert(s.map.script.mask==(127u & ~16u));
        assert(s.handler.message.find("incomplete")!=std::string::npos);
        s.map.failSpawn=0; s.Reset(); s.Untouched();
    }
    else if (mode=="generic")
    {
        assert(s.mgr.ResetBoss(&s.handler,&s.map,1));
        assert(s.map.generic==2 && s.map.processed.empty() && s.map.script.mask==127);
    }
    else return 1;
    std::cout << "Passed " << mode << '\n';
}
