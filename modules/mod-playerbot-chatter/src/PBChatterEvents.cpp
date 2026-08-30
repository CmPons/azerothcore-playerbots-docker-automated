#include "PBChatterEvents.h"
#include "PBChatterAmbient.h"
#include "PBChatterAmbientPrompt.h"
#include "PBChatterConfig.h"
#include "PBChatterQueue.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "QuestDef.h"
#include "Playerbots.h"
#include "DBCStores.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Random.h"
#include <deque>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    struct Seed { uint32_t ms; std::string hint; };
    struct GroupEvent { uint32_t ms; std::string text; };

    enum class EventKind
    {
        LevelUp,
        QuestComplete,
        RareLoot,
        EpicLoot,
        BossKill,
        EliteKill,
        PvPContact,
        PvPSighting,
    };

    struct EventReaction
    {
        uint32_t ms = 0;
        uint64_t groupGuid = 0;
        uint64_t anchorGuid = 0;   // real player anchor: proves the group matters to a human
        uint64_t speakerGuid = 0;  // optional bot that noticed the event and should speak
        EventKind kind = EventKind::PvPSighting;
        std::string hint;
    };

    std::unordered_map<uint64_t, Seed> g_seeds; // bot GUID counter -> last notable event seed
    std::unordered_map<uint64_t, std::deque<GroupEvent>> g_groupEvents; // group GUID raw -> recent party/raid events
    std::deque<EventReaction> g_eventReactions; // event-triggered LLM chatter candidates

    // World-thread only state for event-triggered LLM calls.
    uint32_t g_eventNowMs = 0;
    uint32_t g_eventRateWindowMs = 0;
    uint32_t g_eventRateCount = 0;
    uint32_t g_pvpScanTimerMs = 0;
    std::unordered_map<uint64_t, uint32_t> g_groupEventCooldownUntil;
    std::unordered_map<uint64_t, uint32_t> g_botEventCooldownUntil;
    std::unordered_map<uint64_t, uint32_t> g_seenEnemyCooldownUntil;
    std::unordered_map<uint64_t, uint32_t> g_eventDedupeCooldownUntil;

    // The event hooks below fire from Unit kill/level/quest/item handling inside Map::Update,
    // which AzerothCore can run on MapUpdater worker threads. Take()/RecentForGroup() run on
    // the world thread while prompts are built. All shared maps/deques are behind this mutex.
    std::mutex g_eventsMutex;

    constexpr uint32_t GROUP_EVENT_TTL_MS = 4 * 60 * 1000;
    constexpr size_t GROUP_EVENT_MAX = 18;

    bool IsBot(Player* p)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        return ai && !ai->IsRealPlayer();
    }

    bool IsRealPlayer(Player* p)
    {
        if (!p)
            return false;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        return !ai || ai->IsRealPlayer();
    }

    Player* FindByCounter(uint64_t counter)
    {
        return ObjectAccessor::FindPlayer(
            ObjectGuid::Create<HighGuid::Player>(static_cast<ObjectGuid::LowType>(counter)));
    }

    char const* ClassName(uint8 c)
    {
        switch (c)
        {
            case CLASS_WARRIOR: return "warrior";
            case CLASS_PALADIN: return "paladin";
            case CLASS_HUNTER: return "hunter";
            case CLASS_ROGUE: return "rogue";
            case CLASS_PRIEST: return "priest";
            case CLASS_DEATH_KNIGHT: return "death knight";
            case CLASS_SHAMAN: return "shaman";
            case CLASS_MAGE: return "mage";
            case CLASS_WARLOCK: return "warlock";
            case CLASS_DRUID: return "druid";
            default: return "player";
        }
    }

    char const* RaceName(uint8 r)
    {
        switch (r)
        {
            case RACE_HUMAN: return "human";
            case RACE_ORC: return "orc";
            case RACE_DWARF: return "dwarf";
            case RACE_NIGHTELF: return "night elf";
            case RACE_UNDEAD_PLAYER: return "undead";
            case RACE_TAUREN: return "tauren";
            case RACE_GNOME: return "gnome";
            case RACE_TROLL: return "troll";
            case RACE_BLOODELF: return "blood elf";
            case RACE_DRAENEI: return "draenei";
            default: return "player";
        }
    }

    char const* FactionWord(Player* p)
    {
        return p && p->GetTeamId() == TEAM_HORDE ? "Horde" : "Alliance";
    }

    bool OpposingFaction(Player* a, Player* b)
    {
        return a && b && a != b && a->GetTeamId() != b->GetTeamId();
    }

    uint64_t PairKey(uint64_t a, uint64_t b)
    {
        return (a * 11400714819323198485ull) ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
    }

    uint32_t ChanceFor(EventKind kind)
    {
        switch (kind)
        {
            case EventKind::LevelUp:       return g_PBChatEventChanceLevelUp;
            case EventKind::QuestComplete: return g_PBChatEventChanceQuestComplete;
            case EventKind::RareLoot:      return g_PBChatEventChanceRareLoot;
            case EventKind::EpicLoot:      return g_PBChatEventChanceEpicLoot;
            case EventKind::BossKill:      return g_PBChatEventChanceBossKill;
            case EventKind::EliteKill:     return g_PBChatEventChanceEliteKill;
            case EventKind::PvPContact:    return g_PBChatEventChancePvpContact;
            case EventKind::PvPSighting:   return g_PBChatEventChancePvpSighting;
        }
        return g_PBChatEventChance;
    }

    bool RollEventChance(EventKind kind)
    {
        uint32_t chance = ChanceFor(kind);
        if (chance >= 100)
            return true;
        return roll_chance_i(static_cast<int>(chance));
    }

    char const* QualityName(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_RARE:      return "rare";
            case ITEM_QUALITY_EPIC:      return "epic";
            case ITEM_QUALITY_LEGENDARY: return "legendary";
            case ITEM_QUALITY_ARTIFACT:  return "artifact";
            case ITEM_QUALITY_HEIRLOOM:  return "heirloom";
            default:                     return "notable";
        }
    }

    std::string AreaName(uint32 areaId)
    {
        if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(areaId))
            if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
                if (*nm)
                    return nm;
        return "somewhere nearby";
    }

    void Prune(std::deque<GroupEvent>& events, uint32_t now)
    {
        while (!events.empty() && now - events.front().ms > GROUP_EVENT_TTL_MS)
            events.pop_front();
        while (events.size() > GROUP_EVENT_MAX)
            events.pop_front();
    }

    std::string AgePhrase(uint32_t now, uint32_t then)
    {
        uint32_t ageMs = now >= then ? now - then : 0;
        uint32_t sec = ageMs / 1000u;
        if (sec < 10)
            return "just now";
        if (sec < 60)
            return std::to_string(sec) + "s ago";
        uint32_t min = sec / 60u;
        if (min <= 1)
            return "about 1m ago";
        return "about " + std::to_string(min) + "m ago";
    }

    void Stamp(Player* p, std::string hint)
    {
        if (!g_PBChatEnable || !g_PBChatAmbientEnable)
            return;
        if (!p || !IsBot(p))
            return;
        uint64_t key = p->GetGUID().GetCounter();
        uint32_t now = PBChatterAmbient::NowMs();
        std::lock_guard<std::mutex> lock(g_eventsMutex);
        g_seeds[key] = Seed{ now, std::move(hint) };
    }

    void AppendGroupEvent(Player* actor, std::string text)
    {
        if (!g_PBChatEnable)
            return;
        if (!actor || text.empty())
            return;
        Group* group = actor->GetGroup();
        if (!group)
            return;

        uint32_t now = PBChatterAmbient::NowMs();
        uint64_t key = group->GetGUID().GetRawValue();
        std::lock_guard<std::mutex> lock(g_eventsMutex);
        std::deque<GroupEvent>& events = g_groupEvents[key];
        Prune(events, now);

        // Area updates and trash pulls can fire for multiple party members; keep the prompt useful,
        // not spammy. Exact duplicate text within 20s becomes a single event.
        if (!events.empty() && events.back().text == text && now - events.back().ms <= 20000u)
        {
            events.back().ms = now;
            return;
        }

        events.push_back(GroupEvent{ now, std::move(text) });
        Prune(events, now);
    }

    Player* FindRealAnchor(Group* group)
    {
        if (!group)
            return nullptr;
        for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
        {
            Player* m = r->GetSource();
            if (m && m->IsInWorld() && IsRealPlayer(m))
                return m;
        }
        return nullptr;
    }

    void QueueEventReaction(Player* contextPlayer, EventKind kind, std::string hint,
                            Player* preferredSpeaker = nullptr, uint64_t dedupeKey = 0,
                            uint32_t dedupeMs = 30000)
    {
        if (!g_PBChatEnable || !g_PBChatEventEnable || hint.empty())
            return;
        if (!contextPlayer)
            return;
        Group* group = contextPlayer->GetGroup();
        if (!group)
            return;
        Player* anchor = IsRealPlayer(contextPlayer) ? contextPlayer : FindRealAnchor(group);
        if (!anchor)
            return; // never spend model calls for bot-only groups

        uint32_t now = PBChatterAmbient::NowMs();
        uint64_t groupGuid = group->GetGUID().GetRawValue();
        uint64_t speakerGuid = (preferredSpeaker && preferredSpeaker->GetGroup() == group && IsBot(preferredSpeaker))
            ? preferredSpeaker->GetGUID().GetCounter()
            : 0;

        std::lock_guard<std::mutex> lock(g_eventsMutex);
        if (dedupeKey)
        {
            uint32_t& until = g_eventDedupeCooldownUntil[PairKey(groupGuid, dedupeKey)];
            if (now < until)
                return;
            until = now + dedupeMs;
        }

        g_eventReactions.push_back(EventReaction{ now, groupGuid,
                                                  anchor->GetGUID().GetCounter(), speakerGuid, kind, std::move(hint) });
        while (g_eventReactions.size() > 24)
            g_eventReactions.pop_front();
    }

    void RecordKill(Player* killer, Creature* killed, bool pet)
    {
        if (!g_PBChatEnable)
            return;
        if (!killer || !killed)
            return;

        std::string name = killed->GetName();
        if (name.empty())
            return;

        bool const boss = killed->isWorldBoss() || killed->IsDungeonBoss();
        bool const elite = !boss && killed->isElite();
        bool const notable = boss || elite;
        AppendGroupEvent(killer, notable ? ("the party took down " + name)
                                         : ("the party killed " + name));

        // Self-initiated ambient event lines stay reserved for notable kills; normal trash kills
        // are still available as recent party context in other prompts.
        if (notable)
        {
            Stamp(killer, pet ? ("your pet helped take down " + name)
                              : ("you just took down " + name));

            EventKind const kind = boss ? EventKind::BossKill : EventKind::EliteKill;
            std::string hint = boss
                ? Acore::StringFormat("The party just killed boss {} in {}.", name, AreaName(killer->GetAreaId()))
                : Acore::StringFormat("The party just killed elite {} in {}.", name, AreaName(killer->GetAreaId()));
            QueueEventReaction(killer, kind, hint, nullptr,
                PairKey(static_cast<uint64_t>(killed->GetEntry()), killed->GetGUID().GetCounter()), 45000);
        }
    }

    void RecordStoredItem(Player* player, Item* item, uint32 count, char const* verb)
    {
        if (!g_PBChatEnable)
            return;
        if (!player || !item || count == 0)
            return;

        // OnPlayerLootItem was unsafe on the bot-loot path in this server. OnPlayerStoreNewItem
        // hands us the inventory item after it exists; still keep handling conservative and only
        // mention quest/green+ items so trash/vendor/crafting noise doesn't dominate party chat.
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return;
        if (proto->Quality < ITEM_QUALITY_UNCOMMON && proto->Class != ITEM_CLASS_QUEST)
            return;

        std::string label = proto->Name1;
        if (label.empty())
            return;
        std::string qty = count > 1 ? (" x" + std::to_string(count)) : "";
        AppendGroupEvent(player, std::string(player->GetName()) + " " + verb + " " + label + qty);

        if (proto->Quality >= ITEM_QUALITY_RARE)
        {
            EventKind const kind = proto->Quality >= ITEM_QUALITY_EPIC ? EventKind::EpicLoot : EventKind::RareLoot;
            std::string hint = Acore::StringFormat("{} just {} {} item {}{}.", player->GetName(), verb,
                QualityName(proto->Quality), label, qty);
            QueueEventReaction(player, kind, hint, IsBot(player) ? player : nullptr,
                PairKey(player->GetGUID().GetCounter(), static_cast<uint64_t>(proto->ItemId)), 45000);
        }
    }

    void RecordPvPCombat(Player* player, Unit* enemy)
    {
        if (!g_PBChatEnable || !player || !enemy || !player->GetGroup())
            return;
        Player* enemyPlayer = dynamic_cast<Player*>(enemy);
        if (!enemyPlayer || !OpposingFaction(player, enemyPlayer))
            return;

        std::string hint = Acore::StringFormat("PvP contact: {} is fighting a {} {} {} named {} nearby",
            player->GetName(), FactionWord(enemyPlayer), RaceName(enemyPlayer->getRace()),
            ClassName(enemyPlayer->getClass()), enemyPlayer->GetName());
        AppendGroupEvent(player, hint);
        QueueEventReaction(player, EventKind::PvPContact, hint, IsBot(player) ? player : nullptr,
            PairKey(player->GetGUID().GetCounter(), enemyPlayer->GetGUID().GetCounter()), 30000);
    }

    PBChatChannel ChannelForGroup(Group* group)
    {
        return group && group->isRaidGroup() ? PBChatChannel::Raid : PBChatChannel::Party;
    }

    void CollectGroupBots(Group* group, std::vector<Player*>& out)
    {
        if (!group)
            return;
        for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
        {
            Player* m = r->GetSource();
            if (m && m->IsInWorld() && m->IsAlive() && IsBot(m))
                out.push_back(m);
        }
    }

    void ProcessEventReactions()
    {
        std::deque<EventReaction> pending;
        {
            std::lock_guard<std::mutex> lock(g_eventsMutex);
            pending.swap(g_eventReactions);
        }

        for (EventReaction const& ev : pending)
        {
            if (!g_PBChatEventEnable)
                continue;
            if (g_PBChatEventMaxPerMin && g_eventRateCount >= g_PBChatEventMaxPerMin)
                break;
            if (ev.ms && g_eventNowMs - ev.ms > 60000u)
                continue; // too stale for an organic event reaction
            if (g_eventNowMs < g_groupEventCooldownUntil[ev.groupGuid])
                continue;
            if (!RollEventChance(ev.kind))
                continue;

            Player* anchor = FindByCounter(ev.anchorGuid);
            if (!anchor || !anchor->IsInWorld())
                continue;
            Group* group = anchor->GetGroup();
            if (!group || group->GetGUID().GetRawValue() != ev.groupGuid)
                continue;

            std::vector<Player*> bots;
            CollectGroupBots(group, bots);

            Player* bot = nullptr;
            if (ev.speakerGuid)
            {
                for (Player* candidate : bots)
                {
                    uint64_t counter = candidate->GetGUID().GetCounter();
                    if (counter == ev.speakerGuid && g_eventNowMs >= g_botEventCooldownUntil[counter])
                    {
                        bot = candidate;
                        break;
                    }
                }
                if (!bot)
                    continue; // a sentry event should be spoken by the bot that noticed it
            }
            else
            {
                std::vector<Player*> pool;
                for (Player* candidate : bots)
                {
                    uint64_t counter = candidate->GetGUID().GetCounter();
                    if (g_eventNowMs < g_botEventCooldownUntil[counter])
                        continue;
                    pool.push_back(candidate);
                }
                if (pool.empty())
                    continue;
                bot = pool[urand(0, pool.size() - 1)];
            }
            PBChatJob job;
            job.botGuid          = bot->GetGUID().GetCounter();
            job.playerGuid       = anchor->GetGUID().GetCounter();
            job.playerName       = anchor->GetName();
            job.channel          = ChannelForGroup(group);
            job.systemPrompt     = g_PBChatSystemPrompt;
            job.prompt           = PBChatterAmbientPrompt::Build(PBChatterAmbientPrompt::MODE_EVENT,
                                                                 bot, AMB_GROUP, {}, ev.hint);
            job.ambient          = true;
            job.ambientKind      = AMB_GROUP;
            job.ambientIdent     = ev.groupGuid;
            job.anchorPlayerGuid = anchor->GetGUID().GetCounter();
            PBChatterQueue::Submit(std::move(job));

            ++g_eventRateCount;
            g_groupEventCooldownUntil[ev.groupGuid] = g_eventNowMs + g_PBChatEventCooldown * 1000u;
            g_botEventCooldownUntil[bot->GetGUID().GetCounter()] =
                g_eventNowMs + g_PBChatEventPerBotCooldown * 1000u;
        }
    }

    class NearbyOpposingPlayerCheck
    {
    public:
        NearbyOpposingPlayerCheck(Player* spotter, Player* anchor, float range)
            : _spotter(spotter), _anchor(anchor), _range(range) { }

        bool operator()(Player* candidate)
        {
            if (!candidate || candidate == _spotter || !candidate->IsInWorld() || !candidate->IsAlive())
                return false;
            if (!OpposingFaction(_anchor, candidate))
                return false;
            if (candidate->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2))
                return false;
            if (!_spotter->IsWithinDistInMap(candidate, _range))
                return false;
            // Keep the callout honest: don't report stealthed/phased/out-of-sight players the bot could not perceive.
            if (!_spotter->CanSeeOrDetect(candidate, false, true))
                return false;
            return true;
        }

    private:
        Player* _spotter;
        Player* _anchor;
        float _range;
    };

    bool RecordVisibleEnemy(Player* realAnchor, Player* spotter, Player* enemy)
    {
        if (!realAnchor || !spotter || !enemy || !realAnchor->GetGroup() || realAnchor->GetGroup() != spotter->GetGroup())
            return false;
        if (!IsRealPlayer(realAnchor) || !IsBot(spotter) || !OpposingFaction(realAnchor, enemy))
            return false;
        if (!enemy->IsAlive() || !enemy->IsInWorld())
            return false;
        if (enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2))
            return false;

        uint64_t groupGuid = realAnchor->GetGroup()->GetGUID().GetRawValue();
        uint64_t seenKey = PairKey(groupGuid, enemy->GetGUID().GetCounter());
        if (g_eventNowMs < g_seenEnemyCooldownUntil[seenKey])
            return false;
        g_seenEnemyCooldownUntil[seenKey] = g_eventNowMs + 90000u;

        std::string hint = Acore::StringFormat("PvP sighting: {} spotted a {} {} {} named {} within {:.0f} yards near the party in {}",
            spotter->GetName(), FactionWord(enemy), RaceName(enemy->getRace()), ClassName(enemy->getClass()),
            enemy->GetName(), g_PBChatEventPvpScanRange, AreaName(spotter->GetAreaId()));
        AppendGroupEvent(realAnchor, hint);
        QueueEventReaction(realAnchor, EventKind::PvPSighting, hint, spotter);
        return true;
    }

    bool ScanGroupedBotForPvPContact(Player* realAnchor, Player* spotter)
    {
        if (!realAnchor || !spotter || !IsBot(spotter) || !spotter->IsInWorld() || !spotter->IsAlive())
            return false;
        if (realAnchor->GetGroup() != spotter->GetGroup())
            return false;
        if (!g_PBChatEventPvpScanBattlegrounds && (spotter->InBattleground() || spotter->InArena()))
            return false;

        std::list<Player*> enemies;
        NearbyOpposingPlayerCheck check(spotter, realAnchor, g_PBChatEventPvpScanRange);
        Acore::PlayerListSearcher<NearbyOpposingPlayerCheck> searcher(spotter, enemies, check);
        Cell::VisitObjects(spotter, searcher, g_PBChatEventPvpScanRange);

        for (Player* enemy : enemies)
            if (RecordVisibleEnemy(realAnchor, spotter, enemy))
                return true;

        return false;
    }

    void ScanPvPContacts()
    {
        if (!g_PBChatEnable || !g_PBChatEventEnable || !g_PBChatAmbientGroup || !g_PBChatEventPvpScanMs || g_PBChatEventPvpScanRange <= 0.0f)
            return;

        // Start from real online players/sessions, not ObjectAccessor::GetPlayers(), so random
        // world bots never become scan anchors and bot-only groups never spend CPU/model budget.
        std::unordered_map<uint64_t, Player*> realAnchorsByGroup;
        sWorldSessionMgr->DoForAllOnlinePlayers([&](Player* player)
        {
            if (!player || !player->IsInWorld() || !IsRealPlayer(player))
                return;
            Group* group = player->GetGroup();
            if (!group)
                return;
            if (!g_PBChatEventPvpScanBattlegrounds && (player->InBattleground() || player->InArena()))
                return;
            realAnchorsByGroup.emplace(group->GetGUID().GetRawValue(), player);
        });

        for (auto const& pair : realAnchorsByGroup)
        {
            Player* realAnchor = pair.second;
            Group* group = realAnchor ? realAnchor->GetGroup() : nullptr;
            if (!group)
                continue;

            std::vector<Player*> bots;
            CollectGroupBots(group, bots);
            for (Player* bot : bots)
                ScanGroupedBotForPvPContact(realAnchor, bot);
        }
    }
}

