#include "ArenaRosterStore.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"

namespace ArenaRosterStore
{

bool PartnersExist(uint32 ownerGuid)
{
    QueryResult r = CharacterDatabase.Query("SELECT 1 FROM mod_arena_roster WHERE owner_guid = {} LIMIT 1", ownerGuid);
    return bool(r);
}

std::vector<ArenaPartnerRow> LoadPartners(uint32 ownerGuid)
{
    std::vector<ArenaPartnerRow> out;
    QueryResult r = CharacterDatabase.Query(
        "SELECT bot_guid, class, spec_tab FROM mod_arena_roster WHERE owner_guid = {} ORDER BY class", ownerGuid);
    if (r) do
    {
        Field* f = r->Fetch();
        out.push_back({f[0].Get<uint32>(), f[1].Get<uint8>(), f[2].Get<uint8>()});
    } while (r->NextRow());
    return out;
}

void ReplacePartners(uint32 ownerGuid, std::vector<ArenaPartnerRow> const& rows)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append("DELETE FROM mod_arena_roster WHERE owner_guid = {}", ownerGuid);
    for (ArenaPartnerRow const& row : rows)
        trans->Append("INSERT INTO mod_arena_roster (owner_guid, bot_guid, class, spec_tab) VALUES ({}, {}, {}, {})",
                      ownerGuid, row.botGuid, row.cls, row.specTab);
    CharacterDatabase.CommitTransaction(trans);
}

void SetPartnerSpec(uint32 ownerGuid, uint8 cls, uint8 specTab)
{
    CharacterDatabase.Execute("UPDATE mod_arena_roster SET spec_tab = {} WHERE owner_guid = {} AND class = {}",
                              specTab, ownerGuid, cls);
}

void ClearPartners(uint32 ownerGuid)
{
    CharacterDatabase.Execute("DELETE FROM mod_arena_roster WHERE owner_guid = {}", ownerGuid);
}

std::unordered_set<uint32> AllPinnedBots()
{
    std::unordered_set<uint32> out;
    if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_arena_roster"))
        do { out.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
    if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_arena_pool"))
        do { out.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
    return out;
}

std::vector<ArenaPoolRow> LoadPool()
{
    std::vector<ArenaPoolRow> out;
    QueryResult r = CharacterDatabase.Query(
        "SELECT tier, roster_idx, bot_guid, class, spec_tab, team2, team3, team5 "
        "FROM mod_arena_pool ORDER BY tier, roster_idx");
    if (r) do
    {
        Field* f = r->Fetch();
        ArenaPoolRow row;
        row.tier = f[0].Get<uint8>(); row.rosterIdx = f[1].Get<uint8>();
        row.botGuid = f[2].Get<uint32>(); row.cls = f[3].Get<uint8>(); row.specTab = f[4].Get<uint8>();
        row.team[0] = f[5].Get<uint32>(); row.team[1] = f[6].Get<uint32>(); row.team[2] = f[7].Get<uint32>();
        out.push_back(row);
    } while (r->NextRow());
    return out;
}

void ReplacePool(std::vector<ArenaPoolRow> const& rows)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    trans->Append("DELETE FROM mod_arena_pool");
    for (ArenaPoolRow const& row : rows)
        trans->Append("INSERT INTO mod_arena_pool (tier, roster_idx, bot_guid, class, spec_tab, team2, team3, team5) "
                      "VALUES ({}, {}, {}, {}, {}, {}, {}, {})",
                      row.tier, row.rosterIdx, row.botGuid, row.cls, row.specTab,
                      row.team[0], row.team[1], row.team[2]);
    CharacterDatabase.CommitTransaction(trans);
}

void SetPoolTeams(uint8 tier, uint8 rosterIdx, uint32 t2, uint32 t3, uint32 t5)
{
    CharacterDatabase.Execute("UPDATE mod_arena_pool SET team2 = {}, team3 = {}, team5 = {} "
                              "WHERE tier = {} AND roster_idx = {}", t2, t3, t5, tier, rosterIdx);
}

void ClearPool()
{
    CharacterDatabase.Execute("DELETE FROM mod_arena_pool");
}

} // namespace ArenaRosterStore
