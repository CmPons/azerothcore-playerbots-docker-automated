# Twin Emperors: two encounter groups and a repeatable tank handoff

## Status

**Prepared September 11, 2026; not built or deployed.** No service interruption, live commands, SQL,
gear changes, raid-role/subgroup edits, or configuration changes were made while preparing this.
The running September 11 AQ40 image still contains the previous Twins AI.

The goal is coordination, not reducing encounter difficulty. Repeated Heal Brother explains the
lack of health-bar progress; do not tune DPS/HP around a broken separation strategy.

## The plan for this roster

These are logical encounter groups, not changes to saved raid subgroups:

| Responsibility | Current roster |
| --- | --- |
| Original Vek'nilash-side physical station | Arinerica |
| Opposite physical station | Redshift |
| Caster tank, follows Vek'lor | Beliona |
| Caster DPS, follows Vek'lor | Raney |
| Physical DPS, follows Vek'nilash | Pilbok, Kaaren, Feelesia/pet |
| Assigned to the current melee tank | Meliah |
| Assigned to Beliona | Ailina |
| Floating / temporary-tank / missing-healer coverage | Keilmere |

Selection is role/class/GUID based, **not hardcoded character names**. Bots take the first physical
station; another tank takes the other. A second tank bot can occupy that station if present;
with this roster, it is the human. Warlock class priority precedes GUID, so lower-GUID Raney does
not take Beliona's primary caster role. Mage/priest are caster fallbacks if the warlock is unavailable.
Only living, in-world, same-map-instance/phase group members qualify as tanks.

### Teleport: the human's predictable off-tank job

1. Start with Ari on the original melee side and Redshift on the opposite side. Stay out of
   Vek'lor's melee/Arcane Burst area while he occupies your side.
2. When Vek'nilash arrives on **your** side, catch and hold him there. On Ari's side, she does that.
3. When Vek'lor arrives, the local physical tank steps out of melee and temporarily holds the
   incoming Shadow Bolts while Beliona's caster group moves over and rebuilds threat.
4. **Do not chase the melee emperor to the opposite side afterward.** The waiting physical tank
   stays at its station, ready to catch the next incoming melee emperor. Physical DPS swap bosses;
   the two physical tanks do not race across the room and leave both stations empty.

This corrects the initial proposed “Ari always follows Vek'nilash” plan. With only one bot physical
tank, a cross-room tank chase plus two threat handoffs can consume most of the 30–40-second cycle.
The human is an active tank on alternating sides of the cycle, not an idle backup. This does not
promise nine bots can perform both physical-tanking jobs while the human does nothing.

Physical station ownership flips on an **observed simultaneous exchange of boss positions**.
It does not repeatedly change as bosses cross a geometric midpoint during separation recovery.
A one-time home-position comparison initializes the snapshot; normal out-of-combat housekeeping
clears it between attempts. Snapshot access is mutex-protected and returns values, not references
that another map update could invalidate.

If a physical tank is unavailable, the surviving one is a fallback; that degraded state is not a
promise of effortless recovery. If Beliona accidentally catches melee, she holds/repositions away
from its brother rather than deliberately dragging it across; the physical tank must rescue her.

## Behavior changes

### Tanks, damage and threat

- Exactly one current owner per emperor. The other physical tank waits at the caster-side station,
  helps with local mutated bugs, and does not deliberately attack either emperor while waiting.
- Primary boss tanks **do not switch to mutated bugs**. Supporting DPS prioritize mutated bugs
  within 35 yards of their assigned emperor; they do not chase unrelated bugs into the other camp.
- Beliona gets an explicit normal **Searing Pain** tanking action. She establishes aggro first,
  then uses Shadow Ward when available. Learned ranks, mana, cast times, cooldowns, immunity and
  normal spell threat remain native. No injected threat or free spells.
- Threat checks compare DPS against **that emperor's assigned tank**, not always Redshift.
  The caster tank can open and continue attacking while it has aggro. Spare physical tanks do not
  bypass another owner's ceiling merely because their class strategy says “tank.”
- The generic 80%/AoE limiter cannot undo the encounter-specific result. Threat values used by
  panic/deaggro consumers also recognize the caster tank. Outside Twins, existing policies remain.
- **Healing spells report AoE threat too.** The encounter explicitly preserves heals, friendly
  dispels and defensive buffs instead of treating them as damage to suppress during an opening
  or threat handoff. This is important even when a healer has a boss selected.
- Pet orders respect damage holds instead of immediately re-arming a pet the limiter just stopped.
  Physical pets help their owner's selected local target; the prior caster-pet parking policy remains.

### Movement and healing

- Ordinary automatic follow, raid-wide formation, panic/flee-to-leader and incompatible contact
  movement are suppressed throughout the encounter, **including after bots reach a good spot**.
  Explicit chat shortcuts remain usable, notably the retreat command that enables passive mode.
- Tanks and healers own their encounter movement. DPS retain native reach/rear positioning toward
  their assigned enemy; attacks and healing are not blanket-suppressed as “movement.”
- Only the actual melee emperor victim leads a separation kite. A tank must acquire threat first,
  not leave melee before it has aggro. An owning caster can also relocate Vek'lor, accounting for
  his 45-yard chase range rather than treating him like a melee enemy.
- A new Arcane Burst clearance action moves bots sideways from the incoming caster. It avoids
  known Blizzard/bomb destinations instead of repeatedly running out and back through the boss.
