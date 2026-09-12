# Playerbot durability display

## Status

**Deployed September 12, 2026**, with explicit user permission. Only worldserver was rebuilt and
recreated; raid progress and configuration were preserved. This deployment permission is consumed;
future interruptions require fresh authorization.

## Display-only change

`StatsAction::ListRepairCost` previously averaged only damaged items, including items in the
backpack. Fully repaired gear left the count at zero, producing a division-by-zero/invalid
integer-conversion path instead of a reliable 100% display.

The percentage now:

- Averages **all equipped items with durability**, including fully repaired items.
- Excludes empty slots, non-durable items (e.g. rings), bags and backpack equipment.
- Displays 100% if there is no repairable equipped gear, without dividing by zero.
- Retains the existing upward rounding and color thresholds. Each eligible item has equal
  weight, rather than weighting by its maximum durability points. As before, rounding can
  display 100% for a fractional average just below 100%.

Examples: full + half-worn = 75%; full + broken = 50%; all durable equipped items broken = 0%.
A broken spare weapon in the backpack cannot make a fully repaired equipped set display 0%.

The parenthesized **repair-cost estimate keeps its previous scope and calculation** (equipment
and main backpack). This change does not repair items, spend money, modify gear stats, change
summon/release repairs, or fix the separate missing vendor-repair confirmation. No boss AI,
Twins coordination, scaling, loot, configuration or database changes.

## Source and tests

- Production: `azerothcore-wotlk/modules/mod-playerbots/src/Ai/Base/Actions/StatsAction.cpp`.
- Canonical patch: `patches/0027-playerbot-durability-display.patch`.
- Tests: `scripts/tests/test_durability_display.py` and `cpp/DurabilityDisplayTest.cpp`.

Four offline tests compile the actual display/percentage methods against API doubles with
undefined-behavior, floating divide-by-zero and float-to-integer overflow sanitizers. Cover
full/empty/non-durable gear, mixed/broken gear, unequal maximum durability, rounding/colors,
backpack exclusion, unchanged cost-estimate visits and read-only rendering. Patch round-trip
checks prove that only the display method changed; the cost and per-item helpers are unchanged.
These tests are not a live bot-command test; the full native build also passed as recorded below.

Pre-edit source/status/diff/hash backup:
`backups/durability-display-preparation-20260912-114056/`.
Unrelated module work and root `flake.lock` are preserved.

## Deployment verification — September 12

- Root source commit `97d5dff`; playerbots `2de8f55e6415ab1b721866d1558740f85585b9e5` plus the
  preserved pre-existing working tree. Core unchanged at `5a6f664eec08cecaf1ab4ede235c9db532071647`.
- Full `docker build --network=host` completed all **1,887 Ninja steps**. Used the existing
  Dockerfile and full working source, with the same pinned Ubuntu 24.04 image digest as the Twins
  deployment. No setup/update script or regeneration from pins.
- Input comparison against the deployed Twins build: **only `StatsAction.cpp` changed**, no added
  or removed source files. Source checksums matched before/after build and deployment.
- **67 tests passed; one optional connection-local MySQL test skipped**, including the four new
  sanitizer tests and the existing Twins/AQ40, pet, skull, chatter, bridge, scaling/reset and
  statistical regression suites. Scoped C++ style and 120-column checks passed.
- Image tags `acore/ac-wotlk-worldserver:durability-display-20260912` and `:master`.
- Image ID:
  `sha256:540f4822b42a5aab6e504691db6cae0c3b87b56f73f9016a5632eae253162217`.
- Image/running executable SHA256:
  `d4d4e2fbe77c8e43810c02efbcb71a77757ec443c919e2e84f215f2a6914e13e`.
- Image inspected using a **never-started container**, not a second worldserver. Disassembly
  confirms the equipped-slot gate and guarded average with a 100.0 fallback. Running binary is
  byte-identical to that inspected image. Existing Twins symbols/action names remain present.
- Worldserver stopped cleanly: exit 0, no OOM. New start **2026-09-12T09:48:09.931639162Z**
  (11:48:09 CEST); native readiness after **17 seconds** initialization, restart count zero.
- Database updater disabled. No importer, manual SQL, boss reset, bind change, gear edit or
  database restore. Normal shutdown/startup still makes its ordinary database writes.
- Against the final stopped-server snapshot, all saved instances, binds and AQ40 respawns matched
  exactly after startup. AQ40 **5670** retains **six kills**, completion mask **63**, ten permanent
  binds (`extended=0`), stage **2**, reset **1789397072**, extended deadline **1789656272**.
- Authserver/database identities, image IDs, start times and restart counts unchanged. Pi PID,
  start/state unchanged; `/api/tags` healthy without generating an AI request. Both `.env` files,
  Compose files, existing runtime configs and root flakes matched the fresh backup byte-for-byte.
- Chatter loaded 245 persistent personas; area backfill wrote zero rows. Existing invalid-skill
  startup messages and unrelated compiler warnings remain; not a warning-free claim.
- No synthetic player login or live `stats`/`repair` command was issued for verification.

Private deployment backup: `backups/durability-display-predeploy-20260912-114427/`;
pointer `/tmp/durability-predeploy-backup`. Contains fresh four-database dumps before build and
following shutdown; rollback image archive; full actual source/checksums; env/config/Compose;
repository state, logs/tests/style, saved-instance/respawn comparisons, binaries/disassembly and
before/after container/Pi metadata. Gzip integrity, SQL completion markers, `SHA256SUMS`,
`STOPPED-SHA256SUMS` and `DEPLOYMENT-SHA256SUMS` verified.

Rollback tag: `acore/ac-wotlk-worldserver:pre-durability-20260912-114427`.
Rollback image:
`sha256:c8b13b52ce0f94e6f8f24413178c1e3546bb25fa9ed09b99d39c697d4beb1eef`.
A future rollback requires permission/fresh backups. There is no schema change, so restoring an
old database is not necessary for a binary rollback and would risk losing newer progress.
