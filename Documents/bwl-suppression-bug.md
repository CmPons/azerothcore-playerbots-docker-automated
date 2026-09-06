# BWL: invisible suppression devices (2026-09-06)

## Status / scope

**Deployed with explicit permission.** Only worldserver was gracefully stopped
(exit 0) and recreated. The targeted SQL migration was applied while it was stopped.
No encounter reset, bind edit, or assistant-issued respawn command was performed.

- Live image: `acore/ac-wotlk-worldserver:master`, also tagged `bwl-suppression-fix-test`.
- SHA: `1f8289b93dbe39fd83d13a924113d9dfd68c35934198c0c70048c54c4707d4dd`
- Started: `2026-09-06T19:44:01.75751003Z`; ready, restart count 0.
- Build log: `/tmp/bwl-suppression-fix-build.log` (exit 0).
- SQL verified after startup: mask 1 still targets players; mask 2 targets only eggs.
- BWL 199 boss progress and all saved binds match the post-shutdown/pre-deployment
  snapshot exactly: first four bosses DONE, completedEncounters 15.
- Authserver/database start times and restart counts, plus Pi's PID/start time,
  were checked before and after and are unchanged.
- Pending party/raid join chatter is also in this image and is now deployed;
  its in-game validation remains pending.

Backup: `backups/bwl-predeploy-20260906-213916/` (private, gitignored). Includes
all four databases, configs, original spell conditions, and the previous image
archive/tag. An additional full database snapshot was taken **after worldserver
saved and stopped, before applying SQL**: `databases-after-worldserver-stop.sql.gz`.
Gzip integrity, dump completion markers, and checksums were verified. Startup/
shutdown logs and before/after raid/service snapshots are preserved there.
Rollback image tag: `acore/ac-wotlk-worldserver:pre-bwl-suppression-20260906-213916`.

The user restored the devices using the in-game macro below. A subsequent
read-only query found zero future suppression-device respawn timers in instance
199. Normal visibility of one individually restored pillar was confirmed by the
user; all-device rogue behavior still needs in-game confirmation.

Keep these incidents separate:

- **161, abandoned copy:** Razorgore stayed alive fighting an NPC after a wipe;
  the encounter-in-progress entry lockout remains unexplained. No speculative
  fix to boss combat, charm, evade, or reset behavior is included.
- **199, current copy:** the raid killed Razorgore before completing the eggs
  repeatedly; the encounter correctly restarted. Later, after legitimate
  Razorgore and Vaelastrasz kills, invisible devices suppressed the raid.

## Evidence and confidence

1. All 38 device spawns (entry 179784, spawn IDs 75120..75157) in instance 199
   had the same future respawn timestamp, 1788724540. Their 7200-second default
   delay implies a despawn near epoch 1788717340 (assuming no delay scaling).
   Several unrelated objects, including doors and alchemy labs, had the same
   inferred despawn time.
2. GM on showed the bronze pillars; GM off hid them. A single
   `.gobject respawn 75129` restored a normally visible pillar.
3. GameObject AI updates run before the core's spawned-state handling.
   `go_suppression_device::UpdateAI` tested only GO_STATE_READY. Despawning an
   object resets it to GO_STATE_READY, so it could keep casting every five seconds.
4. GM visibility returns true before the despawn visibility check. Playerbots'
   `AnyGameObjectInObjectRangeCheck` excludes objects for which `isSpawned()` is
   false, explaining why Pilbok did not find the hidden devices to disarm.
5. Razorgore's lethal phase-one `DamageTaken` casts Explode Orb Effect (20037)
   and Explosion (20038). Explosion effect index 1 is ACTIVATE_OBJECT with action
   15 (Despawn), destination-area gameobject targeting, radius index 12 (100 yards).
   The spell's correction enables ignoring line of sight. The existing world
   condition restricts **only effect index 0** (mask 1) to players; effect index 1
   (mask 2) had no entry restriction.
6. An offline replay of `GameObject::IsInRange3d`, using the actual 38 spawn
   positions/orientations/scales and display-5874 DBC bounds, selects all 38 from
   the example caster position (-7640, -1060, 408). This is a rotated expanded-box
   check, not a spherical distance check. The example is **not a captured death
   position**. At Razorgore's spawn position it selects only 22 of the devices.

The hidden-casting defect is established. The unrestricted explosion provides
an independently demonstrated targeting failure and strongly matches this
incident, but there is **no historical cast-to-object trace and no isolated live
encounter reproduction yet**. Do not claim the precise historical cause is
proven, or conflate this with instance 161's combat lockout.

## Changes

Canonical delivery: `patches/0018-core-bwl-suppression-despawn.patch`, reapplied
by `setup.sh`'s existing patch loop. It contains two narrow changes:

- `src/server/scripts/EasternKingdoms/BlackrockMountain/BlackwingLair/boss_broodlord_lashlayer.cpp`:
  require `isSpawned()` as well as GO_STATE_READY before a suppression pulse.
  Keep processing/scheduling events while hidden, so legitimate reactivation and
  natural respawn can resume pulses. Do not add an early return from UpdateAI.
