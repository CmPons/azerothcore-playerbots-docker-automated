// Exercise the actual director, replacing only game APIs, prompt builder and queue.
#include "PBChatterAmbient.cpp"
#include <cassert>
#include <iostream>

bool g_PBChatEnable = true;
bool g_PBChatAmbientEnable = true;
bool g_PBChatAmbientGeneral = true;
bool g_PBChatAmbientGroup = true;
bool g_PBChatAmbientGuild = true;
uint32_t g_PBChatAmbientRaidPreferenceChance = 80;
uint32_t g_PBChatAmbientSeedMin = 60;
uint32_t g_PBChatAmbientSeedMax = 90;
uint32_t g_PBChatAmbientFollowMin = 8;
uint32_t g_PBChatAmbientFollowMax = 20;
uint32_t g_PBChatAmbientActiveWindow = 45;
uint32_t g_PBChatAmbientBotStreakMax = 4;
uint32_t g_PBChatAmbientCooldown = 75;
uint32_t g_PBChatAmbientPerBotCooldown = 120;
uint32_t g_PBChatAmbientMaxPerMin = 14;
uint32_t g_PBChatAmbientBufferLen = 8;
uint32_t g_PBChatAmbientWGeneric = 55;
uint32_t g_PBChatAmbientWReact = 25;
uint32_t g_PBChatAmbientWFlavor = 12;
uint32_t g_PBChatAmbientWEvent = 8;
std::string g_PBChatSystemPrompt = "system";
std::vector<PBChatJob> submitted;
uint32_t eventReads = 0;
bool queueAvailable = true;

bool PBChatterEvents::Take(uint64_t, uint32_t, std::string&)
{
    ++eventReads;
    return false;
}
bool PBChatterQueue::TrySubmitAmbient(PBChatJob job)
{
    if (!queueAvailable)
        return false;
    submitted.push_back(std::move(job));
    return true;
}
std::string PBChatterAmbientPrompt::Build(int, Player*, uint8_t kind,
    std::vector<std::pair<std::string, std::string>> const& recent, std::string const&)
{
    std::string result = std::to_string(kind);
    for (auto const& line : recent)
        result += ":" + line.second;
    return result;
}

struct Fixture
{
    Player human;
    Player bot;
    PlayerbotAI ai;
    Group raid;
    GroupReference botRef{&bot, nullptr};
    GroupReference humanRef{&human, &botRef};

    Fixture()
    {
        g_ctx.clear();
        g_botCooldown.clear();
        g_nowMs = 100000;
        g_rateCount = 0;
        submitted.clear();
        eventReads = 0;
        queueAvailable = true;
        testRoll = 0;
        g_PBChatAmbientRaidPreferenceChance = 80;
        g_PBChatAmbientGroup = true;
        human.guid.value = 1;
        bot.guid.value = 2;
        bot.ai = &ai;
        human.group = bot.group = &raid;
        raid.first = &humanRef;
        ObjectAccessor::players = {{1, &human}, {2, &bot}};
        for (uint8_t kind : {AMB_ZONE, AMB_GUILD, AMB_GROUP})
        {
            auto& c = Ensure(kind, kind == AMB_ZONE ? 10 : kind == AMB_GUILD ? 20 : 99, 1);
            c.nextEmitMs = 0;
            PushLine(c, "human", kind == AMB_GROUP ? "raid-only-history" : "public-only-history");
        }
    }
    Ctx& Context(uint8_t kind)
    {
        return g_ctx.at(Key(kind, kind == AMB_ZONE ? 10 : kind == AMB_GUILD ? 20 : 99));
    }
};

