#include "PBChatterAmbientPrompt.h"
#include "PBChatterAmbient.h"   // for AMB_* kind constants
#include "PBChatterClassifier.h"
#include "PBChatterConfig.h"    // for g_PBChatStyleExamples (file-loaded pool)
#include "PBChatterContext.h"
#include "PBChatterEvents.h"
#include "DBCStores.h"
#include "Group.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"
#include "Random.h"
#include "StringFormat.h"
#include "World.h"
#include <string>
#include <vector>

namespace
{
    char const* ClassName(uint8 c);

    char const* ChannelWord(uint8_t kind)
    {
        switch (kind)
        {
            case AMB_ZONE:  return "the zone's General channel";
            case AMB_GROUP: return "your party/raid chat";
            case AMB_GUILD: return "guild chat";
            default:        return "chat";
        }
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
        return " Grounding rules: Base the line only on current world/party facts, recent conversation, and recent party events listed here. "
               "Do NOT mention wipes, deaths, boss kills, loot, level-ups, quest progress, mana/OOM, repairs, summons, or dungeon mechanics unless those facts are explicitly listed. "
               "Prefer recent events and the current fight/location over generic WoW small talk. If nothing notable is listed, make one small comment about the current visible situation.";
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

    std::string MemberFact(Player* bot, Player* member)
    {
        char const* who = (member == bot) ? "you" : (PBChatterClassifier::IsRealPlayerSender(member) ? "real player teammate" : "bot teammate");
        std::string line = Acore::StringFormat("{} ({}; role {}; level {} {}", member->GetName(), who, RoleName(member), member->GetLevel(), ClassName(member->getClass()));

        if (!member->IsAlive())
            return line + "; dead)";

        line += Acore::StringFormat("; {}% hp", (int)member->GetHealthPct());
        if (member->GetMaxPower(POWER_MANA) > 0)
            line += Acore::StringFormat("; {}% mana", (int)member->GetPowerPct(POWER_MANA));

        line += member->IsInCombat() ? "; in combat" : "; out of combat";
        if (Unit* victim = member->GetVictim())
            line += Acore::StringFormat("; fighting {}", UnitLabel(victim));
        else if (Unit* selected = member->GetSelectedUnit())
            if (selected->IsAlive())
                line += Acore::StringFormat("; selected {}", UnitLabel(selected));

        if (Unit* attacker = FirstAttacker(member))
            if (attacker != member->GetVictim())
                line += Acore::StringFormat("; being attacked by {}", UnitLabel(attacker));

        line += ")";
        return line;
    }

    std::string TacticalNeeds(Player* bot)
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
                out += Acore::StringFormat(" Immediate party need: you're hurt ({}% hp) and fighting {}. It's natural to ask for heals, a peel, or say something like 'get this off me' if it fits.", hp, v->GetName());
            else
                out += Acore::StringFormat(" Immediate party need: you're hurt ({}% hp) in combat. It's natural to ask for heals or help getting mobs off you if it fits.", hp);
        }
        else if (!inCombat && hp <= 35)
            out += Acore::StringFormat(" Party need: you're badly hurt ({}% hp). It's natural to ask for a quick heal, food, or a short pause before the next pull.", hp);

        if (casterish && inCombat && hp <= 55)
            out += " As a caster/ranged type under pressure, it's especially natural to call for someone to peel mobs off you.";

        if (bot->GetMaxPower(POWER_MANA) > 0)
        {
            int mana = (int)bot->GetPowerPct(POWER_MANA);
            if (mana <= 20)
                out += Acore::StringFormat(" Immediate party need: you're nearly OOM ({}% mana). It's natural to ask the party to wait, say you need to drink, or ask for a mana break before another pull.", mana);
            else if (!inCombat && mana <= 35)
                out += Acore::StringFormat(" Party need: your mana is low ({}%). If the party is moving fast, it's natural to ask them to wait a sec while you drink.", mana);
        }

