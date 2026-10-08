// Offline framework around the production equipment fixture and injected TK methods.
// No server, database, config, inventory or encounter is mutated by this test.
constexpr uint32 TEMPEST_KEEP_MAP_ID = 550;
constexpr uint32 EQUIPMENT_SLOT_RANGED = 17;
constexpr uint32 INVENTORY_SLOT_ITEM_START = 23, INVENTORY_SLOT_ITEM_END = 39;
constexpr uint32 ITEM_CLASS_PROJECTILE = 6;
constexpr uint32 UNIT_FLAG_NON_ATTACKABLE = 2, SPELL_PERMANENT_FEIGN_DEATH = 29266;
constexpr uint32 PHASE_SINGLE_ADVISOR = 1;
struct UnitAI { virtual ~UnitAI() = default; };
struct boss_kaelthas : UnitAI
{
    uint32 phase = PHASE_SINGLE_ADVISOR;
    uint32 GetPhase() const { return phase; }
};
struct Map;
struct Unit
{
    UnitAI* ai = nullptr;
    Map* map = nullptr;
    uint32 health = 100, maximum = 100;
    bool nonAttackable = false, feignDeath = false;
    virtual ~Unit() = default;
    UnitAI* GetAI() const { return ai; }
    Map* GetMap() const { return map; }
    uint32 GetHealth() const { return health; }
    uint32 GetMaxHealth() const { return maximum; }
    bool HasUnitFlag(uint32) const { return nonAttackable; }
    bool HasAura(uint32) const { return feignDeath; }
};
using Creature = Unit;
struct Map
{
    uint32 id = TEMPEST_KEEP_MAP_ID, instance = 6510;
    std::map<uint32, Creature*> creatures;
    auto const& GetCreatureBySpawnIdStore() const { return creatures; }
    uint32 GetInstanceId() const { return instance; }
} scene;
std::map<std::string, Unit*> targets;
uint32 clockMs = 0;
uint32 getMSTime() { return clockMs; }
uint32 getMSTimeDiff(uint32 old, uint32 now) { return now - old; }

// AFTER_EQUIPMENT_METHODS
#undef AI_VALUE2
#define AI_VALUE2(type, name, qualifier) botAI->Find(qualifier)
Guid Item::GetOwnerGUID() const { return owner->GetGUID(); }
InventoryResult Player::CanEquipItem(uint8 slot, uint16& dest, Item* item, bool swap) const
{
    ++coreEquipChecks;
    PlayerbotAI ai{const_cast<Player*>(this)};
    return ai.CanEquipItem(slot, dest, item, swap);
}
struct ItemUpgradeValue : ItemUsageValue
{
    explicit ItemUpgradeValue(PlayerbotAI* ai) : ItemUsageValue{ai->bot, ai} {}
    ItemUsage CalculateForItem(Item*);
    ItemUsage CalculateUsage(ItemTemplate const*, int32, Item*);
    ItemUsage QueryItemUsageForAmmo(ItemTemplate const*) const { return ITEM_USAGE_NONE; }
};
struct Action
{
    PlayerbotAI* botAI;
    Player* bot;
    Action(PlayerbotAI* ai, std::string const&) : botAI(ai), bot(ai->bot) {}
    virtual ~Action() = default;
    virtual bool isUseful() { return true; }
    virtual bool Execute(Event) { return false; }
};
struct Trigger : Action
{
    using Action::Action;
    virtual bool IsActive() { return false; }
};
struct AttackAction : Action { using Action::Action; };
struct CastSpellAction : Action { using Action::Action; };
struct CastHealingSpellAction : CastSpellAction { using CastSpellAction::CastSpellAction; };
struct KaelthasSunstriderMisdirectAdvisorsToTanksAction : Action { using Action::Action; };
struct KaelthasSunstriderManageAdvisorDpsTimerAction : Action
{
    using Action::Action;
    bool Execute(Event) override;
};
struct KaelthasSunstriderWaitForDpsMultiplier
{
    PlayerbotAI* botAI;
    Player* bot;
    float GetValue(Action*);
};
namespace TempestKeepHelpers
{
std::map<uint32, time_t> advisorDpsWaitTimer;
Player* GetCapernianTank(Player*) { return nullptr; }
// HELPER_DECLARATION
}
using namespace TempestKeepHelpers;
struct EquipUpgradeAction : Action
{
    using Action::Action;
    void EquipInventoryUpgrades()
    {
        ++botAI->equipAttempts;
        if (botAI->equipEffect) botAI->equipEffect();
    }
    bool Execute(Event) override;
};
bool PlayerbotAI::DoSpecificAction(std::string const& name, Event event, bool)
{
    assert(name == "equip upgrade");
    EquipUpgradeAction equip(this, name);
    return equip.Execute(event);
}
// RECOVERY_CLASSES
// RECOVERY_METHODS

