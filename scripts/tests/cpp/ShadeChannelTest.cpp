// Production channeler/sorcerer AI, channel predicates, current-spell admission and aura cleanup
// are injected. World objects, scheduling, spell effects and cancellation are controlled doubles.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using uint8 = uint8_t;
using uint32 = uint32_t;
using int32 = int32_t;
using namespace std::chrono_literals;
using Duration = std::chrono::milliseconds;
#define ASSERT assert

// NATIVE_ENUMS
constexpr uint32 POINT_MOTION_TYPE = 8;
constexpr uint32 EFFECT_1 = 1;

struct EffectInfo
{
    uint32 TriggerSpell = 0;
};
struct SpellInfo
{
    uint32 Id = 0;
    uint32 AttributesEx = 0;
    uint32 AttributesEx2 = 0;
    uint32 AttributesEx5 = 0;
    EffectInfo Effects[3];
    bool HasAttribute(SpellAttr5 attribute) const { return AttributesEx5 & attribute; }
    bool HasAttribute(SpellAttr2 attribute) const { return AttributesEx2 & attribute; }
    bool IsChanneled() const;
    bool IsActionAllowedChannel() const;
};
// NATIVE_SPELL_PREDICATES

SpellInfo channelInfo;
constexpr int SlowStackLimit = 99;
struct Aura
{
    int stacks = 0;
    unsigned removals = 0;
    void ModStackAmount(int delta)
    {
        if (delta < 0)
            ++removals;
        stacks = std::clamp(stacks + delta, 0, SlowStackLimit);
    }
};
struct Shade
{
    Aura slow;
    Aura* GetAura(uint32 id)
    {
        assert(id == 40520);
        return slow.stacks ? &slow : nullptr;
    }
};
struct AuraEffect {};
struct ChannelRemoval
{
    Shade* shade;
    Shade* GetTarget() const { return shade; }
    SpellInfo const* GetSpellInfo() const { return &channelInfo; }
    // NATIVE_CHANNEL_REMOVAL
};

struct Spell
{
    SpellInfo const* m_spellInfo;
    CurrentSpellTypes type;
    SpellState state = SPELL_STATE_CASTING;
    uint32 castTime = 0;
    bool referenced = false;
    Spell** m_selfContainer = nullptr;
    SpellInfo const* GetSpellInfo() const { return m_spellInfo; }
    CurrentSpellTypes GetCurrentContainer() const { return type; }
    uint32 GetCastTime() const { return castTime; }
    SpellState getState() const { return state; }
    void SetReferencedFromCurrent(bool value) { referenced = value; }
};
struct Unit
{
    std::array<Spell*, CURRENT_AUTOREPEAT_SPELL + 1> m_currentSpells{};
    bool m_AutoRepeatFirstCast = false;
    uint32 state = 0;
    Shade* shade;
    bool channelAura = false;
    explicit Unit(Shade* target) : shade(target) { }
    bool HasUnitState(uint32 value) const { return state & value; }
    void AddUnitState(uint32 value) { state |= value; }
    Spell* GetCurrentSpell(CurrentSpellTypes type) const { return m_currentSpells[type]; }
    void SetCurrentCastedSpell(Spell* spell);
    bool IsNonMeleeSpellCast(bool withDelayed, bool skipChanneled = false, bool skipAutorepeat = false,
                            bool isAutoshoot = false, bool skipInstant = true) const;
    void InterruptSpell(CurrentSpellTypes type, bool withDelayed = true, bool withInstant = true,
                        bool bySelf = false)
    {
        Spell* spell = m_currentSpells[type];
        if (!spell || (!withDelayed && spell->state == SPELL_STATE_DELAYED) ||
            (!withInstant && !spell->castTime && spell->state != SPELL_STATE_CASTING))
            return;
        // Core Spell::cancel(bySelf) deliberately retains a channel's owned aura on self replacement.
        // Effect creation/refresh is modeled in CastSpell below; the cleanup handler itself is native.
        if (type == CURRENT_CHANNELED_SPELL && channelAura && !bySelf)
        {
            ChannelRemoval{shade}.HandleEffectRemove(nullptr, AURA_EFFECT_HANDLE_REAL);
            channelAura = false;
        }
        spell->state = SPELL_STATE_FINISHED;
        spell->SetReferencedFromCurrent(false);
        m_currentSpells[type] = nullptr;
        state &= ~UNIT_STATE_CASTING;
    }
};
// NATIVE_CURRENT_SPELL
// NATIVE_ACTIVE_CAST

