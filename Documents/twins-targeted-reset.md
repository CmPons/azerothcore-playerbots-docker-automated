# Complete targeted Twin Emperors reset

## Status

Implemented and **deployed September 12, 2026**. The first deployment refused the real room's
scarab/scorpion variants; the corrected version is now deployed with explicit permission. See the
[initial deployment](twins-reset-deployment-20260912.md) and
[variant correction / crash incident](twins-reset-variants-20260912.md). No manual SQL edits,
migration, database restore, loot/gear changes or instance-wide reset is part of this feature.

The saved Twins completion was from the user's `.damage` kill, not a legitimate kill or validation
of the newly deployed bug scaling. Deployment must preserve that latest save until the user chooses
to reset the encounter.

## Usage after deployment

Inside your existing AQ40 instance, **out of combat**, stand safely back near the Twins room entrance
(not beside their spawn positions) with the group:

```text
.raidinstance boss list
.raidinstance boss reset 7
.raidinstance boss list
```

On success, Twins should be `NOT_STARTED`, both emperors and all their linked bugs restored, and
only their completion credit cleared. Leave/re-enter the room entrance to replay the native intro,
then pull normally. Their bug scaling is retained. Do **not** use `.raidinstance reset all`.

The command refuses while any encounter, player or loaded creature on this map is in combat, or
while an encounter creature is player-controlled. If it reports **incomplete**, do not pull: retry
out of combat and report the message if it persists. It does not falsely claim success when a
native respawn or door restoration failed.

## Why the old recipe was insufficient

The old recipe set boss state 7 to `NOT_STARTED` and respawned dead entries 15275/15276/15963 plus
some door objects. It omitted the scarabs/scorpions, separate encounter-completion mask, used intro
trigger and leftover living/modified creatures. It was explicitly marked `partial/shared health`.

The new path is restricted to AQ40's Twins recipe. Other bosses' reset behavior is unchanged.
Canonical module files (mirrored to `azerothcore-wotlk/modules/mod-raid-scaling/src/`):

- `RaidScalingMgr.cpp`: recipe status and targeted dispatcher.
- `RaidScalingMgr.h`: reset declarations and one-shot pending instance ID.
- `RaidScalingTwinsReset.cpp`: complete encounter recovery and scoped console request handling.
- `RaidScalingCommand.cpp`: confirmed console-only `raidtwinsreset` entry point.
- `RaidScalingLoader.cpp`: consume a requested reset after instance script/scaling initialization.

The root-managed module is already synchronized by setup/update; no core patch or SQL migration is
needed. No setup/update is run against the live source for this change.

## Recovery sequence

1. Reject combat before changing encounter state; validate the core's encounter-credit mapping.
   Resolve the kill-credit records for Vek'nilash/Vek'lor from the map/difficulty encounter list.
   Use `dbcEntry->encounterIndex`, **not** the script boss index. Locally these are respectively
   completion bit 6 and script state 7. Missing/invalid mappings are rejected rather than guessed.
2. Find the full original spawn catalogue for this map/difficulty, including unloaded/despawned
   creatures: exactly one of each emperor, one Master's Eye, and the scarabs/scorpions whose native
   linked respawn points to Vek'lor. Require both bug species and both doors; reject ambiguous,
   inactive or pooled creature data. Bosses/controller remain single-entry. Linked room bugs may
   have native alternate entries, but every alternate must be scarab 15316 or scorpion 15317.
   Corridor bugs and all other alternate entries remain excluded.
3. Load the selected room/door grids, recheck combat and reject controlled targets. Grid loading
   itself uses normal core behavior; a refusal at this stage may have loaded grids but has not
   changed encounter state or completion credit.
4. Set Twins to `NOT_STARTED`, verify acceptance, and clear only their resolved completion bit using
   `SetCompletedEncountersMask(..., true)`. These ordinary core APIs save to the DB and update its
   in-memory save cache; there are no hand-written SQL statements. Reset only intro trigger **4047**.
5. Snapshot loaded creature GUIDs before callbacks. Cancel old casts/events/threat/auras, then use
   native `Respawn(true)` for both living and dead encounter creatures. This does not invoke
   `Unit::Kill`, shared-health kill callbacks or new kill rewards. Compatibility-mode creatures
   also reinitialize their AI, clearing the old intro flag/scheduler and restoring native AI state.
