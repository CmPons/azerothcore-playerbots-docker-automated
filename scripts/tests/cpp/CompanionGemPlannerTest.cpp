#include "CompanionGemPlanner.h"

#include <cassert>
#include <iostream>

using namespace CompanionGems;

int main()
{
    Option red{1, 101, 2, 10};
    Option yellow{2, 102, 4, 6};
    Option blue{3, 103, 8, 5};
    Option purple{4, 104, 10, 8};
    Option orange{5, 105, 6, 9};
    Option green{6, 106, 12, 7};
    std::vector<Option> pool{red, yellow, blue, purple, orange, green};
    std::vector<std::vector<Option>> sockets(3, pool);
    auto plain = Solve({}, sockets, {});
    assert(plain && plain->gems.size() == 3 && plain->score == 30);
    for (auto const& gem : plain->gems)
        assert(gem.item == 1);

    // Three colors for a BC meta. Hybrids must count as both component colors.
    std::vector<Requirement> allColors{{2, 5, 0, 1}, {3, 5, 0, 1}, {4, 5, 0, 1}};
    auto meta = Solve({}, sockets, allColors);
    assert(meta && meta->score == 27);
    Counts count{};
    for (auto const& gem : meta->gems)
        count = AddColor(count, gem.color);
    assert(Fits(count, allColors));

    // Relative requirements need exact, uncapped counts.
    std::vector<Requirement> moreBlue{{4, 3, 2, 0}};
    auto relative = Solve({}, sockets, moreBlue);
    assert(relative && relative->score == 24);
    count = {};
    for (auto const& gem : relative->gems)
        count = AddColor(count, gem.color);
    assert(count[3] > count[1]);
    assert(Solve({}, sockets, {{2, 2, 4, 0}})->score == relative->score);
    assert(!Solve({}, sockets, {{2, 5, 0, 4}}));
    assert(!Solve({}, {{red}}, {{4, 5, 0, 1}}));
    assert(Solve({0, 0, 0, 2}, {{red}}, {{4, 5, 0, 2}}));
    assert(Solve({0, 0, 0, 2}, {}, {{4, 5, 0, 2}}));
    assert(!Solve({}, {}, {{4, 5, 0, 2}}));

    // Strict less-than and threshold compression must agree with the full predicate.
    assert(Solve({}, sockets, {{2, 2, 0, 1}}));
    assert(!Solve({0, 1, 0, 0}, sockets, {{2, 2, 0, 1}}));
    assert(!Solve({}, sockets, {{0, 5, 0, 1}}));
    assert(!Solve({}, sockets, {{2, 99, 0, 1}}));
    assert(!Solve({}, sockets, {{2, 5, 5, 0}}));
    assert(!Solve({}, sockets, {{2, 5, 0, 201}}));
    assert(!Solve({}, std::vector<std::vector<Option>>(55, pool), {}));
    assert(!Solve({}, {{}}, {}));
    assert(!Solve({}, {{{7, 107, 1, 100}}}, {})); // no meta gems in normal sockets
    assert(!Solve({}, {{{7, 107, 2, -1}}}, {}));

    // Independent per-role scores: a healer-like blue candidate can win without changing the planner.
    auto healing = sockets;
    for (auto& options : healing)
        for (auto& gem : options)
            if (gem.item == 3)
                gem.score = 20;
    auto healer = Solve({}, healing, {});
    assert(healer && healer->score == 60);
    assert(healer->gems[0].item == 3);

    // Stable across invocations; quality cannot be rerolled by periodic maintenance/login.
    unsigned epicCount = 0;
    for (unsigned item = 1; item <= 10000; ++item)
    {
        bool epic = EpicSocket(815, item, 0, 20);
        assert(epic == EpicSocket(815, item, 0, 20));
        epicCount += epic;
        assert(!EpicSocket(815, item, 0, 0));
        assert(EpicSocket(815, item, 0, 100));
    }
    assert(epicCount > 1800 && epicCount < 2200);

    // Small instances compared to exhaustive enumeration, including mixed relative/absolute rules.
    for (auto const& rules : {allColors, moreBlue, std::vector<Requirement>{{2, 5, 0, 2}, {4, 3, 3, 0}}})
    {
        double best = -1;
        for (auto const& a : pool)
            for (auto const& b : pool)
                for (auto const& c : pool)
                {
                    auto counts = AddColor(AddColor(AddColor({}, a.color), b.color), c.color);
                    if (Fits(counts, rules))
                        best = std::max(best, a.score + b.score + c.score);
                }
        auto plan = Solve({}, sockets, rules);
        assert(bool(plan) == (best >= 0));
        assert(!plan || plan->score == best);
    }
    std::cout << "Companion gem planner: all checks passed\n";
}
