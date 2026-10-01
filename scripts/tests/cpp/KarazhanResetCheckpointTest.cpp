// The Python driver injects the production Chess SetData case and combat predicate.
#include "ProgressionRaidReset.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using uint32 = uint32_t;
using ObjectGuid = uint32;
enum State : uint32 { NOT_STARTED, IN_PROGRESS, FAIL, DONE, SPECIAL, TO_BE_DECIDED };
enum Constants : uint32
{
    DATA_CHESS_EVENT = 9, SPELL_GAME_IN_SESSION = 39331,
    TEAM_ALLIANCE = 0, TEAM_HORDE = 1,
    CHESS_FACTION_ALLIANCE = 1690, CHESS_FACTION_HORDE = 1689,
    UNIT_NPC_FLAG_GOSSIP = 1
};
struct Creature
{
    uint32 GetFaction() const { return CHESS_FACTION_ALLIANCE; }
    void SetNpcFlag(uint32) { }
};
struct Map
{
    Creature* GetCreature(ObjectGuid) { return nullptr; }
    Map* ToInstanceMap() { return this; }
    void PermBindAllPlayers() { ++binds; }
    uint32 binds = 0;
};
struct InstanceScript
{
    virtual bool IsEncounterInProgress() const { return otherCombat; }
    virtual ~InstanceScript() = default;
    bool otherCombat = false;
};
struct Karazhan : InstanceScript
{
    uint32 _chessEvent = NOT_STARTED;
    uint32 _chessTeam = TEAM_ALLIANCE;
    std::vector<ObjectGuid> _chessPiecesGUID;
    ObjectGuid m_uiGamesmansExitDoor = 1;
    Map map;
    Map* instance = &map;
    uint32 checkpoint = TO_BE_DECIDED;
    uint32 saves = 0;
    uint32 savedCheckpoint = TO_BE_DECIDED;
    void DoCastSpellOnPlayers(uint32) { }
    void DoRemoveAurasDueToSpellOnPlayers(uint32) { }
    void HandleGameObject(ObjectGuid, bool) { }
    uint32 GetBossState(uint32 id) const { assert(id == DATA_CHESS_EVENT); return checkpoint; }
    bool SetBossState(uint32 id, uint32 state)
    {
        assert(id == DATA_CHESS_EVENT);
        auto previous = checkpoint;
        checkpoint = state;
        // Match core: TO_BE_DECIDED initializes without SaveToDB, returning false.
        if (previous == TO_BE_DECIDED || previous == state)
            return false;
        SaveToDB();
        return true;
    }
    void SaveToDB() { ++saves; savedCheckpoint = checkpoint; }
#include "KarazhanResetProduction.inc"
};

int main()
{
    for (uint32 initial : {uint32(NOT_STARTED), uint32(TO_BE_DECIDED)})
    {
        Karazhan script;
        script.checkpoint = initial;
        assert(!script.IsEncounterInProgress());
        script.SetData(DATA_CHESS_EVENT, IN_PROGRESS);
        assert(script.IsEncounterInProgress() && script.saves == 0);
        script.SetData(DATA_CHESS_EVENT, DONE);
        assert(!script.IsEncounterInProgress());
        assert(script.saves == 1 && script.savedCheckpoint == DONE && script.map.binds == 1);
        // Required Chess checkpoint now contributes to a real full clear.
        std::string data = "K Z 3 3 3 5 3 3 3 3 3 " + std::to_string(script.savedCheckpoint) + " 3 0";
        auto progress = ProgressionRaidReset::ReadProgress(532, data);
        assert(progress && progress->cleared);
        // Friendly replay and later failures leave the persisted victory intact.
        script.SetData(DATA_CHESS_EVENT, SPECIAL);
        assert(script.IsEncounterInProgress());
        script.SetData(DATA_CHESS_EVENT, DONE);
        script.SetData(DATA_CHESS_EVENT, IN_PROGRESS);
        script.SetData(DATA_CHESS_EVENT, NOT_STARTED);
        assert(script.saves == 1 && script.checkpoint == DONE);
    }
    Karazhan failed;
    failed.SetData(DATA_CHESS_EVENT, IN_PROGRESS);
    failed.SetData(DATA_CHESS_EVENT, NOT_STARTED);
    assert(failed.saves == 0 && failed.checkpoint == TO_BE_DECIDED);
    // Neither a friendly game nor a stray DONE constitutes a first PvE victory.
    failed.SetData(DATA_CHESS_EVENT, SPECIAL);
    failed.SetData(DATA_CHESS_EVENT, DONE);
    failed.SetData(DATA_CHESS_EVENT, DONE);
    assert(failed.saves == 0 && failed.checkpoint == TO_BE_DECIDED);
    failed.otherCombat = true;
    assert(failed.IsEncounterInProgress());
    assert(!ProgressionRaidReset::ReadProgress(532, "K Z 3 3 3 5 3 3 3 3 3 5 3 0")->cleared);
    std::cout << "Production Chess completion and reset-deferral cases passed\n";
}
