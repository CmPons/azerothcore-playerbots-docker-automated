# Kael MC deployment — October 9, 2026

Explicitly authorized implementation/build/deployment while Redshift was offline.
Behavior and test boundaries: [Kael mind control](kael-mind-control.md).

## Published source and image

- Playerbots: `e04c1b785c40325c0d6d557cd002362871d9ddeb`.
- Root feature/tests/pin: `1d0ea1d`.
- Core remains `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
- Candidate/live tag: `acore/ac-wotlk-worldserver:kael-mc-20261009-220252`.
- Image: `sha256:866717810478d526d8de38610d5a97a1e86afe6bb981ec3d5a59559a75af07ec`.
- Binary SHA256: `ba0cded45934eda106d21354d5226d90f001b9cb73f97797f6bf92bfe472ddea`.
- Container: `3e368e3e843623273981f9121940d70d0b0c28d4c489ea9478c25b0997a6f78c`.
- Rollback retained: `acore/ac-wotlk-worldserver:pre-kael-mc-20261009-220252`.

The live binary hash matches the inspected image binary. The Compose `master`
alias now points to this image; the unchanged core revision banner alone is not
proof of which module build is running.

## Validation and operation

- Verified four-database dumps before build and after clean shutdown; fresh
  human-online checks, including immediately before stopping, returned zero.
- **115 tests passed in 306.080 seconds**, including ten new MC tests, existing
  recovery, threat/emergency healing, tank authority, advisor/respawn/support
  scaling, loot and selected Twins regressions. This is a named focused suite,
  not a claim that every repository test passes.
- Five changed translation units passed native syntax checks.
- Official scoped style comparison found no new violations: existing west-const
  diagnostics decreased 66→64. Sixteen compiler warnings match the prior build.
- Source manifest: **4,212 records**, with exactly the eight intended changed/
  new native module files versus the running threat-controls build. Root-module
  mirrors match. All **48 runtime config/policy records** remained unchanged.
  The new MC cap uses its compiled default1; no live config edit was needed.
- Native image built **22:09:33–22:10:34 CEST**. Binary inspection checked the
  new scaling hook, target discovery, equipment admission/swap, rescue/loot
  callers and diagnostics, plus retained fixes and dependency resolution.
- Initial private binary validation incorrectly expected a string-returning
  symbol without its `[abi:cxx11]` suffix. The disassembly already contained the
  direct call. The corrected ABI-aware direct-call assertion and all remaining
  checks passed against the **same image**. Original `build.exit=1`/failed log
  remain; `build-revalidation.exit=0` and `build-validation.exit=0` record the
  correction, not a waived check. Initial test-double compile failures likewise
  remain in the investigation evidence alongside the successful final runs.
- Only worldserver was stopped/recreated, using `--no-deps --no-build --pull never`.
  Container started **22:12:44.886 CEST**, ready **22:13:04.043 CEST**.
- No importer/setup, SQL repair/migration, roster synchronization, gear/strategy
  command or Pi restart was performed. Other service identity/start-state and Pi
  invocation comparisons were unchanged at startup.

## Fresh preservation baseline

The fresh TK save reflected the user's late assisted kill, not a legitimate
clear: instance6510, map550, completed mask15, `T E 3 3 3 3 `.
Its row `(6510,3,1791604800,1791691200)` and all ten permanent binds survived
shutdown/startup; only Redshift1501 had `extended=1`. No old tuple was restored.

The audit covers **43 managed characters**, not just ten equipped raid members:

- Before-build→stopped: all **2,131 item identities**, inventory positions and
  **724 equipped identities**, gems and permanent enchants retained. Old-image
  activity changed nine temporary-enchant fields, four reagent-stack counts and
  one oil charge. Three pets changed resources/happiness/save times; no pet
  identity/action-bar/spell change was found in that interval.
- Stopped→startup: all **2,131 item rows and inventory positions matched exactly**;
  equipped gear, abilities/skills, specs, roster, quests, raid saves and pets
  matched. No restoration was performed.
- **Exceptions retained:** group84 disbanded under the existing group policy, and
  **Raney lost 13 saved preference rows**: combat/noncombat/dead strategy strings
  plus ten values, including formationchaos and RTIdiamond. These are not gear,
  spells or talents. Their deletion path and exact active-default equivalence
  were not established. The original strict startup audit remains
  `accepted=false`; no exception file was used to turn it green.

## Live diagnostic check

Read-only `tkmc` requests succeeded in the new server:

| Bot | Required hand | Ability | Known |
| --- | --- | --- | --- |
| Pilbok | offhand16 | Shiv | yes |
| Kaaren | mainhand15 | Stormstrike | yes |
| Feelesia | mainhand15 | Wing Clip | yes |

All three were outside TK with no temporary blade and no rescue target, as
expected after the encounter. This confirms the deployed diagnostic and learned
abilities, **not encounter looting/equipping or a live MC break**. Action success
is still not aura-removal proof. No test fight, GM casts or gameplay commands
were issued.

## Post-save observation

The bounded **380-second** observation finished successfully for infrastructure:
worldserver remained on the verified image with zero restarts/OOMs, one readiness
line, and no human online. Other containers and Pi remained unchanged. Source,
config/policy manifests and updater ledgers stayed stable.

After the normal save interval:

- **724 equipped identities, existing gems and permanent enchants remained
  unchanged**, as did skills/spells, specs, roster, quests and all TK progress,
  deadline and bind rows. Retained items did not move.
- Total items remained **2,131**, but not every GUID was identical: Beliona's
  single soulstone22116 and shard6265 were replaced by new GUIDs with the same
  entries/counts/positions. Old→new: `39280622→39292454` and
  `39280663→39292486`. These are consistent with conjured-item maintenance;
  no event call trace was captured. No equipped/earned gear identity was lost.
- Nine temporary-enchant fields changed (five aged, four refreshed without
  changing enchant ID). Feelesia used one23529 sharpening stone (18→17),
  Keilmere's20748 oil and Beliona's41194 spellstone each used one charge
  (-5→-4). These consumable/temporary changes were not silently excluded.
- All19 pet identities, action bars and spells remained. Three pet save times
  changed; Feelesia's cat also changed current HP/happiness.
- Group84 and Raney's13 missing preferences remained as described above.

`observation.exit=0` is **not** a full preservation pass:
`preservation-observation.exit=1`, original `accepted=false`, with12 strict
category/field findings retained. No SQL restoration, exception allowlist or
second restart was performed. Live Kael MC execution remains unobserved.

Private evidence: `backups/kael-mc-deploy-20261009-220252/` and
`backups/kael-mc-work-20261009-212842/`. Neither private dumps/configs/binaries nor
credentials are committed.