void PBChatterEvents::Tick(uint32_t diff)
{
    if (!g_PBChatEnable)
        return;

    g_eventNowMs += diff;
    g_eventRateWindowMs += diff;
    if (g_eventRateWindowMs >= 60000u)
    {
        g_eventRateWindowMs = 0;
        g_eventRateCount = 0;
    }

    if (g_PBChatEventPvpScanMs)
    {
        g_pvpScanTimerMs += diff;
        if (g_pvpScanTimerMs >= g_PBChatEventPvpScanMs)
        {
            g_pvpScanTimerMs = 0;
            ScanPvPContacts();
        }
    }

    ProcessEventReactions();
}

bool PBChatterEvents::Take(uint64_t botGuidCounter, uint32_t nowMs, std::string& outHint)
{
    std::lock_guard<std::mutex> lock(g_eventsMutex);
    auto it = g_seeds.find(botGuidCounter);
    if (it == g_seeds.end())
        return false;
    bool fresh = (nowMs - it->second.ms) <= 120000u;
    if (fresh)
        outHint = it->second.hint;
    g_seeds.erase(it);
    return fresh;
}

std::vector<std::string> PBChatterEvents::RecentForGroup(Group* group, uint32_t nowMs, uint32_t maxItems)
{
    std::vector<std::string> out;
    if (!group || maxItems == 0)
        return out;

    uint64_t key = group->GetGUID().GetRawValue();
    std::lock_guard<std::mutex> lock(g_eventsMutex);
    auto it = g_groupEvents.find(key);
    if (it == g_groupEvents.end())
        return out;

    Prune(it->second, nowMs);
    if (it->second.empty())
    {
        g_groupEvents.erase(it);
        return out;
    }

    size_t start = it->second.size() > maxItems ? it->second.size() - maxItems : 0;
    for (size_t i = start; i < it->second.size(); ++i)
        out.push_back(AgePhrase(nowMs, it->second[i].ms) + ": " + it->second[i].text);
    return out;
}

