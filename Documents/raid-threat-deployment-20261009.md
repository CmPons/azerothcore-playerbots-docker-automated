# Raid threat controls deployment — October 9, 2026

Explicitly authorized after the implementation handoff. **Worldserver ready at
19:18:43 CEST / 17:18:43 UTC.** Only worldserver was stopped/recreated. Authserver,
database, importer/client-data helpers and Pi bridge were not restarted.

## Source and image

- Playerbots: `68556d789ee3420ff0eadaca11d5e9b03828f473`.
- Source/tests/docs/pin: root `3774595d4b4630d8cb21097932fe00fe42c4c3e1`.
- Core remains `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
- Image: `acore/ac-wotlk-worldserver:raid-threat-20261009-190924`.
- Image ID: `sha256:0f61aeb6f8d33d6bb17316b9dfd1dccbcd58f7065b40de93ece148e1bfe9ea44`.
- Worldserver SHA256: `19ded8dacb8559bff71c72349bbe18a8902b792fe1f84700c5e3e1697adbda1a`.
- Container: `68a8d038eab8da4b4db8f8c7a78dbdf80b2f3d1bf8565278ec3141cd0436f49c`.
- Started: `2026-10-09T17:18:25.917619776Z`.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-raid-threat-20261009-190924`.

Only four source paths differ from the running advisor-fix image: ThreatStrategy,
RaidThreatControl header/runtime script, and PlayerbotCommandScript registration.
The pending 90% AoE default is included in that delta. No core/encounter/SQL/live
configuration edits, updater changes, roster operations or equipment commands.

## Gates and operation

- No human online at preflight or the final pre-stop check.
- Fresh verified four-database dumps before build and after clean stop, plus current
  config, inventory/abilities/pets, groups, roster and raid progress/bind snapshots.
- **105 tests passed in 297.725s**, covering controls, healing admission, current Twins
  policies, pull authority, tank modes, Feelesia recovery, advisor/Twins/respawn and
  support scaling, dead-bot loot, Leotheras reset and direct-token loot.
- Native-header syntax passed for all three changed/new translation units. Source
  publication's scoped official C++ codestyle check also passed.
- **4,210 source / 48 config-policy records** remained stable through build/deployment;
  root-owned module mirrors matched. SQL updater ledgers remained unchanged.
- Build: **19:15:54–19:16:41 CEST**. All 16 warnings were in unchanged source.
- Never-started candidate inspection verified new policy/observation calls, command,
  map cleanup, script-loader linkage and friendly-recipient/boss guards. Retained advisor,
  recovery, loot, tank, respawn, whirlwind and progression hooks were also inspected.
- Dependency check ran only `ldd`, not a second worldserver. The live process's
  `/proc/.../exe` SHA256 matches the inspected candidate, not merely an image tag.
- Deployment used `--no-deps --no-build --pull never --force-recreate ac-worldserver`.
  No setup/importer, automatic rollback, SQL restoration or Pi generation request.

## Preservation

Use the fresh baseline: the user GM-killed Kael and then manually revived the encounter.
The snapshot therefore had TK6510 **mask15 / `T E 3 3 3 0 `**, not the older mask7.
Its progression tuple was `(6510,3,1791604800,1791691200)`: ordinary deadline October10
06:00 CEST, extension October11 06:00 CEST. All ten binds existed; Redshift alone had
`extended=1`. This is saved-state preservation, not a claim of a legitimate Kael clear.

### Before build → clean stop (old image still running)

The item total remained **2,128**; all **725 equipped identities**, existing gems and
permanent enchants survived, with no surviving item moved. Beliona's Soul Shard6265
and Master Soulstone22116 were replaced by new GUIDs of the same entries/counts, consistent
with conjured-item maintenance (individual casts were not traced). Background weapon-buff
refresh/expiry, reagent counts, consumable charges and pet resource/time changes were
also captured before the new image started. These must not be attributed to the new
threat controls. Exact deltas are retained in `preservation-review.json`.

An initial closeout assertion mistakenly demanded prebuild-to-stop identity equality
and caught these two replacements; it was corrected to distinguish old-image activity
from stopped-to-start/post-save preservation, not waived or repaired with SQL.

### Clean stop → immediate startup

- All **2,128 items, every item field and position**, and all 725 equipped identities exact.
- Abilities, specs, levels, roster, quests, profiles, pets and pet spells exact.
- TK progress/deadlines, all ten binds and saved-state records exact.
- **Group67 disbanded.** This is consistent with the existing `KeepAltsInGroup=0`
  behavior; no group-disband call trace was captured and no policy was changed.
- Strict startup audit remains qualified: `accepted=false`, solely the group difference.
  No automatic restoration or blanket exception was applied.

### Post-save observation

After the 380-second observation (past the five-minute save interval):

- All **2,128 item identities and positions**, all **725 equipped identities**, existing
  socket gems and permanent enchants remained intact. No items added/removed/moved.
- Eight item-field changes were only normal temporary-enchant timer decreases.
- Levels, abilities, specs, roster and quests remained exact.
- All **19 pet identities, levels, spells and action bars** remained exact. Two pets had
  health/mana/happiness/save-time changes; no autocast-state change was observed this time.
- TK instance data/mask, progression tuple and all ten binds (including extension flags)
  remained exact. No metadata restoration was needed.
- **Raney lost 13 saved AI-preference rows** (three strategy strings and ten value rows).
  This recurring persistence issue was not fixed by this deployment. His gear, talents,
  spells and skills were unchanged; the deletion's precise action path was not traced.
- Group67 remained disbanded. No profiles, groups, pets or SQL rows were restored.

Infrastructure observation passed: same new container, zero restarts/OOM, one readiness
line, unchanged other services/Pi identity, source/config manifests and updater ledgers.
No humans were online at the final observation.

**`observation.exit=0`, `preservation-observation.exit=1`.** The strict audit remains
`accepted=false` with three unreviewed categories: profiles, groups and pets. Reviewed
field descriptions above do not erase the original strict audit or supply a blanket
waiver. Gear/raid preservation succeeded; not every persisted field was identical.

## Availability and limits

See [raid-threat-controls.md](raid-threat-controls.md) for commands and policy.
Defaults are **90% AoE / 80% current target / configured boss70%**, with emergency
healing **strictly below30% recipient HP**. Damage retains its threat checks. No live
instance overrides were set by the deployment.

This proves deployment, linkage and the recorded preservation outcomes—not a successful
live Kael pull, a guaranteed cast after every allowed check, or an in-game command
acceptance test. Other `IMPOSSIBLE` causes and encounter-specific checks remain separate.

Private evidence: `backups/raid-threat-deploy-20261009-190924/`, pointer
`/tmp/raid-threat-deploy-backup`. Includes scripts, strict audits, backups, manifests,
disassembly, runtime identities and checksums. Credentials/dumps/binaries are not published.
