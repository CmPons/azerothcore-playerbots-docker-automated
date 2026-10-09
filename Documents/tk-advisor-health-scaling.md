# Kael advisor resurrection: preserve the buff on a scaled health base

Status: **deployed October 9, 2026 at 16:56:03 CEST**, following explicit build/deploy
authorization. See the [deployment report](tk-advisor-health-deployment-20261009.md)
for 82-test/build verification, backups and preservation qualifications. The
previously deployed Feelesia fix remains intact; live encounter acceptance is pending.

## Goal and diagnosis

Keep the encounter's native resurrection health increase, but apply the instance's
configured health scale correctly. Do not flatten revived advisors to first-phase
HP, remove the buff, change creature templates, or hardcode a ten-player multiplier.

Installed spell **36450 (Resurrection)** has effect index 1, aura **133
(`SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT`)**, base points 99 yielding **+100% max
health**. Its DBC file was hash-matched to the running server. The native advisor
`SpellHit` removes Permanent Feign Death, refills health, then reactivates the
advisor after six seconds; none of that encounter code changes.

The ordinary scaling path changes CreateHealth and max/current HP, but not the
underlying `UNIT_MOD_HEALTH / BASE_VALUE`. The native percentage-health aura calls
`Creature::UpdateMaxHealth()`, which rebuilds max HP from unit modifiers, not
CreateHealth. Resurrection therefore exposes the unscaled base.

The user's manual `.raidscale boss hp 0.4` workaround reapplies a cached maximum.
On the old path that suppresses the resurrection buff's numerical benefit without
removing the aura. It is not a correct long-term implementation of the encounter.
Both behaviors are now reproduced offline using production methods.

## Narrow implementation

Canonical file: `modules/mod-raid-scaling/src/RaidScalingMgr.cpp`, synchronized to
`azerothcore-wotlk/modules/mod-raid-scaling/src/RaidScalingMgr.cpp`.

A scoped `UsesAuraAwareHealthScaling` predicate extends the existing Twins-bug
health path to **map 550, entries 20060, 20062, 20063 and 20064 only**. Existing
eligibility/exclusions and boss/trash classification still apply. There is no
phase/aura-presence gate: the base must be scaled *before* the buff arrives.

For those advisors, the scaler now:

1. Scales the cached native unit-mod base once, using the current instance setting.
2. Lets the core calculate the maximum with its existing health-aura modifiers.
3. Preserves current health percentage when reapplying/changing scaling; it does
   not refill an injured advisor. The encounter's resurrection still refills HP.
4. Restores the native base on scaling-off, retaining current aura state rather
   than restoring a stale cached buffed/unbuffed maximum. Dead creatures stay dead.
5. Uses the already-deployed respawn hook's fresh baseline after native stat rebuilds.

Twins identification/mutation behavior, other creatures' health paths, damage and
healing scaling, bot assignments, encounter phases/timers, spell data, loot, raid
reset persistence and configuration remain unchanged. Source preparation changed
no gameplay SQL or services; the separately authorized deployment recreated only
worldserver. The reset-row deletion bug remains deferred.

## Expected values at the unchanged 0.4 health multiplier

The audited installed DB baseline is `ceil(basehp1 * HealthModifier)`, at the
unchanged 1.0 world-boss HP rate. The fixture retains those inputs and native spell
identity, rather than asserting round-number estimates from a health bar.

| Advisor | Native base | Scaled first phase | Scaled resurrection | Old unscaled recalculation |
|---|---:|---:|---:|---:|
| Thaladred | 191,218 | 76,487 | **152,974** | 382,436 |
| Sanguinar | 191,218 | 76,487 | **152,974** | 382,436 |
| Telonicus | 191,218 | 76,487 | **152,974** | 382,436 |
| Capernian | 133,838 | 53,535 | **107,070** | 267,676 |

These are source/data-backed fixture results, **not live encounter acceptance**.
The user's approximately 76k/250k observations were not captured with exact advisor
identity and runtime aura state; they are not claimed to establish an exact 3.5x
formula. Native floating-point/integer rounding is retained.

Once deployed, reapplying `.raidscale boss hp 0.4` during resurrection should retain
the buffed maxima above, not lower them to first-phase HP. The workaround should
no longer be necessary for this particular recalculation defect.

## Tests and limits

`scripts/tests/test_tk_advisor_scaling.py` adds ten tests, with API doubles extending
the shared Twins fixture rather than reimplementing the scaling algorithm. They
compile production manager methods, native health-total calculation, creature HP
update, percentage-aura handler, and the advisor's `DamageTaken`/`SpellHit` methods.
Cast delivery, scheduler, world objects and the stat-rebuild boundary are doubles;
existing respawn tests separately exercise the complete native respawn hook body.
The fixture supplies the original map size exactly as runtime configuration does.

Coverage:

- All four advisors: first-phase HP, native fake-death damage clamp, +100% aura,
  full-health resurrection, six-second reactivation and second-phase lethal damage.
- Twenty repeated applications while injured: no compounding, buff clobbering or
  unintended healing; aura removal returns to the scaled first-phase maximum.
- Manual boss multiplier, unrelated trash override, scaling-off/re-enable, and
  dead-advisor restoration without revival.
- Twenty respawn cycles per advisor; changed native baseline on the same GUID;
  active aura during a native rebuild.
- Initial scaling while already buffed; subsequent buff expiry/restoration; isolated
  instances; intro flags, player-origin, disabled, nonraid and noninstance guards.
- Wrong-map/unrelated-boss scope; unchanged Twins eligibility, damage, respawn hook
  and native Kael encounter source; byte-identical module mirrors.
- Actual pre-fix manager reproduces unscaled resurrection and workaround clobbering,
  and fails the corrected-health assertion. Restore-path and late-aura-gate mutants
  fail their intended behavioral scenarios.

The shared harness compiles with C++20, warnings-as-errors, ASan, UBSan and
float-division/float-conversion sanitizers. Native-header syntax and scoped official
C++ style checks pass. An initial fixture run missed map 550's original-size config,
so its real `EnableForMap` correctly refused; the fixture configuration and explicit
return-value checks were corrected, not the production method. A combined test
command hit its wall-clock limit near the final suite and was rerun to completion.

**59 tests passed** in the combined suite below, including the ten new advisor
tests and retained Twins, respawn, creature eligibility, defaults, support-scaling
and Feelesia recovery tests.

Validation command:

```sh
python3 -m unittest \
  scripts.tests.test_tk_advisor_scaling scripts.tests.test_twins_bug_scaling \
  scripts.tests.test_raid_respawn_scaling scripts.tests.test_raid_creature_scaling \
  scripts.tests.test_raid_scaling_default scripts.tests.test_raid_support_scaling \
  scripts.tests.test_tk_weapon_recovery -v
```

Source preparation did not build/restart the server or change gameplay state.
The subsequent authorized deployment is linked above. No encounter pull or live
scaling command was issued by the assistant; live acceptance remains an ordinary
advisor resurrection during the user's next attempt.
