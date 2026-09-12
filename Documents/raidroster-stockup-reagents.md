# Stockup reagents through the player's nearby vendor

## Status

Prepared September 12, 2026; **not built or deployed**. No live stockup, inventory edits, bot login,
server interruption or configuration changes were performed. The previous durability deployment
permission was already consumed; a new build/deployment needs permission.

## Behavior

`.raidroster stockup` keeps its current group/raid scope (not just stored roster rows).

- If **you** can interact with any nearby normal vendor, online playerbots in your group can
  replenish their class/level reagents. They do not each need to stand beside that vendor; this
  reagent-only route also works for online group bots elsewhere/in another map.
- The merchant does **not** have to sell those particular reagents. This uses the command's existing
  free factory provisioning, not a new vendor purchase or money charge.
- At level 60, Ari's existing target is **100 Symbols of Kings** (21177); Meliah's is **40 Sacred
  Candles** (17029). No names are hardcoded: other group bots use their existing class/level targets.
- Only missing amounts are added. Existing surplus is retained, and repeated use does not grow a
  full stockpile. Native inventory-space constraints still apply; a failed storage attempt adds zero.
- Bots actually within interaction range of their own vendor still get the **old full stockup**:
  vendor-only junk sales, ammo, reagents, consumables and potions, in the same order.
- Your vendor alone does **not** authorize remote junk sales or remote ammo/potion/other-consumable
  provisioning. It authorizes reagents only.
- If neither you nor a bot can use a vendor, that bot is skipped. Offline/out-of-world bots, other
  human players (including self-bot-controlled humans) and the caller are not restocked.

No gear/spec regeneration, repair, instance/bind changes, movement or teleportation was added.
This calls the existing `PlayerbotFactory::InitReagents`; it does not replace its reagent catalogue
or other class-specific behavior with a new maintenance routine.

## Cause and reporting

The existing command already called `InitReagents`, but first required **each bot** to pass its own
vendor-interaction check. A nearby merchant on the player's side was insufficient.

The new player-side lookup does not depend on bot AI or selected target. It searches nearby loaded
grid creatures; native phase filtering and `GetNPCIfCanInteractWith(... UNIT_NPC_FLAG_VENDOR)`
enforce the vendor flag, interaction distance, life/flight state and friendliness. The broader grid
search radius is not an increased interaction distance.

The summary distinguishes reagent-only bots and retains skipped-vendor reporting. Its added-item
count is also corrected: the live item templates classify Symbols of Kings and candles as **Misc
(class 15)**, not Reagent (class 5), so the old class filter missed their refills. Counting carried
item units immediately before/after provisioning (after junk sales) captures those additions.

## Sources, tests and deployment

Canonical source: `modules/mod-raid-roster/src/RaidRosterCommand.cpp`, synchronized to
`azerothcore-wotlk/modules/mod-raid-roster/src/RaidRosterCommand.cpp`. This is a root-maintained
module, so no extra core/playerbots patch is required.

`scripts/tests/test_raid_stockup_reagents.py` and `cpp/RaidStockupReagentsTest.cpp` compile actual
command, lookup, counter, bag traversal and factory reagent methods against API doubles, together
with the real core creature-searcher phase logic. Tests cover:

- Caller-side vendor with remote Ari/Meliah, partial topups, bag-carried candles, surplus, repeated
  commands, full bags and the old Misc-item reporting failure.
- No vendor, non-vendor/hostile/dead/wrong-phase vendor, distance/flight gates and NPC reach.
- Existing bot-side full stockup/sale ordering when the caller is not near a vendor.
- Disabled/console/no-group handling, offline/real-player exclusions and level-specific targets.
- Both source copies matching, with no gear/repair/sync shortcuts in the handler.

Validation: **72 tests passed, one optional connection-local MySQL test skipped**, including five
new stockup tests and the existing durability, Twins/AQ40, pet, skull, chatter/bridge, scaling/reset
and statistics suites. Scoped core C++ style checks and 120-column checks on added lines passed.
These are offline sanitizer/API-double tests, **not** a complete server build or live command test.
Preparation backup: `backups/stockup-reagents-preparation-20260912-153047/`.

Until deployment, the existing workaround is to bring **both bots themselves within interaction
range of any usable vendor**, then run `.raidroster stockup`; the current build already includes
Kings/candle replenishment there (its old added-item counter may not report those Misc items).
