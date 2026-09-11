# AQ40 Bug Trio wipe recovery

## Status

**Deployed September 11 with explicit permission**, alongside the Viscidus and
Huhuran adjustments. See [deployment record](aq40-deployment-20260911.md).
Only worldserver was restarted; no manual encounter reset, bind change,
configuration change or migration was performed. The already-completed trio
and the rest of AQ40 save 5670 were preserved.

- Core commit: `63aa5aadd`.
- Canonical patch: `patches/0022-core-aq40-bug-trio-reset.patch`.
- Core publication: `CmPons/azerothcore-wotlk`, branch
  `fix/aq40-bug-trio-wipe-reset`. The existing remote `Playerbot` branch diverged;
  it was **not** force-pushed, rebased or merged into the working tree.
- Source backup and read-only spawn evidence:
  `backups/aq-trio-reset-preparation-20260911-172924/`
  (also recorded in `/tmp/aq-trio-reset-backup`).

## Report and diagnosis

After killing two members and wiping, the survivor could return home alone. A
subsequent pull could leave that survivor effectively unkillable, reported at
1 HP. The user reproduced this with ordinary wipe/revive recovery, not only GM
resurrection. GM recovery is therefore not a necessary trigger.

The first two `boss_bug_trio::JustDied` calls remove lootability and schedule
`DespawnOrUnsummon(3s)`. In dynamic respawn mode, corpse removal removes the
creature object entirely and retains its original DB spawn's respawn time.

The old `EvadeAllBosses` only recovered peers obtained through
`instance->GetCreature(DATA_KRI/DATA_YAUJ/DATA_VEM)`. That resolves a live map
object by its recorded object GUID, **not** a DB spawn waiting to respawn.
Once a consumed boss is removed, this branch silently skips it.

Meanwhile `Reset()` clears the shared death count. Fighting the lone survivor
again no longer represents the third death of the attempt. This is consistent
with its lethal-damage/consumption protection becoming stuck without peers.
We have not traced every internal flag in the user's live encounter.

Read-only local checks support the dynamic-respawn path:

| Creature | Entry | Spawn ID | Respawn seconds |
|---|---:|---:|---:|
| Kri | 15511 | 87602 | 604800 |
| Yauj | 15543 | 87601 | 604800 |
| Vem | 15544 | 87603 | 604800 |

All are map 531, without explicit spawn-group membership or linked-respawn rows
for these spawn IDs. The default group is 0, flags 1 (system, not compatibility);
`Respawn.ForceCompatibilityMode = 0`. These are template/config observations,
not a live AI trace. The patch does not hard-code these spawn IDs.

## Changes

Only the three AQ40 encounter source files are changed:

- `boss_bug_trio.cpp`
- `instance_temple_of_ahnqiraj.cpp`
- `temple_of_ahnqiraj.h`

On a surviving bug's normal evade:

1. Use normal BossAI reset to clear combat/schedulers and the existing trio flags.
2. Evade living peers that are not already evading. Force-respawn resolvable dead
   peers through `Creature::Respawn(true)`; never call it on a living evading peer.
3. Snapshot pending respawn IDs in **this instance map**, filtering strictly to
   the three trio creature entries. Make those original spawns due now through
   `Map::SaveCreatureRespawnTime`.
4. Let the native map respawn machinery recreate removed creatures. It retains
   grid/spawn-group checks and its already-alive duplicate check. This is queued
   recovery, not an immediate summon of substitute bosses.

Dead callers, already-evading callers and completed encounters are guarded at
`EnterEvadeMode` before base reset. First-two-death consumption, final-boss loot,
third-death completion and encounter abilities otherwise retain their logic.

The process-wide static consumption-target variable is also replaced with
instance-owned data (`DATA_BUG_TRIO_CONSUME_TARGET`). Resetting one raid copy
must not clear another copy's pending consumption target. This addresses an
additional concrete isolation flaw, not a claim that multiple copies caused
this user's wipe failure. Simultaneous consumption timing within one encounter
is not redesigned by this patch.

No Skeram state, binds, raid progression deadlines, scaling settings, loot tables,
trash respawns or generic core respawn behavior are changed by this patch.

## Validation

```bash
python3 -m unittest discover -s scripts/tests -p test_bug_trio_reset.py -v
python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v
python3 -m unittest discover -s scripts/tests -p test_raid_scaling_default.py -v
python3 -m unittest discover -s scripts/tests -p test_progression_raid_reset.py -v
```

- Bug Trio: 3 tests, including a C++ production-method/API-double harness.
  - The exact old lookup-only method, recovered by reversing the shipped patch,
    reproduces missing peers after consumption.
  - Actual reset, evade, death and instance-data methods execute against doubles:
    all survivors, zero/one/two deaths, both kill orders, retained/removed corpses,
    dynamic/compatibility behavior and successful next-attempt completion.
  - Other-instance target/counter/timer isolation, unrelated respawn records,
    repeated-evade guards, completed-state guards and final lootability checked.
  - Patch reverse/apply round trip and native map/corpse source contracts checked.
- AQ40/UBRS statistics: 5 passed.
- Raid scaling: 7 passed.
- Progression resets: 8 passed; optional connection-local MySQL test skipped.
- Official C++ style checks on the three core files and harness passed.

**These are not full-engine or live-combat tests.** The harness models native
respawn and BossAI services; it does not execute the complete server, actual
pathfinding, delayed-event timing, ability casts or SQL persistence. Full
worldserver compilation subsequently passed and the deployed binary was
verified. In-game wipe-recovery verification remains observational.

## Deployment and live recovery

This is a future-wipe fix, not an automatic migration of an already-broken live
attempt or a general startup recovery overhaul. No manual actor/saved-state edits
were made; saved boss states, binds, deadlines and AQ40 respawn rows matched
exactly across the authorized deployment.

Future deployments require fresh permission and backups. On a future legitimate
trio attempt, verify a two-death wipe restores all three original spawns, then
complete another attempt. Do not reset the current completed trio just to test.

If manual recovery is still needed, the existing targeted command is:

```text
.raidinstance boss list
.raidinstance boss reset 2
```

Use only deliberately, inside AQ40 with the group out of combat and the survivor
already evaded. The command queues/respawns dead entries but does not explicitly
reset a living survivor's AI. This fallback was not exercised during deployment;
wait for all three before pulling. It is not needed for the current completed trio.

Do **not** substitute `.raidinstance reset all confirm`: that unbinds the group
and disables scaling on the current instance rather than just repairing the trio.
