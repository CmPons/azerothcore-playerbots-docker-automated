# Raid enemy healing and absorb scaling

**Deployed September 24, ready 20:47:14 UTC.** See the
[deployment and preservation report](raid-support-deployment-20260924.md).

User-approved scope, September 24, 2026: normalize enemy healing/finite shields,
end the temporary AH plate/shield boost, and deploy. Do not change raid HP/damage
settings, Shatter, tank modes, encounter scripts, roster, equipment or gem stocking.

## Policy

Flat spell healing and finite per-unit damage-absorb shields between eligible
hostile raid NPCs follow the **recipient's HP multiplier**, not damage multiplier.
This preserves their size relative to the recipient's scaled health pool. The
existing instance settings, boss/trash classification, manual overrides and
`.raidscale off` determine the factor; there is no separate new tuning knob.

At the current ten-player defaults:

| Content | HP / support factor | Damage factor (unchanged) |
|---|---:|---:|
| Karazhan, originally 10 | 1.0 | 1.0 |
| Gruul/Mag/SSC etc., originally 25 | 0.4 | approximately 0.577 |
| MC/BWL/AQ40, originally 40 | 0.25 | approximately 0.435 |

BC examples, before crits, other modifiers and overhealing:

| Spell | Original | With 0.4 HP scaling |
|---|---:|---:|
| Blindeye Heal | 46,250–53,750 | 18,500–21,500 |
| Blindeye Prayer, per target | 92,500–107,500 | 37,000–43,000 |
| Blindeye Greater Power Word: Shield | 25,000 | 10,000 |
| Channeler Dark Mending | 69,375–80,625 | 27,750–32,250 |

Boss-to-trash healing uses **trash HP**, not the healer's boss factor. Changing
only damage to 0.20 does not alter these support amounts. This change does not
reduce enemy numbers, cast frequency, shield duration, interrupts or control jobs.

### Safety boundaries

- Both caster and recipient must be creatures on the same scaled raid instance,
  with hostile-to-player faction evidence. The recipient must qualify for existing
  creature HP scaling. NPC healing triggers can supply healing, but an untargetable
  trigger that does not qualify for HP scaling is not a scaled recipient.
- Players, bots (player characters), pets, charms, player-origin summons, friendly
  encounter NPCs, critters/civilians, unscaled maps and disabled scaling are excluded.
  Existing recursive/cached summon provenance protects player-origin descendants.
- Direct flat healing, mechanical flat healing and flat HoT ticks are supported.
  Each amount is scaled **before heal absorbs, application and overheal accounting**.
- Percentage/max-health healing and leech/funnel-derived healing are not multiplied
  again. Mixed spells containing those effects are conservatively excluded because
  the final-heal callback cannot separate their contributions. Unknown/script-only
  heal spell shapes are also left alone, rather than guessed to be flat heals.
- Shields cover positive finite `SCHOOL_ABSORB` and `MANA_SHIELD` amounts. Zero,
  unlimited/negative sentinel amounts, percentage damage reduction, immunities,
  heal-absorb debuffs, mana costs and other aura effects are unchanged. Krosh's
  **75% Spell Shield remains 75%**; it is not a 75-point absorb.
- Shared area auras/dynamic-object auras are excluded because their single amount
  can cover recipients with different scaling eligibility. Ordinary AoE casts
  producing a separate shield aura on each unit still use that unit's factor.
- Shield scaling occurs on fresh amount calculation after aura-specific handlers
  and stacking. Absorbing a hit does not scale the remaining capacity again.
  Recasts/stack recalculation start from native base calculation, not the depleted
  shield. Existing shields are not retroactively resized when a GM changes factors.
- Direct scripted health resets and support bypassing these standard spell/aura
  paths are not rewritten. This is not a blanket rewrite of encounter mechanics.

The module rounds positive scaled amounts to the nearest integer with minimum 1
and saturation at signed 32-bit maximum; zero remains zero. Factor 1 leaves the
original value untouched. No pointers are retained beyond the current callback.

## Native integration

Two narrowly scoped core changes support the module:

1. `Unit::HealBySpell` now passes **target, healer** to `ModifyHealReceived`, matching
   its declared contract and the existing periodic-heal caller. Previously direct
   heals passed the caster first. Without this correction a boss healing trash
   would use the wrong recipient multiplier. No existing module override of this
   callback was found in the installed checkout before adding the raid handler.
2. A no-op-by-default `UnitScript::OnAuraEffectCalculateAmount` hook is dispatched
   at the end of `AuraEffect::CalculateAmount`, after aura scripts and stacking.
   The hook enum is appended, preserving all existing hook IDs. The raid module
   alone applies the finite-shield policy; ordinary core behavior is unchanged
   without that policy.

Canonical module files are `modules/mod-raid-scaling/src/RaidScalingSupport.h`,
`RaidScalingLoader.cpp` and `RaidScalingMgr.{h,cpp}`, mirrored into the build tree.
No SQL migration or spell-script database binding is needed. This module version
requires the matching core hook and direct-heal argument correction.

## Validation

`scripts/tests/test_raid_support_scaling.py` compiles production support methods
and the actual `Unit::HealBySpell` body against the existing native-faction and
summon-provenance harness. Spell storage, health application and log sinks are
explicit doubles, not a live encounter simulation. Tests cover:

- BC healing values, direct/HoT handling, asymmetric recipient HP factors, damage
  multiplier independence, heal absorption before overheal, logs and saturation;
- finite school/mana shields, repeated fresh calculations, stacks, sentinels,
  percentage effects and shared-area exclusions;
- percentage/max-health/leech/mixed spell exclusion, players/pets/charms/friendly
  actors, different instances, nonraids, disabled/manual-off and 10/25/40 defaults;
- a successfully compiled historical direct-heal argument regression that fails
  the asymmetric boss/trash test, rather than mistaking a compiler error for red;
- native dispatch/call ordering, no absorb-consumption rescaling and source mirrors.

The existing scaling-default source-contract test was updated to recognize
`update.sh` delegating to `setup.sh --sources-only`, rather than looking for the
old duplicated module loop. No setup/update runtime behavior changed.

Production-header syntax checks cover both changed module translation units,
`UnitScript.cpp`, `SpellAuraEffects.cpp` and `Unit.cpp`. Existing AH, scaling-state,
Twins-health and raid-creature eligibility suites are rerun. Full build/deployment
and preservation results are recorded separately; offline tests alone do not
prove in-game cast behavior.
