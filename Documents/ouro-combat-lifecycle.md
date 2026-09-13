# Ouro combat lifecycle

**Deployed September 13, 2026**, after independent corrected review, native build, fresh backups
and confirmed human logout. [Deployment and preservation evidence](ouro-combat-deployment-20260913.md).
Only worldserver was replaced; no encounter reset or gameplay-state restoration occurred.
The implementation and build stages below were isolated from deployment and did not use ongoing
gameplay as a test. Real combat behavior remains for ordinary encounter validation.

## Cause (source-supported, not a captured live trace)

Ouro's melee-only `CanAIAttack` was also used by `ThreatReference::ShouldBeOffline` through
`Creature::CanCreatureAttack`. Ranged players became offline threat, not merely ineligible melee
victims. Online transitions initiate engagement, so all-ranged pulls could leave Emerge/events
uninitialized. First melee engagement casts native Ground Rupture (26100, including knockback);
losing all melee could then make `IsThreatListEmpty()` true and cause FAIL/despawn. This fits the
symptoms but does not establish the exact live sequence.

## Correction

Incremental patch: `patches/0032-core-aq40-ouro-combat-lifecycle.patch`.
Only production file: `azerothcore-wotlk/src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_ouro.cpp`.

- Remove the range-only attack hook. Valid ranged players retain native threat/engagement and
  Sand Blast eligibility. Native visibility/attackability checks still apply.
- Preserve native no-target/evade handling, then choose eligible melee locally for the stationary
  boss; use the native ranged victim only when melee is absent. Keep 110% melee hysteresis,
  availability/taunt/detaunt order and eligible melee fixates. Native `TauntUpdate` assigns distinct
  increasing priorities per caster; public `CompareThreatLessThan` retains those priorities even
  though public `GetTauntState` collapses them.
- Revalidate melee candidates; retain no target/reference pointers across ticks. Avoid repeated
  `AttackStart` for an unchanged victim. Preserve unengaged/dead/charmed/passive guards and stop
  scheduled work after no-target handling. Scan eligible threat for melee presence, retaining the
  historical pet/guardian eligibility.

### Accepted P1: real Sand Blast cone direction

Independent review correctly found that the first fixture treated target GUID as AoE damage
selection. Native `SetTarget` changes GUID only; `DoCastAOE` casts with a null explicit target.
Offline Spell.dbc decode for 26102 shows damage and stun both use **54, caster-relative enemy
cone**, no explicit target flags, cast-time index 5. `Spell::InitExplicitTargets` removes unnecessary
unit targets, so replacing this with `DoCast(target)` would not reliably create spell focus.

The corrected callback explicitly turns toward the highest-threat player, sends the facing update,
and snapshots that orientation. During an actual current generic Sand Blast, the script restores
that orientation after **both** native `SelectVictim` and local melee-selection side effects; genuine
victim invalidation and wipe handling still run. No retargeting or stored player pointer is needed.
A missing/different/finished current spell clears the snapshot on the next AI tick, as do Reset and
no-target handling. This handles completion, interruption and repeated casts without a new timer
or generic casting framework. Normal melee facing still respects native spell focus.

**Intentional mechanic-block changes:** Sand Blast cast-start facing/snapshot in Emerge, and
snapshot clearing in Reset. Emerge and Reset are no longer claimed byte-identical. Tests remove
only these exact additions before comparison with the original blocks. Spell effects, damage,
stun, threat-removal callback, 20-second schedule and three-second target-GUID restoration are
unchanged. Rooting/movement, Ground Rupture, Sweep, other timers, submerge/enrage, mounds and
health transfer remain unchanged. Instance recovery and DONE protection from 0030 are untouched.

## Evidence and reproduction

Root baseline: `c890a15abfc8be2af5cc3f8d8e21082c459e1987`.
Backup: `backups/ouro-combat-preparation-20260913-163128/`.
Original actual boss: `actual/core/src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_ouro.cpp`
under that backup. `p1-before/` retains the reviewed implementation, tests, patch, report, statuses
and offline spell decode. The original baseline is not replaced. No reset/worktree/pinned-source
regeneration was used; 0032 still applies incrementally to that original actual baseline.

```sh
PYTHONPATH=scripts/tests python3 -m unittest \
  test_ouro_combat test_ouro_recovery test_bug_trio_reset test_cthun_positioning -v
```

