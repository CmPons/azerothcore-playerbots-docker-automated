# BC 25-player encounters versus normal Karazhan — September 24

## Scope and conclusion

Read-only investigation of the **installed** creature templates, class/level stats,
client spell records, encounter scripts and raid-scaling implementation. No tuning,
gameplay SQL, encounter reset, build or service interruption was performed.

Compare normal ten-player Karazhan with the configured ten-player defaults for
Gruul's Lair and Magtheridon's Lair, **not** a temporary `0.20` damage override.
These are calculated baselines, not measurements of a particular attempt.

**The baseline HP and melee comparison does not support a blanket claim that BC
25-player scaling is excessively high.** Maulgar's main-hand DPS is approximately
Prince's opening baseline; Gruul starts lower; Magtheridon starts 25% higher.
However, simultaneous mechanics, unchanged enemy healing/shields and at least one
damage-scaling coverage gap matter substantially. Scaling headcount is not the same
as adapting an encounter's roles, burst windows and control requirements.

This narrows the earlier Domo comparison: Maulgar's roughly tenfold advantage over
Domo in single-leader raw melee DPS is **not** evidence that Maulgar is ten times
harder than a relevant level-70 encounter, or that every BC raid needs one universal
new damage multiplier. No matched historical player-stat or whole-fight trace is
available to establish those conclusions.

## Method

- Karazhan: original 10, target 10, HP and damage factors **1.0**.
- Gruul/Mag: original 25, target 10, HP factor **0.4**, damage factor
  **`0.4 ** 0.6 = 0.57707996`**.
- Relevant global creature HP, melee damage and spell damage rates are 1.
- All four council members and the channelers are rank 3 and use the **boss**
  multipliers. Burning Abyssals use **trash** multipliers. Both categories have
  the same defaults here; a boss-only manual override is a different comparison.
- Reuse `scripts/analysis/raid_size_ratios.py::calculate` with the installed
  `creature_template` joined to `creature_classlevelstats` on `maxlevel/unit_class`.
  Apply the HP multiplier with positive nearest-integer rounding and multiply
  calculated melee damage by the damage factor.
- Main-hand DPS means mean raw swing divided by attack period, **before armor,
  avoidance, block, crits, buffs, special attacks, cast downtime and extra weapons**.
  It is not actual tank damage or whole-encounter damage. Do not sum caster melee
  columns and call that the encounter's DPS.
- Spell numbers are base effect values from the running container's `Spell.dbc`,
  with the expected creature damage factor where applicable. They are not combat
  logs; resistance, absorption, buffs and other combat adjustments are excluded.
  Selected world `spell_dbc`/`spell_bonus_data` overrides were checked. Relevant
  source corrections do not replace the cited base values.
- Baseline HP does not include enemy healing, absorbs, repeated phases, summoned
  bodies or damage-vulnerability phases. Those can change actual damage required.

Private evidence: `backups/bc-raid-comparison-20260924-214150/`, including selected
SQL results, extracted spell facts and the private client data used to inspect
those facts. Client DBC files, configs and database dumps are not published.

## Individual baseline comparison

| Encounter/body | Entry | Scaled HP | Raw scaled main-hand swing | Attack period | Raw main-hand DPS |
|---|---:|---:|---:|---:|---:|
| Midnight | 16151 | 265,580 | 4,245–6,025 | 2.000s | 2,567 |
| Attumen, unmounted | 15550 | 265,580 | 11,104–15,758 | 4.130s | 3,252 |
| Attumen, mounted template | 16152 | 265,580 | 11,647–16,530 | 4.130s | 3,411 |
| Moroes | 15687 | 270,892 | 4,811–6,828 | 2.000s | 2,910 |
| Maiden | 16457 | 297,430 | 6,151–8,687 | 2.000s | 3,710 |
| Curator | 15691 | 488,635 | 5,413–7,645 | 2.000s | 3,264 |
| Shade of Aran, caster | 16524 | 594,832 | 2,061–2,997 | 2.000s | 1,265 |
| Illhoof | 15688 | 489,020 | 5,050–7,140 | 2.000s | 3,047 |
| Netherspite | 15689 | 782,460 | 5,800–8,194 | 2.000s | 3,498 |
| Prince, opening main hand | 15690 | 796,740 | 6,509–9,238 | 2.000s | 3,937 |
| Nightbane | 17225 | 929,530 | 9,056–12,853 | 2.000s | 5,477 |
| High King Maulgar, opening main hand | 18831 | 212,464 | 4,733–6,717 | 1.449s | 3,951 |
| Krosh, caster | 18832 | 84,980 | 1,715–2,422 | 1.449s | 1,427 |
| Olm | 18834 | 84,980 | 1,715–2,422 | 1.449s | 1,427 |
| Kiggler, caster | 18835 | 84,980 | 2,858–4,036 | 1.449s | 2,379 |
| Blindeye, healer | 18836 | 84,980 | 720–1,017 | 1.449s | 599 |
| Gruul, zero Growth | 19044 | 956,088 | 2,840–4,030 | 1.449s | 2,371 |
| Magtheridon | 17257 | 1,349,146 | 8,166–11,589 | 2.000s | 4,939 |
| Hellfire Channeler, each | 17256 | 61,186 | 2,840–4,011 | 2.000s | 1,713 |
| Burning Abyssal, temporary summon | 17454 | 586,824 | 583–824 | 2.000s | 352 |
| Olm's Wild Fel Stalker | 18847 | 8,499 | 1,307–1,854 | 2.000s | 790 |

