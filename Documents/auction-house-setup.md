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

## Active stock profile — September 21, 2026

Authorized by the user for deeper stock, ready-to-socket BC gems, and level-70 items.
Applied live through `ahbot reload` and one `ahbot update` console command, with no
worldserver/container restart, build, full setup run, auction clear, or operator SQL writes.

Both private env files now contain:

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

## Operations

GM commands: `.ahbot reload`, `.ahbot update`.
`reload` applies the AH configuration; `update` requests a cycle, subject to cycle timing.
Avoid `.ahbot empty` unless intentionally clearing the bot's auctions.

Future service interruptions still require fresh approval.
