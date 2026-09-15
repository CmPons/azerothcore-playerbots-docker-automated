# Lua ground movement: point-capacity correction

## Status — September 15, 2026

**Fixed in source and validated offline; not built into a server image or deployed.**
Patch: `patches/0037-playerbot-raid-ground-path-capacity.patch`, incremental against
our actual layered source through0036. A worldserver image rebuild and authorized
restart/recreation are still required. The running server and the minimal Lua
proximity probe remain unchanged.

## Defect

`RaidCombatGround.cpp` called `PathGenerator::SetPathLengthLimit(6.0f)` intending to
limit a short movement route to six yards. The actual core setter divides the value
by `SMOOTH_PATH_STEP_SIZE` (4) and truncates it to an integer **point capacity**.
It therefore allocated one output point.

The real smoother writes the origin into that slot and cannot advance. The point
builder has a special case that appends the endpoint when the corridor uses one
polygon. Crossing a polygon boundary instead produces a failed-path classification.
This gives a concrete explanation for movement that sometimes works and sometimes
fails even over very short distances.

The Lua proximity diagnostic reached native movement handling, often reporting
receipt4. That receipt combines several rejection branches; this defect is proven,
but it is not evidence that every observed rejection had the same cause.

## Narrow correction

Request `(maxRoutePoints + 1) * SMOOTH_PATH_STEP_SIZE`: 33 storage slots for the
existing maximum of32 accepted route points. The extra slot is necessary because
the core classifies a saturated buffer as a short/incomplete path.

The adapter still rejects actual routes longer than six yards. Its short-step
limit, maximum32 accepted points, sample budget, endpoint tolerance, support/liquid/
body-ray checks, ground-only restrictions, generation/TTL binding, exact-owned
motion cleanup and manual/CC/cast precedence are unchanged. Core PathGenerator
behavior is not modified globally.

## Evidence

A focused regression extracts actual core declarations and point-building/smoothing
methods and compiles the vendored Detour implementation. A two-yard route is tested
on one polygon and across two adjacent synthetic ground polygons:

| Storage capacity | One polygon | Two polygons |
| --- | --- | --- |
| 1 — old setting | NORMAL (special case) | SHORTCUT + NOPATH |
| 2 | SHORTCUT + SHORT | SHORTCUT + SHORT |
| 33 — correction | NORMAL | NORMAL |

The test extracts the actual adapter setting, passes with the correction, and fails
when the original setting is replayed against the same production method bodies.
The prior lifecycle fixture ignored the setter; it now extracts the actual setter
and explicitly models its capacity classification. This exposed the original defect
rather than silently accepting it.

Parent independently reran:

- `PYTHONPATH=scripts/tests python3 -m unittest test_raid_combat.RaidCombatGroundPathTests -v`
- The existing exact-ground lifecycle/handoff test using its already-built cached
  archives, without CMake configuration. It covers native movement/spline ownership,
  casts, CC, replacement motions and TTL, including added rejection cases for an
  over-six-yard route, unsupported floor and water.

Production-header syntax and scoped C++ style checks passed. Independent fresh
review: **OK with notes**, no findings. The incremental patch also passes an exact
forward/reverse roundtrip against the captured layered baseline.

A read-only query of the real AQ navmesh corroborates relevance: a representative
2.8-yard step for Arinerica, away from Redshift's saved position, crosses three
polygons. Another bot's representative step stays on one polygon. These are saved
coordinates, not synchronized live telemetry, and are not a complete native movement
execution.

## Limits and next live check

The synthetic regression substitutes owner/Z normalization on an exactly flat
floor and unused slope/swimming hooks; it is not the full live Map/CalculatePath
stack. Its standalone UBSan run disables alignment checking because vendored Detour
packs links at four-byte boundaries with64-bit references; other UBSan checks remain.
The separate lifecycle test's sanitizer configuration is unchanged.

After an authorized deployment, keep the small proximity probe and verify visible
retreat plus native movement feedback before restoring encounter tactics. If failures
remain, distinguish individual native rejection stages; do not loosen geometry or
control guards based solely on receipt4. No claim of reliable live C'Thun spacing,
glare avoidance or tentacle handling is made by this correction.

Private baseline, logs, source hashes, saved-coordinate queries and server-identity
checks are retained under `backups/raid-ground-rejection-20260915-173124/`.
