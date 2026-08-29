#include "PBChatterObserver.h"
#include "PBChatterConfig.h"
#include "PBChatterClassifier.h"
#include "PBChatterContext.h"
#include "PBChatterMemory.h"
#include "PBChatterQueue.h"
#include "PBChatterLore.h"
#include "PBChatterAmbient.h"
#include "PBChatterAmbientPrompt.h"   // StyleExamples() — shared few-shot style block
#include "PBChatterEvents.h"
#include "Player.h"
#include "Unit.h"
#include "Group.h"
#include "Guild.h"
#include "Channel.h"
#include "DBCStores.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "World.h"

namespace
{
    char const* ClassName(uint8 c)
    {
        switch (c)
        {
            case CLASS_WARRIOR: return "warrior";    case CLASS_PALADIN: return "paladin";
            case CLASS_HUNTER:  return "hunter";     case CLASS_ROGUE:   return "rogue";
            case CLASS_PRIEST:  return "priest";     case CLASS_DEATH_KNIGHT: return "death knight";
            case CLASS_SHAMAN:  return "shaman";     case CLASS_MAGE:    return "mage";
            case CLASS_WARLOCK: return "warlock";    case CLASS_DRUID:   return "druid";
            default: return "adventurer";
        }
    }

    std::string AreaName(uint32 areaId, char const* fallback);
    std::string MapName(uint32 mapId);

    struct QuestContext
    {
        std::string title;
        std::string location;
        bool nearby = false;
    };

