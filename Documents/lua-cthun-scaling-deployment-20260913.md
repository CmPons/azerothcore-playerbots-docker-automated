# Lua/C'Thun and raid-scaling deployment — September 13, 2026

## Status

**Deployed and preservation-verified.** User explicitly confirmed logout before deployment.
Only worldserver was cleanly stopped/recreated. General raid scaling is live. Lua support is installed,
but **initial policy activation awaits a freshly observed C'Thun instance scope**. The revised native
entry/spread backend remains the fallback. No live encounter-success claim is made.

Source publications: `3baf46b` (general scaling), `e4e99d2` (Lua/C'Thun incremental patch0033).
Both retained reviews accepted their corrected findings. Builder independently reran40 Lua/affected
and38 scaling tests; suites overlap. The pinned existing Dockerfile's native worldserver build passed.

## Deployed artifact

- Image tag: `acore/ac-wotlk-worldserver:lua-cthun-scaling-20260913-210401`, also `:master`.
- Image ID: `sha256:580dbe7bb0daa724a249d913b54eaee9247804bb92dc0e4157905c7bea319724`.
- Worldserver SHA256: `aa7f0977ebd39e6688461f3ffe63316ecbab967bf048dd1f3eeb11367684c994`.
- Started: `2026-09-13T19:25:06.208589356Z` (21:25 CEST).
- Ready, running, restart count0, no OOM; database updater remains disabled.
- Running binary equals the binary extracted from the builder's never-started inspection container.
- Source delta against previous deployed Ouro image: exactly83 accepted non-git Lua/scaling paths.
  Ouro lifecycle/recovery, mound count/speed/timing and other boss mechanics remain unchanged.

Private build evidence: `backups/lua-cthun-scaling-build-20260913-210401/`.
Private fresh deployment backup: `backups/lua-cthun-scaling-predeploy-20260913-212214/`.
These contain secrets and must not be published.

Rollback image retained as `acore/ac-wotlk-worldserver:pre-lua-cthun-scaling-20260913-212214`:
`sha256:614fb71e9d604376ac422e7de11f1afa59164a0c55636d230041ed1e8410410d`.
Rollback assets are not authorization to rewind gameplay or restart again.

## Preservation

Fresh actual-source/config/rollback-image/four-database archives verified before interruption.
Non-random characters online:0; no saved AQ encounter IN_PROGRESS. Worldserver stopped with exit0,
then a final four-database dump and stopped-state snapshots were taken and verified.

Deployment checks and a separate verification script confirmed:

- All managed save data, completion states, progression deadlines and binds match stopped state exactly.
- All saved instances/binds and AQ creature respawn rows match; no recovery exception was needed.
- Ten-character inventory and owned item rows match, including durability/enchantments/counts.
- Authserver, database and Pi bridge identities/start times are unchanged. Pi health used tags only,
  never generation.
- Environment files, runtime server/module configs and root flakes are unchanged.
- The only Compose change is two worldserver-only policy/status directory mounts. No setup, updater,
  importer, reset, historical SQL restore, gear normalization or arbitrary respawn was run.

AQ instance5670 remains mask127, stage2, save `A Q T 5 3 3 3 3 3 3 3 2 0`:
seven legitimate kills, Ouro FAIL, C'Thun unfinished. Reset1789397072 remains September14 at16:44:32
CEST; extended deadline1789656272 is unchanged. No early reset or bind extension was performed.

## Initial Lua installation

Persistent `setup.sh` template and installed Compose override now mount whole directories:

- `runtime/playerbot-policies` -> `/opt/playerbot-policies`, read-only.
- `runtime/playerbot-policy-status` -> `/opt/playerbot-policy-status`, writable.

Restricted ACLs account for rootless Docker's acore UID1000 mapping to host UID100999. Policy read,
status write and administrator status read/removal were tested through the actual running container.
No additional service/daemon or global Lua VM was introduced.

Pinned-runtime offline checker installed at `runtime/playerbot-policy-checker/policy-runtime-test`,
SHA256 `57ce3417a9647a714b4bb8525b30843e42c9ebf17dd1bbc0fd3dcfe8814caae6`.
Checked immutable initial policy revision:
`51636f27d6645bf0bab1c37c8bb74a30c8fc4688da2e1dc05ea66ff502475b03`.

At verification there was no current scope/status file: the human was logged out. This is **not**
proof of Lua activation. Once a current `531-INSTANCE-GENERATION` status appears near C'Thun, publish
with expected revision `native` using the [operator guide](playerbot-lua-policies.md). Publication
and adoption must then be verified. New map generations require explicit initial publication;
old-generation requests are deliberately not reused. Subsequent supported Lua edits/reverts activate
between pulls without rebuild, logout, travel or reset. Live adoption/navigation remains untested.

## Evidence index

Deployment backup includes `SHA256SUMS`, `STOPPED-SHA256SUMS`, `DEPLOYMENT-SHA256SUMS`,
`databases-after-worldserver-stop.sql.gz`, save/item before-after TSVs, `binaries.sha256`,
`worldserver-final.json`, `policy-mounts.json`, `policy-container-access.txt`, directory/file ACLs,
original/expected Compose snapshots, startup/shutdown logs and the exact deployment scripts.

Offline fixtures and native compilation do not establish live movement latency, healing throughput,
navmesh outcomes or encounter difficulty. Ouro-specific bot avoidance/combat-follow remains unresolved;
the user chose to try damage scaling with mound count unchanged. Known unrelated0021 full pinned
patch replay failure and unsafe generic creature-respawn command remain outside this deployment.
