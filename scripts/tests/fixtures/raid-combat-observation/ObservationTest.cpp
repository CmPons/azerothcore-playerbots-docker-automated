#include "Fixture.h"
#include "Fragments.inc"
#include "Candidate.inc"
using namespace GenericPolicy;

ObservedEntity const* find(Observation const& result, ObjectGuid guid)
{
    for (auto const& entity : result.entities) if (entity.guid == guid) return &entity;
    return nullptr;
}
int main()
{
    InstanceMap map;
    InstanceScript script;
    map.script = &script;
    Player observer;
    observer.guid = ObjectGuid(HighGuid::Player, uint32(1));
    map.Add(observer);
    GameObject object;
    object.guid = ObjectGuid(HighGuid::GameObject, 9, uint32(1));
    object.entry = 9; object.x = 1;
    map.Add(object);
    Creature creature;
    creature.guid = ObjectGuid(HighGuid::Unit, 9, uint32(2));
    creature.x = 2;
    map.Add(creature);
    std::vector<ObjectGuid> actors{observer.guid}, needed{object.guid};
    MapCollector collector(17);
    auto collect = [&] { return collector.Collect(map, actors, needed, 0); };
    auto result = collect();
    assert(result.scopeEpoch == 17 && result.sequence == 1);
    assert(find(result, object.guid)->observers == 1);
    assert(result.cells == 1 && result.examined <= ExamineLimit);
    assert(result.observerTests <= result.examined * ObserverLimit);

    // Extracted full CanSeeOrDetect and CanNeverSee branches, not a fixture visibility boolean.
    object.phase = 2;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    assert(find(result, object.guid)->entry == 0 && (result.gaps & RequiredGap));
    object.phase = 1;
    object.ai.visible = false;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.ai.visible = true; observer.conditions = false;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    observer.conditions = true; object.detectable = false;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.detectable = true; object.despawn = true;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.despawn = false; object.m_serverSideVisibility.value[SERVERSIDE_VISIBILITY_GM] = 2;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.m_serverSideVisibility.value[SERVERSIDE_VISIBILITY_GM] = 0;
    object.m_serverSideVisibility.value[SERVERSIDE_VISIBILITY_GHOST] = GHOST_VISIBILITY_GHOST;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.m_serverSideVisibility.value[SERVERSIDE_VISIBILITY_GHOST] = 1;
    object.x = 500;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.x = 1;
    creature.vehicle = &observer;
    assert(!observer.CanSeeOrDetect(&creature, false, true));
    observer.client.insert(observer.guid);
    assert(observer.CanSeeOrDetect(&creature, false, true));
    creature.vehicle = nullptr;
    InstanceMap elsewhere;
    object.map = &elsewhere;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.map = &map;
    object.inWorld = false;
    result = collect(); assert(find(result, object.guid)->observers == 0);
    object.inWorld = true;

    // Target + needed interaction directly resolved outside selected cells. Never infer disappearance.
    creature.RemoveFromGrid(); creature.x = 80; map.Add(creature);
    observer.target = creature.guid;
    result = collect(); assert(find(result, creature.guid)->required && find(result, creature.guid)->observers);
    map.objects.erase(creature.guid);
    result = collect(); assert(find(result, creature.guid) && !find(result, creature.guid)->observers);
    map.objects.emplace(creature.guid, &creature);

    // Actual Map::Visit + real MapGrid/Cell visit must not allocate cells or unloaded grids.
    auto grids = map.grids.size();
    uint32 cells = 0;
    for (auto const& [id, grid] : map.grids) cells += grid->GetCreatedCellsCount();
    result = collector.Collect(map, actors, needed, 100);
    assert(result.gaps & UnloadedGap);
    assert(map.grids.size() == grids);
    uint32 afterCells = 0;
    for (auto const& [id, grid] : map.grids) afterCells += grid->GetCreatedCellsCount();
    assert(afterCells == cells);

    // Whole-instance safety is independent of the 40-observer cap and visibility.
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Safe);
    script.bosses = {{0}, {IN_PROGRESS}};
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Encounter);
    script.bosses.clear();
    observer.m_Controlled.insert(&creature);
    creature.combat = true;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    creature.combat = false;
    Spell spell; spell.castTime = 0;
    creature.m_currentSpells[CURRENT_GENERIC_SPELL] = &spell;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    spell.state = SPELL_STATE_DELAYED;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    creature.m_currentSpells[CURRENT_GENERIC_SPELL] = nullptr;
    creature.m_currentSpells[CURRENT_CHANNELED_SPELL] = &spell;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    creature.m_currentSpells[CURRENT_CHANNELED_SPELL] = nullptr;
    creature.m_currentSpells[CURRENT_AUTOREPEAT_SPELL] = &spell;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    creature.m_currentSpells[CURRENT_AUTOREPEAT_SPELL] = nullptr;
    creature.m_Controlled.insert(&observer); // Cycles terminate without skipping busy nodes.
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Safe);
    assert(CheckReloadSafety(map, 2).state == ReloadSafety::OverBound);
    observer.m_Controlled.insert(nullptr);
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Unknown);
    observer.m_Controlled.erase(nullptr);
    creature.removing = true;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Unknown);
    creature.removing = false;
    observer.teleport = true;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Unknown);
    observer.teleport = false;
    map.script = nullptr;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Unknown);
    map.script = &script;

    std::vector<std::unique_ptr<Player>> extra;
    for (uint32 i = 2; i <= 61; ++i)
    {
        auto player = std::make_unique<Player>(); player->guid = ObjectGuid(HighGuid::Player, i);
        map.Add(*player); extra.push_back(std::move(player));
    }
    observer.combat = true; // Original player is tail: beyond first 40, invisible to planning cap.
    result = collect(); assert(result.gaps & RosterGap);
    auto safety = CheckReloadSafety(map, 1000);
    assert(safety.state == ReloadSafety::Busy && safety.players == 61);
    assert(CheckReloadSafety(map, 40).state == ReloadSafety::OverBound);
    observer.combat = false;
    observer.m_Controlled.clear(); creature.m_Controlled.clear();
    observer.m_mover = &creature; creature.combat = true;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    observer.m_mover = &observer; observer.vehicle = &creature;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Busy);
    observer.vehicle = nullptr; creature.combat = false;
    assert(CheckReloadSafety(map, 1000).state == ReloadSafety::Safe);

    // 40 spatially separated observers, 10000 real intrusive creature references/cell.
    InstanceMap dense;
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<Creature>> population;
    std::vector<ObjectGuid> guids;
    for (uint32 i = 0; i < 40; ++i)
    {
        auto player = std::make_unique<Player>();
        player->guid = ObjectGuid(HighGuid::Player, i+100);
        player->x = float(i % 8) * 400 + 10; player->y = float(i / 8) * 400 + 10;
        dense.Add(*player); guids.push_back(player->guid);
        for (uint32 j = 0; j < 10000; ++j)
        {
            auto mob = std::make_unique<Creature>();
            mob->guid = ObjectGuid(HighGuid::Unit, i+100, j+1);
            mob->x = player->x; mob->y = player->y;
            dense.Add(*mob); population.push_back(std::move(mob));
        }
        players.push_back(std::move(player));
    }
    MapCollector denseCollector(22);
    std::set<ObjectGuid> observed;
    auto begin = std::chrono::steady_clock::now();
    uint32 maximumExamined = 0, maximumTests = 0, maximumCalls = 0;
    for (uint32 tick = 0; tick < 1200; ++tick)
    {
        auto view = denseCollector.Collect(dense, guids, {}, 0);
        assert(view.cells == 40 && view.examined <= ExamineLimit && (view.gaps & PopulationGap));
        assert(view.observerTests <= ExamineLimit * ObserverLimit);
        maximumExamined = std::max(maximumExamined, view.examined);
        maximumTests = std::max(maximumTests, view.observerTests);
        maximumCalls = std::max(maximumCalls, view.visibilityCalls);
        uint64 served = 0;
        for (auto const& entity : view.entities)
            if (entity.type == TYPEID_UNIT) { observed.insert(entity.guid); served |= entity.observers; }
        assert(served == ((uint64(1) << 40) - 1)); // Every observer gets first opportunities each update.
        if (tick == 50) dense.objects.rehash(1000000);
    }
    assert(observed.size() == 400000);
    auto micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()-begin).count()/1200;
    std::cout << "dense40x10000 all400000 reached; us/update=" << micros << " examined=" << maximumExamined
              << " observerTests=" << maximumTests << " visibilityCalls=" << maximumCalls << '\n';

    // Shared-cell actors in disjoint phases: each page evaluated for every observer, not alternating blindness.
    InstanceMap shared;
    Player a, b;
    a.guid = ObjectGuid(HighGuid::Player, uint32(400)); b.guid = ObjectGuid(HighGuid::Player, uint32(401));
    b.phase = 2; shared.Add(a); shared.Add(b);
    Creature phaseOne, phaseTwo;
    phaseOne.guid = ObjectGuid(HighGuid::Unit, 1, uint32(900));
    phaseTwo.guid = ObjectGuid(HighGuid::Unit, 1, uint32(901)); phaseTwo.phase = 2;
    shared.Add(phaseOne); shared.Add(phaseTwo);
    MapCollector sharedCollector(25);
    std::vector<ObjectGuid> sharedGuids{a.guid,b.guid};
    result = sharedCollector.Collect(shared, sharedGuids, {}, 0);
    assert(result.cells == 1);
    assert(find(result, phaseOne.guid)->observers == 1 && find(result, phaseTwo.guid)->observers == 2);
    shared.grids.clear(); // Manager destruction invalidates actual GridObject links.
    assert(!phaseOne.IsInGrid() && !a.IsInGrid());
    result = sharedCollector.Collect(shared, sharedGuids, {}, 0);
    assert(result.gaps & UnloadedGap);
    shared.Add(phaseOne);
    result = sharedCollector.Collect(shared, sharedGuids, {}, 0);
    assert(find(result, phaseOne.guid)->observers == 1);
    // Forty centers retain first opportunity; the remaining 24 proposals rotate through ALL neighbor offsets.
    InstanceMap neighbors;
    std::vector<std::unique_ptr<Player>> scouts;
    std::vector<std::unique_ptr<Creature>> nearby;
    std::vector<ObjectGuid> scoutGuids;
    for (uint32 i = 0; i < 40; ++i)
    {
        auto scout = std::make_unique<Player>(); scout->guid = ObjectGuid(HighGuid::Player, i+2000);
        scout->x = float(i % 8)*400+10; scout->y = float(i/8)*400+10;
        auto mob = std::make_unique<Creature>(); mob->guid = ObjectGuid(HighGuid::Unit, 1, i+2000);
        mob->x = scout->x+80; mob->y = scout->y;
        neighbors.Add(*scout); neighbors.Add(*mob);
        scoutGuids.push_back(scout->guid); scouts.push_back(std::move(scout)); nearby.push_back(std::move(mob));
    }
    MapCollector neighborCollector(26);
    std::set<ObjectGuid> reached;
    for (uint32 tick = 0; tick < 15; ++tick)
    {
        result = neighborCollector.Collect(neighbors, scoutGuids, {}, 1);
        assert(result.cells <= CellLimit && result.examined <= ExamineLimit);
        for (auto const& entity : result.entities) if (entity.type == TYPEID_UNIT) reached.insert(entity.guid);
    }
    assert(reached.size() == 40);
    scoutGuids.push_back(observer.guid);
    needed.assign(InteractionLimit+1, ObjectGuid(HighGuid::GameObject, 1, uint32(90000)));
    result = neighborCollector.Collect(neighbors, scoutGuids, needed, 1);
    assert((result.gaps & ObserverGap) && (result.gaps & RequiredGap));
    assert(result.examined <= ExamineLimit && result.observers.size() == ObserverLimit);
    std::cout << "PASS source-backed Map::Visit/visibility/cast/encounter + candidate collector/safety; "
                 "API doubles explicitly bounded, not full native/live execution\n";
}
