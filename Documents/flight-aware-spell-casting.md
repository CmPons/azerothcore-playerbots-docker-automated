# Flight-aware shared spell casting (October 11)

## Scope

`mod-playerbots/src/Bot/PlayerbotAI.cpp` previously rejected **all** flying bots
before preparing a spell, including unmounted encounter flight. Both unit- and
position-targeted casts now share admission with their matching feasibility checks:

- Taxi / `UNIT_STATE_IN_FLIGHT`: still rejected.
- Flying with a current `SPELL_AURA_FLY` (201), unmounted, outside a vehicle and
  outside either druid flight form: admitted to the existing spell pipeline.
- Flying mount, vehicle, flight form, or flying/disable-gravity flags without the
  granting aura: still rejected. No flags are cleared or synthesized.
- Grounded casting retains its existing rules, including native mounted-spell rules.

This is a generic casting repair, not a Kael/healer exception. It covers friendly
heals, buffs and dispels as well as offensive and destination spells. Admission
is **not** cast success: spell knowledge, target validity, cooldowns, mana/reagents,
range, LOS, movement/control restrictions and native `Spell::CheckCast` / `prepare`
still apply. Existing pet-autocast control and vehicle-specific casting paths stay
unchanged. Melee range, pull authority, threat policy, Gravity Lapse spreading,
Lua policies and the explicitly deferred initial-advisor timer are untouched.

The destination overload also now rejects an invalid spell definition safely and
returns false on a native `prepare` error instead of reporting success. Spell
ownership on a native preparation failure follows the existing unit-target path;
there is no additional cancel/delete of that engine-owned spell.

## Bounded observations

`PlayerbotSpellCasting.cpp` and `SpellCastAdmission.h` provide per-bot observations,
without retaining world pointers or changing a cast decision. Capture is limited
to known player spells in human-led raid groups inside raid maps. There are three
mutex-protected latest snapshots: feasibility rejection, execution rejection and
positive-spell rejection on a friendly target strictly below 30% health. An urgent
observation is diagnostic only—not a healing/threat exemption. Instance changes
reset the snapshots on the next recorded failure; AI destruction discards them.

Server log marker: **`[PlayerbotCastFailure]`**. Ordinary and urgent lanes each
allow one line per five seconds per eligible bot; a map/instance scope change
resets these limits. Both lanes update snapshots even when logging is suppressed.
Read-only command-server query: **`castfail,<bot-guid>`**.
Each snapshot supplies age, map/instance, spell, submitted recipient GUID/health
(or requested destination coordinates), caster mana, center-to-center 3D distance,
heights, flying/taxi/mount/vehicle/aura/form/moving flags and unit-state bits.

- `stage=check` versus `stage=execute` distinguishes feasibility from submission.
- `reason=flight`, `lost-control`, `moving`, `stand-up-retry`: earlier AI refusal;
  `result=-1`, not an invented native error.
- `reason=core`: the exact numeric native `SpellCastResult`, interpreted using
  this core's `SharedDefines.h`.
- Routine feasibility `SPELL_FAILED_NOT_READY` is excluded. These are **selected
  failures**, not a complete action/combat log: unknown spells, other early gates,
  strategy/multiplier vetoes and eventual impact/aura removal are not captured.
  `observed_failures=0` does not prove all casts succeeded. Always inspect ages.

No live logging configuration or bridge change is needed.

## Validation

`scripts/tests/test_flight_casting.py` compiles the complete production bodies of
both feasibility and both execution overloads, plus the actual diagnostic adapter
and admission/trace header, against controlled game doubles. The actual core
`IsFlying`/`HasFlyAura` inline bodies and flag constants also exercise both FLYING
and DISABLE_GRAVITY. Native spell-check
outcomes are injected at the unchanged core boundary; this is not an end-to-end
raid simulation. ASan/UBSan cover execution, snapshot storage and a concurrent
reader/writer exercise.

Coverage includes positive/offensive and instant/cast-time spells; unit/destination
paths; taxi/mount/vehicle/form/stale-flag exclusions; cooldown, spell knowledge,
control, movement, standing retries; injected native LOS/resource/control/range
errors; retained pet behavior; snapshot scoping, urgent retention, five-second
limits and clock wrap. The old source fails both flight admission and ground-cast
false-success cases. Removing the execution recheck or ignoring the destination
preparation result also fails. Exact-source comparisons protect TK mechanics,
advisor timer, shared movement/threat and vehicle/gameobject feasibility paths.
Production-header syntax checks and a full image build are separate validation.

The October 11 capture did **not** record flight flags or native spell errors.
The old veto is source-confirmed; blaming every missed heal/DPS gap on it remains
unproven. This change does not claim to fix Kaaren's separate empty-target periods.
See [the capture](kael-kill-capture-20261011.md) and
[deployment closeout](flight-casting-deployment-20261011.md) for live identity,
preservation qualifications and remaining gameplay acceptance.
