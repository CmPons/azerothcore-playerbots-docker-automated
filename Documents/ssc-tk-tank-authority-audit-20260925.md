# SSC / Tempest Keep tank-authority audit — September 25, 2026

## Status and requested policy

**Audit complete; no production behavior changes, Lua publication, build or restart.**
Audited the currently published/deployed playerbots source
`f01eab18ed6dfa4093f9ec4fc5008e59011b315b`.

The user wants ordinary tank behavior and deliberate player commands to remain
in charge, with encounter-specific tank exceptions authored in our Lua policies,
not competing native encounter scripts. This is the intended direction, **not a
claim that the currently deployed T5 implementation already enforces it**.

The remaining scope question before removal is whether tanks retain native
hazard escape/personal-mechanic handling, or whether those movements/target changes
must also become manual/Lua-only. These are intertwined with tank assignments in
several routines. Removing an entire raid strategy would also remove unrelated
non-tank mechanics and is not the proposed shortcut.

## Coverage and evidence

Inspected both strategy registrations, tank-relevant triggers/actions/multipliers,
role helpers, shared RTI helpers, TK's spell listener, and the deployed generic
Lua interface and target/action integration. Registration inventories:

| Raid | Trigger registrations | Action slots | Multipliers |
|---|---:|---:|---:|
| SSC | 48 | 50 | 28 |
| TK | 40 | 44 | 17 |

These counts include bookkeeping, trash and legitimate mechanics, not just
undesirable overrides. Source paths establish potential behavior, not proof that
every path ran during the user's attempts. No live decision trace was captured.

All links below are immutable references to the audited fork commit.

## Findings across all ten bosses

| Boss | Native behavior competing with ordinary tank control |
|---|---|
| **Hydross** | MT is frost tank, first OT nature tank. Fixed staging/holding positions, phase-crossing movement at 100% marks, forced RTIs/attacks. Inactive phase tank loses normal attack/reach; both lose tank-assist/formation actions. Both assigned boss tanks are excluded from scripted add pickup. Hunters route threat to the assigned phase tank; timed attack/spell suppression also covers the nonassigned tank. [Actions][ssc-hydross-actions], [multipliers][ssc-hydross-multipliers]. |
| **Lurker** | MT is placed at a fixed boss position. During submerge, MT/first OT/second OT get different guardians and fixed RTIs; the assignment trigger requires all three tank slots. Tank assist is suppressed during submerge when at least three tanks are present. Spout escape and its movement suppression are separate safety behavior, not equivalent to the fixed assignments. [Actions][ssc-lurker-actions], [multiplier][ssc-lurker-multiplier]. |
| **Leotheras** | An assistant/first available warlock is designated demon-form tank. Melee tanks are explicitly stopped/reset during demon form; an attack multiplier reinforces that. Final-phase target priority also reaches tanks. Inner Demon logic can forcibly remove a druid tank's bear aura and run a cat rotation; its multiplier suppresses tank assist and other ordinary actions. Threat-wait gates and hunter routing are coupled to the chosen warlock. Whirlwind escape is a distinct safety concern. [Actions][ssc-leo-actions], [multipliers][ssc-leo-multipliers], [warlock selection][ssc-warlock-helper]. |
| **Karathress** | **Four fixed tank assignments**: MT boss, first OT Caribdis, second OT Sharkkis, third OT Tidalvess. Separate positions, RTIs, hunter pulls and a dedicated healer station support that layout. Native restrictions suppress tank assist, formation, AvoidAoE, taunts and area threat spells, explicitly including **Consecration and Avenger's Shield**. The nominal DPS-priority routine also redirects OTs after their assigned add dies. [Actions][ssc-karathress-actions], [multipliers][ssc-karathress-multipliers]. |
| **Morogrim** | MT is moved through a scripted phase-one/phase-two route; formation movement is suppressed. Ranged are moved to a matching fixed stack, and hunter pulls feed the assigned MT. Merely deleting the MT route leaves the ranged stack positioned for a route that no longer exists. [Actions][ssc-morogrim-actions], [multipliers][ssc-morogrim-multipliers]. |
| **Vashj** | Fixed MT positioning and phase-three boss assignment; phase-two/three tank add priorities; custom targeting disables tank assist and native AvoidAoE. First OT receives striders via hunter routing; with raid cheats enabled, the tank routine adds Fear Ward and directly attacks/moves the strider. The mixed DPS routine can stop attacks/casts and teleport a bot down to the platform. Static Charge, spores, roots and core passing need separate classification; they are not all ordinary tank assignments. [Actions][ssc-vashj-actions], [multipliers][ssc-vashj-multipliers]. |
| **Al'ar** | MT/first OT rotate between platforms; second OT is assigned embers and ground positions. Phase two directly assigns embers and requests taunts, and another action swaps boss tanks for Melt Armor. Multipliers block tank assist, normal reach/facing/formation/follow and taunts with Melt Armor. Even the **Flame Quills escape** includes fixed tank waiting stations after landing. [Assignments][tk-alar-assignments], [escape/swap][tk-alar-swap], [multipliers][tk-alar-multipliers]. |
| **Void Reaver** | Any tank currently holding the boss is moved toward a fixed center position; formation is suppressed. The aggro-dump trigger and orb-avoidance trigger explicitly exclude tanks, so those should not be blamed for Ari's tank behavior. [Actions][tk-reaver-actions], [triggers][tk-reaver-triggers], [multiplier][tk-reaver-multiplier]. |
| **Solarian** | All roles can be stacked on a chosen ranged player during vanish. The Solarium Priest routine splits **melee bots, including tanks**, between two priests and forces RTIs/attacks; another multiplier disables tank assist while priests are present. Wrath escape is a separate personal hazard. [Actions][tk-solarian-actions], [multipliers][tk-solarian-multipliers]. |
| **Kael'thas** | MT assigned Sanguinar, first OT Telonicus, chosen warlock Capernian; scripted preparation positions and an assigned healer. MT is deliberately placed near Capernian to bait Conflagration. MT gets the axe while OTs get other weapons, with broad taunt/AoE restrictions. Later MT gets Kael while first/second OTs get phoenixes. Advisor attack waits, tank-assist suppression, forced RTIs and mixed target-priority routines add more control. Loot/equip/use of temporary legendary weapons, mind-control breaking, Shock Barrier, Flame Strike and Gravity Lapse are separate mechanics requiring deliberate retention/removal decisions. [Advisor/weapon actions][tk-kael-advisors], [final-phase actions][tk-kael-final], [multipliers][tk-kael-multipliers]. |