    QuestContext ActiveQuest(Player* player)
    {
        QuestContext ctx;
        if (!player)
            return ctx;
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 qid = player->GetQuestSlotQuestId(slot);
            if (!qid)
                continue;
            Quest const* q = sObjectMgr->GetQuestTemplate(qid);
            if (!q)
                continue;

            ctx.title = q->GetTitle();
            uint32 questZone = q->GetZoneOrSort() > 0 ? uint32(q->GetZoneOrSort()) : 0;
            uint32 questMap = 0;
            if (QuestPOIVector const* pois = sObjectMgr->GetQuestPOIVector(qid))
            {
                for (QuestPOI const& poi : *pois)
                {
                    if (!questZone && poi.AreaId)
                        questZone = poi.AreaId;
                    if (!questMap && poi.MapId)
                        questMap = poi.MapId;
                    if (questZone && questMap)
                        break;
                }
            }

            if (questZone)
                ctx.location = AreaName(questZone, "another zone");
            if (questMap)
            {
                std::string map = MapName(questMap);
                if (ctx.location.empty())
                    ctx.location = map;
                else if (ctx.location != map)
                    ctx.location += Acore::StringFormat(" on {}", map);
            }
            if (ctx.location.empty())
                ctx.location = "an unknown/unspecified area";

            ctx.nearby = (questZone && (questZone == player->GetZoneId() || questZone == player->GetAreaId()))
                || (questMap && questMap == player->GetMapId());
            return ctx;
        }
        return ctx;
    }

    char const* ChannelWord(PBChatChannel channel)
    {
        switch (channel)
        {
            case PBChatChannel::Whisper: return "a whisper";
            case PBChatChannel::Say:     return "nearby /say";
            case PBChatChannel::Party:   return "party chat";
            case PBChatChannel::Raid:    return "raid chat";
            case PBChatChannel::General: return "General chat";
            case PBChatChannel::Guild:   return "guild chat";
        }
        return "chat";
    }

    bool IsSquishyOrCaster(uint8 c)
    {
        switch (c)
        {
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
            case CLASS_DRUID:
            case CLASS_SHAMAN:
            case CLASS_HUNTER:
                return true;
            default:
                return false;
        }
    }

    char const* RoleName(Player* player)
    {
        if (!player)
            return "DPS";
        if (player->HasTankSpec())
            return "Tank";
        if (player->HasHealSpec())
            return "Healer";
        return "DPS";
    }

    std::string RoleGuidance(Player* bot)
    {
        char const* role = RoleName(bot);
        if (std::string(role) == "Tank")
            return " As the party Tank, only talk about threat, loose mobs, healer mana, heals, or cooldowns when the current facts show that problem.";
        if (std::string(role) == "Healer")
            return " As the party Healer, only call for mana breaks, drinking, or mobs-on-healer when the current facts show it.";
        return " As party DPS, only call for peels, heals, or OOM pauses when the current facts show you need them.";
    }

    std::string GroundingRules()
    {
        return "\nGrounding rules: Base your reply only on the player's message, current world/party facts, and recent party events listed here. "
               "Do NOT mention wipes, deaths, boss kills, loot, level-ups, quest progress, mana/OOM, repairs, summons, or dungeon mechanics unless those facts are explicitly listed. "
               "Prefer recent events and the current fight/location over generic WoW small talk. If nothing notable is listed, answer the player or make one small comment about the current visible situation.";
    }

    std::string UnitLabel(Unit* unit)
    {
        if (!unit)
            return "nothing";
        if (unit->IsAlive())
            return Acore::StringFormat("{} ({}% hp)", unit->GetName(), (int)unit->GetHealthPct());
        return Acore::StringFormat("{} (dead)", unit->GetName());
    }

    Unit* FirstAttacker(Player* player)
    {
        if (!player)
            return nullptr;
        for (Unit* attacker : player->getAttackers())
            if (attacker && attacker->IsAlive())
                return attacker;
        return nullptr;
    }

    std::string MemberFactLine(Player* bot, Player* member)
    {
        char const* who = (member == bot) ? "you" : (PBChatterClassifier::IsRealPlayerSender(member) ? "real player" : "party bot");
        std::string line = Acore::StringFormat("- {}: {}, role {}, level {} {}", member->GetName(), who, RoleName(member), member->GetLevel(), ClassName(member->getClass()));

        if (!member->IsAlive())
            return line + ", dead";

        line += Acore::StringFormat(", {}% hp", (int)member->GetHealthPct());
        if (member->GetMaxPower(POWER_MANA) > 0)
            line += Acore::StringFormat(", {}% mana", (int)member->GetPowerPct(POWER_MANA));

        if (member->IsInCombat())
            line += ", in combat";
        else
            line += ", out of combat";

        if (Unit* victim = member->GetVictim())
            line += Acore::StringFormat(", fighting {}", UnitLabel(victim));
        else if (Unit* selected = member->GetSelectedUnit())
            if (selected->IsAlive())
                line += Acore::StringFormat(", selected {}", UnitLabel(selected));

        if (Unit* attacker = FirstAttacker(member))
            if (attacker != member->GetVictim())
                line += Acore::StringFormat(", being attacked by {}", UnitLabel(attacker));

        return line;
    }

    std::string BuildTacticalNeeds(Player* bot)
    {
        if (!bot)
            return "";

        std::string out;
        int hp = (int)bot->GetHealthPct();
        bool inCombat = bot->IsInCombat();
        bool casterish = IsSquishyOrCaster(bot->getClass());

        if (inCombat && hp <= 45)
        {
            if (Unit* v = bot->GetVictim())
                out += Acore::StringFormat("\nImmediate party need: you're hurt ({}% hp) and fighting {}. It is natural to ask for heals, a peel, or say something like 'get this off me' if it fits.", hp, v->GetName());
            else
                out += Acore::StringFormat("\nImmediate party need: you're hurt ({}% hp) in combat. It is natural to ask for heals or help getting mobs off you if it fits.", hp);
        }
        else if (!inCombat && hp <= 35)
            out += Acore::StringFormat("\nParty need: you're badly hurt ({}% hp). It is natural to ask for a quick heal, food, or a short pause before the next pull.", hp);

        if (casterish && inCombat && hp <= 55)
            out += " As a caster/ranged type under pressure, it's especially natural to call for someone to peel mobs off you instead of making unrelated small talk.";

        if (bot->GetMaxPower(POWER_MANA) > 0)
        {
            int mana = (int)bot->GetPowerPct(POWER_MANA);
            if (mana <= 20)
                out += Acore::StringFormat("\nImmediate party need: you're nearly OOM ({}% mana). It's natural to ask the party to wait, say you need to drink, or ask for a mana break before another pull.", mana);
            else if (!inCombat && mana <= 35)
                out += Acore::StringFormat("\nParty need: your mana is low ({}%). If the party is moving fast, it's natural to ask them to wait a sec while you drink.", mana);
        }

        if (!out.empty())
            out += " Prefer these urgent party needs over quest chatter when they apply.";
        return out;
    }

    std::string AreaName(uint32 areaId, char const* fallback)
    {
        if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(areaId))
            if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
                if (*nm)
                    return nm;
        return fallback;
    }

    std::string MapName(uint32 mapId)
    {
        if (MapEntry const* m = sMapStore.LookupEntry(mapId))
            if (char const* nm = m->name[sWorld->GetDefaultDbcLocale()])
                if (*nm)
                    return nm;
        return "the world";
    }

    std::string BuildPlaceContext(Player* bot, Player* sender)
    {
        std::string area = AreaName(bot->GetAreaId(), "the immediate area");
        std::string zone = AreaName(bot->GetZoneId(), "the zone");
        std::string map = MapName(bot->GetMapId());
        MapEntry const* entry = sMapStore.LookupEntry(bot->GetMapId());

        std::string out = Acore::StringFormat(
            "\nCurrent environment: you are in {}, within {}, on {}.", area, zone, map);
        if (entry && entry->IsDungeon())
        {
            out += Acore::StringFormat(
                " This is a {}; ground party chat in what is actually happening in the run right now: current enemies, pulls, patrols, bosses, loot, mana/drinks, threat, positioning, and recent events only when listed. Don't talk as if you're off solo questing outdoors.",
                entry->IsRaid() ? "raid instance" : "dungeon instance");
        }
        else
            out += " Ground party chat in the visible place, nearby mobs, travel, loot, pulls, and immediate surroundings.";

        if (sender && sender->GetMapId() == bot->GetMapId())
        {
            std::string sArea = AreaName(sender->GetAreaId(), "nearby");
            if (sender->GetAreaId() != bot->GetAreaId())
                out += Acore::StringFormat(" {} is currently around {}.", sender->GetName(), sArea);
        }
        return out;
    }

    std::string BuildGroupContext(Player* bot, Player* sender, Group* group, PBChatChannel channel)
    {
        if (!group || (channel != PBChatChannel::Party && channel != PBChatChannel::Raid))
            return "";

        bool const raid = (channel == PBChatChannel::Raid) || group->isRaidGroup();
        std::string out = Acore::StringFormat(
            "\n\nThis was said in {}. You are grouped with {} and adventuring together as {}. "
            "You are NOT questing alone: treat {} as your teammate and talk as if the party's "
            "quests, mobs, pulls, travel, and objectives are things you are doing together. "
            "Use we/us/our naturally when it fits, but don't awkwardly announce that you're in a party.",
            raid ? "raid chat" : "party chat", sender->GetName(), raid ? "a raid" : "a party",
            sender->GetName());

        if (bot->GetMapId() == sender->GetMapId())
        {
            if (bot->GetZoneId() == sender->GetZoneId())
                out += Acore::StringFormat(" You and {} are in the same zone right now.", sender->GetName());
            else
                out += Acore::StringFormat(" You and {} are on the same map, but not the same zone right now.", sender->GetName());
        }
        else
            out += Acore::StringFormat(" You and {} are in the same group, but not on the same map right now.", sender->GetName());

        out += Acore::StringFormat("\nYour party role: {}.", RoleName(bot));
        out += RoleGuidance(bot);
        out += BuildPlaceContext(bot, sender);

        out += raid ? "\nCurrent raid facts (authoritative current state — don't recite it):" : "\nCurrent party facts (authoritative current state — don't recite it):";
        uint32 shown = 0;
        for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
        {
            Player* member = r->GetSource();
            if (!member)
                continue;
            out += "\n" + MemberFactLine(bot, member);
            if (++shown >= 10)
            {
                out += "\n- ...";
                break;
            }
        }

        out += BuildTacticalNeeds(bot);

        auto events = PBChatterEvents::RecentForGroup(group, PBChatterAmbient::NowMs(), 8);
        if (!events.empty())
        {
            out += "\nRecent party events in the last few minutes (background only — use one naturally if relevant; don't list them):";
            for (std::string const& e : events)
                out += Acore::StringFormat("\n- {}", e);
        }

        out += "\nDo NOT steer every reply back to quest logs. Mention quests only when the player asks, the message is quest-related, or the recent event makes it natural. Avoid repair-bill jokes unless someone just wiped/died or mentioned repairs.";

        QuestContext botQuest = ActiveQuest(bot);
        QuestContext senderQuest = ActiveQuest(sender);
        if (!botQuest.title.empty() && !senderQuest.title.empty() && botQuest.title == senderQuest.title)
            out += Acore::StringFormat("\nQuest context if it matters: you and {} both have '{}' around {}. Treat it as a shared objective only when chat is quest-related or that place is where you are now.", sender->GetName(), botQuest.title, botQuest.location);
        else
        {
            if (!botQuest.title.empty())
                out += Acore::StringFormat("\nQuest context if relevant: your quest log includes '{}' around {}. {}",
                    botQuest.title, botQuest.location,
                    botQuest.nearby ? "It may fit the current area." : "It is NOT where you are right now, so don't bring it up unless asked.");
            if (!senderQuest.title.empty())
                out += Acore::StringFormat("\n{}'s quest context if relevant: '{}' around {}. {}",
                    sender->GetName(), senderQuest.title, senderQuest.location,
                    senderQuest.nearby ? "Help naturally if it comes up." : "That is elsewhere, so don't make current party chatter about it.");
        }
        return out;
    }

    std::string BuildPrompt(Player* bot, Player* sender, PBChatChannel channel, Group* group, std::string const& msg)
    {
        std::string p = PBChatterContext::BuildSnapshot(bot);
        p += GroundingRules();
        p += BuildGroupContext(bot, sender, group, channel);
        auto recent = PBChatterMemory::Recent(bot->GetGUID().GetCounter(), sender->GetGUID().GetCounter());
        if (!recent.empty())
        {
            p += Acore::StringFormat(
                "\n\nYou and {} have talked before. This is your memory of those earlier "
                "chats (oldest first) — you DO remember this person, so stay consistent and "
                "never claim it's the first time you've met:", sender->GetName());
            for (auto const& ex : recent)
                p += Acore::StringFormat("\n{}: {}\nYou: {}", sender->GetName(), ex.first, ex.second);
        }
        p += Acore::StringFormat("\n\n{} just said in {}: \"{}\"\nReply briefly, like a normal player chatting back{}{}.",
                                 sender->GetName(), ChannelWord(channel), msg,
                                 (channel == PBChatChannel::Party || channel == PBChatChannel::Raid) ? " as their teammate" : "",
                                 recent.empty() ? "" : ", using what you remember above");
        p += PBChatterAmbientPrompt::StyleExamples(2);
        return p;
    }

    void Enqueue(Player* bot, Player* sender, PBChatChannel channel, std::string const& msg, Group* group = nullptr)
    {
        PBChatJob job;
        job.botGuid       = bot->GetGUID().GetCounter();
        job.playerGuid    = sender->GetGUID().GetCounter();
        job.playerName    = sender->GetName();
        job.channel       = channel;
        job.systemPrompt  = g_PBChatSystemPrompt;
        job.prompt        = BuildPrompt(bot, sender, channel, group, msg);
        job.playerMessage = msg;
        PBChatterQueue::Submit(std::move(job));
    }

    // Whisper path: route a likely factual question to the lore sidecar. Carries the
    // normal reactive prompt as the in-worker fallback, so a sidecar miss still replies.
    void EnqueueLore(Player* bot, Player* sender, std::string const& msg)
    {
        PBChatJob job;
        job.botGuid       = bot->GetGUID().GetCounter();
        job.playerGuid    = sender->GetGUID().GetCounter();
        job.playerName    = sender->GetName();
        job.channel       = PBChatChannel::Whisper;
        job.systemPrompt  = g_PBChatSystemPrompt;
        job.prompt        = BuildPrompt(bot, sender, PBChatChannel::Whisper, nullptr, msg);   // reactive fallback
        job.playerMessage = msg;
        job.lore          = true;
        job.lorePayload   = PBChatterLore::BuildPayload(bot, sender, msg);
        PBChatterQueue::Submit(std::move(job));
    }

    // Drop addon traffic (DBM/Recount/the MultiBot control addon, etc.): it rides the
    // same chat channels but must never become a spoken AI reply. lang carries this.
    bool Eligible(Player* sender, uint32 lang, std::string const& msg)
    {
        return g_PBChatEnable
            && lang != LANG_ADDON
            && PBChatterClassifier::IsRealPlayerSender(sender)
            && !PBChatterClassifier::IsCommand(msg);
    }

    // Core ChatChannels.dbc id for the per-zone General channel.
    constexpr uint32 GENERAL_CHANNEL_ID = 1;

    // Lighter gate for ambient buffer feeds: a real player, non-addon, non-command line.
    // Commands such as "summon" are left for playerbots and must not become reactive
    // replies or ambient chatter context later.
    bool BufferEligible(Player* sender, uint32 lang, std::string const& msg)
    {
        return g_PBChatEnable && g_PBChatAmbientEnable
            && lang != LANG_ADDON
            && PBChatterClassifier::IsRealPlayerSender(sender)
            && !PBChatterClassifier::IsCommand(msg);
    }
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg)
{
    if (Eligible(player, lang, msg) && (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL))
        for (Player* bot : PBChatterClassifier::ResolveSayTargets(player, msg))
            Enqueue(bot, player, PBChatChannel::Say, msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Player* receiver)
{
    if (Eligible(player, lang, msg))
        if (Player* bot = PBChatterClassifier::ResolveWhisperTarget(receiver))
        {
            if (g_PBChatLoreEnable && PBChatterClassifier::IsLikelyQuestion(msg))
                EnqueueLore(bot, player, msg);
            else
                Enqueue(bot, player, PBChatChannel::Whisper, msg);
        }
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group)
{
    if (Eligible(player, lang, msg))
    {
        bool isRaid = (type == CHAT_MSG_RAID || type == CHAT_MSG_RAID_LEADER || type == CHAT_MSG_RAID_WARNING);
        PBChatChannel ch = isRaid ? PBChatChannel::Raid : PBChatChannel::Party;
        for (Player* bot : PBChatterClassifier::ResolveGroupTargets(player, group, msg))
            Enqueue(bot, player, ch, msg, group);
    }
    if (group && BufferEligible(player, lang, msg))
        PBChatterAmbient::OnPlayerLine(AMB_GROUP, group->GetGUID().GetRawValue(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Guild* guild)
{
    if (guild && BufferEligible(player, lang, msg))
        PBChatterAmbient::OnPlayerLine(AMB_GUILD, guild->GetId(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Channel* channel)
{
    if (channel && channel->GetChannelId() == GENERAL_CHANNEL_ID && BufferEligible(player, lang, msg))
        PBChatterAmbient::OnPlayerLine(AMB_ZONE, player->GetZoneId(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}