- Changed encounter destinations can replace an old movement lease. A settled tank/healer stops
  old movement rather than continuing toward an obsolete follow point. CC/controlled movement
  remains respected, and lower-priority settling must not cancel separation/hazard moves.
- Healers may keep **any safe, in-range, line-of-sight position** near their assigned tank. They are
  not forced to a boss-centered ring or to the human master.
- A 34-yard-or-smaller movement buffer leaves room inside actual healing range. Useful casts may
  finish while still in real range; hazards can interrupt them when movement is necessary.
- The assigned tank receives a healing preference below 90%, not exclusive healing. Nearby
  critically injured players still receive priority; a nearby human off-tank remains eligible.
  Remote players cannot lure the healer out of coverage through normal heal-target selection.
- Floating healers include the **actual boss victims**, so the physical tank temporarily taking
  Shadow Bolts is not forgotten. A healer death leaves the other dedicated healer assigned to
  the same tank; a floating healer fills the vacancy where available.

## Diagnostics after deployment

Whisper any participating bot:

```text
/w Arinerica do aq40 twins status
/w Beliona do aq40 twins status
/w Ailina do aq40 twins status
```

Reports current melee/caster owners, the other physical station, the bot's role, passive state,
its healing anchor/range/LOS, boss separation and actual victims. It is an on-demand report, not
continuous raid-chat spam. Healing anchors are meaningful during the encounter.

If Ari was left passive while experimenting with commands, restore her before pulling:

```text
/w Arinerica co -passive
/w Arinerica nc -passive
```

No saved strategies are automatically rewritten by this change. No automatic pre-pull attack,
teleport, roster synchronization, gear regeneration or subgroup rearrangement was added.

## Source, patches and verification

Production files are under `azerothcore-wotlk/modules/mod-playerbots/src/`:

- `Ai/Raid/Aq40/Aq40Coordination.cpp` and `Aq40Actions_Coordination.cpp`: assignments, coverage,
  caster tanking, movement handling, clearance and status.
- Existing AQ40 helpers/actions/multipliers/strategy/context: integrate those policies.
- `Ai/Base/Util/RaidThreatUtils.cpp`, `Strategy/ThreatStrategy.cpp`, `Value/ThreatValues.cpp` and
  `Value/PartyMemberToHeal.cpp`: narrowly scoped integration with ordinary threat/healing.

Canonical patches:

- `0025-playerbot-raid-threat-foundation.patch` preserves the **already deployed** `acb4b750` feature
  in the patch series. It was previously only in local module git history. This adds no new
  runtime behavior beyond what was already present before this task.
- `0026-playerbot-twins-coordination.patch` contains the new 14-file change against the verified
  pre-task source. AQ40's preexisting untracked source is already represented by `0016` and remains
  patch-managed; unrelated module work is not staged or discarded. The four tracked integration
  files are committed locally as playerbots `131ec19521f8d4a77d5d2c37288b2e2635b040c8`; the root
  fork's canonical patch publishes the complete change, including the AQ40 companion sources.

Validation completed: **63 tests passed, one optional connection-local MySQL test skipped**;
all 14 changed/new production source files passed the scoped core C++ style checks and 120-column
limit. A source manifest comparison verified exactly 12 changed files and two new files, with no
removals or changes to unrelated module source or the core working-tree diff.

Tests: `scripts/tests/test_twins_coordination.py`, `cpp/TwinsCoordinationTest.cpp`, and
`fixtures/twins/Framework.h`. The runner compiles real production helpers, actions, multipliers
and threat utilities against API doubles, plus the real heal/threat-value methods. It also compiles
legacy helpers/limiter recovered by reversing `0026`, reproducing the wrong mage assignment,
blocked caster tank, missing caster movement and unnecessary healer-ring movement.

Coverage includes both teleports of a cycle, stable stations despite normal kiting, off-tank
fallbacks, map/phase/instance isolation, healer deaths, floating handoff coverage, threat ceilings,
beneficial casts during threat holds, local adds, pet re-arming, melee range boundaries, movement
settling/preemption, Arcane/Blizzard destination conflict, ordinary casting delegation and DONE
cleanup. Patch reversal/reapplication and action registration are checked as well.

**These are not live combat proof or a full worldserver build.** Native spell damage, actual
healing throughput, full engine scheduling, geometry/pathfinding and raid survival need an
observed attempt after an explicitly authorized build/deployment. Boss damage, HP, healing,
immunities, timers and general raid scaling are unchanged.

### Existing regeneration caveat

A read-only full replay from the pinned module reaches an older, unrelated failure in
`0021-playerbot-hunter-pet-taunt-policy.patch`'s `Playerbots.cpp` registration hunk (it expects
other local history). That failure occurs **before** these new patches. No repair of unrelated
history or broad setup/update run was attempted.

With that unrelated patch excluded, the relevant pinned-module patch chain reproduces all 14
Twins source files byte-for-byte. Do not describe this as a successful replay of the entire server.
A future authorized deployment must use the backed-up **full working source tree**, as previous
successful deployments did; do not regenerate it from pins or run setup on the live installation.

Preparation backup: `backups/twins-coordination-preparation-20260911-211515/` (pointer
`/tmp/twins-coordination-backup`). It contains the full pre-edit playerbot source, hashes, git
status/diffs and repository heads. No live DB restore or boss reset is part of this source-only change.
