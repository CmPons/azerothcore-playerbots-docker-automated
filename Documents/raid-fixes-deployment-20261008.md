# Leotheras and dead-bot loot deployment — October 8, 2026

## Result and scope

The user explicitly authorized building and deploying the two pending fixes while
breaking for dinner after TK 3/4. **Both fixes are deployed; worldserver is ready.**

1. [Leotheras wipe/reset lifecycle](leotheras-wipe-reset.md): restart attacks and
   scheduled mechanics when legitimately re-engaged after a wipe.
2. [Dead-bot equipment valuation](dead-bot-loot-valuation.md): death alone no longer
   disqualifies a prospective upgrade; actual equipment operations retain their
   normal restrictions.

**Preservation is qualified, not an unconditional pass:** every stopped-baseline
item and inventory position survived, but the recurring raid-group loss and Raney
saved-profile loss happened again. No restoration or waiver was performed.

The separate Ailina physical-proc scoring bug and unwanted raid-strategy pulls are
**not fixed by this deployment**. Live encounter/loot acceptance of these two fixes
still requires ordinary play; no encounter was reset and no loot/death was forced.

## Published source and binary

| Component | Revision |
|---|---|
| Core | `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72` |
| Playerbots | `b63dd8d9cb4f7493c8cd397412ed0f10e06abf2b` |
| Root source/tests/pins | `a29a4a4aaa13ee73fb52a931e634048b677a6e96` |
| Level brackets, unchanged | `a2614288ebe41093b8915b3ee3002bcfc8a63c1a` |

Candidate tag: `acore/ac-wotlk-worldserver:raid-fixes-20261008-171506`.
The accepted image was also tagged `master` for the existing Compose service.

- Image ID: `sha256:8a1e64212d6431957ceb2b6174b72d7aa35283b9008354bd55913cbcfe24af4f`
- Running binary SHA256: `ebe84c9e3bbce1720289c3088747e2c2c2b4ea3ac338758e7a79d51160be87db`
- Container: `fbbedf9d7d9293331012871f8a05350d76d2777f8149282ff2116e5351a59d4c`
- Started: `2026-10-08T15:32:39.81411181Z`
- Ready: **`2026-10-08T15:33:02.285213260Z`** (17:33:02 CEST)
- Running with restart count **0** through observation.

Rollback reference, retained but not activated:
`acore/ac-wotlk-worldserver:pre-raid-fixes-20261008-171506`.
Previous image was `sha256:52b331fc95a969cb2372eb98a702d7db14018e10975a0403fe9c4045be79cff6`,
with binary `7b1f8182838a851dc1c682dc370efe5808fb2eafafde91b43813df28833e1a97`.

## Build and deployment controls

- Fresh, gzip-verified four-database backups before building and after clean stop;
  private env/config copies and 43-character inventory/spec/ability/profile/quest/
  roster/group/19-pet/raid snapshots.
- **Exactly four native source files differ from the October 5 deployed manifest:**
  the Leotheras script, `PlayerbotAI.cpp/.h`, and `ItemUsageValue.cpp`.
- **59 tests passed**, covering both fixes, direct-token loot, respawn scaling,
  whirlwind avoidance, tank modes, boss pull focus and source publication policy.
  Expected negative-control assertions are not failures of the accepted suite.
- Four native-header syntax checks passed: Leotheras, PlayerbotAI, ItemUsageValue,
  and the unchanged factory caller of the extended validation API.
- Full image build ran **17:18:14–17:30:50 CEST**. Its 16 warnings all originate in
  files byte-identical to the previous deployed source; the warning review is saved.
- Inspection container was network-disabled and **never started**. Extracted binary
  hash, symbols, dependencies and disassembly passed. The item-usage call passes
  `swap=true`, `not_loading=true`, `checkAlive=false` in the actual binary; the new
  Leotheras engagement override reaches `StartEncounter()`.
- Retained token class uniqueness/spec scoring, threat admission, respawn scaling,
  progression resets and whirlwind call-site proofs passed too.
- All 4,208 source-manifest and 48 config/policy records remained unchanged during
  the procedure; root-owned module mirrors match.
- No humans online at preflight, immediately before stop, or at observation. The
  original container stayed up through the build. It stopped cleanly with exit 0.
- Only worldserver was recreated, using `--no-deps --no-build --pull never`.
  Auth/database/importer/client-data container identities and Pi service identity
  stayed unchanged; Pi `/api/tags` remained healthy.
