// The runner injects production target-filter, comparator, spell IDs and cap.
// WorldObject/SpellInfo are API doubles; this is not a complete spell-engine test.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>

using uint32 = std::uint32_t;
/* SPELLS_AND_CAP */

struct WorldObject
{
    float position;
    bool GetDistanceOrder(WorldObject const* left, WorldObject const* right) const
    {
        return std::abs(position - left->position) < std::abs(position - right->position);
    }
};

namespace Acore
{
/* COMPARATOR */
}

struct SpellInfo
{
    uint32 Id;
    uint32 MaxAffectedTargets;
};

struct Filter
{
    WorldObject caster{10.f};
    SpellInfo info;
    SpellInfo const* GetSpellInfo() const { return &info; }
    WorldObject* GetCaster() { return &caster; }
    /* FILTER */
    /* LEGACY_FILTER */
};

void Check(uint32 spell, uint32 dbcCap, uint32 expectedCap, uint32 count)
{
    Filter filter{{10.f}, {spell, dbcCap}};
    std::vector<WorldObject> storage;
    storage.reserve(count);
    std::list<WorldObject*> targets;
    for (uint32 i = 0; i < count; ++i)
    {
        // Furthest first; assertions must verify distance, not just a resized list.
        storage.push_back({filter.caster.position + float(count - i)});
        targets.push_back(&storage.back());
    }
    auto original = targets;
    filter.FilterTargets(targets);
    assert(targets.size() == (count > expectedCap ? expectedCap : count));
    if (count <= expectedCap)
        assert(targets == original);
    else
    {
        uint32 distance = 1;
        for (WorldObject* target : targets)
            assert(target->position - filter.caster.position == float(distance++));
    }
    auto once = targets;
    filter.FilterTargets(targets);
    assert(targets == once);
}

int main()
{
    static_assert(WYVERN_STING_MAX_TARGETS == 3);
    for (uint32 count : {0u, 1u, 2u, 3u, 4u, 9u, 10u, 11u, 15u, 16u, 40u})
    {
        Check(SPELL_WYVERN_STING, 10, 3, count);
        Check(SPELL_WYVERN_STING, 15, 3, count); // Local Sting cap, not DBC cap.
        Check(SPELL_POISON_BOLT, 15, 15, count); // Damage spell is not nerfed.
        Check(SPELL_POISON_BOLT, 7, 7, count); // Retains the existing DBC policy.
    }

    // The previous filter retained ten, regardless of a smaller custom cast cap.
    Filter legacy{{10.f}, {SPELL_WYVERN_STING, 10}};
    std::vector<WorldObject> storage(10);
    std::list<WorldObject*> targets;
    for (uint32 i = 0; i < storage.size(); ++i)
    {
        storage[i].position = 10.f + float(10 - i);
        targets.push_back(&storage[i]);
    }
    legacy.LegacyFilterTargets(targets);
    assert(targets.size() == 10);
    legacy.FilterTargets(targets);
    assert(targets.size() == 3);
    for (WorldObject* target : targets)
        assert(target->position <= 13.f);

    // Equal-distance candidates retain the existing stable list-sort behavior.
    Filter ties{{10.f}, {SPELL_WYVERN_STING, 10}};
    WorldObject a{11.f}, b{11.f}, c{11.f}, d{11.f};
    targets = {&a, &b, &c, &d};
    ties.FilterTargets(targets);
    assert((targets == std::list<WorldObject*>{&a, &b, &c}));
    std::cout << "Production Huhuran target-filter regressions passed\n";
}
