# Raid versus dungeon health and damage: first measured baseline

Date: 2026-09-09. Research only; **no tuning, configuration, database writes or
service changes**. Pet-taunt implementation was completed before this study.

## Important distinction: server evidence versus Blizzard history

The reproducible numbers below are computed from **this server's unscaled
AzerothCore world templates and class/level stats**, using its actual damage
formula. They describe Vanilla-content and Wrath-content baselines in a 3.3.5a
implementation. They are **not authenticated original-2005 Blizzard combat-log
measurements**, and Vanilla dungeons can have later-era retuning in this database.

I did not find a published universal Blizzard "40-player -> 25-player" health or
damage multiplier. Blizzard's archived [Magtheridon's Lair: An Inside Look][blizzard]
describes redesigning encounters for smaller rosters (including fewer guaranteed
class utility abilities), not applying one universal numerical conversion.

Public databases are useful crosschecks, but are not interchangeable damage
measurements. For example:

- [ClassicDB Chromaggus][classic-chrom] lists 1,832,050 HP and **15,715–18,235** damage.
- The local unbuffed Chromaggus template computes the same HP but **3,295–4,368**
  raw main-hand damage. Another public [Wrath database][evo-chrom] lists
  **3,844–5,096**. These are materially different, not rounding errors.
- [ClassicDB Rivendare][classic-baron] lists 47,064–47,866 HP and 925–1,103 damage;
  the local template computes 46,620 HP and 351–466 damage.
