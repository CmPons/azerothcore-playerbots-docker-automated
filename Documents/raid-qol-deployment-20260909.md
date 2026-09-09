# Raid QoL deployment — 2026-09-09

## Result and scope

User explicitly authorized deployment while not playing. The worldserver was
built and restarted successfully; **raid health/damage tuning was not changed**.
Only the worldserver service was stopped/recreated. Authserver, database and Pi
bridge retained their container/process identities and start times.

Activated:

1. [Ambient raid-chat preference](chatter-raid-channel-preference.md), default and
   runtime value **80**. Uses the selected bot's own raid context, with normal
   pacing, admission limits and audience checks.
2. [Combat-only skull targeting](playerbot-skull-combat-only.md). A skull alone
   does not order a precombat pull; genuine assistance and deliberate pulls remain.
3. [Tank-aware hunter pet taunt policy](hunter-pet-taunt-policy.md). Growl/true
   taunts are blocked on targets attacking a living, in-world tank-spec player in
   the hunter's group. Solo behavior and loose-mob/non-tank rescue remain allowed.

No SQL migration, importer, updater, broad setup, encounter tuning, gear edits,
instance-reset commands or manual bind changes were performed. Only the already
prepared chatter setting became effective with the updated binary. The local
`.env` files and runtime chatter config had their stale "prepared" comments
updated afterward; values were unchanged. All pre-existing runtime `.conf` files
were compared against the backup and verified unchanged except that comment.

## Versions

- Source root before deployment record: `ff5ed2a`.
- Core: `112d363423f6a933f8e708eb45951129ffe2a109`.
- Playerbots: `b57c412b` (pet policy), following `9b420fbe` (skull).
- Canonical feature commits: `a35267f`, `0012b86`, `44a031f`.
- Image tag: `acore/ac-wotlk-worldserver:raid-qol-20260909`, also tagged `:master`.
- Image ID:
  `sha256:464d82c98891e5e3bb75ebbdc748aeee8bc5557bbead70c039a1ab1c7818edff`.
- Running executable SHA256:
  `cd5b46ee8821bcb95fd5f120cbbda55b5e54c5277c58448f7b33261049593873`.
- Worldserver container start: **2026-09-09T20:17:35.806944016Z**
  (22:17:35 CEST).
- Readiness observed; world initialization reported 17 seconds. Restart count 0
  at post-deployment verification, no OOM kill.

The image was built from the current full working tree. Pre-existing uncommitted
playerbots work was preserved, not reset or staged; its status matched the prior
progression-deployment record. A complete source archive and checksums capture
what was actually built. Owned chatter source content matched the build-tree
copy. This was not a module/upstream update or a clean-checkout replacement.

Docker's Ubuntu 24.04 base resolved to
`sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254`.
Build log retains compiler/runtime package versions. No `/etc/nixos` or root
flake changes were made; root flake hashes were verified unchanged.

## Verification

- 25 pet/skull/chatter/bridge regression tests passed.
- 7 scaling regressions passed; 8 progression-reset regressions passed, with the
  optional connection-local MySQL test skipped. No migration was needed.
- Prior targeted official style logs were retained; shell syntax/diff checks passed.
- Full host-network Docker worldserver build passed (1,885 build steps in its
  CMake/Ninja phase, compiler-cache reuse enabled).
- The produced executable contains the pet policy registration, chatter config
  key and new attackers-cache `Get()` symbol. Its SHA256 matches the executable
  inside the running container.
- Logs confirm chatter enabled, 113 style examples and 223 persistent personas
  loaded, followed by worldserver readiness. Existing skill-condition and
  Eye of Dar'Khan waypoint warnings remain; this is not a claim of warning-free
  startup. Build warnings included pre-existing missing `override` declarations
  and unrelated module warnings, not build errors.
- Current managed BWL save **3335** remained **8/8 DONE**, stage 3 (cleared), with
  reset deadline `1789012800` and extension deadline `1789099200` unchanged.
  All ten managed character binds matched exactly across shutdown/startup.
- Runtime config verified raid preference 80, raid target 10 and damage exponent
  0.6. Existing raid-scaling settings were byte-compared against the backup.
- Auth/DB identities and starts matched; Pi PID/start matched; `/api/tags`
  responded without issuing a generation request.

**In-game behavior remains to be observed.** Readiness, binary markers and offline
regressions do not prove actual raid-chat distribution, pet cast decisions or
skull behavior in every encounter. No characters were logged in or moved by the
assistant to manufacture a gameplay test.

## Backups and rollback

Private backup directory:

`backups/raid-qol-predeploy-20260909-221212/`

Pointer: `/tmp/raid-qol-predeploy-backup`.

Contains:

- Four-database dump before building and another after clean worldserver shutdown
  (auth, characters, world, playerbots), gzip and dump-completion verified.
- Saved rollback image archive/tag, runtime config archive, both envs and Compose
  files; full build-source archive/checksums and uncommitted playerbots diff.
- Save/bind snapshots, before/stopped/after container metadata, bridge state,
  regression/style/build logs and binary verification.
- `SHA256SUMS`, `STOPPED-SHA256SUMS`, `DEPLOYMENT-SHA256SUMS` and verification logs.

Rollback image:

- Tag: `acore/ac-wotlk-worldserver:pre-raid-qol-20260909-221212`.
- ID: `sha256:6d8f83feb682615edbe238eafb0c47409125faf459d8f8e5c8aa71ff5377bee5`.

Future rollback/restart requires permission. These three changes do not change the
DB schema, so a binary rollback does **not** inherently require restoring an old
database dump and discarding subsequent progress. Retag the rollback image as
`:master` and recreate only the worldserver when authorized; leave auth/DB/bridge
running. Preserve fresh backups before any subsequent deployment.
