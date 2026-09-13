# Ouro: original-spawner recovery after a failed pull

## Scope and authorization

September 13, 2026: the user reported Ouro emerging and disappearing, leaving an empty room.
Read-only inspection found encounter 8 in FAIL, the existing Twins completion intact, and original
spawner 88073 / entry 15957 on a far-future respawn timer. The user authorized **fix and build only**,
explicitly **no deployment**, while learning C'Thun. C'Thun's Eye Beam is a separate investigation.

This patch repairs recovery, not the as-yet-unconfirmed reason for the initial evade. No boss
health, damage, targeting, threat, submerge timing, loot or playerbot strategy is changed.

Canonical patch: `patches/0030-core-aq40-ouro-spawner-recovery.patch`.
Sole production file: `src/server/scripts/Kalimdor/TempleOfAhnQiraj/instance_temple_of_ahnqiraj.cpp`.
`boss_ouro.cpp` remains byte-identical to the pre-edit source.

## Cause

The spawner despawns itself in `npc_ouro_spawner::JustSummoned` after creating Ouro. Its default
spawn group uses non-compatibility respawning: despawn removes the creature from the map and
`InstanceScript::OnCreatureRemove` erases the typed GUID. The old FAIL handler called
`GetCreature(DATA_OURO_SPAWNER)->Respawn()` only if that now-missing pointer still resolved.

The original DB spawn remains in the map's respawn queue. Its seven-day template respawn delay
can be converted to the native year-ahead lockout sentinel by `Map::SaveCreatureRespawnTime`;
that timestamp is not, by itself, evidence of database corruption. The missing recovery path
failed to bring this original spawn's timer forward after the failed encounter.

## Correction

- Coalesce a delayed recovery check after Ouro FAIL, relevant creature removal, or player entry.
  Player entry also handles already-failed saves after map unload/restart, without relying on a
  runtime GUID or on a state-load callback returning true.
- Only recover while Ouro is NOT_STARTED or FAIL. Wait while an Ouro creature or any tracked
  Ouro dirt mound remains; a surviving mound may still re-emerge into the same encounter.
  Normal IN_PROGRESS submerges must not create a new spawner/boss.
- A surviving dead spawner object uses native `Respawn()` without force. A living spawner is left
  alone; compatibility-mode recovery resets its normal AI through the native respawn lifecycle.
- When the GUID has gone, identify exactly one matching original, single-entry spawner in this
  instance's existing respawn timers. Validate map and entry; snapshot its spawn ID before queue
  mutation, then request the native timer at current game time.
- Let normal map/grid respawning recreate that original spawn. No replacement summons, forced
  grid loads, synchronous queue draining, recursive evades or direct SQL repair.
- Reject late Ouro FAIL/NOT_STARTED transitions after DONE, so leftover mound callbacks cannot
  reopen a completed encounter. Other bosses retain their normal state handling.
- Preserve base player-enter and creature-remove callbacks and the instance's other scheduled work.
  No other encounter state, completion mask, bind, deadline, inventory or respawn is edited.

If activated later, eligible failed-save recovery occurs on normal player entry and respects the
native loaded-grid lifecycle. **Nothing is restored by merely editing or building this patch.**
Deployment and any live recovery still require fresh approval. Do not use the unsafe native
`.respawn creature guid` workaround or restore historical saves over newer C'Thun progress.

## Tests and limitations

Four focused tests compile actual production methods with checked STL iterators, warnings as
errors and UBSan. They reproduce the previous missing-GUID failure and cover original-spawn
recreation, live/dead/compatibility cases, delayed removal, submerge and surviving-mound guards,
re-engagement, coalescing, unrelated scheduled work, unloaded grids, persisted idle states, DONE
protection, malformed/ambiguous metadata, instance isolation and unchanged C'Thun state handling.
Patch round-trip, focused pinned core replay and native queue/lifecycle contracts are checked.
The older Bug Trio fixture unwinds/replays 0030 around 0022 because both touch the instance file.
The selected regression suite passes: **94 tests, 93 passed and one optional MySQL fixture skipped**.
Scoped production C++ style and added-line checks pass.

These are offline API doubles, not a live pull or proof of the initial evade cause. A future
approved deployment and ordinary pull are needed to validate live recovery. Build-time database
backups are live snapshots, not final stopped saves; never restore them over ongoing progression.

Preparation backup: `backups/ouro-recovery-preparation-20260913-122640/`.
