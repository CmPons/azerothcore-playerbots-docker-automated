# Ari apparent inactivity — live capture, October 9

User requested an immediate read-only capture. No strategy/role/gear commands,
configuration changes, builds, restarts, or gameplay SQL were performed.

## Evidence

Private directory: `backups/ari-live-investigation-20261009-194422/`;
pointer `/tmp/ari-live-investigation`.

- Bounded capture: **19:44:22.622–19:47:34.368 CEST**, 30 rounds / 240 responses,
  Ari142, Ailina1180, Meliah815, Keilmere1315. Completed automatically, no query errors.
- Separate 12-round Ari movement/target-position capture, one second apart.
- Initial ad-hoc query, before the bounded capture, reported combat, Ari88% HP,
  target Devastation, `reach melee - FAILED`, `melee - FAILED`, formation multiplier
  rejection and `no actions executed`. Its exact query timestamp was not recorded.
- `values` uses a separate connection drained until idle, preserving raw uint8 bytes
  in JSON; it is never mixed into the line-framed basic queries.
- Basic/action/value reads are consecutive, non-atomic and sometimes cached. Action
  history is a rolling record, not a timestamped combat log. The second percentage in
  remote `hp` is **target HP**, not Ari's mana.

## What the capture actually establishes

Ari was **not continuously inactive** during this window:

| Sample time CEST | Recorded successful action / observation |
| --- | --- |
| 19:44:22 | `lay on hands on party - OK`; current target Infinity Blades |
| 19:44:29–35 | `my attacker count=6`; target Phaseshift Bulwark |
| 19:44:35 | `consecration - OK` |
| 19:45:14 | `equip upgrades packet action - OK` (not proof of an actual item move) |
| 19:45:21 | `judgement of wisdom - OK` |
| 19:45:47 | `loot - OK` |
| 19:45:53 | `reach melee - OK`, target Thaladred |
| 19:46:00 onward | Target Telonicus; successful facing actions and later spell rejections |

`MyAttackerCountValue::Calculate` returns `bot->getAttackers().size()`, so the six
attackers are stronger evidence of real tanking load than simply having a selected
target. Conversely, `has aggro` can also mean acquisition is prohibited against
another tank's target, and is **not** used here as proof of exclusive ownership.

Ari remained at roughly `(768.807,28.0919,46.7846)` through much of the weapons capture.
At19:45:20 the shield was at `(766.150,26.7117,46.7788)`, approximately **3 yards away**.
Remaining stationary in that sample is not by itself evidence of a pathing failure.
Decoded mana samples were44–100% during the weapon window; the capture does not support
an out-of-mana explanation for the whole period.

The priests also recorded successful heals/shields. Ailina was already dead in the
first bounded sample and later alive; the capture does not establish her earlier death
cause, who revived her, or her pre-death tank/healer assignment.

## Important interpretation correction

The first response identified failed reach/melee attempts. **Those labels alone do not
prove that movement or damage was blocked.** After reading the actual implementation:

- `MeleeAction` inherits `AttackAction`. `AttackAction::Attack` deliberately returns
  false when already attacking the same target in the same mode. That can produce
  `melee - FAILED` even while auto-attacks continue outside the AI action scheduler.
- The same false result can also mean other failures: target validity, LOS, flight,
  vehicle restrictions, or the separate tank admission guard. This capture cannot
  identify which return branch was taken for each failure.
- `ReachCombatTo` can return false for movement restrictions/path/claim/hazard checks,
  **or because already within its computed reach distance**. The initial reach failure
  did not capture enough state to distinguish those cases.
- TK explicitly suppressed `combat formation move` in sampled histories. That is
  distinct from the generic reach-melee action and not proof that every movement path
  was prohibited.
- Repeated spell `IMPOSSIBLE` results remain unexplained individually. Saved spell
  data includes Hammer of the Righteous53595 and judgements, but not Shield of
  Righteousness53600/61411 or Divine Plea54428. The saved mainhand/offhand were full
  durability90/90 and120/120; these DB observations can lag live equipment/spells by
  the five-minute save interval and are not a complete live gear/auras inspection.

**Conclusion:** captured tanking load and successful abilities contradict a continuous
freeze. No Ari threat-multiplier veto was established in the cited idle-looking samples.
The trace does not prove normal DPS output, nor identify every spell rejection or the
reason for the user's earlier visual observation. Do not relabel this as another proven
healing-threshold failure, a confirmed pathing bug, or a fixed defect.

Relevant source: `GenericActions.h` (MeleeAction), `AttackAction.cpp`,
`ReachTargetActions.cpp`, `MovementActions.cpp::ReachCombatTo`,
`AttackerCountValues.cpp`, and `PlayerbotAI.cpp::HandleRemoteCommand`.
