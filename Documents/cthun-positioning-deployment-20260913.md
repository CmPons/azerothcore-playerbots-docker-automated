# C'Thun positioning + Ouro recovery deployment — September 13, 2026

**Deployed and verified.** User explicitly authorized fixing and deploying bot spacing and Dark Glare
avoidance. The combined image also includes the already-prepared Ouro recovery. Only worldserver
was interrupted, after the human logout and no-active-AQ-encounter checks passed.

## Changes

- C'Thun-specific pre-pull/combat spacing, yielding around clumps, short checked movement and
  predictive red-sweep avoidance: [cthun-positioning.md](cthun-positioning.md), patch 0031.
- Original Ouro mound recovery through native per-instance respawn scheduling: patch 0030,
  [ouro-spawner-recovery.md](ouro-spawner-recovery.md).
- **No C'Thun damage, health, spell, add-count or raid-scaling change.** No inventory/gear actions,
  migrations, database restoration, reset command, broad respawn or historical save restoration.

Published source revisions: `f62e441` and final corridor-combat guard `c46c4f7`.
Exactly five production files differ from the previous live Twins-learning image: the AQ instance
recovery script and four playerbot AQ files (two new). No removed or unrelated changed build inputs.

## Validation and image

- Final selected regression run: **97 tests, 96 passed, one optional MySQL fixture skipped**, 61.254s.
- Scoped production C++ style / added-line checks, incremental roundtrip, focused pinned replay
  and retained Twins regressions passed. Full setup replay's pre-existing 0021 issue remains; setup
  was not run.
- Native build: **1,889 Ninja steps**, source-stability checks passed.
- Inspected from a **never-started container**: seven C'Thun symbols/actions, retained feature
  symbols, Ouro lifecycle/recovery callback and disabled database updater. No second worldserver ran.

```text
Image: acore/ac-wotlk-worldserver:cthun-positioning-20260913 and :master
ID: sha256:c34291ccba26a4431096bdf1ce81e0002cf923e65fb1b5af5aafed11b67a5b4b
worldserver SHA256: b14e5786ace4d16a2e98fa12f1e7a87ff4ae3b2b867c250717cd6735d34c51c2
Started: 2026-09-13T12:45:11.049496517Z (14:45 CEST)
Ready: confirmed; running, restart count 0, OOM false
AC_UPDATES_ENABLE_DATABASES=0
```

The earlier build `b8193d74…` was inspected but **never deployed**; the final build above additionally
prevents pre-pull spacing from taking over unrelated corridor trash combat.

## Preservation

Fresh preparation backup: `backups/cthun-positioning-preparation-20260913-134114/`.
Final predeployment backup: **`backups/cthun-positioning-predeploy-20260913-144122/`**, including actual
source/config/image archives, live four-database dump, latest **cleanly stopped** four-database dump,
save and item snapshots, test/build/inspection/deployment logs, scripts and checksums.
Earlier build backup: `backups/cthun-positioning-predeploy-20260913-143404/` (not deployed).

Compared against the final stopped save, not an older seven-kill snapshot:

- All saved instances, completion states/masks, permanent binds, progression stages and deadlines.
- All ten roster characters' `character_inventory` rows and owned `item_instance` rows, including
  counts, enchantments and durability.
- All AQ creature respawn rows: **identical at verification**. The native Ouro map-load recovery
  had not yet been observed; deployment does not prove the mound has already appeared in-game.
- Runtime configs/env/Compose/root flakes and the actual source manifest.
- Image binary versus the running container's binary, byte-for-byte.
- Auth/database container identities and Pi service identity unchanged. Pi `/api/tags` healthy,
  without generating a request.

AQ state at verification:

```text
instance5670 map531 difficulty0 completedEncounters127
A Q T 5 3 3 3 3 3 3 3 2 0
stage2 resetTime1789397072 extendedResetTime1789656272
```

Seven legitimate kills remain recorded, with Ouro FAIL and C'Thun not completed. No encounter
state reset was performed. The previous live image is preserved as the precautionary asset
`acore/ac-wotlk-worldserver:pre-cthun-20260913-144122`; it was not reactivated.

Manifests: `SHA256SUMS`, `STOPPED-SHA256SUMS`, `DEPLOYMENT-SHA256SUMS`.
Scripts: `/tmp/{cthun-preflight,build-cthun,inspect-cthun-image,deploy-cthun,verify-cthun-deployment}.sh`.
The save comparator permits only native recovery of the original failed/not-started Ouro timer;
seven offline comparator cases checked that unrelated saves, completed Ouro and other respawn
changes are rejected. In this deployment even the Ouro timer remained unchanged at verification.

## Live validation still needed

The user can log back in. Give bots time to separate on approach before committing the pull.
No live bot commands or test pulls were issued. Compilation, startup and straight-line API-double
scenarios do not prove real corridor routing, cast latency, healing uptime or a clean encounter.
This is the requested first spacing/sweep iteration, not complete tentacle/stomach coordination.
