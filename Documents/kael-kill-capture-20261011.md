# Kael kill capture — October 11, 2026

## Outcome and scope

The user reports **Kael killed**, classifying this as **partially assisted**:
they think they may have used GM `.revive` on Raney. Preserve that uncertainty;
this is not an independently confirmed revive or an unassisted benchmark.
Raney has no sampled dead state, but sequential samples cannot exclude a death
and revive between observations. Do not merge this attempt with October 9/10 pulls.

Capture started during initial **Sanguinar**, not at encounter start. The user's
initial “Thaladred” report was immediately corrected to Sanguinar. Kael was sampled
at4% around10:49:15 CEST; later samples show combat ending, consistent with the
reported kill. They do not supply an exact lethal-hit timestamp or damage log.

Private evidence: `backups/kael-mc-live-20261011-103800/`.

- Full capture: **1,170 responses**,10:38:00.959–10:51:05.709 CEST.
- Fast MC capture: **1,075 responses**,10:38:00.960–10:51:03.487 CEST.
- No recorded subprocess/query errors in either collector.
- STOP requested after the kill report; both collectors finished by10:51:09.
- `user-observations.jsonl` records report times, not exact symptom onset.
- `late-healing-and-kaaren.txt`, `healing-report-context.txt` and
  `timestamped-mc-events.log` retain focused evidence; `SHA256SUMS` seals raw files.

This was read-only: no bot commands, strategy/config edits, SQL writes, source
implementation, build or service restart. The October 10 threat/policy deployment
remained installed; this capture alone does not fully validate its encounter behavior.

## Initial Sanguinar: separate DPS-start wait

At10:38:00 Ari selected Sanguinar at37% target health. DPS bots were in non-combat
state with empty targets. Their action histories explicitly named
**`kael'thas sunstrider wait for dps multiplier`**, blocking DPS assist and the
advisor-priority action. The block persisted across multiple samples; Ari still
selected Sanguinar at18% around10:38:37. This is distinct from the shared threat
multiplier corrected on October 10, and precedes the revived-advisor Lua policy.

Ailina's designated timer-management action reported `FAILED` at10:38:02 and
10:38:26. Current production source:

- `TKMultipliers.cpp::KaelthasSunstriderWaitForDpsMultiplier::GetValue` applies only
  in `PHASE_SINGLE_ADVISOR`; a missing timer or timer younger than10 seconds invokes
  the wait, with role/healing exceptions.
- `TKActions.cpp::KaelthasSunstriderManageAdvisorDpsTimerAction::Execute` writes the
  timer only when a discovered tankable advisor is at **exactly full health** and
  not flagged non-attackable. Thaladred is not in its three-entry list.

A missed full-health initialization window can therefore leave the wait unbounded.
That is a source-demonstrable failure path, consistent with the captured failures.
The actual timer map/value was **not** observed, so missing initialization versus
intervening timer refreshes is not completely traced. Earlier conversational
wording calling the missing timer confirmed was too definite. Do not solve this
by disabling all threat safeguards, removing the entire TK strategy, or forcing
another full pull merely to reproduce these already-recorded vetoes.

## Kaaren: real gaps, but not inactive throughout

The user's AFK report has supporting evidence:

-10:45:49–10:46:13: repeated non-combat/empty-target samples at the same position,
  with failed boss-positioning actions.
-10:47:01–10:47:26: another repeated non-combat/empty-target, stationary sequence.
  Subsequent samples include movement and an off-heal, not continuous inactivity.
-10:48:02–10:48:14: explicit gravity-lapse multiplier vetoes of DPS assist; healing
  attempts also reported `FAILED`.
-10:48:26,10:48:38 and10:49:15: Kael selected with many melee ability
  `IMPOSSIBLE`/melee `FAILED` results. Sampled center distances are roughly16–17
  yards, not proof of melee contact or of the actual rejection reason.

Some restrictions are intentional mechanics: the existing gravity-lapse multiplier
blocks `MovementAction` except its midair-spread action, which also catches inherited
attack/reach actions. The capture does **not** yet distinguish appropriate downtime
from a failure to resume attacking, nor prove why casts/reach attempts failed.
`FAILED` melee can mean an attack was already started; rolling action counts are
not casts-per-second or a damage meter.

Kaaren also has successful offensive actions, off-healing, egg handling and—most
conclusively—a successful MC rescue. It would be wrong to conclude she did nothing
throughout the encounter.

## Live MC rescue evidence

Timestamped worldserver messages (converted to CEST):

| Time | Rescuer / action | Target | Immediate MC state |
| --- | --- | --- | --- |
|10:44:43.249 |Pilbok / Shiv |Kaaren |absent |
|10:45:07.993 |Pilbok / Shiv |Feelesia |present |
|10:45:08.720 |Kaaren / Stormstrike |Feelesia |absent |

Kaaren was herself rescued, then later rescued Feelesia. Pilbok's first submission
on Feelesia did **not** immediately remove MC; Kaaren's follow-up was about0.73
seconds later. These aura-state logs are stronger evidence than action `OK` alone,
but do not establish a guaranteed proc on every submission or exhaustively prove
the number of targets in every MC wave.

## Ari's low-health window: mana was available

Ari's basic sample at10:48:30.926 shows **6% self HP with Phoenix selected**.
Selection is not independent proof of the Phoenix's actual victim. Healer value
samples around that window show:

| Time (CEST) | Healer | Self mana | Selected heal recipient / cached HP |
| --- | --- | ---: | --- |
|10:48:20.007 |Ailina |100% |Ari /19% |
|10:48:22.258 |Keilmere |100% |Ari /6% |
|10:48:32.122 |Ailina |100% |Ari /6% |
|10:48:33.245 |Meliah |82% |Ari /6% |
|10:48:34.371 |Keilmere |100% |Redshift /24% |

Nearby Ailina healing-touch/rejuvenation attempts and Keilmere renew attempts
reported `FAILED`; Meliah's heal fallbacks reported `IMPOSSIBLE`. Ailina also
executed phoenix-handling movement around10:48:32. Ari subsequently appeared at78%
HP around10:48:43; this does not identify the healer, spell or other recovery source.

These are sequential, potentially cached values—not an atomic healer-decision
trace. Still, the evidence is not compatible with simply assuming all healers were
out of mana or intentionally refusing to select Ari until an emergency threshold.
They sometimes selected her while having substantial mana, yet healing actions
failed. Some selection also went to other injured members. Range/LOS, movement,
control state, phase transitions and action admission need investigation before
claiming a root cause. Normal current-target threat ratios are not the exact
special boss-gate denominator; Ailina's AoE ratio also reached99 in this window.
There is no captured first-blocking threat verdict proving that caused these heals
to fail. No healing exemption or threshold override was applied by the assistant.

## Follow-up boundaries

Priorities supported by this capture:

1. Bound/fix the initial advisor wait's missing-timer path without changing human
   pull authority or disabling encounter mechanics.
2. Trace failed urgent heals with selected recipient and actual cast/movement/LOS
   rejection reasons, rather than guessing from `FAILED` alone.
3. Examine Kaaren's target loss, positioning and re-engagement around phase/gravity
   transitions; preserve necessary midair avoidance and MC rescue.

No new implementation or deployment is implied by this investigation. The win is
recorded with its assistance qualification, and the remaining bot problems remain
open rather than being dismissed because the boss died.
