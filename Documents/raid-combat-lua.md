# Raid combat Lua API2 (deployed September 14, 2026)

API2 runs in the existing instance-owned VM/mailbox: one active plus one candidate,
serialized map updates, last-good retention, active-fault quarantine. The manifest
transport remains `1 1 <publication> <sha256>`; the Lua payload deliberately declares
`api=2`. API1 policy files and their positioning protocol remain supported.

## Edit → check → publish → next pull

The reviewed API2 image and checked initial C'Thun default are installed. See
[deployment and preservation evidence](raid-combat-lua-deployment-20260914.md).
From the project root, use the existing directories (legacy config names retained):

```sh
POLICY_DIRECTORY=runtime/playerbot-policies
STATUS_DIRECTORY=runtime/playerbot-policy-status
```

No daemon, watcher, per-instance publication command, logout or travel is needed.

1. Edit `raid-policies/aq40/combat.lua`. Its small `release`, `goal`, `aura`, and
   `distance` helpers are self-contained; no imports or generated bundle are required.
2. Check with the installed offline runner (built from `scripts/tests/lua-runtime`):

   ```sh
   python3 scripts/playerbot_policy.py check raid-policies/aq40/combat.lua \
     --checker runtime/playerbot-policy-checker/raid-combat-check
   ```

3. Publish the checked bytes once when the policy change is authorized:

   ```sh
   python3 scripts/playerbot_policy.py publish-default raid-policies/aq40/combat.lua \
     --checker runtime/playerbot-policy-checker/raid-combat-check --directory "$POLICY_DIRECTORY"
   ```

   The initial default was published during the authorized deployment. Subsequent
   publications replace it at the next safe boundary. Checking/publication prints the source
   filename; Lua syntax/runtime errors include the retained `cthun-policy:<line>`
   chunk label. Native output-schema errors identify the invalid field.

4. Finish the pull. Each whole instance automatically adopts at an independently
   checked out-of-combat boundary, including players, controlled units and casts.
   Unknown/overflow population defers adoption. New instances inherit the default.
   Invalid candidates preserve the old VM. Active faults release policy movement
   and quarantine those bytes; publish distinct corrected bytes to resume.

Read a current status filename, then:

```sh
python3 scripts/playerbot_policy.py status --status-directory "$STATUS_DIRECTORY" \
  --scope "<map>-<instance>-<epoch>"
```

`active`, `queued`, `desired`, `safe_boundary`, `fault_error`, `api`, `adopted`,
`observation_gaps` and `receipts` explain adoption/fallback. Status is sampled, not
an effect-success acknowledgement. A stale/destroyed status is not a live instance.

## Copied snapshot and output

`plan(s)` returns one data-only intent per `s.members` entry. Caps: **40 members,
96 observed entities, eight auras per unit**; source **32KiB**, VM **2MiB**, evaluation
**300000 instructions**. Collection occurs every 250ms; plans expire after 1000ms.
No native pointer, script callback, arbitrary command or Lua I/O is exposed.

Snapshot: `map`, `instance`, `sampled_at`, `sequence` (decimal string), `combat`,
`gaps`, `members`, `entities`. Members add `human`, `eligible`, `healer`, `tank`,
`melee`, `combat`, `movement_receipt`, `action_receipt`. Units contain decimal-string
`guid`, `victim`, `cast_token`; `entry`, `x/y/z`, `facing`, health percentage,
`alive`, `casting`, `cast_spell`, `auras={spell,dispel,harmful}`, `aura_gap`.

Observed entities additionally contain `attackable`, `engaged`, `visible_to`,
`los_known`, `los_to`. Visibility is **not** attackability: unattackable hazards are
copied. LOS masks refer to member indices (bit `1 << (i-1)`); at most four observed
creatures receive current per-member LOS queries per collection. Unknown LOS is
not cover. These current-position rays plus checked-ground rejection feedback are
sufficient to tune Lua cover waypoints; no new navigation/physics engine is added.

`gaps` bits: observers=1, roster=2, required rows=4, partial spatial horizon=8,
unloaded grid=16, population/compact cap=32, exhausted identity=64. Missing rows or
auras are not evidence of absence. Collection is bounded and never loads grids.

```lua
local intent = {movement=0, target=0, operation=0, spell=0, aura=0, action_target=0}
-- movement: 0 release, 1 hold, 2 checked ground goal (also provide x, y, z)
-- target: one-based observed-entity priority; 0 retains normal target arbitration
-- operation: 0 none, 1 interrupt, 2 dispel
-- spell: a known active native spell ID/rank, NOT the enemy's cast ID
-- action_target: observed-entity index; dispel also names the observed aura spell
```

Unknown keys, nonfinite coordinates, invalid indices, noninteger fields and claims
for ineligible members reject the entire plan. Native code re-resolves visibility,
control, current cast/aura, binding and TTL **after** callback-bearing validation.
Harmful requests require existing engagement with this raid, including the final
collected harmful spell targets. Ordinary native costs/range/LOS/GCD/cooldowns and
immunities still decide casts. Busy actors defer/reject; no cast cancellation,
triggered casting, free resources or new pulls are provided. A request is attempted
at most once per actor/frame; admission is not proof of effect success. The one-second
deadline applies to new request admission, immediately before native spell preparation.
An admitted cast continues under native rules; a later plan or elapsed deadline does not cancel it.

## Movement and native coexistence

