#include "RaidThreatControl.h"
#include <cassert>
#include <iostream>
#include <thread>
using namespace ai::threat::control;
int main()
{
    Settings defaults;
    assert(defaults.aoe == 90 && defaults.target == 80 && defaults.boss == 70);
    assert(defaults.healing == HealingMode::Emergency && defaults.emergencyHealth == 30);
    for (auto text : {"", "status", " reset ", "aoe 1", "aoe 100", "target 90", "boss 70",
                     "healing normal", "healing exempt", "healing emergency 30"})
        assert(Parse(text));
    for (auto text : {"status x", "reset all", "aoe", "aoe 0", "aoe 101", "aoe -1", "aoe +1",
                     "aoe 50.5", "aoe 90%", "aoe 90junk", "aoe 9999999999999999999999", "aoe 90 80",
                     "healing", "healing emergency", "healing emergency 0", "healing emergency 101",
                     "healing normal 30", "healing exempt 30", "healing nonsense", "nonsense 90"})
        assert(!Parse(text));
    for (unsigned value = 1; value <= 100; ++value)
    {
        auto command = Parse("aoe " + std::to_string(value));
        assert(command && command->value == value);
    }
    assert(HealingBypass(defaults, true, 29.9f) == Reason::Emergency);
    assert(!HealingBypass(defaults, true, 30));
    assert(!HealingBypass(defaults, false, 1));
    defaults.healing = HealingMode::Normal;
    assert(!HealingBypass(defaults, true, 1));
    defaults.healing = HealingMode::Exempt;
    assert(HealingBypass(defaults, true, 100) == Reason::Exempt);
    assert(!HealingBypass(defaults, false, 1));
    defaults.healing = HealingMode::Emergency;

    Store store;
    constexpr std::uint64_t a = (std::uint64_t(550) << 32) | 6510;
    constexpr std::uint64_t b = (std::uint64_t(550) << 32) | 6511;
    constexpr std::uint64_t c = (std::uint64_t(548) << 32) | 6510;
    store.Apply(a, *Parse("target 90"), defaults);
    assert(store.Get(a, defaults).target == 90);
    assert(store.Get(b, defaults).target == 80 && store.Get(c, defaults).target == 80);
    store.Apply(a, *Parse("healing normal"), defaults);
    store.Apply(a, *Parse("boss 85"), defaults);
    store.Apply(a, *Parse("aoe 95"), defaults);
    assert(store.Get(a, defaults).healing == HealingMode::Normal);
    assert(store.Get(a, defaults).boss == 85 && store.Get(a, defaults).aoe == 95);
    assert(store.Get(a, defaults).target == 90);
    assert(store.Read(a, defaults).overridden);

    Decision decision;
    decision.reason = Reason::Aoe;
    decision.settings = store.Get(a, defaults);
    decision.time = 1;
    store.Record(a, 815, decision);
    decision.reason = Reason::Allowed;
    decision.time = 2;
    store.Record(a, 815, decision);
    auto observation = store.Read(a, defaults).observations.at(815);
    assert(observation.latest.reason == Reason::Allowed);
    assert(observation.lastBlock && observation.lastBlock->reason == Reason::Aoe);
    store.Apply(a, *Parse("healing emergency 35"), defaults);
    assert(store.Get(a, defaults).emergencyHealth == 35);
    assert(store.Read(a, defaults).observations.empty());
    store.Record(a, 815, decision); // In-flight old policy decision is rejected.
    assert(store.Read(a, defaults).observations.empty());
    decision.settings = store.Get(a, defaults);
    store.Record(a, 815, decision);
    assert(store.Read(a, defaults).observations.size() == 1);
    store.Apply(a, *Parse("reset"), defaults);
    assert(!store.Read(a, defaults).overridden && store.Get(a, defaults).target == 80);
    store.Record(a, 815, decision); // Reset also invalidates old decisions.
    assert(store.Read(a, defaults).observations.empty());
    decision.settings = store.Get(a, defaults);
    for (unsigned i = 1; i <= 85; ++i)
    {
        decision.time = i;
        store.Record(a, i, decision);
    }
    assert(store.Read(a, defaults).observations.size() == 80);
    assert(!store.Read(a, defaults).observations.count(1));
    store.Erase(a); // Same key reused after unload starts at defaults without diagnostics.
    assert(!store.Read(a, defaults).overridden && store.Read(a, defaults).observations.empty());
    store.Apply(0, *Parse("aoe 1"), defaults);
    store.Record(0, 1, decision);
    assert(store.Read(0, defaults).observations.empty() && !store.Read(0, defaults).overridden);

    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 4; ++i)
        threads.emplace_back([&, i]
        {
            for (unsigned j = 0; j < 1500; ++j)
            {
                store.Apply(a, *Parse(j % 2 ? "reset" : "aoe 65"), defaults);
                Decision sample;
                sample.settings = store.Get(a, defaults);
                assert(sample.settings.aoe == 90 || sample.settings.aoe == 65);
                store.Record(a, i + 1, sample);
                auto snapshot = store.Read(a, defaults);
                for (auto const& [guid, seen] : snapshot.observations)
                {
                    (void)guid;
                    assert(seen.latest.settings.generation == snapshot.settings.generation);
                }
            }
        });
    for (auto& thread : threads) thread.join();
    std::cout << "Policy parser, lifecycle, diagnostics and concurrency passed\n";
}
