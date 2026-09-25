# Explicit cooperative MT/OT modes

## Status

Deployed with user authorization on **September24, 2026**. See
[tank-modes-deployment-20260924.md](tank-modes-deployment-20260924.md) for build/runtime
verification and the observed group/profile startup exceptions. Exact published
native source revisions are in `repo-pins.txt`; they may be ahead of the running
image. The September 25 damage-assistance correction below is **not deployed**.

This replaces the old split MT/off-tank heuristics with two coherent modes. It is
**not** the blanket spell-hook taunt restriction that was previously reverted.
Maulgar's separately requested strategy removal is deployed in the same image;
Gruul's own tactics remain enabled.

## September 25: covered-target damage assistance (not deployed)

The user reported Ari standing idle when the sole enemy was attacking Redshift.
The source had conflated ordinary attack permission with permission to acquire
tank ownership: the selector rejected every co-tank-held target, and the scheduled
attack gate independently rejected starting an attack on it.

The correction separates `CanAttack` from `CanAcquire`:

- MT and OT can select a co-tank-held enemy as a damage-assist fallback when no
  eligible tank work remains. This includes the single-enemy case, without a mark.
- Loose pickups and owned targets retain priority over that fallback, including
  when a covered boss is marked. Existing MT available-boss priorities remain.
- Ordinary attack and RTI execution admit damage assistance; taunts still use the
  stricter ownership gate. A newly arrived loose add can replace the assist target.
- The low-health MT pause still prevents starting this fallback; it does not gain
  a new exemption through the attack gate. Explicit orders remain overrides.
- No stance, Righteous Fury, threat generation, gear, role, health thresholds or
  encounter strategy changes. Damage can still overtake another tank's threat;
  this is not a promise of zero aggro stealing or a controlled tank swap.

`CanAcquire`, `SuppressAutomaticSpell`, ownership and pause functions were verified
unchanged against the prior source. Production-body fixtures reproduce the old
selector failure and old scheduled-attack rejection independently, then pass with
the correction. Coverage includes MT/OT, boss/trash, single/all-covered targets,
new adds, owned targets, RTIs, CC, ownership changes, health hysteresis, taunts,
manual attacks and inactive modes. The focused suite passed **33 tests**, and six
production-header translation units passed syntax checks; official C++ codestyle
has no new findings. These are offline checks, not live acceptance or a server build.

No server build, restart, configuration, Lua publication or database mutation was
performed. A separately authorized build/deployment is required. Existing native
encounter multipliers may still suppress actions if their strategies are enabled.

## Commands

Whisper without quotation marks:

```text
/w Arinerica tank strategy offtank
/w Arinerica tank strategy MT
/w Arinerica tank strategy status
```

`ot` / `maintank` are aliases; arguments are case-insensitive. Bare `tank strategy`
and `tank strategy ?` also report status.

- **Offtank:** assign another tank as group MT; Ari collects eligible loose enemies
  rather than competing for enemies held by that tank or another living co-tank.
- **MT:** assign Ari as group MT; prefer an available boss, otherwise collect loose
  enemies and maintain her own targets. A boss held by another tank is not treated
  as unclaimed simply because Ari has the MT flag.
- **Status:** report the effective mode, current MT when online, whether an explicit
  assignment or fallback tank order selected it, and the MT health-pause state.

Changing the assignment is **not an immediate tank swap**: it does not forcibly
taunt a boss the other tank already holds, or erase Ari's existing threat.

Assignments require the requester and bot in the same group, and the requester to
be group leader or raid assistant. The bot must already have an active tank combat
strategy. These commands do not change spec, talents, equipment, class strategies,
profiles, group leadership or composition. They refuse BG/arena role changes.

For OT, prefer another current MT, then the requesting tank, then another group
tank. If no co-tank exists, refuse instead of silently appointing a healer/DPS.
With the usual Redshift/Ari pair, `offtank` selects Redshift as MT, while `MT` selects
Ari. Other co-tanks are recognized through tank spec/active bot tank role; an
explicitly assigned MT is also respected in the ownership checks.

## One source of truth

The native **group Main Tank assignment** is authoritative. The command and the
raid-panel MT control update the same assignment; there is no competing saved bot
"mode" setting that overrides the UI later. Every other group tank is OT.
Unassigned groups use the existing `GetMainTankGuid` fallback order, reported as
such by status. Use the command to remove that ambiguity.

