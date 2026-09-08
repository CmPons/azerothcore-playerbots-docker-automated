# Persistent raid scaling default (2026-09-07)

## Status

Implemented and tested at the state/setup level; **not built into a worldserver
image or deployed yet**. No service interruption, live command, configuration
reload, boss reset, or bind edit was performed. The current raid's manually
selected scale remains in effect.

## Configuration / workflow

```ini
# Root .env (setup source) and azerothcore-wotlk/.env
RAID_SCALING_DEFAULT_PLAYERS=10

# env/dist/etc/modules/mod_raid_scaling.conf
RaidScaling.DefaultTargetPlayers = 10
```

Both env files and the runtime config are prepared. The compiled module and dist
config also default to 10. A future setup run writes the env value into the module
config. `setup.sh` and `update.sh` now include `mod-raid-scaling` in their canonical
local-module sync lists, so fresh installs/updates do not silently omit this owned module.

- Values 1..40 select a fixed target; 0 disables automatic scaling while leaving
  manual commands available. Invalid numeric targets fall back to 10 with a warning.
- Each newly loaded supported raid initializes once, including new copies after
  daily resets and saved instances reloaded after a worldserver restart.
- The existing original-size table is used (MC/BWL 40, ZG/AQ20 20, etc.). Unknown
  raids are not guessed: log a warning, leave scaling inactive, and report that
  state on player entry. Open world, normal dungeons, and base maps are excluded.
- No live attendance calculation: deaths, disconnects, or late arrivals never
  resize the raid. Egg counts, add schedules, and encounter mechanics are unchanged.
- Boss and eligible trash HP/damage use the same formulas as `.raidscale 10`:
  HP = target/original; default damage = pow(target/original, 0.6), with the existing
  configured clamps/exponent still respected. At 10/40 this is 25% HP and about
  43.5% damage; at 10/20 it is 50% HP and about 66% damage.
- Real players receive one status message on each entry, e.g. `RaidScale: Blackwing
  Lair #123 scaled for 10 players (server default).` Bot sessions do not receive it.
  `.raidscale status` reports the multipliers and default/manual source.

Existing commands still work:

```text
.raidscale 20
.raidscale boss damage 0.6
.raidscale off
.raidscale status
```

**Manual overrides are intentionally per loaded instance**, not new persistent
per-instance database records. Re-entry, newly spawned adds, and player deaths do
not overwrite them. When the instance unloads or worldserver restarts, its next
load uses the server default again. To change the lasting server preference,
change the env/config setting. No new SQL tables or migrations are needed.

## Implementation

Canonical module: `modules/mod-raid-scaling/`; source/conf/tests synchronized into
`azerothcore-wotlk/modules/mod-raid-scaling/`.

- `RaidScalingLoader.cpp`: AllMapScript hooks for creation, destruction, and player
  entry. Entry is notification-only; no polling or mid-fight auto-rescaling.
- `RaidScalingMgr.cpp`: initialize settings at map creation and apply them to
  already-loaded creatures. This core preloads grids **before** calling the map
  creation hook; applying to the loaded spawn store is therefore necessary.
  The existing creature-add hook scales subsequent eligible summons/spawns using
  that instance's current settings, including manual overrides.
- `RaidScalingState.h`: synchronized registry, with value snapshots rather than
  pointers into shared state. This is needed because different maps update on
  different threads. Initialization cannot replace an existing manual/off entry.
- Original creature-health caches are now also partitioned by map/instance and
  locked. Map destruction clears that map's settings/cache, avoiding stale state
  when instances are reloaded or IDs reused. Ordinary non-raid damage avoids the
  registry lock entirely.

## Verification

```bash
python3 -m unittest discover -s scripts/tests -p 'test_raid_scaling_default.py' -v
bash -n setup.sh
```

Seven checks pass, including compiling/running the actual standalone C++ state
registry with C++20, warnings-as-errors and multithreaded tests. Coverage includes
fresh instances, explicit 20/off overrides, snapshot safety, map-ID isolation,
unload/reload, process-restart semantics, concurrent registry mutations, setup
fallback/0/20/40/idempotence, preservation of other config values, and canonical
build-tree synchronization. Lifecycle hook checks are source contracts, not a
live map integration test. Modified module sources pass the official C++ style
checker. No full worldserver build has been performed for this change.

Pre-change env/runtime config copies:
`backups/raid-scaling-default-20260907-204555/` (private, gitignored).
Only the new knob was edited; full setup was not run against the active server.

## Next steps (permission required)

Build the updated image with host networking. Do not restart merely to build it.
A full working-tree image includes other existing module changes, not just this
feature. Obtain explicit permission before replacing the live worldserver.

**2026-09-08 dependency:** the working core now also contains the pending
[conditional raid reset change](raid-progression-resets.md). An image built from
that tree requires its `instance_progression_reset` characters-table migration
before startup, even when only deploying/testing raid scaling. The scaling module
itself still needs no SQL migration.

After deployment, verify in-game:

1. Enter a fresh MC/BWL/ZG copy: entry/status reports default target 10 and expected
   multipliers before a pull. Check initially loaded trash and later summoned adds.
2. Set target 20 or a custom multiplier; add/relog another member and spawn adds:
   settings remain manual, not silently reset to 10.
3. Set `off`; re-enter while the map remains loaded: scaling stays off.
4. Unload/reload the copy or perform an approved server restart: default 10 returns.
5. Deaths and partial raid attendance do not alter settings; non-raids stay unchanged.
6. Confirm raid health/damage and normal boss mechanics in play. This work does not
   claim to fix the separate Razorgore charm/evade lockout or add bot orb control.
