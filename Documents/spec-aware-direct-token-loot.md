# Spec-aware direct token loot

Base policy: **deployed October 1, 2026**, with explicit build/restart authorization.
**Addition deployed October 5:** no repeated class within one generated boss/chest loot
list; see [per-boss class uniqueness](#per-boss-class-uniqueness) below and the
[deployment and qualified preservation review](raid-qol-deployment-20261005.md).
Playerbots source commit: `cef0162a7202f7a10688e989c0dba5d76828dbb1`.
See [deployment and preservation review](spec-token-loot-deployment-20261001.md).
No live config, loot-table, spec or equipment edits were made. The deployment review
separately records startup mail cleanup and normal autonomous companion activity.

This supersedes the fully random policy in `generic-direct-token-loot-plan.md`.
The existing enabled mode (`AiPlayerbot.DirectTokenLoot.Mode = 1`) gains this policy;
there is no new setting to enable. Mode 0/unsupported modes remain non-converting.

## Selection

This describes the original base policy; the deployed October 5 addition below narrows step 3.

For each existing single, unlooted, non-quest token slot in creature or gameobject loot:

1. Resolve its existing quest/vendor gear rewards. Apply the configured quality floor.
2. Remove rewards with positive resilience stats or an honor-rank requirement, paths
   requiring honor/arena points or personal arena rating, and PvP-quest paths. Unknown
   extended costs/quests are not eligible. This is an explicit safety filter, not merely
   a negative resilience score. All 85 Gladiator rewards in the T4 fixture are excluded.
3. Intersect token/reward class masks and any quest class restriction. Merge duplicate
   vendor/quest paths. **Roll uniformly between the remaining classes**, independently
   of raid class counts, represented specs or the number of sets offered per class.
4. Inspect that class's active talent tabs among online, in-world members of the looter's
   group in the **same map instance**. Humans and bots both count; death does not remove
   a spec. Offline/other-instance members do not count. Solo/personal loot uses the owner.
5. For each represented tab, select the highest-scoring usable PvE reward. Scores use the
   existing `StatsWeightCalculator` with its new isolated loot-scoring mode. Aggregate
   repeated members of the same tab without multiplying that tab's chances; exact ties
   choose the lower item ID deterministically. No cross-spec raw-score competition.
6. Deduplicate the winning item IDs and roll between them. Existing duplicate avoidance
   operates **only among these winners**: it may prefer a winner not already in the loot
   window, but never substitutes inferior/off-spec/PvP gear to avoid a duplicate.

If the rolled class is absent or has no finite, usable score, **retain the original
token**; never reroll the class, broaden to PvP, or discard the loot. Unknown/non-token
items, stacked tokens, quest items, already-looted items and unsupported stores retain
existing behavior. Item count and other loot slots are untouched. Replacement suffix,
random-property and free-for-all/follow-loot-rules refresh behavior is preserved.

The valid pool is still the generic resolver's pool, not a new hardcoded raid/tier list.
Currency-like items it previously recognized are still recognized. No new loot sources,
extra drops, ownership checks, automatic equips or retroactive item conversions are added.

## Scoring scope

Only the direct-loot call opts into `StatsWeightCalculator(player, true)`. Ordinary
bot equipment evaluation keeps its prior defaults and adjustments.

Loot scoring reuses the basic class/talent-tab weights, collected item stats/equip
spells, sockets, inherent item quality/level, and weapon-type penalties. It excludes
current hit/defense/etc. caps, armor-penetration gear tuning, existing set completion,
stance/form/bot strategy role detection and aura-dependent talent refinements. It does
not read the current item in the slot or ask whether the reward is an upgrade.
Normal class/race/faction/level/proficiency restrictions still apply.

Active talent tabs come from `AiFactory::GetPlayerSpecTab`, which reads only the active
spec mask. Feral has one shared PvE tier set: loot scoring uses stable cat weights for
that tab, not whichever form the druid happens to occupy when loot is generated.
Death knights retain the scorer's Blood-tank/Frost-Unholy-DPS convention, independent
of temporary presence. No player/bot talents or strategies are changed.

**Scores are not set labels.** The requested highest-score policy does not guarantee
that every healer receives the set Blizzard labeled for healing. Installed T4 checks:

- All five Protection-warrior slots choose Warbringer Armor (654).
- Arms/Fury choose Warbringer Battlegear (655); Protection paladins choose Justicar Armor (625).
- 125 of 135 class/tab/slot cases select the conventionally matching tier set.
- Ten measured crossovers, preserved explicitly in tests rather than hidden:
  - Discipline/Holy priest: gloves 29057 and legs 29059 (caster Incarnate set 664).
  - Restoration shaman: chest 29033, helm 29035, shoulders 29037 (caster Cyclone set 632).
  - Restoration druid: gloves 29092, shoulders 29095, legs 29094 (caster Malorne set 639).

These are the existing basic weights preferring overlapping caster/healer stats with
set-completion bonuses disabled. No weights or guaranteed-set override were added to
force those tests to choose a particular label. Higher tiers still use the same generic
rule; the 135-case acceptance matrix specifically covers installed T4 data, not every tier.

## Validation

```sh
python -m unittest scripts.tests.test_direct_token_loot -v
python scripts/tests/raid_combat_syntax.py --output /tmp/token-loot-syntax \
  modules/mod-playerbots/src/Mgr/Item/DirectTokenLootScript.cpp \
  modules/mod-playerbots/src/Mgr/Item/StatsWeightCalculator.cpp
```

The offline C++ fixture executes the production loot hook and production scorer bodies,
including the basic weights, item-stat collection and flat equip-aura handling. It uses
copied public installed item/vendor/flat spell/socket data, not a running server or raw
client DBC files. It checks all 15 T4 tokens, 170 distinct reward paths, class-first RNG
bounds, mixed specs, duplicate paths/members/winners, every Gladiator exclusion, quest
class restrictions, absent/invalid contexts, negative/nonfinite scores, source/config
gates, item preservation and equipment-mode defaults. A historical pool fixture proves
that the previous implementation admitted Gladiator's Plate Legguards (24547).

Validation passed: **21 focused tests** (including the eight new token-loot tests and
existing tank-mode, selection-only pull and skull-focus regressions), the **135-case T4
matrix**, **seven native-header syntax checks** (loot/scorer plus resolver, item usage,
equip, query-usage and token redemption callers), and the official C++ style checker
on changed native files and the new C++ fixture. All seven C++ fixture scenarios,
including the full matrix, also passed AddressSanitizer and UndefinedBehaviorSanitizer.
A full native image build and binary/runtime verification subsequently passed during
the authorized deployment; see the deployment record.

Native-header `-fsyntax-only` checks are not a full worldserver build. The fixture mocks
world/player/DBC access and flat spell lookup, not live encounter behavior. The new binary is running; acceptance of an actual new boss-token drop remains pending
live gameplay.

When debug logging is enabled, replacements report chosen class and distinct winner
count. Safe non-conversions with a valid pool report the absent/unusable rolled class.

## Per-boss class uniqueness

The user requested **no duplicate class rolls for one boss**, not bad-luck protection
across kills. Published playerbots commit: `8275e8f8f9998136047ee034458bb1e2bc49dd2f`,
pinned in `repo-pins.txt`.
It is **active in the running worldserver** following the explicitly authorized October 5
build/deployment. No configuration change or retroactive token conversion was required.
The deployment review records startup and subsequent live-play preservation exceptions.

`DirectTokenLootScript.cpp` now keeps a local rolled-class mask for one generated loot
list. Before each class roll, it excludes classes already selected in that list from the
token's own eligible PvE reward-class mask, then rolls uniformly among those remaining.
Different token IDs and equipment slots share that mask; unrelated token families retain
their own restrictions. Two priests or multiple specs do not grant a class a second roll.
Spec scoring, winner deduplication, PvP filtering and equipment-mode behavior are unchanged.

- If no unused eligible class remains, keep the original token without consuming RNG.
- An absent/unusable selected class still consumes its class roll, while retaining the
  original token. Do not reroll that failed selection now or on a later token in the batch.
- Class choices start fresh for the next generated loot list, even for the same looter
  or a reused `Loot` object. There is no persistent/global/player/lockout history.
- This applies to the existing creature and gameobject loot sources, including boss
  chests. Ordinary non-token gear does not consume a class roll and is not changed.
  Quest, stacked and already-looted token slots retain their existing skip behavior.
- This is one generated corpse/chest loot list, not bookkeeping across independent
  chests from a single encounter. It does not inspect existing ownership or guarantee
  every reward is an upgrade; there is no retroactive replacement of unwanted paladin gear.

The native `Loot::FillLoot` hook runs once after `tab->Process` has expanded reference
loot templates, before group access/distribution. The local mask therefore covers all
convertible slots in that generated list; opening its loot window is not a fresh class roll.
No hook/core change or new setting is needed. Debug logging identifies exhaustion separately
from an absent/unusable selected class.

Validation for this addition:

- **12 token-loot tests passed**, including the unchanged 135-case T4 scoring matrix.
  Another **23 tank-mode, pull/skull-focus and publication-workflow regressions passed**.
- New production-hook fixture exercises all six class orderings for a three-class token,
  shrinking RNG bounds, repeated paladin prevention, two priests/mixed specs, exhaustion,
  different token IDs/families, absent/unusable classes, skipped slots and fresh corpse/chest fills.
- All eight C++ scenarios passed ASan/UBSan. The full previous script from playerbots
  `e94b0f7df302f0001c6313768bfe3d7582d53a67` compiles against the same fixture and fails
  the new distinct-class assertion, reproducing the original problem.
- Production-header syntax checking of `DirectTokenLootScript.cpp` and the official C++
  style checker on the changed source/test files passed. Offline fixtures are not live
  boss-drop acceptance or a full server build.

Reproduce with the test command above; syntax-only checking needs only
`modules/mod-playerbots/src/Mgr/Item/DirectTokenLootScript.cpp` for this addition.