## Why fixing only the obvious tank triggers is insufficient

1. **Attack/movement gates live in independent multipliers.** Deleting a positioning
   action while keeping its suppression multiplier can leave the tank unable to
   reach or acquire anything. Conversely, removing only the multiplier leaves
   the forced attack/movement action scheduled.
2. **Mixed actions reach tanks.** Examples: Karathress kill order, Vashj phase
   priorities, Solarian priest assignments, Kael weapons and phoenix handling.
   Searching only for class names containing `Tank` misses these.
3. **Indirect threat routing matters.** Hunter encounter actions select MT/OT
   slots even though the actor is not a tank. Default class Misdirection and
   explicit player orders should not be confused with these encounter mappings.
4. **Markers are shared state.** The [shared marking helper][shared-markers]
   overwrites group icons; an action run by a DPS can thereby influence another
   bot's RTI selection or overwrite a deliberate mark. Merely excluding tank
   actors from that action does not remove this indirect control. `SetRtiTarget`
   also rewrites the acting bot's RTI preference.
5. **Caster-tank designation bypasses ordinary tank-role detection.** Both
   Leotheras and Capernian can select a warlock just for being an assistant or the
   first available warlock. A generic `IsTank(bot)` guard alone does not prevent
   this new assignment. [Leotheras helper][ssc-warlock-helper],
   [Capernian helper][tk-warlock-helper].
6. **Bookkeeping must survive as a coherent unit.** A tank may be selected as the
   mechanic-tracker bot. Excluding all tank triggers, including timers, could
   leave other bots' DPS waits permanently waiting for a timer that never starts.
   [Tracker selection][shared-tracker].
7. **Our cooperative tank policy is not a universal interception hook.** It guards
   routine scheduled acquisitions/spells; direct encounter `Attack`, spell calls
   and bespoke movement are a separate control path. Native raid behavior is not
   automatically made cooperative by setting the UI MT flag.
   [Ordinary acquisition guard][ordinary-attack], [tank spell policy][tank-policy].
8. TK's [spell listener][tk-listener] records incoming Void Reaver orb locations;
   it does not command tanks. It need not be removed to stop fixed assignments.

## Lua: what we can do now, and what is not implemented

**Yes: SSC/TK can use the existing instance-owned API2 runtime.** It operates on
raid maps generally, not just AQ40. Lua dispatches by map/entity data, and checked
publication is adopted between pulls without restarting the server. No new
per-boss native runtime is necessary. [Generic raid update][lua-update],
[current operator guide](raid-combat-lua.md).

Currently implemented:

- Ground movement goals, hold/release, bounded native route validation.
- Preferred target among observed, already-engaged enemies; normal manual target
  locks and native eligibility remain authoritative.
- Checked interrupt and dispel operations using a known native spell.
- Copied roles (`tank`, `healer`, `melee`), current victim, position, health,
  casts and bounded aura observations. Missing/overflow observations are explicit.

**Not implemented in API2:**

- A general taunt operation, explicit tank-swap authorization, or controlled
  override of normal co-tank ownership.
