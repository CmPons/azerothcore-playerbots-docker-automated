# Kaaren's passed T5 gloves — October 6, 2026

## Result

**The old gloves are preserved and the T5 gloves are a clear scored upgrade.**
The reported Pass is not explained by the normal 10% upgrade threshold. A concrete
existing death-state rejection can produce Pass followed by `qi` → `Equip replace`
after resurrection, but Kaaren's state at the actual roll is not recorded. That
historical cause remains **unconfirmed**, pending the user's recollection or a
future decision trace. No production fix, live item manipulation, config change,
forced save, build or restart was performed in this investigation.

## Read-only evidence

Snapshot: **2026-10-06**, private evidence under
`backups/kaarens-glove-investigation-20261006-212111/` (the private pointer
`/tmp/kaarens-glove-investigation` identifies the actual capture directory).

- Equipped: **Cataclysm Gauntlets (30189)**, Enhancement T5, item level 133.
- Bag: **Gauntlets of the Dragonslayer (28827)**, item level 125, bag item 7915982,
  slot 5. Both sockets empty; no permanent enchant. Original item remains intact.
- Another bag pair: Fel Leather Gloves (25685); not the pair worn in the October 5 audit.
- Existing **Cataclysm Shoulderplates (30194)** share set 636 with the new gloves,
  yielding a two-piece set. Old gloves belong to no set.
- Need setting is 2 (Need enabled); Greed setting is 0 (otherwise Greed becomes Pass).
- Group 33 uses Group Loot. The new gloves are not unique-equipped; their template
  flag 4096 is not the unique-equipped flag.

Compared with Dragonslayer, the new gloves retain 24 agility, add 22 attack power,
24 hit and seven stamina, lose four base intellect and two sockets, and complete
the existing T5 pair. This is not simply an item-level recommendation.

## Independent score reconstruction

Using installed item/spell/enchantment data and the current Enhancement weight,
socket, set and integer quality-blend formulae, the new gloves exceed the 1.1
threshold in all four checks:

| Mental Dexterity weight | New two-piece premium | New / old score |
| --- | --- | ---: |
| absent | omitted | 1.757 |
| present | omitted | 1.499 |
| absent | included | 2.583 |
| present | included | 2.204 |

These are an **independent arithmetic reconstruction**, not captured live AI scores
or an end-to-end native executable replay. The captured talents include Mental
Dexterity, but aura state at the historical roll was not captured. Armor, the old
48-AP equip spell, its three-intellect socket bonus, socket-count multiplier and
set-completion premium are included. Ordinary equipped gems/enchants are not part
of this template scorer; both relevant pairs are unenchanted and unsocketed here.
The matching reconstruction with the recorded two-piece setup is about 2.20×,
not evidence of 2.20× actual character damage.

## Decision path and discovered defect

Both ordinary rolling and `qi` read the same `item usage` value. Its default
interval is 1, meaning `CalculatedValue::Get()` recalculates every read: there is
no long-lived cached item-usage decision to blame by default.

However, `ItemUsageValue::QueryItemUsageForEquip` creates a temporary item and calls
`PlayerbotAI::CanEquipItem(..., true, true)` **before scoring**. The last true
means `not_loading`; this calls core `Player::CanUseItem(Item*, true)`, which
returns `EQUIP_ERR_YOU_ARE_DEAD` for a dead bot. The evaluator then returns
`ITEM_USAGE_NONE` for equipping. A class-usable tier item can subsequently become
KEEP (or another non-upgrade usage). Ordinary armor rolls translate that to Greed,
and the configured Greed-off policy translates it to **Pass**. After resurrection,
the same evaluation reaches scoring and can return **Equip replace**.

This is a longstanding source path, not introduced by October's token changes.
It wrongly mixes **can equip immediately** with **worth acquiring for later**.
Any eventual fix must preserve class/level/proficiency/unique ownership checks and
actual equip-time restrictions, while removing inappropriate transient restrictions
from loot valuation. Do not simply disable all equip validation.

Other reviewed paths:

- Ordinary `ITEM_USAGE_REPLACE` maps directly to Need before independent vetoes.
- Need is not disabled, and these gloves are not unique-equipped.
- Core can auto-pass through eligibility/opt-out checks; historical runtime opt-out,
  conditions and hooks are not recorded by the saved inventory.
- The packet-driven fallback and scheduled rolling paths are separate; no captured
  trace identifies which submitted this particular vote.
- The user's explicit Pass report is retained; this is not being reclassified as
  a timeout. A raid-strategy scheduling hypothesis does not establish a cause.
- No per-item vote/reason log was found in the checked Server/Playerbots logs.

## Changes/history

October 1 made direct-token reward generation spec-aware and isolated its scorer
mode; ordinary equipment valuation retains the original non-loot mode. October 5
added per-generated-loot-list class uniqueness. Neither changed this ordinary
armor Need/Pass mapping. The trinket-proc bug and SSC pre-pull discussion have not
been deployed as fixes.

## Source references

Playerbots revision `8275e8f8f9998136047ee034458bb1e2bc49dd2f`:

- `src/Ai/Base/Actions/LootRollAction.cpp`: normal vote selection and vetoes.
- `src/Ai/Base/Actions/QueryItemUsageAction.cpp`: `qi` uses the same item-usage value.
- `src/Ai/Base/Value/ItemUsageValue.cpp`: early equip validation and later scoring.
- `src/Bot/PlayerbotAI.cpp`: custom equip validation delegates item usability to core.
- `src/Bot/Engine/Value/Value.h`: default interval recalculates each read.
- `src/Mgr/Item/StatsWeightCalculator.cpp`: weights, set replacement and socket premiums.

Core revision `8b8b7bcf8615c62b4aedaa00c33713a0c1f6df0e`:

- `src/server/game/Entities/Player/PlayerStorage.cpp`: instance-item usability rejects death.
- `src/server/game/Groups/Group.cpp`: group eligibility/auto-pass and vote processing.
