#ifndef MOD_AHBOT_PRICE_CALC_H
#define MOD_AHBOT_PRICE_CALC_H

#include "Define.h"

struct ItemTemplate;

// Per-single-unit buy-value band, in copper. The AH bot buyer compares a random
// draw within [minCopper, maxCopper] * stackCount against a listing's buyout.
struct AHPriceBand
{
    uint64 minCopper;
    uint64 maxCopper;
};

// Reimplements mod-ah-bot-plus AuctionHouseBot::CalculateItemValue's deterministic
// ENVELOPE (min/max, not one random sample), reading the live AuctionHouseBot.*
// config keys so it always matches the running config. Includes the deterministic
// AuctionHouseBot::GetAdvancedPricingMultiplier port (per-subclass formulas for
// consumables/gems/trade goods/misc, plus the CategoryMount/CategoryPet quality
// multipliers) AND the per-(class x quality) CategoryQuality matrix (which ships
// with large values, e.g. Recipe/Epic 20x). Remaining simplification: the advanced
// DROP-RATE multiplier (AdvancedListingRules.UseDropRates, off by default) and the
// per-item PriceMinimumCenterBase.OverrideItems floor map (empty by default).
AHPriceBand AHPriceComputeBand(ItemTemplate const* proto);

#endif