- Conversely, local heroic [Loken][loken] matches WotLKDB's 512,278 HP and
  5,951–8,200 melee, and local 25-player [Anub'Rekhan][anub] matches its
  6,763,325 HP and 32,840–45,236 melee.

Consequently, I have **not mixed ClassicDB's displayed damage with AzerothCore's
Wrath formula** and called the result Blizzard's design. The exact historical
Vanilla melee comparison remains unverified. These local ratios are directly
relevant to our configuration, but are only a first pass at the historical question.

## Method

Read-only snapshot of `acore_world.creature_template` joined to
`creature_classlevelstats` by creature class and maximum template level. Modes are
resolved explicitly: Naxxramas 25-player normal uses the difficulty-1 templates;
Wrath heroic five-player dungeons use their heroic templates, not their normal
entries. ICC uses 25-player normal, not heroic. This is original-style 3.3.5a data,
not the later Wrath Classic Naxxramas buff or Titan Rune dungeon variants.

We compare **boss to boss** and **trash to trash**, not a raid boss to one ordinary
dungeon mob. Ratios are ratios of medians within small, named illustrative samples,
not claims about every NPC in either expansion. The samples are hand-selected and
not a random/statistically representative survey. In particular, caster bosses'
low melee values can make a dungeon baseline look low; much of their damage is in
spells excluded here. Raid tier also matters: MC and Naxx are entry raids; BWL and
ICC are later progression, not identical relative tiers.

The baseline is main-hand white damage with neutral modifiers:

```text
HP = ceil(baseHP[expansion] * HealthModifier)
APterm = attackPower / 14 * BaseVariance
seconds = BaseAttackTime / 1000 (zero is normalized by core to 2000 ms)
minimum swing = (baseDamage[expansion] + APterm) * DamageModifier * seconds
maximum swing = (1.5 * baseDamage[expansion] + APterm) * DamageModifier * seconds
mean swing = (minimum + maximum) / 2
raw melee DPS = mean swing / seconds
```

Global rank rates are treated as 1. No custom raid scaling is applied until the
explicit final comparison. These are nominal pre-mitigation stat lines: **not
observed incoming damage**. Armor, block, misses, dodge/parry, crits/crushing,
offhand attacks, attack interruptions, boss abilities, auras, frenzy/enrage,
resistances, encounter adds and damage-sharing mechanics are excluded. The attack
speed multiplier in this core matters: a faster template can hit for less per
swing without reducing its computed baseline DPS.

Sources for the formula at core commit `112d363423f6a933f8e708eb45951129ffe2a109`:

- [CreatureBaseStats::GenerateHealth / GenerateBaseDamage][health-code]
- [Creature::SelectLevel][select-code]
- [Creature::CalculateMinMaxDamage][damage-code]
- [Unit::GetAPMultiplier][speed-code]

## Boss ratios

| Sample / dungeon baseline | HP ratio | Mean melee hit ratio | Raw melee DPS ratio |
|---|---:|---:|---:|
| MC 40 / level-60-content 5-player bosses | **21.26x** | **3.96x** | **3.96x** |
| BWL 40 / level-60-content 5-player bosses | **47.16x** | **6.60x** | **6.60x** |
| Naxx 25 / level-80 normal 5-player bosses | **19.11x** | **8.77x** | **9.75x** |
| Naxx 25 / level-80 heroic 5-player bosses | **14.84x** | **4.97x** | **5.52x** |
| Native Naxx 10 / level-80 heroic 5-player bosses | **4.90x** | **2.48x** | **2.76x** |
| ICC 25 normal / ICC-era heroic 5-player bosses | **35.51x** | **9.44x** | **14.05x** |

Named samples:

- Vanilla-content dungeons: Darkmaster Gandling, Baron Rivendare, Balnazzar, King
  Gordok (Scholomance/Stratholme/Dire Maul). No UBRS: it was not a five-player
  Vanilla baseline. NPCs are levels 61–62; "level 60 dungeon" means player cap.
- MC: Magmadar, Golemagg, Garr. Adds are not included in these boss stat lines.
- BWL: Ebonroc, Chromaggus, Nefarian. Use one drake rather than triple-weighting
  the identical Ebonroc/Flamegor/Firemaw templates. Exclude Razorgore's eggs and
  Vaelastrasz's special starting-health/resource setup from a simple HP comparison.
- Wrath normal dungeons: Loken and King Ymiron, from cap-level normal dungeons.
- Wrath heroic dungeons: Loken, Ymiron, Gal'darah, Sjonnir.
- Naxx 10/25 matched bosses: Anub'Rekhan, Maexxna, Patchwerk. **Hateful Strike is
  additional to Patchwerk's white swings and is not counted here.**
- Late-tier sensitivity example: Marrowgar/Festergut against heroic Pit of Saron
  bosses Garfrost/Ick/Tyrannus. This tiny sample is not an ICC average. Marrowgar's
  Bone Slice and Festergut's damage stacks need separate encounter-level analysis.

The heroic versus normal dungeon denominator substantially changes the answer.
A 40-player raid has eight times a dungeon's headcount, but a boss can have far
more than eight times its HP: raid DPS composition, gear and intended fight length
also change. Per-tank damage is not distributed evenly across all raid members.
More healers do not imply that all of them are healing the same tank continuously.

### Concrete comparison: Chromaggus versus King Gordok

Local unscaled values:

| NPC | HP | Mean raw hit | Interval |
|---|---:|---:|---:|
| King Gordok | 29,133 | 753.25 | 2 s |
| Chromaggus | 1,832,050 | 3,831.70 | 2 s |

That is approximately **62.9x health but 5.09x melee damage**, before spells,
mitigation and other encounter behavior. Against Rivendare instead, the ratios
are about **39.3x HP and 9.38x melee**. This denominator sensitivity is why one
boss pair must not become a universal scaling law.

## Trash ratios: a distinctly different picture

| Sample / dungeon trash baseline | HP ratio | Mean hit ratio | Raw melee DPS ratio |
|---|---:|---:|---:|
| MC 40 / Stratholme ghouls | **15.73x** | **5.37x** | **4.74x** |
| BWL 40 / Stratholme ghouls | **24.37x** | **8.73x** | **7.27x** |
| Naxx 25 / heroic Halls of Lightning melee trash | **5.59x** | **1.15x** | **1.02x** |
| Native Naxx 10 / heroic Halls of Lightning melee trash | **2.13x** | **0.66x** | **0.59x** |

Samples: Plague Ghoul/Ghoul Ravener/Fleshflayer Ghoul; MC Molten Giant/Destroyer;
BWL Wyrmguard/Flamescale/Captain; Halls of Lightning Hardened Steel Reaver/Berserker;
Naxx Patchwork Golem/Sludge Belcher. Variable-level NPCs use their maximum level,
explicitly recorded in the full table. These are **individual mobs, not whole packs**.

Thus "raid trash" does not imply a fixed multiple of dungeon-trash damage. Larger
packs, CC, casts, add roles and encounter design can supply difficulty instead.
The BWL sample is much more punishing relative to its local dungeon baseline than
the Naxx sample. Do not generalize two Naxx mobs to every Wrath trash pack.

## Implications for our current ten-player setting

Current ordinary automatic scaling for 10 versus 40 is:

```text
HP:     10/40 = 0.25
Damage: (10/40)^0.6 = 0.435275...
```

Applying those factors to the local ratios, with dungeon baselines unchanged:

| Scaled sample / its dungeon baseline | HP ratio | Raw melee DPS ratio |
|---|---:|---:|
| MC scaled to 10 | **5.32x** | **1.72x** |
| BWL scaled to 10 | **11.79x** | **2.87x** |
| MC trash scaled to 10 | **3.93x** | **2.06x** |
| BWL trash scaled to 10 | **6.09x** | **3.16x** |

For comparison, the native Naxx 10 boss sample sits at **4.90x heroic-dungeon HP
and 2.76x raw melee DPS**. The BWL melee multiplier relative to its local dungeon
baseline is therefore not obviously wildly low by this limited comparison.
But this is not proof of equal difficulty: class mechanics, baseline dungeon
retuning, gear, abilities, encounter length and bot behavior remain different.

For Chromaggus specifically, ordinary ten-player scaling changes the baseline
swing from approximately **3,295–4,368 to 1,434–1,901**, before armor, block or
other mitigation. Full damage would be approximately **2.30x the currently scaled
value**, not four times. That helps explain why a healed pet might survive, but
cannot establish whether it would survive a full-damage encounter. No actual pet
combat log or incoming-damage trace was captured for this study.

### A closer size-only crosscheck: native Naxx 10 versus 25

Across the three matched templates:

- 10-player HP is approximately **33%** of 25-player HP, not 40%.
- Baseline main-hand melee DPS is **50%** of the 25-player value.
- Mean swing is 50% for Anub/Maexxna, but 31.25% for Patchwerk because his local
  10/25 templates also use different attack intervals.

A size-power curve fitting that DPS pair would have exponent
`log(0.5)/log(10/25) ~= 0.756`, versus our chosen 0.6. Extrapolating it to 10/40
would produce about **35% damage**, actually *less* than our current 43.5%.
That extrapolation is **not a recommendation**: it illustrates why "the pet could
tank it, so the exponent must be wrong" does not follow from these ratios alone.

## Conclusions and remaining work

1. **Health and damage do not follow the same ratio.** The local sampled raid
   bosses have roughly 15–47x dungeon-boss HP in the main comparisons, but only
   about 4–7x raw melee DPS when using the appropriate heroic Wrath baseline.
   Normal-dungeon and late-ICC comparisons fall outside that damage range.
2. **Bosses and trash need separate evaluation.** Our current scaled BWL trash
   remains relatively severe even when boss melee looks manageable.
3. **Healer count alone is insufficient.** Measure main-tank damage, raid-wide
   damage, bursts and required simultaneous tanks separately.
4. Do not retune from a single pet-tanked kill. First collect representative
   incoming-damage logs and check positioning, pet threat and actual adds/CC.
5. Original Blizzard-era Vanilla damage remains a source-validation question.
   Archived logs with known armor, auras and attack intervals (or a validated
   historical dataset) are needed before labeling any raw ratio definitive.
   TBC-specific numerical cohorts have not been added; the Blizzard TBC article
   is design context, not a numerical BC measurement.

**No recommendation to change the current settings yet.**

## Reproduction and artifacts

- [Frozen non-private template snapshot](data/raid-size-template-snapshot-20260909.tsv)
- [Full generated results](data/raid-size-computed-ratios-20260909.md)
- Calculator: `scripts/analysis/raid_size_ratios.py`
- Tests: `scripts/tests/test_raid_size_ratios.py` (3 passed)

```bash
python3 scripts/analysis/raid_size_ratios.py
python3 -m unittest discover -s scripts/tests -p test_raid_size_ratios.py -v
```

Data was read from the running world database on 2026-09-09, not from modified
creatures in a loaded/scaled raid. No player/account data or credentials are in
the snapshot. Arithmetic is a neutral template reconstruction; it does not claim
to execute the entire combat engine. Minor float precision/rounding differences
from C++ are immaterial to the rounded ratios.

[blizzard]: https://warcraft.wiki.gg/wiki/Magtheridon%27s_Lair:_An_Inside_Look
[classic-chrom]: https://classicdb.ch/?npc=14020
[classic-baron]: https://classicdb.ch/?npc=10440
[evo-chrom]: https://wotlk.evowow.com/?npc=14020
[loken]: https://wotlkdb.com/?npc=31538
[anub]: https://wotlkdb.com/?npc=29249
[health-code]: https://github.com/CmPons/azerothcore-wotlk/blob/112d363423f6a933f8e708eb45951129ffe2a109/src/server/game/Entities/Creature/CreatureData.h#L313-L335
[select-code]: https://github.com/CmPons/azerothcore-wotlk/blob/112d363423f6a933f8e708eb45951129ffe2a109/src/server/game/Entities/Creature/Creature.cpp#L1500-L1555
[damage-code]: https://github.com/CmPons/azerothcore-wotlk/blob/112d363423f6a933f8e708eb45951129ffe2a109/src/server/game/Entities/Unit/StatSystem.cpp#L1116-L1173
[speed-code]: https://github.com/CmPons/azerothcore-wotlk/blob/112d363423f6a933f8e708eb45951129ffe2a109/src/server/game/Entities/Unit/Unit.cpp#L13633-L13638