Modes are **group-scoped**, not a permanent character preference across unrelated
parties. Native group membership/role persistence handles relogging while the group
survives. No playerbot profile reset/save is used by these commands.

The core now permits MT assignment in normal parties as well as raids. Assistant
permissions and main-assist assignments remain raid-only. The old subgroup-1
heuristic for `IsOffTank` is removed; it now uses the same MT resolver as encounter
assignments. The legacy `offtank` strategy name remains an action source, but no
longer independently changes the role: use the new command or group MT marker.

### Saved flag repair

Core `Group::SetGroupMemberFlag` now saves former unique-role owners and the new
owner in one prepared-statement transaction. Removing a flag from one member does
not erase someone else's assignment. The client handler no longer clears old flags
in memory before the persistence function can see them.

The previous code was reproduced in the offline fixture: it cleared the former MT
in memory but left that character's saved MT flag set. The new code persists both
sides and preserves unrelated assistant bits. This also fixes the same persistence
issue for the existing unique main-assist flag.

No historical groups are bulk-rewritten. A normal assignment updates the affected
group. Earlier saved duplicate flags were not proof of duplicate *live* MT roles.

## Cooperation and low-health MT safety

- Ownership means the creature's current victim (with threat-manager fallback),
  not a temporary taunt aura, highest historical threat or selected target.
- A living, in-world tank in the same group/map protects its currently held PvE
  mobs from routine tank acquisition by the other bot. Dead owners allow rescue.
- Both MT and OT honor ownership protection. With the pending September 25
  correction, damage assistance is allowed but does not grant taunt permission;
  the selector and RTI path keep actual tank work ahead of that fallback.
- MT prefers an available boss, with the flagged dungeon/encounter boss ahead of
  merely boss-ranked council members. Without one, healthy MT and OT prioritize
  loose enemies, then maintain their own current target. This uses template flags,
  not hard-coded council assignments or an inferred tank-swap order.
- MT pauses **new pickups below 40% health** and resumes at **65%**. These are
  initial code constants in `TankModes.h`, not silently applied live config values.
- During that pause she retains a GUID-only set of mobs she was holding, so a mob
  peeling onto a healer can still be recovered. If the human OT takes one, its new
  tank ownership wins and Ari will not routinely reclaim it.
- Pause does not switch her passive, stop healing/defensive buffs, drop threat or
  abandon her own enemies. New routine offensive area/chain casts are withheld
  while paused, as are routine offensive casts at disallowed new targets. This
  deliberately reduces fresh cleave/AoE while overloaded; existing ground effects,
  DoTs, autoattacks already running and incidental healing/reflect threat are not
  erased. These modes are not a hard cap on how many enemies can reach her.
- Health and spell admission are inactive for non-bot sessions, non-tank roles,
  solo bots, BGs/arenas and PvP combat. Role/group/death changes reset the health
  latch; leaving combat clears remembered ownership. An injured MT also avoids
  autonomous prepulls, but does not spam combat-help messages while resting.

### Taunts and deliberate overrides

Routine `CastSpellAction` checks the mode during usefulness and again at execution,
only for **scheduled automatic actions**. It covers direct taunts/forced attacks,
Righteous Defense's friendly-target/multi-attacker behavior, Death Grip's parent,
and caster-centered area taunts such as Challenging Shout/Roar.

There is **no `AllSpellScript` blanket ban** and no change to human taunts.
Explicit commands run unscheduled; dedicated encounter code calling `CastSpell`
or `Attack` directly is not globally vetoed. Ordinary automatic taunt-and-retarget competition
is suppressed, but deliberate orders and encounter swap logic remain distinct.
An explicit override or dedicated script may therefore acquire a target despite
normal mode preferences; modes are not a global rewrite of every raid mechanic.

Healthy ordinary damage and threat are not globally suppressed. A bot attacking
a target can still overtake threat through damage. With the pending September 25
correction, an already-covered boss can be selected for fallback damage assistance,
not as authorization to taunt it. This does not change the bot into a DPS spec or
add a `tank swap` or `tank drop` command.

## Help call

When an in-combat MT is in the low-health pause, she whispers her real-player master:

> MT: I'm low on health! Please take some enemies off me. I'm holding my current
> mobs but pausing new pickups until 65% health.

