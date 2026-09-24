// Offline API doubles exercise production target selection and taunt guards.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using uint8 = uint8_t;
using uint32 = uint32_t;
using ObjectGuid = uint32;
constexpr int BOT_STATE_COMBAT = 0;
constexpr int SPELL_EFFECT_ATTACK_ME = 114;
constexpr int SPELL_AURA_MOD_TAUNT = 11;
constexpr int ALLSPELLHOOK_ON_SPELL_CHECK_CAST = 1;
constexpr int ALLSPELLHOOK_CAN_PREPARE = 2;
enum SpellCastResult { SPELL_CAST_OK, SPELL_FAILED_DONT_REPORT, SPELL_FAILED_OTHER };
class Unit;
class Player;
class PlayerbotAI;
class Group;

struct ThreatReference
{
    Unit* owner = nullptr;
    Unit* GetOwner() { return owner; }
};
struct ThreatManager
{
    Unit* victim = nullptr;
    bool canHaveList = true;
    float threat = 1;
    std::map<uint32, ThreatReference*> threatenedByMe;
    bool CanHaveThreatList() { return canHaveList; }
    Unit* GetCurrentVictim() { return victim; }
    float GetThreat(Unit*) { return threat; }
    auto const& GetThreatenedByMeList() { return threatenedByMe; }
};
struct Unit
{
    virtual ~Unit() = default;
    virtual Player* ToPlayer() { return nullptr; }
    Unit* victim = nullptr;
    ThreatManager threats;
    bool alive = true;
    bool inWorld = true;
    bool creature = true;
    bool controlled = false;
    float x = 0;
    int map = 1;
    ObjectGuid guid = 0;
    Unit* GetVictim() { return victim; }
    ThreatManager& GetThreatMgr() { return threats; }
    bool IsAlive() { return alive; }
    bool IsInWorld() { return inWorld; }
    bool IsCreature() { return creature; }
    bool IsControlledByPlayer() { return controlled; }
    int GetMap() { return map; }
    ObjectGuid GetGUID() { return guid; }
    float GetDistance(Unit* other) { return std::abs(x - other->x); }
    bool IsWithinDistInMap(Unit* other, float radius)
    {
        return map == other->map && GetDistance(other) <= radius;
    }
    bool IsWithinMeleeRange(Unit* other) { return GetDistance(other) <= 5; }
};
struct WorldSession
{
    bool bot = true;
    bool IsBot() { return bot; }
};
struct Player : Unit
{
    Player() { creature = false; }
    Player* ToPlayer() override { return this; }
    Group* group = nullptr;
    PlayerbotAI* ai = nullptr;
    WorldSession session;
    WorldSession* GetSession() { return &session; }
    bool tankSpec = false;
    bool tankRole = false;
    bool explicitMT = false;
    bool offTank = false;
    Group* GetGroup() { return group; }
};
struct GroupReference
{
    Player* player = nullptr;
    GroupReference* following = nullptr;
    Player* GetSource() { return player; }
    GroupReference* next() { return following; }
};
struct Group
{
    std::vector<GroupReference> members;
    ObjectGuid moon = 0;
    Group(std::initializer_list<Player*> players)
    {
        members.reserve(players.size());
        for (Player* player : players)
        {
            player->group = this;
            members.push_back({player, nullptr});
        }
        for (size_t i = 1; i < members.size(); ++i)
            members[i - 1].following = &members[i];
    }
    GroupReference* GetFirstMember() { return members.empty() ? nullptr : &members[0]; }
    ObjectGuid GetTargetIcon(int) { return moon; }
};
template<class T> struct Value
{
    T data{};
    T Get() { return data; }
    void Set(T value) { data = value; }
};
struct Context
{
    std::map<std::string, Value<Unit*>> units;
    std::map<std::string, Value<std::string>> strings;
    std::map<std::string, Value<uint8>> counts;
    template<class T> Value<T>* GetValue(std::string name);
};
template<> Value<Unit*>* Context::GetValue<Unit*>(std::string name) { return &units[name]; }
template<> Value<std::string>* Context::GetValue<std::string>(std::string name) { return &strings[name]; }
template<> Value<uint8>* Context::GetValue<uint8>(std::string name) { return &counts[name]; }
struct PlayerbotAI
{
    Player* bot;
    Player* master = nullptr;
    Context context;
    bool offTankStrategy = false;
    bool realPlayer = false;
    std::vector<Unit*> attackers;
    explicit PlayerbotAI(Player* player) : bot(player) { bot->ai = this; }
    Player* GetBot() { return bot; }
    Player* GetMaster() { return master; }
    Context* GetAiObjectContext() { return &context; }
    bool HasStrategy(std::string, int) { return offTankStrategy; }
    static bool IsTank(Player* player, bool bySpec = false)
    {
        return bySpec ? player->tankSpec : player->tankRole || player->tankSpec;
    }
    static bool IsOffTank(Player* player) { return player->offTank; }
    bool IsExplicitMainTank(Player* player) { return player->explicitMT; }
    bool IsMainTank(Player* player) { return player->explicitMT; }
    uint32 GetGroupTankNum(Player*) { return 2; }
    bool IsValidUnit(Unit* unit) { return unit && unit->IsInWorld(); }
    bool IsRealPlayer() { return realPlayer; }
    bool HasAggro(Unit* unit);
};
#define GET_PLAYERBOT_AI(player) ((player)->ai)
struct HasAggroValue
{
    PlayerbotAI* botAI;
    Player* bot;
    Unit* target;
    Unit* GetTarget() { return target; }
    bool Calculate();
};
struct TankAssistTrigger
{
    PlayerbotAI* botAI;
    Player* bot;
    bool IsActive();
};
#define AI_VALUE(type, name) (botAI->GetAiObjectContext()->GetValue<type>(name)->Get())
#define AI_VALUE2(type, name, qualifier) (HasAggroValue{botAI, bot, AI_VALUE(Unit*, qualifier)}.Calculate())
enum class TargetValueExclusionType { Tank };
struct FindTargetStrategy
{
    PlayerbotAI* botAI;
    Unit* result = nullptr;
    explicit FindTargetStrategy(PlayerbotAI* ai) : botAI(ai) {}
    virtual ~FindTargetStrategy() = default;
    virtual TargetValueExclusionType GetExclusionType() { return TargetValueExclusionType::Tank; }
    virtual void CheckAttacker(Unit*, ThreatManager*) = 0;
};
using FindNonCcTargetStrategy = FindTargetStrategy;
struct RtiTargetValue
{
    Unit* icon = nullptr;
    Unit* Calculate() { return icon; }
};
struct TankTargetValue : RtiTargetValue
{
    PlayerbotAI* botAI;
    Player* bot;
    explicit TankTargetValue(PlayerbotAI* ai) : botAI(ai), bot(ai->GetBot()) {}
    Unit* Calculate();
    Unit* FindTarget(FindTargetStrategy* strategy)
    {
        for (Unit* attacker : botAI->attackers)
            strategy->CheckAttacker(attacker, &attacker->GetThreatMgr());
        return strategy->result;
    }
};
struct SpellEffectInfo
{
    bool area = false;
    float radius = 0;
    bool IsTargetingArea() const { return area; }
    float CalcRadius(Unit*) const { return radius; }
};
struct SpellInfo
{
    uint32 Id = 0;
    bool attackMe = false;
    bool tauntAura = false;
    SpellEffectInfo Effects[3]{};
    bool HasEffect(int) const { return attackMe; }
    bool HasAura(int) const { return tauntAura; }
    bool IsTargetingArea() const
    {
        return std::any_of(std::begin(Effects), std::end(Effects), [](auto const& effect) { return effect.area; });
    }
};
struct SpellCastTargets
{
    Unit* unit = nullptr;
    Unit* GetUnitTarget() { return unit; }
};
struct Spell
{
    Unit* caster;
    SpellInfo const* info;
    SpellCastTargets m_targets;
    Unit* GetCaster() { return caster; }
    SpellInfo const* GetSpellInfo() { return info; }
};
struct AuraEffect {};
struct AllSpellScript
{
    AllSpellScript(char const*, std::initializer_list<int>) {}
    virtual ~AllSpellScript() = default;
    virtual void OnSpellCheckCast(Spell*, bool, SpellCastResult&) {}
    virtual bool CanPrepare(Spell*, SpellCastTargets const*, AuraEffect const*) { return true; }
};

