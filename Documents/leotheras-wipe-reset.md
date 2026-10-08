# Leotheras: inert boss after a wipe

## Status — October 8, 2026

**Built and deployed with explicit authorization on October 8.** See the
[deployment report](raid-fixes-deployment-20261008.md) for binary and preservation checks.
Core fix: `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72` (published to our fork).
Only the worldserver was recreated. No encounter was force-reset or respawned,
and no gameplay SQL or configuration edits were made.
The user reports a wipe followed by a re-pull where Leotheras stood still for the
entire fight. The source reproduces the broken lifecycle; no live decision/event
trace of that specific failed encounter was captured.

## Cause

`boss_leotheras_the_blind::Reset()` called `BossAI::Reset()`, cancelling its task
scheduler, then set the boss passive. Berserk and the elf/demon ability schedule
were initialized only by `ACTION_CHECK_SPELLBINDERS` after the final spellbinder
died. There was no boss-specific `JustEngagedWith()` reinitialization.

Consequently, when the spellbinders were already dead, a later direct boss pull
could inherit a passive boss with no encounter tasks. Reset also did not explicitly
normalize equipment/metamorphosis/whirlwind or the final split's non-selectable flag.

The installed world data was checked read-only:

- Boss 21215 uses `boss_leotheras_the_blind`.
- Three Greyheart Spellbinders (21806) share the boss formation.
- Their SmartAI death event sends action 1 to the formation leader.
- Formation flags are 28 (evade together, respawn-on-evade and don't-respawn-leader).
  Current `CreatureGroup::MemberEvaded()` handles evade-together in the first arm
  and respawning in its `else` arm. Dead guards can therefore remain dead with this
  combined flag configuration. **No global formation semantics were changed.**

## Narrow repair

Only `boss_leotheras_the_blind.cpp` changes in the core repository.

- Reset clears the per-attempt start latch and restores the normal model/equipment,
  whirlwind state and selectability. It refuses to reset an engaged or dead boss,
  matching the base reset eligibility rather than partly resetting around a refusal.
- A released boss with no living formation followers returns to aggressive/standing
  readiness. **Reset itself neither enters zone combat nor schedules combat abilities.**
- Initial last-spellbinder release and subsequent `JustEngagedWith` share a guarded
  `StartEncounter()` initializer. It runs once per attempt, including when zone combat
  synchronously invokes the engagement callback.
- Living spellbinders still gate activation. A genuine release is remembered across
  same-AI wipes even after dead followers unload. Fresh AI does not mistake an empty
  or partially loaded formation for a defeated gate: without the release memory,
  re-engagement requires three loaded, dead spellbinders. Missing formations fail closed.
- The existing death-notification path remains authoritative for the initial release;
  it does not require old guard corpses to remain loaded.
- A null victim in the demon movement helper returns safely instead of being passed
  into its distance/chase calls.

No minions are respawned or removed by this change. Boss health/damage, 15% split,
10-minute Berserk, whirlwind/phase timings, loot, instance-reset policy, playerbot
strategies, threat policy and other encounters are untouched. This does not implement
the separate request to prevent playerbot strategies from initiating pulls.

## Validation and limitations

`python3 -m unittest scripts.tests.test_leotheras_reset -v`: **six tests passed**.
The C++ fixture compiles the complete production boss class against offline doubles:

- fresh, empty and partially loaded formations; living/partly defeated spellbinders;
- initial release and synchronous/repeated release/engagement notifications;
- whirlwind execution, demon-form reset, final-split kneel/selectability reset;
- retained final-split summon/stand callbacks;
- **20 repeated wipes/re-pulls**, one schedule per attempt, no combat/timers at home;
- dead/engaged reset refusal, missing formation, evade-time notification, null victim;
- recreated AI with three loaded dead binders;
- ASan/UBSan run with leak checking;
- the pre-fix production class at core `8b8b7bcf8615c62b4aedaa00c33713a0c1f6df0e`
  fails the regression as expected;
- mutants removing re-engagement startup or retaining the stale start latch also fail.

The scheduler/formation/creature doubles validate local lifecycle behavior, not real
map/grid timing, actual spell effects or persistence. The actual production translation
unit also passed a syntax-only check against native headers. The native C++ style
checker passed on the changed source and fixture.

Existing respawn-scaling and expansion-reset suites: **15 additional tests passed**.
Those retain their intentional old-code/mutant rejection checks; expected assertion
failures in those negative-control subprocesses are not ignored test failures.

**Live encounter acceptance remains pending:** following deployment, observe an
ordinary wipe/re-pull with guards already dead. The boss should resume attacks and
normal abilities, with one Berserk/phase schedule and no premature start at home.
Do not force a wipe/reset or alter the current completed encounter to test this.

## Related investigations

- [Kaaren's glove roll](kaarens-glove-roll-investigation-20261006.md): scored upgrade;
  transient death-state rejection found, historical cause still unconfirmed.
- [SSC/TK authority audit](ssc-tk-tank-authority-audit-20260925.md): separate native
  playerbot assignments and pull-control concerns.
- [Compatibility respawn scaling](raid-respawn-scaling.md): preserved unchanged.