        if (!out.empty())
            out += " Prefer these urgent party needs over quest chatter when they apply.";
        return out;
    }

    // Built-in FALLBACK few-shot STYLE anchors, used only when no external file is loaded
    // (g_PBChatStyleExamples empty — see PlayerbotChatter.StyleExamplesFile and
    // data/style-examples.txt, which is the normal source and is live-editable). The model is
    // told to match the tone/length, not the content. Deliberately varied — statements, LFG, AH,
    // gripes, congrats, plus dry/sarcastic, teasing, and goofy lines — so a small model doesn't
    // collapse into one rut (e.g. opening every line with "anyone ...?") AND so the edge in the
    // system prompt has concrete patterns to imitate. A couple stay warm/wholesome on purpose so
    // the blend reads mostly chill, not all-snark.
    char const* const kExamples[] = {
        "ugh wiped on the last boss in heroic HoL again, healer went oom",
        "wts [Titansteel Bar] x5, pst with an offer",
        "lf1m heroic gundrak, need a tank then we're good",
        "gz on the mount drop man, so jealous",
        "saronite prices on the AH are nuts right now",
        "grizzly hills has the best zone music in the game imo",
        "finally hit 80, what a grind",
        "brb gotta grab water before the next pull",
        "ty for the summon",
        "this sons of hodir rep grind is killing me",
        "tank dc'd mid-pull, classic",
        "man the cooking daily in dalaran takes forever",
        "oh good, another escort quest, hope the npc walks extra slow this time",
        "loot table really said no again, cool cool cool",
        "0 for 40 on this drop, the rng hates me specifically",
        "respecced, hated it, respecced back, there goes my gold",
        "gg tank, bold pull, real bold",
        "nice of the healer to finally show up",
        "pretty sure my hearthstone cooldown is shorter than my attention span",
        "i would lose a 1v1 to a single murloc right now ngl",
    };

    // Topics valid at any level — variety that's never level-inappropriate.
    char const* const kTopicsAny[] = {
        "where you are right now or a quest you're working on",
        "your class or spec — a talent choice, a new ability, how it plays",
        "a profession you're leveling or some mats you need",
        "gold, the auction house, or saving up for something",
        "an alt you're leveling or thinking about rolling",
        "a run of bad luck — a drop that won't drop, a messy pull, bad rolls",
        "looking for a group or a couple more for a dungeon your level",
    };

    // Content topics GATED to the bot's level band, so a level-30 never gets handed a raid topic
    // (the main cause of level-30 bots chatting about Naxx). Each band only references content
    // that bracket actually plays.
    char const* const kTopics_1_19[] = {
        "a low-level dungeon like the Deadmines, Wailing Caverns, or Shadowfang Keep",
        "looking forward to your first mount at level 20",   // WotLK: Apprentice Riding @20, cheap
        "where to quest next as you level up",
        "a tough elite or group quest you could use a hand with",
    };
    char const* const kTopics_20_39[] = {
        "a dungeon like Scarlet Monastery, Razorfen Kraul, or Uldaman",
        "a profession you're skilling up as you level",
        "which zone to level in next",
        "a nasty elite or escort quest",
    };
    char const* const kTopics_40_59[] = {
        "a dungeon like Zul'Farrak, Maraudon, or the Sunken Temple",
        "gear or gold you're picking up while questing",
        "the high-level dungeons (Blackrock Depths, Stratholme, Scholomance)",
        "thinking about heading to Outland soon",
    };
    char const* const kTopics_60_69[] = {
        "leveling through Outland — Hellfire Peninsula or Zangarmarsh",
        "an Outland dungeon like Hellfire Ramparts or the Blood Furnace",
        "pushing through Outland toward Northrend",
    };
    char const* const kTopics_70_79[] = {
        "leveling through Northrend and which zone is next",
        "a Northrend dungeon like Utgarde Keep, The Nexus, or Gundrak",
        "pushing the last stretch to level 80",
        "gear and rep on the way up",
    };
    char const* const kTopics_80[] = {
        "a heroic dungeon run and the emblems from it",
        "a raid you're working on (Naxxramas, Ulduar, Trial of the Crusader, or ICC)",
        "a gear upgrade or a drop you're chasing",
        "daily quests or a rep grind (Sons of Hodir, the Argent Crusade)",
        "PvP — Wintergrasp, a battleground, or arena",
    };

    // Pick a topic appropriate to the bot's level: ~half the time a universal topic, otherwise a
    // level-banded content topic.
    char const* PickTopic(uint8_t level)
    {
        char const* const* band; int bandN;
        if      (level <= 19) { band = kTopics_1_19;  bandN = (int)(sizeof(kTopics_1_19)  / sizeof(*kTopics_1_19)); }
        else if (level <= 39) { band = kTopics_20_39; bandN = (int)(sizeof(kTopics_20_39) / sizeof(*kTopics_20_39)); }
        else if (level <= 59) { band = kTopics_40_59; bandN = (int)(sizeof(kTopics_40_59) / sizeof(*kTopics_40_59)); }
        else if (level <= 69) { band = kTopics_60_69; bandN = (int)(sizeof(kTopics_60_69) / sizeof(*kTopics_60_69)); }
        else if (level <= 79) { band = kTopics_70_79; bandN = (int)(sizeof(kTopics_70_79) / sizeof(*kTopics_70_79)); }
        else                  { band = kTopics_80;    bandN = (int)(sizeof(kTopics_80)    / sizeof(*kTopics_80)); }

        int const anyN = (int)(sizeof(kTopicsAny) / sizeof(*kTopicsAny));
        if (urand(0, 1) == 0)
            return kTopicsAny[urand(0, anyN - 1)];
        return band[urand(0, bandN - 1)];
    }

    char const* PickGroupTopic(Player* bot)
    {
        static char const* const dungeonTopics[] = {
            "the current pull, the last listed event, or the next visible trash pack",
            "a mob that just died or a drop someone just looted, only if listed in recent events",
            "threat, patrols, line of sight, or positioning when the current facts make it relevant",
            "asking for heals, a mana break, or a quick wait only when current facts show you need it",
            "calling for a peel only if mobs are actually on a caster or healer",
            "how the dungeon room or hallway feels right now",
            "the next boss or mechanic only if it is current/recent/visible",
        };
        static char const* const outdoorTopics[] = {
            "nearby mobs, pulls, patrols, or respawns that are current/visible",
            "loot or quest items that just dropped, only if listed in recent events",
            "asking for heals, a mana break, or a quick wait only when current facts show you need it",
            "calling for a peel only if mobs are actually on a caster or healer",
            "the road, camp, cave, village, or terrain you're moving through",
            "where the party should head next without obsessing over the quest log",
            "your role in the party and how the current/last listed fight felt",
        };

        MapEntry const* entry = bot ? sMapStore.LookupEntry(bot->GetMapId()) : nullptr;
        if (entry && entry->IsDungeon())
            return dungeonTopics[urand(0, (int)(sizeof(dungeonTopics) / sizeof(*dungeonTopics)) - 1)];
        return outdoorTopics[urand(0, (int)(sizeof(outdoorTopics) / sizeof(*outdoorTopics)) - 1)];
    }

    // Shared tail: every ambient prompt asks for exactly one short line and forbids meta-talk.
    std::string Tail()
    {
        return "\n\nWrite exactly ONE short chat line and nothing else. Don't mention this "
               "instruction, don't narrate, no quotes, no asterisks.";
    }

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

    std::string PlaceContext(Player* bot)
    {
        if (!bot)
            return "";
        std::string area = AreaName(bot->GetAreaId(), "the immediate area");
        std::string zone = AreaName(bot->GetZoneId(), "the zone");
        std::string map = MapName(bot->GetMapId());
        MapEntry const* entry = sMapStore.LookupEntry(bot->GetMapId());

        std::string out = Acore::StringFormat(" You are currently in {}, within {}, on {}.", area, zone, map);
        if (entry && entry->IsDungeon())
            out += Acore::StringFormat(" This is a {}; chatter should be grounded in what is actually happening in the run right now: current enemies, pulls, packs, bosses, loot, mana/drinks, threat, patrols, positioning, and recent events only when listed — not outdoor solo questing.", entry->IsRaid() ? "raid instance" : "dungeon instance");
        else
            out += " Chatter can reference visible surroundings, nearby mobs, travel, pulls, and loot.";
        return out;
    }

    std::string GroupContext(Player* bot, uint8_t kind)
    {
        if (kind != AMB_GROUP || !bot)
            return "";
        Group* group = bot->GetGroup();
        if (!group)
            return "";

        bool const raid = group->isRaidGroup();
        std::string out = raid
            ? " You are in this raid; these are your teammates, not strangers."
            : " You are in this party; these are your teammates, not strangers.";
        out += Acore::StringFormat(" You are adventuring together, not solo — talk as if current pulls, mobs, travel, loot, and objectives are shared group business. Your party role: {}.", RoleName(bot));
        out += RoleGuidance(bot);
        out += PlaceContext(bot);

        out += raid ? " Current raid facts (authoritative current state — don't recite it):" : " Current party facts (authoritative current state — don't recite it):";
        uint32 shown = 0;
        for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
        {
            Player* member = r->GetSource();
            if (!member)
                continue;
            out += Acore::StringFormat(" {}{}", shown ? "," : "", MemberFact(bot, member));
            if (++shown >= 10)
            {
                out += ", ...";
                break;
            }
        }

        out += TacticalNeeds(bot);

        auto events = PBChatterEvents::RecentForGroup(group, PBChatterAmbient::NowMs(), 8);
        if (!events.empty())
        {
            out += " Recent party events in the last few minutes (background only — mention one naturally if it fits; don't list them):";
            for (std::string const& e : events)
                out += Acore::StringFormat(" {}{}", (&e == &events.front()) ? "" : "; ", e);
        }

        out += " Don't steer every idle line back to quests. Mention quests only when the recent chat/event makes it relevant. Avoid repair-bill jokes unless there was a wipe/death or someone mentioned repairs.";

        QuestContext quest = ActiveQuest(bot);
        if (!quest.title.empty())
            out += Acore::StringFormat(" Your current quest is '{}' around {} if quest context matters. {}",
                quest.title, quest.location,
                quest.nearby ? "It may fit the current area." : "It is not where you are right now, so don't bring it up unless asked.");
        return out;
    }
}

