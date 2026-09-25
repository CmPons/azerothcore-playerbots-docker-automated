// Offline API doubles; production policy, selectors, commands and group assignment are injected below.
#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "DataMap.h"

using uint8 = uint8_t;
using uint32 = uint32_t;
using Milliseconds = std::chrono::milliseconds;
constexpr int BOT_STATE_COMBAT = 0;
constexpr int SPELL_EFFECT_ATTACK_ME = 114;
constexpr int SPELL_EFFECT_PERSISTENT_AREA_AURA = 27;
constexpr int SPELL_AURA_MOD_TAUNT = 11;
constexpr int CHAR_UPD_GROUP_MEMBER_FLAG = 1;
enum GroupMemberFlags { MEMBER_FLAG_ASSISTANT = 1, MEMBER_FLAG_MAINTANK = 2, MEMBER_FLAG_MAINASSIST = 4 };
struct ObjectGuid
{
    uint32 id = 0;
    ObjectGuid() = default;
    ObjectGuid(uint32 value) : id(value) {}
    uint32 GetCounter() const { return id; }
    bool IsEmpty() const { return id == 0; }
    void Clear() { id = 0; }
    auto operator<=>(ObjectGuid const&) const = default;
    static ObjectGuid const Empty;
};
ObjectGuid const ObjectGuid::Empty{};
class Unit;
class Player;
class Creature;
class Group;
class PlayerbotAI;
struct ThreatReference
{
    Unit* owner;
    Unit* GetOwner() { return owner; }
};
struct ThreatManager
{
    Unit* victim = nullptr;
    bool hasList = true;
    float threat = 1;
    std::map<ObjectGuid, ThreatReference*> references;
    bool CanHaveThreatList() { return hasList; }
    Unit* GetCurrentVictim() { return victim; }
    float GetThreat(Unit*) { return threat; }
    auto const& GetThreatenedByMeList() { return references; }
};
class Unit
{
public:
    virtual ~Unit() = default;
    virtual Player* ToPlayer() { return nullptr; }
    virtual Creature* ToCreature() { return nullptr; }
    Unit* victim = nullptr;
    ThreatManager threats;
    std::set<Unit*> attackers;
    bool alive = true, inWorld = true, controlled = false, combat = true;
    float x = 0, health = 100;
    int map = 1;
    ObjectGuid guid;
    DataMap CustomData;
    Unit* GetVictim() { return victim; }
    ThreatManager& GetThreatMgr() { return threats; }
    auto const& getAttackers() { return attackers; }
    bool IsAlive() { return alive; }
    bool IsInWorld() { return inWorld; }
    bool IsInCombat() { return combat; }
    bool IsDuringRemoveFromWorld() { return false; }
    bool IsCreature() { return ToCreature() != nullptr; }
    bool IsControlledByPlayer() { return controlled; }
    int GetMap() { return map; }
    ObjectGuid GetGUID() { return guid; }
    float GetHealthPct() { return health; }
    float GetDistance(Unit* other) { return std::abs(x - other->x); }
    bool IsWithinDistInMap(Unit* other, float radius) { return map == other->map && GetDistance(other) <= radius; }
    bool IsWithinMeleeRange(Unit* other) { return GetDistance(other) <= 5; }
};
class Creature : public Unit
{
public:
    Creature* ToCreature() override { return this; }
    bool boss = false, worldBoss = false;
    bool IsDungeonBoss() { return boss; }
    bool isWorldBoss() { return boss || worldBoss; }
};
struct WorldSession
{
    bool bot = false;
    bool IsBot() { return bot; }
};
struct CombatManager
{
    bool pvp = false;
    bool HasPvPCombat() { return pvp; }
};
class Player : public Unit
{
public:
    Player* ToPlayer() override { return this; }
    Group* group = nullptr;
    bool tankSpec = true, tankRole = true;
    WorldSession session;
    CombatManager combatManager;
    bool battleground = false, arena = false;
    bool InBattleground() { return battleground; }
    bool InArena() { return arena; }
    CombatManager& GetCombatManager() { return combatManager; }
    Group* GetGroup() { return group; }
    WorldSession* GetSession() { return &session; }
    std::string GetName() { return "player" + std::to_string(guid.id); }
};
namespace ObjectAccessor
{
    std::map<ObjectGuid, Player*> players;
    Player* FindPlayer(ObjectGuid guid)
    {
        auto it = players.find(guid);
        return it == players.end() ? nullptr : it->second;
    }
}
struct GroupReference
{
    Player* player;
    GroupReference* following = nullptr;
    Player* GetSource() { return player; }
    GroupReference* next() { return following; }
};
struct CharacterDatabasePreparedStatement
{
    uint32 data[2]{};
    void SetData(int i, uint32 value) { data[i] = value; }
};
struct Transaction
{
    std::vector<std::unique_ptr<CharacterDatabasePreparedStatement>> statements;
    void Append(CharacterDatabasePreparedStatement* statement) { statements.emplace_back(statement); }
};
using CharacterDatabaseTransaction = std::shared_ptr<Transaction>;
struct Database
{
    std::map<uint32, uint8> saved;
    int commits = 0;
    size_t rows = 0;
    CharacterDatabaseTransaction BeginTransaction() { return std::make_shared<Transaction>(); }
    CharacterDatabasePreparedStatement* GetPreparedStatement(int) { return new CharacterDatabasePreparedStatement(); }
    void Execute(CharacterDatabasePreparedStatement* stmt)
    {
        auto transaction = BeginTransaction();
        transaction->Append(stmt);
        CommitTransaction(transaction);
    }
    void CommitTransaction(CharacterDatabaseTransaction const& tx)
    {
        ++commits;
        rows = tx->statements.size();
        for (auto const& stmt : tx->statements)
            saved[stmt->data[1]] = stmt->data[0];
    }
} CharacterDatabase;
class Group
{
public:
    struct MemberSlot { ObjectGuid guid; uint8 flags; };
    using MemberSlotList = std::list<MemberSlot>;
    using member_citerator = MemberSlotList::const_iterator;
    using member_witerator = MemberSlotList::iterator;
    MemberSlotList m_memberSlots;
    std::vector<GroupReference> references;
    ObjectGuid icons[8]{};
    ObjectGuid leader;
    bool raid = true;
    int updates = 0, iconWrites = 0;
    ObjectGuid guid{10};
    Group(std::initializer_list<Player*> members)
    {
        references.reserve(members.size());
        for (Player* player : members)
        {
            player->group = this;
            m_memberSlots.push_back({player->guid, 0});
            references.push_back({player, nullptr});
            ObjectAccessor::players[player->guid] = player;
        }
        for (size_t i = 1; i < references.size(); ++i)
            references[i - 1].following = &references[i];
    }
    bool isRaidGroup() { return raid; }
    MemberSlotList const& GetMemberSlots() { return m_memberSlots; }
    GroupReference* GetFirstMember() { return references.empty() ? nullptr : &references[0]; }
    member_witerator _getMemberWSlot(ObjectGuid id)
    {
        return std::find_if(m_memberSlots.begin(), m_memberSlots.end(), [id](auto const& s) { return s.guid == id; });
    }
    bool IsLeader(ObjectGuid id) { return id == leader; }
    bool IsAssistant(ObjectGuid id)
    {
        auto slot = _getMemberWSlot(id);
        return slot != m_memberSlots.end() && (slot->flags & MEMBER_FLAG_ASSISTANT);
    }
    ObjectGuid GetGUID() { return guid; }
    ObjectGuid GetTargetIcon(uint8 index) { return icons[index]; }
    void SetTargetIcon(uint8 index, ObjectGuid, ObjectGuid target)
    {
        if (!target.IsEmpty())
            for (auto& icon : icons)
                if (icon == target)
                    icon.Clear();
        icons[index] = target;
        ++iconWrites;
    }
    void ToggleGroupMemberFlag(member_witerator slot, uint8 flag, bool apply)
    {
        if (apply) slot->flags |= flag;
        else slot->flags &= ~flag;
    }
    void SendUpdate() { ++updates; }
    void SetGroupMemberFlag(ObjectGuid guid, bool apply, GroupMemberFlags flag);
    void RemoveUniqueGroupMemberFlag(GroupMemberFlags flag);
};
class EventMap
{
    uint32 remaining = 0;
    bool due = false;
public:
    void Update(uint32 diff)
    {
        if (remaining && diff >= remaining) { remaining = 0; due = true; }
        else if (remaining) remaining -= diff;
    }
    uint32 ExecuteEvent() { bool result = due; due = false; return result ? 1 : 0; }
    void ScheduleEvent(int, Milliseconds time) { remaining = time.count(); due = false; }
};
template<class T> struct Value
{
    T value{};
    T Get() { return value; }
};
struct Context
{
    std::map<std::string, Value<Unit*>> units;
    template<class T> Value<T>* GetValue(std::string name);
};
template<> Value<Unit*>* Context::GetValue<Unit*>(std::string name) { return &units[name]; }
class PlayerbotAI
{
public:
    Player* bot;
    Player* master;
    Context context;
    struct { bool scheduled = true; } raidCombat;
    std::vector<std::string> messages;
    std::vector<Unit*> attackers;
    PlayerbotAI(Player* player, Player* owner) : bot(player), master(owner) {}
    Player* GetBot() { return bot; }
    Player* GetMaster() { return master; }
    bool IsRealPlayer() { return bot == master; }
    Context* GetAiObjectContext() { return &context; }
    bool IsValidUnit(Unit* unit) { return unit && unit->IsInWorld(); }
    static bool IsTank(Player* player, bool bySpec = false) { return bySpec ? player->tankSpec : player->tankRole; }
    static ObjectGuid GetMainTankGuid(Group* group);
    static bool IsMainTank(Player* player);
    static bool IsExplicitMainTank(Player* player);
    static bool IsOffTank(Player* player);
    bool HasAggro(Unit* target);
    bool TellMasterNoFacing(std::string text) { messages.push_back(text); return true; }
    bool TellError(std::string text) { messages.push_back(text); return true; }
};
struct HasAggroValue
{
    PlayerbotAI* botAI;
    Player* bot;
    Unit* target;
    Unit* GetTarget() { return target; }
    bool Calculate();
};
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
    uint32 ChainTarget = 0;
    bool IsTargetingArea() const { return area; }
    float CalcRadius(Unit*) const { return radius; }
};
class SpellInfo
{
public:
    uint32 Id = 0;
    bool attackMe = false, tauntAura = false, persistent = false, positive = false;
    SpellEffectInfo Effects[3]{};
    bool HasEffect(int effect) const { return effect == SPELL_EFFECT_ATTACK_ME ? attackMe : persistent; }
    bool HasAura(int) const { return tauntAura; }
    bool IsPositive() const { return positive; }
    bool IsTargetingArea() const
    {
        return std::any_of(std::begin(Effects), std::end(Effects), [](auto const& effect) { return effect.area; });
    }
};
struct Event
{
    std::string param;
    Player* owner;
    std::string getParam() { return param; }
    Player* getOwner() { return owner; }
};
class Action
{
public:
    PlayerbotAI* botAI;
    Player* bot;
    Action(PlayerbotAI* ai, std::string) : botAI(ai), bot(ai->GetBot()) {}
    virtual ~Action() = default;
    virtual bool Execute(Event) { return false; }
};

