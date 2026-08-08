#include "AHPriceCalc.h"
#include "Config.h"
#include "ItemTemplate.h"
#include "StringFormat.h"
#include <algorithm>
#include <cmath>

namespace
{
    // class -> (config suffix, default floor in copper). nullptr suffix = unmapped class.
    struct CatInfo { char const* suffix; uint32 floor; };

    CatInfo CategoryInfo(uint32 itemClass)
    {
        switch (itemClass)
        {
            case ITEM_CLASS_CONSUMABLE:  return { "Consumable", 1000 };
            case ITEM_CLASS_CONTAINER:   return { "Container",  1000 };
            case ITEM_CLASS_WEAPON:      return { "Weapon",     1000 };
            case ITEM_CLASS_GEM:         return { "Gem",        1000 };
            case ITEM_CLASS_ARMOR:       return { "Armor",      1000 };
            case ITEM_CLASS_REAGENT:     return { "Reagent",    1000 };
            case ITEM_CLASS_PROJECTILE:  return { "Projectile",    5 };
            case ITEM_CLASS_TRADE_GOODS: return { "TradeGood",   850 };
            case ITEM_CLASS_GENERIC:     return { "Generic",    1000 };
            case ITEM_CLASS_RECIPE:      return { "Recipe",     1000 };
            case ITEM_CLASS_QUIVER:      return { "Quiver",     1000 };
            case ITEM_CLASS_QUEST:       return { "Quest",      1000 };
            case ITEM_CLASS_KEY:         return { "Key",        1000 };
            case ITEM_CLASS_MISC:        return { "Misc",       1000 };
            case ITEM_CLASS_GLYPH:       return { "Glyph",      1000 };
            default:                     return { nullptr,      1000 };
        }
    }