- `data/sql/updates/pending_db_world/rev_1788721200000000000.sql`:
  restrict spell 20038's **effect index 1 / SourceGroup 2** to type 5 gameobjects
  with entry 177807 (Black Dragon Egg). Preserve effect index 0's player-only
  condition, spell 20037, Destroy Egg (19873), and unrelated conditions.

This preserves egg cleanup and the intended player-wipe punishment, but prevents
Explosion from despawning suppression traps, doors, the orb, and other unrelated
objects. The orb's separate spell 20037 is unchanged; verify its behavior during
integration testing. This does not remove the suppression mechanic or make
warriors able to disarm traps.

## Validation performed

- Six offline tests pass:
  `python3 -m unittest discover -s scripts/tests -p 'test_bwl_suppression.py' -v`.
  Tests execute the shipped SQL in SQLite, verify the effect-mask isolation,
  idempotence, egg allowlist, retained player filter, and replay cross-room target
  eligibility. The C++ guard test is a **source contract**, not runtime AI testing.
- Actual MySQL syntax/idempotence tested twice against a connection-local
  **temporary table** copied from the relevant current conditions. One egg filter,
  one unchanged player filter, and the unchanged Destroy Egg filter remained.
  No production conditions were modified.
- Patch applies to a clean baseline and reverse-checks successfully; generated
  source matches the working tree. `git diff --check` passes.
- SQL codestyle passes. Modified C++ file passes the official checker in isolation.
  Repository-wide C++ codestyle fails on pre-existing unrelated files (tabs,
  blank lines, qualifier alignment, whitespace); these were not edited.
- After explicit build-only permission, the full host-network Docker worldserver
  build passed, including compilation of `boss_broodlord_lashlayer.cpp` and image
  packaging. **No isolated live encounter reproduction performed.** Deployment
  was subsequently explicitly approved and completed as recorded above.
- Startup reached ready and loaded 14661 conditions. No spell-20038 rejection was
  observed. Logs are not entirely error-free: process-priority permission and
  unrelated invalid skill-condition warnings were present; no claim of fixing
  those is made.

## Immediate recovery used for this copy

While in BWL, click once; remain there for approximately 76 seconds. Requires an
account with `.gobject respawn` permission, not necessarily GM mode enabled.
The IDs below were verified as exactly the 38 existing map-469 suppression
spawns on this server; do not reuse blindly on a different database.

```lua
/run local f=CreateFrame("Frame");local i,t=75120,0;f:SetScript("OnUpdate",function(s,e)t=t+e;if t<2 then return end;t=0;SendChatMessage(".gobject respawn "..i,"SAY");i=i+1;if i>75157 then s:SetScript("OnUpdate",nil)end end)
```

`.gobject respawn` resolves the existing spawn in the caller's current map. It
calls `GameObject::Respawn`, clearing the map's respawn record and allowing the
loaded object's next update to restore it. It does not create duplicate spawns,
reset bosses, or change permanent binds. It is not console-capable. SQL-only
respawn-table edits would not reliably repair already-loaded objects.

## Deployment and remaining validation

The approved deployment is complete. Any further interruption requires fresh
permission. The full working-tree build also includes other staged/uncommitted
module work and the group-join chatter change; it is not an isolated hotfix image.

**SQL deployment caveat:** although the world DB lists the pending-update directory
in `updates_include`, the worldserver Docker target sets
`AC_UPDATES_ENABLE_DATABASES=0` and does not package the core SQL tree. Merely
recreating worldserver with this image will NOT apply the egg-targeting migration.
The new C++ pulse guard is in the image; the SQL must be applied separately during
an approved deployment (preferably while worldserver is stopped, before startup),
or through an explicitly approved db-import deployment containing the new SQL.
Do not assume the existing db-import image contains this new file. Avoid running
broad setup/import work against the active raid for this one change.

For this deployment, spell 20038's existing conditions were backed up, then only
`data/sql/updates/pending_db_world/rev_1788721200000000000.sql` was applied inside
a transaction while worldserver was stopped. Masks 1 and 2 were verified before
and after startup. No db-import or broad setup run was needed. The SQL is
idempotent if a future updater applies it again; it was applied manually, not
recorded as an updater-run migration. No production SQL was applied during the
earlier build-only request.

In a disposable test copy, with a separate process or later authorized test window:

1. Baseline: record every suppression device's spawned/state/respawn values.
2. Kill Razorgore before egg completion at several positions. Capture spell 20038
   targets. Repeat failures; verify the player wipe and egg/reset behavior, while
   devices and doors remain spawned. Check the orb separately.
3. Complete the eggs and kill Razorgore legitimately; confirm ordinary progression.
4. Despawn a device deliberately in the test copy: no new suppression pulse.
   Respawn it: normal visible active pulses return.
5. Test player-rogue disarm/reactivation and Pilbok's automation; neither should
   depend on GM mode. Playerbot disarming currently sets GO_STATE_ACTIVE directly
   rather than using the encounter's DoAction path; changing that is outside this fix.
6. Complete Broodlord and verify the intended trap shutdown/disarm behavior.

For the separate instance-161-style lockout, capture the still-live boss's charm,
controller, victim NPC entry/GUID, engagement/threat/evade state, and encounter
state **before** any reset or unbind. Saved player coordinates are not reliable
live evidence. Existing logs did not establish the original NPC opponent or the
precise control/evade sequence; that issue remains open.
