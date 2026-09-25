# Tank damage assistance + selected-boss pull guard — September 25, 2026

## Authorization and result

The user explicitly requested deployment of both fixes. The authorized worldserver
replacement is ready. **Normal-save preservation observation passed after explicit
review of background companion activity and the saved-party cleanup below.**
No authserver, database, importer/helper or Pi bridge restart was requested or used.

Ready: **2026-09-25T19:31:48.549505860Z**.

| Component | Accepted revision / identity |
|---|---|
| Playerbots | `39923c6843a75185f8d613a39edc576b8863a6ce` |
| Core (unchanged) | `f6c0fd3cf0d1a3a153bca530cd8efe9efd61b105` |
| Root build source | `f6b5eb6` (published before build) |
| Image tag | `acore/ac-wotlk-worldserver:raid-tank-pull-20260925-212628` |
| Image | `sha256:e41f4d7d83b20d9847fcc81cc3b1c3dc83c947abcc4afc2b510f9695a653e5d7` |
| Binary SHA256 | `fc2a8059a0997bb9fb2555769c9d5c1e9f28cfd9bd0a46222a7461851d999ee7` |
| Container | `936f05e8131f6f247563d753f02195ae76e8c6274155dfccc5035f2a86794fc4` |

## What changed

- [Covered-target damage assistance](playerbot-tank-modes.md): Ari can start an
  ordinary attack on the enemy another tank holds when she has no eligible tank
  work. Loose pickups/owned targets outrank that fallback. Automatic taunts still
  use the stricter acquisition gate; health safety and manual commands remain.
- [Selected-boss pull guard](raid-boss-selection-pull-guard.md): automatic DPS focus
  on a taunt-immune raid boss requires actual engagement with the MT. Merely
  selecting the boss or fighting unrelated trash no longer authorizes that focus.

These are generic fixes, not Hydross choreography. No boss scripts, scaling,
gear/spec changes, `.raidroster sync`, Lua publication, strategy exclusion setting
or global interception of all possible scripted pulls was introduced. Native
encounter packages can still control bots when enabled; this is not their removal.

Only six native source files differ from the running Mag-channeler image:

```text
Ai/Base/Actions/AttackAction.cpp
Ai/Base/Actions/ChooseTargetActions.cpp
Ai/Base/Util/RaidThreatUtils.cpp
Ai/Base/Util/TankModes.cpp
Ai/Base/Util/TankModes.h
Ai/Base/Value/TankTargetValue.cpp
```

## Verification and operations

- Fresh private four-database dump, both envs and runtime configs before build;
  another complete dump after clean worldserver shutdown. Dumps/checksums verified.
- Compared **4,206 source records** and **48 config/policy records**, including
  authored-module mirrors. Configuration/policy files unchanged through startup.
- 18 focused prebuild tests, then 36 expanded regression tests passed. Historical
  source fixtures reproduce the idle selector, rejected damage attack and
  selected-idle-boss pull independently. Eight production-header syntax checks
  passed; official C++ codestyle has no new findings for the source changes.
- Native image build succeeded. Candidate ELF/disassembly verifies ordinary attack
  calls `TankModes::CanAttack` and boss focus calls `Unit::IsEngagedBy`; retained
  scaling, gem maintenance, tank commands and BG queue symbols checked. Shared
  library dependencies resolve. Running binary matches the staged binary hash.
- Human online count was zero immediately before stop; inspected saved raid states
  showed no encounter in progress. Old server exited cleanly with exit code 0.
- Recreated only worldserver using `--no-deps --no-build --pull never`. No importer,
  setup, direct gameplay SQL writes, forced save, reset, sync or restoration.
- Core and playerbots updater ledgers unchanged. Auth/database/helper container
  identities/start times and Pi service identity unchanged; Pi endpoint responds.

## Preservation review

At startup, **43 monitored characters and all 1,999 item identities/rows** were
preserved. Inventory placements, levels/XP, roster, quests, talents/spells/skills,
profiles and raid saves matched the stopped-state snapshot exactly.

