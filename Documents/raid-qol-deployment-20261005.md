# Raid QoL deployment — October 5, 2026

## Result and authorization

The user explicitly requested **“Build and deploy, I'm logged out.”** The worldserver
was replaced once and became ready at **2026-10-05 15:05:01 UTC** (17:05:01 CEST).
All three published changes are now included:

- [Per-boss token class uniqueness](spec-aware-direct-token-loot.md#per-boss-class-uniqueness):
  no repeated class within a generated corpse/chest token-loot list; exhausted pools
  retain their tokens, and choices reset for the next list.
- [TK trash whirlwind avoidance](tk-trash-whirlwind-avoidance.md): two audited Bloodwarder
  spins, owner-tank exemption, escape and shared automatic movement/approach guards.
- [In-place respawn health scaling](raid-respawn-scaling.md): native respawn hook and
  matching module, fresh per-life baseline and current-health-fraction preservation.

**Deployment is live; preservation review is qualified, not an unconditional pass.**
The immediate startup snapshot retained all 2,058 monitored item identities/fields,
abilities, specs, pets and raid saves. Group 4909 was disbanded by startup, reproducing
an existing issue. The user resumed play during the save-window observation. One spare
bag wand disappeared afterward and remains unexplained; it is explicitly **not waived**
in the follow-up audit. See below. No restoration was performed.

## Published source and binary

| Component | Revision |
| --- | --- |
| Root build source | `3d12bc399d2777df8085d8868f1f0c26337732fe` |
| Core | `8b8b7bcf8615c62b4aedaa00c33713a0c1f6df0e` |
| Playerbots | `8275e8f8f9998136047ee034458bb1e2bc49dd2f` |
| Level brackets | `a2614288ebe41093b8915b3ee3002bcfc8a63c1a` |

- Image tag: `acore/ac-wotlk-worldserver:raid-qol-20261005-164819`
- Image ID: `sha256:52b331fc95a969cb2372eb98a702d7db14018e10975a0403fe9c4045be79cff6`
- Worldserver SHA256: `7b1f8182838a851dc1c682dc370efe5808fb2eafafde91b43813df28833e1a97`
- Container: `1dbb24552d38934a7174b1367e38a0413f4a17956d15d2f7b0ddeb0e4a24fa68`
- Started: `2026-10-05T15:04:40.546437938Z`; restart count zero at closeout.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-raid-qol-20261005-164819`
- Previous image: `sha256:a6e97b4939667208b679625bacc43363eac37dc4f33bc7d138afb2974bcbecb7`
- Previous binary: `7e700ba144d6b39c0698da38902a51c7ee1753d7511cef4fa21902f6949fa9ff`

The previous container had already restarted once since the October 1 deployment;
its observed start was October 2, 19:44:23 UTC. This task did not assume that the old
container's initial October 1 start time was still current.

## Build and controlled replacement

- Fresh four-database dumps before the build and after clean shutdown, private env/config
  copies, 43-character snapshots, 19 pets, loot/gear/talent/profile/group/save snapshots.
- Running old binary matched the prior accepted deployment. Exactly **14 native source
  files** differed from that build: four core hook files, three scaling-module files,
  and seven playerbots files. Root-authored module mirrors matched.
- **100 tests passed, one optional connection-local MySQL test skipped**, across 101
  tests. The token/scaling/whirlwind suites include sanitizers and compiled old-code
  regressions; existing combat, reset, tank and publication safeguards were exercised.
- All **eight production-header syntax checks** passed.
- Full image build succeeded. Its 16 compiler warnings were all in files byte-identical
  to the previous running source; none came from a changed source file.
- Extracted the binary from a never-started, network-disabled inspection container.
  Symbol/disassembly checks verified the new respawn dispatcher/caller/module override,
  whirlwind escape and gap-closer guards, token-exhaustion log and spec-scoring ABI,
  and retained tank, scaling, Chess/progression and other protection symbols.
  Dependency inspection reported no missing libraries.
- Immediately before stopping, no human account was online and the current saved TK
  encounter data was `T E 3 3 3 0 `. Worldserver stopped cleanly with exit zero.
- Used `docker compose up -d --no-deps --no-build --pull never --force-recreate ac-worldserver`
  after tagging the verified candidate as the configured `master` image.
- Running binary matched the candidate byte-for-byte. Auth/database/importer/client-data
  container identities, start times and restart counts, and Pi service identity were
  unchanged. Pi tags endpoint remained healthy.
- All 4,208 source and 48 configuration/policy manifest records stayed unchanged during
  build/deployment. Core and playerbots updater ledgers matched throughout. No setup,
  importer run, gameplay SQL, forced save, migration, item grant or configuration edit.

The initial read-only preflight used incorrect talent-column names. Its failed log/exit
are retained; it was corrected to the actual `talentGroupsCount`/`activeTalentGroup`
schema before backups/tests/build proceeded. No failing source test was waived.

## Preservation and live activity

### Immediate startup

All 2,058 monitored items and their fields, inventory positions, levels/XP, roster,
quests, preferences, talents/spells/skills, active talent groups, 19 pets/pet spells,
and saved raid state matched shutdown exactly.

**Exception:** ten-member raid group **4909** disappeared. Its exact rows were reviewed,
not restored. This repeats the known startup disband symptom with `KeepAltsInGroup=0`
and unchanged login/group source; no event-level trace was enabled. The original strict
failure remains in `audit-after-start-first-strict.json`.

### Normal-save observation, 17:11:03 CEST

The observation occurred more than 380 seconds after startup. **Redshift had returned
and entered Gruul's Lair**, so it is not a controlled idle-server preservation test:

- All **727 equipped item identities and inventory positions** across the 43 monitored
  characters remained exact. Permanent enchants and previously occupied sockets stayed
  intact. Existing companion maintenance logged **17 empty socket fills** on seven items,
  including two epic gems; no occupied socket was overwritten.
- Levels/XP, roster, quests, talents/spells/skills, active talent groups and pet spells
  matched. All 19 pet identities/owners/names/levels survived; three active pets had
  runtime health/mana/happiness/save-time changes, and Raney's elemental moved to inactive
  storage (`slot 100`). No pet restoration or replacement was done by the deployment.
- Inventory went **2,058 → 2,080**: 2,049 prior identities remained, 31 new consumable/ammo
  items appeared, and eight old consumable/ammo stacks were replaced. Six arrow stacks
  totaling 5,053 became six new stacks totaling 6,000; soulstone/shard refreshes and
  reagents/oils/poisons/stones appeared. Sixteen existing consumable stacks increased to
  20. These match the unchanged stockup/consumable behavior during resumed play; no
  per-command trace was enabled. Temporary enchant timers and 14 expired trade flags
  also changed normally.
- **Unresolved ninth missing item:** Raney's spare **Tirisfal Wand of Ascendancy (28673)**,
  item GUID `24531107`, formerly bag `7103062`, slot `10`. It was present immediately
  after startup, then absent from `item_instance` during live-play review. It was not
  equipped. A sale/deletion event was not traced; the user was asked whether they sold
  or junked it. **This loss remains unwaived. No claim that every bag item survived.**
- Raney's **14 stored preference rows** disappeared by the follow-up snapshot, repeating
  the previously reported symptom. They were still present immediately after startup;
  the exact later trigger is unproved. No preference restoration was performed.
- The raid was reformed as **group 33**, with the same ten members. The old MT/assistant
  flags on Redshift were not restored automatically.
- Original **TK save 2787**, completed mask 7, `T E 3 3 3 0 `, progression metadata and binds
  were preserved. Its existing unextended deadline remained **October 5, 19:26:29 UTC**
  (21:26:29 CEST). No reset or extension was introduced by deployment.
- New **Gruul save 18**, ten temporary binds and three creature-respawn rows appeared
  during live play; no existing TK row was removed. The observation snapshot had no
  completed Gruul bosses; a later read-only view showed `G L 3 1 `, consistent with
  subsequent Maulgar completion and Gruul in progress. Do not treat that later activity
  as startup state or a completed Gruul kill.

The first strict observation failed and is retained with `observation.exit=1`. Exact
reviewed rows have per-phase exception files and a machine-checked preservation proof.
**The reviewed observation still exits 1** for the unwaived bag-wand inventory/identity
loss. Infrastructure closeout passes separately; this does not turn the item audit green.

## Runtime warnings and evidence limits

Retained startup warning: `Can't set process priority class, error: Permission denied`.
Duplicate `pet_spell` insert warnings involved keys `356-11762` and `353379-50256`, owners
719 and 753 respectively, both outside the monitored roster. No repair was attempted.
The first rank row was not in the stopped dump; do not call it an unchanged pre-existing
row merely because this general warning category has appeared on earlier startups.

No forced boss kill, respawn, extra loot generation or encounter reset was used for
validation. Deployment/binary verification is not live acceptance of TK avoidance,
new token drops or the compatibility-respawn scaling boundary.

Private evidence: `backups/raid-qol-deploy-20261005-164819/`;
pointer `/tmp/raid-qol-oct05-backup`. Strict outputs, qualified reviews, source/config
manifests, dumps, binaries and runtime logs are retained and checksummed. Backups are
not permission to restore gameplay state. No second restart was performed.
