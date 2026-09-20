// Production strategy, trigger, action and registration bodies are inserted by the runner.
// API doubles isolate form/priority decisions; this is not a linked worldserver simulation.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
struct Event {};
constexpr int LIQUID_MAP_UNDER_WATER = 2;
struct Player
{
    std::set<uint32> auras;
    bool combat = false;
    struct Liquid { int Status = LIQUID_MAP_UNDER_WATER; } liquid;
    bool HasAura(uint32 id) const { return auras.contains(id); }
    bool GetAura(uint32 id) const { return HasAura(id); }
    bool IsInCombat() const { return combat; }
    Liquid GetLiquidData() const { return liquid; }
};
struct LastSpellCast { time_t timer = 0; };
struct Context
{
    LastSpellCast last;
    struct Value { LastSpellCast* p; LastSpellCast& Get() { return *p; } } value{&last};
    template<typename T> Value* GetValue(std::string const&) { return &value; }
};
struct PlayerbotAI
{
    Player* bot;
    int nearby = 1, health = 100, mana = 100, balance = 100;
    bool hasMana = true;
    Context context;
    static uint32 AuraId(std::string const& name)
    {
        if (name == "aquatic form") return 1066;
        if (name == "travel form") return 783;
        assert(name == "tree of life");
        return 33891;
    }
    int GetNearGroupMemberCount(float) const { return nearby; }
    Context* GetAiObjectContext() { return &context; }
    bool HasAura(std::string const& name, Player* p) { return p->HasAura(AuraId(name)); }
    void RemoveAura(std::string const& name) { bot->auras.erase(AuraId(name)); }
    template<typename T> T Value(std::string const& name, std::string const&) const
    {
        if (name == "health") return static_cast<T>(health);
        if (name == "mana") return static_cast<T>(mana);
        if (name == "has mana") return static_cast<T>(hasMana);
        assert(name == "balance");
        return static_cast<T>(balance);
    }
};
#define AI_VALUE(type, name) botAI->Value<type>(name, "")
#define AI_VALUE2(type, name, qualifier) botAI->Value<type>(name, qualifier)
struct Config
{
    float sightDistance = 100;
    int almostFullHealth = 95, highMana = 80, mediumMana = 40;
} sPlayerbotAIConfig;
struct Trigger
{
    PlayerbotAI* botAI;
    Player* bot;
    Trigger(PlayerbotAI* ai, std::string const&, int = 1) : botAI(ai), bot(ai->bot) {}
    virtual ~Trigger() = default;
    virtual bool IsActive() = 0;
};
class HealerShouldAttackTrigger : public Trigger
{
public:
    using Trigger::Trigger;
    bool IsActive() override;
};
class AquaticFormTrigger : public Trigger
{
public:
    using Trigger::Trigger;
    bool IsActive() override;
};
struct Action
{
    PlayerbotAI* botAI;
    Player* bot;
    Action(PlayerbotAI* ai, std::string const&) : botAI(ai), bot(ai->bot) {}
    virtual ~Action() = default;
    virtual bool isUseful() { return true; }
    virtual bool isPossible() { return true; }
    virtual bool Execute(Event) { return false; }
};
struct CastBuffSpellAction : Action { using Action::Action; };
// PRODUCTION_ACTION_CLASSES
struct NextAction
{
    std::string name;
    float relevance;
    NextAction(std::string n, float r) : name(n), relevance(r) {}
};
struct TriggerNode
{
    std::string name;
    std::vector<NextAction> actions;
    TriggerNode(std::string n, std::vector<NextAction> a) : name(n), actions(a) {}
};
struct DruidHealerDpsStrategy { void InitTriggers(std::vector<TriggerNode*>&); };
struct GenericDruidStrategy { void InitTriggers(std::vector<TriggerNode*>&) {} };
struct RestoDruidStrategy : GenericDruidStrategy { void InitTriggers(std::vector<TriggerNode*>&); };
// PRODUCTION_METHODS
using Factory = Action* (*)(PlayerbotAI*);
std::map<std::string, Factory> Factories()
{
    std::map<std::string, Factory> creators;
    // PRODUCTION_REGISTRATION
    return creators;
}
int main()
{
    Player bot;
    PlayerbotAI ai{&bot, 1, 100, 100, 100, true, {}};
    HealerShouldAttackTrigger attack(&ai, "healer should attack");
    AquaticFormTrigger swim(&ai, "aquatic form");
    auto creators = Factories();
    std::vector<TriggerNode*> nodes;
    DruidHealerDpsStrategy().InitTriggers(nodes);
    assert(nodes.size() == 1 && nodes.front()->name == "healer should attack");
    auto candidates = nodes.front()->actions;
    std::stable_sort(candidates.begin(), candidates.end(), [](auto const& a, auto const& b)
                     { return a.relevance > b.relevance; });
    auto nextUseful = [&]() -> std::string
    {
        if (!attack.IsActive()) return "";
        for (auto const& candidate : candidates)
        {
            if (creators.contains(candidate.name))
            {
                std::unique_ptr<Action> action(creators.at(candidate.name)(&ai));
                if (!action->isUseful()) continue;
                assert(action->isPossible());
                assert(action->Execute(Event{}));
                return candidate.name;
            }
            return candidate.name; // Damage candidate: do not pretend to run the core spell engine.
        }
        return "";
    };

    // Reproduce swimming -> target chosen -> no actual aggro yet -> caster spell.
    assert(swim.IsActive());
    bot.auras = {1066, 9885};
    assert(nextUseful() == "cancel aquatic form");
    assert((bot.auras == std::set<uint32>{9885})); // Mark of the Wild is untouched.
    assert(nextUseful() == "moonfire");
    bot.combat = true;
    assert(!swim.IsActive());
    bot.combat = false; // The eel dies. Returning to seal while underwater is normal.
    assert(swim.IsActive());
    bot.auras.insert(1066);
    assert(nextUseful() == "cancel aquatic form"); // Next eel must not deadlock again.
    assert(nextUseful() == "moonfire");

    // Tree's existing offensive cancellation and land travel share the same priority gate.
    for (auto const& [id, name] : std::map<uint32, std::string>{{33891, "cancel tree form"},
                                                              {783, "cancel travel form"}})
    {
        bot.auras = {id};
        assert(nextUseful() == name);
        assert(bot.auras.empty());
        assert(nextUseful() == "moonfire");
    }
    // This must not inherit caster-form's >medium-health mana threshold.
    ai.mana = 20;
    bot.auras = {1066};
    assert(nextUseful() == "cancel aquatic form");
    assert(bot.auras.empty());
    ai.mana = 100;

    // Caster and compatible moonkin forms do not shift repeatedly; unrelated forms untouched.
    for (uint32 form : {0u, 24858u, 768u, 5487u})
    {
        bot.auras = form ? std::set<uint32>{form} : std::set<uint32>{};
        auto before = bot.auras;
        assert(nextUseful() == "moonfire");
        assert(bot.auras == before);
    }
    // Party healing/mana gates must not be bypassed just to remove a swim form.
    ai.nearby = 3;
    ai.health = 40;
    bot.auras = {1066};
    assert(nextUseful().empty());
    assert(bot.HasAura(1066));
    ai.health = 100; ai.mana = 10;
    assert(nextUseful().empty());
    ai.mana = 100;
    assert(nextUseful() == "cancel aquatic form");
    bot.auras = {33891};
    ai.context.last.timer = time(nullptr);
    assert(nextUseful().empty()); // Preserve existing five-second Tree healing grace.
    ai.context.last.timer -= 10;
    assert(nextUseful() == "cancel tree form");

    // Actual restoration healing actions outrank every offensive form cancellation.
    std::vector<TriggerNode*> healing;
    RestoDruidStrategy().InitTriggers(healing);
    for (auto* node : healing)
    {
        if (node->name.find("party member") == 0)
            for (auto const& action : node->actions)
                assert(action.relevance > candidates.front().relevance);
        delete node;
    }
    for (auto* node : nodes) delete node;
    std::cout << "druid healer form transitions and healing gates passed\n";
}