// A few example lines as a style block. Picks a random run so it varies call to call.
// Draws from the file-loaded pool (g_PBChatStyleExamples) when present, else the built-in
// kExamples list above.
std::string PBChatterAmbientPrompt::StyleExamples(int n)
{
    bool const useFile = !g_PBChatStyleExamples.empty();
    int const total = useFile ? (int)g_PBChatStyleExamples.size()
                              : (int)(sizeof(kExamples) / sizeof(kExamples[0]));
    if (total <= 0)
        return "";
    if (n > total) n = total;
    int start = urand(0, total - 1);
    std::string out = "\nMatch this tone and length (don't reuse the content):";
    for (int i = 0; i < n; ++i)
    {
        int idx = (start + i) % total;
        char const* line = useFile ? g_PBChatStyleExamples[idx].c_str() : kExamples[idx];
        out += Acore::StringFormat("\n- {}", line);
    }
    return out;
}

std::string PBChatterAmbientPrompt::Build(int mode, Player* bot, uint8_t kind,
                                          std::vector<std::pair<std::string, std::string>> const& recent,
                                          std::string const& eventHint)
{
    char const* where = ChannelWord(kind);
    std::string groupContext = GroupContext(bot, kind);

    switch (mode)
    {
        case MODE_REACT:
        {
            std::string p = Acore::StringFormat(
                "{}{}{} You're chatting in {}. Here's the recent conversation (oldest first):\n",
                PBChatterContext::BuildGroundedBrief(bot), groupContext, GroundingRules(), where);
            for (auto const& [speaker, text] : recent)
                p += Acore::StringFormat("{}: {}\n", speaker, text);
            p += "Reply directly to the last message like you're part of the conversation — agree, "
                 "joke, answer the question, or add your own take. If there's recent party event "
                 "context, you may reference it casually. Stay true to your own level: if they're "
                 "talking about content you're not high enough for yet, react like a player who "
                 "isn't there yet (curious, or looking forward to it), don't pretend you're doing "
                 "it. Keep it short and natural, don't repeat what they said, and don't start with "
                 "\"anyone\".";
            return p + StyleExamples(2) + Tail();
        }
        case MODE_FLAVOR:
        {
            std::string snap = PBChatterContext::BuildSnapshot(bot);
            return Acore::StringFormat(
                "{}{}{}\n\nYou're chatting in {}. Make a short, casual remark about what you're doing, "
                "what the party just killed/looted, or where you are right now, like a player "
                "thinking out loud. Keep it appropriate to your level. Don't list your stats or "
                "inventory, don't force quest talk, and don't make it sound like a game announcement.{}",
                snap, groupContext, GroundingRules(), where, StyleExamples(2)) + Tail();
        }
        case MODE_EVENT:
        {
            return Acore::StringFormat(
                "{}\n\nPRIMARY EVENT:\n{}\n\nYou're chatting in {}. React naturally to the PRIMARY EVENT in one short party-style line. "
                "The environmental and party facts below are grounding context, but the event is the main reason you're speaking. "
                "Do not invent extra outcomes: no deaths, wipes, loot, upgrades, quest credit, level-ups, kills, or PvP results unless explicitly listed. "
                "For loot, it's safe to say gz/nice drop, but don't claim it's an upgrade unless listed. "
                "For PvP sightings/contact, a brief callout like inc horde warrior is fine, but don't say they attacked or died unless listed. "
                "Never force a catchphrase, and never say \"Ding!\" or \"Quest complete\".{}{}{}",
                PBChatterContext::BuildGroundedBrief(bot), eventHint, where, groupContext, GroundingRules(), StyleExamples(2)) + Tail();
        }
        case MODE_GENERIC:
        default:
        {
            return Acore::StringFormat(
                "{}{}{} You're hanging out and chatting in {}. Bring up this: {}. Say something short "
                "and casual about it, like a real player typing in chat. Stay true to your "
                "character — you're only that level, so don't talk about raids, zones, or content "
                "above your level. Make it a natural comment or statement, NOT a poll to the whole "
                "channel, don't force quest talk, and don't start with \"anyone\".{}",
                PBChatterContext::BuildGroundedBrief(bot), groupContext, GroundingRules(), where,
                (kind == AMB_GROUP) ? PickGroupTopic(bot) : PickTopic(bot->GetLevel()), StyleExamples(3)) + Tail();
        }
    }
}