- All updater ledgers remained identical. No setup/importer/migration, live config
  edit, forced save, item grant, gameplay SQL mutation or automatic restore.

## Preservation findings

### Build interval on the old running binary

Prebuild snapshot contained 2,174 items; clean stop contained 2,172. The only removed
identities were Beliona's two Soul Shards and one Master Soulstone; a new Master
Soulstone replaced the latter. This is consistent with ordinary warlock activity
while the old server continued running, not deployment-time gear replacement.
No exact spell/action trace was captured. The clean-stop snapshot is the restart
baseline; the earlier snapshot and full backup are also retained.

### Immediate startup

All **2,172 item identities, fields and inventory positions** exactly matched clean
stop. Levels/XP, roster, quests, saved preferences, talents/spells/skills, spec groups,
all 19 pets/pet spells and raid saves also matched exactly.

**Exception:** raid group **33**, all ten memberships and its MT assignment disappeared.
This repeats previous startup behavior. The unchanged `KeepAltsInGroup=0` login path
is a known suspect; this deployment did not trace or repair the cause. Re-forming the
raid and checking/reassigning tank roles is required; no raid roster was rebuilt.

### Normal save-cycle observation

Snapshot at **17:39:02 CEST**, more than 380 seconds after container startup:

- **All 2,172 item identities and every inventory position retained**; no new or
  missing item identities. All **727 equipped item positions/IDs** unchanged.
- Existing gems and permanent enchants intact. Maintenance filled **12 empty sockets
  on seven items** and activated three socket bonuses; it replaced no existing gems.
- 24 BOP-trade flags cleared. Eleven temporary-enchant records changed, including
  normal timer decreases and buff applications/refreshes.
- One Arcane Powder and two Adamantite Sharpening Stones consumed; two Brilliant
  Mana Oil items each used one charge. These match reagent/buff activity; no exact
  per-action trace was captured and no assistant maintenance command was issued.
- Levels/XP, roster, quests, talents/spells/skills, spec groups and pet spells exact.
- All 19 pet identities retained. Three active pets changed only health/mana,
  happiness and save time; no pet spell/identity replacement.
- **Raney's 14 saved profile/preference rows disappeared after startup**, repeating
  the earlier observation. His talents, learned spells, equipment and spec did not
  disappear. Cause remains untraced; no profile restoration or default override.
- Raid-group loss remained. Both failures remain recorded as failures, not silently
  accepted because they had happened on previous deployments.

### Raid progress

Stored instance/bind/progression/respawn data matched exactly through observation:

- **TK 6510:** mask 7, `T E 3 3 3 0 ` — **3/4 retained**.
- **Karazhan 106:** mask 720, `K Z 0 0 0 5 5 3 3 0 3 3 3 0 ` retained.
- Existing reset deadlines untouched. Kara's normal deadline was October 8
  **15:50:10 UTC**; TK's was October 10 **16:05:06 UTC**. These are observation-time
  facts, not a promise those saves remain indefinitely.

The previous October 5 unexplained spare-wand loss is **not resolved or restored** by
this successful item-identity comparison against a fresh October 8 baseline.

## Other runtime observations

Startup retained the process-priority permission warning. Captured runtime logs
contained 194 action-button cleanup warnings (23 in the readiness snapshot), none
for monitored characters. Three duplicate `pet_spell` inserts
involved pets 3207, 3320 and 78346, owned by characters 899, 299 and 9 respectively,
all outside the monitored roster. No repair was attempted; these are not hidden by
calling the server ready.

## Evidence and acceptance distinction

Private evidence: `backups/raid-fixes-deploy-20261008-171506/`, pointer
`/tmp/raid-fixes-oct08-backup`. It contains executable preparation/build/deployment/
observation scripts, dumps, manifests, logs, hashes, binary proofs, original strict
JSON audits and `preservation-review.json`. Credentials/dumps/binaries are not committed.

- Preparation/build/binary validation/deployment/infrastructure observation: passed.
- Strict startup audit: **failed for group loss**.
- `preservation-observation.exit=1`: strict preservation did **not** pass.
- Detailed review: gear, abilities, pets' identities and raid progression preserved;
  **group and Raney profile loss remain unresolved**.

No previous deployment's exception allowlist was reused. The infrastructure closeout
is not a blanket preservation waiver or a claim of live encounter acceptance.