**15 tests passed.** The five Ouro tests compile actual production classes, selected native threat
and lifecycle methods and the actual native TaskScheduler with warnings-as-errors, checked STL
and UBSan. The corrected fixture separates target GUID, orientation and effect-time cone checks;
negative untargeted casts acquire no focus. It covers different melee/ranged bearings across AI
ticks, completion/interruption, repeated casts, active victim/target invalidation, wipe, Reset,
unchanged Sweep and 105%/110%/>110% threat switching. An initial-turn-only mutant fails the
intervening-tick cone assertion; the reviewed pre-P1 implementation also fails corrected targeting.
Original baseline engagement/knockback failures remain reproduced. Existing tests retain coverage
of native recovery, DONE protection, Bug Trio and C'Thun positioning.

Actual-baseline patch apply/reverse byte comparisons and scoped native C++ style pass. Logs and
final hashes are in the backup (`p1-focused-tests.log`, `p1-red.log`, `p1-codestyle.log`,
`p1-preservation.log`, `final-sha256.txt`). Unrelated flake.lock intent-to-add, instance recovery,
and modified/untracked playerbot files are preserved; no staged diff in any repository.
Full pinned replay is not claimed: unrelated 0021 failure remains; focused recovery replay passes.

## Limits

Independent corrected review and native image compilation passed as separate gates. API doubles do not
prove real combat, packet-visible facing, pathfinding, knockback physics or recovery. Cone checks
are simplified geometry and the fixture uses a synthetic preparation interval, not a claim about
native cast-time execution. No live DB/spell overrides or encounter were queried in this stage.
The earlier report's GUID-based Sand Blast assertion was insufficient and is superseded here.

## Verified BUILD-ONLY evidence — 2026-09-13

Corrected independent review: **OK with notes; no issues found**. Parent accepted exact source
SHA256 `7b7e7ba51d9330be9e4efb9e0eb7ad2652685621b9c92a3fb46117a8a859ab11` and 0032 patch
SHA256 `7909d7d38dd3c82b7a4f6f5bded7d00478cd876a110f83dcba8db9ac631d3364` before building.

**Native image build succeeded.** This stage was build-only; deployment occurred afterward.
Actual existing sources (including unrelated
modified/untracked module inputs) were used, without regeneration, config changes or service
interruption. Dockerfile `worldserver` target, host networking, UID/GID 1000/acore, Ubuntu base
`24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254`.

- Staged tag: `acore/ac-wotlk-worldserver:ouro-combat-20260913-150729`
- Image: `sha256:614fb71e9d604376ac422e7de11f1afa59164a0c55636d230041ed1e8410410d`
- Binary SHA256: `8aac434b627e0ec7454141a48098b0d651d444d2b9482245f4141ce8de95b4d4`
- Evidence directory: `backups/ouro-combat-build-20260913-150729/`
- Actual build archive: `actual-build-inputs.tar.gz`; manifest: `build-source-manifest-before.json`.
- Runtime baseline: `runtime-config-baseline.tar.gz`; manifest: `runtime-config-manifest-before.json`.
  Archive paths are relative to the project root. These private archives include configuration
  secrets and Git metadata; do not publish them.

All **15 focused tests passed** again (51.657 seconds). Build compiled `boss_ouro.cpp` and linked
worldserver successfully; unrelated module warnings remain in `docker-build.log`. Binary was
copied from a uniquely named, network-none, **never-started** inspection container, then only that
container and its anonymous volume were removed. Ouro victim-selection, AI tick, engagement,
reset/evade, Sand Blast hit and original-spawner recovery symbols were verified; fixed methods
were disassembled. Staged image and live container retain `AC_UPDATES_ENABLE_DATABASES=0`;
this native environment override supersedes the unchanged host config value of 7.

Archive verification matched all 4,664 build-input files and 33 config files. Before/prebuild/
afterbuild/final manifests match exactly, including untracked/ignored inputs, env, Compose and
flakes. World/auth/DB IDs, image IDs, PIDs, start times, restart counts and running/OOM states
are unchanged; Pi identity/state is unchanged. `:master` remains
`sha256:c34291ccba26a4431096bdf1ce81e0002cf923e65fb1b5af5aafed11b67a5b4b`.
Live worldserver remains `eb5d589b6ff748b33eb6dc11ffbfaec905a7a17ae523bf5a378d026f3b3a8a86`,
PID 2740360, started `2026-09-13T12:45:11.049496517Z`, restart count 0. No staged diffs in
root/core/playerbots; root intent-to-add flake.lock preserved. About 100 GiB remains available;
no unrelated assets were pruned. Logs: `focused-tests.log`, `docker-build.log`,
`image-inspection.log`, `archive-verification.txt`, `final-verification.log`.

No DB access, live encounter commands, second worldserver, deployment, restart, master retag,
commit or push occurred in this build stage. Native compilation and offline tests still do not
prove real combat/pathfinding/packet-visible facing. Deployment, fresh deployment backups and
ordinary human gameplay validation remain separate parent-coordinated gates.
