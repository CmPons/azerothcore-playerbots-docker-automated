# BC and Wrath progression raid resets

## Status

Implemented and offline-tested October 1, 2026. Core commit:
`ab00bd5c78071f80a69c06d06ac2bb33b6bbd880`. Build/deployment subsequently authorized
by the user, but **not yet deployed** at this source-publication checkpoint.
The running server still needs the new binary; keep existing lockouts extended until
that deployment is verified.

This extends [the existing Classic policy](raid-progression-resets.md), rather than
adding a second reset mechanism or changing `Rate.InstanceResetTime`.

- Empty copy: next configured daily reset.
- First successful encounter: exactly **72 hours** from that completion at the current
  `Instance.ProgressionReset.Days=3` setting.
- Full clear: switch to the **next configured daily reset**, currently 04:00 UTC,
  **not** exactly 24 hours after the last kill.
- Further kills, reloads and ordinary saves do not refresh the progression deadline.
- Explicit player extensions remain supported. No change to size/difficulty sharing,
  bind ownership, boss loot, equipment, bot strategies or raid scaling.

## Completion catalogue

All regular encounters below are required, not merely the final boss. The same decoder
applies to every supported difficulty; the existing save manager keeps each instance
ID and its native difficulty/bind semantics separate.

| Map | Raid | Required encounters | Optional / ignored slots |
| --- | --- | --- | --- |
| 532 | Karazhan | Ten: Attumen, Moroes, Maiden, Opera, Curator, Aran, Terestian, Netherspite, Chess, Prince | Animal boss and summoned Nightbane optional |
| 534 | Mount Hyjal | All five bosses | — |
| 544 | Magtheridon | Magtheridon | Channelers are not separate completions |
| 548 | SSC | All six bosses | — |
| 550 | Tempest Keep | All four bosses | Kael's advisors/weapons are not separate completions |
| 564 | Black Temple | All nine bosses | Akama's door-event slot 8 ignored |
| 565 | Gruul | Maulgar and Gruul | Council members are not separate completions |
| 568 | Zul'Aman | All six bosses | Rescue timer/chest counters ignored |
| 580 | Sunwell | All six bosses | Felmyst door-event slot 3 ignored |
| 533 | Naxxramas | All fifteen encounters | Horsemen count as one encounter |
| 603 | Ulduar | All thirteen regular bosses | Algalon optional; keepers still required; hard-mode flags/timer ignored |
| 615 | Obsidian Sanctum | Sartharion | Drakes may start progression but are not needed for a clear |
| 616 | Eye of Eternity | Malygos | — |
| 624 | Vault of Archavon | Archavon, Emalon, Koralon, Toravon | Legacy four-state save format |
| 631 | Icecrown Citadel | All twelve bosses | Svalna, Sindragosa gauntlet and Blood Prince trash slots ignored |
| 649 | Trial of the Crusader / Grand Crusader | Complete the sequential raid through Anub'arak | Uses its explicit progression checkpoint, not a boss-state array |
| 724 | Ruby Sanctum | Baltharus, Saviana, Zarithrian, Halion | Three Halion introduction flags ignored |

Onyxia (249) was already included and remains covered in its Wrath form. Together
with the existing six map IDs, this covers **23 raid maps**. Normal/heroic dungeons,
world maps and battlegrounds remain outside the policy.

An optional boss can start the progression clock without being required for a clear.
Internal door/intro/trash flags can do neither. Hard-mode achievements are not required.
These choices follow the existing ZG optional-summon convention; the table explicitly
states what “full clear” means instead of deriving it from a client kill-credit mask.

## Save-format handling

Production policy: `azerothcore-wotlk/src/server/game/Instances/ProgressionRaidReset.h`.
Layouts are verified against the installed fork's encounter enums, `DataHeader`,
`SetBossNumber` and actual save writers. No generic guess based on encounter count,
final-boss entry, kill-credit mask or a modern upstream layout is used.

- Most maps serialize spaced header characters and boss-state slots. Only `DONE=3`
  counts. Appended counters, attempt limits, timers and persistent data are ignored.
