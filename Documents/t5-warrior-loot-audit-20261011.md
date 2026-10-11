# T5 Warrior drop audit — October 11

## Conclusion

**No Warrior-specific exclusion, bad Protection reward choice, or human-only
eligibility failure was found in the current conversion path.** This is a source,
installed-data and controlled-production-code result—not proof that every past
kill rolled correctly. Available logs cannot reconstruct the historical rolls.

The investigation was read-only on the server. No rates, configuration, items,
SQL data, source gameplay behavior or services were changed. Only offline tests,
a public item-data fixture and this report were added to the workspace.

## Current character and eligibility

The saved Redshift snapshot has level 70, Human Warrior, active talent group 0.
Installed Talent/TalentTab DBC ranks resolve the active mask to **8/8/45**, Protection
(tab 2). Talent ranks come from `SpellMgr::LoadSpellTalentRanks`, not merely SQL
`spell_ranks` rows. Plate skill **293** is present. Saved data can lag the live
character; this is not an observation of every prior kill.

All ten Warrior T5 prototypes have class mask 1, allow Human race, require level
70 and no additional profession rank, spell, faction reputation or holiday.
The extra plate-proficiency check passes. `Player::CanUseItem(ItemTemplate const*)`
does not reject a dead character; `PresentPlayers` includes dead members and has
no bot-only/GM-only filter. It does require an online/in-world member in the loot
owner's group and same map instance. A missing class keeps the original token,
without rerolling its class.

`StatsWeightCalculator(player, true)` derives tank scoring from the active Warrior
Protection tab, not a playerbot AI pointer, tank role marker, current gear or threat.
The current saved inventory contains **Destroyer Legguards (30116)** and no other
Destroyer piece or Vanquished token. The core-ten ownership query found no Warrior
T5 item parked on another companion. Ownership is not a record of past drops,
unlooted items, passed rolls or previously sold duplicates.

## Real T5 routes and production replay

The installed 15 tokens resolve through `npc_vendor`/ItemExtendedCost to 170
distinct item/cost paths: 85 PvE and 85 PvP alternatives. PvP paths are excluded.
There are no T5-token quest routes or extended-cost DB overrides. The 39 PvE equip
spells have no matching core corrections or installed `spell_dbc` override rows.

For each Defender token the final class pool is exactly **Warrior / Priest / Druid**,
mask **1041**. All three RNG indices were exercised through the production selector
and full conversion hook. Two priests do not double their class probability, and
a priest loot owner does not exclude the human Warrior.

| Boss | Defender token | Protection winner | Set |
| --- | ---: | --- | ---: |
| Kael'thas | 30237 | Destroyer Chestguard, 30113 | 656 |
| Leotheras | 30240 | Destroyer Handguards, 30114 | 656 |
| Vashj | 30243 | Destroyer Greathelm, 30115 | 656 |
| Karathress | 30246 | Destroyer Legguards, 30116 | 656 |
| Void Reaver | 30249 | Destroyer Shoulderguards, 30117 | 656 |

All five tank pieces beat the DPS alternative under the actual loot scorer and
produce finite scores. Arms/Fury select the corresponding DPS pieces, set 657.
All six class orders for three identical Defender tokens produce one Warrior,
one Priest and one Druid reward. Other token families cannot roll Warrior.
A fresh corpse can repeat the same already-owned piece; there is no cross-kill
history or upgrade/catch-up guarantee.

**13 tests passed together** in 74.301 seconds: the prior 12 token tests, including
135 T4 cases, plus the new T5 test. T5 replay uses actual converter/scoring bodies
with frozen installed data and controlled world/DBC/talent/usability doubles;
ASan/UBSan passed. The resolver's database cache is represented by the extracted
vendor paths, not constructed inside a running server during the test.

## Why several clears are not several chances at every slot

The installed boss loot references first roll token families, before conversion:

- Void Reaver: reference 34054, shoulders.
- Leotheras: 34059, gloves.
- Karathress: 34060, legs.
- Vashj: 34105, head.
- Kael: 34104, chest.

Each is processed three times at current referenced-amount rate 1. Each reference
contains equal-chance Champion, Defender and Hero entries. Duplicate non-equippable
tokens are permitted up to three, so the three rolls need not include all families.
The converter preserves these family restrictions; it does not select freely
among the entire raid's classes.

With all eligible classes present and no earlier convertible reward consuming a
class, the current no-repeat rule gives Warrior a **1/3 chance of a tier piece per
such boss kill**: conditional on N Defender tokens it gets N/3 probability, and the
expected number of Defender tokens is one. This is not a guarantee, nor an estimate
of the user's actual historical luck without kill/loot records. Repeated Karathress
legs would not add a new set slot; the other four slots come from their own bosses.

### Generic Kael interaction, not a Warrior-only filter

Kael also drops Verdant Sphere 32405. Installed quest 11007 offers four epic necks,
with unrestricted class masks. The generic resolver recognizes quest rewards too.
The per-loot `rolledClasses` mask is shared by **all convertible rewards**, not only
tier pieces. Consequently, if an earlier Sphere conversion selects Warrior, a
later chest-token conversion in that same loot list cannot select Warrior again.
The same applies to other selected classes. This is consistent with the current
whole-loot class-uniqueness policy, but means a necklace can consume that class's
turn rather than a tier chest. This interaction cannot explain missing Leotheras
or Void Reaver tier rewards, and its occurrence on the recorded Kael kill is unknown.
No live loot ordering or historical Sphere selection was captured by this audit.

## Logging blind spot and confidence limits

`AiPlayerbot.DirectTokenLoot.Debug=1` enables the code's messages, but they are
`LOG_INFO("playerbots", ...)`. The installed `Logger.playerbots=3,Playerbots`
threshold is **WARN**, while INFO is level **4**. Thus that logger suppresses the
conversion decisions despite the feature's debug flag. The file appender's level
5 is not the blocking threshold. Searches of the available text logs did not find
actual conversion events; a build's string dump is not runtime evidence.

This prevents distinguishing past unlucky class rolls from an occasional missing
participant, unclaimed item or other transient failure. No logging configuration
was changed. A future bounded record of token input, selected class, winner and
participating group would be stronger evidence than inferring drops from gear.

## Provenance

- Runtime/source playerbots revision: `1e7ee6b890767da1f3ff517b2eb97eab9bf06054`.
- Production: `src/Mgr/Item/DirectTokenLootScript.cpp`, `TokenItemResolver.cpp`,
  `StatsWeightCalculator.cpp`; `AiFactory::GetPlayerSpecTabs/GetPlayerSpecTab`;
  core `PlayerStorage.cpp`, `ItemTemplate.h`, `LootMgr.cpp` and logging definitions.
- Tests: `scripts/tests/test_t5_warrior_token_loot.py`,
  `scripts/tests/cpp/T5WarriorTokenTest.cpp`.
- Public fixture: `scripts/tests/fixtures/direct-token-loot/t5-items.json`.
- Private evidence: `backups/t5-warrior-audit-20261011-170528/`, pointer
  `/tmp/t5-warrior-audit`. Raw DBCs, character records and query output stay private.
- An initial read-only Sphere query used the wrong class-column name; the schema
  lookup and corrected `quest_template_addon.AllowableClasses` query are retained.

See [the policy](spec-aware-direct-token-loot.md) for the October 1 spec selection
and October 5 per-loot class uniqueness changes. Neither was altered by this audit.
