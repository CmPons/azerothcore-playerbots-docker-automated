#ifndef MOD_ARENA_ROSTER_STORE_H
#define MOD_ARENA_ROSTER_STORE_H
#include "ObjectGuid.h"
#include <vector>
#include <unordered_set>
#include <cstdint>

struct ArenaPartnerRow
{
    uint32 botGuid;
    uint8  cls;
    uint8  specTab;
};

struct ArenaPoolRow
{
    uint8  tier;       // 1..4
    uint8  rosterIdx;  // 0..14
    uint32 botGuid;
    uint8  cls;
    uint8  specTab;
    uint32 team[3];    // [0]=2v2, [1]=3v3, [2]=5v5 arena_team ids
};

namespace ArenaRosterStore
{
    // Partner pool
    bool PartnersExist(uint32 ownerGuid);
    std::vector<ArenaPartnerRow> LoadPartners(uint32 ownerGuid);       // ordered by class
    void ReplacePartners(uint32 ownerGuid, std::vector<ArenaPartnerRow> const&);
    void SetPartnerSpec(uint32 ownerGuid, uint8 cls, uint8 specTab);
    void ClearPartners(uint32 ownerGuid);
    std::unordered_set<uint32> AllPinnedBots();                        // partner + pool bot guids

    // Opponent pool
    std::vector<ArenaPoolRow> LoadPool();                              // ordered by tier, roster_idx
    void ReplacePool(std::vector<ArenaPoolRow> const&);
    void SetPoolTeams(uint8 tier, uint8 rosterIdx, uint32 t2, uint32 t3, uint32 t5);
    void ClearPool();
}
#endif