At most once every **30 seconds per bot**, subject to normal bot chat delivery.
Rapid role changes do not reset the cooldown. No human master/group, no call.
This is deterministic native bot messaging, not a Pi/LLM request.

## Visual role markers

- **MT: blue square** (raid icon slot 5).
- **OT: purple diamond** (slot 2).

Issuing a mode command opts that bot/group into markers for the selected pair.
Deliberate UI MT changes update them once while the commanding leader/assistant
remains available. Merely asking for status does not set markers. Group changes
clear the tracking; nothing is permanently assigned across new groups.

All eight WoW raid icons are shared. The implementation:

- does not evict enemy or third-player marks occupying these slots;
- preserves a tank's existing marker in another icon slot;
- may move the chosen role pair between the two tanks when changing modes;
- does not repeatedly reclaim a marker an encounter subsequently reuses.

If an icon is busy the command reports the conflict; modes still work. After a
manual or encounter marker override, text status—not the missing/stale icon—is
authoritative. Generic encounter marking remains enabled; there is no universally
unused icon pair. Skull/star DPS focus and moon CC are not appropriated here.

## Source and tests

Native playerbots:

- `Ai/Base/Util/TankModes.{h,cpp}` — role/ownership, health latch, bounded help timer,
  scheduled spell admission and best-effort markers.
- `Ai/Base/Actions/TankModeAction.{h,cpp}` and chat registries — authorized commands.
- `TankTargetValue.cpp`, `AttackerCountValues.cpp`, `PlayerbotAI.cpp` — unified role
  and routine targeting/aggro semantics.
- `AttackAction.cpp`, `ChooseTargetActions.cpp`, `GenericSpellActions.cpp`,
  `Script/Playerbots.cpp` — routine-action admission and update wiring; no global
  spell/attack hook.

Core: `Groups/Group.cpp`, `Handlers/GroupHandler.cpp` — assignment persistence.
Root-owned chatter classifier: built-in `tank strategy` command filtering works
with older private keyword lists too; root/build-tree sources are synchronized.
The deployed classifier works without changing the existing runtime keyword list.

```sh
PYTHONPATH=scripts/tests python -m unittest \
  scripts.tests.test_tank_modes scripts.tests.test_chatter_commands \
  scripts.tests.test_maulgar_manual_control scripts.tests.test_skull_combat_only \
  scripts.tests.test_raid_combat scripts.tests.test_source_repos -v
```

The September 24 validation passed 27 tests, including production-body C++ fixtures for commands, permissions,
roles, old saved-flag failure, target selection, health hysteresis/help cooldown,
manual-vs-scheduled spell policy, party persistence, PvP/human exclusions and marker
conflicts. The historical flag test intentionally expects the old implementation's
assertion failure. Sixteen changed/integration translation units pass native-header
syntax checks; official native C++ codestyle has no new findings versus baseline.
These checks do not replace a full build or live combat acceptance. The previously
known hunter-pet and Twins historical patch-replay failures were not repaired or
counted as passes; this is a focused suite, not a claim that every repository test
passes.

## In-game acceptance still to verify

1. Select Ari OT; confirm Redshift MT, square on Redshift/diamond on Ari, and loose
   add pickup without routine taunting of Redshift's target.
2. Select Ari MT; confirm the reverse markers and that she leaves Redshift-held
   adds alone while focusing an available boss.
3. Below 40%, verify a help whisper, no new routine pickups/fresh offensive AoE,
   existing-target fighting and defensive buffs. At 50% remain paused; at 65% resume.
4. Have the human take one of her mobs and verify she does not immediately taunt it
   back through routine class behavior. Separately verify explicit cast overrides.
5. Change MT through the raid panel, inspect status/markers, then verify group roles
   survive an ordinary save/relog without duplicate saved flags.
6. Occupy a role icon with an enemy: confirm no forced clearing/re-marking loop.
7. After deploying the September 25 correction, have Redshift hold the sole enemy:
   Ari should attack without automatic taunts. Add a loose enemy on a healer and
   verify she switches to pickup; keep an owned add and verify it outranks assisting
   on Redshift's boss. Repeat with and without a boss focus icon, in MT and OT modes.
8. Check normal party use, no-target/all-targets-covered behavior, death/rescue and
   dedicated raid swap logic. Do not interpret offline assertions as a live pass.
