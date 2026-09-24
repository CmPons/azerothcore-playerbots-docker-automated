# Manual Maulgar control

The user requested removal of Maulgar's encounter-specific bot behavior while
preserving Gruul's own tactics. `GruulStrategy.cpp` no longer registers Maulgar's
ten triggers and five multipliers. Normal class/group behavior remains active.
Gruul's positioning, ranged spread and Shatter triggers/multipliers are unchanged.

This removes the council's forced marks, tank/mage/Moonkin assignments, DPS order,
movement, Misdirection, Banish, Bloodlust delay and encounter-specific spell
restrictions. It also removes its dedicated mage-tank/Spellsteal sequence and
hazard avoidance; normal class behavior/manual control must handle those mechanics.
Generic marking strategies and existing personal RTI preferences are not changed.

## Taunt/MT changes reverted at user request

The separate cooperative-target/taunt policy from playerbots commit `560817b1`
was too restrictive for the user's existing tank-swap play. It has been reverted
in `62a53835646b37e007942736c242f3d718341744`, pending further discussion.

At that rollback, the taunt hook/helper were deleted; `PlayerbotAI.cpp`,
`AttackerCountValues.cpp`, `TankTargetValue.cpp` and `Playerbots.cpp` were restored
byte-for-byte to `693840886d1db462c141f4d31cbb606eb96b8a84`.
The only remaining native difference at that point was `GruulStrategy.cpp`.
Associated taunt tests/documentation were removed; the Maulgar/Gruul registration
check is retained in `scripts/tests/test_maulgar_manual_control.py`.

No replacement policy was included in that rollback. The user's later request for
explicit modes is documented separately in [playerbot-tank-modes.md](playerbot-tank-modes.md).
The earlier duplicate-MT observation was from saved database rows, **not live group
memory**. The pre-fix core cleared old flags in memory but persisted only the
newly assigned member, which could leave stale saved flags. The later tank-mode
work fixes that persistence gap; it does not establish the actual fight-time
assignments from that earlier encounter.

## Verification and deployment

- At the rollback checkpoint, exact native diff against the pre-change revision:
  only `GruulStrategy.cpp`.
- `python -m unittest scripts.tests.test_maulgar_manual_control -v` passes.
- The retained strategy translation unit previously passed native-header syntax
  checks and has not changed during this rollback.
- Source commits and `repo-pins.txt` are published through the normal fork workflow.

The rejected blanket policy and its rollback were never deployed. Maulgar's retained
strategy removal was subsequently deployed with the separately requested tank modes
on **September24, 2026**, after explicit authorization. See
[tank-modes-deployment-20260924.md](tank-modes-deployment-20260924.md) for verification
and startup exceptions. The new modes do not restore the reverted global taunt hook.
