# Ouro combat deployment — September 13, 2026

The user explicitly authorized deployment and confirmed logout. Independent corrected review
found no issues; 15 focused tests passed twice and the native worldserver build succeeded.
Source commit: `42d86b2` (patch 0032, tests and lifecycle documentation), published to fork/main.

## Deployed artifact

- Tag: `acore/ac-wotlk-worldserver:ouro-combat-20260913-150729` (also `:master`).
- Image: `sha256:614fb71e9d604376ac422e7de11f1afa59164a0c55636d230041ed1e8410410d`.
- Running binary SHA256: `8aac434b627e0ec7454141a48098b0d651d444d2b9482245f4141ce8de95b4d4`.
- Started: `2026-09-13T15:17:10.604441933Z`; ready, running, restart count 0, no OOM.
- Native database-updater environment override remains `AC_UPDATES_ENABLE_DATABASES=0`.
- Exactly `boss_ouro.cpp` differs from the previous live C'Thun image's build-source manifest.
  Existing original-spawner recovery, C'Thun positioning and all unrelated source inputs remain.

## Fresh backup and verification

Private backup: `backups/ouro-combat-predeploy-20260913-171557/`.
Build evidence: `backups/ouro-combat-build-20260913-150729/`.

Backed up actual source, runtime configs/env/Compose, four databases, previous live image and
repository identities. Verified archives/checksums and the staged image's independently extracted
binary. Confirmed zero online non-random-account characters and no saved active AQ encounter.

Stopped **only worldserver** cleanly (exit 0, no OOM), then took and verified a final four-database
dump and saved-instance/bind/deadline/roster-item snapshots. Recreated only worldserver with the
accepted image. Startup and a separate verification script both passed.

- All saved instances, completion masks, permanent/extended binds and deadlines matched exactly.
- AQ5670 retained mask 127 and `A Q T 5 3 3 3 3 3 3 3 2 0`: seven legitimate kills,
  Ouro failed, C'Thun unfinished. AQ respawn rows were identical at verification.
- Reset remained `1789397072` (September 14, 16:44:32 CEST); stored extended deadline
  `1789656272` remained unchanged. No early reset or bind extension was requested/performed.
- All ten roster characters' inventory and owned item-instance rows matched the cleanly stopped
  state, including durability/enchantments/counts.
- Running binary matched the never-started inspection container's binary byte-for-byte.
- Runtime config files, root/server env, Compose and flakes were unchanged.
- Auth/database container identities and Pi service identity were unchanged; Pi tags health check
  used no generation request.

Evidence includes `STOPPED-SHA256SUMS`, final database dump, `shutdown.log`, `startup.log`,
`managed-saves-after-{stop,start}.tsv`, `roster-items-after-{stop,start}.tsv`,
`saves-and-aq-respawns-after-{stop,start}.tsv`, `ouro-startup-respawn-comparison.txt`, container
snapshots, `running-binary.sha256` and deployment/verification scripts and logs in the backup.
Backups contain secrets and gameplay data and must not be published.

## Limits and rollback

This establishes build/startup and preservation, not successful real Ouro combat, packet-visible
cone facing, knockback physics or map-load recovery. No live pull, bot command, gear manipulation,
encounter reset, old database restore, setup or updater was used to test the change.

Previous image `sha256:c34291ccba26a4431096bdf1ce81e0002cf923e65fb1b5af5aafed11b67a5b4b`
is retained under `acore/ac-wotlk-worldserver:pre-ouro-combat-20260913-171557` and archived.
Any future rollback is a separately authorized image/code operation with fresh preservation,
not restoration of old inventories, kills, binds or deadlines.

Next: instance-owned Lua tactical policy integration and C'Thun room-entry/positioning migration.
