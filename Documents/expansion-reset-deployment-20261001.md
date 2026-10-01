# BC/Wrath progression reset deployment — October 1, 2026

Authorization: **“You are free to deploy when you're done.”** Only worldserver was
replaced. No importer, database, authserver or Pi restart; no live config edits,
roster sync, equipment grants/rerolls, forced saves, direct gameplay SQL or restoration.

## Build and running identity

- Core: `ab00bd5c78071f80a69c06d06ac2bb33b6bbd880`.
- Playerbots unchanged: `cef0162a7202f7a10688e989c0dba5d76828dbb1`.
- Root build source: `4445110bf619462221d4754443152ad009dc03df`.
- Tag: `acore/ac-wotlk-worldserver:expansion-reset-20261001-214845`.
- Image: `sha256:a6e97b4939667208b679625bacc43363eac37dc4f33bc7d138afb2974bcbecb7`.
- Binary SHA256: `7e700ba144d6b39c0698da38902a51c7ee1753d7511cef4fa21902f6949fa9ff`.
- Container: `90cdfca8388044c20d584f449902e9b9a3576627ca48c74bbe020482b001608c`.
- Started: `2026-10-01T20:09:21.581613817Z`.
- Ready: **`2026-10-01T20:09:42.914177455Z`**.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-expansion-reset-20261001-214845`.
- Previous image: `sha256:70612854d5351b8009de868b58b0a2ca0d16528e068980a007efb9649ca5d764`.

The [new policy and completion catalogue](expansion-raid-progression-resets.md) covers
all 23 supported Classic/BC/Wrath raid maps. Existing settings remain enabled with
three progression days and the daily boundary at 04:00 UTC.

## Safety and build evidence

Fresh private four-database/config backups were taken before building, then fresh
four-database dumps after a clean worldserver shutdown. Checksums and dump completion
were verified. Private evidence: `backups/expansion-reset-deploy-20261001-214845/`.

- 4,206 source records, root-module mirrors and 48 config/policy records verified.
- Exactly three source differences from the previous running image: reset policy header,
  Karazhan Chess checkpoint/combat predicate, and worldserver config-template comments.
- Pre-build combined suite: **47 passing tests, one opt-in SQL test skipped** (48 total).
- Three production-header syntax checks passed. Policy, native-enum contracts and
  extracted production Chess tests also passed ASan/UBSan; old Chess fails the regression.
- Targeted official C++ style checks passed; all ten unique native build-warning
  signatures already existed in the preceding build (no new warning signature).
- Full worldserver image build passed using the existing Dockerfile and pinned Ubuntu
  digest. Offline disassembly verified the expanded parser, new Chess combat predicate
  and persistence path, plus retained token loot and tank-assistance/pull guards.
- `ldd` resolved dependencies. Running binary hash matches the inspected candidate.
- No humans were online at the stop gate; saved SSC was partial and Gruul/Mag complete,
  with no saved encounter in progress. Old process exited 0 without OOM/forced timeout.
- Replacement used `--no-deps --no-build --pull never --force-recreate` for worldserver
  only. Database updating remained disabled. No migration was needed or applied.

## Actual adopted saves

Verified against the **fresh stopped-state** records, not the pre-build gameplay snapshot:

| Instance | Saved progress | Stage | Base deadline (UTC) | Extension deadline (UTC) |
| --- | --- | --- | --- | --- |
| SSC 213 | `S S 3 3 3 0 0 0 ` — Hydross, Lurker, Leotheras defeated | Progression | October 4, 20:09:21 | October 7, 20:09:21 |
| Gruul 18572 | `G L 3 3 ` | Cleared | October 2, 04:00 | October 3, 04:00 |
| Magtheridon 18591 | `M L 3 ` | Cleared | October 2, 04:00 | October 3, 04:00 |

Redshift's SSC bind remained permanent with **`extended=1`**. The extra three-day
extension is still opt-in, already selected by the player; adoption did not clear it.
Gruul/Mag extension deadlines are available only if a player elects to extend.

Independent adoption verification checks:

- Encounter data, instance IDs, kill-credit masks, difficulty, **all bind/extension
  rows**, tracked creature respawns, instance gameobject respawns and saved GO state exact.
- Global reset table unchanged; only three new per-instance metadata rows were added.
- Partial deadline is exactly 72 hours after a timestamp within startup; cleared
  deadlines align with the next 04:00 boundary. No managed deadline was refreshed.
- No historical kill timestamp was fabricated: SSC receives its documented one-time
  adoption grace window, not a retroactively guessed first-kill time.

## Preservation audit and disclosed exceptions

Startup monitored 43 characters, the 40-member roster and **2,073 item identities**.
Inventory/equipment, item fields, existing gems/enchants, levels/XP, quests, talents and
spells matched exactly at the initial snapshot. Save differences were only the verified
metadata adoption described above.

Two previously observed bot-startup behaviors recurred and were disclosed to the user:

1. **Raid group 412 disbanded**, including all ten core characters and Ari's MT marker.
   This is consistent with the existing offline-master login group cleanup with
   `KeepAltsInGroup=0`. Raid binds and boss progress survived. Reform the raid and mark MT
   when returning; no operator regroup was performed.
2. **Raney lost exactly 14 saved AI-preference rows** (`playerbots_db_store`). This
   recurring loss was documented in September as well. It includes saved value and
   combat/noncombat/dead strategy preferences, not gear/talents/spells. The precise
   triggering event remains unproven. Playerbot source/config did not change. Her exact
   stopped rows remain in the private backup; no restoration or speculative fix occurred.

These are reviewed exceptions, **not a claim of perfect profile/group preservation**.
Initial strict failures remain retained before exact per-deployment exceptions are
accepted. No historical whitelist was blindly reused.

### Normal-save observation

At `2026-10-01T20:15:44Z` (more than 380 seconds after process start), ordinary saves
confirmed the same SSC state/deadline/extension and unchanged other raid saves. No
forced save was issued. Levels/XP, roster, quests, talents and spells remained exact;
the group/profile exceptions above remained the only differences in those categories.

All **2,073 pre-existing item identities remained owned**, with unchanged permanent
enchants, gems, counts and durability. Autonomous activity produced these exact changes:

- Meliah, Beliona, Ailina, Kaaren, Keilmere and Feelesia equipped their **already-owned
  Fishing Pole (6256)**. Their six previous mainhands and three displaced offhands all
  remained in their bags: 15 inventory moves, no deleted/replaced combat gear. This
  matches the unchanged `FishingAction::Execute` / `EquipFishingPoleAction` path; no
  individual action trace was enabled. Check weapons when regrouping after fishing.
- One Raw Brilliant Smallfish (6291), GUID 31596367, appeared in Feelesia's bag. Total
  monitored item identities became **2,074**. No operator item grant occurred.
- Eleven existing temporary-enchantment timers decreased. One Ailina ring's BOP-trade
  flag expired (`257→1`, item GUID 31493209, entry 33055). No new socket filling occurred
  during this observation.

The strict initial observation failed on inventory moves/new fish; its original
`observation.exit=1` and report are retained. `review-activity.py` independently proves
all prior identities, permanent enchants/gems and displaced combat gear, then permits
only the exact observed rows. Reviewed audit and observation both exit 0. This does not
waive the separately disclosed profile/group losses.

Final source/config manifests and updater ledgers matched; auth/database/importer/data
helper identities and Pi invocation/PID were unchanged. One ready line, no automatic
restart or OOM. Runtime retained the process-priority warning and two duplicate
`pet_spell` primary-key errors for pets 8105/14329 (owners 2280/389, outside the monitored
43); the referenced spell rows exist. These are disclosed, not an error-free startup
claim. No pet operation or unrelated repair was attempted.

No second restart, restore, gameplay SQL edit or change to the published feature code
was made. Real future expiration and new live Karazhan Chess victory remain gameplay
acceptance checks, not events manufactured for this deployment.
