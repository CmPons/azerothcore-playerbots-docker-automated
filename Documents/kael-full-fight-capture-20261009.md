# Kael full-attempt capture — October 9, 2026

## Scope and user clarification

User reports **no GM interventions** and **no bear tank** on this attempt. Ailina was
healing; her captured combat strategies include `resto`, `save mana`, `tranquility`
and `threat`. Do not interpret this as the earlier bear attempt or the earlier short
Ari capture.

Read-only sampling of all nine bots: **20:22:50.649–20:33:15.875 CEST**, 52 rounds /
**936 responses**, no query errors. User requested stop; process finished 20:33:18.941.
No capture remains running. First samples already show Thaladred, so this does not
include the entire introductory speech. Full nine-bot rounds took about 12 seconds.

Private evidence: `backups/kael-full-fight-20261009-202250/`; pointer
`/tmp/kael-full-fight-capture`. Raw `live-samples.jsonl`, bounded collector, separate
movement/target coordinates, `state-target-timeline.txt`, and `kael-active-detail.txt`.

No server/config/strategy/gear changes or gameplay SQL. Readings are sequential,
non-atomic and sometimes cached; action strings are rolling history, not per-cast
combat logs. Remote `hp` shows **bot HP / selected-target HP**, not mana. Raw uint8
values are decoded separately. No raw damage events, aura timeline, exact hostile
victim GUIDs, or human Redshift action stream were recorded.

## Progress was real

The capture is consistent with the user's report of substantially cleaner weapons
and revived advisors. No bot dead-state sample appears before the active Kael portion;
all nine are seen alive entering it. There is some last-advisor overlap: Capernian was
still selected at low HP in some samples after other bots began selecting Kael.
Do not claim all advisors were dead before his activation or invent an exact activation
second from the first bot target change (20:30:21).

The first dead-state samples were:

| Bot | First sampled dead, CEST |
| --- | --- |
| Arinerica | 20:32:06 |
| Beliona | 20:32:12 |
| Feelesia | 20:32:27 |
| Meliah | 20:32:32 |
| Keilmere | 20:32:33 |
| Kaaren | 20:32:38 |
| Ailina | 20:32:55 |
| Raney | 20:32:59 |

Pilbok was not sampled dead by capture end. These are first observations, not exact
kill timestamps. Meliah earlier briefly displayed 0% while still in the combat engine,
then 100%; integer percentages/states alone do not establish death/resurrection, and
this is **not** evidence contradicting the user's no-GM account.

## Intended phoenix behavior in the installed strategy

`TKActions.cpp::KaelthasSunstriderHandlePhoenixesAndEggsAction`:

- First living assist tank chooses the first alive phoenix by GUID, marks square,
  attacks it, and attempts to move 12yd away from nearby non-tanks once it is attacking
  that tank. With Redshift MT and Ailina healing, Ari is the intended first assist tank.
- Second assist tank only receives a target when at least two phoenixes are found.
- DPS prioritize eggs when Shock Barrier is absent; other bots avoid phoenix proximity.
- The trigger explicitly refuses this job for a tank who is **Kael's current victim**.
  Boss selection is not proof of victim ownership. Whether this guard prevented Ari's
  early pickup was not directly captured.
- The action uses per-bot target discovery, sorts by GUID, and does not provide an
  explicit one-tank assignment to every additional phoenix. It is not a complete
  threat-aware multi-add ownership controller.

This is tank/add control with separation, not an instruction to leave phoenixes on
healers. Human Redshift's job is Kael; the NPC MT movement routine does not move him.

## Observed phoenix-control gap

- **20:30:44–45:** Phoenix is found in healer values.
- **20:30:53–20:31:30:** Ari continues selecting Kael in every sampled round. Her values
  already find Phoenix from 20:30:54, so this is not evidence of universal invisibility.
- **20:30:55:** healer Ailina is 44% HP in the basic query (34% in the subsequent values),
  has one attacker and is approximately**3.4yd** from the phoenix she selects.
- **20:31:07:** Ailina is approximately**1.5yd** from it at `(683.933,65.6882)`, far from
  the boss-side cluster. This is consistent with an uncontrolled phoenix pressuring
  the healer; it is not normal evidence of a bear doing her assigned job.
- **20:31:20:** Meliah is 24% HP, one attacker, approximately**4.8yd** from her selected
  phoenix. The action history records Pain Suppression OK.
- **20:31:54:** Ari first selects Phoenix in the sampled timeline, at 70% HP. Ari is
  `(750.202,21.349,46.7788)` and the selected phoenix is
  `(700.681,67.8688,46.8096)`: approximately**67.94yd** apart.
- **20:32:06:** Ari is sampled dead. The capture does not identify what killed her and
  does not prove that selecting the distant phoenix meant she had successfully tanked it.

This supports a real failure to establish prompt, stable phoenix coverage. It does
**not** yet identify the exact reason Ari's early pickup was absent: role/trigger state,
Kael victim ownership, control effects, competing actions and other admission paths
are not fully observable through this endpoint.

## Concrete additional blocker: threat vetoes the mixed phoenix action

At **20:30:55**, Ailina's history explicitly contains:

