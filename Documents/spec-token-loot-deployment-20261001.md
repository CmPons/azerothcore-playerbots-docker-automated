# Spec-aware token loot deployment — October 1, 2026

Authorized by: **“Very good. Build and restart.”** Scope: build and replace only the
worldserver. No importer, auth/database/Pi restart, roster sync, gear reroll, forced
save, direct gameplay SQL update or restoration.

## Running build

- Playerbots: `cef0162a7202f7a10688e989c0dba5d76828dbb1`.
- Core unchanged: `f6c0fd3cf0d1a3a153bca530cd8efe9efd61b105`.
- Root build source: `370c309fa6110af2512838f9b3830d024cb5f026`.
- Image tag: `acore/ac-wotlk-worldserver:spec-token-loot-20261001-180610`.
- Image ID: `sha256:70612854d5351b8009de868b58b0a2ca0d16528e068980a007efb9649ca5d764`.
- Binary SHA256: `53a789d0a5da067611bcc8418a43eab9a5946289b3aad1ff323cb3634bba0024`.
- Container: `1cfca61ab93f9e992522a397e0d8e53670fb4bdb5b9de01f37ff0eed737986f1`.
- Started: `2026-10-01T16:11:11.591157017Z`.
- Ready: **`2026-10-01T16:11:26.978750615Z`**.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-spec-token-loot-20261001-180610`.
- Previous image: `sha256:e41f4d7d83b20d9847fcc81cc3b1c3dc83c947abcc4afc2b510f9695a653e5d7`.

The active policy is described in [Spec-aware direct token loot](spec-aware-direct-token-loot.md).
Runtime `AiPlayerbot.DirectTokenLoot.Enable=1`, `Mode=1`, quality floor 3 and debug
logging were already configured; no config edits were necessary. Existing items,
including previously obtained Gladiator pieces, are not converted retroactively.
The deployment verifies running code, not a new live boss-drop acceptance test.

## Build and interruption controls

Fresh private four-database/config backups were made before building and a fresh
four-database backup was taken after clean shutdown. Dumps and manifests were checksum
verified. Evidence is under `backups/spec-token-loot-deploy-20261001-180610/` (private).

The source manifest verifies **4,206 source records** and exactly four differences
from the previous deployed sources: the loot script, scorer source/header and config
template comments. **48 config/policy records** and all root-authored module mirrors
matched. Prior tank assistance, selection-only boss-pull protection, support scaling,
companion gems, BG behavior and other source changes remain present.

The accepted 21-test suite and seven native-header syntax checks were rerun before
building. The image was built using the existing Dockerfile/worldserver target and
pinned Ubuntu digest. Candidate disassembly confirms the loot hook calls the scorer
constructor with `forLoot=true`, performs scoring/use checks and contains the new
class/spec decision logs. Retained tank attack and boss engagement guards and required
feature symbols were checked. Dependencies resolve; the running binary matches the
candidate hash.

The first post-build inspection stopped on an ambiguous *inspection script* substring
match (`AttackAction` also matched unrelated derived action names). The native build
had succeeded. The inspector was corrected to match the exact function and all checks
passed without rebuilding or changing server sources. Original `build.exit=1` is retained;
`reviewed-build.exit=0` and `build-validation.exit=0` record the corrected inspection.

No human was online before stopping. Only completed Gruul 18572 and Magtheridon 18591
instances were saved, with no fight in progress. Old worldserver exit was 0, not killed
by the timeout. Replacement used `--no-deps --no-build --pull never --force-recreate`.
Auth, database, importer/client-data helper identities and Pi invocation remain unchanged.

## Preservation review

The audit compares against the **fresh stopped-state snapshot**, not against older
September snapshots or live gameplay before the build. It monitors 43 characters and
the 40-member roster. Initial strict failures are retained before explicitly reviewing
exact differences; there is no broad whitelist or automatic restore.

At startup:

- All 2,001 inventory placements/identities, retained item rows, levels/XP, talents,
  spells, quests, roster, profiles, raid saves/binds and saved group rows matched.
- Total owner-attributed item records changed **2,028 → 2,001**. The 27 removed rows
  were **attachments in four expired mails**, not equipped/bagged/banked items.
  Offline parsing of the stopped dump matched every removed GUID to mail IDs
  10338–10341, addressed to Arinerica/Meliah. All four were already-returned,
  self-addressed “problems with equipping/storing in inventory” mails, expired
  **2026-09-30 11:25:19 UTC**. Unchanged `MailMgr::ReturnOrDeleteOldMails` deletes
  expired returned-mail attachments. No mail/item restoration was performed.
- Beliona's First Aid, Cooking and Fishing maxima normalized **350 → 450**, while
  actual skill values stayed 350. Grand Master spells 45542/51296/51294 were already
  present in the stopped spell rows; native spell loading raises learned skill maxima.
  No spell/talent grant, level change or direct skill edit was performed.

After the 380-second normal-save observation (no forced saves):

- Equipped item identities and placements, existing gems and permanent enchants are
  exact. Levels/XP, roster, profiles, talents/spells, quests and both raid saves/binds
  remain unchanged. Final item count is **2,000 retained identities**, with no new items.
- The unchanged `CompanionGems` routine filled **13 previously empty sockets** on seven
  equipped items: Ari 4, Ailina 4, Keilmere 3, Kaaren 1, Feelesia 1. Logs report two epic
  gems. Two socket bonuses activated. No preexisting socket was overwritten.
- Beliona's one-use **Master Healthstone (22103)** was removed from bag slot 24 during
  ordinary bot activity. Two healing consumables decreased by one each; Ari consumed
  11 Symbols of Kings. Pilbok's Netherblade chest lost one durability point. These are
  exact, reviewed activity differences, not automatically ignored inventory changes.
- Eleven temporary-enchant timers decreased/expired and 23 BOP-trade flags expired.
- Saved normal party **38010** (Redshift, Ari, Meliah) disbanded through the existing
  bot-login group cleanup after the initial snapshot. No saved MT flag was present.
  Reform the group and set MT as needed. No raid save was reset.
- Ari was in Eye of the Storm; Beliona and Pilbok were in Arathi Basin under the unchanged
  solo-BG policy. No human was online during the observation.

`audit-after-start-first-strict.json` and `audit-observation-first-strict.json` preserve
initial strict rejections. Exact mail/skill and activity exceptions are in separate
reviewed JSON files. The reviewed startup/observation audits accepted every enumerated
change; `reviewed-audit.exit=0` and `reviewed-observation.exit=0` establish closeout.
The original observation exit 1 is intentionally retained, not hidden or overwritten.
No restoration, compensating grant, broad ignore rule or second restart occurred.

Final source/config/updater ledgers and other service identities matched. The accepted
container remained running with restart count 0, no OOM/crash and a single ready line.
The complete evidence/dump checksums and closeout metadata are retained privately.

Startup logs include existing level-70/Wrath quest/skill-condition warnings, invalid
random-bot shaman action-button cleanup, and the process-priority permission warning.
These paths were not changed by the token patch. The startup mail processor reported
11,053 expired mails server-wide; the 27 monitored attachment records above are the
specific preservation exception, not a claim of database-wide byte equality.
