# Auction house setup

Activated 2026-09-06 using the already-built `mod-ah-bot-plus` in the worldserver image.
No new image was required. Only worldserver was stopped/started with user approval.

## Identity

- Seller name: **Marketkeeper**, character GUID **2502**.
- Dedicated account: **AHMARKET**, account ID **252**.
- Not a random-playerbot account and not a companion or playable character.
- This is an AH-only identity row. The module creates transient `Player` objects with
  `Player::Initialize(GUID)` rather than logging this character in or loading its inventory.
- Account uses a generated/discarded password and is IP-locked to loopback; no GM access.
- Created while worldserver was stopped to avoid colliding with its character GUID allocator.
- Do not log this identity in, recruit it as a playerbot, or delete it while auctions exist.

## Original September 6 settings

These were the initial settings in both env files, superseded by the September 21
profile below:

```ini
AHBOT_GUIDS=2502
AHBOT_MIN_ITEMS=5000
AHBOT_MAX_ITEMS=5000
AHBOT_ITEMS_PER_CYCLE=500
AHBOT_BUY_CANDIDATES=10
AHBOT_LEVEL_RESTRICT=true
AHBOT_MAX_REQUIRED_LEVEL=60
AHBOT_ITEM_LEVEL_RESTRICT=true
AHBOT_MAX_ITEM_LEVEL=92
AHPRICE_ENABLE=1
```

`setup.sh` maps these into `env/dist/etc/modules/mod_ahbot.conf` and
`mod_ahbot_price.conf`. Only the AH configuration block was applied during activation;
full setup was deliberately not run, to avoid changing unrelated runtime settings.

- Seller and buyer enabled; one-minute cycles.
- Target 5,000 listings **per house**, adding up to 500 per sell cycle.
- Independent Alliance/Horde/Neutral houses; cross-faction auction sharing is unchanged.
- Crafting/consumable-weighted random mix, including weapons and armor; no tailored loot.
- Buyer considers up to 10 player listings per house per cycle at the module's calculated
  prices, with acceptable-price modifier 1. It is not a guaranteed instant buyer.
- Module filters exclude soulbound and quest-bound items. Raid BoP gear is not sold.
- Required level <=60 and item level <=92; recipes use their crafted item's level where
  the module can resolve it. These are **level filters, not a strict Vanilla whitelist**.
- AHPrice enabled; `.ahprice` provides pricing lookup (client addon remains optional).

## Initial activation validation

- `bash -n setup.sh` passed; AH-only config application was idempotent.
- Three isolated config tests passed (`python3 -m unittest discover -s scripts/tests
  -p 'test_ah_setup.py' -v`): disabled, defaults, and level-60 profile.
- Worldserver ready on existing image
  `sha256:458d485446edc9190c3b3da7638fbeb0d5ee5d762447ac986903f36adfb54a00`.
- First cycle created 500 auctions in each house (IDs 2, 6, 7), all owned by 2502.
- First-batch SQL audit: maximum required level 60; no BoP/quest-bound listings.
- Actual player-auction purchases have not yet been tested.
- Startup logged the process-priority permission warning; world initialization and AH
  listing generation nevertheless succeeded.

## Baseline stock profile — September 21, 2026

Authorized by the user for deeper stock, ready-to-socket BC gems, and level-70 items.
Applied live through `ahbot reload` and one `ahbot update` console command, with no
worldserver/container restart, build, full setup run, auction clear, or operator SQL writes.

The September 21 baseline values were:

```ini
AHBOT_GUIDS=2502
AHBOT_MIN_ITEMS=10000
AHBOT_MAX_ITEMS=10000
AHBOT_ITEMS_PER_CYCLE=500
AHBOT_BUY_CANDIDATES=10
AHBOT_TBC_CUT_GEM_MULTIPLIER=5
AHBOT_LEVEL_RESTRICT=true
AHBOT_MAX_REQUIRED_LEVEL=70
AHBOT_ITEM_LEVEL_RESTRICT=true
AHBOT_MAX_ITEM_LEVEL=164
AHPRICE_ENABLE=1
```

- Exactly nine runtime AH keys changed: six house min/max targets, two upper level
  limits, and the item listing-multiplier list. Other runtime config files were unchanged.
- Prices, buyer policy, category/quality weights, listing lifetimes, seller identity,
  raw-material/potion multipliers, and independent faction houses are unchanged.
- Stock rises naturally at up to 500 listings per house per one-minute sell cycle;
  expirations/purchases continue normally. No existing auctions were explicitly removed.
