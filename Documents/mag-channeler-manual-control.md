# Manual Magtheridon channeler control

## Status

**Deployed September 25, 2026**, after explicit build/deployment authorization.
See [deployment and preservation results](mag-channeler-deployment-20260925.md).
Only worldserver was recreated. No configuration or direct gameplay-data changes
were made; the deployment record discloses startup group, instance and mail cleanup.

## Why

The user reports Arinerica going to the western channeler and tanking only that
one while Redshift handles the remaining pack and infernals. The installed source
contains matching behavior, although this is not a captured live decision trace:

- `MagTriggers.cpp` assigns the first assist tank to the northwest channeler.
- `MagActions.cpp` forces that tank's RTI to diamond, attacks that channeler and
  moves toward a fixed northwest tank position once it has aggro. The second
  assist tank gets the northeast channeler; the main tank gets the first three.
- `MagtheridonControlTankActionsMultiplier` blocks normal tank-assist and combat
  formation actions whenever a tank has a victim and Magtheridon is found, even
  before the boss activates. Main tanks also lose AoE avoidance; after the first
  three channelers die, selected taunt/reach actions are suppressed while waiting.

This is a scripted three-tank split, not normal add gathering. Removing it does
not establish that every difficulty in the fight was caused by the strategy.

## Change

Native fork: `azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Mag/`.

`MagStrategy.cpp` no longer registers six automatic trigger/action pairs:

1. Main tank's first-three-channeler assignment and subsequent waiting position.
2. First assist tank's northwest assignment/position/diamond RTI.
3. Second assist tank's northeast assignment/position/triangle RTI.
4. Hunter west/east pulls using Misdirection and Steady Shot.
5. Forced channeler kill order and RTIs (including that action's final boss fallback).
6. Warlock-specific Burning Abyssal banish/fear assignments.

As with the Maulgar removal, the old action/trigger implementations remain in the
fork but are no longer scheduled by the encounter strategy. This applies at all
raid sizes; no new attendance or roster heuristic is introduced. Normal class,
group and manual behavior remains available, including ordinary interrupts/CC.

`MagMultipliers.cpp` makes the tank-control multiplier return neutral before
Magtheridon activates, and removes its old channeler-dependent taunt/reach gate.
Its behavior after Magtheridon activates is unchanged.

Preserved: boss tank positioning, ranged spreading, Manticron cube handling,
Blast Nova timers, debris avoidance and spell tracking, timer/assignment cleanup,
and the boss-release six-second DPS wait. Boss-phase controls still apply after
release, including if surviving channelers remain. HP/damage/support scaling,
encounter NPC mechanics, cooperative tank modes, loot/trading and AH/gems are
untouched.

The ordinary MT/OT ownership rule still prevents Ari from automatically stealing
adds already attacking Redshift. Removing encounter assignments is not an order
to transfer all current aggro. Existing manual/saved RTIs are not cleared by this
change, and no forced raid encounter was used for validation.

## Validation

- `python -m unittest scripts.tests.test_mag_channeler_manual_control scripts.tests.test_maulgar_manual_control scripts.tests.test_tank_modes scripts.tests.test_skull_combat_only -v`: **13 tests passed**.
- New tests assert the exact six retained boss trigger/action pairs and four
  multipliers. The extracted production tank multiplier runs **1,728** checks
  with explicit game-state/action doubles, covering main/off/non-tanks, current
  victims, boss presence/activation/ownership and channeler presence.
- The same harness compiles the prior multiplier from PB commit
  `292669a0b536c3e988a31ec874996d85ee5dc2e2` and rejects its channeler-phase blocker.
- Both changed production `.cpp` files pass C++20 syntax checks against native
  headers (`scripts/tests/raid_combat_syntax.py`). The subsequently authorized
  native image build and compiled registration/disassembly checks also passed.
- Official core C++ style checker passes on both changed production files and
  the new C++ fixture; `git diff --check` passes.
