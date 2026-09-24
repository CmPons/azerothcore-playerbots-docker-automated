# Raid support scaling and AH rollback deployment — September 24, 2026

## Authorization and scope

User: “You can revert the AH boost, and let's fix the healing and shield scaling.
Leave everything else the same. It's late at night so you're free to deploy this
when you're done.”

Completed a worldserver-only deployment. [Support policy and tests](raid-healing-shield-scaling.md)
cover hostile NPC flat healing and finite per-unit absorb amounts following the
recipient's HP factor. At 25→10, Blindeye's 25k shield becomes 10k and Dark Mending
becomes 27,750–32,250 before other combat modifiers/overheal.

No raid HP/damage config changes, Shatter repair, encounter-script changes, tank-mode
changes, roster sync, equipment operation, gem-stock change or historical recovery.

## Published build source and runtime

- Root build source: `412da18bcbfe2258d44f0f8810998294366c2b84`.
- Core: `f6c0fd3cf0d1a3a153bca530cd8efe9efd61b105`.
- Playerbots unchanged: `292669a0b536c3e988a31ec874996d85ee5dc2e2`.
- Other native/module pins unchanged; all maintained fork tips verified.
- Tag: `acore/ac-wotlk-worldserver:raid-support-20260924-222628`.
- Image: `sha256:1a18971a2062be9256ccd295818ca875bc36d40c0a040145843a9b5e9c1674a1`.
- Running binary SHA256: `d988c63afcdb54c4b01f508ad7f64ce575d6d396f30913af1d8ccbbf5ca17a3f`.
- Container: `1230a69db0a25c1d7acfdf5f2f189253c0e4051983ed042cb21433104e71c175`.
- Started: `2026-09-24T20:46:52.448063177Z`.
- Ready: **`2026-09-24T20:47:14.001541768Z`**.

Retained rollback tag `acore/ac-wotlk-worldserver:pre-raid-support-20260924-222628`
points to prior image
`sha256:7ac8ea42fe4f68ecbabb984dbc4dc3cfe6baa5d26bf6de231b1dc2f8dc757d13`.
No rollback was performed. Future service interruptions require fresh authorization.

## Validation and deployment sequence

1. Fresh private four-database and config backups before build; read-only snapshots
   of 43 managed characters, items, abilities, profiles, group/roster state and raids.
2. AH rollback applied separately with exactly six keys changed in each private env
   and ten runtime AH keys. One acknowledged `ahbot reload`, no forced refill/clear.
   Restored 10k targets and armor weights 20/10/3; removed 170 armor multipliers while
   preserving all 303 other multipliers, including the BC gem multiplier of 5.
3. **55 focused tests passed**, including AH, support scaling, existing raid scaling,
   Twins/creature eligibility, tank modes and skull targeting. Five production-header
   syntax checks passed; official C++ style comparison found no new findings.
4. Native image built successfully while the old server remained available. A
   4,206-record manifest identified exactly **nine changed build-source files**:
   four raid-scaling module files and five core hook/integration files. Mirrors
   matched. No unrelated native source changes or pending source were included.
5. Expected support/tank/gem/BG symbols verified, aura/heal dispatch calls confirmed
   in disassembly, and linked dependencies checked. Candidate inspection container
   was created but never started as a server; the dependency check ran only `ldd`.
6. No human players online immediately before the clean stop. Took a second verified
   four-database dump and preservation snapshots after worldserver exited normally.
7. Recreated only worldserver using `--no-deps --no-build --pull never`. No importer,
   setup, database migration, gameplay SQL writes, forced saves or encounter resets.
8. Running binary matched the staged image extraction. No OOM/crash/restart loop
   through the review window extending beyond a normal five-minute character save.

An initial old scaling-default test expected the pre-source-workflow duplicate
module loop in `update.sh`; its source contract was updated, not production setup.
One intermediate test run observed the brief gap while a reviewed module edit was
being synchronized. The stable final mirrored source passed all 55 tests. Neither
intermediate result was counted as a passing validation or deployed candidate.

## Preservation results

Stopped-versus-startup snapshots matched for all monitored datasets. After the
normal-save review window:

- **43 characters:** levels/XP, roster, quests, talents, learned spells and skills
  preserved. All nine core companions online at level 70; Redshift offline.
- **1,993 item identities and inventory placements preserved**. Existing permanent
  enchants and socket gems unchanged; no new empty sockets filled in this deployment.
- Eleven temporary enchant timers advanced/expired normally. No other item-row
  changes were accepted by the audit.
- Saved AI profiles unchanged; **no additional profile loss**. This does not restore
  Raney's historical missing rows from the earlier deployment.
- Saved group state unchanged (no managed group before this stop).
- Raid rows, 30 permanent binds and associated monitored respawn/GO state exact:
  Karazhan 5869/mask727, Gruul 6061/mask3, Mag 6306/mask1.
- Core updater ledgers unchanged. The enabled playerbot updater ledger stayed exact
  and matched the unchanged playerbot SQL source.
- All 48 tracked config/policy records unchanged during build/deploy. Relative to the
  previous deployment, only the two private env files and `mod_ahbot.conf` differ,
  precisely for the authorized AH rollback. Raid and companion settings unchanged.
- Authserver, database and dependency-helper container identities/start times unchanged.
  Pi bridge PID/invocation unchanged; `/api/tags` healthy.

As with any restart, memory-only `.raidscale` overrides do not persist. Configured
ten-player defaults are unchanged; no overrides were issued by this deployment.

## Nonfatal observations and limitations

- Existing process-priority permission warning.
- One duplicate `pet_spell` insert (`5820-7816`), traced to **Keenir, GUID1750,
  level42**, outside the 43 managed characters. No pet data repair attempted.
- Two low-velocity spline rejections for creature entry2974/spawn14044, the same
  movement-warning identity seen before this task. No movement changes attempted.

Offline extraction tests and a successful native deployment do not establish live
encounter acceptance. No test mobs were spawned, spells forced, saved bosses reset,
or player fights instrumented. The next real attempt can verify cast/absorb results.

Private evidence: `backups/raid-support-deploy-20260924-222628/`, especially
`deployment-closeout.json`, `audit-observation.json`, source/config manifests,
`tests-accepted.log`, `syntax-final.log`, `style.log`, build/deploy logs, stopped DB
backup checksums and `DEPLOYMENT-SHA256SUMS`. No credentials, dumps, binaries or
client DBCs are committed.