- `config/ahbot/tbc-cut-gems.tsv` is an explicit catalog of 127 tradable BC socket gems:
  six normal colors, meta and prismatic, uncommon through epic. Audited against the
  installed item templates: positive `GemProperties`, no bonding or duration, and a
  buy/sell value. Unused Infinite/Chromatic Spheres and Heavy Tonk Armor are excluded.
- `scripts/ahbot_stock.py` merges this catalog into the existing multiplier list without
  replacing unrelated entries. It only prints the new value; it does not edit config,
  contact the server, or modify SQL. Setup calls it only when the env knob is nonblank.
  Reapplication is idempotent. Blank preserves the existing list; explicit `0` removes
  the catalog's entries (does not reconstruct any earlier custom overrides of those IDs).
- The multiplier makes five listings **after a gem is randomly selected**, not five
  guaranteed copies of every gem and not a fivefold increase in its initial selection
  probability. Normal eligibility filters still apply; individual cuts can be absent.
- These remain **level filters, not a strict expansion whitelist**. Some Wrath gems
  already passed the old limits because their required level is zero and item level
  is 70/80. The new limits can also allow some Wrath equipment usable at/below 70.
  Only the added gem boost is specifically BC. Soulbound/quest-bound drops remain excluded.

### Live acceptance

Before reload: 4,991 Alliance bot listings, 87 BC socket-gem listings and no level-70
weapons/armor. After the first cycle: 5,491 listings, 112 BC socket-gem listings and
2 level-70 gear listings. The first observed examples were The Night Blade and
Fel Orc Brute Sword, plus five Bold Crimson Spinels. These are point-in-time stock
observations, not promises of continuous availability or a completed 10,000-item refill.

Twelve isolated setup/helper tests passed, including default compatibility, the 70/164
profile, idempotence, raw-material preservation, zero/removal behavior, malformed input,
and the read-only CLI. `bash -n setup.sh` and `git diff --check` passed. Worldserver,
authserver, and database container IDs/start times/restart counts remained unchanged;
Pi bridge PID/invocation unchanged. Console reload/update acknowledgements were captured.

Private evidence/config backups: `backups/ah-stock-20260921-195804/`. Configuration rollback
means reviewing/restoring only the changed AH/env settings and reloading the AH module;
newly listed auctions expire normally. Do not restore an old database or clear auctions.

## Temporary plate/shield gearing profile — September 24, 2026

User-requested boost while gearing Arinerica for level-70 raids. This is **manual
opt-in until gearing is finished**, not a timed expiry or a new default. It preserves
all existing auctions, prices, buyer policy, seller identity, level/ilvl filters,
listing lifetimes, cut-gem multipliers and other category weights.

Both private env files use this temporary override of the September 21 baseline:

```ini
AHBOT_MIN_ITEMS=12000
AHBOT_MAX_ITEMS=12000
AHBOT_LEVEL70_ARMOR_MULTIPLIER=10
AHBOT_ARMOR_UNCOMMON_WEIGHT=40
AHBOT_ARMOR_RARE_WEIGHT=40
AHBOT_ARMOR_EPIC_WEIGHT=12
```

- `config/ahbot/level70-plate-shields.tsv` contains **170** audited tradable plate/shield
  templates, including **15 shields**. Required level 65–70, item level <=164,
  uncommon/rare/epic, usable by paladins with no profession/reputation requirement.
  Includes some Wrath items (notably Cobalt), consistent with the existing level filters.
  BoP/quest gear, disabled/test items and items with duration/no vendor value are excluded.
- Regular catalog items get batches of **10**, and **24** items with native defense,
  dodge, parry, block rating/value stats get batches of **20**. This is a simple stat
  tag, not a BiS ranking: resistance gear can qualify; random suffixes are not guaranteed
  to be tank-oriented. Armor color or item level alone does not make an upgrade.
- Armor quality selection weights change from **20/10/3 to 40/40/12**. These weights
  affect **all armor** of those qualities, not only plate/shields. Other armor remains
  eligible; no existing stock is cleared. Unchanged category weights still have a
  smaller relative share when the armor weights rise.
- Target depth rises from 10,000 to 12,000 per house to permit a refill without deleting
  auctions. Work remains capped at 500 new listings per house per one-minute cycle.
- Item multipliers only apply **after random selection**. They do not guarantee any
  particular piece will appear, or that each house gets the same selection.
- The read-only helper supports either or both profiles in one invocation; applying
  one preserves the other. Setup defaults retain the old weights and no armor boost.

Preview the new item-multiplier value without editing/reloading anything:

