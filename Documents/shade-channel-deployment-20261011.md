# Shade channel fix deployment — October 11

## Result and scope

The user explicitly authorized rebuilding and restarting the **worldserver** after
logging out. The Shade of Akama channel-admission fix is now live. Readiness was
observed at **22:00:02 CEST / 20:00:02.726852726Z**.

Only the worldserver was stopped/recreated. Authserver, database, importer helper,
client-data helper and Pi bridge retained their identities/start times or service
invocation. No setup, SQL restore/migration, raid reset, Lua publication, config
change, gear grant or live test pull was performed. No second restart was needed.

Infrastructure verification passed. **Strict preservation did not pass**: the
existing group/preset issues and bounded maintenance differences are documented
below, without an exception allowlist or automatic restoration.

## Published code and runtime identity

- Core fix: `e18441e6917b8898d15532b9c4597b5e84727b34`.
- Playerbots unchanged: `1e7ee6b890767da1f3ff517b2eb97eab9bf06054`.
- Root implementation/tests/pin: `3b69938`; investigation report: `3df9b9b`.
- Candidate/live tag: `acore/ac-wotlk-worldserver:shade-channel-20261011-214841`.
- Compose alias: `acore/ac-wotlk-worldserver:master`.
- Image: `sha256:3caab244bc665cb9815bd23b2e4a8e4aa6b05189edcde27a8055c3548589e2e8`.
- Binary SHA256: `5f7112e286a093f66831e23eab0f408e03d4b04f60da0cd6678ee01733c8a6c6`.
- New container started: `2026-10-11T19:59:44.519706588Z`.
- Final process: running, restart count 0, no OOM, one readiness cycle.

Rollback tag: `acore/ac-wotlk-worldserver:pre-shade-channel-20261011-214841`.
It retains the preceding flight-casting image
`sha256:6828e979ae51dddc2a55903d22d77f4ea5695e02e9a9ab7393d4d26cdefe1e46`, binary
`fa5d2f0547cd75d61103bb333f8b6a27bbf0546af6da064ec6451dc70b11af75`.
Using it would be another interruption and needs fresh authorization; do not restore
a database merely to roll back a binary.

## Preparation, validation and switch

- Fresh four-database dumps were verified before work and after clean shutdown.
  Live env/config files, Lua payload/checker, source/config manifests and character,
  inventory, abilities, profile, pet and raid-state snapshots were retained privately.
- No real-player account was online initially, immediately before stopping, or at
  the final observation. No encounter was IN_PROGRESS.
- **4,215 source records** were compared with the preceding image's accepted source
  manifest. The sole change is
  `src/server/scripts/Outland/BlackTemple/boss_shade_of_akama.cpp`.
  Authored module mirrors matched. All **50 config/policy/checker records** remained
  identical through the final check.
- The preflight explicitly checked current TK and BT serializers. BT has ten native
  state slots: auxiliary Akama/Illidan slot 8 remains TO_BE_DECIDED=5, not an active
  encounter or a tenth raid boss. No state was normalized or altered by the gate.
- **147 selected tests passed**, no skips, in **437.108 seconds**, including the
  new Shade regressions, T5 audit and prior flight/threat/TK/scaling/loot coverage.
  Changed native-header syntax passed. Earlier focused style checks passed.
- Build: **21:56:51–21:57:29 CEST** while the old server kept running. The 16 compiler
  warnings match the preceding build after line-number normalization; none added.
- The candidate binary was copied from a created, **never-started** inspection
  container. Disassembly verifies the real channeler callback now calls
  `Unit::IsNonMeleeSpellCast` with channel checking enabled before casting 40401,
  and still repeats its scheduled check. Prior raid fixes and dependencies were
  also checked. No worldserver executable was launched for staging validation.
- The first binary validator used an overly broad `MovementInform` symbol match,
  which also matched nested lambda/typeinfo symbols. The image build itself had
  completed. The validator was corrected to the exact method signature, and the
  **same image** passed revalidation. Original `build.exit=1` and failure output
  remain preserved; `revalidation.exit` and `build-validation.exit` are 0.
