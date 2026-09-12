# Complete Twins reset deployment — September 12, 2026

## Result and authorization

**Deployed, worldserver ready.** The user authorized fixing the reset, then explicitly authorized
building/deploying while they went biking. Only worldserver was stopped/recreated; no live reset
was performed. Auth, database and Pi bridge stayed running. Further interruptions need fresh
permission; this deployment permission is consumed.

Source/history on root `fork/main`:

- `d6cc7f1`: [complete targeted Twins reset](twins-targeted-reset.md).
- `ee90a69`: correct this core's `GetThreatMgr` API spelling, align the test double/header contract.
- `f9d3fe9`: model deferred native door respawn and incomplete/retry behavior in regression tests.

Core and playerbots revisions/working trees are unchanged from the prior bug-scaling deployment.
The earlier bug-scaling, stockup, durability and coordination changes remain included.

## Build and image

Initial native build caught `GetThreatManager` versus this core's `GetThreatMgr`; it failed before
any service interruption. That was corrected, tests rerun, and a **fresh corrected-source backup**
made before rebuilding. Initial attempt/evidence remains under
`backups/twins-reset-predeploy-20260912-172244/`.

Corrected full 1888-step Ninja/cache-reused build passed. It used host-network Docker, the existing
Dockerfile/worldserver target, actual full working source, and the previously pinned Ubuntu base:

```text
ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254
```

Full source-input comparison with the preceding deployed bug-scaling image showed only:

```text
Changed: modules/mod-raid-scaling/src/RaidScalingMgr.cpp
Changed: modules/mod-raid-scaling/src/RaidScalingMgr.h
Added:   modules/mod-raid-scaling/src/RaidScalingTwinsReset.cpp
Removed: none
```

No setup/update/reset-to-pins was used. The older unrelated `0021` full pinned-replay limitation
remains; this is a build from preserved actual source, not a claim of successful full regeneration.

```text
Image tags:
  acore/ac-wotlk-worldserver:twins-reset-20260912
  acore/ac-wotlk-worldserver:master
Image ID:
  sha256:d154241b29ad8459eb69a2691b0d9955ebac14415105f38641c1645b3ff1b493
Image/running binary SHA256:
  442b03961c304992cd76a4167bede0228386c5a369edd67f549ef32e63d870d2
Worldserver started:
  2026-09-12T15:32:26.302657396Z (17:32 CEST)
Initialization:
  17 seconds; worldserver-daemon) ready...
State:
  running, restart count 0, OOM false, AC_UPDATES_ENABLE_DATABASES=0
```

## Verification and preserved state

- **85 tests passed, one optional connection-local MySQL test skipped** (86 total), including seven
  reset regressions plus all previously selected bug-scaling, stockup, durability, Twins/AQ40, pet,
  skull, chatter/Pi, scaling/reset and statistics tests. Scoped C++ style/width checks passed.
- Thirteen expected production symbols, three Twins action names, and stockup/reset messages were
  found in the image binary extracted from a **never-started** container. No second worldserver ran.
- Reset disassembly confirms native completion-mask persistence, AI reinitialization, per-spawn
  creature/gameobject respawn processing and door-state handling. Existing bug-scaling and earlier
  behavior symbols remain present. Image/running binaries are byte-identical.
- The non-random-player online gate was zero. Old worldserver stopped cleanly, exit 0, no OOM.
- Fresh all-four-database backups were taken before build and again after shutdown, with gzip
  integrity, SQL completion marker and checksums verified before the new server started.
- All saved instances, binds, AQ creature respawns and progression deadlines match the **final
  stopped-worldserver** snapshot exactly after startup.
- Auth/DB container identities/images/start times/restart counts and Pi PID/start state unchanged.
  Pi `/api/tags` healthy; no generation was requested.
- Runtime configs, both envs, Compose files, root flakes and build-source hashes unchanged.
- Chatter loaded 272 persistent personas; area backfill wrote zero rows. Existing unrelated warnings
  are not claimed fixed or absent.

Saved AQ40 state **retained, not reset by deployment**:

```text
instance 5670, map 531, difficulty 0
completedEncounters 127
"A Q T 5 3 3 3 3 3 3 3 5 0 " (trailing space retained)
stage 2
resetTime 1789397072
extendedResetTime 1789656272
10 permanent binds, all extended=0
```

The seventh completion is the user's reported `.damage` Twins kill. No claim of a legitimate Twins
kill or live validation is inferred from this save. No older six-/four-kill snapshot was restored.

## Backup and rollback

Authoritative corrected-source deployment backup:

```text
backups/twins-reset-predeploy-20260912-172606/
```

Contains full source/config archives, manifests, git state, four DB dumps before build/after stop,
rollback image archive, saves/binds/AQ respawns, binaries/disassembly, container/Pi state, scripts and
logs. `SHA256SUMS`, `STOPPED-SHA256SUMS` and `DEPLOYMENT-SHA256SUMS` verified. The original
preflight style log was retained unchanged for its checksum; final corrected-source style evidence
is stored separately as `corrected-source-style.log`.

```text
Rollback tag: acore/ac-wotlk-worldserver:pre-twinsreset-20260912-172606
Rollback image:
  sha256:0816bfbf2db2efdc2b697107f43adc9447c2e2fdfeaba029bc82096488883b4e
```

A source-only rollback need not restore databases or discard newer progress, but any further
worldserver interruption requires permission.

## User's next action / limitations

The reset itself was deliberately **not executed**. Out of combat, safely back near the room
entrance, the user can run `.raidinstance boss reset 7`, confirm `NOT_STARTED` with `boss list`,
then leave/re-enter the entrance to replay the intro before pulling. If an incomplete message
appears, do not pull; wait a tick and retry/report it. No gear, loot, binds or deadline adjustment
is performed by the agent. No SQL migration/importer, bot command or Pi generation was run.

Native build/startup and API-double tests do not prove live reset scheduling, intro/client behavior,
pathfinding, healing throughput or encounter difficulty. The first proper rematch remains the
live validation opportunity.
