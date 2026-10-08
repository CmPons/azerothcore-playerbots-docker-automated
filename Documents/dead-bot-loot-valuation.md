# Dead bots passing on equipment upgrades

## Status — October 8, 2026

**Built and deployed with explicit authorization on October 8.** Playerbots commit:
`b63dd8d9cb4f7493c8cd397412ed0f10e06abf2b`. See the
[deployment report](raid-fixes-deployment-20261008.md).
Only the worldserver was recreated; no bridge interruption, live configuration edit,
equipment swap, resurrection, forced save or gameplay SQL write was performed.

The user explicitly approved fixing the death-state defect found during
[Kaaren's glove investigation](kaarens-glove-roll-investigation-20261006.md), even
though they cannot remember whether Kaaren was dead at that particular roll.
**The historical reason for that Pass remains unconfirmed.** The defect itself is
reproduced through production methods in an offline test.

## Defect

`ItemUsageValue::QueryItemUsageForEquip` validates a temporary candidate through
`PlayerbotAI::CanEquipItem` before scoring it. That validator calls core
`Player::CanUseItem(Item*, not_loading=true)`, which rejects dead players. Thus a
useful upgrade can be classified as a non-upgrade merely because the bot cannot
put it on immediately. For ordinary class-usable tier armor, that can fall back to
KEEP, then Greed, then **Pass** with the configured Greed-off policy. A later `qi`
after resurrection can correctly report **Equip replace**.

## Narrow change

Three playerbots files change:

- `src/Bot/PlayerbotAI.h`: append `checkAlive=true` to the custom equip-validation API.
- `src/Bot/PlayerbotAI.cpp`: forward `not_loading && checkAlive` only to the core
  instance-item usability check.
- `src/Ai/Base/Value/ItemUsageValue.cpp`: opt out of the death check for valuation.

All existing callers keep the default death check. Core storage/equip handlers are
unchanged, so this does **not** let a corpse equip gear. The temporary candidate is
still removed from the update queue and deleted; the bot's life state and inventory
are never changed by valuation.

The current core instance-item `not_loading` flag gates only its death check. Tests
pin that assumption so a future expansion of its meaning is not silently inherited.
Crucially, the custom validator's original `not_loading` value is retained for its
script hook and offhand-swap checks. Simply changing the old valuation call to
`not_loading=false` would change those semantics, including rejecting an otherwise
valid two-handed replacement when an offhand is equipped; that shortcut is tested
and rejected.

Unchanged protections include class/race/level, proficiency and required skills/spells,
reputation, faction/holiday restrictions, ownership, script vetoes, uniqueness,
weapon-slot rules, offhand storage and disarm checks. This is specifically a **death
exception for valuation**, not a general bypass of temporary or permanent restrictions.
The scoring weights, set/socket handling, 10% threshold, token conversion, class
uniqueness and configured Need/Greed/Pass policy are unchanged. Non-upgrades remain
non-upgrades while dead. Generic item-usage consumers such as `qi` see the corrected
valuation as well; actual equip execution still validates separately.

## Validation

`python3 -m unittest scripts.tests.test_dead_bot_loot -v`: **five tests passed**.

The C++ fixture compiles these real source methods against offline state doubles:

- both core `Player::CanUseItem` overloads;
- playerbots' custom `CanEquipItem`;
- `ItemUsageValue::QueryItemUsageForEquip`;
- ordinary `LootRollAction::Execute`, `CalculateRollVote`, and `RollUniqueCheck`.

It checks alive/dead upgrade → Need, dead non-upgrade → Pass, unchanged Need-disabled,
Greed-only and master/free-for-all modes, normal dead-player equip refusal,
class/race/level/proficiency/skill/spell/reputation/holiday/script/unique/slot/disarm/
count restrictions, valid and blocked offhand swaps, resurrection and ownership.
Temporary-item queue-removal accounting and ASan/UBSan with leak checking pass.

The old playerbots source at `8275e8f8f9998136047ee034458bb1e2bc49dd2f` demonstrably
submits **Pass** for the dead-bot fixture before failing the desired-Need assertion.
The loading-flag shortcut also fails. Initial fixture compilation errors were missing
double declarations/fields and a misspelled error enum; they were corrected, not waived.

The score provider and outer non-upgrade KEEP fallback are doubles. This tests the
eligibility/decision path, not real stat-weight accuracy or a live server roll.
Kaaren's actual item comparison is recorded separately in the investigation.

Combined with direct-token and Leotheras reset regression suites: **23 tests passed**,
including the existing 135 installed T4 class/spec/slot cases. Production-header
syntax checks passed for `PlayerbotAI.cpp`, `ItemUsageValue.cpp`, and the unchanged
factory caller. Native style checks passed on the changed methods/declaration and
fixture; `git diff --check` passed.

Live acceptance is pending an ordinary loot event following deployment;
do not kill/resurrect bots or generate loot just to force this test.