- Stop requested **21:59:25 CEST**; old worldserver exited cleanly at
  **21:59:33.191 CEST**, exit 0/no OOM. The fresh stopped snapshot is the preservation
  baseline. Only worldserver was recreated with `--no-deps --no-build --pull never`.
- Deployment and infrastructure observation exits are 0. The delayed observation
  occurred after the 380-second startup deadline, beyond the five-minute save interval.

## Fresh raid baseline: preserved

These values, all character binds, global resets and saved gameobject state were
unchanged through the delayed observation:

| Instance | Map | Completed mask | Stage | Reset | Extended reset |
| --- | ---: | ---: | ---: | ---: | ---: |
| BT 711 | 564 | **7** | 2 | **1791992081** | **1792251281** |
| TK 2510 | 550 | **15** | 3 | **1791777600** | **1791864000** |

BT serialization remains `B T 3 3 3 0 0 0 0 0 5 0 `: Naj'entus, Supremus and Shade
completed. TK remains `T E 3 3 3 3 `. BT creature-respawn rows were included in this
fresh audit. No obsolete SSC/TK save or deadline was restored.

## Preservation: exact gear, qualified persistence

### Stopped → startup

For **43 managed characters**:

- All **2,255 item identities**, every inventory position and **727 equipped item
  identities** matched exactly, with no item-field changes.
- Gems, permanent enchants, levels, XP, abilities/skills, specs, roster, quests,
  saved profiles, pets/pet spells and raid saves matched.
- Strict audit was false because group **1438** disbanded. This is consistent with
  the existing `KeepAltsInGroup=0` behavior; no exception file was used to hide it.

### Stopped → delayed save

- All **2,255 identities**, all positions and **727 equipped identities** still
  matched. No missing/new/moved items, stack or charge changes, gem replacements,
  new socket gems or permanent-enchant changes.
- Levels, XP, abilities, specs, roster, quests, pet spells/autocast and raid state
  were unchanged.
- Raney lost **14 saved profile rows**: three strategies and eleven values,
  including formation `chaos` and RTI `skull`. This is the known unresolved
  persistence defect, not silently repaired or attributed to the Shade guard.
- Eleven temporary-enchant durations changed: ten decreased normally, while
  Kaaren's item **40212979 / entry 29962** refreshed the same enchant **2636** from
  284775 to 1785580 ms. No permanent enchant or enchant identity changed.
- Two expired BOP-trade flags changed from 257 to 1, as already handled by the audit.
- Three pets changed current health/mana/happiness/save times. Their identities,
  levels, actionbars and spells/autocast remained intact; individual maintenance
  events were not traced.

The strict observation result remains **false**, `preservation-observation.exit=1`,
with **four findings**: profiles, group, pets and the one temporary imbue refresh.
The read-only qualification does not turn that into full preservation acceptance.
No SQL/config restoration or further restart was performed.

### Before preparation → clean stop, on the old runtime

The fresh stopped baseline differs from the initial snapshot by ordinary-looking
maintenance that happened **before** the new binary ran:

- 2,256 → 2,255 identities: Beliona's soulstone **22116** was replaced and one soul
  shard **6265** disappeared. Exact casts were not captured.
- Two reagent stacks decreased: item 17029 by one, item 21177 by five.
- Eleven temporary-enchant duration changes and pet runtime/save-field updates.
- Equipped identities, inventory positions of retained items, existing gems,
  permanent enchants, abilities/profiles and raid state remained intact.

## Logs and evidence

The process-priority permission warning remains; no crash/assertion/OOM, missing
runtime dependency or extra readiness cycle was observed. The available observation
log did not show the prior deployments' duplicate `pet_spell` inserts.

Private evidence: `backups/shade-channel-deploy-20261011-214841/`, pointer
`/tmp/shade-deploy-backup`. It contains original failures as well as corrected
validation, snapshots, dumps, disassembly, manifests, audits and service identity
checks. `SHA256SUMS` covers retained evidence; `repo-status-final.txt` records remote
publication verification.

See [the investigation and fix](shade-of-akama-stall-20261011.md) for the source
failure chain, user-confirmed `.unaura 40520` recovery, controlled-test limitations
and unchanged mechanics. The recovered pre-fix kill was GM-assisted. No new live
Shade pull has been used to claim post-fix encounter acceptance.