```text
Multiplier threat made action kael'thas sunstrider handle phoenixes and eggs useless
A:kael'thas sunstrider handle phoenixes and eggs - IMPOSSIBLE
```

The class inherits **AttackAction**, but its healer path includes **moving away from
phoenixes**. Therefore this threat veto can suppress a defensive avoidance operation,
not just an offensive spell. The new emergency-healing bypass only exempts genuine
healing actions; it does not solve this mixed attack/movement-action classification.

The immediately queried threat values are cached/non-atomic and do not establish the
exact internal gate for the historical veto. Do not claim it was conclusively the 80%
check from these ratios. Nor does one veto prove all movement was blocked throughout;
Ailina demonstrably moved between snapshots.

Pilbok's phoenix action was also threat-vetoed at 20:31:48, but his DPS branch can attack
eggs, so that veto must not automatically be called a healer-escape veto.

## Follow-up: live-phoenix DPS versus egg DPS

The user clarified that these were **two separate attempts**:

- In the recorded attempt, Ari did have some Kael aggro initially; Redshift then corrected
  it. This supports the relevance of the boss-victim guard, but its exact invocation and
  duration were not recorded. Do not describe boss ownership as wrong for the whole phase.
- A second, **uncaptured** attempt again reached Kael cleanly, then collapsed after phoenix
  spawning, with more than one phoenix alive. Do not attribute the first capture's exact
  timestamps, targets or vetoes to that second attempt.

Multiple living phoenixes can reflect new-spawn overlap, missed eggs allowing rebirth,
or both. Their number alone does not distinguish insufficient live-phoenix DPS from
failed egg cleanup. The repeat failure strengthens the case for investigating the entire
pickup/separation/burnout/egg cycle, rather than treating initial Kael ownership as the
complete explanation.

The installed encounter action **does not explicitly assign DPS to living phoenixes**.
It assigns assist tanks to phoenixes, DPS to eggs when Shock Barrier is absent, and
otherwise avoidance. Generic targeting, marks, pets, cleave and damage-over-time effects
can still cause phoenix damage; this is not proof of zero damage.

In the recorded pull's basic samples from 20:30:40 onward, among Raney, Beliona, Pilbok,
Kaaren and Feelesia, only Raney once selected a living Phoenix. Kaaren and Feelesia each
selected an egg once, at the later timestamps noted below. This supports the user's
impression of no coordinated live-phoenix burn, not a damage-meter claim.

Phoenixes lose health through Burn themselves, so declining HP is not proof the raid
was focusing them. The native SmartAI data casts Burn 36720, creates egg 21364 at the
burnout threshold, and gives the egg a 15000ms update event to initiate rebirth. A tank
holding phoenixes away while the raid kills eggs is a valid approach, but relies on
reliable containment and timely egg kills. Those prerequisites were not demonstrated
here. Deliberate live-phoenix DPS could shorten exposure, but does not replace pickup,
separation or killing the resulting egg; Shock Barrier remains a competing priority.

## Other notable signals

Mind-control handling was active: Kaaren recorded break-mind-control OK at 20:30:37 and
20:32:02; other attempts failed. Party members became selected targets, including Pilbok,
Ari and Keilmere. Those are consistent with the mechanic, **not evidence of GM activity**.
An OK result can mean movement toward a controlled player or a spell attempt, not proof
that mind control was removed. Exact aura ownership/duration and damage attribution
remain unproven.

Feelesia and Kaaren eventually select Phoenix Egg at 20:32:15 and 20:32:26, respectively,
after Ari's death. This proves some egg targeting, not timely handling of every egg
or which eggs hatched. Several bots also alternate noncombat/targetless and combat
states during active boss combat; those transitions need separate tracing before being
labeled deadlocks or mind-control events.

**Next investigation priority:** reliable first-phoenix pickup, safe healer avoidance
that is not treated as damage admission, and interaction with mind control/boss ownership.
This does not justify reverting the successful earlier fixes, declaring a gear wall,
or assuming a bear is the missing solution. No implementation or service change was
performed. Any new encounter-specific assignments/positioning policy must follow the
workspace's explicit Lua-policy requirement.

## Source and evidence identity

Installed playerbots revision: `68556d789ee3420ff0eadaca11d5e9b03828f473`.

- [Phoenix actions](https://github.com/CmPons/mod-playerbots/blob/68556d789ee3420ff0eadaca11d5e9b03828f473/src/Ai/Raid/TK/TKActions.cpp#L1898-L1988)
- [Phoenix and mind-control triggers](https://github.com/CmPons/mod-playerbots/blob/68556d789ee3420ff0eadaca11d5e9b03828f473/src/Ai/Raid/TK/TKTriggers.cpp#L479-L518)
- [Threat multiplier and healing-only bypass](https://github.com/CmPons/mod-playerbots/blob/68556d789ee3420ff0eadaca11d5e9b03828f473/src/Ai/Base/Strategy/ThreatStrategy.cpp#L21-L95)

Raw `live-samples.jsonl` SHA256:
`74d4006258a0dc0abeae900acce3ad15d2c289dd5e3c6eec13959e4813f63e8a`.
Private `SHA256SUMS` preserves the completed capture files and derived reports.
