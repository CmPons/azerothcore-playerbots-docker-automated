# Raid-combat Lua deployment — September 14, 2026

**Deployed and preservation-verified.** The user authorized deployment; fresh checks found
zero online non-random-bot characters before shutdown. Only worldserver was cleanly
stopped/recreated. API2 and automatic default loading are now installed together.

Source publication: `b54f54a88f5070520c79ab5972b906b792e124d0` (incremental0035),
including accepted automatic loading from0034. Independent review: **OK** after withdrawing
an incorrect requirement to expire already-admitted native casts when their Lua request ages.

## Installed artifacts

- Image tag: `acore/ac-wotlk-worldserver:raid-combat-20260914-113620`, also `:master`.
- Image ID: `sha256:58904b1ba207bf91189916c33a09f5363c2ebed70551211139dbdea05b518062`.
- Worldserver SHA256: `8b75977ebc3922d0cd892e21c1e55e10ea7c05171312228eab1bc73eba817164`.
- Started: `2026-09-14T10:05:50.096998298Z` (12:05:50 CEST); ready, running, restart count0, no OOM.
- Initial Lua source: `raid-policies/aq40/combat.lua`.
- Published immutable revision:
  `3271cd8a08eca9d692df8c491b6a9e1229dfaee922d5d31270dcdbb549ede1fd`.
- Installed host checker: `runtime/playerbot-policy-checker/raid-combat-check`.
- Checker SHA256: `d8b6ba08532da4f8b6dba7d9aa07a59392dd8d4396f5dd6ffb19a1e0d06a21f3`.

The existing policy/status mounts and ACLs are unchanged. Actual container policy reads
and status-write access passed. The checked default is installed; there was no eligible
live raid scope while logged out, so this is **not** a claim of observed live adoption or
encounter success. Eligible raids load the default automatically, with between-pull adoption.
See [the edit/check/publish workflow](raid-combat-lua.md).

## Preservation

Fresh private actual-source/config/policy/rollback-image and four-database backups were
verified before interruption. Shutdown exited0, not OOM. A second four-database dump and
final save/item snapshots were captured and verified with worldserver stopped.

Separate post-start verification confirmed:

- All instance save data, completion masks, binds, progression deadlines and AQ respawns
  exactly match stopped state.
- Inventory and owned item rows for the ten-character roster match, including durability,
  enchantments and counts.
- The running binary equals the inspected staged binary.
- Authserver, database and Pi bridge identities/start times are unchanged. Pi health used
  `/api/tags` only, never generation.
- Runtime configs, both env files, Compose and root flakes are unchanged.
- No setup replay, reset, bind extension, historical database restore or gear normalization.

AQ5670 retains mask127/stage2, seven kills, Ouro FAIL and C'Thun unfinished. Its reset
remains September14,16:44:32 CEST; deployment did not advance or extend it.

**Updater distinction:** the core updater is disabled by `AC_UPDATES_ENABLE_DATABASES=0`.
The pre-existing playerbots updater remains enabled. All28 included SQL files matched the
applied name/SHA1/state ledger before stopping and after startup; no pending, changed or
orphaned update was found. No updater setting was changed. Older blanket descriptions of
“updater disabled” should be read with this distinction.

## Evidence and rollback

Private directories (contain secrets; do not publish):

- `backups/raid-combat-build-20260914-113620/`: pinned build exit0, actual32-path0034/0035
  delta reconstruction, image/binary/checker inspection, source/config/identity preservation,
  archived artifacts and verified `BUILD-SHA256SUMS`.
- `backups/raid-combat-predeploy-20260914-120422/`: fresh/final stopped four-DB dumps,
  source/config/ACL/policy backups, save/item comparisons, deployment scripts/logs,
  installed default and verified checksum manifests.

Rollback image: `acore/ac-wotlk-worldserver:pre-raid-combat-20260914-120422`,
ID `sha256:580dbe7bb0daa724a249d913b54eaee9247804bb92dc0e4157905c7bea319724`.
Its existence is not permission for another restart or gameplay-data rewind.

Build/static/offline tests do not prove live navmesh, reaction latency, healing throughput
or boss success. Initial C'Thun content is tunable; Ouro/Chromaggus Lua policies remain
subsequent authoring work. Deferred vehicle/interaction mechanics are not release gates.
