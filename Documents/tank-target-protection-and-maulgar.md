# Cooperative tank targeting and manual Maulgar control

## Status and scope

Source-only change requested after Redshift and Arinerica repeatedly taunted the
same council target while other adds were loose. Not deployed by this task:
no server image build, restart, runtime configuration/strategy changes or database
writes. A separately authorized build/deployment is needed before live acceptance.
Published playerbots revision: `560817b1c40e7c57f406968cfd6f987c19316ff6`,
also recorded in `repo-pins.txt`.

Two changes are intentionally separate:

1. Respect another living tank's current PvE mob ownership in generic tank target
   selection and bot taunts, regardless of either character's Main Tank flag.
2. Stop registering **Maulgar** encounter triggers/multipliers in `gruulslair`,
   while preserving **Gruul** positioning, ranged spread and Shatter behavior.

## Cause supported by source and tests

Both characters had the Main Tank flag in the read-only group snapshot. This is
not itself proof of which spell took threat during the fight. However, the source
had several concrete gaps:

- `HasAggroValue` and `PlayerbotAI::HasAggro` exempted an explicitly marked MT from
  the usual "another tank already has this" behavior.
- The stronger master/MT target exclusion required off-tank mode, which an MT flag
  could bypass. The RTI path could also select a human tank's marked mob.
- An explicit MT's current-target preference ran before loose-add priority.
- Righteous Defense targets a friendly victim and can take several attackers;
  checking only a selected hostile target would not cover this fallback.

The new offline fixture failed against the old production selector with
`Assertion 'selector.Calculate() == nullptr' failed` for a mob already held by
Redshift while Ari was also explicitly MT. It passes with the change.

## Ownership rule

`src/Ai/Base/Util/TankTargetProtection.{h,cpp}` supplies the shared policy:

- Use the creature's current victim, falling back to the threat manager's current
  victim when needed. Do not reserve targets based on highest historical threat,
  selected target, saved role, raid icon or MT flag.
- Protect another **living, in-world, same-group, same-map tank-spec player**.
  Use the existing `PlayerbotAI::IsTank(player, true)` classifier for humans and
  bots, including Protection warrior/paladin and the core's DK/bear tank rules.
  A bot's loaded strategy does not have to match its tank spec for protection.
- The bot's own mobs, loose mobs and mobs hitting healers/DPS remain eligible.
  Dead/out-of-world owners no longer block rescue through the old off-tank path.
- Unrelated groups and player/player-controlled hostile targets are outside the
  new hostile-target protection rule.
- The generic tank selector applies protection before RTI and candidate ranking.
  Loose-add pickup precedes explicit-MT focus; when neither candidate needs a tank,
  existing MT focus and distance/threat ordering remain intact.
- Both "has aggro" paths treat a protected mob as already covered, allowing the
  existing tank-assist trigger to switch to the loose target instead of taunting
  the protected one again.

No talents, saved profiles, gear, role flags or player group assignments are edited.
DPS target selection is not changed: DPS can still attack mobs held by tanks.
Existing off-tank protection for the master/MT is retained, with living-owner checks.

## Taunt enforcement

`src/Script/PlayerbotsTankTaunt.cpp` registers an `AllSpellScript` for bot-session
player casters only. Human casts and the existing hunter-pet policy are unchanged.
It checks both spell eligibility (`OnSpellCheckCast`) and actual preparation
(`CanPrepare`), including direct/triggered bot casts and victim changes after the
AI's earlier decision.

- Generic single-target taunts/forced attacks are identified by
  `SPELL_EFFECT_ATTACK_ME` / `SPELL_AURA_MOD_TAUNT`.
- Righteous Defense **31789** is rejected when its friendly target is another
  protected tank; its hostile triggered taunts are also subject to the shared rule.
- Death Grip **49576** is checked before its scripted pull, not merely when its
  triggered taunt executes.
- Caster-centered area taunts (Challenging Shout/Roar) are withheld if their spell
  radius includes a mob currently held by another protected tank. Candidates come
  from group tanks' threat-reference lists, not the bot's cached attacker list.
  Outside-radius or no-longer-held mobs do not block the cast.