6. Advance and process **only each selected original spawn's** native respawn. Recreate emperors
   before their linked bugs so the bugs do not inherit the old boss deadline or a random linked
   respawn delay. Unloaded/deleted corpses are covered by the catalogue, not assumed still present.
   No temporary summons, direct DB-spawn construction or map-wide respawn/removal queue draining.
   Normal old-corpse removal remains with the core; exactly one living creature per selected spawn
   is required before success. Old corpse removal cannot erase the new instance GUID mapping.
7. Apply the existing instance scaling to only the restored encounter creatures, not every loaded
   raid creature. Explicit scaling-off and manual settings remain intact.
8. Restore/process only the two original doors, then explicitly set entrance **180634 open** and
   exit **180635 closed**, including repeat resets already in `NOT_STARTED`. Require one spawned
   object per selected door before success. The native instance script controls them on later pulls.

Current world data has two emperors, one Eye, and **100 linked room-bug spawn locations**: 103
creature spawns total, plus two doors. Fifty bug records have scarab as their base entry and fifty
have scorpion; all 100 allow the other species through `creature_multispawn`, so actual living
species counts are random, not necessarily 50/50. Counts are derived from data; spawn GUIDs are
not hardcoded. Other kills, binds, progression deadline, extension state and player inventory are not
modified. Previously acquired `.damage` loot is not removed; unlooted old corpses are replaced.

The reset is retryable/idempotent, not a claimed multi-statement SQL transaction. A native failure
after state changes is reported as incomplete rather than attempting an unsafe automatic rollback.

## Verification

`scripts/tests/test_twins_reset.py` compiles the actual reset implementation, dispatcher and header
against offline map/API doubles, with C++20, warnings-as-errors and undefined-behavior sanitizer.
Eleven reset tests cover:

- Both emperors, unloaded Eye/dead bug, surviving mutated bug, original respawn order, intro/doors,
  one-live-spawn checks and repeat reset before old corpse objects have been removed.
- Preserved other kills, deadline, extension, binds, corridor mobs and another live instance.
- Compatibility-mode AI reinitialization, scaling-off and encounter-only scaling calls.
- Combat/controlled-target checks, including combat detected after grid loading.
- Missing/invalid credit mappings, dynamic mask index, missing bug links/doors, pools, inactive
  groups, multi-entry spawns and wrong-map catalogue entries.
- Native respawn failure reporting and successful retry; unchanged other-boss dispatch.
- Canonical/build-tree identity and source contracts for native respawn/GUID bookkeeping.

These tests model map lifecycle APIs; they do not execute a live reset or prove full live scheduling,
pathfinding, encounter difficulty or client behavior. Native compilation and startup checks are
performed separately during the authorized deployment. No completed boss is reset just to test it.

The corrected full selected suite passed **89 tests, with one optional connection-local MySQL test
skipped** (90 total). Added coverage includes native bug variants, immediate spawn-store erasure,
one-shot console requests and confirmation guards. The actual legacy native respawn loop fails
under checked iterators in the offline reproduction; the corrected module's GUID snapshots pass.
Scoped C++ style checks passed. The full native build passed after correcting this
core's threat accessor spelling (`GetThreatMgr`, not `GetThreatManager`) and aligning the test
double/header contract. A deferred native door-respawn test also verifies incomplete/retry behavior.

Preparation backup: `backups/twins-reset-preparation-20260912-170514/`.
The first deployment preserved the existing seven-completion save; its reset attempt subsequently
refused without mutation. The user's native state command then saved Twins as `NOT_STARTED`, but
the native GUID respawn command crashed. The corrected deployment preserved that latest state,
then an explicitly authorized one-shot reset was requested for instance 5670.

## Console operation

`raidtwinsreset <existing-AQ40-instance-id> confirm` is console-only. It runs the same guarded reset
on a loaded AQ40 instance, or queues it once for that instance's next map creation (after script
state and default scaling initialize, before player entry). A queued request **is not a completed
reset**: check the `[RaidScaling] Twins reset for instance ... completed` log before pulling.
Only one request may be pending; it is in memory and lost on restart. The request is consumed
before execution even on refusal/failure, so it cannot later retry unexpectedly during combat.
Unknown/unallocated IDs, malformed input and missing confirmation are rejected. Existing in-world
`.raidinstance boss reset 7` remains available.

Do not use the legacy `.respawn creature guid` workaround: that native command still has its
iterator-invalidating loop. This correction uses the safe module path; it does not patch the core
command.
