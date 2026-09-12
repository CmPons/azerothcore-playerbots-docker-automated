// Offline production-method regressions; API doubles do not simulate live raid combat.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include "RaidScalingMgr.h"

constexpr uint32 CREATURE_ELITE_NORMAL = 0, CREATURE_ELITE_ELITE = 1,
    CREATURE_ELITE_RAREELITE = 2, CREATURE_ELITE_WORLDBOSS = 3;
enum UnitMods { UNIT_MOD_HEALTH, UNIT_MOD_END };
enum UnitModifierFlatType { BASE_VALUE, TOTAL_VALUE };
enum UnitModifierPctType { BASE_PCT, TOTAL_PCT };
constexpr uint8 AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK = 1, AURA_EFFECT_HANDLE_STAT = 2;
constexpr uint32 SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT = 133;
#define LOG_ERROR(...) assert(false)
template<class T> uint32 CalculatePct(T value, float pct) { return uint32(float(value) * pct / 100.0f); }
struct Creature;
class Map
{
public:
    uint32 id = 531, instance = 1;
    bool raid = true;
    std::map<uint32, Creature*> spawns;
    uint32 GetId() const { return id; }
    uint32 GetInstanceId() const { return instance; }
    bool IsRaid() const { return raid; }
    char const* GetMapName() const { return "test"; }
    auto const& GetCreatureBySpawnIdStore() const { return spawns; }
};
class ChatHandler
{
public:
    void SendSysMessage(char const*) {}
    template<class... Args> void PSendSysMessage(char const*, Args...) {}
};
class Unit
{
public:
    virtual ~Unit() = default;
    bool player = false, controlled = false;
    uint32 health = 3052, maxHealth = 3052;
    float flat[2] = {3052, 0}, pct[2] = {1, 1};
    virtual Creature* ToCreature() { return nullptr; }
    bool IsPlayer() const { return player; }
    bool IsControlledByPlayer() const { return controlled; }
    uint32 GetHealth() const { return health; }
    uint32 GetMaxHealth() const { return maxHealth; }
    float GetHealthPct() const { return 100.0f * float(health) / float(maxHealth); }
    bool IsAlive() const { return health != 0; }
    void SetMaxHealth(uint32 n) { maxHealth = n; health = std::min(health, n); }
    void SetHealth(uint32 n) { health = std::min(n, maxHealth); }
    virtual void UpdateMaxHealth() {}
    float GetFlatModifierValue(UnitMods, UnitModifierFlatType type) const { return flat[type]; }
    float GetPctModifierValue(UnitMods, UnitModifierPctType type) const { return pct[type]; }
    float GetTotalAuraModValue(UnitMods) const;
    void SetStatFlatModifier(UnitMods, UnitModifierFlatType type, float n) { flat[type] = n; UpdateMaxHealth(); }
    void ApplyStatPctModifier(UnitMods, UnitModifierPctType type, float n)
    {
        pct[type] *= 1 + n / 100;
        UpdateMaxHealth();
    }
    void SetStatPctModifier(UnitMods, UnitModifierPctType type, float n) { pct[type] = n; UpdateMaxHealth(); }
    float GetTotalAuraMultiplier(uint32) const { return 1; } // no other health aura in these fixtures
};
struct CreatureTemplate { uint32 rank = 0; };
struct CreatureData { uint32 id = 15316; };
struct Creature : Unit
{
    Map* map;
    CreatureTemplate proto;
    CreatureData data;
    uint32 entry = 15316, spawnId = 1, createHealth = 3052;
    bool pet = false, trigger = false, critter = false, civilian = false, boss = false,
        worldBoss = false, hasData = true, hasProto = true;
    explicit Creature(Map* m) : map(m) {}
    Creature* ToCreature() override { return this; }
    Map* GetMap() const { return map; }
    uint32 GetMapId() const { return map->GetId(); }
    uint32 GetEntry() const { return entry; }
    uint32 GetSpawnId() const { return spawnId; }
    ObjectGuid GetGUID() const { return ObjectGuid::Create<HighGuid::Unit>(entry, spawnId); }
    CreatureData const* GetCreatureData() const { return hasData ? &data : nullptr; }
    CreatureTemplate const* GetCreatureTemplate() const { return hasProto ? &proto : nullptr; }
    bool IsPet() const { return pet; }
    bool IsTrigger() const { return trigger; }
    bool IsCritter() const { return critter; }
    bool IsCivilian() const { return civilian; }
    bool IsDungeonBoss() const { return boss; }
    bool isWorldBoss() const { return worldBoss; }
    bool isDead() const { return !IsAlive(); }
    uint32 GetCreateHealth() const { return createHealth; }
    void SetCreateHealth(uint32 n) { createHealth = n; }
    void UpdateMaxHealth() override;
    void ResetPlayerDamageReq() {}
};
struct ObjectMgr
{
    std::map<ObjectGuid, ObjectGuid> links;
    ObjectGuid GetLinkedRespawnGuid(ObjectGuid guid) const
    {
        auto i = links.find(guid);
        return i == links.end() ? ObjectGuid{} : i->second;
    }
} objectMgr;
#define sObjectMgr (&objectMgr)
struct AuraApplication
{
    Unit* target;
    Unit* GetTarget() const { return target; }
};
struct AuraEffect
{
    int GetAmount() const { return 300; } // unchanged spell 802 health percentage
    void HandleAuraModIncreaseHealthPercent(AuraApplication const*, uint8, bool) const;
};
using DamageEffectType = uint32;
struct RaidScalingUnitScript
{
    uint32 DealDamage(Unit*, Unit*, uint32, DamageEffectType);
};
/* PRODUCTION */