struct Setup
{
    Player bot;
    PlayerbotAI ai{&bot};
    boss_kaelthas bossAI;
    Creature kael, sanguinar;
    Item main, ranged, knife, spare;
    KaelthasSunstriderReequipGearAction recovery{&ai};
    KaelthasSunstriderLegendaryWeaponsWereLostTrigger trigger{&ai};
    Setup() : main{nullptr, &bot}, ranged{nullptr, &bot}, knife{nullptr, &bot}, spare{nullptr, &bot}
    {
        clockMs = 0;
        targets.clear(); advisorDpsWaitTimer.clear();
        scene = Map{};
        kael.ai = &bossAI; kael.map = &scene;
        scene.creatures[158218] = &kael;
        targets["kael'thas sunstrider"] = &kael;
        targets["lord sanguinar"] = &sanguinar;
        bot.dualWield = true;
        bot.classMask = 4; // Hunter.
        templates.clear();
        auto proto = [](uint32 id, uint32 inventory, uint8 slot, uint32 subclass, float score)
        {
            ItemTemplate item;
            item.ItemId = id; item.Class = ITEM_CLASS_WEAPON;
            item.AllowableClass = 4;
            item.proficiency = 0;
            item.InventoryType = inventory; item.slot = slot; item.SubClass = subclass;
            item.score = score;
            templates[id] = item;
        };
        proto(29924, INVTYPE_WEAPON, EQUIPMENT_SLOT_MAINHAND, 0, 220);
        proto(34892, 26, EQUIPMENT_SLOT_RANGED, 18, 300);
        proto(7005, INVTYPE_WEAPON, EQUIPMENT_SLOT_MAINHAND, ITEM_SUBCLASS_WEAPON_MISC, 0);
        proto(999, INVTYPE_WEAPON, EQUIPMENT_SLOT_MAINHAND, 0, 100);
        main.proto = &templates.at(29924); ranged.proto = &templates.at(34892);
        knife.proto = &templates.at(7005); spare.proto = &templates.at(999);
        bot.equipped[EQUIPMENT_SLOT_MAINHAND] = &main;
        bot.equipped[EQUIPMENT_SLOT_RANGED] = &ranged;
        knife.inventorySlot = 23; spare.inventorySlot = 24;
        bot.bags[{INVENTORY_SLOT_BAG_0, 23}] = &knife;
    }
    void addSpare() { bot.bags[{INVENTORY_SLOT_BAG_0, 24}] = &spare; }
};

void knifeCase()
{
    Setup f;
    // This knife is legal in the empty OH, but actual owned-item valuation rejects it.
    uint16 dest = 0;
    assert(f.ai.CanEquipItem(EQUIPMENT_SLOT_OFFHAND, dest, &f.knife, false) == EQUIP_ERR_OK);
    ItemUpgradeValue usage(&f.ai);
    assert(usage.CalculateForItem(&f.knife) == ITEM_USAGE_NONE);
    assert(!f.trigger.IsActive());
    assert(!f.recovery.Execute({}) && f.ai.equipAttempts == 0);
    assert(f.bot.coreEquipChecks == 0); // Probing uses PB's side-effect-free validator.
}

void recoveryCases()
{
    Setup f;
    f.addSpare();
    assert(f.trigger.IsActive());
    f.ai.equipEffect = [&] { f.bot.equipped[EQUIPMENT_SLOT_OFFHAND] = &f.spare; };
    assert(f.recovery.Execute({}));
    assert(f.ai.equipAttempts == 1 && !f.trigger.IsActive());
    assert(!f.recovery.isUseful());
    clockMs = 5000;
    assert(f.recovery.isUseful() && !f.recovery.Execute({}));
    assert(f.ai.equipAttempts == 1);
    f.bot.equipped.erase(EQUIPMENT_SLOT_OFFHAND);
    // Missing main/ranged weapons after temporary gear expiry still recover normally.
    f.bot.equipped.erase(EQUIPMENT_SLOT_MAINHAND);
    f.ai.equipEffect = [&] { f.bot.equipped[EQUIPMENT_SLOT_MAINHAND] = &f.spare; };
    assert(f.trigger.IsActive() && f.recovery.Execute({}));
    clockMs += 5000;
    f.bot.equipped.erase(EQUIPMENT_SLOT_RANGED);
    f.ranged.inventorySlot = 25;
    f.bot.bags[{INVENTORY_SLOT_BAG_0, 25}] = &f.ranged;
    f.ai.equipEffect = [&] { f.bot.equipped[EQUIPMENT_SLOT_RANGED] = &f.ranged; };
    assert(f.trigger.IsActive() && f.recovery.Execute({}));
}

