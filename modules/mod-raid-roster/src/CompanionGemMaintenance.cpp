#include "CompanionGemCatalog.h"
#include "CompanionGemPlanner.h"
#include "RaidRosterStore.h"

#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PlayerScript.h"
#include "StatsWeightCalculator.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <atomic>
#include <memory>
#include <unordered_set>

namespace CompanionGems
{
    constexpr char StateKey[] = "raid-roster.companion-gems";

    struct CatalogGem
    {
        Option option;
        uint32_t quality;
        std::vector<Requirement> requirements;
    };

    struct Settings
    {
        bool enabled = false;
        uint32_t intervalMs = 300000;
        uint32_t minimumLevel = 61;
        uint32_t epicPercent = 20;
        std::unordered_set<uint32_t> eligible;
        std::vector<CatalogGem> gems;
    };

    // Registry/catalog publication is world-thread-only. Map threads read immutable snapshots.
    std::atomic<std::shared_ptr<Settings const>> settings{std::make_shared<Settings>()};

    struct State : DataMap::Base
    {
        uint32_t remainingMs = 2000;
        bool running = false;
    };

    bool ReadRequirements(uint32_t condition, std::vector<Requirement>& result)
    {
        if (!condition)
            return true;
        auto const* entry = sSpellItemEnchantmentConditionStore.LookupEntry(condition);
        if (!entry)
            return false;
        for (uint8_t i = 0; i < 5; ++i)
        {
            if (!entry->Color[i])
                continue;
            if (entry->Color[i] > 4 || entry->CompareColor[i] > 4 ||
                (entry->Comparator[i] != 2 && entry->Comparator[i] != 3 && entry->Comparator[i] != 5))
                return false;
            result.push_back({entry->Color[i], entry->Comparator[i], entry->CompareColor[i], entry->Value[i]});
        }
        return true;
    }

