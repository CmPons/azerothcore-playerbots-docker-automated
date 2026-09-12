# Twins bug scaling + stockup deployment — September 12, 2026

## Result

**Deployed and ready.** Fresh explicit authorization: “Go for deploying”. Only worldserver was
cleanly stopped and recreated. The player was asked to log out; the pre-stop non-random-player
online check returned zero. Authserver, database and Pi bridge stayed running.

Deployed source commits on root `fork/main`:

- `53fc352`: [caller-vendor reagent stockup](raidroster-stockup-reagents.md).
- `a8b0fd1`: [Twins encounter bug scaling](twins-bug-raid-scaling.md).

Core remains `5a6f664eec08cecaf1ab4ede235c9db532071647`; playerbots remains
`2de8f55e6415ab1b721866d1558740f85585b9e5`. Their prior working-tree changes were preserved.
This deployment permission is consumed; future interruptions require fresh permission.

## Image and readiness

```text
Tags:
  acore/ac-wotlk-worldserver:twins-bug-scaling-20260912
  acore/ac-wotlk-worldserver:master
Image:
  sha256:0816bfbf2db2efdc2b697107f43adc9447c2e2fdfeaba029bc82096488883b4e
Image/running worldserver SHA256:
  c9391302c522f0a29c63f5e6cff86177bde714b4153ad45dddb0393b02c15bfd
Started:
  2026-09-12T14:36:48.056094997Z (16:36 CEST)
Initialization:
  16 seconds; worldserver-daemon) ready...
State:
  running, restart count 0, OOM false, database updater disabled
```

The native 1887-step Ninja/cache-reused build passed, including the changed roster command,
scaling manager and header-dependent scaling command/loader. Host-network Docker build used the
existing full working source and pinned base:

```text
ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254
```

The full input manifest compared with the deployed durability image found **exactly three changed
files, with no additions/removals**:

```text
modules/mod-raid-roster/src/RaidRosterCommand.cpp
modules/mod-raid-scaling/src/RaidScalingMgr.cpp
modules/mod-raid-scaling/src/RaidScalingMgr.h
```

No setup/update/reset-to-pins was run. The older unrelated `0021` full-patch-replay limitation remains;
this build is from the backed-up actual source, not a claim of successful full pinned regeneration.

## Verification

- **78 tests passed, one optional connection-local MySQL test skipped** (79 total). Includes six new
  bug-scaling tests, five stockup tests, and existing durability, Twins/AQ40, pet, skull, chatter/Pi,
  scaling/reset and statistics regressions. Scoped C++ style/width checks passed during preparation.
- Image binary extracted from a **never-started container**; no second worldserver ran. Twelve
  expected production symbols, three Twins action names and the reagent-only summary were present.
- Disassembly confirms the dedicated room-bug predicate, health-base setter in both scaling and
  restoration, and caller-side vendor search. Existing tank/healer/caster/movement/threat and
  durability-display methods remain present. Image and running binaries match byte-for-byte.
- Clean old worldserver shutdown: exit 0, no OOM. Final four-database dump taken after shutdown,
  gzip integrity and SQL completion marker verified before startup.
- Every saved instance, character bind, AQ creature respawn and progression deadline matched the
  **final stopped-worldserver** snapshot exactly after startup.
- Runtime configs, both env files, Compose files and root flakes are byte-identical to predeployment.
  Build source manifests remained unchanged through build/startup.
- Auth/DB container identities, images, start times and restart counts unchanged. Pi PID/start state
  unchanged; `/api/tags` healthy without generating a response.
- Chatter loaded 272 persistent personas; area backfill wrote zero rows. Existing warnings are not
  claimed absent or fixed by this deployment.

### Latest save preserved — not the older six-kill snapshot

```text
AQ40 instance 5670, map 531, difficulty 0
completedEncounters = 127
saved data = "A Q T 5 3 3 3 3 3 3 3 5 0 " (trailing space retained)
stage = 2
resetTime = 1789397072
extendedResetTime = 1789656272
10 permanent character binds, all extended = 0
```

The latest database already recorded seven completed encounters, including Twins, **before this
build/deployment**. It was preserved exactly as found; this is not evidence of a legitimate kill or
combat validation of the new fix. No boss was reset, and no historical six-/four-kill backup was
restored. The extension deadline does not mean an extension was activated.

## Backups / rollback

Fresh backup:

```text
backups/twins-bug-scaling-predeploy-20260912-163305/
```

Includes full actual source/archive/manifests, runtime configs/env/Compose, repo state, all four
DBs before build and after shutdown, rollback image/archive, before/after saves/binds/AQ respawns,
container/Pi state, image/running binaries, disassembly, scripts and logs. `SHA256SUMS`,
`STOPPED-SHA256SUMS` and `DEPLOYMENT-SHA256SUMS` verified.

```text
Rollback tag: acore/ac-wotlk-worldserver:pre-bugscale-20260912-163305
Rollback image:
  sha256:540f4822b42a5aab6e504691db6cae0c3b87b56f73f9016a5632eae253162217
```

A source-only rollback need not restore databases or discard newer progress. It would require
fresh permission to interrupt worldserver; never automatically restore an older raid snapshot.

## Remaining live validation

No test pull, live stockup command, manual inventory/gear/spec edit, SQL migration/importer, bot
command, encounter reset or Pi generation was performed. Startup/native tests do not prove combat
pathfinding, add-switch timing, healing throughput or whether the encounter will now be beatable.
The mutation tooltip deliberately remains +300% HP / +1800% damage; the fix scales the encounter
bugs beneath that unchanged mechanic. Observe the next legitimate attempt and normal stockup use.