// PRODUCTION_CODE

int main()
{
    Player ari, redshift, healer, other;
    ari.guid = 142;
    redshift.guid = 1501;
    ari.tankSpec = redshift.tankSpec = true;
    ari.explicitMT = redshift.explicitMT = true;
    Group group{&ari, &redshift, &healer};
    Group unrelated{&other};
    other.tankSpec = true;
    PlayerbotAI ai(&ari);
    ai.master = &redshift;
    TankTargetValue selector(&ai);
    Unit held, loose, owned;
    held.guid = 1;
    loose.guid = 2;
    owned.guid = 3;
    held.victim = &redshift;
    loose.victim = &healer;
    owned.victim = &ari;
    ai.attackers = {&held};

    // Reproduces the duplicate-MT bug: the only available mob belongs to Redshift.
    assert(selector.Calculate() == nullptr);
    selector.icon = &held;
    assert(selector.Calculate() == nullptr); // An icon cannot override another tank's ownership.
    ai.attackers = {&held, &loose};
    assert(selector.Calculate() == &loose);
    selector.icon = nullptr;
    ai.context.units["current target"].data = &held;
    assert(selector.Calculate() == &loose);
    ai.context.units["tank target"].data = selector.Calculate();
    ai.context.counts["attacker count"].data = 2;
    TankAssistTrigger assist{&ai, &ari};
    assert(assist.IsActive()); // The live trigger must actually request the change, not just rank it.
    ai.context.units["current target"].data = &loose;
    assert(!assist.IsActive()); // Already acquiring the loose add; don't oscillate.
    ai.context.units["current target"].data = &held;

    // No MT flags, bot tank with no tank strategy, and a non-master tank are protected too.
    ari.explicitMT = redshift.explicitMT = false;
    ai.master = &healer;
    assert(selector.Calculate() == &loose);
    PlayerbotAI redAI(&redshift);
    assert(selector.Calculate() == &loose);
    ari.explicitMT = true;
    ai.context.units["current target"].data = &owned;
    ai.attackers = {&owned, &loose};
    assert(selector.Calculate() == &loose); // MT focus must not starve loose-add pickup.
    std::reverse(ai.attackers.begin(), ai.attackers.end());
    assert(selector.Calculate() == &loose);
    ai.attackers = {&owned};
    assert(selector.Calculate() == &owned); // Keep tanking our own mobs.
    Unit ownedFar;
    ownedFar.victim = &ari;
    ownedFar.x = 15;
    ai.context.units["current target"].data = &ownedFar;
    ai.attackers = {&owned, &ownedFar};
    assert(selector.Calculate() == &ownedFar); // With no loose adds, preserve MT focus as before.

    HasAggroValue aggro{&ai, &ari, &held};
    assert(aggro.Calculate()); // "Already covered" even when Ari is explicitly MT.
    assert(ai.HasAggro(&held));
    held.victim = nullptr;
    held.threats.victim = &redshift;
    assert(ai.HasAggro(&held));
    ai.attackers = {&held};
    assert(selector.Calculate() == nullptr); // Threat-manager victim fallback.
    held.threats.victim = nullptr;
    assert(selector.Calculate() == &held); // Unclaimed target can be collected.
    held.victim = &redshift;
    redshift.alive = false;
    assert(selector.Calculate() == &held); // Dead tank must not reserve a mob.
    ai.master = &redshift;
    ai.offTankStrategy = true;
    ari.explicitMT = false;
    assert(selector.Calculate() == &held); // Legacy off-tank/master guard must release dead owners too.
    assert(!aggro.Calculate());
    assert(!ai.HasAggro(&held));
    ai.offTankStrategy = false;
    ai.master = &healer;
    ari.explicitMT = true;
    redshift.alive = true;
    redshift.tankSpec = false;
    redshift.explicitMT = false;
    assert(selector.Calculate() == &held); // Rescue DPS/healer, not their icon/old role.
    held.victim = &other;
    assert(selector.Calculate() == &held); // An unrelated group is outside the policy.
    held.victim = &redshift;
    redshift.tankSpec = true;

#ifdef HAVE_TAUNT_GUARD
    SpellInfo taunt{355, true, false};
    SpellInfo mockingBlow{694, false, true};
    SpellInfo damage{20271, false, false};
    SpellInfo defense{31789, false, false};
    SpellInfo grip{49576, false, false};
    SpellInfo area{1161, false, true};
    area.Effects[0] = {true, 10};
    using ai::threat::WouldTauntOtherTank;
    assert(WouldTauntOtherTank(&ai, &taunt, &held));
    assert(WouldTauntOtherTank(&ai, &mockingBlow, &held));
    assert(WouldTauntOtherTank(&ai, &grip, &held));
    assert(!WouldTauntOtherTank(&ai, &taunt, &loose));
    assert(!WouldTauntOtherTank(&ai, &taunt, &owned));
    assert(!WouldTauntOtherTank(&ai, &damage, &held));
    assert(!WouldTauntOtherTank(nullptr, &taunt, &held));
    assert(!WouldTauntOtherTank(&ai, nullptr, &held));
    assert(!WouldTauntOtherTank(&ai, &taunt, nullptr));
    held.alive = false;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held));
    held.alive = true;
    assert(WouldTauntOtherTank(&ai, &defense, &redshift));
    assert(!WouldTauntOtherTank(&ai, &defense, &healer));
    assert(!WouldTauntOtherTank(&ai, &defense, &ari));
    redshift.alive = false;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held));
    redshift.alive = true;

    ThreatReference reference{&held};
    redshift.threats.threatenedByMe[held.guid] = &reference;
    assert(WouldTauntOtherTank(&ai, &area, &ari));
    held.x = 30;
    assert(!WouldTauntOtherTank(&ai, &area, &ari));
    held.x = 0;
    held.victim = &healer;
    assert(!WouldTauntOtherTank(&ai, &area, &ari));
    held.victim = &redshift;
    redshift.map = 2;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held));
    redshift.map = 1;
    redshift.inWorld = false;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held));
    redshift.inWorld = true;

    PlayerbotsTankTauntScript hook;
    Spell cast{&ari, &taunt, {&held}};
    SpellCastResult result = SPELL_CAST_OK;
    hook.OnSpellCheckCast(&cast, true, result);
    assert(result == SPELL_FAILED_DONT_REPORT);
    assert(!hook.CanPrepare(&cast, nullptr, nullptr));
    held.victim = &healer;
    result = SPELL_CAST_OK;
    hook.OnSpellCheckCast(&cast, true, result);
    assert(result == SPELL_CAST_OK);
    held.victim = &redshift; // Changed since eligibility check: recheck at prepare.
    assert(!hook.CanPrepare(&cast, nullptr, nullptr));
    result = SPELL_FAILED_OTHER;
    hook.OnSpellCheckCast(&cast, true, result);
    assert(result == SPELL_FAILED_OTHER);
    ai.realPlayer = true;
    assert(hook.CanPrepare(&cast, nullptr, nullptr)); // Never restrict human casts.
    ai.realPlayer = false;
    ari.session.bot = false;
    assert(hook.CanPrepare(&cast, nullptr, nullptr)); // A human session is never restricted, even with AI.
    ari.session.bot = true;
    cast.caster = &healer;
    assert(hook.CanPrepare(&cast, nullptr, nullptr)); // No bot AI, no guard.
    assert(hook.CanPrepare(nullptr, nullptr, nullptr));

    // Shared rule is live, not a saved victim/MT lock. PvP player targets stay untouched.
    Player opponent;
    opponent.victim = &redshift;
    assert(!WouldTauntOtherTank(&ai, &taunt, &opponent));
    held.controlled = true;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held)); // PvP pets/charmed units are not tank assignments.
    held.controlled = false;
    ari.group = nullptr;
    assert(!WouldTauntOtherTank(&ai, &taunt, &held));
#endif
    std::cout << "Tank ownership, loose-add selection and taunt guard tests passed\n";
}