    void RefreshSettings()
    {
        auto next = std::make_shared<Settings>();
        next->enabled = sConfigMgr->GetOption<bool>("CompanionMaintenance.SocketGems.Enable", false);
        next->intervalMs = std::clamp(sConfigMgr->GetOption<uint32>(
            "CompanionMaintenance.SocketGems.IntervalSeconds", 300), 30u, 3600u) * 1000;
        next->minimumLevel = std::clamp(sConfigMgr->GetOption<uint32>(
            "CompanionMaintenance.SocketGems.MinLevel", 61), 1u, 80u);
        next->epicPercent = std::min(sConfigMgr->GetOption<uint32>(
            "CompanionMaintenance.SocketGems.EpicPercent", 20), 100u);
        if (next->enabled)
        {
            next->eligible = RaidRosterStore::AllPinnedBots();
            // Friend ownership, not transient masters/groups. Ignore both random and addclass
            // accounts as owners, even if an account is not currently in the random-login pool.
            std::unordered_set<uint32_t> botAccounts;
            if (QueryResult accounts = PlayerbotsDatabase.Query(
                "SELECT account_id FROM playerbots_account_type WHERE account_type IN (1, 2)"))
            {
                do
                {
                    botAccounts.insert(accounts->Fetch()[0].Get<uint32>());
                } while (accounts->NextRow());
            }
            // This additive gem-only scope intentionally does not depend on the friend protection cap.
            if (QueryResult friends = CharacterDatabase.Query(
                "SELECT DISTINCT cs.friend, c.account FROM character_social cs "
                "INNER JOIN characters c ON c.guid = cs.guid WHERE (cs.flags & 1) = 1"))
            {
                do
                {
                    Field* fields = friends->Fetch();
                    uint32_t account = fields[1].Get<uint32>();
                    if (account && !botAccounts.count(account) && !sPlayerbotAIConfig.IsInRandomAccountList(account))
                        next->eligible.insert(fields[0].Get<uint32>());
                } while (friends->NextRow());
            }
            for (uint32_t id : BcGemItems)
            {
                ItemTemplate const* item = sObjectMgr->GetItemTemplate(id);
                if (!item || item->Class != ITEM_CLASS_GEM || item->Quality < ITEM_QUALITY_RARE ||
                    item->Quality > ITEM_QUALITY_EPIC || item->Bonding != NO_BIND || item->RequiredSkill ||
                    item->ItemLimitCategory || item->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE) || item->Duration)
                    continue;
                auto const* props = sGemPropertiesStore.LookupEntry(item->GemProperties);
                auto const* enchant = props ?
                    sSpellItemEnchantmentStore.LookupEntry(props->spellitemenchantement) : nullptr;
                if (!enchant || enchant->GemID != id || enchant->requiredSkill || !props->color || props->color > 14 ||
                    ((props->color & SOCKET_COLOR_META) && props->color != SOCKET_COLOR_META))
                    continue;
                CatalogGem gem{{id, props->spellitemenchantement, uint8_t(props->color), 0}, item->Quality, {}};
                if (ReadRequirements(enchant->EnchantmentCondition, gem.requirements))
                    next->gems.push_back(std::move(gem));
            }
        }
        settings.store(std::move(next));
    }

    bool IsCompanion(Player* player, Settings const& config)
    {
        if (!config.enabled || !player || !player->GetSession() || !player->GetSession()->IsBot() ||
            !config.eligible.count(player->GetGUID().GetCounter()))
            return false;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        return ai && !ai->IsRealPlayer();
    }

    bool SafeToMaintain(Player* player)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        return ai && !ai->IsRealPlayer() && player->IsInWorld() && !player->IsDuringRemoveFromWorld() &&
               !player->IsBeingTeleported() && !player->GetSession()->isLogingOut() && player->IsAlive() &&
               !player->IsInCombat() && ai->GetState() != BOT_STATE_COMBAT && !player->GetTradeData() &&
               !player->IsNonMeleeSpellCast(false);
    }

    bool Editable(Item* item, Player* player)
    {
        return item && item->GetOwnerGUID() == player->GetGUID() && item->IsEquipped() &&
               !item->IsBroken() && !item->IsInTrade() && !item->IsRefundable() && !item->IsBOPTradable();
    }

    uint8_t SocketColor(Item* item, uint8_t index)
    {
        uint8_t color = item->GetTemplate()->Socket[index].Color;
        if (!color && item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT))
        {
            uint8_t first = 0;
            while (first < MAX_GEM_SOCKETS && item->GetTemplate()->Socket[first].Color)
                ++first;
            if (first == index)
                return SOCKET_COLOR_RED | SOCKET_COLOR_YELLOW | SOCKET_COLOR_BLUE;
        }
        return color;
    }

    // Mirror the native socket handler's stat/bonus/meta update order without allocating
    // inventory items. This is explicitly free gem provision, not a purchase or loot claim.
    // Never touch permanent enchants, occupied sockets, bags, money or gear identity.
    bool ApplyGem(Player* player, Item* item, uint8_t index, Option const& gem)
    {
        if (!Editable(item, player) || index >= MAX_GEM_SOCKETS ||
            item->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + index)))
            return false;
        uint8_t color = SocketColor(item, index);
        if (!color || ((color == SOCKET_COLOR_META) != (gem.color == SOCKET_COLOR_META)))
            return false;
        auto const* enchant = sSpellItemEnchantmentStore.LookupEntry(gem.enchant);
        if (!enchant || enchant->GemID != gem.item ||
            (gem.color == SOCKET_COLOR_META && !player->EnchantmentFitsRequirements(enchant->EnchantmentCondition, -1)))
            return false;
        uint8_t slot = item->GetSlot();
        bool oldBonus = item->GemsFitSockets();
        player->ToggleMetaGemsActive(slot, false);
        for (uint8_t i = 0; i < MAX_GEM_SOCKETS; ++i)
            player->ApplyEnchantment(item, EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i), false);
        item->SetEnchantment(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + index), gem.enchant, 0, 0, player->GetGUID());
        for (uint8_t i = 0; i < MAX_GEM_SOCKETS; ++i)
            player->ApplyEnchantment(item, EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i), true);
        bool newBonus = item->GemsFitSockets();
        if (oldBonus != newBonus)
        {
            player->ApplyEnchantment(item, BONUS_ENCHANTMENT_SLOT, false);
            item->SetEnchantment(BONUS_ENCHANTMENT_SLOT, newBonus ? item->GetTemplate()->socketBonus : 0,
                                 0, 0, player->GetGUID());
            player->ApplyEnchantment(item, BONUS_ENCHANTMENT_SLOT, true);
        }
        player->ToggleMetaGemsActive(slot, true);
        item->SendUpdateSockets(); // SetEnchantment already marks the item changed for normal saving.
        return true;
    }

    struct Target
    {
        ObjectGuid item;
        uint8_t index;
        uint8_t color;
    };

    void FillSockets(Player* player, Settings const& config)
    {
        std::vector<Target> normal;
        std::vector<Target> metas;
        Counts fixed{};
        std::vector<Requirement> existingRules;
        std::vector<Requirement> activeRules;
        for (uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || item->IsBroken() || !item->HasSocket())
                continue;
            for (uint8_t i = 0; i < MAX_GEM_SOCKETS; ++i)
            {
                uint32_t current = item->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i));
                if (current)
                {
                    auto const* enchant = sSpellItemEnchantmentStore.LookupEntry(current);
                    auto const* proto = enchant ? sObjectMgr->GetItemTemplate(enchant->GemID) : nullptr;
                    auto const* props = proto ? sGemPropertiesStore.LookupEntry(proto->GemProperties) : nullptr;
                    if (!props)
                        continue;
                    fixed = AddColor(fixed, props->color);
                    if (props->color == SOCKET_COLOR_META)
                    {
                        std::vector<Requirement> rules;
                        if (!ReadRequirements(enchant->EnchantmentCondition, rules))
                            return; // Cannot prove that manual meta activation will be preserved.
                        existingRules.insert(existingRules.end(), rules.begin(), rules.end());
                        if (player->EnchantmentFitsRequirements(enchant->EnchantmentCondition, -1))
                            activeRules.insert(activeRules.end(), rules.begin(), rules.end());
                    }
                    continue;
                }
                uint8_t color = SocketColor(item, i);
                if (color && Editable(item, player))
                    (color == SOCKET_COLOR_META ? metas : normal).push_back({item->GetGUID(), i, color});
            }
        }
        if (normal.empty() && metas.empty())
            return;

        StatsWeightCalculator calculator(player); // existing loaded-role + talent-spec weights
        std::vector<CatalogGem> available;
        for (CatalogGem gem : config.gems)
        {
            auto const* proto = sObjectMgr->GetItemTemplate(gem.option.item);
            auto const* enchant = sSpellItemEnchantmentStore.LookupEntry(gem.option.enchant);
            if (!proto || !enchant || proto->RequiredLevel > player->GetLevel() ||
                enchant->requiredLevel > player->GetLevel() || player->CanUseItem(proto) != EQUIP_ERR_OK)
                continue;
            gem.option.score = calculator.CalculateEnchant(gem.option.enchant);
            if (std::isfinite(gem.option.score) && gem.option.score > 0)
                available.push_back(std::move(gem));
        }
        std::vector<std::vector<Option>> choices;
        for (Target const& target : normal)
        {
            bool epic = EpicSocket(player->GetGUID().GetCounter(), target.item.GetCounter(), target.index,
                                   config.epicPercent);
            std::map<uint8_t, Option> byColor;
            for (CatalogGem const& gem : available)
            {
                if ((gem.quality != ITEM_QUALITY_RARE && !(epic && gem.quality == ITEM_QUALITY_EPIC)) ||
                    gem.option.color == SOCKET_COLOR_META || !gem.requirements.empty())
                    continue;
                Option option = gem.option;
                if (option.color & target.color)
                    option.score *= 1.2; // modest socket-color preference, not a whole-loadout optimizer
                auto found = byColor.find(option.color);
                if (found == byColor.end() || option.score > found->second.score)
                    byColor[option.color] = option;
            }
            // Blue fallbacks remain available even on epic rolls: never sacrifice role fit
            // or a feasible meta activation just to force a purple-quality gem.
            std::vector<Option> options;
            for (auto const& [color, option] : byColor)
                options.push_back(option);
            if (options.empty())
                return;
            choices.push_back(std::move(options));
        }
        auto plan = Solve(fixed, choices, existingRules);
        // Keep active manual metas; impossible inactive ones stay untouched.
        if (!plan)
            plan = Solve(fixed, choices, activeRules);
        Option meta;
        // Standard BC gear has one meta socket. Handle one empty meta per pass; recheck
        // all existing constraints next pass rather than inventing multi-meta assumptions.
        if (!metas.empty())
        {
            double best = -1;
            for (CatalogGem const& gem : available)
            {
                if (gem.option.color != SOCKET_COLOR_META)
                    continue;
                auto rules = existingRules;
                rules.insert(rules.end(), gem.requirements.begin(), gem.requirements.end());
                auto candidate = Solve(AddColor(fixed, SOCKET_COLOR_META), choices, rules);
                if (candidate && candidate->score + gem.option.score > best)
                {
                    best = candidate->score + gem.option.score;
                    plan = std::move(candidate);
                    meta = gem.option;
                }
            }
        }
        if (!plan)
            return;
        uint32_t filled = 0;
        uint32_t epics = 0;
        for (size_t i = 0; i < normal.size(); ++i)
        {
            Target const& target = normal[i];
            if (!ApplyGem(player, player->GetItemByGuid(target.item), target.index, plan->gems[i]))
                return;
            ++filled;
            epics += sObjectMgr->GetItemTemplate(plan->gems[i].item)->Quality == ITEM_QUALITY_EPIC;
        }
        if (meta.item && ApplyGem(player, player->GetItemByGuid(metas.front().item), metas.front().index, meta))
            ++filled;
        if (filled)
            LOG_INFO("module", "[CompanionGems] {} filled {} empty sockets ({} epic); existing gems preserved",
                     player->GetName(), filled, epics);
    }

    void Request(Player* player, Item* item)
    {
        auto config = settings.load();
        if (!item || !item->HasSocket() || !IsCompanion(player, *config))
            return;
        State* state = player->CustomData.GetDefault<State>(StateKey);
        if (!state->running)
            state->remainingMs = std::min(state->remainingMs, 2000u);
    }
}