namespace
{
    class PBChatterEventScript : public PlayerScript
    {
    public:
        PBChatterEventScript() : PlayerScript("PBChatterEventScript", {
            PLAYERHOOK_ON_LEVEL_CHANGED,
            PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
            PLAYERHOOK_ON_CREATURE_KILL,
            PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
            PLAYERHOOK_ON_STORE_NEW_ITEM,
            PLAYERHOOK_ON_GROUP_ROLL_REWARD_ITEM,
            PLAYERHOOK_ON_UPDATE_AREA,
            PLAYERHOOK_ON_PLAYER_ENTER_COMBAT,
        }) {}

        // Each hook bails on the enable flags BEFORE touching any game object, so a disabled
        // module does zero work (and never dereferences a hook argument).
        void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
        {
            if (!g_PBChatEnable)
                return;
            if (!player)
                return;
            bool const actorIsBot = IsBot(player);
            AppendGroupEvent(player, std::string(player->GetName()) + " hit level " + std::to_string(player->GetLevel()));
            Stamp(player, "you just dinged level " + std::to_string(player->GetLevel()));
            QueueEventReaction(player, EventKind::LevelUp,
                actorIsBot ? Acore::StringFormat("You just hit level {}.", player->GetLevel())
                           : Acore::StringFormat("{} just hit level {}.", player->GetName(), player->GetLevel()),
                actorIsBot ? player : nullptr,
                PairKey(player->GetGUID().GetCounter(), static_cast<uint64_t>(player->GetLevel())), 60000);
        }