This is not a blanket damage/threat nerf. Normal damage, AoE, healing and existing
threat remain unchanged; incidental AoE threat can still move a mob. It does not
force bots idle when no loose targets exist or globally forbid attacking a tanked
mob. It prevents selecting those mobs as generic tank assignments and taunting
away their ownership.

**Deliberate tank swaps:** no new override/handoff command is introduced. A bot's
scripted or manually requested taunt is also blocked while another living tank-spec
group member holds that mob. Human taunts are unrestricted. Encounters requiring
automatic tank-to-tank taunt swaps need an explicit, reviewed exception rather than
silently bypassing this policy via MT flags or marks.

## Maulgar removal

`src/Ai/Raid/Gruul/GruulStrategy.cpp` no longer registers Maulgar's ten triggers and
five multipliers. Normal class/group AI remains active. The council no longer
forces its tank/mage/Moonkin assignments, five icons, DPS order, movement,
Misdirection, Banish, Bloodlust delay, or encounter-specific spell restrictions.

This also removes the dedicated mage-tank/Spellsteal sequence and council hazard
avoidance. It does **not** remove those encounter mechanics or guarantee a win;
manual control/class behavior must handle them. Generic marking/CC strategies are
not globally disabled. Existing personal `rti` preferences are not rewritten.

Gruul's three triggers and three multipliers are retained unchanged. Boss scripts,
loot, instance saves, raid scaling and summon counts are untouched.

## Offline verification

```sh
python -m unittest scripts.tests.test_tank_target_protection -v
python scripts/tests/raid_combat_syntax.py --output /tmp/tank-target-protection-syntax \
  modules/mod-playerbots/src/Ai/Base/Util/TankTargetProtection.cpp \
  modules/mod-playerbots/src/Script/PlayerbotsTankTaunt.cpp \
  modules/mod-playerbots/src/Ai/Base/Value/TankTargetValue.cpp \
  modules/mod-playerbots/src/Ai/Base/Value/AttackerCountValues.cpp \
  modules/mod-playerbots/src/Bot/PlayerbotAI.cpp \
  modules/mod-playerbots/src/Ai/Raid/Gruul/GruulStrategy.cpp \
  modules/mod-playerbots/src/Script/Playerbots.cpp
```

The three targeted tests pass. The C++ harness compiles the production helper,
selector, aggro methods, tank-assist trigger and spell-hook bodies with API doubles.
It checks duplicate/no MT flags, human/bot spec owners, RTI exclusion, loose-add
selection in both candidate orders, existing owned-target focus, dead-owner rescue,
threat-victim fallback, same-group/map eligibility, PvP exclusion, Righteous Defense,
Death Grip, area radius, changing victims between check/prepare, and human-cast
exemption. All seven changed/new production translation units pass native-header
syntax checks. Official C++ codestyle has no new findings versus baseline.

The related hunter-pet, skull and Twins behavior checks also pass: **12 selected
checks passed in total**, including the three new tests. Two existing
historical patch-replay checks fail, independently of this implementation:

- `HunterPetTauntTests.test_patch_round_trip_and_isolated_scope`: old 0021 loader
  context no longer matches after later script registrations. Reproduced using
  unchanged playerbots HEAD files in an isolated temporary directory.
- `TwinsCoordinationTests.test_pinned_replay_of_twins_sources`: tries to apply old
  creation patches on a fork pin that already contains those files. Twins sources,
  the test and its input pin were unchanged when this failure was observed.

These historical tests were not altered to hide their failures. Fork publication
and exact pins, not historical patch replay, are the current source workflow.

## Live acceptance after an authorized deployment

1. With Redshift/Ari in Protection, let Redshift hold a mob and leave a second on a
   healer. Ari should choose the loose mob, even if both have MT flags or the held
   mob bears her preferred icon.
2. Have Redshift taunt one of Ari's mobs. She must not taunt it straight back;
   verify single-target and Righteous Defense fallback, then another loose add.
3. Confirm rescue becomes available when the owning tank dies, and Ari keeps
   fighting her own targets when no loose targets exist.
4. On Maulgar, verify no council icon/assignment enforcement; explicitly check DPS
   `rti` preferences instead of assuming a star always controls all bots.
5. Verify Gruul movement/Shatter behavior still activates.

No live combat result is claimed by the offline tests.