Illhoof/Nightbane are included as reference points, not as a claim the user reported
killing them. Attumen rows are separate templates/phases, **not three full health
bars that must simply be added together**. Prince gains dual wield, Thrash and
armor reduction in phase two; his 3,937 figure is not peak Prince pressure. Aran's
melee figure is not a useful representation of his normal spell damage.

### HP totals and useful same-expansion comparisons

- **Maulgar plus four council members: 552,384 HP**, before heals/shields/summons.
  That is below Aran's 594,832 and about **69% of Prince's** single health bar.
- Oz: Dorothee 106,225; Roar/Strawman/Tinhead 77,490 each; Crone 106,225.
  Those required bodies total **444,920 HP**. Tito adds **26,558** if summoned and
  killed, giving **471,478**. Council starting HP is about **24% above the former
  or 17% above the latter**, not several times larger. Oz releases enemies on a
  staggered schedule and brings the Crone after the first four die; this is not
  equivalent simultaneous pressure.
- **Gruul: 1.20× Prince HP, 0.60× Prince opening main-hand DPS**, before Growth and
  specials.
- **Magtheridon: 1.69× Prince HP, 1.25× Prince opening main-hand DPS**. His baseline
  melee remains below Nightbane's.
- **Mag plus five channelers: 1,655,076 starting HP**, about **2.08× Prince HP**,
  excluding heals and Burning Abyssals. The installed raid map 544 has five
  channeler spawns; the other five rows using that entry are on map 542, not ten
  channelers in this raid.

## Patterns a baseline table misses

### 1. Council adds retain substantial individual jobs

Krosh's Greater Fireball (`33051`) starts at **8,550–9,450**, or approximately
**4,934–5,453** at the default raid damage factor. Aran's normal Fireball (`29953`)
is **3,910–5,290**, with no headcount reduction in Kara. Thus Krosh alone retains
roughly a Kara caster boss's individual fireball size, despite a much smaller HP
bar and four other active enemies.

Krosh's Spell Shield (`33054`) reduces incoming magic damage by **75%**. If stolen,
it also substantially changes the caster tank's fireball exposure: about
**1,234–1,363** from the above baseline. Without that interaction, assigning Krosh
as just another melee-tanked add is not an equivalent workload. Blast Wave
(`33061`) is another approximately **3,470–4,032 per nearby target** at defaults.

Kiggler's Lightning Bolt (`36152`) is approximately **859–1,161** at defaults,
plus his other control/threat mechanics. Olm adds Death Coil, stacking Dark Decay
(**about 289 per two-second tick per stack**, `33129`) and Fel Stalkers. Blindeye
adds healing and shielding. These are distinct jobs, not five identical melee
mobs whose total damage can be inferred by summing template swing DPS.

Maulgar also retains Arcing Smash, Mighty Blow and Whirlwind; below 50% he gains
Flurry and schedules charges/fear. His Prince-like opening melee baseline is not
a claim that his entire damage pattern matches Prince's.

### 2. Health shrinks; flat enemy healing and absorbs do not

The module changes creature health and outgoing damage, **not flat NPC healing or
absorb amounts**:

| Ability | Base value retained | Relevant scaled health |
|---|---:|---:|
| Blindeye Heal, `33144` | 46,250–53,750 | 84,980 per council add |
| Blindeye Prayer of Healing, `33152` | 92,500–107,500 per affected target | 84,980 per council add |
| Blindeye Greater Power Word: Shield, `33147` | 25,000 absorb | 84,980 on Blindeye |
| Channeler Dark Mending, `30528` | 69,375–80,625 | 61,186 per channeler |

A landed Prayer or Dark Mending has enough base healing to refill an entire scaled
add's health bar, **subject to missing health and overhealing**. A 25k shield is
about **29%** of Blindeye's scaled HP instead of about **12%** of his original HP.
Flat healing/absorb magnitude relative to HP is therefore **2.5×** its original
ratio. This makes a missed interrupt or shield-breaking requirement relatively
more expensive; it does not prove that those spells actually landed in the user's
attempt, nor that effective healing rises by exactly 2.5× after overheal.

### 3. Gruul ramps, and Shatter has a scaling coverage gap

Growth (`36300`) adds **15% damage per stack** on a roughly **30.3-second** schedule.
Ten stacks make the opening main-hand baseline about **5,926 raw DPS**, before
other combat effects. Hurtful Strike (`33813`) has a default-scaled base of
approximately **7,127–7,877**, before Growth/mitigation.