```sh
python3 scripts/ahbot_stock.py \
  --config azerothcore-wotlk/env/dist/etc/modules/mod_ahbot.conf \
  --level70-armor-multiplier 10
```

### Ending the temporary boost

When the user is finished gearing, set both private env files to:

```ini
AHBOT_MIN_ITEMS=10000
AHBOT_MAX_ITEMS=10000
AHBOT_LEVEL70_ARMOR_MULTIPLIER=0
AHBOT_ARMOR_UNCOMMON_WEIGHT=20
AHBOT_ARMOR_RARE_WEIGHT=10
AHBOT_ARMOR_EPIC_WEIGHT=3
```

Apply **only** the corresponding six runtime house targets, three armor quality
weights and the multiplier value printed by the helper with
`--level70-armor-multiplier 0`; then issue `ahbot reload`. Do not run full setup.
Keep `AHBOT_TBC_CUT_GEM_MULTIPLIER=5`. Zero removes the catalog entries rather than
recovering earlier per-item custom overrides; none of these 170 IDs had an override
before this deployment. If later edits introduce custom values, review before removal.

Do **not** restore an old full config/database, cancel auctions, or run `ahbot empty`.
Stock above the restored target expires/sells naturally, and replenishment resumes
below the target. Purchased items and player auctions are left alone.

### Validation

Seventeen offline helper/setup tests pass, including combined profiles, idempotence,
removal preserving gem/material multipliers, defensive priority, invalid inputs,
read-only CLI, and legacy/default setup behavior. No module/core source changes or build.
Activated with one bounded `ahbot reload` and one `ahbot update`, both acknowledged.
Exactly ten runtime AH keys and six keys in each private env changed. All original
303 item multipliers were retained and 170 added; removing the armor profile in an
in-memory check recovered the original list exactly. Other runtime configs were
hash-verified unchanged. No operator SQL writes, auction clear, full setup or restart.

Observed bot stock during the refill (point-in-time, not guaranteed availability):

| House | Plate/shield catalog listings before | After three observed batches | Shields after |
| --- | ---: | ---: | ---: |
| Alliance | 14 | 84 | 24 |
| Horde | 12 | 85 | 1 |
| Neutral | 19 | 175 | 3 |

At that snapshot each house had about 11,460 bot listings, still refilling toward
12,000. Alliance retained Shield of the Wayward Footman. Neutral had batches of
Felsteel Gloves, Gauntlets of the Iron Tower and Cobalt Chestpiece; Horde had
Topaz-Studded Battlegrips. Most newly listed Alliance shields were caster-oriented
Draenei Honor Guard Shields: increased stock alone does not guarantee tank upgrades.

Worldserver, authserver and database container IDs, start times and restart counts
were unchanged; all were running. Pi bridge PID/invocation unchanged. Private
config/audit/console/stock evidence: `backups/ah-plate-stock-20260924-125746/`.

## Temporary boost ended — September 24, 2026

At the user's request, restored the September 21 stock profile using only the six
AH keys in each private env file and ten corresponding runtime AH keys. Targets
are again **10,000 per house**, armor weights **20/10/3**, and the 170 temporary
plate/shield item multipliers were removed. The **BC cut-gem multiplier remains 5**;
all 303 pre-boost gem/material/other item multipliers were preserved.

One bounded `ahbot reload` was acknowledged. No auction clear, forced refill,
operator gameplay SQL or full setup was run. Existing stock expires/sells naturally;
purchased equipment and player auctions are unchanged by this rollback. This AH
reload itself did not restart a service. The separately authorized enemy support
scaling deployment is tracked in `raid-healing-shield-scaling.md`.

Private evidence: `backups/raid-support-deploy-20260924-222628/ah-rollback.json`,
config copies/hashes and `ah-reload.log`.

## Healing-potion stock boost — October 3, 2026

At the user's request, increased the listing batches for **Major Healing Potion
(13446)** and **Super Healing Potion (22829)** from **5 to 20** in each house's
shared item-multiplier policy. Other healing potions, all mana potions, prices,
stack-size policy, buyer settings, category weights, level filters and the
10,000-listing targets remain unchanged. All 301 unrelated multipliers were
preserved, including the BC cut-gem boost.

Both private env files now contain:

```ini
AHBOT_HEALING_POTION_MULTIPLIER=20
```

The opt-in setup knob uses `scripts/ahbot_stock.py` and can be combined with the
existing gem/armor profiles. Blank preserves existing potion values; `0` removes
these two overrides; **`5` restores the pre-boost policy**. Zero is not a rollback
to the previous five-listing batches. Fresh setups remain unchanged unless the
knob is explicitly set.

