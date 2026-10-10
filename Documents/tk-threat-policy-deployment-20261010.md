# TK threat reference / advisor policy deployment — October 10, 2026

The user explicitly authorized fixing the threat bug, changing the TK assignments/
DPS order, and building/deploying while offline. Only the worldserver was replaced.
No Pi bridge restart, setup run, SQL restoration, boss reset, role/gear rewrite or
encounter command was performed.

See [feature, safety boundaries and tests](raid-threat-owners-and-tk-policy.md).

## Published implementation

- Playerbots: `fa9f3eb52f3fbf1aa069773061cc00dcd1ebec78`.
- Root policy/tests/pin: `908ccda`; invariant-test follow-up: `25a1655`.
- Core remains `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
- Generic boss protection now references the highest-threat eligible living tank
  **on that boss**, preserving the explicit Twins owner and existing threat math.
- Revived advisors: human MT holds **Sanguinar + Capernian**; existing first-OT
  Telonicus assignment remains (Ari in this group). Eligible DPS share
  **Thaladred → Telonicus → Sanguinar → Capernian**. The policy recognizes roles,
  not hardcoded character GUIDs. Human actions remain manual.
- Capernian finisher positioning is limited to non-tank melee before Kael becomes
  active; it does not force melee into her burst. See the feature document for
  observation-gap, native-fallback and active-Kael release qualifications.

No `.raidthreat` override was issued. Defaults/configs are unchanged; existing
per-instance `.raidthreat` overrides are memory-only and cannot survive a restart.

## Preflight and build

Private evidence: `backups/tk-threat-deploy-20261010-214528/`.

- Fresh four-database dump, env/config backup, managed-character/gear/pet/raid
  snapshots and updater ledgers. Non-random-bot online count was zero, checked
  again immediately before stopping. No encounter was `IN_PROGRESS`.
- 4,213 source records matched the frozen manifest, with **exactly nine intended
  playerbots source-file changes** from the running MC build. Root-owned module
  mirrors matched; other native forks were unchanged.
- Initial 128-test run:126 passed, two whole-source preservation checks failed
  because the requested extensions intentionally changed their frozen bodies.
  They were updated to retain exact comparisons of the old threat math and full
  legacy advisor-order fallback, with dedicated production tests covering the new
  behavior. Both corrected checks passed independently, then **all128 tests passed
  together in367.913 seconds**. No tests were skipped in this deployment suite.
- Original failure artifacts remain: `prepare.exit=1`, `tests-initial.log`.
  Corrected preflight: `prepare-final.exit=0`, `tests-accepted.exit=0`.
  Earlier syntax/fixture failures also remain in the private implementation folder.
- Six production translation-unit syntax checks passed. Official C++ style found
  no new violations (34 existing qualifier diagnostics before and after).
- Build22:05:26–22:06:34 CEST;16 compiler warnings, identical to the prior MC build.
  The staged image was built without touching the running container or master tag.
- Offline symbol/disassembly checks verified the new reference, ownership resolver,
  TK preference consumers and parser, plus retained MC/proc/scaling and other
  existing fixes. Dependencies resolved. `build.exit=0`, `build-validation.exit=0`.

## Runtime identity and Lua publication

| Item | Value |
| --- | --- |
| Staged/live tag | `acore/ac-wotlk-worldserver:tk-threat-20261010-214528` |
| Compose alias | `acore/ac-wotlk-worldserver:master` |
| Image | `sha256:6a2c79b7c5ce0f8969cf7b5ee871f7c5235f2d4d743f87b5d54ee5d67cffb009` |
| Worldserver SHA256 | `47fb02e479c9c4c122beec040e7235f802cfe6e6207f704eb5ef372ecc0c4cc6` |
| Stop requested |22:07:47 CEST; clean exit0, no OOM |
| Container started | `2026-10-10T20:08:06.805977193Z` |
| Ready | **22:08:29 CEST** (`20:08:29.319238666Z`) |
| Image rollback tag | `acore/ac-wotlk-worldserver:pre-tk-threat-20261010-214528` |

After a fresh stopped-state dump/snapshot, the checked Lua default was published
while the old runtime was stopped. The standalone production-linked checker was
refreshed from the same source. The new image was started with
`--no-deps --no-build --pull never --force-recreate ac-worldserver`.
`deploy.exit=0`. The running executable hash exactly matched the inspected image.
The unchanged core banner alone is not evidence of the module version.

- New Lua SHA256:
  `8f322f0878e85fcf89c55ba82484891d68a56ff4c52b5829d7a32322658d8791`.
- Default publication:
  `d503f67e6b49030d:8f322f0878e85fcf89c55ba82484891d68a56ff4c52b5829d7a32322658d8791`.
- Retained prior Lua SHA256:
  `03f9e35d9345878b40a3beca5ea5e1a38cfad0221974711ec05bec9f5672b2f8`.
- Runtime manifest, retained source and configured read-only mount were verified
  inside the live container. The installed checker hash matched the tested build.
- Exactly two policy-store files changed: the default manifest and one new immutable
  revision. All48 prior config/env/flake/policy records were otherwise unchanged;
  the resulting manifest has49 records. Checker replacement is recorded separately.
- Authserver, database, importer/client-data helpers and Pi bridge retained their
  container/service identity, start times and restart counts. Pi tags remained healthy.

Re-publishing the retained prior Lua revision using the checked publisher can
restore the native advisor policy without removing the threat fix. Adoption still
requires a safe boundary. Image rollback requires separately authorized interruption
and the matching prior Lua default; do not restore old database snapshots casually.

## Fresh raid baseline and immediate preservation

This deployment uses the **current** saves, not the obsolete October8 tuples:

| Save | Recorded state | Progress metadata |
| --- | --- | --- |
| SSC1734 |mask63, `S S 3 3 3 3 3 3 ` |stage3, reset1791691200, extended1791777600 |
| TK2510 |mask7, `T E 3 3 3 0 ` |stage2, reset1791908978, extended1792168178 |

These are saved completion flags, not proof of unassisted clears. Both raids'
progress, deadlines and binds matched exactly across startup, as did gameobject
respawns/global resets. No reset SQL or deadline restoration was issued.

For43 managed characters, stopped→startup:

- **2,214 items retained exactly**, no new/missing/moved items or item-field changes.
- **727 equipped identities**, existing gems and permanent enchants unchanged.
- Levels, roster, quests, profiles, pets/pet spells, specs and saves matched exactly.
- Raid group3520 disbanded, consistent with accepted `KeepAltsInGroup=0` behavior.
- Raney's skill maxima for129/182/185/356 (first aid, herbalism, cooking, fishing)
  rose350→450; earned skill stayed350. Spells/talents/specs were not rewritten.
- The strict startup audit remains **`accepted=false` with two findings** (abilities
  and group rows). No allowlist, waiver, restoration or second restart was applied.

Before→stopped was not a globally static interval:15 item records changed
(10 temporary-enchantment records, four stack counts and one charge record), plus
pet state. No identities, inventory positions, equipment, existing gems or permanent
enchants changed in that interval. The fresh stopped snapshot is the deployment
comparison baseline, not a claim that offline-master bots performed no activity.

## Post-start observation

The 380-second observation crossed the five-minute save/maintenance interval.
Infrastructure checks passed: same healthy new container, no restart/OOM, matching
image/binary, unchanged source/configs (apart from the declared publication), stable
updater ledgers and untouched other services. `observation.exit=0`.

Stopped→later snapshot still retained **all 2,214 item identities, all inventory
positions and 727 equipped identities**. Existing gems/permanent enchants, raid
progress/deadlines/binds, levels, roster, quests, spells, specs and pet spells remained
unchanged. No SQL restoration was performed.

The later audit is nevertheless **`accepted=false` with 12 findings**;
`preservation-observation.exit=1` remains intact:

- Raney's **14 saved preference rows** disappeared after startup: three engine
  strategies and eleven values, including formation `chaos` and RTI `skull`.
  This known persistence problem is separate from allowed group disbanding and
  has not been waived or repaired here.
- Group3520 and the four skill-max changes above remain in the comparison.
- Feelesia's cat changed health, happiness and save timestamp; its identity,
  action bar, spells and autocast flags were retained.
- Eight existing pieces acquired **14 gems in formerly empty sockets**, with
  associated socket bonuses where applicable. No existing gem was replaced.
  These are consistent with the installed empty-socket maintenance, not a new
  gear-generation change in this deployment. The strict audit retains all eight
  enchantment findings rather than adding an exception list.
- Across23 item records: eight temporary-enchantment timers decreased,15 BOP-trade
  flags cleared, and the eight socket/bonus changes occurred (overlapping rows).
  No stack counts or charges changed after the stopped snapshot. The precise
  flag-clearing call path was not instrumented; do not confuse this comparison
  with complete item-state immutability.

Infrastructure success is not blanket preservation acceptance. Encounter behavior
still requires a fresh live pull; passing fixtures, readable Lua publication and
server readiness are not live Kael acceptance.
