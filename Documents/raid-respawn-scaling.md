# Raid health scaling after in-place respawn

## Status — October 2, 2026

**Implemented and offline-tested; not built or deployed.** The user authorized the
proper fix after confirming that `.raidscale trash hp 0.4` corrected the current
SSC guards. That authorization is not a new build/restart authorization.

Core hook commit: `8b8b7bcf8615c62b4aedaa00c33713a0c1f6df0e`.
Root-authored module changes are mirrored into the native build tree. Native core
and module must be built together: the module consumes a new core script hook.
No live configuration, database, equipment, lockout or running process was changed.

## Observed problem and scope

The user saw approximately **600k HP per Fathom-Guard**, while Karathress had his
correctly scaled **509k HP**. Installed data at the configured 25→10 baseline:

| Creature | Native HP | Intended HP (0.4) |
| --- | ---: | ---: |
| Karathress | 1,274,700 | 509,880 |
| Caribdis | 603,120 | 241,248 |
| Tidalvess | 603,120 | 241,248 |
| Sharkkis | 603,120 | 241,248 |

Unscaled guards add 1,085,616 HP above the intended combined encounter pool. The guards
are rank-1 elites without the dungeon-boss flag, so the module uses its **trash**
settings for them. This classification is unchanged.

Read-only spawn-group inspection found only **four compatibility-mode spawns among
2,570 creature spawn rows** in the eight sampled raids (MC, BWL, AQ40, Karazhan, Magtheridon,
SSC, TK and Gruul): this council. They explicitly use Legacy Group 1, flags 3.
This is not a claim about every map or every installation.

Kri/Yauj/Vem use the default dynamic group. Their
[previous reset repair](aq40-bug-trio-wipe-reset.md) had to account for removed
creature objects and queue missing original spawns. Karathress's existing minion
recovery instead uses resolvable objects and in-place respawn. Merely switching his
spawn group would therefore change encounter recovery assumptions. The narrower fix
supports both legitimate core paths, without converting spawn groups or rewriting SSC.

## Cause

1. `Creature::Respawn` in compatibility mode rebuilds stats with `SelectLevel()`.
   That restores native maximum/create/current HP and then returns the creature to life.
2. The same runtime creature remains in the map: `AddToWorld` is not called again.
3. Raid scaling previously listened only to `OnCreatureAddWorld` and explicit map
   application. It missed this rebuild, while a surviving boss could retain scaled HP.

The code path, installed compatibility flags, observed HP and successful manual
reapplication agree. There was no event-level trace of the user's earlier wipe, so
this does not retroactively establish every call made during that particular attempt.
The reset-policy deployment did not edit these health/respawn paths.

## Implementation

- New no-op-by-default `AllCreatureScript::OnCreatureRespawn`, dispatched by ScriptMgr.
- Called **once inside the successful compatibility respawn branch**, after native
  stat rebuild, transition to alive, immediate AI reset and react-state initialization,
  before visibility refresh. Refused/no-op respawns and dynamic recreation do not emit it.
- Raid scaling discards only that creature's previous-life cached baseline, then reuses
  existing world-entry eligibility/provenance and the **current** instance settings.
  This captures fresh native stats rather than compounding a scaled value or retaining
  an earlier level/health pool. It also invalidates stale baselines while scaling is off.
- Defaults are never reinitialized. Manual boss/trash HP and damage factors and explicit
  off/disabled settings remain authoritative. Player-origin, pet, controlled-creature,
  nonraid and noninstance exclusions remain intact. An AI reset that leaves a creature
  dead is not undone by the module.
- Ordinary health reapplication now always uses **current HP/current maximum HP**.
  The old special case for a maximum equal to the cached native maximum could heal a
  wounded creature to its original snapshot at a 1.0 multiplier. Repeated application
  must preserve current percentage, not old-life health.
- Dynamic respawns retain their existing `OnCreatureAddWorld` path. No timer, encounter
  state, bind, loot, spell, damage/healing formula, tank control or movement change.

This is a respawn-boundary fix, not a universal observer of every later stat change.
The native deferred `JustRespawned` callback remains deferred; arbitrary future scripts
that rebuild health later in that callback or elsewhere require their own review.

## Verification

**54 tests passed**, including 11 new tests in `test_raid_respawn_scaling.py` and the
existing creature eligibility, Twins mutation, support scaling, default/manual/off,
Bug Trio reset and source-publication suites. No live encounter was reset for testing.

New executable fixtures compile extracted production manager/loader methods and the
**complete production `Creature::Respawn` body** against explicit world/storage/AI
doubles. ASan/UBSan are enabled. Native stat storage and engine scheduling are not a
complete running server simulation.

Coverage includes:

- 20 same-object respawns: guards return to 241,248 HP, without compounding or carrying
  injured previous-life HP; an unrelated scaled, wounded boss remains untouched.
- Changed native baseline/AI starting HP fraction; restoring scaling uses the new baseline.
- Manual multipliers, explicit off, global disabled/re-enable, inactive instances,
  nonraid maps, pets/player origins and dead-creature protection.
- Ten dynamic recreations and ten mutation-active Twins-addon respawns; native aura
  application/removal retains the existing health-base fix.
- Repeated health application at 0.4, 1.0 and 2.0 preserves current HP percentage.
- Ten full native respawn branches: successful, already alive, condition denied, AI
  denied, linked-respawn deferred, forced, dynamic, no AI, alternate entry and hard reset.
- Exact pre-fix core respawn and pre-fix module loader both compile then fail the new
  runtime assertions. Stale-baseline and old cached-HP implementations also fail.

Four production-header syntax checks passed: `Creature.cpp`, `AllCreatureScript.cpp`,
`RaidScalingLoader.cpp`, `RaidScalingMgr.cpp`. Official C++ style checks passed for
changed production lines and both new C++ fixtures; diffs and module mirrors checked.

Reproduce:

```sh
python -m unittest scripts.tests.test_raid_respawn_scaling \
  scripts.tests.test_raid_creature_scaling scripts.tests.test_twins_bug_scaling \
  scripts.tests.test_raid_support_scaling scripts.tests.test_raid_scaling_default \
  scripts.tests.test_bug_trio_reset scripts.tests.test_source_repos -v
```

## Until deployment

Outside combat, inside SSC, `.raidscale trash hp 0.4` reapplies the intended guard HP.
It is a map-wide HP refresh, not a single-target command; check health and let creatures
refill before pulling. It does not change damage multipliers or reset the lockout.
The user confirmed this workaround. Another in-place respawn can undo it until the new
binary is deployed with separate authorization and fresh preservation checks.
