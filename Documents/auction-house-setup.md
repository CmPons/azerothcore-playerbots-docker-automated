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

## Persistent settings

Both root `.env` and `azerothcore-wotlk/.env` contain:

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

## Validation

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

## Operations

GM commands: `.ahbot reload`, `.ahbot update`.
`reload` applies the AH configuration; `update` requests a cycle, subject to cycle timing.
Avoid `.ahbot empty` unless intentionally clearing the bot's auctions.

Future service interruptions still require fresh approval.