class AttackAction : public Action
{
public:
    explicit AttackAction(PlayerbotAI* ai) : Action(ai, "attack") {}
    Unit* target = nullptr;
    unsigned attacks = 0;
    Unit* GetTarget() { return target; }
    bool Attack(Unit*) { ++attacks; return true; }
    bool Execute(Event) override;
};

// PRODUCTION_CODE

void TestCoveredTargetAssistance()
{
    Player red, ari, healer;
    red.guid = 3001; ari.guid = 3002; healer.guid = 3003;
    ari.session.bot = true;
    healer.tankSpec = healer.tankRole = false;
    Group group{&red, &ari, &healer};
    group.leader = red.guid;
    PlayerbotAI ai(&ari, &red);
    TankTargetValue selector(&ai);
    AttackAction attack(&ai);
    Creature held, own, loose, coveredAdd;
    held.guid = 4001; own.guid = 4002; loose.guid = 4003; coveredAdd.guid = 4004;
    held.victim = coveredAdd.victim = &red;
    own.victim = &ari;
    loose.victim = &healer;
    SpellInfo taunt; taunt.Id = 355; taunt.attackMe = true;
    SpellInfo defense; defense.Id = 31789;
    SpellInfo damage; damage.Id = 20271;
    red.attackers.insert(&held);

    for (bool mainTank : {false, true})
    {
        group.SetGroupMemberFlag(mainTank ? ari.guid : red.guid, true, MEMBER_FLAG_MAINTANK);
        assert(TankModes::GetMode(&ai) == (mainTank ? TankModes::Mode::MainTank : TankModes::Mode::OffTank));
        for (bool boss : {false, true})
        {
            held.boss = boss;
            ai.context.units["current target"].value = nullptr;
            ai.attackers = {&held};
            selector.icon = nullptr;
            // Reported regression: the sole enemy belongs to the other tank; do not idle.
            assert(selector.Calculate() == &held);
            assert(TankModes::CanAttack(&ai, &held));
            assert(!TankModes::CanAcquire(&ai, &held));
            attack.target = selector.Calculate();
            unsigned before = attack.attacks;
            assert(attack.Execute({}));
            assert(attack.attacks == before + 1);
            assert(!TankModes::SuppressAutomaticSpell(&ai, &damage, &held));
            assert(TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
            assert(TankModes::SuppressAutomaticSpell(&ai, &defense, &red));
            HasAggroValue aggro{&ai, &ari, &held};
            assert(aggro.Calculate()); // Tank-assist trigger may switch away if an add appears.

            // Both iteration orders and a marked covered boss must prefer actual tank work.
            for (bool reverse : {false, true})
            {
                selector.icon = &held;
                ai.attackers = reverse ? std::vector<Unit*>{&own, &held} : std::vector<Unit*>{&held, &own};
                assert(selector.Calculate() == &own);
                ai.context.units["current target"].value = &held;
                ai.attackers.push_back(&loose);
                assert(selector.Calculate() == &loose);
                ai.attackers = {&held};
                selector.icon = &loose;
                assert(selector.Calculate() == &loose); // Available marked add beats fallback boss.
                selector.icon = &held;
                assert(selector.Calculate() == &held);
                ai.attackers.clear();
                assert(selector.Calculate() == &held); // Covered RTI fallback remains damage-only.
            }

            // Preserve moon CC exclusion on ordinary attacker selection.
            selector.icon = nullptr;
            ai.attackers = {&held};
            group.icons[4] = held.guid;
            assert(!selector.Calculate());
            group.icons[4].Clear();
            held.alive = false;
            assert(!selector.Calculate());
            held.alive = true;

            // Ownership can change between selection and execution; do not require a stale owner.
            held.victim = &healer;
            assert(TankModes::CanAcquire(&ai, &held));
            assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
            assert(attack.Execute({}));
            held.victim = &red;
            red.alive = false;
            assert(TankModes::CanAcquire(&ai, &held));
            red.alive = true;
            held.victim = nullptr;
            held.threats.victim = &red;
            assert(TankModes::CanAttack(&ai, &held));
            assert(!TankModes::CanAcquire(&ai, &held));
            held.threats.victim = nullptr;
            held.victim = &red;
        }

        // All enemies covered: retain the assist target rather than bouncing between them.
        held.boss = false;
        ai.context.units["current target"].value = &coveredAdd;
        for (bool reverse : {false, true})
        {
            ai.attackers = reverse ? std::vector<Unit*>{&coveredAdd, &held} :
                std::vector<Unit*>{&held, &coveredAdd};
            assert(selector.Calculate() == &coveredAdd);
        }
    }

    // Damage-assist permission must never bypass an MT's low-health pause.
    ai.attackers = {&held};
    selector.icon = &held;
    ari.health = 39;
    assert(TankModes::IsPaused(&ai));
    assert(!selector.Calculate());
    assert(!TankModes::CanAttack(&ai, &held));
    assert(!attack.Execute({}));
    assert(TankModes::SuppressAutomaticSpell(&ai, &damage, &held));
    ai.attackers.push_back(&own);
    assert(selector.Calculate() == &own);
    attack.target = &own;
    assert(attack.Execute({}));
    attack.target = &held;
    ai.raidCombat.scheduled = false;
    assert(attack.Execute({})); // Explicit orders remain deliberate overrides.
    assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
    ai.raidCombat.scheduled = true;
    ari.health = 50;
    assert(!TankModes::CanAttack(&ai, &held));
    ari.health = 65;
    ai.attackers = {&held};
    assert(selector.Calculate() == &held);
    assert(attack.Execute({}));
    assert(TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));

    attack.target = nullptr;
    assert(!attack.Execute({}));
    attack.target = &held;
    held.inWorld = false;
    assert(!attack.Execute({}));
    held.inWorld = true;
    ari.battleground = true;
    assert(TankModes::CanAcquire(&ai, &held) && TankModes::CanAttack(&ai, &held));
    ari.battleground = false;
    ari.arena = true;
    assert(TankModes::CanAcquire(&ai, &held) && TankModes::CanAttack(&ai, &held));
    ari.arena = false;
    ari.tankRole = false;
    assert(TankModes::CanAcquire(&ai, &held) && TankModes::CanAttack(&ai, &held));
    ari.tankRole = true;
    ari.group = nullptr;
    assert(TankModes::CanAcquire(&ai, &held) && TankModes::CanAttack(&ai, &held));
    std::cout << "Covered-target damage assistance, priority and acquisition separation passed\n";
}

