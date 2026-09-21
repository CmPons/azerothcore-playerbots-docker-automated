#ifndef MOD_RAID_ROSTER_COMPANION_GEM_PLANNER_H
#define MOD_RAID_ROSTER_COMPANION_GEM_PLANNER_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Pure, bounded planning: no Player pointers, SQL, random rolls, or item mutations.
namespace CompanionGems
{
    using Counts = std::array<uint8_t, 4>; // meta, red, yellow, blue (DBC color order)

    struct Requirement
    {
        uint8_t color = 0; // DBC one-based color index
        uint8_t comparator = 0; // 2: <, 3: >, 5: >=
        uint8_t compareColor = 0;
        uint32_t value = 0;
    };

    struct Option
    {
        uint32_t item = 0;
        uint32_t enchant = 0;
        uint8_t color = 0; // socket color bitmask, not a DBC index
        double score = 0;
    };

    struct Plan
    {
        double score = 0;
        std::vector<Option> gems;
    };

    inline bool EpicSocket(uint32_t bot, uint32_t item, uint8_t socket, uint32_t percent)
    {
        uint64_t value = (uint64_t(bot) << 32) | item;
        value += uint64_t(socket + 1) * 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;
        return value % 100 < std::min(percent, 100u);
    }

    inline bool Fits(Counts const& counts, std::vector<Requirement> const& requirements)
    {
        for (Requirement const& rule : requirements)
        {
            if (!rule.color || rule.color > 4 || rule.compareColor > 4)
                return false;
            uint32_t left = counts[rule.color - 1];
            uint32_t right = rule.compareColor ? counts[rule.compareColor - 1] : rule.value;
            switch (rule.comparator)
            {
                case 2:
                    if (left >= right)
                        return false;
                    break;
                case 3:
                    if (left <= right)
                        return false;
                    break;
                case 5:
                    if (left < right)
                        return false;
                    break;
                default:
                    return false;
            }
        }
        return true;
    }

    inline Counts AddColor(Counts counts, uint8_t mask)
    {
        for (uint8_t i = 0; i < 4; ++i)
            if ((mask & (1u << i)) && counts[i] < 255)
                ++counts[i];
        return counts;
    }

    // Retain exact counts for relative-color rules. For absolute rules, counts above
    // the largest relevant threshold are equivalent. That keeps normal BC metas tiny.
    // Fail closed rather than an unbounded search or a potentially inactive meta.
    inline std::optional<Plan> Solve(Counts fixed, std::vector<std::vector<Option>> const& sockets,
                                     std::vector<Requirement> const& requirements)
    {
        if (sockets.size() > 54)
            return std::nullopt;
        Counts caps{};
        for (Requirement const& rule : requirements)
        {
            if (!rule.color || rule.color > 4 || rule.compareColor > 4 || rule.value > 200 ||
                (rule.comparator != 2 && rule.comparator != 3 && rule.comparator != 5))
                return std::nullopt;
            if (rule.compareColor)
            {
                caps[rule.color - 1] = 255;
                caps[rule.compareColor - 1] = 255;
            }
            else
                caps[rule.color - 1] = std::max<uint8_t>(caps[rule.color - 1], rule.value + 1);
        }
        auto clamp = [&caps](Counts counts)
        {
            for (uint8_t i = 0; i < 4; ++i)
                counts[i] = std::min(counts[i], caps[i]);
            return counts;
        };
        std::map<Counts, Plan> states{{clamp(fixed), {}}};
        for (auto const& options : sockets)
        {
            std::map<Counts, Plan> next;
            for (auto const& [counts, plan] : states)
            {
                for (Option const& gem : options)
                {
                    // Meta sockets are selected separately; never allow a meta in a normal socket.
                    if (!gem.item || !gem.enchant || !gem.color || (gem.color & 1) ||
                        gem.color > 14 || !std::isfinite(gem.score) || gem.score <= 0)
                        continue;
                    Counts key = clamp(AddColor(counts, gem.color));
                    double score = plan.score + gem.score;
                    auto found = next.find(key);
                    if (found != next.end() && found->second.score >= score)
                        continue;
                    Plan candidate = plan;
                    candidate.score = score;
                    candidate.gems.push_back(gem);
                    next[key] = std::move(candidate);
                    if (next.size() > 16384)
                        return std::nullopt;
                }
            }
            states = std::move(next);
            if (states.empty())
                return std::nullopt;
        }
        std::optional<Plan> best;
        for (auto const& [counts, plan] : states)
            if (Fits(counts, requirements) && (!best || plan.score > best->score))
                best = plan;
        return best;
    }
}

#endif