void Mutate(Creature& bug, bool apply = true)
{
    AuraApplication application{&bug};
    AuraEffect{}.HandleAuraModIncreaseHealthPercent(&application, AURA_EFFECT_HANDLE_STAT, apply);
}
void Link(Creature& bug, uint32 boss = 15276)
{
    bug.data.id = bug.entry;
    objectMgr.links[bug.GetGUID()] = ObjectGuid::Create<HighGuid::Unit>(boss, 999);
    bug.map->spawns[bug.spawnId] = &bug;
}
RaidScaleSettings Default()
{
    return RaidScalingMgr::Instance().MakeSettings(40, 10, true);
}
void Enable(RaidScalingMgr& mgr, Map& map, RaidScaleSettings settings = Default())
{
    mgr._state.Set(mgr.MakeKey(&map), settings);
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    RaidScalingMgr& mgr = RaidScalingMgr::Instance();
    Map map;
    Creature bug(&map);
    Link(bug);
    Enable(mgr, map);
    Unit player;
    player.player = true;
    if (scenario == "mutation")
    {
        for (uint32 entry : {15316u, 15317u})
        {
            Creature add(&map);
            add.entry = entry;
            add.spawnId = entry;
            Link(add);
            mgr.OnCreatureAddWorld(&add);
            assert(add.GetCreateHealth() == 763 && add.GetMaxHealth() == 763);
            add.SetHealth(381);
            Mutate(add);
            assert(add.GetMaxHealth() == 3052 && add.GetHealth() == 1524);
            // Mutated scaled max equals original max: reapplication must NOT heal or divide again.
            mgr.ApplyToCreature(&add);
            mgr.ApplyToCreature(&add);
            assert(add.GetMaxHealth() == 3052 && add.GetHealth() == 1524);
            Mutate(add, false);
            assert(add.GetMaxHealth() == 763 && add.GetHealth() == 381);
            Mutate(add);
            assert(add.GetMaxHealth() == 3052 && add.GetHealth() == 1524);
            mgr.RestoreCreature(&add);
            assert(add.GetMaxHealth() == 12208 && add.GetHealth() == 6096);
            Mutate(add, false);
            assert(add.GetMaxHealth() == 3052 && add.GetHealth() == 1524);
        }
    }
    else if (scenario == "damage")
    {
        RaidScalingUnitScript hook;
        assert(std::fabs(mgr.GetDamageScale(&bug, &player) - std::pow(.25f, .6f)) < .00001f);
        // Feed already-mutated physical damage into the real outgoing-damage hook, just once.
        assert(hook.DealDamage(&bug, &player, 1900, 0) == 827);
        Unit pet;
        pet.controlled = true;
        assert(hook.DealDamage(&bug, &pet, 1900, 0) == 827);
        Creature npc(&map);
        assert(hook.DealDamage(&bug, &npc, 1900, 0) == 1900);
        assert(hook.DealDamage(&player, &bug, 1900, 0) == 1900);
        assert(hook.DealDamage(&bug, &player, 0, 0) == 0);
        auto settings = Default();
        settings.trashDamage = .6f;
        settings.bossDamage = .2f;
        mgr._state.Set(mgr.MakeKey(&map), settings);
        assert(hook.DealDamage(&bug, &player, 1900, 0) == 1140);
        mgr._enabled = false;
        assert(hook.DealDamage(&bug, &player, 1900, 0) == 1900);
    }
    else if (scenario == "scope")
    {
        assert(mgr.IsScalableCreature(&bug));
        for (bool Creature::*flag : {&Creature::pet, &Creature::trigger, &Creature::critter, &Creature::civilian})
        {
            bug.*flag = true;
            assert(!mgr.IsScalableCreature(&bug));
            bug.*flag = false;
        }
        assert(!mgr.IsScalableCreature(nullptr));
        bug.hasProto = false;
        assert(!mgr.IsScalableCreature(&bug));
        bug.hasProto = true;
        bug.hasData = false;
        assert(!mgr.IsScalableCreature(&bug));
        bug.hasData = true;
        Link(bug, 15275); // not the room's native Vek'lor respawn link
        assert(!mgr.IsScalableCreature(&bug));
        Link(bug);
        map.id = 1;
        assert(!mgr.IsScalableCreature(&bug));
        map.id = 531;
        bug.map = nullptr;
        assert(!mgr.IsScalableCreature(&bug));
        bug.map = &map;
        map.raid = false;
        assert(mgr.GetDamageScale(&bug, &player) == 1);
        map.raid = true;
        map.instance = 0;
        assert(mgr.GetDamageScale(&bug, &player) == 1);
        map.instance = 1;
        bug.entry = 15300;
        Link(bug);
        assert(!mgr.IsScalableCreature(&bug));
        bug.entry = 15316;
        Link(bug);
        objectMgr.links.clear(); // same entry, corridor spawn
        mgr.OnCreatureAddWorld(&bug);
        Mutate(bug);
        assert(bug.GetMaxHealth() == 12208 && mgr.GetDamageScale(&bug, &player) == 1);
        for (uint32 rank : {1u, 2u, 3u})
        {
            bug.proto.rank = rank;
            assert(mgr.IsScalableCreature(&bug));
        }
    }
    else if (scenario == "lifecycle")
    {
        Mutate(bug); // first enable/apply can happen while mutation is already present
        bug.SetHealth(6104);
        mgr.ApplyToMap(&map);
        assert(bug.GetMaxHealth() == 3052 && bug.GetHealth() == 1526);
        auto settings = Default();
        settings.trashHealth = .5f;
        settings.bossHealth = .1f;
        mgr._state.Set(mgr.MakeKey(&map), settings);
        mgr.ApplyToMap(&map);
        assert(bug.GetMaxHealth() == 6104 && bug.GetHealth() == 3052);
        mgr.DisableForMap(&map);
        assert(bug.GetMaxHealth() == 12208 && bug.GetHealth() == 6104);
        assert(mgr.GetDamageScale(&bug, &player) == 1);
        mgr.OnCreatureAddWorld(&bug);
        assert(bug.GetMaxHealth() == 12208);
        Enable(mgr, map);
        mgr.ApplyToMap(&map);
        assert(bug.GetMaxHealth() == 3052 && bug.GetHealth() == 1526);
        Map other;
        other.instance = 2;
        Creature unscaled(&other);
        Link(unscaled);
        mgr.OnCreatureAddWorld(&unscaled);
        assert(unscaled.GetMaxHealth() == 3052 && mgr.GetDamageScale(&unscaled, &player) == 1);
        bug.SetHealth(0);
        mgr.ApplyToCreature(&bug);
        assert(bug.GetHealth() == 0);
        mgr.DisableForMap(&map);
        assert(bug.GetMaxHealth() == 12208 && bug.GetHealth() == 0); // no dead-bug revival on restore
    }
    else if (scenario == "legacy")
    {
        // Reproduce why only adding eligibility is insufficient: mutation ignores CreateHealth.
        bug.SetCreateHealth(763);
        bug.SetMaxHealth(763);
        bug.SetHealth(763);
        Mutate(bug);
        assert(bug.GetMaxHealth() == 12208);
        // Other elite/boss scaling remains on the existing path, without changing their unit-mod base.
        Creature boss(&map);
        boss.entry = 15275;
        boss.spawnId = 2;
        boss.proto.rank = 3;
        mgr.ApplyToCreature(&boss);
        assert(boss.GetMaxHealth() == 763 && boss.flat[BASE_VALUE] == 3052);
        mgr.RestoreCreature(&boss);
        assert(boss.GetMaxHealth() == 3052);
    }
    else
        return 1;
    std::cout << "Passed " << scenario << '\n';
}
