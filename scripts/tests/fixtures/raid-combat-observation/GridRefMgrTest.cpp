#include "GridRefMgr.h"
#include "GridReference.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

std::string GetDebugInfo() { return {}; }
namespace Acore
{
[[noreturn]] void Assert(std::string_view, uint32, std::string_view, std::string_view, std::string_view, std::string_view)
{
    std::abort();
}
}
struct Node { unsigned id; GridReference<Node> ref; };
using Manager = GridRefMgr<Node>;
static_assert(!std::is_copy_constructible_v<Manager> && !std::is_move_constructible_v<Manager>);
static_assert(!std::is_copy_constructible_v<GridReference<Node>>);

std::vector<unsigned> page(Manager& manager, unsigned quota)
{
    std::vector<unsigned> result;
    manager.VisitBounded(quota, [&](Node* node) { result.push_back(node->id); });
    return result;
}
int main()
{
    Manager manager;
    assert(manager.VisitBounded(4, [](Node*) { assert(false); }).complete);
    Node single{0, {}};
    single.ref.link(&manager, &single);
    assert(page(manager, 5) == std::vector<unsigned>{0});
    single.ref.unlink();
    assert(page(manager, 5).empty());
    single.ref.link(&manager, &single);
    single.ref.invalidate();
    assert(!single.ref.isValid() && single.ref.GetSource() == &single);
    assert(page(manager, 5).empty());

    std::vector<std::unique_ptr<Node>> nodes;
    for (unsigned i = 0; i < 10000; ++i)
    {
        auto node = std::make_unique<Node>();
        node->id = i;
        node->ref.link(&manager, node.get());
        nodes.push_back(std::move(node));
    }
    auto* originalFirst = manager.getFirst();
    std::set<unsigned> observed;
    for (unsigned i = 0; i < 197; ++i)
    {
        auto result = manager.VisitBounded(51, [&](Node* node) { observed.insert(node->id); });
        assert(result.examined == 51 && !result.complete);
    }
    assert(observed.size() == 10000); // Original prefix design fails this assertion.
    assert(manager.getFirst() == originalFirst); // Ordinary native iteration order unchanged.
    assert(manager.VisitBounded(10000, [](Node*) {}).complete);

    manager.clearReferences();
    for (unsigned i = 0; i < 6; ++i)
        nodes[i]->ref.link(&manager, nodes[i].get()); // 5 4 3 2 1 0
    assert(page(manager, 1) == std::vector<unsigned>{5});
    nodes[4].reset(); // Delete next cursor node.
    assert(page(manager, 1) == std::vector<unsigned>{3});
    nodes[3].reset(); // Delete previously examined node.
    nodes[2]->ref.invalidate(); // Invalidate next cursor node.
    assert(page(manager, 1) == std::vector<unsigned>{1});
    nodes[0]->ref.unlink(); // Delete next/tail: cursor must restart safely at head.
    assert(page(manager, 1) == std::vector<unsigned>{5});
    Manager destination;
    nodes[1]->ref.link(&destination, nodes[1].get()); // Transfer next cursor node.
    assert(page(manager, 8) == std::vector<unsigned>{5});
    assert(page(destination, 8) == std::vector<unsigned>{1});

    // Callback removes current AND next; no dereference of removed current after callback.
    nodes[0]->ref.link(&manager, nodes[0].get());
    nodes[2]->ref.link(&manager, nodes[2].get()); // 2 0 5, cursor at wrap
    bool once = false;
    auto mutation = manager.VisitBounded(3, [&](Node* node)
    {
        if (!once)
        {
            once = true;
            node->ref.unlink();
            if (nodes[0]->ref.isValid())
                nodes[0]->ref.unlink();
        }
    });
    assert(!mutation.complete);
    manager.VisitBounded(1, [&](Node*)
    {
        assert(!manager.VisitBounded(10, [](Node*) { assert(false); }).complete);
    });
    try { manager.VisitBounded(1, [](Node*) { throw std::runtime_error("probe"); }); }
    catch (std::runtime_error const&) { }
    assert(!page(manager, 1).empty());

    {
        Manager callbacks;
        auto last = std::make_unique<Node>(); last->id = 1; last->ref.link(&callbacks, last.get());
        auto next = std::make_unique<Node>(); next->id = 2; next->ref.link(&callbacks, next.get());
        auto current = std::make_unique<Node>(); current->id = 3; current->ref.link(&callbacks, current.get());
        auto result = callbacks.VisitBounded(3, [&](Node* node)
        {
            if (node->id == 3)
            {
                current.reset();
                next.reset();
            }
        });
        assert(!result.complete && page(callbacks, 3) == std::vector<unsigned>{1});
    }

    // Manager destruction invalidates references while derived cursor fields still live.
    {
        auto transient = std::make_unique<Manager>();
        nodes[0]->ref.link(transient.get(), nodes[0].get());
        nodes[2]->ref.link(transient.get(), nodes[2].get());
        page(*transient, 1);
    }
    assert(!nodes[0]->ref.isValid() && !nodes[2]->ref.isValid());
    nodes[0]->ref.unlink();

    // Hash rehash moves buckets, not independently owned nodes or the intrusive grid links.
    std::unordered_map<unsigned, std::unique_ptr<Node>> registry;
    Manager dense;
    for (unsigned i = 0; i < 200; ++i)
    {
        auto node = std::make_unique<Node>(); node->id = i;
        node->ref.link(&dense, node.get()); registry.emplace(i, std::move(node));
    }
    observed.clear();
    for (unsigned i = 0; i < 200; ++i)
    {
        auto ids = page(dense, 7); observed.insert(ids.begin(), ids.end());
        registry.rehash(1000 + i * 13);
        registry.at(199)->ref.unlink();
        registry.at(199)->ref.link(&dense, registry.at(199).get());
    }
    for (unsigned i = 0; i < 199; ++i) assert(observed.contains(i));
    dense.clearReferences(); // Unload and repopulate at same manager address.
    registry.at(1)->ref.link(&dense, registry.at(1).get());
    assert(page(dense, 8) == std::vector<unsigned>{1});
    std::cout << "PASS real GridRefMgr: 10000-node middle progress, empty/singleton/tail, delete/invalidate/transfer, "
                 "callback mutation/reentry/throw, destructor, bounded churn, rehash, unload/repopulate\n";
}
