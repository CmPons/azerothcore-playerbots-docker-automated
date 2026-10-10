# Per-target raid threat reference and TK advisor policy

## Problem confirmed October 10, 2026

This is our added raid-boss safeguard, not the ordinary upstream threat ratio.
`mod-playerbots` commit `acb4b750` introduced the designated-MT comparison in
`Ai/Base/Util/RaidThreatUtils.cpp::ShouldHoldDamageOnTauntImmuneBoss`.
Except for the later Twin Emperors ownership exception, it compared DPS with the
MT's threat on every qualifying taunt-immune raid boss, regardless of which tank
was actually responsible for that enemy. The separate ordinary `ThreatValue`
calculation already compares with the highest-threat recognized living tank.

The second October 10 capture contains explicit threat vetoes while targeting
**Telonicus**, not merely Capernian:

| Sample (CEST) | Bot | `main tank` | Ordinary target / AoE ratio |
| --- | --- | --- | --- |
| 21:06:51 | Pilbok | Redshift | 0 / 1 |
| 21:06:52 | Kaaren | Redshift | 0 / 15 |
| 21:06:53 | Feelesia | Redshift | 12 / 49 |

Rolling action histories include blocked melee/Sinister Strike/Killing Spree,
Lava Lash, Aimed Shot and Steady Shot. Samples are sequential/cached, not atomic
combat events; the exact raw MT/bot threat and first-blocking branch at each veto
were not recorded. Source proves the wrong-owner rule, and these observations
strongly implicate it, not a precise numeric reconstruction of each rejected action.
Incidental positive Redshift threat was a hypothesis, not a measured value.

## Shared threat fix

For ordinary taunt-immune raid bosses, choose the highest-threat **eligible tank
on that target's threat table**. Eligibility requires a living, in-world group
tank on the same map/phase, friendly to the bot, not charmed or in GM mode.
DPS/healers do not become references simply by pulling aggro. An eligible tank
with zero threat remains available for the existing opening-pull check.

Preserved unchanged:

- Twin Emperors' explicit physical/caster ownership and missing-owner behavior.
- Thresholds, settings and emergency-healing controls; no threat generation changes.
- Tank exemption, bot-as-current-victim stop, non-boss/non-raid paths, and existing
  zero-tank/positive-bot-threat branch.
- `GetMainTankTarget` target-specific engagement, deliberate attack/pull commands,
  and `StopDirectDamage`. Choosing a reference is not permission to pull.
- Ordinary `ThreatValues.cpp` calculations. Their nonstandard-owner/gaze behavior
  is not silently rewritten by this fix.

This is not limited to TK or one phase. Other qualifying off-tanked bosses receive
the same correction. The TK Phoenix has taunt immunity but normal elite rank, so
it does not meet this particular boss check. **Kael and all four advisors are
also taunt-immune in the live data.** Earlier advice to taunt Kael off Ari was wrong.
No immunity or creature data was changed.

## Authored TK policy

The shared default remains at the historical authoring path
`raid-policies/aq40/combat.lua`; its existing AQ40 logic is unchanged. The added
map550 branch supplies this revived-advisor order to all eligible non-healer,
non-tank bots, including Raney and Beliona:

**Thaladred → Telonicus → Sanguinar → Capernian**

The human main tank owns **Sanguinar and Capernian**. Ari's existing first-OT
Telonicus job is retained, not replaced with another native assignment. Once a
fresh Lua Capernian ownership declaration resolves, existing TK helper consumers
(including ranged-tank selection and misdirection) use it instead of choosing a
warlock. Beliona is therefore free to follow the common DPS order. No human
movement, attack, taunt, threat, raid flag, gear or spec is changed by the policy.

The branch recognizes revived advisors from multiple active advisors or the
resurrection aura36450 and resets its state out of combat. It leaves the initial
single-advisor phase alone. Missing human MT, duplicate advisor identities or
critical observation gaps release the plan to native behavior. It excludes dead,
zero-health, unselectable, unattackable and unengaged advisors. Human/manual target
locks and native admission remain authoritative. Preferences continue while
advisors overlap with active Kael; no Kael-health/timer change is made.

