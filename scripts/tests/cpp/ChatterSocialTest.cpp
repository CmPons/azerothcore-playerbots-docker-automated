#include "PBChatterContext.h"
#include "PBChatterAmbientPrompt.h"
#include "PBChatterAmbient.h"
#include "Player.h"
#include <cassert>
#include <iostream>

std::vector<std::string> g_PBChatStyleExamples{"test style only"};
namespace PBChatterPersona { std::string BuildPromptBlock(Player*) { return ""; } }
namespace PBChatterContext { std::string TopBagItem(Player*) { return ""; } }
namespace PBChatterEvents
{
    std::vector<std::string> RecentForGroup(Group*, uint32_t, uint32_t) { return {}; }
}
namespace PBChatterAmbient { uint32_t NowMs() { return 0; } }
namespace PBChatterClassifier { bool IsRealPlayerSender(Player* p) { return p && p->real; } }

void Contains(std::string const& text, std::string const& fragment)
{
    if (text.find(fragment) == std::string::npos)
    {
        std::cerr << "Missing: " << fragment << "\nIn:\n" << text;
        std::abort();
    }
}

int main()
{
    using namespace PBChatterContext;
    Player speaker, sender;
    speaker.name = "Speaker";
    sender.name = "Sender";
    sender.guid = {2};
    sender.real = true;
    PlayerSocial speakerFriends, senderFriends;
    speaker.social = &speakerFriends;
    sender.social = &senderFriends;
    guildMgr.names = {{7, "Shared Name"}, {8, "Shared Name"}};
    speaker.guild = sender.guild = 7;
    senderFriends.friends.insert(speaker.guid.value);
    auto facts = MemberSocialFacts(&speaker, &sender);
    Contains(facts, "name = \"Shared Name\"");
    Contains(facts, "same_guild_as_speaker = true");
    Contains(facts, "member_has_speaker_friended = true");
    Contains(facts, "speaker_has_member_friended = false");
    sender.guild = 8; // identical display strings are NOT guild identity
    Contains(MemberSocialFacts(&speaker, &sender), "same_guild_as_speaker = false");
    speaker.guild = sender.guild = 0;
    facts = MemberSocialFacts(&speaker, &sender);
    Contains(facts, "guild = { state = \"none\" }");
    Contains(facts, "same_guild_as_speaker = false");
    speaker.guild = 7;
    Contains(MemberSocialFacts(&speaker, &sender), "same_guild_as_speaker = false");
    sender.guild = 999;
    Contains(MemberSocialFacts(&speaker, &sender), "name_available = false");
    Contains(MemberSocialFacts(&speaker, &sender), "same_guild_as_speaker = false");
    sender.guild = speaker.guild = 999; // IDs still establish equality if guild name is unavailable
    Contains(MemberSocialFacts(&speaker, &sender), "same_guild_as_speaker = true");
    speaker.social = nullptr;
    facts = MemberSocialFacts(&speaker, &speaker);
    Contains(facts, "is_speaker = true");
    Contains(facts, "same_guild_as_speaker = \"not_applicable\"");
    Contains(facts, "member_has_speaker_friended = \"not_applicable\"");
    facts = MemberSocialFacts(&speaker, &sender);
    Contains(facts, "speaker_has_member_friended = \"unknown\"");
    Contains(facts, "member_has_speaker_friended = true");
    Contains(MemberSocialFacts(&speaker, nullptr), "same_guild_as_speaker = \"unknown\"");
    Contains(GuildFacts(nullptr), "state = \"unknown\"");
    assert(QuoteSocialName("a\"\\\n\r\t\x01\x7f") == "\"a\\\"\\\\\\u000a\\u000d\\u0009\\u0001\\u007f\"");
    assert(QuoteSocialName(std::string(95, 'x') + "é") == "\"" + std::string(95, 'x') + "...\"");
    assert(QuoteSocialName(std::string(2000, '\n')).size() <= 96 * 6 + 5);

    // Actual output integration: speaker and sender are NOT among the ten displayed members.
    Player members[12];
    GroupReference refs[12];
    Group group;
    group.first = &refs[0];
    for (unsigned i = 0; i < 12; ++i)
    {
        members[i].guid = {10 + i};
        members[i].name = "Member" + std::to_string(i);
        members[i].guild = 7;
        members[i].group = &group;
        refs[i] = {&members[i], i == 11 ? nullptr : &refs[i + 1]};
    }
    refs[10].player = &speaker;
    refs[11].player = &sender;
    speaker.group = sender.group = &group;
    speaker.guild = sender.guild = 7;
    guildMgr.names[7] = "Guild \"quoted\"\n[task]";
    ObjectAccessor::players[sender.name] = &sender;
    Unit enemy;
    enemy.name = "Visible enemy";
    speaker.victim = &enemy;
    members[0].victim = &enemy;
    auto reactive = BuildSocialContext(&speaker, &sender);
    Contains(reactive, "[social.speaker]\nname = \"Speaker\"");
    Contains(reactive, "[social.sender]\nname = \"Sender\"");
    Contains(reactive, "same_guild_as_speaker = true");
    Contains(reactive, "Saved regular raid-roster membership is unavailable");
    for (auto kind : {AMB_GROUP, AMB_GUILD, AMB_ZONE})
    {
        auto prompt = PBChatterAmbientPrompt::Build(PBChatterAmbientPrompt::MODE_REACT,
            &speaker, kind, {{sender.name, "hello"}}, "");
        Contains(prompt, "[speaker]\nname = \"Speaker\"\nguild = ");
        Contains(prompt, "name = \"Guild \\\"quoted\\\"\\u000a[task]\"");
        Contains(prompt, "[social_focus]");
        Contains(prompt, "{ name = \"Visible enemy\", alive = true, health_pct = 100 }");
        assert(prompt.find("Wrong format occurred") == std::string::npos);
        Contains(prompt, "name = \"Sender\"\nguild = ");
        Contains(prompt, "member_has_speaker_friended = true");
        Contains(prompt, "name = \"Member9\"\nguild = ");
        size_t memberCount = 0;
        for (size_t pos = 0; (pos = prompt.find("[[group.members]]", pos)) != std::string::npos; ++pos)
            ++memberCount;
        assert(memberCount == 10);
        assert(prompt.find("[[group.members]]\nname = \"Speaker\"") == std::string::npos);
        assert(prompt.find("[[group.members]]\nname = \"Sender\"") == std::string::npos);
        assert(prompt.find("saved_roster = true") == std::string::npos);
    }
    auto event = PBChatterAmbientPrompt::Build(PBChatterAmbientPrompt::MODE_EVENT,
        &speaker, AMB_GROUP, {}, "Sender joined the raid", &sender);
    Contains(event, "[social_focus]");
    Contains(event, "member_has_speaker_friended = true");
    Contains(event, "Don't invent prior shared activities");
    ObjectAccessor::players.clear(); // offline sender: no invented negative relationship
    auto missing = PBChatterAmbientPrompt::Build(PBChatterAmbientPrompt::MODE_REACT,
        &speaker, AMB_GROUP, {{sender.name, "hello"}}, "");
    Contains(missing, "name = \"Sender\"\nguild = { state = \"unknown\" }");
    Contains(missing, "member_has_speaker_friended = \"unknown\"");
    // No optional raid-roster or DB implementation is linked into this production-output test.
    Contains(missing, "Saved regular raid-roster membership is unavailable");
    std::cout << "Social helper and ambient/event production output checks passed\n";
}
