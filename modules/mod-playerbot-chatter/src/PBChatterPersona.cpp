#include "PBChatterPersona.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "Player.h"
#include "QueryResult.h"
#include "SharedDefines.h"
#include "StringFormat.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    enum PersonaRoleMask : uint8
    {
        ROLE_ANY    = 0xFF,
        ROLE_TANK   = 0x01,
        ROLE_HEALER = 0x02,
        ROLE_DPS    = 0x04,
    };

    struct PersonaDef
    {
        char const* id;
        char const* label;
        char const* voice;
        char const* notices;
        char const* quirks;
        char const* avoids;
        uint8 roles;
    };

    PersonaDef const kPersonas[] =
    {
        {
            "dry_veteran", "dry veteran",
            "dry, understated, a little sarcastic, sounds like someone who has cleared too many old raids",
            "bad pathing, odd boss mechanics, threat wobble, weird loot tables, long corpse runs",
            "uses deadpan understatement; can lightly complain without doomposting",
            "pep-talk spam, corporate positivity, big dramatic speeches",
            ROLE_ANY
        },
        {
            "loot_goblin", "loot goblin",
            "excitable about drops, gold, bags, rolls, and tokens, but still casual",
            "loot names, boss drops, vendor trash, bag space, crafting mats, who might want an item",
            "occasionally salty about RNG or bureaucracy; gets happy when real gear appears",
            "claiming something is an upgrade unless facts say so, begging for loot constantly",
            ROLE_ANY
        },
        {
            "nervous_healer", "nervous healer brain",
            "slightly anxious but caring, watches health bars and mana like a hawk",
            "low health, low mana, cleaves, loose adds, overpulls, people standing in bad",
            "short worried asides; relief after messy survival",
            "bragging, blaming the tank every line, asking for mana when mana is fine",
            ROLE_HEALER | ROLE_DPS
        },
        {
            "practical_tactician", "practical tactician",
            "calm, concise, practical, talks like a player making useful observations rather than leading a lecture",
            "positioning, targets, patrols, boss health, adds, line of sight, timing",
            "prefers concrete nouns and short calls; notices room mechanics",
            "generic morale filler, fake strategy for mechanics not listed in facts",
            ROLE_ANY
        },
        {
            "cocky_dps", "cocky dps",
            "playfully overconfident, competitive, mildly parse-brained, but not toxic",
            "boss health, big crits, threat danger, target swaps, fast kills, annoying downtime",
            "trash-talks mobs more than teammates; jokes about threat as a personal problem",
            "serious raid-leader tone, insulting teammates, claiming meters that are not listed",
            ROLE_DPS
        },
        {
            "tired_tank", "tired tank",
            "weary, blunt, protective, sounds like someone used to being blamed for everything",
            "frontals, loose mobs, taunt immunity, healer mana, pathing, people standing ahead of the tank",
            "short practical grumbles; dry comments about boss movement",
            "constant orders, fake authority, talking about tank problems when not tanking",
            ROLE_TANK | ROLE_DPS
        },
        {
            "curious_lore_nerd", "curious lore nerd",
            "curious and observant, notices places, factions, ruins, creatures, and item names",
            "zone names, raid atmosphere, boss names, relics, ruins, weird mobs, quest flavor",
            "asks tiny rhetorical questions or makes small lore-flavored observations",
            "long lore dumps, sounding like an NPC narrator, inventing lore facts",
            ROLE_ANY
        },
        {
            "chaos_gremlin", "chaos gremlin",
            "mischievous, amused by messy pulls and weird outcomes, still cooperative",
            "near-disasters, odd mechanics, goofy mob names, bad RNG, accidental comedy",
            "drops quick jokes when things get strange; laughs at chaos after survival",
            "derailing serious moments, pretending wipes happened, overusing memes",
            ROLE_ANY
        },
        {
            "quiet_pro", "quiet pro",
            "low-key competent, sparse, observant, rarely excited",
            "specific targets, clean recoveries, cooldown windows, mana state, useful loot",
            "short matter-of-fact lines; one concrete observation at a time",
            "small-talk storms, exclamation-heavy hype, repeating catchphrases",
            ROLE_ANY
        },
        {
            "crafting_packrat", "crafting packrat",
            "practical and a little material-obsessed, notices bags, reagents, repairs, and economy",
            "scarabs, idols, cloth, ore, herbs, enchanting mats, repair vendors, AH prices",
            "turns loot and trash into small economic comments",
            "talking about professions when no items/materials/place facts support it",
            ROLE_ANY
        },
        {
            "road_weary_adventurer", "road-weary adventurer",
            "friendly but tired, grounded in travel, terrain, dungeon rooms, and the pace of the run",
            "long runs, caves, hallways, roads, flight paths, corpse distance, instance atmosphere",
            "small immersive player comments without roleplay acting",
            "grand speeches, pretending to be an NPC, motivational spam",
            ROLE_ANY
        },
        {
            "button_masher", "button masher",
            "casual, impulsive, a little impatient, wants to press buttons and see loot",
            "downtime, boss health, annoying immunity phases, adds, visible targets, simple plans",
            "blurts short reactions; sometimes admits confusion with mechanics",
            "deep strategy essays, fake expertise, yelling at teammates",
            ROLE_DPS | ROLE_TANK
        },
    };

    struct PersonaRecord
    {
        std::string id;
        uint8 spice = 3;
    };

    std::mutex g_mutex;
    std::unordered_map<uint64, PersonaRecord> g_cache;
    bool g_initialized = false;

    uint8 RoleMask(Player* bot)
    {
        if (!bot)
            return ROLE_ANY;
        if (bot->HasTankSpec())
            return ROLE_TANK;
        if (bot->HasHealSpec())
            return ROLE_HEALER;
        return ROLE_DPS;
    }

    uint64 HashBot(Player* bot)
    {
        uint64 h = bot ? bot->GetGUID().GetCounter() : 1;
        if (bot)
            for (char c : bot->GetName())
                h = (h * 1315423911u) ^ static_cast<unsigned char>(c);
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;
        return h;
    }

    PersonaDef const& DefaultPersona()
    {
        return kPersonas[0];
    }

    PersonaDef const* FindPersona(std::string const& id)
    {
        for (PersonaDef const& def : kPersonas)
            if (id == def.id)
                return &def;
        return nullptr;
    }

    PersonaRecord AssignPersona(Player* bot)
    {
        uint8 role = RoleMask(bot);
        std::vector<PersonaDef const*> candidates;
        for (PersonaDef const& def : kPersonas)
            if (def.roles == ROLE_ANY || (def.roles & role))
                candidates.push_back(&def);

        uint64 h = HashBot(bot);
        PersonaDef const* def = candidates.empty() ? &DefaultPersona() : candidates[h % candidates.size()];
        PersonaRecord rec;
        rec.id = def->id;
        rec.spice = static_cast<uint8>(1 + ((h >> 8) % 5));
        return rec;
    }

    void EnsureTable()
    {
        CharacterDatabase.Execute(
            "CREATE TABLE IF NOT EXISTS `mod_playerbot_chatter_persona` ("
            "`guid` BIGINT UNSIGNED NOT NULL,"
            "`persona_id` VARCHAR(64) NOT NULL,"
            "`spice` TINYINT UNSIGNED NOT NULL DEFAULT 3,"
            "`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "`updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
            "PRIMARY KEY (`guid`)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    }

    void SavePersona(uint64 guid, PersonaRecord const& rec)
    {
        std::string id = rec.id;
        CharacterDatabase.EscapeString(id);
        CharacterDatabase.Execute(Acore::StringFormat(
            "INSERT INTO `mod_playerbot_chatter_persona` (`guid`, `persona_id`, `spice`) "
            "VALUES ({}, '{}', {}) "
            "ON DUPLICATE KEY UPDATE `persona_id` = VALUES(`persona_id`), `spice` = VALUES(`spice`)",
            guid, id, rec.spice));
    }

    PersonaRecord GetOrCreate(Player* bot)
    {
        uint64 guid = bot ? bot->GetGUID().GetCounter() : 0;
        if (!guid)
            return AssignPersona(bot);

        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_cache.find(guid);
        if (it != g_cache.end())
            return it->second;

        PersonaRecord rec = AssignPersona(bot);
        g_cache[guid] = rec;
        if (g_initialized)
            SavePersona(guid, rec);
        return rec;
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

    char const* SpiceText(uint8 spice)
    {
        switch (spice)
        {
            case 1: return "very subtle; almost all lines should sound like normal practical chat";
            case 2: return "subtle; occasional flavor, mostly grounded chat";
            case 3: return "moderate; persona should color word choice without becoming a bit";
            case 4: return "noticeable; more jokes, opinions, or texture when facts give an opening";
            case 5: return "spicy; sharper jokes or stronger opinions are okay, but still short and grounded";
            default: return "moderate; persona should color word choice without becoming a bit";
        }
    }
}

void PBChatterPersona::Initialize()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cache.clear();
    EnsureTable();

    if (QueryResult res = CharacterDatabase.Query("SELECT `guid`, `persona_id`, `spice` FROM `mod_playerbot_chatter_persona`"))
    {
        do
        {
            Field* f = res->Fetch();
            uint64 guid = f[0].Get<uint64>();
            PersonaRecord rec;
            rec.id = f[1].Get<std::string>();
            rec.spice = std::max<uint8>(1, std::min<uint8>(5, f[2].Get<uint8>()));
            if (!FindPersona(rec.id))
                rec.id = DefaultPersona().id;
            g_cache[guid] = std::move(rec);
        } while (res->NextRow());
    }

    g_initialized = true;
    LOG_INFO("server.loading", "[PlayerbotChatter] Loaded {} persistent chatter personas.", g_cache.size());
}

std::string PBChatterPersona::BuildPromptBlock(Player* bot)
{
    PersonaRecord rec = GetOrCreate(bot);
    PersonaDef const* def = FindPersona(rec.id);
    if (!def)
        def = &DefaultPersona();

    return Acore::StringFormat(
        "\n[persona]\n"
        "id = {}\n"
        "label = {}\n"
        "voice = {}\n"
        "notices = {}\n"
        "quirks = {}\n"
        "avoids = {}\n"
        "spice = {}\n"
        "spice_guidance = {}\n"
        "instruction = \"Let this persona subtly color word choice and priorities. Do not announce or explain the persona. Do not force a gimmick every line. Stay grounded in the current facts.\"\n",
        TomlQuote(def->id),
        TomlQuote(def->label),
        TomlQuote(def->voice),
        TomlQuote(def->notices),
        TomlQuote(def->quirks),
        TomlQuote(def->avoids),
        rec.spice,
        TomlQuote(SpiceText(rec.spice)));
}
