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

    for (uint32_t map : {249, 309, 409, 469, 509, 531, 532, 534, 544, 548, 550,
                         564, 565, 568, 580, 533, 603, 615, 616, 624, 631, 724})
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
        State adopted = Advance({}, progress, now, 3, 4);
        assert(adopted.stage == Stage::Cleared && adopted.deadline == tomorrow);
        assert(Advance(adopted, progress, now + Day, 3, 4) == adopted);
        // Leaving ANY required encounter up must prevent fast resets.
        for (uint32_t i = 0; i < layout->slots; ++i)
            if (layout->requiredMask & (uint32_t(1) << i))
            {
                states[i] = 0;
                assert(!ReadProgress(map, Save(map, states))->cleared);
                states[i] = 3;
            }
        // Each slot independently: optional bosses start progression, internal flags do not.
        // Difficulty is intentionally not an input: copies/10/25/heroic keep independent State.
        for (uint32_t i = 0; i < layout->slots; ++i)
        {
            std::vector<uint32_t> single(layout->slots, 0);
            single[i] = 3;
            auto one = ReadProgress(map, Save(map, single));
            bool const encounter = (layout->encounterMask & (uint32_t(1) << i)) != 0;
            bool const onlyRequired = layout->requiredMask == (uint32_t(1) << i);
            assert(one && one->started == encounter && one->cleared == onlyRequired);
            State state = Advance({}, one, now, 3, 4);
            assert(state.stage == (onlyRequired ? Stage::Cleared : encounter ? Stage::Progression : Stage::Fresh));
            assert(state.deadline == (encounter && !onlyRequired ? now + 3 * Day : tomorrow));
            assert(Advance(state, one, now + Day, 3, 4) == state);
        }
        // Neither optional nor internal slots need DONE; interrupted/unused slots are accepted.
        for (uint32_t state : {0, 1, 2, 4, 5})
        {
            std::vector<uint32_t> values(layout->slots, state);
            assert(!ReadProgress(map, Save(map, values))->started);
            for (uint32_t i = 0; i < layout->slots; ++i)
                if (layout->requiredMask & (uint32_t(1) << i))
                    values[i] = 3;
            assert(ReadProgress(map, Save(map, values))->cleared);
        }
        // Bad/truncated states must fail closed, not classify as empty or fully cleared.
        std::vector<uint32_t> invalid(layout->slots, 3);
        invalid.back() = 99;
        assert(!ReadProgress(map, Save(map, invalid)));
        invalid.pop_back();
        assert(!ReadProgress(map, Save(map, invalid)));
        assert(!ReadProgress(map, "WRONG 3 3 3"));
    }
    // Trial's scalar is NOT a boss-state array. 3 is an intro checkpoint, 9 is pre-Anub.
    for (uint32_t checkpoint : {0, 1, 2, 3, 4, 6, 8, 9, 10})
        for (std::string const suffix : {"", " 50 1 1", " 0 0 0"})
        {
            auto progress = ReadProgress(649, "T C R " + std::to_string(checkpoint) + suffix);
            assert(progress && progress->started == (checkpoint >= 2) && progress->cleared == (checkpoint == 10));
            auto state = Advance({}, progress, now, 3, 4);
            assert(state.deadline == (checkpoint >= 2 && checkpoint != 10 ? now + 3 * Day : tomorrow));
            assert(Advance(state, progress, now + Day, 3, 4) == state);
        }
    for (auto const* invalid : {"T C R", "T C R -1", "T C R 5", "T C R 7", "T C R 11", "T C R nope"})
        assert(!ReadProgress(649, invalid));
    // Representative SSC save: first three dead, last three alive. Never a daily clear.
    auto ssc = ReadProgress(548, "S S 3 3 3 0 0 0");
    assert(ssc && ssc->started && !ssc->cleared);
    auto sscState = Advance({}, ssc, now, 3, 4);
    assert(sscState.stage == Stage::Progression && sscState.deadline == now + 3 * Day);
    assert(ExtendedDeadline(sscState, 3, 4) == now + 6 * Day);
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
    for (uint32_t map : {0, 530, 571, 585, 595, 608, 632, 650, 658, 668, 9999})
        assert(!GetLayout(map));
    assert(!ReadProgress(229, clearBwl));
    assert(WarningStage(3601) == 0 && WarningStage(3600) == 1);
    assert(WarningStage(900) == 2 && WarningStage(300) == 3);
    assert(WarningStage(60) == 4 && WarningStage(0) == 4);
    // Independent copies: MC can reset tomorrow while BWL retains three days.
    assert(clear.deadline < partial.deadline);
    std::cout << "Progression raid reset policy tests passed\n";
}