int main()
{
    Player red, ari, healer, third;
    red.guid = 1501; ari.guid = 142; healer.guid = 815; third.guid = 42;
    ari.session.bot = true;
    healer.tankSpec = healer.tankRole = false;
    Group group{&red, &ari, &healer, &third};
    group.leader = red.guid;
    PlayerbotAI ai(&ari, &red);
    TankModeAction command(&ai);
    auto flags = [&](Player& p) -> uint8& { return group._getMemberWSlot(p.guid)->flags; };

    // Reproduce stale duplicate MT records and preserve unrelated role bits while repairing them.
    flags(red) = MEMBER_FLAG_MAINTANK | MEMBER_FLAG_ASSISTANT;
    flags(ari) = MEMBER_FLAG_MAINTANK;
    CharacterDatabase.saved[red.guid.id] = flags(red);
    CharacterDatabase.saved[ari.guid.id] = flags(ari);
    assert(command.Execute({" MT ", &red}));
    assert(PlayerbotAI::IsMainTank(&ari));
    assert(PlayerbotAI::IsOffTank(&red));
    assert(flags(red) == MEMBER_FLAG_ASSISTANT && flags(ari) == MEMBER_FLAG_MAINTANK);
    assert(CharacterDatabase.saved[red.guid.id] == MEMBER_FLAG_ASSISTANT);
    assert(CharacterDatabase.saved[ari.guid.id] == MEMBER_FLAG_MAINTANK);
    assert(CharacterDatabase.rows == 2);
    assert(group.icons[TankModes::MainTankIcon] == ari.guid);
    assert(group.icons[TankModes::OffTankIcon] == red.guid);
    int writes = group.updates;
    assert(command.Execute({"status", &red}));
    assert(group.updates == writes);
    assert(!command.Execute({"garbage", &red}));
    assert(!command.Execute({"offtank", &healer})); // No role authority.
    assert(group.updates == writes);
    assert(command.Execute({"offtank", &red}));
    assert(PlayerbotAI::IsMainTank(&red) && PlayerbotAI::IsOffTank(&ari));
    assert(group.icons[TankModes::MainTankIcon] == red.guid);
    assert(group.icons[TankModes::OffTankIcon] == ari.guid);

    Creature held, own, loose, boss;
    held.guid = 1001; own.guid = 1002; loose.guid = 1003; boss.guid = 1004;
    held.victim = &red; own.victim = &ari; loose.victim = &healer; boss.victim = &healer;
    boss.boss = true;
    ThreatReference ownRef{&own}, heldRef{&held}, looseRef{&loose};
    ari.threats.references[own.guid] = &ownRef;
    red.threats.references[held.guid] = &heldRef;
    healer.threats.references[loose.guid] = &looseRef;
    red.attackers.insert(&held); healer.attackers.insert(&loose);
    TankTargetValue selector(&ai);
    ai.attackers = {&held, &own, &loose};
    selector.icon = &held;
    assert(selector.Calculate() == &loose); // OT ignores our MT's mob, even with a focus icon.
    HasAggroValue aggro{&ai, &ari, &held};
    assert(aggro.Calculate());
    assert(ai.HasAggro(&held));
    held.victim = nullptr; held.threats.victim = &red;
    assert(!TankModes::CanAcquire(&ai, &held));
    held.threats.victim = nullptr;
    assert(TankModes::CanAcquire(&ai, &held));
    held.victim = &red;
    red.alive = false;
    assert(TankModes::CanAcquire(&ai, &held));
    assert(!aggro.Calculate());
    red.alive = true;

    assert(command.Execute({"mt", &red}));
    selector.icon = &loose;
    ai.attackers = {&held, &own, &loose, &boss};
    assert(selector.Calculate() == &boss); // MT prefers available boss, not a DPS focus add.
    Creature lieutenant;
    lieutenant.guid = 1005; lieutenant.worldBoss = true; lieutenant.victim = &healer;
    ai.attackers.push_back(&lieutenant);
    boss.victim = &ari;
    selector.icon = &lieutenant;
    assert(selector.Calculate() == &boss); // Keep the primary boss ahead of boss-ranked council adds.
    ai.attackers.pop_back();
    selector.icon = &loose;
    boss.victim = &red;
    assert(selector.Calculate() == &loose); // Even MT leaves the human OT's boss alone.
    boss.victim = &healer;
    ai.attackers = {&held, &own, &loose};
    ai.context.units["current target"].value = &own;
    assert(selector.Calculate() == &loose); // MT may collect more while healthy.
    size_t const beforeHelp = ai.messages.size();
    ari.health = 39;
    TankModes::Update(&ai, 1);
    assert(TankModes::IsPaused(&ai));
    assert(ai.messages.size() == beforeHelp + 1);
    assert(ai.messages.back().find("Please take some enemies") != std::string::npos);
    assert(selector.Calculate() == &own); // Low health: maintain, do not collect the loose add.
    assert(!TankModes::CanAcquire(&ai, &boss));
    own.victim = &healer;
    assert(TankModes::CanAcquire(&ai, &own)); // Reacquire our own peeled mob.
    own.victim = &red;
    assert(!TankModes::CanAcquire(&ai, &own)); // A co-tank's handoff is never an automatic reclaim.
    own.victim = &ari;
    ari.health = 50;
    TankModes::Update(&ai, 1000);
    assert(TankModes::IsPaused(&ai));
    assert(ai.messages.size() == beforeHelp + 1);
    TankModes::Update(&ai, 29000);
    assert(ai.messages.size() == beforeHelp + 2);
    ari.health = 65;
    TankModes::Update(&ai, 1);
    assert(!TankModes::IsPaused(&ai));
    assert(selector.Calculate() == &loose);
    ari.health = 40;
    assert(!TankModes::IsPaused(&ai)); // Boundary: BELOW 40 pauses.
    ari.health = 39;
    assert(TankModes::IsPaused(&ai));

    SpellInfo taunt; taunt.Id = 355; taunt.attackMe = true;
    SpellInfo defense; defense.Id = 31789;
    SpellInfo damage; damage.Id = 20271;
    SpellInfo aoe; aoe.Id = 26573; aoe.persistent = true; aoe.Effects[0] = {true, 10, 0};
    SpellInfo cleave; cleave.Id = 53595; cleave.Effects[0].ChainTarget = 3;
    SpellInfo defensive; defensive.Id = 20925; defensive.positive = true; defensive.Effects[0].area = true;
    assert(TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
    assert(TankModes::SuppressAutomaticSpell(&ai, &taunt, &loose)); // Paused MT cannot acquire it.
    assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &own));
    assert(TankModes::SuppressAutomaticSpell(&ai, &defense, &red));
    assert(TankModes::SuppressAutomaticSpell(&ai, &defense, &healer));
    assert(TankModes::SuppressAutomaticSpell(&ai, &aoe, &own));
    assert(TankModes::SuppressAutomaticSpell(&ai, &cleave, &own));
    assert(!TankModes::SuppressAutomaticSpell(&ai, &defensive, &ari));
    assert(!TankModes::SuppressAutomaticSpell(&ai, &damage, &own));
    assert(TankModes::SuppressAutomaticSpell(&ai, &damage, &loose));
    assert(TankModes::SuppressAutomaticSpell(&ai, &damage, &held));
    ai.raidCombat.scheduled = false;
    assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
    assert(!TankModes::SuppressAutomaticSpell(&ai, &defense, &red)); // Deliberate casts remain overrides.
    ai.raidCombat.scheduled = true;
    assert(command.Execute({"offtank", &red}));
    assert(!TankModes::IsPaused(&ai)); // MT latch does not leak into OT.
    assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &loose));
    assert(!TankModes::SuppressAutomaticSpell(&ai, &aoe, &own));
    assert(!TankModes::SuppressAutomaticSpell(&ai, &damage, &held)); // Normal damage threat is not a taunt ban.
    assert(!TankModes::SuppressAutomaticSpell(&ai, &defense, &healer));

    // UI assignment and commands share the same authoritative role and one-shot marker update.
    group.SetGroupMemberFlag(ari.guid, true, MEMBER_FLAG_MAINTANK);
    TankModes::Update(&ai, 1);
    assert(TankModes::GetMode(&ai) == TankModes::Mode::MainTank);
    assert(group.icons[TankModes::MainTankIcon] == ari.guid);
    assert(group.icons[TankModes::OffTankIcon] == red.guid);
    group.SetTargetIcon(TankModes::MainTankIcon, red.guid, boss.guid);
    writes = group.iconWrites;
    TankModes::Update(&ai, 1);
    assert(group.iconWrites == writes); // Do not fight an encounter that reclaims an icon.
    assert(command.Execute({"mt", &red}));
    assert(group.icons[TankModes::MainTankIcon] == boss.guid); // Enemy mark wins.
    group.SetTargetIcon(7, red.guid, ari.guid);
    group.icons[TankModes::MainTankIcon].Clear();
    assert(command.Execute({"mt", &red}));
    assert(group.icons[7] == ari.guid && group.icons[TankModes::MainTankIcon].IsEmpty());

    // An offline MT assignment must not cause per-update clearing/reapplication of icons.
    group.SetTargetIcon(7, red.guid, ObjectGuid::Empty);
    group.SetGroupMemberFlag(third.guid, true, MEMBER_FLAG_MAINTANK);
    ObjectAccessor::players.erase(third.guid);
    TankModes::Update(&ai, 1);
    writes = group.iconWrites;
    TankModes::Update(&ai, 1);
    assert(group.iconWrites == writes);
    ObjectAccessor::players[third.guid] = &third;
    group.SetGroupMemberFlag(ari.guid, true, MEMBER_FLAG_MAINTANK);
    TankModes::Update(&ai, 1);

    // Health safety also prevents automatic low-health prepulls, without idle help spam.
    ari.combat = false;
    size_t beforeIdle = ai.messages.size();
    TankModes::Update(&ai, 30000);
    assert(TankModes::IsPaused(&ai));
    assert(!TankModes::CanAcquire(&ai, &loose));
    assert(ai.messages.size() == beforeIdle);
    ari.alive = false;
    assert(!TankModes::IsPaused(&ai));
    ari.alive = true;
    ari.combat = true;

    // Party support and persistence: removing a non-owner does not clear the real MT.
    group.raid = false;
    group.SetGroupMemberFlag(red.guid, true, MEMBER_FLAG_MAINTANK);
    group.SetGroupMemberFlag(ari.guid, false, MEMBER_FLAG_MAINTANK);
    assert(PlayerbotAI::IsMainTank(&red));
    assert(CharacterDatabase.saved[red.guid.id] & MEMBER_FLAG_MAINTANK);
    writes = group.updates;
    group.SetGroupMemberFlag(healer.guid, true, MEMBER_FLAG_ASSISTANT);
    assert(group.updates == writes); // Raid-only assistant permissions remain raid-only.
    group.SetGroupMemberFlag(ObjectGuid(999999), true, MEMBER_FLAG_MAINTANK);
    assert(group.updates == writes);
    assert(command.Execute({"mt", &red}));
    assert(PlayerbotAI::IsMainTank(&ari));
    for (auto const& slot : group.m_memberSlots)
        assert(!CharacterDatabase.saved.count(slot.guid.id) || slot.flags == CharacterDatabase.saved[slot.guid.id]);

    // New group/reset ownership and unrelated/PvP/non-tank cases.
    held.controlled = true;
    assert(TankModes::CanAcquire(&ai, &held));
    held.controlled = false;
    ari.group = nullptr;
    assert(TankModes::GetMode(&ai) == TankModes::Mode::Inactive);
    TankModes::Update(&ai, 1);
    assert(!TankModes::IsPaused(&ai));
    assert(TankModes::CanAcquire(&ai, &held));
    ari.group = &group;
    red.tankSpec = red.tankRole = false;
    third.tankSpec = third.tankRole = false;
    assert(!command.Execute({"offtank", &red})); // Cannot invent a second tank.
    assert(PlayerbotAI::IsMainTank(&ari));
    ari.tankRole = false;
    assert(!command.Execute({"mt", &red}));
    assert(TankModes::GetMode(&ai) == TankModes::Mode::Inactive);
    ari.tankRole = red.tankRole = red.tankSpec = true;
    red.health = ari.health = 100;
    for (auto& slot : group.m_memberSlots)
        slot.flags = 0;
    assert(PlayerbotAI::IsMainTank(&red));
    assert(PlayerbotAI::IsOffTank(&ari)); // Same subgroup is no longer interpreted as two MTs.
    assert(TankModes::Status(&ai).find("automatic tank order") != std::string::npos);
    assert(!command.Execute({"mt", &third}));
    PlayerbotAI humanAI(&red, nullptr);
    assert(TankModes::GetMode(&humanAI) == TankModes::Mode::Inactive);
    ari.battleground = true;
    assert(!command.Execute({"mt", &red}));
    assert(TankModes::GetMode(&ai) == TankModes::Mode::Inactive);
    assert(!TankModes::SuppressAutomaticSpell(&ai, &taunt, &held));
    ari.battleground = false;
    ari.combatManager.pvp = true;
    assert(TankModes::GetMode(&ai) == TankModes::Mode::Inactive);
    ari.combatManager.pvp = false;
    std::cout << "Tank modes, commands, ownership, health/help, markers and flag persistence passed\n";
    TestCoveredTargetAssistance();
}