Lua supplies a room goal (within 240 yards); native code checks short ground steps,
not an author-managed spline TTL. A maximum of one bounded path attempt per actor/
frame prevents retry spinning. Proposed geometry is checked **before** handoff of
positively scheduled, unleased follow/chase, then authority/origin are checked again
following native finalization. Holds suppress scheduled follow/chase/reach, including
combat without a target; they never report a successful action tick. Native rotations,
healing and emergency actions continue. Utility requests defer to low-health healer
triage. Native/manual casts are never stopped to start a ground step.

Manual/unknown movement, stay/passive, CC and every nonpolicy lease take priority.
Cleanup stops only the exact still-live owned spline and removes only its owned
active generator, including beneath a controller; replacement/controlled motion and
CC state are not blanket-cleared. Control/fault/expiry revocation is handled at the
next serialized maintenance opportunity, not claimed to occur during a stalled server.
The old candidate's “wait for spline duration” is not used as cleanup.

Movement receipts: 0 none, 1 release/ineligible, 2 authority/busy, 3 hold, 4 checked
route rejected, 5 launched. Action receipts: 0 none, 10 rejected, 11 healer triage,
12 native submission. Ground/support/body-ray checks are bounded conservative samples,
not continuous collision proofs. Vehicles, transports, flight, swimming and jumps
fall back to native behavior.

## Initial content and evidence limits

`aq40/combat.lua` selects the observed Eye of C'Thun entry 15589, uses the existing
west-entry landmarks, spreads ground positions and reacts to observed red-facing
hazards. Entry now holds bots on the known route while a living human approaches,
then advances them with a 17-yard proximity margin and one-yard approach goals when
within 21 yards of the preceding member. Route progress orders the queue, with roster
order breaking ties. Entrants also wait for nearby members just inside the room to clear.
Green-phase positioning now requires room-wide stations instead of accepting any locally
spaced position near the entrance. Humans reserve inner sectors; alternating healers/melee
use a 22-yard ring and ranged DPS a 40-yard ring. The anchor freezes during combat and
dead/CC/ineligible members retain their places. For the current ten-member composition,
ideal station spacing is at least 18.54 yards; every station is within 34.40 yards of another
healer station. This is a geometry calculation, not guaranteed live coverage.
Travel uses outer arc waypoints before peeling inward. Each proposed step is capped at
2.5 yards and scored against actual living-player spacing (17-yard margin, including nearby
stair occupants) and distance to a living healer (36-yard margin). Out-of-coverage bots can
move toward a healer. Local detours/escape take priority over reaching a station; arrival
alone is not accepted while crowded. Hypothetical healing LOS is not exposed by this API.
Observed red-facing avoidance takes precedence: facing
changes determine sweep direction, with no guessed boss timer. Bots ahead of the sweep
start escaping within1.3 radians and keep moving until1.8 radians clear; the unknown-direction
opening and trailing edge use separate conservative margins. Short0.3-radian arc waypoints
replace large chords, with a retained20–40-yard orbit radius and escape away from the current
beam if it overtakes a delayed runner. These margins are tuning values, not immunity;
existing casts are still not forcibly cancelled.
Keep moving clear of the doorway yourself: Lua cannot reposition you. Casts, simultaneous
movement and path rejection mean this is not guaranteed separation or zero beam damage.
It requires an observed Eye and known route position; casts/manual/CC/nonpolicy movement
and incomplete observations still limit enforcement. Surface-room non-healer DPS now prefer
nearby small/giant eye tentacles (15726/15334) over the central Eye: up to28 yards for
ranged and4.5 yards for melee, without changing entry/spacing movement. Only visible,
attackable, raid-engaged tentacles qualify; known blocked LOS is excluded, unknown LOS
still requires native cast validation. This priority also works without the central Eye
observed during body phase, but does not handle stomach tactics. Explicit/manual target
priorities still win. In particular, `AttackMyTargetAction` writes the selected GUID into
`prioritized targets`, which blocks `CurrentTargetValue` from using Lua's preferred target.
The user confirmed using `attack` to start C'Thun pulls. Between pulls, `follow` clears that
list (and re-enables noncombat following); pull personally without another `attack` order
when testing autonomous tentacle selection. This identifies an override, not live proof
that tentacle killing now works. It is initial tunable content, not a zero-damage guarantee or
exhaustive phase-two strategy. The first Viscidus live-tuning policy now asks eligible non-healer
melee bots (including bot tanks) to approach on their current side and hold within
4.5 yards of his center, reapproaching toward a 3.5-yard goal if needed. It prioritizes
the already-engaged boss but does not change rotations or force caster melee attacks.
Per the user's simplified request, this version does **not** dodge poison or continually
reposition behind him. Human/manual/emergency control remains authoritative. No new unit
tests were requested for this iteration; required checker validation and live feedback
are the tuning loop. Ouro hazards and Chromaggus cover remain next authoring targets.
All encounter IDs/landmarks/decisions remain Lua data.

Run `test_raid_combat.py` for real-VM budgets, exact native adapters/collector/ground
lifecycle, generic dispatch/reload and temporary checked publication. API doubles
cover external world services; this is not a full worldserver/navmesh/live-pull test.
The native deployment is complete. API2 default adoption has now been observed for nine
bots in AQ40; encounter behavior remains live tuning, not established by offline tests. Further server
restarts require fresh authorization. Lua-only publication does not restart the server.
