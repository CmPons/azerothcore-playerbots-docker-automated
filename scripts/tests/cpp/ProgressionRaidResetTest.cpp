#include "ProgressionRaidReset.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>

using namespace ProgressionRaidReset;

static std::string Save(uint32_t map, std::vector<uint32_t> const& states)
{
    auto layout = GetLayout(map);
    assert(layout);
    std::string data;
    for (char c : layout->header)
        data += std::string(1, c) + ' ';
    for (uint32_t state : states)
        data += std::to_string(state) + ' ';
    return data;
}

int main()
{
    constexpr int64_t now = 20000 * Day + 20 * 3600;
    constexpr int64_t tomorrow = 20001 * Day + 4 * 3600;
    auto emptyBwl = Save(469, std::vector<uint32_t>(8, 0));
    auto sixBwl = Save(469, {3, 3, 3, 3, 3, 3, 0, 0});
    auto clearBwl = Save(469, std::vector<uint32_t>(8, 3));

    // Production InstanceSave exposes atomic stage/deadline fields to readers
    // on other map workers. Exercise their conversions used by the policy bridge.
    std::atomic<Stage> storedStage{Stage::Fresh};
    std::atomic<int64_t> storedDeadline{tomorrow};
    State snapshot{storedStage, storedDeadline};
    assert(snapshot.stage == Stage::Fresh && uint64_t(storedDeadline) == uint64_t(tomorrow));

    State fresh = Advance({}, ReadProgress(469, emptyBwl), now, 3, 4);
    assert(fresh.stage == Stage::Fresh && fresh.deadline == tomorrow);
    State partial = Advance(fresh, ReadProgress(469, sixBwl), now, 3, 4);
    assert(partial.stage == Stage::Progression && partial.deadline == now + 3 * Day);
    for (int64_t later : {now + 60, now + Day, now + 4 * Day})
    {
        assert(Advance(partial, ReadProgress(469, sixBwl), later, 3, 4) == partial);
        assert(Advance(partial, ReadProgress(469, emptyBwl), later, 3, 4) == partial);
        assert(Advance(partial, std::nullopt, later, 3, 4) == partial);
    }
    // Fully clearing supersedes the progression deadline, even near its expiry.
    State clear = Advance(partial, ReadProgress(469, clearBwl), now, 3, 4);
    assert(clear.stage == Stage::Cleared && clear.deadline == tomorrow);
    assert(Advance(clear, ReadProgress(469, clearBwl), now + 5 * Day, 3, 4) == clear);
    auto lateClear = Advance(partial, ReadProgress(469, clearBwl), partial.deadline - 1, 3, 4);
    assert(lateClear.deadline > partial.deadline);
    assert(NextDailyReset(tomorrow - 1, 4) == tomorrow);
    assert(NextDailyReset(tomorrow, 4) == tomorrow + Day);
    assert(NextDailyReset(tomorrow + 1, 4) == tomorrow + Day);
    assert(NextDailyReset(20000 * Day, 0) == 20001 * Day);
    assert(ExtendedDeadline(partial, 3, 4) == partial.deadline + 3 * Day);
    assert(ExtendedDeadline(clear, 3, 4) == tomorrow + Day);

    // Legacy saves get one grace window, not one per restart.
    assert(Advance({}, ReadProgress(469, sixBwl), now, 3, 4) == partial);
    auto unknown = Advance({}, std::nullopt, now, 3, 4);
    assert(unknown == partial);
    assert(Advance(unknown, std::nullopt, now + Day, 3, 4) == unknown);
    assert(Advance({}, ReadProgress(469, clearBwl), now, 3, 4) == clear);

    for (uint32_t map : {249, 309, 409, 469, 509, 531})
    {
        auto layout = GetLayout(map);
        std::vector<uint32_t> states(layout->slots, 0);
        for (uint32_t i = 0; i < layout->slots; ++i)
            if (layout->requiredMask & (uint32_t(1) << i))
                states[i] = 3;
        if (map == 531)
            states[0] = 5; // AQ40's unused slot must not prevent a clear.
        auto progress = ReadProgress(map, Save(map, states) + "14309 14310 ");
        assert(progress && progress->started && progress->cleared);
        // Leaving ANY required encounter up must prevent fast resets.
        for (uint32_t i = 0; i < layout->slots; ++i)
            if (layout->requiredMask & (uint32_t(1) << i))
            {
                states[i] = 0;
                assert(!ReadProgress(map, Save(map, states))->cleared);
                states[i] = 3;
            }
    }
    // Final-boss-only clears are insufficient, including Hakkar and C'Thun.
    assert(!ReadProgress(309, Save(309, {0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0}))->cleared);
    assert(!ReadProgress(531, Save(531, {5, 0, 0, 0, 0, 0, 0, 0, 0, 3}))->cleared);
    // ZG adds do not start a progression clock; optional summons do.
    assert(!ReadProgress(309, Save(309, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3}))->started);
    assert(ReadProgress(309, Save(309, {0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0}))->started);
    // An unsuccessful Razorgore kill may set a kill-credit bit, but no DONE state.
    assert(!ReadProgress(469, emptyBwl)->started);
    for (uint32_t state : {1, 2, 4, 5})
        assert(!ReadProgress(469, Save(469, std::vector<uint32_t>(8, state)))->cleared);

    for (auto const& data : {"", "B W L 3", "M C 3 3 3 3 3 3 3 3", "B W L 3 3 3 3 3 3 3 99"})
        assert(!ReadProgress(469, data));
    assert(!GetLayout(229));
    assert(!GetLayout(533));
    assert(!ReadProgress(229, clearBwl));
    assert(WarningStage(3601) == 0 && WarningStage(3600) == 1);
    assert(WarningStage(900) == 2 && WarningStage(300) == 3);
    assert(WarningStage(60) == 4 && WarningStage(0) == 4);
    // Independent copies: MC can reset tomorrow while BWL retains three days.
    assert(clear.deadline < partial.deadline);
    std::cout << "Progression raid reset policy tests passed\n";
}