        void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
        {
            if (!g_PBChatEnable)
                return;
            if (!player)
                return;
            std::string title = quest ? quest->GetTitle() : "";
            bool const actorIsBot = IsBot(player);
            AppendGroupEvent(player, title.empty() ? (std::string(player->GetName()) + " finished a quest")
                                                  : (std::string(player->GetName()) + " finished " + title));
            Stamp(player, title.empty() ? "you just finished a quest"
                                        : ("you just finished the quest \"" + title + "\""));
            QueueEventReaction(player, EventKind::QuestComplete,
                title.empty() ? (actorIsBot ? "You just completed a quest."
                                            : Acore::StringFormat("{} just completed a quest.", player->GetName()))
                              : (actorIsBot ? Acore::StringFormat("You just completed quest {}.", title)
                                            : Acore::StringFormat("{} just completed quest {}.", player->GetName(), title)),
                actorIsBot ? player : nullptr,
                PairKey(player->GetGUID().GetCounter(), static_cast<uint64_t>(quest ? quest->GetQuestId() : 0)), 60000);
        }

        void OnPlayerCreatureKill(Player* killer, Creature* killed) override
        {
            RecordKill(killer, killed, false);
        }

        void OnPlayerCreatureKilledByPet(Player* petOwner, Creature* killed) override
        {
            RecordKill(petOwner, killed, true);
        }

        void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override
        {
            RecordStoredItem(player, item, count, "looted");
        }

        void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, RollVote /*voteType*/, Roll* /*roll*/) override
        {
            RecordStoredItem(player, item, count, "won");
        }

        void OnPlayerEnterCombat(Player* player, Unit* enemy) override
        {
            RecordPvPCombat(player, enemy);
        }

        void OnPlayerUpdateArea(Player* player, uint32 /*oldArea*/, uint32 newArea) override
        {
            if (!g_PBChatEnable)
                return;
            if (!player || !player->GetGroup())
                return;
            AppendGroupEvent(player, "the party moved into " + AreaName(newArea));
        }
    };
}

PlayerScript* PBChatterMakeEventScript()
{
    return new PBChatterEventScript();
}
