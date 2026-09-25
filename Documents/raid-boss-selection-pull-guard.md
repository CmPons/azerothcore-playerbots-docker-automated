# Selected raid boss is not a pull order

## September 25, 2026 source fix

The user reported bots initiating attacks before the group had aggro, outside
normal proximity aggro range. Investigation found a concrete independent path in
`DpsTargetValue::Calculate`: non-tanks prefer the main tank's target when it is a
taunt-immune raid boss. `ai::threat::GetMainTankTarget` returned the tank's selected
unit even if neither the tank nor group had engaged it. `NotDpsTargetActiveTrigger`
then schedules DPS assist without requiring an attacker count.

Consequently merely selecting an idle Hydross could initiate a ranged attack.
This path does not require the native `ssc` strategy. It is source-reproduced,
not a captured decision trace proving the cause of every reported pull.

## Correction

`Ai/Base/Util/RaidThreatUtils.cpp::GetMainTankTarget` now requires the returned
unit to be alive, in world, on the tank's map, and **engaged by that tank**.
The same check applies to the tank's active attack target: merely beginning to
approach an out-of-range enemy does not let ranged DPS start ahead of the tank.

A generic combat flag is insufficient: fighting trash does not authorize a
selected, unengaged boss. Actual target-specific engagement is used, including
zero-valued threat relationships; there is no minimum threat amount added here.
Once the tank engages, the existing taunt-immune boss focus and threat discipline
continue. After a threat reset the ordinary group-attacker selector remains
available; this helper no longer injects a stale selected boss.

Explicit attack-my-target, pull, and custom-cast commands are unchanged. Existing
RTI and prioritized-target paths remain separate; this patch is **not a universal
interception of every scripted pull, pet attack or proximity aggro**. It does not
disable native encounter packages, introduce a global taunt ban, change aggro
radius, or modify boss mechanics. A different demonstrated auto-pull path must
be traced separately rather than claiming all possible pulls have been proven safe.

## Validation

```sh
python -m unittest scripts.tests.test_raid_boss_pull_focus \
  scripts.tests.test_tank_modes scripts.tests.test_skull_combat_only \
  scripts.tests.test_mag_channeler_manual_control scripts.tests.test_maulgar_manual_control -v
```

18 tests passed. The new fixture injects the actual target helper and DPS selector;
its old-source regression fails at the selected-idle-boss assertion. Coverage:
selection only; unrelated combat; other group's engagement; unreached autoattack;
actual engagement; threat reset; dead, missing, unloaded or cross-map targets;
existing RTI priority and ordinary tank/caster/combo selection. Explicit command
bodies are compared to the prior source. Two native-header syntax checks pass for
the helper and DPS selector. This is not a live combat test.

The user authorized building and deploying this with the separate covered-target
Ari damage-assistance correction. Both are deployed; see
[raid-tank-pull-deployment-20260925.md](raid-tank-pull-deployment-20260925.md)
for binary verification and preservation results. Live combat acceptance remains
separate from the offline regressions and successful deployment.
