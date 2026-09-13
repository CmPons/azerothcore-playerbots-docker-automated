# C'Thun bot spacing and Dark Glare avoidance

September 13, 2026. Authorized implementation and worldserver deployment; first iteration, not a
complete C'Thun strategy. Canonical incremental patch:
`patches/0031-playerbot-cthun-positioning.patch` (after 0030).

## Scope

Four playerbot production files in `src/Ai/Raid/Aq40/`: new `Aq40Cthun.{cpp,h}` and registration in
`Aq40ActionContext.h` / `Aq40Strategy.cpp`. No C'Thun boss script, spell damage, health, add count,
loot, gear, scaling setting or raid progression change. The pending Ouro recovery patch 0030 is
included in the combined image; its recovery logic does not reset completed encounters.

The `aq40` strategy is already installed in both combat and noncombat engines. The new emergency
position action and scoped movement multiplier therefore operate before aggro as well as during
combat. Actual native paths/walking/running only: no teleport, speed injection, forced path endpoint,
threat injection, learned-spell bypass or passive-state changes.

## Green-beam spacing

- Desired center-to-center distance: native 10-yard chain jump + both players' **current combat
  reach** + two yards. Normally **15 yards**, with a small settling tolerance. Both human and bot
  party members are obstacles; only bots are moved. Pets are not beam targets in this DBC.
- Act on the upper floor near C'Thun (within 240 yards), outside the stomach, with a living encounter
  creature and raid group. Finished encounters do not activate the policy.
- Before combat, require the living master on that floor within 180 yards and neither the bot nor
  master fighting unrelated trash. Spread on approach,
  without crossing the Eye's native `90 + Eye reach + player reach` aggro boundary: another
  **20 yards** is reserved to isolate the puller. With the observed Eye reach of 15 yards, bots
  aim to remain at least 126.5 yards from its center before aggro. This is not a command to pull.
- Minimize nearby clumping; also consider other bots' recent short movement destinations. A bot
  that is itself spaced yields farther when necessary to let an enclosed clump disperse.
- Use deterministic, role-based soft approach bearings, not hardcoded character names or rigid
  stations. Dead cohort members retain their slot while present. Once spaced and in usable range,
  hold rather than constantly rearranging the raid.
- Retain melee/spell reach, LOS and nearby healer coverage as positioning goals. An out-of-range
  injured party member supplies a healer approach goal if ordinary heal selection supplies none;
  spell selection and healing triage themselves are unchanged.

Generic follow/formation/flank/reach/flee and automatic movement spells such as Blink, Disengage
and Charge cannot immediately undo the checked positions while this policy is active. Ordinary
attacks, spells, healing, dispels, interrupts, target selection and explicit chat/custom orders remain
available. Real-player AI, passive bots, charmed/immobile bots and active higher-priority manual
movement leases are not overridden. Expired leases do not disable coordination indefinitely.

## Red sweep

Observe the Eye's actual red aura **22518** and facing, not private boss-AI direction or timers.
The warning region includes the native five-yard forward beam, reach/width margin, and approximately
four seconds of the native `pi / 35` radians-per-second rotation **in either direction**. This also
uses the native three-second red telegraph before the first lethal cast.

Seek short steps away from that warning. Paths cannot cross a beam/warning they started outside;
when already inside, clearance must improve monotonically along the path. Red-sweep escape takes
precedence over spacing if there is no route satisfying both (the main Eye does not cast green
beams during its red phase). Re-evaluate as facing and other players move. Return to ordinary
spacing when the red aura ends. The Eye's native zero-health **fake death** (which can leave
`IsAlive()` true) switches to the phase-two body and releases the old Eye's large collision envelope.

## Routing bounds and support

- At most eight native path attempts per plan, using exact `PATHFIND_NORMAL` only.
- Six-yard waypoints, at most twelve yards of routed travel; no incomplete/shortcut/no-navmesh fallback.
- Terrain-adjusted step bound, at most three yards of height change, LOS and endpoint verification.
- Sample **entire path segments** at intervals no greater than 0.75 yards. Check body/aggro envelope,
  glare, and new green-beam links throughout, not just at endpoints.
- Keep a still-safe owned waypoint rather than restarting it each tick. Replace unsafe automatic
  movement, or stop it when no safe replacement exists. Settled bots do not interrupt support casts.
- Movement uses normal run mode, exact checked waypoints and the existing combat-priority lease.

## Diagnostics

On demand, without changing state:

```text
/w Arinerica do aq40 cthun status
/w Meliah do aq40 cthun status
```

Reports active approach/combat status, movement owed, red warning, nearest party member and actual
versus desired center spacing. No diagnostic commands were sent to live bots for testing.

## Tests and limits

`python3 -m unittest scripts.tests.test_cthun_positioning -v` compiles the actual production planner,
action and multiplier against API doubles, with checked assertions and UBSan. Cases include:

- Packed ten-player approach and subsequent spaced advance, yielding to release enclosed clumps.
- Both sweep directions, several starting headings including angle wrap, and all nine bots;
  native five-yard lethal-line assertions run **before** the simulated bot response to each new heading.
- Passive/human/root/charm/death/phase/map/stomach/DONE/manual-lease guards, normal attacks/support
  surviving movement suppression, automatic movement spells, and diagnostic registration.
- Enlarged player reach, ignored dead/different-phase/stomach neighbors, short waypoint reservations,
  healer approach, continued-waypoint stability and human withdrawal from the pre-pull area.
- Rejected shortcut/no-path/height/LOS/detour routes, including safe endpoints with unsafe intervening
  green links or red-beam crossings; stopping obsolete movement if no safe route exists.
- Incremental patch roundtrip, focused pinned replay and retained Twins regressions. Full pinned
  setup replay still has the pre-existing unrelated 0021 dependency failure; setup was not run.

Doubles do **not** simulate real navmeshes, wall geometry, latency, casting cadence, HPS, collision
with moving players or tentacle slows/knockbacks. No offline result guarantees a clean live pull.
Give the bots time to separate before committing the entrance. Packed summons directly into an
active encounter or a human moving through them can still produce unavoidable short-notice links.

This is intentionally not new tentacle target assignment, interrupt coordination, stomach escape,
or a complete phase-two strategy. Surface spacing continues in phase two, but small targets may
not offer enough safe melee positions for everybody; safety can cost damage uptime. Those are
separate iteration points, not reasons to silently reduce C'Thun's mechanics.

Selected regression suite: **97 tests, 96 passed and one optional MySQL fixture skipped** (60.626s).
Scoped production C++ style and added-line checks passed.

Preparation backup: `backups/cthun-positioning-preparation-20260913-134114/`.
Deployment evidence will be recorded separately after verification.