- A general attack/rotation pause. Movement `hold` is **not** “stop attacking.”
- Dedicated MT/OT/group-role identity fields, aura stack counts, general pet/item/
  gameobject operations, or arbitrary spellcasting. Lua receives aura IDs, not
  enough structured data to safely claim all stack-driven swaps are expressible.
- An automatic override of native encounter multipliers. Publishing a Lua target
  or position does not by itself neutralize an independent native attack blocker.

The authoritative [snapshot/intent schema][lua-schema] contains only operations
`None`, `Interrupt`, `Dispel`. [Native action admission][lua-actions] enforces that;
[preferred target arbitration][lua-target] yields to existing prioritized targets.
A target hint is not a promise to taunt, transfer threat, or bypass tank ownership.

If needed, extend the **generic** copied-observation/checked-intent API for role
flags, stack counts and a narrow explicit swap/taunt operation, with tests for
manual control, current victim, cooldown/range/LOS/immunity, stale state and
last-good/fault handling. Then author boss-specific conditions in Lua. Do not
smuggle boss IDs/coordinates/assignments into another native “generic” helper.
This is a proposed extension, not part of the deployed interface or this audit.

## Removal boundary to implement after the hazard-policy choice

- Remove native tank assignment/position/swap/forced-attack registrations and the
  matching suppression gates throughout both raids, not just Hydross.
- Remove role-specific hunter pulls and implicit warlock tank appointments.
- Prevent retained mixed DPS/utility routines from commandeering tanks, including
  indirect shared-icon and assigned-healer coupling. Role changes must be checked
  at runtime, not assumed forever from strategy-construction time.
- Keep ordinary class rotations, MT/OT acquisition/health/ownership, explicit
  commands, saved equipment and player marks authoritative.
- Keep non-tank mechanics and bookkeeping unless inseparable from a removed
  assignment; document and test any necessary coupled change.
- Decide explicitly whether native hazard escapes/personal duties may still
  control tanks. Quills' post-jump stations must be separated from its escape;
  “Inner Demon” must not silently authorize forced bear-to-cat role changes.
- Missing/faulted/expired Lua must release to ordinary tank behavior, not resurrect
  a hidden native encounter tank plan.
- Do not change encounter NPC scripts, HP/damage/healing scaling, loot tables,
  raid size, gear or progression to achieve this policy.

Future regression coverage should include all ten bosses; all affected attack,
reach, taunt and area-threat gates; MT/OT/caster tanks and role transitions;
retained non-tank behavior; hazard policy; manual RTIs; and cross-instance isolation.
Source audit alone is not a native build or live acceptance test.

[ssc-hydross-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L140-L476
[ssc-hydross-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L45-L145
[ssc-lurker-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L478-L684
[ssc-lurker-multiplier]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L191-L232
[ssc-leo-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L697-L1092
[ssc-leo-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L258-L401
[ssc-warlock-helper]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCHelpers.cpp#L124-L155
[ssc-karathress-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L1096-L1465
[ssc-karathress-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L419-L533
[ssc-morogrim-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L1468-L1635
[ssc-morogrim-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L535-L589
[ssc-vashj-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCActions.cpp#L1639-L2063
[ssc-vashj-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/SSC/SSCMultipliers.cpp#L591-L786
[tk-alar-assignments]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L45-L377
[tk-alar-swap]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L380-L501
[tk-alar-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKMultipliers.cpp#L26-L120
[tk-reaver-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L611-L656
[tk-reaver-triggers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKTriggers.cpp#L130-L193
[tk-reaver-multiplier]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKMultipliers.cpp#L122-L134
[tk-solarian-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L817-L980
[tk-solarian-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKMultipliers.cpp#L136-L177
[tk-kael-advisors]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L999-L1582
[tk-kael-final]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKActions.cpp#L1795-L2100
[tk-kael-multipliers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/TKMultipliers.cpp#L179-L429
[tk-warlock-helper]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/Util/TKHelpers.cpp#L325-L350
[tk-listener]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/TK/Util/TKScripts.cpp#L16-L53
[shared-markers]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/RaidBossHelpers.cpp#L16-L105
[shared-tracker]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/RaidBossHelpers.cpp#L107-L128
[ordinary-attack]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Base/Actions/AttackAction.cpp#L20-L45
[tank-policy]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Base/Util/TankModes.cpp#L131-L192
[lua-update]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/Policy/RaidCombatPolicy.cpp#L189-L262
[lua-schema]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/Policy/RaidCombatData.h#L12-L63
[lua-actions]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Raid/Policy/RaidCombatActions.cpp#L93-L163
[lua-target]: https://github.com/CmPons/mod-playerbots/blob/f01eab18ed6dfa4093f9ec4fc5008e59011b315b/src/Ai/Base/Value/CurrentTargetValue.cpp#L12-L29