void rejectionCases()
{
    Setup f;
    f.addSpare();
    assert(f.trigger.IsActive());
    f.bot.alive = false; assert(!f.trigger.IsActive() && !f.recovery.Execute({})); f.bot.alive = true;
    scene.id = 0; assert(!f.trigger.IsActive()); scene.id = TEMPEST_KEEP_MAP_ID;
    f.bot.map = nullptr; assert(!f.trigger.IsActive()); f.bot.map = &scene;
    scene.creatures.clear(); assert(!f.trigger.IsActive()); scene.creatures[158218] = &f.kael;
    f.bot.distance = 151; assert(!f.trigger.IsActive()); f.bot.distance = 150;
    assert(f.trigger.IsActive());
    f.bot.level = 69; assert(!f.trigger.IsActive()); f.bot.level = 70;
    f.bot.classMask = 1; assert(!f.trigger.IsActive()); f.bot.classMask = 4;
    f.bot.uniqueResult = EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE; assert(!f.trigger.IsActive());
    f.bot.uniqueResult = EQUIP_ERR_OK;
    scripts.equipAllowed = false; assert(!f.trigger.IsActive()); scripts.equipAllowed = true;
    f.bot.twoHand = true; assert(!f.trigger.IsActive()); f.bot.twoHand = false;
    Player stranger; stranger.identity = 99;
    f.spare.owner = &stranger; assert(!f.trigger.IsActive()); f.spare.owner = &f.bot;
    f.spare.inventorySlot = 26; assert(!f.trigger.IsActive()); f.spare.inventorySlot = 24;
    // The same candidate can be in an actual carried bag rather than the backpack.
    f.bot.bags.erase({INVENTORY_SLOT_BAG_0, 24});
    Item bag{&templates.at(999), &f.bot};
    f.bot.equipped[INVENTORY_SLOT_BAG_START] = &bag;
    f.spare.bag = INVENTORY_SLOT_BAG_START; f.spare.inventorySlot = 0;
    f.bot.bags[{INVENTORY_SLOT_BAG_START, 0}] = &f.spare;
    assert(f.trigger.IsActive());
}

void fairnessCase()
{
    Setup f;
    f.addSpare(); // Valuation succeeds, but an equip handler can still reject the actual swap.
    KaelthasSunstriderManageAdvisorDpsTimerAction timer(&f.ai, "timer");
    KaelthasSunstriderWaitForDpsMultiplier wait{&f.ai, &f.bot};
    AttackAction attack(&f.ai, "dps assist");
    CastHealingSpellAction heal(&f.ai, "heal");
    assert(wait.GetValue(&attack) == 0.0f && wait.GetValue(&heal) == 1.0f);
    unsigned commands = 0;
    // Minimal scheduler double: a successful high-priority action consumes this tick;
    // failure/usefulness rejection lets lower-priority bookkeeping or commands run.
    auto tick = [&]
    {
        if (f.trigger.IsActive() && f.recovery.isUseful() && f.recovery.Execute({})) return;
        if (timer.Execute({})) return;
        ++commands;
    };
    tick();
    assert(advisorDpsWaitTimer.count(scene.instance) == 1);
    f.sanguinar.health = 99;
    tick();
    assert(commands == 1 && f.ai.equipAttempts == 1);
    advisorDpsWaitTimer[scene.instance] -= 11;
    assert(wait.GetValue(&attack) == 1.0f);
}

void retryCase()
{
    Setup f;
    f.addSpare();
    clockMs = std::numeric_limits<uint32>::max() - 1000;
    assert(!f.recovery.Execute({})); // Child returns true, but there was no actual slot recovery.
    assert(f.ai.equipAttempts == 1 && !f.recovery.isUseful());
    Player other;
    PlayerbotAI otherAI{&other};
    KaelthasSunstriderReequipGearAction otherRecovery(&otherAI);
    assert(otherRecovery.isUseful()); // Backoff belongs to this bot's action, not the raid.
    assert(!f.recovery.Execute({}) && f.ai.equipAttempts == 1);
    clockMs += 4999; // Monotonic millisecond counter wraps.
    assert(!f.recovery.isUseful());
    ++clockMs;
    assert(!f.recovery.isUseful()); // Core's wrap helper is conservative by one millisecond.
    ++clockMs;
    assert(f.recovery.isUseful());
    f.ai.equipEffect = [&] { f.bot.equipped[EQUIPMENT_SLOT_OFFHAND] = &f.spare; };
    assert(f.recovery.Execute({}) && f.ai.equipAttempts == 2);
    assert(!f.trigger.IsActive());
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string scenario = argv[1];
    if (scenario == "knife" || scenario == "all") knifeCase();
    if (scenario == "fairness" || scenario == "all") fairnessCase();
    if (scenario == "retry" || scenario == "all") retryCase();
    if (scenario == "all") { recoveryCases(); rejectionCases(); }
    assert(Item::allocations == Item::removals);
    std::cout << "TK weapon recovery, no-op fairness and advisor gate checks passed\n";
}