- Vault of Archavon has no base boss-state array: its writer appends four legacy
  encounter states immediately after `V A`. Those four positions are explicitly mapped.
- Trial has no base boss-state array either. Its `T C R` scalar means:
  - 0/1: no successful encounter yet (including the intro).
  - 2/3/4/6/8/9: progression started; 9 only makes Anub'arak available.
  - 10: Anub'arak defeated, full clear.
  - Unknown/malformed checkpoints fail closed. Heroic attempts/achievement flags are
    trailing data, never confused with boss completion. Individual Beasts sub-boss
    kills do not complete the Northrend Beasts encounter.

### Karazhan Chess persistence

Previously, Chess wrote only `_chessEvent` in memory, leaving its allocated persisted
boss slot unused. Requiring that slot without fixing its writer would prevent normal
Karazhan clears from ever reaching the daily schedule.

The first **PvE IN_PROGRESS → DONE** now records `DATA_CHESS_EVENT=DONE` and saves it.
A still-uninitialized `TO_BE_DECIDED` slot needs an explicit save because core
`SetBossState` treats its first assignment as initialization and otherwise does not save.
Friendly replays, losses and repeated notifications do not erase or fabricate a victory.
An encounter-in-progress override also defers expiry during PvE or friendly Chess.
This does not redesign Chess movement, replay controls, loot or its UI.

Old saves whose Chess result was never serialized cannot be reconstructed reliably.
They receive the conservative partial-save treatment; a genuine PvE Chess victory on
that copy supplies the missing checkpoint. No historical kill is invented.

## Existing saves and deployment

No new database schema/migration or runtime config change is needed: the existing
`instance_progression_reset` table and enabled configuration are reused.

The unchanged save-manager lifecycle adopts newly supported saves at startup:

- Existing unfinished saves receive **one fresh 72-hour grace window at adoption**,
  because an original first-kill timestamp is unavailable.
- Existing clears and empty saves get the next daily deadline.
- Unreadable data receives the conservative progression window, not a guessed clear.
- Already-managed deadlines are restored unchanged; they are not refreshed.
- Character permanent-bind and extension flags are loaded as before. An unconsumed
  extension follows the managed policy (three more days for progression, one daily
  cycle for a clear/empty copy).
- Adoption metadata is persisted before logins. Global daily resets skip managed
  copies. Expiry defers during combat and uses the existing per-instance cleanup.

Implementation does not touch live saves, character items or configs. Deployment must
back up and verify the **then-current** save/bind/respawn records, especially SSC, before
and after replacing only worldserver. A source commit does not protect a live save from
the old daily reset before deployment; use the client extension meanwhile.

## Verification

```sh
python -m unittest scripts.tests.test_progression_raid_reset \
  scripts.tests.test_expansion_raid_reset -v
python scripts/tests/raid_combat_syntax.py --output /tmp/expansion-reset-syntax \
  src/server/game/Instances/InstanceSaveMgr.cpp \
  src/server/game/Instances/InstanceScript.cpp \
  src/server/scripts/EasternKingdoms/Karazhan/instance_karazhan.cpp
```

Checks include all 23 layouts; every required, optional and ignored slot; every valid
Trial checkpoint and invalid/gapped values; partial SSC; malformed/truncated saves;
exact daily boundaries; adoption/no-refresh; existing extension/lifecycle contracts;
compiled production encounter enums/save-writer contracts; and extracted production
Chess completion/combat-predicate execution. Three production-header syntax checks
cover both policy consumers and Karazhan.

The historical patch round-trip test now uses its original published core revision
`112d363423f6a933f8e708eb45951129ffe2a109`, not later maintained-fork source. Historical
patches are not regenerated or replayed onto the running tree.

The combined policy/expansion/source-workflow/scaling suite passed **29 tests**, with
one opt-in SQL test skipped (30 discovered). The expanded policy, native-enum contracts
and production Chess fixture also passed ASan/UBSan. The pre-change Chess source fails
the new runtime regression. Targeted official C++ style checks passed before and after.

Offline tests and syntax checks are not a live expiry, boss-clear or extension test.
Deployment preservation checks and subsequent real gameplay remain separate evidence.
