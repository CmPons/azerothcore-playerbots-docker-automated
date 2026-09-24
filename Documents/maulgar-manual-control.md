# Manual Maulgar control — source only

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

The taunt hook/helper are deleted; `PlayerbotAI.cpp`, `AttackerCountValues.cpp`,
`TankTargetValue.cpp` and `Playerbots.cpp` are byte-for-byte restored to their
pre-change versions at `693840886d1db462c141f4d31cbb606eb96b8a84`.
The only remaining native difference from that revision is `GruulStrategy.cpp`.
Associated taunt tests/documentation were removed; the Maulgar/Gruul registration
check is retained in `scripts/tests/test_maulgar_manual_control.py`.

No replacement taunt policy, MT role change or flag-persistence fix is implemented.
The earlier duplicate-MT observation was from saved database rows, **not live group
memory**. Core `RemoveUniqueGroupMemberFlag` clears old flags in memory while
`SetGroupMemberFlag` persists only the assigned member, so stale saved flags are a
possible explanation. The actual fight-time assignments were not established.

## Verification and deployment

- Exact native diff against the pre-change revision: only `GruulStrategy.cpp`.
- `python -m unittest scripts.tests.test_maulgar_manual_control -v` passes.
- The retained strategy translation unit previously passed native-header syntax
  checks and has not changed during this rollback.
- Source commits and `repo-pins.txt` are published through the normal fork workflow.

**Neither the original changes nor this rollback were deployed.** No server build,
restart, live strategy/config change, role/profile modification or database write
was performed. Maulgar's strategy removal still requires a separately authorized
build/deployment. Await the user's requirements before changing tank behavior again.
