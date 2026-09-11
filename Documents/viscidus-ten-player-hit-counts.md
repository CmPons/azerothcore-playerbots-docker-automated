# Viscidus: fixed ten-player hit requirements

## Status

**Deployed September 11 after separate explicit deployment permission.** This
adjustment, the [Bug Trio reset fix](aq40-bug-trio-wipe-reset.md), and the
[Huhuran cap](huhuran-ten-player-sting.md) are now in the running binary.
See [deployment record](aq40-deployment-20260911.md); in-game validation remains.

- Core commit: `267f29e60` (on top of trio fix `63aa5aadd`).
- Core fork branch: `CmPons/azerothcore-wotlk:fix/aq40-ten-player-encounters`.
- Canonical patch: `patches/0023-core-viscidus-ten-player-hit-counts.patch`.
- Changed core file:
  `src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_viscidus.cpp`.
- Source backup/test logs:
  `backups/viscidus-hit-counts-preparation-20260911-181634/`.

## Approved adjustment

The existing health/damage conversion did not scale Viscidus's fixed hit
counters. With one mage, the frost-count requirement can outlast the ordinary
health pool by a large margin. These are raid-wide qualifying hit counts, not
necessarily one Frostbolt cast per count or a check for 40 actual players.

Quarter every threshold for the intended ten-player roster, rounding positive
halves up:

| Stage | Original | Ten-player |
|---|---:|---:|
| Slowed | 100 | 25 |
| Slowed further | 150 | 38 |
| Frozen | 200 | **50** |
| Cracking | 50 | 13 |
| Shattering emote | 100 | 25 |
| Final shatter / split trigger | 150 | **38** |

The **15-second frozen window is unchanged**. The current direct-hit rules and
melee-attacker check for the finishing trigger are unchanged. This preserves
freeze/shatter gameplay rather than removing low-health protection.

This is an explicit **fixed local ten-player tuning change**, not dynamic
mechanic scaling. It applies to Viscidus in every copy running this binary,
including copies with `.raidscale off` or a different manual target size.
It does not count online players, deaths, mages or healers. Revisit the constants
if the intended raid size changes; do not assume the raid-scaling command also
changes these hit thresholds.

## Deliberately unchanged

Everything outside the `HitCounter` enum is byte-identical to the previous
script: hit classification, frost-wand handling, resets, poison abilities,
glob spawning/rejoining, shrink/grow behavior, low-health protection, the
finishing kill path and all timers. No health/damage setting, loot rule, other
boss, bind, progression deadline or database row was edited.

The existing protection checks health before incoming damage; it is not a
clamp to exactly 5% HP. This patch does not diagnose every possible 1-HP state
or overhaul Viscidus's reset/phase logic. The lower counters still need in-game
validation after deployment.

## Validation

```bash
python3 -m unittest discover -s scripts/tests -p test_viscidus_hit_counts.py -v
python3 -m unittest discover -s scripts/tests -p test_bug_trio_reset.py -v
python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v
python3 -m unittest discover -s scripts/tests -p test_raid_scaling_default.py -v
```

All **19 tests passed** (4 Viscidus, 3 trio, 5 AQ40/statistics, 7 scaling).
The Viscidus tests check exact arithmetic, distinct ordered stages, uint8 range,
hit/timer/finish source contracts and patch reverse/apply round-trip. Reversing
the patch also proves the rest of the source is byte-identical. These four tests
are arithmetic/source checks, **not live combat or production AI execution**.
The official C++ style checker and core whitespace checks passed.

The subsequent full worldserver build passed. The deployed executable's
50/38 comparisons and binary hash were verified; current raid progress was
preserved. Only worldserver was restarted, with no setup, migration or manual
encounter reset. Future deployments require fresh permission and current
backups; never restore an old snapshot over later kills.
