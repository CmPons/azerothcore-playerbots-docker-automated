# TK weapon-recovery deployment — October 8, 2026 (evening)

## Result and scope

**Feelesia's recovery fix is deployed.** The worldserver was ready at
**21:18:39 CEST / 19:18:39 UTC**. Infrastructure checks passed; preservation is
**qualified**, not an unconditional pass (see the reset-metadata, profile and pet
findings below). Ordinary live Kael/command acceptance remains pending.

The user explicitly authorized **build and deploy** after finishing for the night.
Only the five TK playerbot files from
[`tk-weapon-recovery.md`](tk-weapon-recovery.md) changed versus the afternoon image.
No advisor-health fix, encounter tuning, config edit, item grant, gameplay SQL
repair, bot strategy command, forced encounter or second restart was performed.
The user's advisor `.raidscale boss hp 0.4` workaround remains available. Runtime
instance overrides are not persisted through restarts; the unchanged default is
25-to-10 scaling, with health multiplier 0.4.

### Revisions and image

- Root build/source pin commit: `9c789f4`.
- Playerbots: `8d73b1a5721848071cd5e3048c7ad84a8f1c8194`.
- Core unchanged: `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
- Image: `acore/ac-wotlk-worldserver:tk-recovery-20261008-210957`.
- Image ID: `sha256:0526c941b7562e2062323696b92d85612ce8190cc3806e45a75997362f375bd9`.
- Worldserver SHA256: `aaf00b5a914fcc2eb8f4eddf16cce151b702276fc7e970059a84760986ab6ced`.
- Retained rollback tag: `acore/ac-wotlk-worldserver:pre-tk-recovery-20261008-210957`
  (the afternoon image `sha256:8a1e64212d6431957ceb2b6174b72d7aa35283b9008354bd55913cbcfe24af4f`).
- Auth, database, importer, client-data helper and Pi bridge identities/start times
  were unchanged; Pi's model endpoint remained healthy.

## Verification

- Fresh verified four-database dumps before building and after a clean stop.
  Snapshot coverage: 43 managed characters, inventory, gear, abilities, profiles,
  specs, pets and pet spells, all current raid saves/binds/progression/respawns.
- No humans online at preflight, immediately before stopping, or at observation.
- **67 tests passed**, including eight recovery tests and retained dead-loot,
  Leotheras, direct-token, respawn-scaling, whirlwind, tank and pull-guard coverage.
- Three production translation units passed native-header syntax checks.
- Full image build: **21:13:52–21:14:49 CEST**; cache reuse made this build short.
  All 16 compiler warnings were in files unchanged from the deployed afternoon source.
- 4,208 source records and 48 config/policy records matched before/after build,
  before stopping, after startup and after observation. Root-owned module mirrors
  matched. Database updater ledgers were unchanged.
- The image-inspection container was never started. Offline binary inspection
  verified owned-instance candidate valuation, the safe validator, virtual equip
  dispatch followed by checks of all three slots, and five-second monotonic backoff.
  Running-binary hash matched the inspected executable; dependency checks passed.
- Only the worldserver was cleanly stopped and recreated, using `--no-deps
  --no-build --pull never`. No automatic rollback or additional restart occurred.
- Readiness and a **380-second post-start observation** passed, with zero restarts
  or OOM. This exceeds the five-minute character save interval.
- The runtime log contained 80 action-button cleanup warnings, none for managed
  characters, and no duplicate-entry errors. The existing process-priority
  permission and level-cap-related data warnings remained; no repair was attempted.

The initial binary inspector incorrectly expected a direct `DoSpecificAction`
call and a positive 5000 constant. The compiled code uses virtual dispatch and
Clang's unsigned negative-delta comparison. Inspection was corrected to verify the
actual vtable target, post-dispatch slot checks and emitted wrap-aware comparison,
including threshold examples. The original failed inspection logs are retained;
this was an inspection correction, not a bypass or source change.

## Preservation review

### Before build → clean stop (old image still running)

All **2,161 item identities, inventory positions and 717 equipped positions**
remained intact. Existing gems and permanent enchants were unchanged. Nine
weapon temporary-enchant records aged/expired, and Keilmere's Sacred Candle stack
17029 decreased from two to one. These occurred on the old running server and are
consistent with ordinary bot timers/buffing; no precise action trace was captured.
Other monitored snapshots matched exactly.

### Clean stop → immediate startup

All **2,161 items, every item field and inventory position** matched exactly.
Levels/XP, roster, quests, abilities, specs, pets and pet spells matched.

Differences requiring disclosure:

1. **Group 206 disbanded**, removing its ten membership rows and MT assignment.
   This is consistent with `KeepAltsInGroup=0` while the human master is offline;
   the user has clarified that group disbanding itself need not be repaired.
2. **Raney lost 14 saved profile/preferences rows** again. Talents, spells, skills,
   specialization and equipment were unchanged. Cause is still untraced; nothing
   was restored or treated as an approved preservation exception.
3. **TK 6510 lost its persisted `instance_progression_reset` row.** Its instance
   row, **3/4 completed-boss mask (7)**, `T E 3 3 3 0` data and all ten permanent
   character binds remained intact. The deleted metadata was:

   ```text
   instanceId  stage  resetTime   extendedResetTime
   6510        2      1791648306  1791907506
   ```

   The ordinary deadline was **October 10, 16:05:06 UTC**; the extension deadline
   was October 13 at the same UTC time. Losing metadata is not the same as losing
   the three kills, but it compromises persistence of their reset schedule and
   must not be described as a successful exact preservation result.

Four advisor respawn rows (158219–158222) briefly appeared at startup and were gone
again by the persistence observation. No assistant respawn command was issued.

### After 380 seconds

All **2,161 item identities, every inventory position and all 717 equipped
positions** remained intact. No new/missing item IDs, replaced gems,
permanent-enchant changes or new socket gems were found. Levels/XP, roster, quests,
abilities, specs, boss progress, binds, GO respawns and global-reset tables matched.

- Eight temporary-enchant timers aged; Feelesia applied enchant 2713 to Netherbane
  29924 and consumed one Adamantite Sharpening Stone 23529 (15 → 14). No assistant
  equip/maintenance command was issued; this is consistent with ordinary upkeep.
- All 19 pet identities remained. Feelesia's Cat 215916 changed only slot/runtime
  health/happiness/save-time fields, **plus three pet-spell autocast states**:
  Growl 27047, Claw 27049 and Rake 59885 changed **193 (enabled) → 129 (disabled)**.
  Spell IDs were retained. The persisted action bar already recorded those spells
  as 129 before restarting, which may explain normalization, but the precise
  action path was not traced and autocast preservation is not claimed.
- Group disbanding, Raney's missing preferences and TK's missing deadline row
  persisted through observation. No database rows were restored.
- Feelesia was subsequently observed non-combat in Netherstorm, outside TK. That
  is **not** live acceptance of recovery near Kael. No assistant resurrection or
  teleport was performed.

The strict audit remains failed (`preservation-observation.exit=1`); infrastructure
verification is separately successful (`observation.exit=0`). No inherited
exception allowlist or new blanket waiver was used. Earlier missing-item or
preference investigations are not resolved by this fresh-baseline audit.

## Reset-metadata deletion bug (source fix deferred)

Source inspection found a concrete pre-existing cleanup hazard:

- `Group::Disband` records a connected member's nonzero instance ID and calls
  `InstanceSaveMgr::DeleteInstanceSavedData(instanceId)` for GO-state cleanup.
- That helper also deletes `instance_progression_reset` for the instance, without
  checking whether its instance row and other permanent player binds remain.
- Feelesia was the monitored companion still in TK before this restart; the other
  nine were outside. This gives a source path consistent with the simultaneous
  disband/deadline deletion. No runtime call-stack trace was captured.

The cleanup code predates this five-file TK fix. Group disbanding itself may be
acceptable; deleting an intact raid's deadline as a side effect is a distinct
problem. The loaded in-memory deadline was not directly measured, so its current
value must not be asserted from the missing DB row alone. Do not restart again or
restore/replace raid state automatically. The exact pre-stop row and full backups
are retained for a separately authorized narrow repair, with fresh state checks
(the later authorization and one-row restoration are recorded below).
A durable source repair should separate actual instance deletion/reset from mere
GO/group cleanup and preserve other players' progression metadata.

## Authorized deadline-only restoration — 21:40 CEST

After the user explicitly approved restoring the one row, the exact pre-stop
`6510 / 2 / 1791648306 / 1791907506` record was restored at **21:40:19 CEST**.
The full pre-stop SQL dump was checked for the same tuple, in addition to its
verified checksum/gzip integrity and the separate snapshot.

Fresh checks found the deadline row still absent, the same 3/4 boss data and all
ten original permanent/unextended binds, with no human online. A transaction
locked the relevant rows and used a conditional plain INSERT requiring that
unchanged state, an absent target row and an unexpired deadline. It inserted
**exactly one row**: no UPDATE, REPLACE, bulk restore or overwrite. Immediate and
five-second post-commit reads matched the exact backup row; instance data and
binds remained byte-for-byte unchanged. Worldserver identity/start time/restart
count were unchanged. No other gameplay data was modified by the restoration.

The original failed deployment audits above remain as historical evidence, not
rewritten as passes. Raney's preferences and pet autocast states were not restored.
**The underlying deletion bug is deliberately deferred at the user's request**;
this was a deadline-only data repair, not a code change.

Private restoration evidence:
`backups/tk-reset-deadline-restore-20261008-214013/` (pre/post/recheck snapshots,
backup tuple, guarded SQL, transaction row count, service identity and checksums).

## Evidence

Private directory: `backups/tk-recovery-deploy-20261008-210957/`.
Pointer: `/tmp/tk-recovery-deploy-backup`.
It retains dumps/checksums, original strict audits, all preservation deltas,
source/config manifests, build logs and warning review, inspected/live hashes,
disassembly and corrected binary validation, container/service evidence, and logs.
No credentials, dumps, binaries or runtime configs are committed to Git.