Read-only preview:

```sh
python3 scripts/ahbot_stock.py \
  --config azerothcore-wotlk/env/dist/etc/modules/mod_ahbot.conf \
  --healing-potion-multiplier 20
```

Only the multiplier-list runtime key and one addition to each private env were
applied. One bounded `ahbot reload` and one `ahbot update` were acknowledged.
**No restart, build, full setup, auction deletion, direct item grant or operator
SQL write** was used. Container identities/start times/restart counts and Pi
bridge identity were unchanged; all other runtime `.conf` files were hash-checked.

This is **four times the batch size after random selection**, not four times the
selection probability or guaranteed immediate inventory. Houses were near full
before reload and reached 10,000 listings in the first follow-up. That snapshot
still had 49 Major potions and no Super potions in Alliance; Horde had 42 Major
and 2 Super, Neutral 18 Super. The policy is active, but **additional potion stock
was not yet observed**. Replenishment depends on turnover and random item selection.
Do not clear existing auctions just to force this profile to show results.

Twenty offline helper/setup tests passed, including combined profiles, exact
item scope, mana/material preservation, idempotence, restoring five, removal,
invalid values and read-only CLI behavior. `bash -n setup.sh` and whitespace
checks passed. Private config/stock/console evidence:
`backups/ah-healing-potions-20261003-200951/`.

Rollback is to set the env knob to `5`, merge those two runtime entries back to
`5`, and issue `ahbot reload`; existing auctions continue to sell/expire normally.
Do not restore a database or a whole old config to reverse this setting.

## One-time shield restock — October 10, 2026

At the user's request, added **40 shields per house (120 total)** through the
normal AH seller. All seven selected templates appeared in each house:

| Shield | Entry | Quality | Required level | Alliance | Horde | Neutral |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| Shield of the Wayward Footman | 31200 | Rare | 65 | 2 | 7 | 3 |
| Draenei Honor Guard Shield | 31287 | Rare | 68 | 3 | 2 | 4 |
| Crystal Pulse Shield | 31292 | Rare | 69 | 7 | 5 | 3 |
| Kaskala Buckler | 36447 | Uncommon | 69 | 8 | 9 | 9 |
| Spiked Targe | 36448 | Uncommon | 70 | 12 | 5 | 9 |
| Beneficent Bulwark | 37747 | Rare | 69 | 4 | 3 | 3 |
| Cobalt Triangle Shield | 40668 | Uncommon | 70 | 4 | 9 | 9 |

These are verified BoE shields, required level65–70, ilvl<=164, with no
profession/reputation requirement. Existing item stats, random-property
selection, pricing and expiration policy were unchanged. **This is additional
variety, not a promise of an upgrade over Cobalt:** the rare options are mostly
caster shields; Wayward Footman is an older, lower-level tank option. No raid,
reputation or other BoP reward was made tradeable or listed.

For this bounded operation only, the existing item-ID filter was restricted to
these seven IDs using min/max0 and explicit exceptions. Per-cycle work was capped
at40, house targets temporarily allowed10040 listings, and AH-bot buying was
paused during the restock. One acknowledged `ahbot reload`, one `ahbot update`,
and a final restoring `ahbot reload` completed successfully. The exact original
AH config was restored in a `finally` block; both env files and every runtime
`.conf` hash match the originals. No permanent weights, multipliers, caps,
buyer settings or stock-policy change remains.

The source audit identified that an already pending multiplied-item batch can
survive reload; the before/after inventory check therefore inspected every new
listing rather than assuming a restricted pool guaranteed exclusivity. In this
run **all120 new listings were selected shields**, with zero other additions,
zero removed existing auctions and zero changes to existing auction rows at the
verification snapshot. Stock counts are point-in-time, not future guarantees.

World/auth/database/helper container identities, images, start times, statuses
and restart counts, plus Pi bridge identity/state, were unchanged. No build,
restart, auction clearing, direct player-item grant, operator SQL write or
full setup occurred. Native AH code performed its ordinary auction transactions.
The stock sells/expires normally (existing15-minute to24-hour lifetime range);
this was a one-time refill, not scheduled guaranteed shield replenishment.

Private config backups, candidate audit, auction snapshots, console acknowledgments,
restoration assertions and service checks:
`backups/ah-shield-stock-20261010-163800/`.

## Operations

GM commands: `.ahbot reload`, `.ahbot update`.
`reload` applies the AH configuration; `update` requests a cycle, subject to cycle timing.
Avoid `.ahbot empty` unless intentionally clearing the bot's auctions.

Future service interruptions still require fresh approval.