int main()
{
    for (uint8_t source : {AMB_ZONE, AMB_GUILD})
        for (uint32_t roll = 0; roll < 100; ++roll)
        {
            Fixture f;
            testRoll = roll;
            TryEmit(f.Context(source));
            assert(submitted.size() == 1 && g_rateCount == 1 && eventReads == 1);
            auto const& job = submitted[0];
            bool redirected = roll < 80;
            assert(job.channel == (redirected ? PBChatChannel::Raid :
                source == AMB_ZONE ? PBChatChannel::General : PBChatChannel::Guild));
            assert(job.ambientKind == (redirected ? uint8_t(AMB_GROUP) : source));
            assert(job.ambientIdent == (redirected ? 99 : source == AMB_ZONE ? 10 : 20));
            assert(job.anchorPlayerGuid == 1);
            assert(job.prompt.find(redirected ? "public-only-history" : "raid-only-history") == std::string::npos);
            assert(job.prompt.find(redirected ? "raid-only-history" : "public-only-history") != std::string::npos);
            assert(g_botCooldown.at(2) == g_nowMs + 120000);
            if (redirected)
                assert(f.Context(source).nextEmitMs > g_nowMs && f.Context(AMB_GROUP).nextEmitMs > g_nowMs);
        }

    // Preference never bypasses raid pacing or spends quota/event hints when held.
    for (int hold = 0; hold < 5; ++hold)
    {
        Fixture f;
        auto& target = f.Context(AMB_GROUP);
        if (hold == 0) target.nextEmitMs = g_nowMs + 10000;
        if (hold == 1) target.cooldownUntilMs = g_nowMs + 10000;
        if (hold == 2) target.lastAuthorBot = 2;
        if (hold == 3) target.eligible = false;
        if (hold == 4) target.anchor = 999;
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.empty() && g_rateCount == 0 && eventReads == 0 && g_botCooldown.empty());
        assert(f.Context(AMB_GUILD).nextEmitMs > g_nowMs);
    }
    for (int unchanged = 0; unchanged < 6; ++unchanged)
    {
        Fixture f;
        if (unchanged == 0) g_PBChatAmbientRaidPreferenceChance = 0;
        if (unchanged == 1) g_PBChatAmbientGroup = false;
        if (unchanged == 2) f.raid.raid = false;
        if (unchanged == 3) f.raid.bg = true;
        if (unchanged == 4) f.raid.bf = true;
        if (unchanged == 5) f.raid.first = &f.botRef; // no human member
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.size() == 1 && submitted[0].channel == PBChatChannel::Guild);
    }
    {
        Fixture f;
        g_PBChatAmbientRaidPreferenceChance = 100;
        testRoll = 99;
        TryEmit(f.Context(AMB_ZONE));
        assert(submitted.size() == 1 && submitted[0].channel == PBChatChannel::Raid);
    }
    {
        Fixture f;
        g_ctx.erase(Key(AMB_GROUP, 99));
        TryEmit(f.Context(AMB_ZONE));
        assert(submitted.empty() && eventReads == 0 && g_botCooldown.empty());
    }
    {
        Fixture f;
        queueAvailable = false;
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.empty() && g_rateCount == 0 && g_botCooldown.empty());
        assert(f.Context(AMB_GROUP).nextEmitMs == g_nowMs + 3000);
    }
    {
        Fixture f;
        TryEmit(f.Context(AMB_GROUP));
        assert(submitted.size() == 1 && submitted[0].channel == PBChatChannel::Raid);
    }
    {
        Fixture f;
        g_botCooldown[2] = g_nowMs + 10000;
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.empty() && eventReads == 0 && g_rateCount == 0);
    }
    {
        Fixture f;
        g_rateCount = g_PBChatAmbientMaxPerMin;
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.empty() && eventReads == 0 && g_botCooldown.empty());
    }
    {
        Fixture f;
        f.human.guild = 21; // stale source anchor: do not use another guild's buffer
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.empty() && eventReads == 0);
    }
    {
        Fixture f;
        Player otherHuman;
        otherHuman.guid.value = 3;
        otherHuman.group = &f.raid;
        ObjectAccessor::players[3] = &otherHuman;
        f.Context(AMB_GROUP).anchor = 3;
        TryEmit(f.Context(AMB_GUILD));
        assert(submitted.size() == 1 && submitted[0].anchorPlayerGuid == 3);
        assert(submitted[0].ambientIdent == 99 && submitted[0].channel == PBChatChannel::Raid);
    }
    assert(PBChatterChannelPolicy::SameGroup(99, 99, 99));
    assert(!PBChatterChannelPolicy::SameGroup(99, 98, 99));
    assert(!PBChatterChannelPolicy::SameGroup(99, 99, 98));
    assert(!PBChatterChannelPolicy::SameGroup(99, 0, 99));
    assert(!PBChatterChannelPolicy::SameGroup(0, 0, 0));
    std::cout << "Production ambient routing, pacing and audience tests passed\n";
}
