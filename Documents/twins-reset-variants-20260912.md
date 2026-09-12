# Twins reset correction and live recovery — September 12, 2026

## Result

**Corrected reset deployed and executed successfully** for AQ40 instance **5670** at
**19:37:00 CEST / 17:37:00 UTC**, after explicit user authorization for implementation, build,
worldserver restart and the actual Twins reset. User logged out for deployment, then logged in
to load the instance. Only worldserver was restarted; no auth/DB/Pi interruption.

Native success log:

```text
Twin Emperors reset: restored 103 original spawns (100 room bugs), cleared their completion credit
and reset the intro/doors. Other kills, binds and reset deadline retained.
[RaidScaling] Twins reset for instance 5670 completed.
```

Read-only DB comparison immediately after the reset verified:

- Completion mask **127 -> 63**, clearing only Twins credit.
- Twins script state **NOT_STARTED**. It was already NOT_STARTED in the latest saved state following
  the user's manual state command; no historical seven-/six-kill snapshot was restored.
- All other encounter states unchanged: the other six kills retained.
- All instance binds unchanged, including the ten permanent AQ40 binds and extension flags.
- Progression stage **2**, deadline **1789397072**, extended deadline **1789656272**, unchanged.
- Other saved instances unchanged. No SQL edits, migration, database restoration, gear commands,
  loot removal or whole-instance reset used. Native APIs persisted the reset normally.

User was told to step out/back through the entrance for the intro and cleared to pull. This verifies
successful native recovery, not a legitimate kill or live combat balance/pathfinding/healing.

## Two distinct defects

### Refusal on legitimate bug variants

The initial reset rejected any `id2`/`id3` creature entry. Read-only inspection of
`creature_multispawn` showed **all 100 linked room-bug records legitimately alternate scarab15316
and scorpion15317**. Fifty base records name each species, but actual living counts are randomized.
These were neither pooled nor assigned a custom inactive spawn group.

The corrected guard permits only this two-species variant set on Vek'lor-linked room bugs. Bosses
and Master's Eye remain single-entry; unknown variants, pools, inactive groups and corridor bugs
remain excluded. Refusal messages now name the spawn and specific failing condition.

The first API-double fixtures missed this actual data shape. They now contain native variants by
default, exercise selection of the opposite species, and reject unsafe variants in both slots.
Existing real scaling-manager tests additionally verify alternate live entry versus original DB
link identity, including mutated HP and damage scaling. No additional tuning was applied.

### Crash in the recommended native workaround

At **19:16:34 CEST**, worldserver received SIGSEGV. Docker restarted it automatically at
17:17:21 UTC, reaching ready at 17:17:40 UTC. This was not an agent-issued restart and not OOM.

Kernel/core offset **0x15a8a4e**, resolved against the matching deployed binary:

```text
Unit::isDead() const
  inlined by misc_commandscript::HandleRespawnCreatureByGuidCommand
  cs_misc.cpp:2472
```

That native command iterates the spawn store while `Respawn(true)` calls removal cleanup, which
can erase its current iterator. The actual native loop is now extracted into an offline regression;
checked libstdc++ iterators reproduce the invalid/singular-iterator failure. Core dumps are disabled
for that intentionally failing test.

The module reset instead snapshots GUIDs before callbacks and resolves them afresh. Tests now
model immediate store removal (native cleanup) as well as deferred removal and repeat recovery.
**The legacy core command itself was not patched: do not use `.respawn creature guid` as the
workaround.** The agent acknowledged that recommending it was unsafe.

## Console execution

Root source commit **613d5a8**, pushed to `fork/main`, includes:

- Correctly scoped variant acceptance and clearer diagnostics.
- `raidtwinsreset <allocated-instance-id> confirm`, console-only.
- Immediate execution if that AQ40 map is loaded, or a single in-memory request consumed after
  the instance script and scaling initialize on its next load. No map creation, forced player login,
  teleport or offline bind manipulation is needed.
- One pending request; consumed before execution even on failure. No delayed automatic retry in
  combat. Restart discards any pending request. A queue acknowledgment is not reset success.
- Tests for missing confirmation, malformed/overflow/unallocated IDs, in-world rejection, wrong
  map/instance, one-shot consumption and failed-request behavior.

The original in-world `.raidinstance boss reset 7` also uses the corrected reset.

At 19:36:08 CEST the agent sent only `raidtwinsreset 5670 confirm` through the existing Docker
console connection. No network admin service was enabled. An initial socket lookup failed before
sending any command; the actual connection used the existing rootless `DOCKER_HOST` socket.
The instance was unloaded, so the request queued. User login loaded it at 19:37:00; success was
then logged and saved state verified before the agent cleared the pull.

## Build/deployment evidence

- **89 tests passed, one optional connection-local MySQL test skipped (90 total).** Eleven are
  reset tests, including the expected-failure native iterator reproduction. Scoped style passed.
- Full native worldserver build passed without further compiler corrections.
- Four native input files changed versus the prior deployed reset image: `RaidScalingTwinsReset.cpp`,
  `RaidScalingMgr.h`, `RaidScalingCommand.cpp`, `RaidScalingLoader.cpp`. No additions/removals.
- Actual full source backed up; no setup/update/reset-to-pins. Older unrelated `0021` full-replay
  limitation remains. Root modules and build mirrors identical; nested core/playerbot work preserved.
- Existing Dockerfile/worldserver target, host networking, pinned Ubuntu base retained.
- Sixteen expected production symbols inspected from a **never-started** image container; image
  and running binaries identical. No second worldserver was run.
- Fresh four-DB backups before build and after clean worldserver stop. Saved instances, binds and
  AQ respawns matched the stopped snapshot on startup **before** the authorized reset.
- Runtime configs/env/Compose/root flakes unchanged. Auth/DB identities and Pi PID/start unchanged;
  `/api/tags` healthy, no Pi generation requested.

```text
Image: acore/ac-wotlk-worldserver:twins-variants-20260912 (also :master)
ID: sha256:569c28cb2d0ed4ecf204fd7d7dd9f4fcdb183cb25911a0706755505802306ba1
Binary: f7bc44617f97d6bd881e52c16e5ded62e6012c948e826ebc2372aaf567faa655
Started: 2026-09-12T17:34:16.315610602Z (19:34 CEST)
Ready: 14-second world initialization
State at reset verification: running, restart0, OOMfalse, updater disabled
```

Backups:

```text
backups/twins-reset-variants-preparation-20260912-191224/
backups/twins-variants-predeploy-20260912-193138/
```

The preparation directory preserves pre-edit sources/status/diffs plus spawn-variant evidence,
kernel/core metadata and symbol resolution. Deployment directory contains full source/config/image
archives, four-DB dumps, source manifests, tests, disassembly, service states and reset before/after
snapshots/logs. Predeployment/stopped/deployment checksums were verified; reset evidence is separate.

Rollback tag: `acore/ac-wotlk-worldserver:pre-twinsvariants-20260912-193138`, image
`d154241b29ad8459eb69a2691b0d9955ebac14415105f38641c1645b3ff1b493` (prior reset image).
Do not restore its historical DB snapshot over this successful reset or later progress. Further
interruptions or resets require fresh permission; this authorization has been used.
