# Huhuran: three-target Wyvern Sting

## Status

**Deployed September 11 with explicit deployment permission.** The user approved
reducing the sleep target count after whole-raid sleeps caused wipes. Only
worldserver was restarted; no manual encounter reset, configuration change,
bind edit or migration was performed. See [deployment record](aq40-deployment-20260911.md).

The deployed AQ40 changes comprise:

1. [Bug Trio wipe recovery](aq40-bug-trio-wipe-reset.md).
2. [Viscidus ten-player hit counts](viscidus-ten-player-hit-counts.md).
3. This Huhuran sleep-cap adjustment.

Core commit `5a6f664ee` is published to
`CmPons/azerothcore-wotlk:fix/aq40-ten-player-encounters`, on top of the other two.
The canonical patch is `patches/0024-core-huhuran-ten-player-sting.patch`.
Only `src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_huhuran.cpp` changes.
Source backup and test/style logs are in
`backups/huhuran-sting-preparation-20260911-183327/`.

## Narrow change

Wyvern Sting (26180) now selects **up to three nearest eligible targets** rather
than ten. This is the approved fixed-ten-player adjustment: 10 original targets
multiplied by 10/40, with 2.5 rounded up to 3.

Both selection layers use the same `WYVERN_STING_MAX_TARGETS` constant:

- The boss's custom cast uses the cap of three.
- The shared area-target filter sorts and trims **Sting** to three directly.

Changing only the custom cast would be insufficient to preserve nearest-target
behavior: the old filter would select the nearest ten using the DBC cap, then
`Spell::SelectImplicitAreaTargets` would randomly reduce that list to three using
the custom cast cap. The filter change avoids that extra random reduction.

**Poison Bolt (26052) is unchanged.** Although it shares the filter class, its
branch still uses its DBC `MaxAffectedTargets` (15 in the checked running data).
Its damage and ability to hit the entire ten-player raid remain intact.

Everything else remains unchanged: the 100-yard base Sting radius, sleep effect,
25–43-second repeat interval, Acid Spit, Noxious Poison, frenzy, 30% berserk,
hard-enrage behavior, melee damage and dispel backlash. The latter retains its
3000 **base** damage override, subject to the existing damage pipeline; it is not
a promise of exactly 3000 delivered damage.

There is no special healer/tank immunity and no replacement of sleeping targets.
Stacked healers can still be among the three selected. This avoids routine
whole-raid sleep without removing the mechanic or lowering her damage values.

Like the approved Viscidus counts, this is **fixed local tuning**, not dynamic
attendance scaling. It applies to every AQ40 copy running this binary, including
copies using `.raidscale off` or another manual raid-size override. Revisit it if
the intended raid size changes. No binds, progression deadlines or other boss
states are edited.

## Validation and deployment

```bash
python3 -m unittest discover -s scripts/tests -p test_huhuran_sting_targets.py -v
python3 -m unittest discover -s scripts/tests -p test_viscidus_hit_counts.py -v
python3 -m unittest discover -s scripts/tests -p test_bug_trio_reset.py -v
python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v
python3 -m unittest discover -s scripts/tests -p test_raid_scaling_default.py -v
```

All **23 tests passed** (4 Huhuran, 4 Viscidus, 3 trio, 5 statistics, 7 scaling).
Huhuran tests execute the production filter and distance comparator against API
doubles, covering empty/small/large lists, nearest-three selection, ties,
idempotence and unchanged Poison Bolt limits. Source contracts verify the cast
cap and native selection ordering. Patch reverse/apply checks prove everything
outside the cap declaration, cast argument and filter limit is byte-identical.
The official C++ style checker and core whitespace checks passed.

These are offline tests, not live combat. A subsequent full-server build passed;
the deployed filter's spell-26180-specific cap of three and binary hash were
verified. Actual spell delivery, immunity/resist handling, bot healing and fight
difficulty still need observation. Future restarts require fresh permission
and backups; current AQ40 progress was preserved during this deployment.
