# AQ40 encounter deployment — 2026-09-11

## Result

The user explicitly authorized deployment after finishing AQ40. The full
worldserver image build passed, worldserver shut down cleanly (exit 0, no OOM),
and **only worldserver** was recreated. Readiness was observed, with restart
count zero. Authserver, database and Pi bridge retained their identities and
start times. This deployment permission is consumed; future interruptions need
fresh authorization.

Activated:

1. [Bug Trio wipe recovery](aq40-bug-trio-wipe-reset.md): restore original consumed
   spawns through the native respawn queue; isolate the consumption target per
   instance and avoid repeated/completed-state evade resets.
2. [Viscidus hit counts](viscidus-ten-player-hit-counts.md): 25/38/50 frost stages
   and 13/25/38 cracking/shatter stages. The frozen window remains 15 seconds.
3. [Huhuran Wyvern Sting](huhuran-ten-player-sting.md): at most three nearest
   eligible targets. Both the custom cast and target filter use the same cap;
   Poison Bolt retains its separate DBC limit and damage values are unchanged.

The two numeric adjustments are fixed local ten-player tuning, not attendance
or manual raid-size dependent. No general raid HP/damage setting was changed.
No setup, importer, SQL migration, manual encounter reset, bind manipulation,
gear edit or old-save restoration was performed. Normal server startup and
shutdown still perform their ordinary database writes.

## Versions and build scope

- Root before deployment documentation: `615cbe0`.
- Core: `5a6f664eec08cecaf1ab4ede235c9db532071647`.
- Core fixes: `63aa5aadd`, `267f29e60`, `5a6f664ee`; canonical patches 0022–0024.
- Playerbots: `b57c412b`, with the same pre-existing local work as the prior build.
- Core fork branch: `CmPons/azerothcore-wotlk:fix/aq40-ten-player-encounters`.
- Image tags: `acore/ac-wotlk-worldserver:aq40-encounters-20260911` and `:master`.
- Image ID:
  `sha256:d81a7bbed1a7d0517e79618dd152d763baf8bea1c9de57d2e1d2cbe5553cbc20`.
- Running executable SHA256:
  `399317dbe52572ee5a95635956ec24cb81d2941f1a3825251276b7bd47ee8953`.
- Worldserver start: **2026-09-11T16:49:36.727969848Z** (18:49:36 CEST).
- Readiness identifies core `5a6f664eec08+`, Playerbot branch, RelWithDebInfo,
  Static; world initialization reported 18 seconds.

Build used `docker build --network=host`, target worldserver, the full current
working tree and the existing Dockerfile/arguments. A manifest comparison to the
September 9 deployed build showed **exactly five changed files and no added or
removed build-input files**: the trio boss/instance/header, Viscidus and Huhuran.
All pre-existing nested module changes were preserved and were already part of
the previous image. The complete actual build source was archived, and its
checksums still matched after the build.

The build completed 1,885 Ninja steps with cache reuse. Ubuntu 24.04 resolved to
`sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254`.
Unrelated compiler warnings remain in the build log; this is not a warning-free
build claim.

## Progress preservation

The authoritative comparison uses the final **stopped-worldserver** snapshot,
not an older backup that could discard recent progress. All saved instance rows,
character binds and AQ40 creature-respawn rows matched exactly after startup.
Managed boss states and deadlines were also separately compared.

AQ40 save **5670**, map 531, difficulty 0:

- Skeram, Bug Trio, Sartura and Fankriss remain DONE.
- Completion mask: **15**. Viscidus and Huhuran were not marked completed.
- Serialized state: `A Q T 5 3 3 3 3 0 0 0 5 0 ` (trailing space retained in DB).
- Progression stage: **2**.
- Reset deadline: **1789397072** — September 14, 14:44:32 UTC.
- Extended deadline: **1789656272** — September 17, 14:44:32 UTC.
- **Ten permanent character binds**, all with `extended=0`, unchanged. The
  extended deadline's presence does not mean these binds are currently extended.

No completed trio was respawned just to test the fix. No old BWL save was restored.
All pre-existing runtime `.conf` files, both `.env` files and both Compose files
matched their fresh backup byte-for-byte. Root `flake.nix`/`flake.lock` hashes
were unchanged; the pre-existing intent-to-add `flake.lock` was not committed.

## Verification and limitations

- **56 offline tests passed; one optional connection-local MySQL test skipped.**
  Suites: Huhuran 4, Viscidus 4, trio 3, AQ40/statistics 5, scaling 7, hunter pet 4,
  skull 3, chatter routing 5, bridge 13, progression 8 passed plus 1 skipped.
- Official targeted C++ style checks and whitespace checks passed.
- Inspected the new image without starting a second worldserver. Executable
  disassembly contains the trio's native respawn-queue call, Viscidus's 50 and 38
  comparisons, and Huhuran's cap of three conditional on spell 26180.
- The running executable is byte-identical to that inspected image executable.
- Image and running-container metadata confirm database updater disabled.
- Native readiness observed; final status running, restart count 0, no OOM.
- Existing invalid-skill startup messages remain; startup is not claimed error-free.
  Chatter loaded 237 persistent personas; its area backfill completed with zero
  writes. No artificial player login or encounter was started for verification.
- Authserver/database container IDs, images, start times and restart counts match
  predeployment records. Pi PID/start/state match; `/api/tags` responded without
  generating a model request.

**These checks do not prove live combat behavior.** Observe a future legitimate
trio wipe, Viscidus's freeze/shatter sequence and Huhuran's actual sleep delivery
in game. Bot coordination, immunity/resist handling and encounter difficulty
still require play feedback.

## Backup and rollback

Private backup: `backups/aq40-encounters-predeploy-20260911-184249/`.
Pointer: `/tmp/aq40-predeploy-backup`.

Includes four-database dumps before building and after clean shutdown; old image
archive; runtime configs/envs/Compose; complete build source/checksums and nested
diff/status; build/test/style logs; save/bind/respawn snapshots; inspected/running
binaries and disassembly; before/stopped/after container and Pi metadata; copied
preflight/deployment scripts. Gzip integrity and SQL completion markers checked.
`SHA256SUMS`, `STOPPED-SHA256SUMS` and `DEPLOYMENT-SHA256SUMS` were verified.

Rollback tag: `acore/ac-wotlk-worldserver:pre-aq40-20260911-184249`.
Rollback image:
`sha256:464d82c98891e5e3bb75ebbdc748aeee8bc5557bbead70c039a1ab1c7818edff`.

Future rollback/restart requires permission and fresh backups. There is no schema
change, so a binary rollback does **not** inherently require restoring an old DB
or losing subsequent raid progress. Retag the rollback image as `:master` and
recreate only worldserver when authorized; leave auth, database and Pi running.