    char const* QualitySuffix(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_POOR:      return "Poor";
            case ITEM_QUALITY_NORMAL:    return "Normal";
            case ITEM_QUALITY_UNCOMMON:  return "Uncommon";
            case ITEM_QUALITY_RARE:      return "Rare";
            case ITEM_QUALITY_EPIC:      return "Epic";
            case ITEM_QUALITY_LEGENDARY: return "Legendary";
            case ITEM_QUALITY_ARTIFACT:  return "Artifact";
            case ITEM_QUALITY_HEIRLOOM:  return "Heirloom";
            default:                     return "Normal";
        }
    }

    float DefaultQualityMul(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_UNCOMMON:  return 1.8f;
            case ITEM_QUALITY_RARE:      return 1.9f;
            case ITEM_QUALITY_EPIC:      return 2.1f;
            case ITEM_QUALITY_LEGENDARY:
            case ITEM_QUALITY_ARTIFACT:
            case ITEM_QUALITY_HEIRLOOM:  return 3.0f;
            default:                     return 1.0f;
        }
    }

    float MountMultiplier(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_POOR:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityPoor", 1.0f);
            case ITEM_QUALITY_NORMAL:    return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityNormal", 1.0f);
            case ITEM_QUALITY_UNCOMMON:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityUncommon", 1.0f);
            case ITEM_QUALITY_RARE:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityRare", 3000.0f);
            case ITEM_QUALITY_EPIC:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityEpic", 5750.0f);
            case ITEM_QUALITY_LEGENDARY: return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityLegendary", 1.0f);
            case ITEM_QUALITY_ARTIFACT:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityArtifact", 1.0f);
            case ITEM_QUALITY_HEIRLOOM:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityHeirloom", 1.0f);
            default:                     return 1.0f;
        }
    }

    float PetMultiplier(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_POOR:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityPoor", 1.0f);
            case ITEM_QUALITY_NORMAL:    return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityNormal", 1.0f);
            case ITEM_QUALITY_UNCOMMON:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityUncommon", 1.0f);
            case ITEM_QUALITY_RARE:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityRare", 1.0f);
            case ITEM_QUALITY_EPIC:      return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityEpic", 1.0f);
            case ITEM_QUALITY_LEGENDARY: return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityLegendary", 1.0f);
            case ITEM_QUALITY_ARTIFACT:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityArtifact", 1.0f);
            case ITEM_QUALITY_HEIRLOOM:  return sConfigMgr->GetOption<float>("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityHeirloom", 1.0f);
            default:                     return 1.0f;
        }
    }

    // Deterministic per-subclass multiplier, ported verbatim from mod-ah-bot-plus
    // AuctionHouseBot::GetAdvancedPricingMultiplier (AuctionHouseBot.cpp:455). Pure
    // function of subclass/itemLevel/quality with NO randomness, so it applies
    // identically to both ends of the band. Each subclass is gated by its
    // AuctionHouseBot.AdvancedPricing.*.Enabled flag (all default true), matching the
    // original. NOTE: this is separate from the drop-rate multiplier
    // (AdvancedListingRules.UseDropRates), which is off by default and left at 1.0.
    double AdvancedPricingMultiplier(ItemTemplate const* itemProto)
    {
        double advancedPricingMultiplier = 1.0;
        if (itemProto->Class == ITEM_CLASS_CONSUMABLE)
        {
            switch (itemProto->SubClass)
            {
                case ITEM_SUBCLASS_POTION:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Consumable.Potion.Enabled", true))
                        break;
                    double potionMultiplierHelper = std::log(1.0 + (0.08 * itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(potionMultiplierHelper,3.0)) / (1 + (4.0 * potionMultiplierHelper))) + (std::pow(potionMultiplierHelper,2.5));
                    break;
                }
                case ITEM_SUBCLASS_ELIXIR:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Consumable.Elixir.Enabled", true))
                        break;
                    double elixirMultiplierHelper = std::log(1.0 + (1.6 * itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(elixirMultiplierHelper,3.1)) / (1 + (5.0 * elixirMultiplierHelper))) + (0.05 * std::pow(elixirMultiplierHelper,3.2)) - 1.0;
                    break;
                }
                case ITEM_SUBCLASS_FLASK:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Consumable.Flask.Enabled", true))
                        break;
                    advancedPricingMultiplier = (220000 + (250000-220000) * (std::log(itemProto->SellPrice) - std::log(1250)) / (std::log(10000) - std::log(1250))) / itemProto->SellPrice;
                    break;
                }
                default:
                    break;
            }
        }
        else if (itemProto->Class == ITEM_CLASS_GEM && sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Gem.Enabled", true))
        {
            double gemMultiplierHelper = std::log(1.0 + (0.05 * itemProto->ItemLevel));
            advancedPricingMultiplier = ((std::pow(gemMultiplierHelper,1.0)) / (1 + (10.0 * gemMultiplierHelper))) + (std::pow(gemMultiplierHelper,3.0));
        }
        else if (itemProto->Class == ITEM_CLASS_TRADE_GOODS)
        {
            switch (itemProto->SubClass)
            {
                case ITEM_SUBCLASS_CLOTH:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Cloth.Enabled", true))
                        break;
                    double clothMultiplierHelper = std::log(1.0 + (itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(clothMultiplierHelper,2.0)) / (1 + (0.8 * clothMultiplierHelper))) + (0.001 * std::pow(clothMultiplierHelper,3.5)) - 0.3;
                    break;
                }
                case ITEM_SUBCLASS_HERB:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Herb.Enabled", true))
                        break;
                    double herbMultiplierHelper = std::log(1.0 + (5.0 * itemProto->ItemLevel));
                    advancedPricingMultiplier = (std::pow(herbMultiplierHelper,3.0) / (1 + (1.8 * herbMultiplierHelper))) - 4.2;
                    break;
                }
                case ITEM_SUBCLASS_METAL_STONE:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.MetalStone.Enabled", true))
                        break;
                    double metalMultiplierHelper = std::log(1.0 + (75.0 * itemProto->ItemLevel));
                    advancedPricingMultiplier =  ((std::pow(metalMultiplierHelper,3.0)) / (1 + (7.0 * metalMultiplierHelper))) + (0.001 * std::pow(metalMultiplierHelper,3.5)) - 5.2;
                    break;
                }
                case ITEM_SUBCLASS_LEATHER:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Leather.Enabled", true))
                        break;
                    double leatherMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(leatherMultiplierHelper,0.15)) / (1 + (2.0 * leatherMultiplierHelper))) + (0.4 * std::pow(leatherMultiplierHelper,3.0)) - 0.2;
                    break;
                }
                case ITEM_SUBCLASS_ENCHANTING:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Enchanting.Enabled", true))
                        break;
                    double enchantingMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(enchantingMultiplierHelper,0.15)) / (1 + (2.0 * enchantingMultiplierHelper))) + (0.4 * std::pow(enchantingMultiplierHelper,3.0)) - 0.2;
                    break;
                }
                case ITEM_SUBCLASS_ELEMENTAL:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Elemental.Enabled", true))
                        break;
                    advancedPricingMultiplier = 85 - (itemProto->ItemLevel / 0.97);
                    break;
                }
                case ITEM_SUBCLASS_MEAT:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.TradeGood.Meat.Enabled", true))
                        break;
                    double meatMultiplierHelper = std::log(1.0 + (0.5 * itemProto->ItemLevel));
                    advancedPricingMultiplier = ((std::pow(meatMultiplierHelper,3.2)) / (1 + (2.0 * meatMultiplierHelper))) + (0.05 * std::pow(meatMultiplierHelper,3.2)) - 0.1;
                    break;
                }
                default:
                    break;
            }
        }
        else if (itemProto->Class == ITEM_CLASS_MISC)
        {
            switch (itemProto->SubClass)
            {
                case ITEM_SUBCLASS_JUNK:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Misc.Junk.Enabled", true))
                        break;
                    double miscMultiplierHelper = std::log(1.0 + (0.12 * itemProto->ItemLevel));
                    advancedPricingMultiplier = (std::pow(miscMultiplierHelper,3.2) / (1 + miscMultiplierHelper));
                    break;
                }
                case ITEM_SUBCLASS_JUNK_MOUNT:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Misc.Mount.Enabled", true))
                        break;
                    advancedPricingMultiplier = MountMultiplier(itemProto->Quality);
                    break;
                }
                case ITEM_SUBCLASS_JUNK_PET:
                {
                    if (!sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedPricing.Misc.Pet.Enabled", true))
                        break;
                    advancedPricingMultiplier = PetMultiplier(itemProto->Quality);
                    break;
                }
                default:
                    break;
            }
        }

        return advancedPricingMultiplier;
    }
}

