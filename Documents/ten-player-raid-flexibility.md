# Ten-player progression: flexible roles before a bigger roster

## Player preference

Keep Redshift and the established nine companions as the default raid. The user
likes the class coverage, familiar companions and generous individual loot
progression: a "Diablo with companions" experience. Gearing fifteen additional
bots just to match native 25-player strategies is not the preferred solution.

Ailina is normally Restoration, but a reversible bear-tank setup is a useful
third-tank option when Redshift and Arinerica need help. A few reserve companions
may be useful later; neither recruitment nor gearing changes are authorized by
this preference alone.

The user controls **only Redshift**. Advice must explain what the bots actually
will do and how the human can adapt, not assume the user can simultaneously
position, interrupt, target and tank on behalf of nine other players.

## Al'ar: successful live gameplay report

The user reported killing Al'ar with the same ten characters after changing
Ailina to bear tank:

- Redshift and Arinerica covered the boss platforms.
- Ailina handled the Embers successfully; the user said she did "just fine."
- Ranged DPS killed the Embers rather than leaving her to hold them indefinitely.
- The roster traded a healer for a tank: three tanks, two healers and five DPS.
- Kaaren died during the attempt; the user attributed this to Redshift reaching
  the wrong platform too late. The kill therefore succeeded with one DPS down
  for part of the fight, not necessarily for its entire duration.
- No Al'ar-specific tuning or source deployment was performed for this trial.
  Existing ten-player scaling remained the baseline.

This is **user-reported live acceptance of this role arrangement**, not proof
that every native action executes correctly or that Ailina is fully geared for
all raid tanking. A pre-attempt read-only check found one Restoration spec and
mostly caster gear. A secondary `bear pve` spec was suggested; the exact final
saved talents, equipment and any later return to Restoration were not audited.
Do not assume her current role from this historical record.

### Native strategy verified before the attempt

With `+tempestkeep`, the registered Al'ar routines explicitly allocate:

| Role | Phase one |
| --- | --- |
| Main tank | Platforms 1 and 3 |
| First off-tank | Platforms 2 and 4 |
| Second off-tank | Ground-level Ember pickup and separation from the raid |

Ranged DPS prioritize Embers in both phases, first moving at least 16 yards away
from their death explosion. In phase two the second OT handles one Ember; the
MT/first OT not currently tanking Al'ar is selected for the other. A human
selected for a job still has to perform it manually.

OT ordering prioritizes raid assistants, then group iteration order, and these
encounter checks ignore dead players. With Redshift marked MT, Ari as an
assistant and Ailina not an assistant, the intended living three-tank ordering
is Ari first OT and Ailina second OT. Deaths or roster/role changes can alter it.

Verified against playerbots commit
`cef0162a7202f7a10688e989c0dba5d76828dbb1`:

- [Registered Al'ar actions](https://github.com/CmPons/mod-playerbots/blob/cef0162a7202f7a10688e989c0dba5d76828dbb1/src/Ai/Raid/TK/TKStrategy.cpp#L20-L44)
- [Platform, Ember tank and ranged DPS routines](https://github.com/CmPons/mod-playerbots/blob/cef0162a7202f7a10688e989c0dba5d76828dbb1/src/Ai/Raid/TK/TKActions.cpp#L61-L377)
- [Ember triggers](https://github.com/CmPons/mod-playerbots/blob/cef0162a7202f7a10688e989c0dba5d76828dbb1/src/Ai/Raid/TK/TKTriggers.cpp#L55-L64)
- [Off-tank ordering](https://github.com/CmPons/mod-playerbots/blob/cef0162a7202f7a10688e989c0dba5d76828dbb1/src/Bot/PlayerbotAI.cpp#L2544-L2602)

## Default approach to future encounters

1. **Check the actual native workload and assignments first.** Identify missing
   simultaneous jobs, targeting restrictions and role ordering, not just boss HP.
2. **Try a reversible role swap within the ten** when it covers a missing job.
   Ailina's bear setup is now a demonstrated option, not a speculative demand to
   recruit a new tank. Check her current state and suitable owned gear first.
3. **Retain useful encounter helpers where practical.** Do not assume disabling
   a whole strategy is always better, or that enabling it respects ordinary MT/OT
   behavior. Inspect the specific encounter before giving instructions.
4. **Consider a small reserve roster before a full 25-player conversion**, if
   the user wants one. Do not start gearing or recruiting companions unasked.
5. **Tune only the demonstrated remaining problem, with approval.** The user is
   comfortable with modest damage tuning for a ten-player BC tour; role swaps
   are worth trying first. Willingness to tune is not blanket authorization to
   edit settings or deploy code.

Preserve Restoration talents, gear, gems and progression. Do not use
`.raidroster sync` as a convenient respec: it can rebuild much more than talents.
Off-spec gearing and automatic equipment selection should be deliberate; a
spec switch alone is not a promise to restore the previous equipment set.

## Lessons from SSC and limits of the conclusion

Karathress was also cleared with `-ssc`, Caribdis first, Redshift separating
Karathress and Ari holding the guards. That was a workable manual arrangement
for this roster, not proof that all native encounter assistance should be removed.

Vashj remains a different workload problem: the user reported killing Elites
and Striders with `-ssc` while ignoring Enchanted Elementals; `+ssc` successfully
disabled at least one generator but diverted substantial DPS toward elementals.
That demonstrates some working pieces, **not** that all jobs can be covered
simultaneously or that broad HP/spawn-rate nerfs are necessary. Al'ar's third-tank
success does not resolve Vashj's elemental interception/core-delivery allocation.

The proposed explicit OT raid-marker ownership (for example, Ari owns square
and circle, then falls back to normal OT behavior) remains an **unimplemented
idea**. See the [SSC/TK tank-authority audit](ssc-tk-tank-authority-audit-20260925.md)
and the encounter tank-authority policy in `../AGENTS.md`; do not describe that
feature as available or introduce new hardcoded assignments without agreement.
