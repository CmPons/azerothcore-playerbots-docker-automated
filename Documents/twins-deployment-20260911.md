# Twin Emperors coordination deployment — September 11, 2026

## Result

The user explicitly authorized deployment after source preparation. The full image build passed,
worldserver stopped cleanly (exit 0, no OOM), and **only worldserver** was recreated. Native readiness
was observed; final restart count was zero. Authserver, database and Pi bridge stayed running with
unchanged identities/start times. This deployment permission is consumed; future interruptions
require fresh authorization.

The [two-group coordination changes](twins-coordination.md) are now live: physical tank stations,
caster tank acquisition/threat, independent healer coverage, encounter movement, local add/pet
policies and the on-demand `do aq40 twins status` action.

**No boss stats or encounter mechanics were changed.** No setup/update script, importer, migration,
manual SQL change, raid reset, bind manipulation, gear edit or old-save restoration was performed.
Normal server shutdown/startup still makes its ordinary database writes.

## Versions and build evidence

- Root source publication: `9e6382e` (`fork/main`).
- Core unchanged: `5a6f664eec08cecaf1ab4ede235c9db532071647`.
- Playerbots: `131ec19521f8d4a77d5d2c37288b2e2635b040c8`, plus preserved pre-existing local work and
  patch-managed AQ40 source. Canonical coordination patch `0026`; `0025` records the already-deployed
  raid-threat foundation, not an additional live behavior change in this deployment.
- Image tags: `acore/ac-wotlk-worldserver:twins-coordination-20260911` and `:master`.
- Image ID:
  `sha256:c8b13b52ce0f94e6f8f24413178c1e3546bb25fa9ed09b99d39c697d4beb1eef`.
- Image and running executable SHA256:
  `c1bafbf17866f0dc7ab97fd2f17e744527f3323dce4930f7bc2400cd5a674fa8`.
- Worldserver start: **2026-09-11T20:52:53.744708551Z** (22:52:53 CEST).
- World initialization: **15 seconds**; native `worldserver-daemon) ready...` observed.

Build used `docker build --network=host`, the existing Dockerfile, worldserver target, and the full
actual working tree. Ubuntu 24.04 was explicitly pinned to the same digest as the previous build:
`sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254`.
All **1,887 Ninja steps** completed with cache reuse, including both new coordination translation
units and the shared action context. Existing unrelated compiler warnings remain.

A complete input-manifest comparison against the earlier September 11 AQ40 deployment found
**exactly 12 changed and two new playerbot source files, with no removals**. Core, other modules and
unrelated playerbot source were unchanged. Source checksums matched before/after building and
again before shutdown/after startup. The full pre-existing working tree was preserved rather than
regenerated from pins; the previously documented older `0021` patch-replay caveat remains separate.

## Progress preservation

The authoritative comparison used the fresh **stopped-worldserver** database snapshot. All saved
instances, character binds and AQ40 creature-respawn rows matched exactly after startup. Managed
encounter states and reset deadlines were also compared separately.

AQ40 instance **5670**, map 531, difficulty 0:

- **Six completed bosses**, completion mask **63**: Skeram, Bug Trio, Sartura, Fankriss, Viscidus,
  and Huhuran. Twins remain unfinished.
- Serialized state: `A Q T 5 3 3 3 3 3 3 0 5 0 ` (trailing space retained).
- Progression stage **2**.
- Reset deadline **1789397072** — September 14, 14:44:32 UTC.
- Extended deadline **1789656272** — September 17, 14:44:32 UTC.
- **Ten permanent character binds**, all `extended=0`, unchanged. The extended deadline does not
  mean the characters have activated an extension.

This preserves the later Viscidus/Huhuran kills; the previous deployment's four-kill snapshot was
**not** restored. No real player was online at the shutdown gate.

## Verification and limitations

- Fresh deployment regression run: **63 tests passed, one optional connection-local MySQL test
  skipped**. Twins production/API-double/legacy cases plus Trio, Viscidus, Huhuran, pet taunt,
  skull, chatter routing, bridge, scaling, progression and statistics suites.
- All 14 production source files had passed scoped core C++ style and 120-column checks; the
  manifest confirmed those same sources went into the native build.
- Inspected the image executable from a **never-started container**, not a second worldserver.
  Expected tank assignment, healer coverage, caster action, status, movement and threat-integration
  symbols were present. Stored their disassembly and checked the three new action-name strings.
- The running executable is byte-identical to the inspected image executable.
- Database updater disabled in image and running-container metadata.
- All pre-existing runtime config files, both `.env` files, both Compose files and root flakes
  matched their fresh backups byte-for-byte. Unrelated root `flake.lock` remains intent-to-add.
- Authserver/database container IDs, images, start times and restart counts unchanged. Pi PID,
  active timestamp and state unchanged; `/api/tags` healthy, without generating an AI request.
- Chatter loaded **245 persistent personas**, with area backfill completing at **zero writes**.
  Existing invalid-skill startup warnings remain; this is not an error-free-log claim.

**No live pull was performed.** Successful compilation, registration/binary evidence, startup and
unit tests do not prove real combat scheduling, pathfinding, healing throughput or tank survival.
The user's next legitimate Twins attempt is the live validation.

## Backup and rollback

Private backup: `backups/twins-coordination-predeploy-20260911-224727/`.
Pointer: `/tmp/twins-predeploy-backup`.

Contains full four-database dumps before building and after clean shutdown; the previous image
archive; actual build source/checksums; configs/envs/Compose; repository heads/status/diff; test,
style, build and deployment logs; before/stopped/after save/bind/respawn records; image/running
binaries and disassembly; container/Pi metadata; and copied deployment scripts. Gzip integrity,
SQL dump completion and `SHA256SUMS`, `STOPPED-SHA256SUMS`, `DEPLOYMENT-SHA256SUMS` were verified.

Rollback tag: `acore/ac-wotlk-worldserver:pre-twins-20260911-224727`.
Rollback image:
`sha256:d81a7bbed1a7d0517e79618dd152d763baf8bea1c9de57d2e1d2cbe5553cbc20`.

A future rollback needs permission and fresh backups. There is no schema change: reverting the
binary does **not** inherently require restoring an old DB or losing subsequent raid progress.
