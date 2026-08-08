#include "ArenaRosterConfig.h"
#include "Config.h"
#include "Log.h"
#include "StringConvert.h"
#include "Tokenize.h"

bool   g_ArEnable = false;
bool   g_ArDebug = false;
std::array<uint32_t, 3> g_ArTierCeilings = {800, 1400, 1800};
std::array<uint32_t, 4> g_ArTierRatings  = {400, 1100, 1600, 2000};
uint32_t g_ArGraceLogoutSecs = 30;

template <size_t N>
static void ParseUintList(char const* key, std::string const& csv, std::array<uint32_t, N>& out)
{
    auto tokens = Acore::Tokenize(csv, ',', false);
    if (tokens.size() != N)
        LOG_WARN("server.loading", "[ArenaRoster] {} config list has {} entries, expected {} — missing entries keep defaults",
            key, tokens.size(), N);
    size_t i = 0;
    for (auto tok : tokens)
    {
        if (i >= N) break;
        if (auto v = Acore::StringTo<uint32_t>(tok)) out[i] = *v;
        ++i;
    }
}

void ArenaRosterLoadConfig()
{
    g_ArEnable = sConfigMgr->GetOption<bool>("ArenaRoster.Enable", false);
    g_ArDebug  = sConfigMgr->GetOption<bool>("ArenaRoster.Debug", false);
    ParseUintList("ArenaRoster.TierCeilings", sConfigMgr->GetOption<std::string>("ArenaRoster.TierCeilings", "800,1400,1800"), g_ArTierCeilings);
    ParseUintList("ArenaRoster.TierRatings", sConfigMgr->GetOption<std::string>("ArenaRoster.TierRatings", "400,1100,1600,2000"), g_ArTierRatings);
    g_ArGraceLogoutSecs = sConfigMgr->GetOption<uint32_t>("ArenaRoster.GraceLogoutSecs", 30);
    LOG_INFO("server.loading", "[ArenaRoster] Enable={} TierCeilings={},{},{} TierRatings={},{},{},{} GraceLogoutSecs={}",
        g_ArEnable ? 1 : 0,
        g_ArTierCeilings[0], g_ArTierCeilings[1], g_ArTierCeilings[2],
        g_ArTierRatings[0], g_ArTierRatings[1], g_ArTierRatings[2], g_ArTierRatings[3],
        g_ArGraceLogoutSecs);
}

uint8_t ArenaRosterTierFor(uint32_t rating)
{
    for (uint8_t t = 0; t < g_ArTierCeilings.size(); ++t)
        if (rating < g_ArTierCeilings[t])
            return t;
    return 3;
}
