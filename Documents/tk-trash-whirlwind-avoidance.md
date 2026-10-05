# TK trash whirlwind avoidance

Status: **built and deployed October 5 with explicit authorization**; live TK trash
acceptance remains pending. See the [deployment and qualified preservation review](raid-qol-deployment-20261005.md).
Original playerbots commit: `e94b0f7df302f0001c6313768bfe3d7582d53a67`, included in
the deployed `8275e8f8f9998136047ee034458bb1e2bc49dd2f` alongside token class uniqueness.
The respawn-scaling fix was deployed in the same image.

## Report and confirmed detection gap

While progressing through The Eye, the user reported bots remaining beside
spinning trash enemies and taking repeated damage. Read-only installed SmartAI
and spell-data checks identified:

| Creature | Aura | Triggered hit | Radius | Aura duration / tick |
| --- | --- | --- | --- | --- |
| Bloodwarder Legionnaire (20031) | 33500 | 15578 | 8 yards | 2 seconds / 2 seconds |
| Bloodwarder Marshal (20035) | 36132 | 15589 | 8 yards | 6 seconds / 1 second |

Both triggered hits use `SPELL_EFFECT_WEAPON_DAMAGE`. These are ordinary
selectable enemies carrying periodic-trigger auras, not dynamic ground objects
or unselectable trigger creatures.

The old generic `AvoidAoeAction::isUseful` only considered an area debuff, damage
traps and possible trigger creatures. Its `AvoidUnitWithDamageAura` branch
requires `UNIT_FLAG_NOT_SELECTABLE` and only recognizes `SPELL_EFFECT_SCHOOL_DAMAGE`.
Consequently these trash whirlwinds were outside that detection path. This is a
source/data-supported gap, **not an event-level trace of every reported death**.

## Narrow correction

New playerbot helper: `src/Ai/Base/Util/TrashWhirlwind.{h,cpp}`.

- Covers only the two creature/aura pairs above on map 550. It does not classify
  every periodic aura as dangerous, change other bosses' whirlwinds, or alter
  spell damage, duration, spawn data or tank assignments.
- Uses current nearby `possible targets no los` GUIDs, resolves each at use time,
  and builds local position/radius snapshots. No retained creature pointers.
- Requires a living, in-world, hostile, in-combat creature in the bot's exact map;
  player-controlled creatures are excluded. Missing, expired and removed auras
  do not qualify. Invalid/stale candidates do not suppress later valid ones.
- Reads radius from the triggered damage spell and respects the existing maximum
  avoidance radius and ignore-list entries for either the aura or hit spell.
  Adds the bot's combat reach plus one yard of reaction margin to the danger zone.
- Runs through the existing `avoid aoe` strategy, independently of `tempestkeep`.
  `AutoAvoidAoe` normally installs that strategy; disabling it disables this
  addition. Manual `stay` is respected.
- **The tank currently holding a spinning creature is exempt for that creature.**
  The fix does not automatically kite the tank and drag the enemy through the
  raid. Other tanks and non-tanks can avoid it. A DPS who has aggro is not exempt.
- Uses existing melee/ranged flee-candidate selection and normal ground
  pathfinding. Emergency movement can preempt ordinary approach timing, but not
  an existing Lua movement claim. Casting is interrupted only after a successful
  escape move request. The retry timer updates independently of chat logging.
- Scheduled `MoveTo`, combat approach, follow, chase and gap-closer usefulness
  checks reject movement into/across the active danger sphere; movement out is
  permitted. This prevents ordinary melee approach from immediately undoing the
  escape. Safe ranged/healing approach distances remain allowed.
- Explicit unscheduled movement retains authority. No generic healing, attack,
  taunt, spellcast or target-selection suppression is introduced. Existing RTIs
  and co-tank ownership remain unchanged. When the aura ends, the movement gate
  disappears on the next observation; there is no sticky encounter flag.

Existing dynamic-object, trap and unselectable-trigger avoidance methods are
unchanged. No pet-avoidance behavior is added.

## Validation and limits

Five new tests in `scripts/tests/test_trash_whirlwind.py`:

- Compile the complete production helper and extracted avoidance action methods
  with explicit world/AI/movement doubles, normally and under ASan/UBSan.
- Exercise both enemy/aura pairs; selectable ordinary enemies; stale candidates;
  expiry/removal; invalid radius or spell; wrong map/entry/aura; friendly,
  controlled, dead, unloaded and out-of-combat exclusions; ignore lists; tank
  ownership; manual stay/commands; movement claims; failed movement; successful
  escape throttling; and unchanged access to legacy hazard branches.
- Check melee/gap-closer re-entry rejection, safe ranged approach, movement away,
  different elevations and 360 angular escape/entry cases.
- Compile the exact previous detector/dispatch from playerbots
  `cef0162a7202f7a10688e989c0dba5d76828dbb1`; it fails the selectable-spin assertion.
- Verify wiring at the production movement admission sites and byte-for-byte
  preservation of the existing hazard/flee/MovePoint method bodies.

Combined whirlwind, tank-mode, boss-pull and raid-combat/Lua/ground regression
suite: **22 tests passed**, plus **10 source-publication workflow tests**.
Three production-header syntax checks passed:
`TrashWhirlwind.cpp`, `MovementActions.cpp`, `ReachTargetActions.cpp`.
Official C++ style checks passed for new files/fixture and changed production
lines; whitespace checks passed.

The initial combined test invocation named a nonexistent historical
`test_tank_target_protection` module and omitted the existing raid-combat suite's
`PYTHONPATH=scripts/tests` requirement. Those were test-invocation errors; the
corrected available suite passed. No failing assertion was waived.

These tests are **not live navigation/encounter acceptance**. Admission checks
use a conservative straight segment; existing terrain pathfinding still builds
the actual route, so this is not a full hazard-aware navmesh planner. Walls,
multiple overlapping hazards, immobilization, reaction latency and explicit
stay/commands may still prevent an escape. Direct scripted movement or gap
closers outside these shared entry points are not universally intercepted.

The initial source-only work performed no build, restart or live-state change.
An explicitly authorized full build/deployment followed on October 5. On normal TK trash
pulls, non-owning melee should step away during the
spin and resume afterward, ranged/healers should retain safe casts, and the
owning tank should continue defending. Do not force a wipe or alter loot solely
for validation.

Reproduce the offline suite:

```sh
PYTHONPATH=scripts/tests python3 -m unittest \
  scripts.tests.test_trash_whirlwind scripts.tests.test_tank_modes \
  scripts.tests.test_raid_boss_pull_focus scripts.tests.test_raid_combat -v
```

Temporary validation logs: `/tmp/trash-whirlwind-tests.log`,
`/tmp/trash-whirlwind-regressions-final.log`, `/tmp/trash-whirlwind-syntax.log`,
`/tmp/trash-whirlwind-style.log`.
