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

    Unit* FirstAttacker(Player* player)
    {
        if (!player)
            return nullptr;
        for (Unit* attacker : player->getAttackers())
            if (attacker && attacker->IsAlive())
                return attacker;
        return nullptr;
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

    char const* RaceName(uint8 r)
    {
        switch (r)
        {
            case RACE_HUMAN: return "human";       case RACE_ORC: return "orc";
            case RACE_DWARF: return "dwarf";       case RACE_NIGHTELF: return "night elf";
            case RACE_UNDEAD_PLAYER: return "undead"; case RACE_TAUREN: return "tauren";
            case RACE_GNOME: return "gnome";       case RACE_TROLL: return "troll";
            case RACE_BLOODELF: return "blood elf"; case RACE_DRAENEI: return "draenei";
            default: return "traveler";
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

    std::string TomlQuote(std::string const& in)
    {
        std::string out = "\"";
        for (char c : in)
        {
            switch (c)
            {
                case '\\': out += "\\\\"; break;
                case '\"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        out += "\"";
        return out;
    }

    char const* TomlBool(bool v) { return v ? "true" : "false"; }

    std::string TaxiNodeName(uint32 node)
    {
        if (!node)
            return "";
        if (TaxiNodesEntry const* t = sTaxiNodesStore.LookupEntry(node))
            if (char const* nm = t->name[sWorld->GetDefaultDbcLocale()])
                if (*nm)
                    return nm;
        return "";
    }

    std::string MotionName(Player* bot)
    {
        if (!bot)
            return "unknown";
        if (bot->IsInFlight() || bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
            return "taxi_flight";
        if (bot->IsMounted())
            return "mounted";
        if (bot->IsInCombat())
            return "combat";
        return "normal";
    }

    std::string TomlUnit(Unit* unit)
    {
        if (!unit)
            return "";
        return Acore::StringFormat("{ name = {}, alive = {}, health_pct = {} }",
            TomlQuote(unit->GetName()), TomlBool(unit->IsAlive()), (int)unit->GetHealthPct());
    }

    std::string BuildFactToml(Player* bot, uint8_t kind)
    {
        if (!bot)
            return "[facts]\navailable = false\n";

        std::string area = AreaName(bot->GetAreaId(), "the immediate area");
        std::string zone = AreaName(bot->GetZoneId(), "the zone");
        std::string map = MapName(bot->GetMapId());
        MapEntry const* mapEntry = sMapStore.LookupEntry(bot->GetMapId());
        std::string channel = ChannelWord(kind);
        std::string faction = (bot->GetTeamId() == TEAM_ALLIANCE) ? "Alliance" : "Horde";

        std::string out;
        out += "# TOML-style authoritative current facts. Use as data, not text to copy.\n";
        out += "[speaker]\n";
        out += Acore::StringFormat("name = {}\n", TomlQuote(bot->GetName()));
        out += Acore::StringFormat("level = {}\n", bot->GetLevel());
        out += Acore::StringFormat("race = {}\n", TomlQuote(RaceName(bot->getRace())));
        out += Acore::StringFormat("class = {}\n", TomlQuote(ClassName(bot->getClass())));
        out += Acore::StringFormat("role = {}\n", TomlQuote(RoleName(bot)));
        out += Acore::StringFormat("faction = {}\n", TomlQuote(faction));
        out += "kind = \"playerbot\"\n\n";

        out += "[chat]\n";
        out += Acore::StringFormat("channel = {}\n", TomlQuote(channel));
        out += Acore::StringFormat("is_group_channel = {}\n\n", TomlBool(kind == AMB_GROUP));

        out += "[location]\n";
        out += Acore::StringFormat("map_id = {}\n", bot->GetMapId());
        out += Acore::StringFormat("map = {}\n", TomlQuote(map));
        out += Acore::StringFormat("zone_id = {}\n", bot->GetZoneId());
        out += Acore::StringFormat("zone = {}\n", TomlQuote(zone));
        out += Acore::StringFormat("area_id = {}\n", bot->GetAreaId());
        out += Acore::StringFormat("area = {}\n", TomlQuote(area));
        out += Acore::StringFormat("is_dungeon = {}\n", TomlBool(mapEntry && mapEntry->IsDungeon()));
        out += Acore::StringFormat("is_raid = {}\n\n", TomlBool(mapEntry && mapEntry->IsRaid()));

        out += "[movement]\n";
        out += Acore::StringFormat("state = {}\n", TomlQuote(MotionName(bot)));
        out += Acore::StringFormat("in_flight = {}\n", TomlBool(bot->IsInFlight()));
        out += Acore::StringFormat("mounted = {}\n", TomlBool(bot->IsMounted()));
        uint32 source = bot->m_taxi.GetTaxiSource();
        uint32 dest = bot->m_taxi.GetTaxiDestination();
        out += Acore::StringFormat("taxi_source_node = {}\n", source);
        out += Acore::StringFormat("taxi_source = {}\n", TomlQuote(TaxiNodeName(source)));
        out += Acore::StringFormat("taxi_destination_node = {}\n", dest);
        out += Acore::StringFormat("taxi_destination = {}\n", TomlQuote(TaxiNodeName(dest)));
        out += "taxi_path_nodes = [";
        uint32 shownTaxi = 0;
        for (uint32 node : bot->m_taxi.GetPath())
        {
            if (shownTaxi++)
                out += ", ";
            out += std::to_string(node);
            if (shownTaxi >= 8)
                break;
        }
        out += "]\n\n";

        out += "[combat]\n";
        out += Acore::StringFormat("in_combat = {}\n", TomlBool(bot->IsInCombat()));
        out += Acore::StringFormat("alive = {}\n", TomlBool(bot->IsAlive()));
        out += Acore::StringFormat("health_pct = {}\n", (int)bot->GetHealthPct());
        out += Acore::StringFormat("has_mana = {}\n", TomlBool(bot->GetMaxPower(POWER_MANA) > 0));
        out += Acore::StringFormat("mana_pct = {}\n", bot->GetMaxPower(POWER_MANA) > 0 ? (int)bot->GetPowerPct(POWER_MANA) : -1);
        out += Acore::StringFormat("victim = {}\n", TomlQuote(TomlUnit(bot->GetVictim())));
        out += Acore::StringFormat("first_attacker = {}\n\n", TomlQuote(TomlUnit(FirstAttacker(bot))));

        QuestContext quest = ActiveQuest(bot);
        out += "[quest]\n";
        out += Acore::StringFormat("active_title = {}\n", TomlQuote(quest.title));
        out += Acore::StringFormat("location = {}\n", TomlQuote(quest.location));
        out += Acore::StringFormat("nearby = {}\n\n", TomlBool(quest.nearby));

        if (Group* group = bot->GetGroup())
        {
            out += "[group]\n";
            out += "in_group = true\n";
            out += Acore::StringFormat("is_raid = {}\n", TomlBool(group->isRaidGroup()));
            out += Acore::StringFormat("speaker_role = {}\n\n", TomlQuote(RoleName(bot)));

            uint32 shown = 0;
            for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
            {
                Player* member = r->GetSource();
                if (!member)
                    continue;
                out += "[[group.members]]\n";
                out += Acore::StringFormat("name = {}\n", TomlQuote(member->GetName()));
                out += Acore::StringFormat("kind = {}\n", TomlQuote(member == bot ? "speaker" : (PBChatterClassifier::IsRealPlayerSender(member) ? "real_player" : "playerbot")));
                out += Acore::StringFormat("level = {}\n", member->GetLevel());
                out += Acore::StringFormat("class = {}\n", TomlQuote(ClassName(member->getClass())));
                out += Acore::StringFormat("role = {}\n", TomlQuote(RoleName(member)));
                out += Acore::StringFormat("alive = {}\n", TomlBool(member->IsAlive()));
                out += Acore::StringFormat("health_pct = {}\n", (int)member->GetHealthPct());
                out += Acore::StringFormat("has_mana = {}\n", TomlBool(member->GetMaxPower(POWER_MANA) > 0));
                out += Acore::StringFormat("mana_pct = {}\n", member->GetMaxPower(POWER_MANA) > 0 ? (int)member->GetPowerPct(POWER_MANA) : -1);
                out += Acore::StringFormat("in_combat = {}\n", TomlBool(member->IsInCombat()));
                out += Acore::StringFormat("victim = {}\n", TomlQuote(TomlUnit(member->GetVictim())));
                out += Acore::StringFormat("first_attacker = {}\n\n", TomlQuote(TomlUnit(FirstAttacker(member))));
                if (++shown >= 10)
                    break;
            }

            auto events = PBChatterEvents::RecentForGroup(group, PBChatterAmbient::NowMs(), 8);
            if (!events.empty())
            {
                out += "[recent_events]\n";
                out += "items = [";
                for (size_t i = 0; i < events.size(); ++i)
                {
                    if (i)
                        out += ", ";
                    out += TomlQuote(events[i]);
                }
                out += "]\n\n";
            }
        }
        else
        {
            out += "[group]\nin_group = false\n\n";
        }

        std::string item = PBChatterContext::TopBagItem(bot);
        if (!item.empty())
        {
            out += "[inventory]\n";
            out += Acore::StringFormat("notable_stack_item = {}\n\n", TomlQuote(item));
        }

        return out;
    }

    std::string PromptPreamble()
    {
        return "Use the TOML-style fact block as authoritative current game state. "
               "Do not recite the facts. Do not invent unlisted wipes, deaths, loot, level-ups, quest progress, mana problems, summons, or dungeon mechanics. "
               "Write like the player behind this character typing in WoW chat, not like an NPC.\n\n";
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
    std::string facts = BuildFactToml(bot, kind);

    switch (mode)
    {
        case MODE_REACT:
        {
            std::string p = PromptPreamble();
            p += facts;
            p += Acore::StringFormat("\n[task]\nmode = \"react\"\nchat_channel = {}\ninstructions = {}\n\n",
                TomlQuote(where),
                TomlQuote("Continue the conversation naturally by responding to the last line, not by starting a new unrelated topic. Treat bot speakers as real party/guildmates. Answer, riff, ask a tiny follow-up, disagree lightly, or joke back. Don't just agree by default and don't start most replies with yeah/yea/yep. Stay true to the speaker's level and current facts."));
            p += "[recent_conversation]\n# oldest first\n";
            for (auto const& [speaker, text] : recent)
                p += Acore::StringFormat("line = {}\n", TomlQuote(speaker + ": " + text));
            return p + StyleExamples(2) + Tail();
        }
        case MODE_FLAVOR:
        {
            return PromptPreamble() + facts + Acore::StringFormat(
                "\n[task]\nmode = \"flavor\"\nchat_channel = {}\ninstructions = {}\n{}",
                TomlQuote(where),
                TomlQuote("Make a short, casual remark about what the speaker is doing, what the party just killed/looted, travel state, or where they are right now. Keep it appropriate to their level. Don't list stats or inventory; use them only as background. Don't force quest talk or sound like a game announcement."),
                StyleExamples(2)) + Tail();
        }
        case MODE_EVENT:
        {
            return PromptPreamble() + facts + Acore::StringFormat(
                "\n[primary_event]\nhint = {}\n\n[task]\nmode = \"event\"\nchat_channel = {}\nspeaker_name = {}\ninstructions = {}\n{}",
                TomlQuote(eventHint), TomlQuote(where), TomlQuote(bot ? bot->GetName() : "the bot"),
                TomlQuote("React naturally to the primary event in one short party-style line. If the event is about the speaker, use first person and don't congratulate yourself. Do not invent extra outcomes: no deaths, wipes, loot, upgrades, quest credit, level-ups, kills, or PvP results unless explicitly listed. For loot, gz/nice drop is fine, but don't claim it's an upgrade unless listed. For deaths, sound like a surviving party member reacting briefly; don't claim a wipe, rez, blame, or cause unless listed. For PvP sightings/contact, a brief callout is fine, but don't say they attacked or died unless listed. Never force a catchphrase, and never say Quest complete."),
                StyleExamples(2)) + Tail();
        }
        case MODE_GENERIC:
        default:
        {
            return PromptPreamble() + facts + Acore::StringFormat(
                "\n[task]\nmode = \"generic\"\nchat_channel = {}\ntopic = {}\ninstructions = {}\n{}",
                TomlQuote(where),
                TomlQuote((kind == AMB_GROUP) ? PickGroupTopic(bot) : PickTopic(bot->GetLevel())),
                TomlQuote("Say something short and casual about the topic, like a real player typing in chat. Stay true to the speaker's level and current facts. Make it a natural comment or statement, not a poll to the whole channel. Don't force quest talk and don't start with anyone."),
                StyleExamples(3)) + Tail();
        }
    }
}