**Source-confirmed gap:** the installed `33654 -> spell_gruul_shatter` binding makes
players cast Shatter's damaging spell (`33671`) themselves, without supplying
Gruul as original caster. `GetDamageScale` returns **1.0 for a player attacker**.
The spell's **9,000 base damage**, reduced with distance by its script, therefore
bypasses this module's 0.577 damage factor. Changing the boss damage multiplier
also does not repair that player-source path.

This is a source/data finding, not a measured explanation of a particular death.
Actual Shatter damage still depends on positioning and applicable combat modifiers.
Do **not** address it by globally scaling player damage: that would also risk
reducing the raid's own DPS. Any fix requires a narrowly scoped attribution or
encounter-hazard design and separate testing/authorization.

### 4. Mag's opening is a control/healing workload, not just boss HP

Five channelers remain five channelers. Each can heal, fear, volley and summon;
reducing HP does not remove those jobs or reduce the number of simultaneous casts.
An unbuffed Shadow Bolt Volley (`30510`) is approximately **859–1,161 per affected
player per channeler** at defaults. This is a per-cast value, not an assertion that
all five always hit every player simultaneously.

Soul Transfer (`30531`) retains **+20% damage and +10% casting speed per stack** on
survivors, further changing the later part of that opening.

Burning Abyssals have roughly **587k calculated scaled HP each**, but are timed
**60-second** summons, not ordinary adds that should be included as required full
kills. Spell `30511` uses wild summon properties 64 with no special health override
in that summon path. Their unusually large health makes control/expiration a
material distinction from simply collecting and cleaving down small adds. This
investigation does not establish how the bots targeted or controlled them in the
user's run.

## Interpretation and next scope

The ten-person roster's Kara success is a useful same-expansion benchmark. It does
not justify blaming headcount, and the baseline findings do not justify setting
all BC raids to `0.20` either. They support separating:

1. **Baseline survivability/burst tuning**, particularly Mag's larger individual
   swings and simultaneous opening damage.
2. **Encounter adaptation**, including healing/absorbs, crowd-control jobs, summons,
   ramps and role-specific mechanics.
3. **Actual coverage defects**, specifically the Shatter player-source path.

No new tuning target is established without observing casts, incoming damage,
healing and target selection. Cooperative tank modes can improve assignments;
they cannot themselves scale heals, remove mechanics or repair Shatter attribution.

## Validation

All 21 table rows were checked against the fresh installed template snapshot for
HP, swing range, attack period and DPS. Council, Oz and Mag-plus-channeler totals
were independently checked. Both Shatter script bindings were verified in the
installed world database. The eight existing raid-size/UBRS analysis tests passed.
No live combat trace or new encounter gameplay test was performed.

## Source anchors

Source revisions inspected: root `0fd326b07a4a719ad778d87496942a02e7e94bc7`, core
`2e6dae2ce03a59e6b341ec63351e90b14c53b6f5` (the running native source).

- [Headcount factors](https://github.com/CmPons/azerothcore-playerbots-docker-automated/blob/0fd326b07a4a719ad778d87496942a02e7e94bc7/modules/mod-raid-scaling/src/RaidScalingMgr.cpp#L206-L235)
  and [creature-only damage selection](https://github.com/CmPons/azerothcore-playerbots-docker-automated/blob/0fd326b07a4a719ad778d87496942a02e7e94bc7/modules/mod-raid-scaling/src/RaidScalingMgr.cpp#L533-L558).
- [Scaling hooks; no NPC heal/absorb scaling](https://github.com/CmPons/azerothcore-playerbots-docker-automated/blob/0fd326b07a4a719ad778d87496942a02e7e94bc7/modules/mod-raid-scaling/src/RaidScalingLoader.cpp).
- [Gruul Growth scheduling](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/scripts/Outland/GruulsLair/boss_gruul.cpp#L74-L118)
  and [Shatter player cast/distance reduction](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/scripts/Outland/GruulsLair/boss_gruul.cpp#L238-L293).
- [CastSpell original-caster defaults](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/game/Entities/Unit/Unit.cpp#L1361-L1417)
  and [Spell constructor fallback to immediate caster](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/game/Spells/Spell.cpp#L627-L639).
- [Maulgar and council encounter logic](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/scripts/Outland/GruulsLair/boss_high_king_maulgar.cpp).
- [Prince's phase-two changes](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/scripts/EasternKingdoms/Karazhan/boss_prince_malchezaar.cpp#L95-L146).
- [Opera activation/summoning logic](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/scripts/EasternKingdoms/Karazhan/bosses_opera.cpp).
- [Wild summon duration/health path](https://github.com/CmPons/azerothcore-wotlk/blob/2e6dae2ce03a59e6b341ec63351e90b14c53b6f5/src/server/game/Spells/SpellEffects.cpp#L2404-L2507).
