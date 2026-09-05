# Generic Direct Token Loot Plan

## Goal

Make token-based raid loot feel like direct boss loot without smart gearing assistance.

When a boss would normally drop a gear token, replace that token in the loot window with **one random real reward item** from that token's valid reward pool.

Example:

```text
Boss would drop: Qiraji Martial Drape
Instead drops: one random AQ20 cloak reward, e.g. Shroud of Infinite Wisdom
```

This preserves MC-style RNG:

- no raid composition filtering
- no class-smart targeting
- no guaranteed upgrades
- if nobody needs the item, that is just bad luck
- same original token slot count, but the visible loot is actual gear

## Desired Behavior

Original loot structure:

```text
- token slot: Qiraji Martial Drape
- normal epic slot A
- normal epic slot B
```

New loot structure:

```text
- token slot becomes: one random real reward from the Qiraji Martial Drape reward pool
- normal epic slot A
- normal epic slot B
```

Important: this is **not** flattening into independent extra drops. The token slot still produces one item. The only change is that the token is resolved before players see/roll on loot.

## Scope

Implement this generically so it can affect future token mechanics, not just AQ20.

Should cover, where resolvable:

- AQ20 Qiraji ring/cloak/weapon tokens
- AQ40 tier tokens
- ZG-style gear turn-ins if they resolve cleanly
- TBC/WotLK vendor or extended-cost tier tokens
- future similar custom token systems

## Existing Foundation

`mod-playerbots` already has generic token resolver logic:

```cpp
TokenItemResolver::FindTokenRewards(uint32 tokenItemId)
```

It can discover reward candidates through:

- quest turn-ins
- vendor rewards
- extended-cost vendor token systems

This should be reused rather than hardcoding AQ20.

## Implementation Approach

### 1. Add a loot post-processing module hook

Use AzerothCore's existing loot hook:

```cpp
MiscScript::OnAfterLootTemplateProcess(
    Loot* loot,
    LootTemplate const* tab,
    LootStore const& store,
    Player* lootOwner,
    bool personal,
    bool noEmptyError,
    uint16 lootMode)
```

This runs after normal loot generation.

The script scans:

```cpp
loot->items
```

For each generated loot item:

1. Treat its `itemid` as a potential token.
2. Ask the token resolver for valid gear reward candidates.
3. If rewards exist, randomly choose one.
4. Replace the loot item's `itemid` with the chosen reward item id.
5. Preserve item count, loot slot, group rules, and other loot metadata as much as possible.

### 2. Token resolution rules

A dropped item qualifies for replacement only if:

- it resolves to at least one gear reward
- reward item exists in `item_template`
- reward is equippable gear or otherwise explicitly allowed
- reward is not another unresolved token
- original loot item count is sane, probably `count == 1`

Do not require:

- player class match
- raid composition match
- reputation
- scarabs/idols/materials
- current quest availability

The loot system is replacing the token with the final reward, not performing the original turn-in.

### 3. Config

Add config options, likely in `playerbots.conf.dist` or a small dedicated module config:

```ini
Playerbot.DirectTokenLoot.Enable = 0
Playerbot.DirectTokenLoot.Mode = 1
Playerbot.DirectTokenLoot.MinRewardQuality = 3
Playerbot.DirectTokenLoot.Debug = 0
```

Mode meanings:

```text
0 = normal token loot, no replacement
1 = random direct reward from token reward pool
2 = future optional class-usable reward mode
```

For this server's preferred old-school progression feel, use mode `1`.

### 4. Logging

When debug is enabled, log replacements:

```text
DirectTokenLoot: replaced Qiraji Martial Drape (20885) with Shroud of Infinite Wisdom (21412)
```

Also log skipped tokens when helpful:

```text
DirectTokenLoot: skipped token 12345, no gear rewards found
```

### 5. Edge cases

Handle carefully:

- duplicate rewards in a single corpse
- unique-equipped items
- quest-only items
- class-specific rewards appearing for wrong classes — allowed in mode 1, because this is MC-style RNG
- reference loot tables
- group loot rolls after item replacement
- item display/cache correctness in the loot window
- condition-bound loot visibility

### 6. Testing plan

Read-only DB verification:

- list AQ20 token items and resolved reward pools
- list AQ40 token items and resolved reward pools
- list any TBC/WotLK vendor token candidates

Runtime testing on a private/fresh instance:

1. Enable config.
2. Kill or force-kill a boss that normally drops a token.
3. Confirm corpse shows real reward item, not token.
4. Confirm group loot/need-greed still works.
5. Confirm bots can roll/evaluate/equip/keep the direct reward.
6. Confirm no extra loot count inflation.

### 7. Preferred final behavior

AQ20 should feel like:

```text
Boss dies.
Real epic appears.
Maybe nobody needs it.
That's fine — classic RNG.
```

Not:

```text
Boss dies.
Blue administrative token appears.
Need Honored.
Need scarabs.
Need idols.
Fly to Cenarion Hold.
Maybe later this becomes loot.
```

## Open questions

- Should direct token replacement apply only to creature/boss loot, or also containers/gameobjects?
- Should quest-starting heads/items like Head of Nefarian be excluded even if resolver finds rewards?
- Should there be a per-map allowlist/denylist for safety?
- Should replacement happen before or after conditions/loot visibility checks?
- Should mode 1 preserve the token's original quality threshold behavior, or should the final reward's quality control loot rolling?