Capernian safety needs an explicit qualification: native melee distancing only
covers the **single-advisor** phase. When Capernian is the final priority and Kael
is still observed dormant, this policy asks non-tank melee bots to remain at least
24 yards from her center (25-yard retreat goal). It does not force melee into her
burst range just to satisfy the shared order. Ground claims are released once
Kael is seen active and are never reacquired during a later invulnerable transition;
missing/spatially incomplete observations also suppress those claims. This leaves
MC rescue, flames and phoenix movement to native handling. Gaze/other existing
mechanics are retained; checked movement is a request, not guaranteed clearance.

New encounter decisions are Lua, not additional native boss assignment routines.
The native changes only add a checked ownership adapter and let the existing TK
DPS-priority routine yield to a valid authored preference. Without one, its full
legacy body remains unchanged. MC, loot, advisor health, phoenix assignment,
spell mechanics and timers are not modified.

## Compatible API2 extension

Snapshots add `selectable` on units and `main_tank` on members. An intent may
optionally contain `tank_targets={entityIndex,...}` (at most four). This is
**declarative ownership**, not an actor-control request. A living tank can be named
including a human/ineligible member; their movement, target or spell requests are
still rejected by the old eligibility checks. Non-tanks, duplicate ownership,
invalid/sparse keys, bad indices and unengaged/inactive targets reject the entire
plan transactionally. Old API2 outputs need no new fields.

`RaidCombat::AssignedTank` re-resolves current GUIDs and validates fresh scope,
map, group, phase, living tank role, friendly/non-charmed/non-GM status, target
identity/availability and engagement. It stores no world pointer across ticks.
An expired, faulted or invalid declaration falls back to native role selection.
The declaration does not override the new highest-actual-tank-threat calculation.

Publish this payload only with the new native runtime: the older API2 parser
will reject `tank_targets` when the TK branch uses it. The production-linked
checker must be refreshed too. Reverting to the retained prior Lua default
restores the old advisor policy without undoing the native threat fix or needing
a server restart; adoption still waits for a safe boundary.

## Validation and evidence

- `test_raid_threat_owner.py`: production selector/hold bodies under ASan+UBSan;
  established OT vs incidental MT threat, exact threshold, reference handoff,
  invalid owners, opening behavior, Twins exception and unchanged pull/stop paths.
  The same fixture rejects the original MT-only implementation.
- `test_tk_advisor_policy.py`: production ownership resolver, Capernian helper and
  new priority adapter; fresh/invalid ownership, fallback, manual locks and
  tank/healer exclusions. Legacy priority and selected mechanic bodies compare
  exactly with the deployed pre-change source.
- `test_raid_combat.py`: actual production Lua parser/runtime executes the full
  authored policy, order progression, human ownership, phase recognition, gaps,
  Capernian finisher safety/active-Kael release and malformed declarations.
  Existing native admission, ground lifecycle, collector and reload tests remain.
- Six production translation units receive native-header syntax checks. The syntax
  runner now includes the vendored Lua headers; the initial missing-`lua.h` failure
  is retained. An adapter fixture indentation error was corrected, not suppressed.
- The historical Twins patch replay uses a compatible pre-change `RaidThreatUtils`
  snapshot, just as it already did for `ThreatStrategy`; semantic tests still
  compile **both current production files**. The shared double adds the previously
  absent charmer query. Original failed fixture logs are retained.

These are source-bound fixtures and API doubles, not live encounter acceptance.
A fresh pull is still needed to observe the corrected throughput and new order.

Private implementation/test evidence: `backups/tk-threat-work-20261010-213207/`.
Private completed captures:

- `backups/kael-mc-live-20261010-204651/`: 810 full-capture responses and747 fast MC
  responses,20:47:06–20:56:08 CEST, no query errors; both collectors exited by
 20:56:12. Pilbok submitted Shiv on Redshift and Ailina; Kaaren submitted Stormstrike
  on Meliah; each log reports the MC aura absent immediately after submission and
  subsequent samples agree with removal well before natural expiry. Feelesia
  obtained/equipped the dagger but later returned to30316; her rescue participation
  is not established. Ari was sampled dead, then alive, then dead again; the
  resurrection mechanism was not captured. User reports she initially held both
  Kael and the first Phoenix while Redshift was distant.
- `backups/kael-mc-live-20261010-210644/`: 1728 full-capture responses and1593 fast MC
  responses,21:06:44–21:26:02 CEST, no query errors; includes post-attempt time.
  Both collectors exited by21:26:06. Telonicus remained alive when a Kael MC was
  sampled at21:06:57. Do not merge the two attempts or infer damage totals from
  action histories.
