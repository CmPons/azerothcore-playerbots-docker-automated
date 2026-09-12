# Twins strategy learning: deployment — September 12, 2026

## Outcome

The user reported a legitimate Twin Emperors kill **before** these strategy changes were deployed,
then approved deployment while taking a break before Ouro. At 21:09 CEST, the still-running previous
image had zero restarts and AQ instance 5670 already had completion mask 127 and Twins state DONE.
This update learns from the attempts; it is not credited with producing that kill.

Both 0028 and 0029 are now deployed:

- Meliah stays with Ari's physical station.
- Keilmere stays with Redshift's physical station.
- Ailina supports the actual caster victim, with Beliona as the no-victim fallback.
- Any eligible bot holding caster aggro can seek local healer coverage using bounded, path-checked
  movement rather than assuming Beliona will promptly recover threat.

See [strategy details and limitations](twins-caster-victim-coverage.md). No boss stats, mechanics,
threat injection, reset command, manual SQL write, gear provisioning or Ouro behavior was changed.

## Build and runtime evidence

- Source commits: `dee31d6` (station foundation), `f555f4d` and `4c0882e` (victim coverage/safety).
- Selected regression suite: **90 tests, 89 passed, one optional MySQL test skipped**.
- Scoped production C++ style, added-line checks, patch round-trip and focused pinned replay passed.
  The pre-existing unrelated 0021 full-replay limitation remains; no live reset-to-pins was used.
- Fresh source manifest matched the previous deployed variant image except for exactly **five**
  playerbot coordination/healing files. No source additions/removals or unrelated changes.
- Native worldserver image build passed all **1,888 Ninja steps**, using the preserved actual tree.
- Image inspected from a **never-started container**: all 18 expected symbols present, including
  actual-victim lookup/routing; database updater disabled. No second worldserver ran.
- Navigation prerequisite checked: `MoveMaps.Enable = 1` and the AQ navigation-map file is present.
- New image: `sha256:0f1b3651ad1cf1ab26f043b27c7c4db81d26107931e9de010e74b2c52c6de967`.
- Tags: `acore/ac-wotlk-worldserver:twins-learning-20260912` and `:master`.
- Image/running binary SHA256: `c6ab7851858f9556b3ceda471ba6c3eb5bc703b549d8ab5d158277ee5560ce38`.
- Started **2026-09-12T19:20:17.186767672Z** (21:20 CEST); initialization took 19 seconds.
- Final state running/ready, restart count 0, OOM false, `AC_UPDATES_ENABLE_DATABASES=0`.

Offline doubles, native compilation and binary checks do not establish live pathfinding, latency,
healing throughput or future pull success. Existing unrelated startup/compiler warnings are not
claimed fixed. These Twins-only changes await the next ordinary encounter for live validation.

## Preservation checks

The non-random-player logout gate passed before interruption. Only worldserver was stopped and
recreated; shutdown was clean, exit 0. Full four-database dumps were verified before the build and
again after the clean stop. **The final stopped snapshot**, not an older attempt/reset backup, was
the reference for startup comparisons.

Exact comparisons passed for:

- All managed saved encounters, completion masks, progression stages, deadlines and character binds.
- All saved instances and AQ creature respawn records.
- The ten-character roster's inventory/bank placements and all owned `item_instance` rows, including
  stored item identities, counts, enchantments and durability.
- Runtime configs, server/root env files, Compose files and root flakes.
- Authserver/database container identities and Pi service identity; none was restarted.
- Built versus running worldserver binaries, and source manifests before/after build and restart.

Preserved AQ state: instance 5670, mask **127**, data `A Q T 5 3 3 3 3 3 3 3 5 0 `, stage 2,
reset deadline **1789397072**, extended deadline **1789656272**, with all ten permanent binds.
No encounter reset was requested or executed. Pi health was checked with `/api/tags`, not generation.

## Evidence and recovery assets

Backup: `backups/twins-learning-predeploy-20260912-211344/`.

Includes full source/config archives, four-database dumps, stopped/startup save and roster-item
snapshots, old image archive, both binaries/disassembly, build/test/startup logs and verified
`SHA256SUMS`, `STOPPED-SHA256SUMS`, `DEPLOYMENT-SHA256SUMS` manifests.

Previous image retained as `acore/ac-wotlk-worldserver:pre-twinslearning-20260912-211344`, pointing to
`sha256:569c28cb2d0ed4ecf204fd7d7dd9f4fcdb183cb25911a0706755505802306ba1`.
This is a precautionary recovery asset, **not a rollback action or pending decision**. Do not restore
historical database snapshots over the legitimate kill or any subsequent progress.