class CompanionGemWorld : public WorldScript
{
public:
    CompanionGemWorld() : WorldScript("CompanionGemWorld") { }
    void OnAfterConfigLoad(bool /*reload*/) override { remainingMs = 0; }
    void OnStartup() override { CompanionGems::RefreshSettings(); remainingMs = 60000; }
    void OnUpdate(uint32 diff) override
    {
        if (remainingMs > diff)
            remainingMs -= diff;
        else
        {
            CompanionGems::RefreshSettings();
            remainingMs = 60000;
        }
    }
private:
    uint32 remainingMs = 60000;
};

class CompanionGemPlayer : public PlayerScript
{
public:
    CompanionGemPlayer() : PlayerScript("CompanionGemPlayer", {
        PLAYERHOOK_ON_AFTER_UPDATE, PLAYERHOOK_ON_EQUIP, PLAYERHOOK_ON_STORE_NEW_ITEM
    }) { }

    void OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        CompanionGems::Request(player, item);
    }

    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        CompanionGems::Request(player, item);
    }

    void OnPlayerAfterUpdate(Player* player, uint32 diff) override
    {
        auto config = CompanionGems::settings.load();
        if (!CompanionGems::IsCompanion(player, *config) || player->GetLevel() < config->minimumLevel)
            return;
        auto* state = player->CustomData.GetDefault<CompanionGems::State>(CompanionGems::StateKey);
        if (state->running)
            return;
        if (state->remainingMs > diff)
        {
            state->remainingMs -= diff;
            return;
        }
        state->remainingMs = 0;
        if (!CompanionGems::SafeToMaintain(player))
            return;
        state->running = true;
        CompanionGems::FillSockets(player, *config);
        state->running = false;
        state->remainingMs = config->intervalMs + player->GetGUID().GetCounter() % 30000;
    }
};

void AddCompanionGemMaintenanceScripts()
{
    new CompanionGemWorld();
    new CompanionGemPlayer();
}
