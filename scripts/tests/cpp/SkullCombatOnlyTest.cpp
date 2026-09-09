// Game/AI doubles. The Python runner appends unmodified production method bodies.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
using int32 = int32_t;
struct ObjectGuid
{
    uint64_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool IsEmpty() const { return id == 0; }
    bool operator==(ObjectGuid const&) const = default;
};
using GuidVector = std::vector<ObjectGuid>;
struct Unit
{
    ObjectGuid guid{42};
    bool dead = false, world = true, valid = true, combat = false;
    float distance = 10;
    ObjectGuid GetGUID() const { return guid; }
    bool isDead() const { return dead; }
    bool IsInWorld() const { return world; }
    bool IsInCombat() const { return combat; }
    bool IsPlayer() const { return false; }
    int GetMapId() const { return 469; }
};
struct Group
{
    std::array<ObjectGuid, 8> icons{};
    ObjectGuid GetTargetIcon(int index) const { return icons.at(index); }
};
struct Duel { Unit* Opponent = nullptr; };
struct Player : Unit
{
    Group* group = nullptr;
    Duel* duel = nullptr;
    bool los = true;
    Group* GetGroup() const { return group; }
    bool IsWithinLOSInMap(Unit*) const { return los; }
    bool InArena() const { return false; }
};
template<class T> struct Value
{
    T value{};
    T Get() const { return value; }
    void Set(T v) { value = v; }
};
struct Context
{
    template<class T> Value<T>* GetValue(std::string const& name)
    {
        static std::map<std::string, Value<T>> values;
        return &values[name];
    }
};
enum { ALL_ACTIVITY, STRATEGY_TYPE_HEAL, BOT_STATE_COMBAT, BOT_STATE_NON_COMBAT };
struct PlayerbotAI
{
    Player* bot = nullptr;
    Player* master = nullptr;
    Context* context = nullptr;
    Unit* unit = nullptr;
    bool heal = false, engineCombat = true;
    Player* GetBot() { return bot; }
    Player* GetMaster() { return master; }
    Unit* GetUnit(ObjectGuid guid) { return unit && guid == unit->guid ? unit : nullptr; }
    Context* GetAiObjectContext() { return context; }
    bool AllowActivity(int) { return true; }
    bool ContainsStrategy(int) { return heal; }
    int GetState() { return engineCombat ? BOT_STATE_COMBAT : BOT_STATE_NON_COMBAT; }
    void TellError(char const*) {}
};
struct ServerFacade
{
    static ServerFacade& instance() { static ServerFacade facade; return facade; }
    bool IsDistanceGreaterThan(float a, float b) { return a > b; }
    float GetDistance2d(Player*, Unit* unit) { return unit->distance; }
};
struct { float sightDistance = 45; } sPlayerbotAIConfig;
#define AI_VALUE(T, name) (botAI->GetAiObjectContext()->GetValue<T>(name)->Get())
struct RtiTargetValue
{
    Player* bot;
    PlayerbotAI* botAI;
    std::string type = "rti";
    static constexpr int skullIndex = 7;
    static int32 GetRtiIndex(std::string const rti);
    Unit* Calculate();
};
struct ObjectGuidListCalculatedValue
{
    bool cached = false;
    GuidVector value;
    int calculations = 0;
    virtual ~ObjectGuidListCalculatedValue() = default;
    virtual GuidVector Calculate() = 0;
    virtual GuidVector Get()
    {
        if (!cached)
        {
            ++calculations;
            value = Calculate();
            cached = true;
        }
        return value;
    }
    void Reset() { cached = false; }
};
struct AttackersValue : ObjectGuidListCalculatedValue
{
    AttackersValue(Player* bot, PlayerbotAI* ai) : bot(bot), botAI(ai) {}
    Player* bot;
    PlayerbotAI* botAI;
    bool wasInCombat = false;
    std::vector<Unit*> realGroupAttackers;
    GuidVector Get() override;
    GuidVector Calculate() override;
    void AddAttackersOf(Player*, std::unordered_set<Unit*>&) {}
    void AddAttackersOf(Group*, std::unordered_set<Unit*>& targets)
    {
        targets.insert(realGroupAttackers.begin(), realGroupAttackers.end());
    }
    void RemoveNonThreating(std::unordered_set<Unit*>&) {}
    static bool IsValidTarget(Unit* unit, Player*) { return unit->valid && !unit->dead; }
};
struct FindTargetStrategy
{
    PlayerbotAI* botAI;
    bool IsHighPriority(Unit* attacker);
};
struct Event {};
struct AttackRtiTargetAction
{
    Player* bot;
    PlayerbotAI* botAI;
    Context* context;
    int attacks = 0;
    bool Attack(Unit*) { ++attacks; return true; }
    bool Execute(Event);
    bool isUseful();
};
struct PullRtiTargetAction
{
    Player* bot;
    PlayerbotAI* botAI;
    Unit* GetPullTarget(Event);
};