**Explicit exception:** normal playerbot login cleanup disbanded saved normal
party **872**, containing Redshift (1501), Arinerica (142), and Meliah (815).
There was no saved MT flag in that party. The first strict audit rejected this
change; only those exact group rows were then reviewed and allowlisted. No manual
disband, role reset, or restoration was performed. Re-form the party/raid and set
wanted MT roles in game.

Saved instance rows preserved at startup:

| Instance | Map | Completed mask | Saved state |
|---|---:|---:|---|
| 39 | 544, Magtheridon | 1 | `M L 3` |
| 84 | 532, Karazhan | 720 | `K Z 0 0 0 5 5 3 3 0 3 5 3 0` |
| 343 | 548, SSC | 4 | `S S 0 5 3 0 0 0` |
| 2353 | 565, Gruul | 3 | `G L 3 3` |

These are observed saved states, not an assertion about how each kill was earned.
No historical empty-Mag-instance or expired-mail exception was carried over from
the preceding deployment.

After the 380-second observation window, levels/XP, roster, profiles,
talents/spells/skills and all raid saves still matched exactly. The strict first
observation audit rejected background changes; only the exact reviewed differences
were then accepted, without restoration or a second restart:

- All equipped item identities and placements, existing gems and permanent enchants
  preserved. No gear was regenerated or swapped by the deployment.
- Existing `CompanionGems` logs confirm **six empty sockets filled**: Meliah three
  (one epic), Kaaren three. These were on items whose BOP-trade flags had cleared;
  the unchanged maintenance policy skips still-tradable items. One newly satisfied
  socket bonus was applied. No existing socket content was overwritten.
- Six fresh temporary weapon enchants, four progressing temporary-enchant timers,
  seven cleared BOP-trade flags, four consumable stack decrements and one wizard-oil
  charge used. Pilbok was in Eye of the Storm under the unchanged solo-BG policy.
- Beliona's Soul Shard and Master Soulstone were replaced with otherwise identical
  items; those two inventory GUID changes are explicitly recorded. Total item rows
  remained 1,999, with **1,997 exact item identities retained**, not a claim that
  all identities remained identical after autonomous play.
- Beliona's completed quest **7562, Mor'zul Bloodbringer**, advanced to rewarded.
  This was not a quest reset or an administrator-issued completion.

No human was online during the observation. The initial strict audit failure is
preserved separately; `reviewed-audit.exit` and `reviewed-observation.exit` are zero.
Source/config manifests and updater ledgers remain unchanged, other services retain
their original identities, and worldserver has no crash, OOM or extra restart.
The pre-existing nonfatal process-priority permission warning remains. Live combat
acceptance is not claimed.

## Player follow-up

Instance strategy auto-application remains enabled. On entering SSC, between pulls,
repeat `co -ssc` and `nc -ssc` if ordinary-AI testing is desired; verify with
`co ?` and `nc ?`. This deployment does not promise permanent strategy exclusion.

Acceptance checks:

1. Merely select an unengaged boss as MT: DPS should not start via the boss-focus
   shortcut. Explicit attack/pull commands should still work.
2. Hold one enemy as Redshift: Ari should attack it without automatically taunting.
   Spawn/engage a loose add normally and verify pickup still outranks assistance.
3. Report any remaining unsolicited pull with bot name, target, active strategies
   and whether a mark/manual command was involved. Other direct scripted paths,
   pet behavior and actual proximity aggro were not globally rewritten.

## Evidence and rollback

Private evidence:
`backups/raid-tank-pull-deploy-20260925-212628/`;
pointer `/tmp/raid-tank-pull-deploy-backup`.
Includes preparation/build/deploy/observation scripts, dumps, source/config
manifests, binary/disassembly evidence and strict preservation reports.

Rollback image retained as
`acore/ac-wotlk-worldserver:pre-raid-tank-pull-20260925-212628`,
image `sha256:d672a3e5c586835de1a580cf1c13ac569256d62d2503f0f45b75078e88f077a2`.
Rollback would require separate permission and another preservation review; no
automatic rollback or database restoration was attempted.
