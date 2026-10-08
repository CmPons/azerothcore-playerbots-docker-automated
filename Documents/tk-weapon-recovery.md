# TK weapon recovery must not monopolize bot actions

Status: **deployed October 8 at 21:18:39 CEST** after explicit build/deploy approval.
Playerbots source commit: `8d73b1a5721848071cd5e3048c7ad84a8f1c8194`.
See [deployment and qualified preservation review](tk-weapon-recovery-deployment-20261008.md).
Live Kael/command acceptance remains pending; deployment verification passed, but
startup exposed separate reset-metadata/profile/pet-autocast preservation issues.

## Observed failure

Near Kael, Feelesia repeatedly executed `kael'thas sunstrider reequip gear - OK`
at priority 101. Her queued advisor-timer action had priority 100; other bots
reported the shared advisor DPS multiplier blocking their attacks. The user also
reported that she stopped answering inventory and other commands.

The trigger treated any empty main-hand/off-hand/ranged slot with a legally
equippable inventory item as evidence of lost legendary weapons. It did not
establish actual legendary ownership or loss. Feelesia had Netherbane, a legitimate
empty offhand, and a skinning knife in her bags. Legal equippability and automatic
upgrade policy are different: miscellaneous weapons such as that tool are rejected
by equipment valuation. `EquipUpgradeAction::Execute` nevertheless returns true
when nothing was equipped, allowing this higher-priority action to consume ticks
indefinitely.

The user's two-handed-weapon workaround removed the loop and Sanguinar DPS worked.
This strongly supports tracker starvation, but the live shared timer value was not
captured. The exact inventory candidate considered by the live trigger was not
captured either. Later idle snapshots taken while Feelesia was dead showed other
failures, not this running recovery loop; these must not be conflated. The separate
report of healers apparently ignoring Redshift during weapons remains unresolved.

## Narrow repair

Only five files in `mod-playerbots/src/Ai/Raid/TK/` change:

- `Util/TKHelpers.cpp/.h`: an empty-slot candidate must pass the side-effect-free
  playerbot validator, resolve to the requested slot, and be accepted by the
  **owned-instance** `ItemUpgradeValue::CalculateForItem` policy. Its EQUIP,
  REPLACE and BAD_EQUIP categories match `EquipInventoryUpgrades`.
- `TKTriggers.cpp`: use that helper and reject dead bots; preserve map, boss
  proximity, spawn and slot checks.
- `TKActions.cpp/.h`: count success only when an eligible previously empty weapon
  slot actually becomes occupied. Do not trust the generic action's return value.
  After an attempt, a per-action five-second monotonic backoff leaves other actions
  eligible. Both usefulness and execution enforce it, including stale queued or
  directly invoked actions. A rejected swap may retry later; it cannot report
  repeated successful recovery while making no progress.

This retains ordinary main-hand/off-hand/ranged recovery, including recovery after
encounter weapons disappear. It does not invent legendary-loss history. A legitimate
empty slot with no automatic equip candidate no longer triggers recovery. The
backoff is still necessary because candidate valuation does not guarantee that an
actual equip handler will succeed.

No generic equipment policy, real equip/death restriction, command priority,
encounter assignment, DPS ordering, Capernian safety, tank aggro delay, legendary
loot/use behavior, whirlwind avoidance, configuration, SQL or item grant changes.
The shared advisor timer itself is unchanged: the repair removes this particular
source of starvation, not every conceivable reason a designated tracker could stall.

## Validation

```sh
PYTHONPATH=scripts/tests python3 -m unittest \
  scripts.tests.test_tk_weapon_recovery scripts.tests.test_dead_bot_loot \
  scripts.tests.test_direct_token_loot scripts.tests.test_trash_whirlwind -v
```

**30 tests passed**, including eight new TK tests. Three full modified translation
units passed native-header syntax checks; changed C++ regions passed the native
style checker and diffs passed whitespace checks.

The TK fixture injects production trigger/helper/action definitions, full core and
playerbot equipment validators, owned-instance upgrade classification, advisor timer
updater and DPS multiplier. It reuses the dead-bot equipment fixture. Item scores,
world objects, the actual equip handler and the small priority scheduler are doubles;
this is not a full AI engine or live client command acceptance test.

Coverage includes legal-but-unwanted knife rejection, genuine slot recovery,
backpack/bag scanning, ownership and instance-position validation, map/proximity,
death/class/level/unique/script restrictions, two-handed offhand rejection, false
success, failed-swap retry, per-bot backoff and millisecond wrap using the production
core time-difference helper. The core wrap helper is conservative by one millisecond.
A successful high-priority-action scheduler double reproduces starvation with the
old action; after repair the real timer updater can run, commands get a subsequent
tick after advisor damage, and the real DPS multiplier releases after its delay.
ASan/UBSan/leak checks pass. Always-success and no-backoff mutants fail their intended
assertions; both pre-fix October 8 and Monday equipment code reproduce the knife
false positive. TK code was identical at those two source revisions. This is an
offline reproduction of a pre-existing failure path, **not** proof of the historical
working pull's state or a controlled old/new binary comparison.

Initial fixture compilation needed missing ranged-slot/default callback definitions.
Review then replaced a simplified wrap calculation with the actual core helper and
re-ran the complete suite. These are test-fixture corrections, not waived failures.

Live acceptance remains pending: with the original legitimate one-hander/empty-OH
loadout, Feelesia should respond to commands without persistent recovery spam;
genuine temporary-weapon cleanup should still restore available ordinary weapons;
advisor DPS timing should progress normally. Do not force gear changes or another
pull merely for validation without the user's agreement.
