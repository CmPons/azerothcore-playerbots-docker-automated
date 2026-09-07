#include "../src/RaidScalingState.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    RaidScaleSettings defaults;
    defaults.originalPlayers = 40;
    defaults.targetPlayers = 10;
    defaults.bossHealth = defaults.trashHealth = 0.25f;
    defaults.fromDefault = true;
    auto bwl = RaidScaleKey(469, 199);
    auto mc = RaidScaleKey(409, 199);
    auto freshBwl = RaidScaleKey(469, 200);
    assert(bwl != mc && bwl != freshBwl);

    RaidScalingState state;
    assert(state.Initialize(bwl, defaults));
    assert(state.Get(bwl)->targetPlayers == 10);
    assert(state.Get(bwl)->fromDefault);
    assert(!state.Get(mc));

    RaidScaleSettings manual = defaults;
    manual.targetPlayers = 20;
    manual.fromDefault = false;
    state.Set(bwl, manual);
    assert(!state.Initialize(bwl, defaults));
    assert(state.Get(bwl)->targetPlayers == 20);
    assert(!state.Get(bwl)->fromDefault);

    // Readers receive copies; later tuning cannot mutate an outstanding snapshot.
    auto snapshot = state.Get(bwl);
    assert(state.Modify(bwl, [](RaidScaleSettings& settings) { settings.bossDamage = 0.3f; }));
    assert(snapshot->bossDamage == 1.0f);
    assert(state.Get(bwl)->bossDamage == 0.3f);

    state.Disable(bwl);
    assert(!state.Get(bwl));
    assert(!state.Initialize(bwl, defaults));
    assert(!state.Modify(bwl, [](RaidScaleSettings&) { assert(false); }));
    state.Set(bwl, manual);
    assert(state.Get(bwl)->targetPlayers == 20);

    // Fresh instance IDs are independent. Unloading/reloading restores the default,
    // not a stale manual override or off marker from the former loaded map.
    assert(state.Initialize(freshBwl, defaults));
    assert(state.Get(freshBwl)->targetPlayers == 10);
    state.Erase(bwl);
    assert(!state.Get(bwl));
    assert(state.Initialize(bwl, defaults));
    assert(state.Get(bwl)->targetPlayers == 10);
    RaidScalingState afterRestart;
    assert(afterRestart.Initialize(bwl, defaults));
    assert(afterRestart.Get(bwl)->targetPlayers == 10);

    std::atomic<bool> valid = true;
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 8; ++i)
        workers.emplace_back([&, i]
        {
            for (unsigned j = 0; j < 1000; ++j)
            {
                auto key = RaidScaleKey(469 + i, j % 25);
                state.Initialize(key, defaults);
                state.Set(key, manual);
                if (auto settings = state.Get(key))
                    if (settings->targetPlayers != 10 && settings->targetPlayers != 20)
                        valid = false;
                state.Disable(key);
                state.Erase(key);
                // Exercise reads concurrently with insertions/erasures on other maps.
                (void)state.Get(bwl);
            }
        });
    for (auto& worker : workers)
        worker.join();
    assert(valid);
    std::cout << "Raid scaling state lifecycle, override, snapshot and concurrency tests passed\n";
}