struct Creature : Unit
{
    using Unit::Unit;
    bool alive = true;
    unsigned submissions = 0;
    unsigned rejected = 0;
    unsigned rejectNext = 0;
    std::vector<std::unique_ptr<Spell>> storage;
    void CastSpell(Creature* target, uint32 id, bool triggered)
    {
        assert(target == this && id == 40401 && triggered && alive);
        ++submissions;
        if (rejectNext)
        {
            --rejectNext;
            ++rejected;
            return;
        }
        auto spell = std::make_unique<Spell>();
        spell->m_spellInfo = &channelInfo;
        spell->type = CURRENT_CHANNELED_SPELL;
        SetCurrentCastedSpell(spell.get());
        storage.push_back(std::move(spell));
        channelAura = true; // Same-caster 40401 refresh does not remove its existing aura.
        shade->slow.ModStackAmount(1); // Each cast's trigger effect applies shared 40520 again.
    }
    void Die()
    {
        InterruptSpell(CURRENT_CHANNELED_SPELL);
        alive = false;
    }
};

struct TaskContext;
struct Task
{
    Duration due;
    Duration interval;
    std::function<void(TaskContext)> callback;
};
struct TaskScheduler
{
    Duration now{0};
    std::vector<Task> tasks;
    template<class F> void Schedule(Duration delay, F callback)
    {
        tasks.push_back({now + delay, delay, callback});
    }
    void Update(uint32 diff);
};
struct TaskContext
{
    TaskScheduler* scheduler;
    Task task;
    void Repeat() { scheduler->Schedule(task.interval, task.callback); }
};
void TaskScheduler::Update(uint32 diff)
{
    now += Duration(diff);
    for (unsigned count = 0; count < 100; ++count)
    {
        auto next = std::min_element(tasks.begin(), tasks.end(), [](Task const& a, Task const& b)
        {
            return a.due < b.due;
        });
        if (next == tasks.end() || next->due > now)
            return;
        Task const task = *next;
        tasks.erase(next);
        task.callback(TaskContext{this, task});
    }
    assert(false);
}
struct NullCreatureAI
{
    Creature* me;
    explicit NullCreatureAI(Creature* creature) : me(creature) { }
    virtual ~NullCreatureAI() = default;
    virtual void Reset() { }
    virtual void UpdateAI(uint32) { }
    virtual void MovementInform(uint32, uint32) { }
};
// NATIVE_CHANNELER
// NATIVE_SORCERER

struct Pack
{
    Shade shade;
    std::vector<std::unique_ptr<Creature>> creatures;
    std::vector<std::unique_ptr<npc_ashtongue_channeler>> brains;
    explicit Pack(unsigned count)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            creatures.push_back(std::make_unique<Creature>(&shade));
            brains.push_back(std::make_unique<npc_ashtongue_channeler>(creatures.back().get()));
            brains.back()->Reset();
        }
    }
    void Tick(uint32 diff = 3600)
    {
        for (unsigned i = 0; i < brains.size(); ++i)
            if (creatures[i]->alive)
                brains[i]->UpdateAI(diff);
    }
    void KillAll()
    {
        for (auto& creature : creatures)
            creature->Die();
    }
};