// PRODUCTION_METHODS

int main()
{
    Group group;
    Player bot, master;
    Unit target;
    Context context;
    PlayerbotAI ai{&bot, &master, &context, &target};
    bot.group = &group;
    group.icons[7] = target.guid;
    RtiTargetValue rti{&bot, &ai};
    AttackersValue attackers{&bot, &ai};
    FindTargetStrategy strategy{&ai};
    AttackRtiTargetAction attack{&bot, &ai, &context};
    PullRtiTargetAction pull{&bot, &ai};
    context.GetValue<std::string>("rti")->Set("skull");

    // A pre-pull skull is not an attacker, target, priority, or executable attack.
    // Master/target/AI engine combat must not substitute for the bot's real flag.
    master.combat = target.combat = ai.engineCombat = true;
    assert(attackers.Get().empty());
    assert(attackers.Get().empty() && attackers.calculations == 1); // cache stays enabled
    assert(rti.Calculate() == nullptr);
    assert(!strategy.IsHighPriority(&target));
    assert(!attack.isUseful());
    context.GetValue<Unit*>("rti target")->Set(&target); // simulate stale cached result
    assert(!attack.Execute({}) && attack.attacks == 0);
    assert(context.GetValue<GuidVector>("prioritized targets")->Get().empty());
    assert(context.GetValue<ObjectGuid>("pull target")->Get().IsEmpty());

    // Explicit pull rti retains its independent direct-icon lookup before combat.
    context.GetValue<Unit*>("rti target")->Set(nullptr);
    assert(pull.GetPullTarget({}) == &target);

    bot.combat = true;
    assert(attackers.Get() == GuidVector{target.guid} && attackers.calculations == 2);
    assert(rti.Calculate() == &target);
    assert(strategy.IsHighPriority(&target));
    assert(attack.isUseful());
    assert(attack.Execute({}) && attack.attacks == 1); // direct fallback still works in combat
    assert(context.GetValue<GuidVector>("prioritized targets")->Get() == GuidVector{target.guid});

    // On leaving combat, the marker immediately loses its automatic effect again.
    context.GetValue<GuidVector>("prioritized targets")->Set({});
    bot.combat = false;
    assert(attackers.Get().empty() && attackers.calculations == 3 && rti.Calculate() == nullptr);
    assert(!strategy.IsHighPriority(&target) && !attack.Execute({}));

    // Genuine group assistance and intentional priority orders are not disabled.
    attackers.realGroupAttackers = {&target};
    assert(attackers.Calculate() == GuidVector{target.guid});
    attackers.realGroupAttackers.clear();
    context.GetValue<GuidVector>("prioritized targets")->Set({target.guid});
    assert(attackers.Calculate() == GuidVector{target.guid});
    assert(strategy.IsHighPriority(&target));
    context.GetValue<GuidVector>("prioritized targets")->Set({});

    // Other assigned icons and CC semantics are unchanged.
    group.icons[4] = target.guid;
    context.GetValue<std::string>("rti")->Set("moon");
    assert(rti.Calculate() == &target && attack.isUseful());
    assert(attack.Execute({}));
    rti.type = "rti cc";
    context.GetValue<std::string>("rti cc")->Set("skull");
    assert(rti.Calculate() == &target);
    rti.type = "rti";
    context.GetValue<std::string>("rti")->Set("skull");
    bot.combat = true;

    // Existing validity, LOS and range checks still apply after combat starts.
    target.dead = true;
    assert(rti.Calculate() == nullptr);
    target.dead = false;
    bot.los = false;
    assert(rti.Calculate() == nullptr);
    bot.los = true;
    target.distance = 100;
    assert(rti.Calculate() == nullptr);
    target.distance = 10;
    ai.heal = true;
    assert(!attack.isUseful());
    bot.group = nullptr;
    assert(rti.Calculate() == nullptr);
    std::cout << "Skull combat-only production-method tests passed\n";
}