AHPriceBand AHPriceComputeBand(ItemTemplate const* proto)
{
    CatInfo cat = CategoryInfo(proto->Class);
    char const* qual = QualitySuffix(proto->Quality);

    // Live config reads (identical keys to mod-ah-bot-plus).
    bool useSellIfHigher = sConfigMgr->GetOption<bool>(
        "AuctionHouseBot.PriceMinimumCenterBase.UseItemSellPriceIfHigher", true);
    float reduce = sConfigMgr->GetOption<float>("AuctionHouseBot.BuyoutVariationReducePercent", 0.15f);
    float add    = sConfigMgr->GetOption<float>("AuctionHouseBot.BuyoutVariationAddPercent", 0.25f);
    uint32 maxClamp = sConfigMgr->GetOption<uint32>("AuctionHouseBot.MaxBuyoutPriceInCopper", 1000000000);
    bool belowEn = sConfigMgr->GetOption<bool>(
        "AuctionHouseBot.BuyoutBelowVendorVariationAddPercentEnabled", true);
    float belowAdd = sConfigMgr->GetOption<float>("AuctionHouseBot.BuyoutBelowVendorVariationAddPercent", 0.25f);
    float accept = sConfigMgr->GetOption<float>("AuctionHouseBot.Buyer.AcceptablePriceModifier", 1.0f);

    uint32 floor = cat.floor;
    float catMul = 1.0f;
    float ilvlMul = 0.0f;
    float classQualMul = 1.0f;
    if (cat.suffix)
    {
        floor  = sConfigMgr->GetOption<uint32>(
            Acore::StringFormat("AuctionHouseBot.PriceMinimumCenterBase.{}", cat.suffix), floor);
        catMul = sConfigMgr->GetOption<float>(
            Acore::StringFormat("AuctionHouseBot.PriceMultiplier.Category.{}", cat.suffix), 1.0f);
        ilvlMul = sConfigMgr->GetOption<float>(
            Acore::StringFormat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.{}", cat.suffix), 0.0f);
        // Per-(class x quality) matrix (AuctionHouseBot.cpp:354). Key has NO dot between
        // "Category" and the class name, and ".Quality" before the quality name — distinct
        // from the single-level ".Category.<name>" multiplier above. The shipped conf sets
        // large values here (e.g. Recipe/Epic 20x, Glyph/Normal 14x, Quest/Rare 7x,
        // Weapon+Armor/Rare 2.5x /Epic 3x), so it is NOT a safe 1.0 simplification.
        classQualMul = sConfigMgr->GetOption<float>(
            Acore::StringFormat("AuctionHouseBot.PriceMultiplier.Category{}.Quality{}", cat.suffix, qual), 1.0f);
    }
    float qualMul = sConfigMgr->GetOption<float>(
        Acore::StringFormat("AuctionHouseBot.PriceMultiplier.Quality.{}", qual), DefaultQualityMul(proto->Quality));

    if (catMul      <= 0.0f) catMul      = 1.0f;
    if (qualMul     <= 0.0f) qualMul     = 1.0f;
    if (classQualMul <= 0.0f) classQualMul = 1.0f;

    // Deterministic per-subclass multiplier (mirrors the original's line 399-400
    // "<= 0 => 1.0" clamp; also guard against non-finite from a degenerate flask calc).
    double advMul = AdvancedPricingMultiplier(proto);
    if (!std::isfinite(advMul) || advMul <= 0.0)
        advMul = 1.0;

    double sell = double(proto->SellPrice);
    double baseBuyout = useSellIfHigher ? sell : 1.0;
    double center = (baseBuyout < double(floor)) ? double(floor) : baseBuyout;

    double lo = center * (1.0 - reduce);
    double hi = center * (1.0 + add);

    lo *= double(qualMul) * double(catMul) * double(classQualMul) * advMul;
    hi *= double(qualMul) * double(catMul) * double(classQualMul) * advMul;

    // Item-level multiplier: only when set, item has a level, AND advanced pricing is
    // inactive (advMul == 1.0) — faithful to the original (AuctionHouseBot.cpp:431).
    if (ilvlMul > 0.0f && proto->ItemLevel > 0 && advMul == 1.0)
    {
        lo *= double(proto->ItemLevel) * double(ilvlMul);
        hi *= double(proto->ItemLevel) * double(ilvlMul);
    }

    lo = std::min(lo, double(maxClamp));
    hi = std::min(hi, double(maxClamp));

    // Below-vendor bump: values under SellPrice are raised into [SellPrice, SellPrice*(1+belowAdd)].
    if (belowEn && sell > 0.0)
    {
        if (lo < sell)
            lo = sell;
        if (hi < sell)
            hi = sell * (1.0 + double(belowAdd));
        else if (lo <= sell) // some draws would have been bumped up to SellPrice*(1+belowAdd)
            hi = std::max(hi, sell * (1.0 + double(belowAdd)));
    }

    lo *= double(accept);
    hi *= double(accept);

    if (hi < lo)
        std::swap(hi, lo);

    AHPriceBand band;
    band.minCopper = uint64(lo < 0.0 ? 0.0 : lo);
    band.maxCopper = uint64(hi < 0.0 ? 0.0 : hi);
    if (band.maxCopper == 0)
        band.maxCopper = 1;
    return band;
}