int main(int argc, char** argv)
{
    // INSTALLED_SPELL_DATA
    assert(channelInfo.IsChanneled() && channelInfo.IsActionAllowedChannel());
    std::string const mode = argc > 1 ? argv[1] : "lifecycle";
    if (mode == "legacy")
    {
        Pack pack(3);
        for (unsigned i = 0; i < 40; ++i)
            pack.Tick();
        assert(pack.shade.slow.stacks == 99);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 96);
        std::cout << "Legacy guard: three repeatedly refreshed channels leave 96 orphan stacks\n";
        return 0;
    }
    if (mode == "lifecycle")
    {
        Pack pack(6);
        pack.Tick(3599);
        assert(pack.shade.slow.stacks == 0);
        pack.Tick(1);
        assert(pack.shade.slow.stacks == 6);
        for (unsigned i = 0; i < 100; ++i)
            pack.Tick();
        assert(pack.shade.slow.stacks == 6);
        for (auto& creature : pack.creatures)
        {
            assert(!creature->HasUnitState(UNIT_STATE_CASTING));
            assert(creature->IsNonMeleeSpellCast(false));
            assert(creature->submissions == 1);
        }
        for (unsigned i = 0; i < pack.creatures.size(); ++i)
        {
            pack.creatures[i]->Die();
            assert(pack.shade.slow.stacks == 5 - int(i));
            pack.Tick();
            assert(pack.shade.slow.stacks == 5 - int(i));
        }
        assert(pack.shade.slow.removals == 6);
    }
    else if (mode == "interrupt")
    {
        Pack pack(1);
        pack.Tick();
        auto& caster = *pack.creatures.front();
        for (unsigned i = 0; i < 25; ++i)
        {
            caster.InterruptSpell(CURRENT_CHANNELED_SPELL);
            assert(pack.shade.slow.stacks == 0);
            assert(!caster.IsNonMeleeSpellCast(false));
            pack.Tick();
            assert(pack.shade.slow.stacks == 1);
            pack.Tick();
            assert(pack.shade.slow.stacks == 1);
        }
        assert(caster.submissions == 26);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 0);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 0);
    }
    else if (mode == "retry")
    {
        Pack pack(1);
        auto& caster = *pack.creatures.front();
        caster.rejectNext = 1;
        pack.Tick();
        assert(pack.shade.slow.stacks == 0 && !caster.IsNonMeleeSpellCast(false));
        pack.Tick();
        assert(pack.shade.slow.stacks == 1 && caster.submissions == 2 && caster.rejected == 1);
        pack.Tick();
        assert(caster.submissions == 2);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 0);
    }
    else if (mode == "ordinary")
    {
        channelInfo.AttributesEx5 = 0;
        Pack pack(1);
        pack.Tick();
        assert(pack.creatures.front()->HasUnitState(UNIT_STATE_CASTING));
        for (unsigned i = 0; i < 10; ++i)
            pack.Tick();
        assert(pack.creatures.front()->submissions == 1);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 0);
    }
    else if (mode == "other-cast")
    {
        Pack pack(1);
        auto& caster = *pack.creatures.front();
        SpellInfo other;
        other.Id = 123;
        Spell generic{&other, CURRENT_GENERIC_SPELL, SPELL_STATE_PREPARING, 1500};
        caster.SetCurrentCastedSpell(&generic);
        pack.Tick();
        assert(caster.submissions == 0);
        caster.InterruptSpell(CURRENT_GENERIC_SPELL);
        pack.Tick();
        assert(caster.submissions == 1 && pack.shade.slow.stacks == 1);
        pack.KillAll();
    }
    else if (mode == "repull")
    {
        Pack pack(6);
        for (unsigned attempt = 0; attempt < 20; ++attempt)
        {
            if (attempt)
                for (unsigned i = 0; i < pack.creatures.size(); ++i)
                {
                    pack.creatures[i]->alive = true;
                    pack.brains[i]->Reset();
                }
            for (unsigned i = 0; i < 5; ++i)
                pack.Tick();
            assert(pack.shade.slow.stacks == 6);
            pack.KillAll();
            assert(pack.shade.slow.stacks == 0);
        }
        for (auto& creature : pack.creatures)
            assert(creature->submissions == 20);
    }
    else if (mode == "sorcerer")
    {
        Pack pack(6);
        pack.Tick();
        Creature sorcerer(&pack.shade);
        npc_ashtongue_sorcerer brain(&sorcerer);
        brain.MovementInform(POINT_MOTION_TYPE, POINT_ENGAGE);
        assert(pack.shade.slow.stacks == 7 && sorcerer.IsNonMeleeSpellCast(false));
        for (unsigned i = 0; i < 10; ++i)
        {
            brain.UpdateAI(3600);
            pack.Tick();
        }
        assert(sorcerer.submissions == 1 && pack.shade.slow.stacks == 7);
        pack.KillAll();
        assert(pack.shade.slow.stacks == 1);
        sorcerer.Die();
        assert(pack.shade.slow.stacks == 0);
    }
    else
        assert(false);
    std::cout << "PASS: " << mode << '\n';
}
