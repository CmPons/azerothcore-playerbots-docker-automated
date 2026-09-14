# Instance-owned C'Thun Lua policy MVP

**Current deployment: API2 raid-combat plus automatic loading (0034/0035), September14.**
The checked initial C'Thun default is installed. Use the
[raid-combat API/operator workflow](raid-combat-lua.md) and
[current deployment evidence](raid-combat-lua-deployment-20260914.md).
The user narrowed first playable to raid combat; deferred mechanics and broad boss demonstrations
are not prerequisites. Live adoption/navigation/encounter behavior still needs observation.

The material below documents the retained API1 C'Thun implementation and its historical
[September13 deployment](lua-cthun-scaling-deployment-20260913.md), not the current loading workflow.
Incremental patch: `patches/0033-playerbot-cthun-lua-policy.patch`. Actual baseline root HEAD:
`b03ae99b0891289abd923bfc6d076561c152c9d3`. Do not run setup/reset-to-pins to install this delta.
The unrelated full pinned replay failure at0021 remains; focused Twins replay is deliberately
limited to Twins-owned paths rather than claiming full reproducibility.

## What changes

One **active** Lua5.4.8 state per relevant AQ instance, owned by `Map::CustomData` through
`CthunPolicy::Scope`. No global VM, AI mutexes, live-object callbacks or worker daemon.
`OnMapUpdate` creates the scope only for a human-led raid with bots near the verified approach.
A reload may temporarily own one candidate state in the same scope; failure destroys that candidate,
not the active state. Map `DataMap` destruction closes its own state once. MapInstanced chooses
unload instead of scheduling an update; MapMgr waits for scheduled updates before the next cycle.
There is no separate OnDestroyMap close or retained map/player/creature pointer.

**Timing:** the actual map hook runs AFTER player/object updates. It builds one value snapshot/plan
per250ms, consumed by subsequent bot updates. Session-only updates bypass planning; Lua intents
expire after1000ms. Final native guards recheck phase, roster GUID, movement ownership and geometry.
There is exactly one C'Thun action/multiplier backend: Lua when active, otherwise revised native
fallback. A bad active policy retires its revision once and selects native, never repeatedly retries
its bad code. Failed candidate reloads leave the old revision/token intact.

Native rotations, healing recipient/spell selection, threat and pet policies are unchanged. No
health, damage, loot, inventory, economy, binds, save state, encounter reset or boss source edits.
Ordinary spacing/entry does not interrupt casts. Native red-glare escape may interrupt a cast only
after a valid route is found. Holds return false, leaving successful action ticks to healing/DPS.
Existing native AI cast/delay gates still apply; these tests do not establish real reaction latency.
Lua's move intent itself makes the action useful; there is no hidden native `Needs` veto on a valid
Lua proposal. The shipped Lua explicitly holds when safe/in range, rather than marching to rigid stations.

## Actual room entry, not exterior spread

The approach is WEST/SOUTHWEST, not the superficially nearby NE trash on another stacked floor.
Parent read-only DB evidence puts Eye15589 at(-8578.79,1986.18,100.304), body15727 at
(-8578.65,1985.85,100.304). An offline probe compiled against actual core Detour sources found
this final connected approach (XYZ):

```
-8634.93 1913.87 108.979
-8641.33 1914.40 108.979
-8652.53 1921.60 108.979
-8661.33 1941.33 108.979
-8663.47 1950.40 108.979
-8640.00 1962.67 100.713
-8597.33 1984.00 100.446
```

Actual ground polygon5314828:2505 is the descending doorway trapezoid:
(-8640,1962.667,100.713),(-8661.333,1962.667,108.979),
(-8655.733,1953.333,108.979),(-8640,1950.667,102.313).
Adjacent landing polygon2878, allZ100.713, has XY vertices
(-8637.333,1984),(-8640,1980.8),(-8640,1962.667),(-8618.667,1962.667),(-8618.667,1984).
The inset landing x[-8638,-8620],y[1964,1979] recognizes a committed human BEFORE reaching the
central spread region. The steering scaffold continues through the landing into the room, not a
permanent common assembly waypoint. A12y/4y-height scaffold tube is an eligibility envelope, NOT
proof of walkability: current native PathGenerator, height, LOS and whole-route checks decide that.

The conservative spread region is a45y disk around the Eye, floor98<Z<104. Every integer-degree
bearing at radii35/40/42/45 lay in actual ground polygons with all vertices98<Z<104;48/50/55 failed
39/62/120 bearings. Sampling is not continuous clearance proof. Native full-route validation remains
mandatory. Relevant mesh SHA256:

- `5314727.mmtile`: `e394fca2252ff899119916a1e0568d5a02d88292a7892b5546a005ee27421f9a`
- `5314728.mmtile`: `9f8e4786371c23ebc00d3e9fd8bcd5ce65536f597c560b57eea6b639d7971175`
- `5314827.mmtile`: `a796d6434bebed770c476cae69462615a1b76566f9f2780756834dee246cb06a`
- `5314828.mmtile`: `a73f97c9590f9d4a134faad50ad784166636a64e2265497267aa29c250980c59`

`CthunRoom.h` uses these landmarks; private geometry evidence is retained with the preparation
backup. Raw client-data tiles/full polygon dumps are not published.

- Corridor/pre-pull trash: release to ordinary human-led behavior; no autonomous assembly pull.
- Human committed to the true landing/interior, or encounter engaged: transit toward the room.
  Before engagement, native proposals cannot overtake the human's distance to the Eye.
- Transit: Lua prioritizes inward progress; crowding is soft rather than an impossible hard doorway
  spacing constraint. **Beam damage during transit is possible.** Exterior waiting is not a safe
  substitute. Soft near-endpoint reservations prefer distinct progress destinations where available.
  Progress is qualified BEFORE applying that preference: when the doorway forces a shared endpoint,
  bots still advance instead of waiting outside. Distinct endpoints/beam spacing cannot be guaranteed
  in genuinely constrained transit. Glare, route, floor, collision envelope and manual checks remain enforced.
- Interior: Lua uses spacing, reservations, yielding, support/range and observed glare decisions;
  native endpoints/routes stay within the conservative interior. Current visible aura/facing covers
  both possible glare directions, not private future timers/direction.
- Eye zero-health fake death selects the body even with no current target; stale phase intents release.
  Encounter phase comes from the instance, not the current AI engine; C'Thun remains registered in
  both combat and noncombat engines. Stomach/DONE/other-floor,
  passive/stay/CC/human players and non-human-led bots are excluded.

The current objective is the user's10-player raid. Larger rosters are bounded best effort;40 enlarged
combat envelopes cannot be promised simultaneous perfect spacing in this disk.

## Authority, routes and budgets

`LastMovement` carries C'Thun owner and spline IDs; ordinary setters/copies clear ownership.
Reload/stale/fault cleanup stops only the matching live policy spline; that token never authorizes
`MotionMaster::Clear()`. Completed ownership is retired without stopping or clearing replacement
active/controlled generators: native fear initialized during root can retain the finalized spline ID.
Only positively identified current automatic follow may clear its follow generator after native
eligibility checks. All active non-policy movement leases are protected regardless of priority. Explicit follow protects its current spline, not a
new permanent PvE until-go mode. An explicit combat-follow strategy (including the existing PvP
follow override) excludes tactical movement until that strategy is removed. Follow/go command
implementations and their PvP semantics are untouched.

`RecordCthunFollow` distinguishes normal scheduled follow from chat shortcut and direct engine
requests (`Engine::ExecuteAction` calls `MakeVerbose` before execution, including remote requests
without an Event owner). FollowAction does not otherwise use that flag; it is consumed locally.
An eligible current hold may stop positively tagged automatic follow, but never ambiguous
chase/follow/point motion. Unknown active chase/follow is excluded until it finishes or ordinary
movement replaces it; this conservative choice can delay tactical recovery. No generic engine rewrite was introduced.

Checked movement submits the exact validated linear PointsArray through `MoveSplineInit::MovebyPath`,
not `MoveTo(...exact_waypoint)` which regenerates a route. Retention validates the executing spline's
current position and remaining native control points, NOT a new path to its endpoint. Current origin is substituted BEFORE
validation. `StopMovingOnCurrentPos` disables the previous spline before launch; transport/vehicle/
flying/swimming are excluded, with no smoothing or water/hover-coordinate correction. This route
contract is source-audited and the production launch body is tested with explicit game API doubles;
it is not a native navmesh/live movement proof.

A route/LOS failure is native feedback, not an invitation to resume unchecked follow. The instance
records bounded failed endpoints per GUID and origin, rejects the current intent, and removes those
endpoints from the next candidate pool; Lua chooses the alternative. Budget deferral does NOT mark a
route failed. Entries are pruned to the current pool and discarded on movement, ineligibility, phase
change or accepted code replacement. Only after Lua has exhausted useful alternatives may old failures
be retried (at least1s since first rejection). Slow actors therefore explore alternatives before the
blocked best is reopened. No path is accepted without final native validation.

Bounds:32KiB source;2MiB Lua allocator per state (at most4MiB during candidate staging);300,000
VM instructions per protected initialization/evaluation;40 members, at most17 generated candidates
per member, one integer choice per member;64 path/retained-route validations per instance interval,
at most8 per bot per interval (also8 per native fallback search). Extra attempts reserve one first
validation for every other eligible roster member, including later members; this is not a promise of
8 attempts for all40 at once. Failure storage is at most53 endpoints per member and40 members, with
no retained unit pointers.64 route points,12y route length,6y waypoint,0.75y hazard samples. Candidate
features/neighbor work are bounded by raid/candidate limits, not repeated by movement multipliers.
The underlying pathfinder/OS still provide no hard real-time latency guarantee.

All allocating Lua C API operations run inside protected trampolines with only trivial C++ locals,
so Lua error/OOM longjmp cannot skip C++ RAII cleanup. Text-only load; no OS/io/package/debug,
coroutines, load/require, protected script calls, object userdata, action/DB/teleport/health APIs.
API v1 snapshot fields: `combat`, `eye`, `committed`, `members`. A member has opaque`guid`,
`eligible`, `entering`, `healer`, `melee`, `reach`, `candidates`. Candidate fields are
`x,y,z,glare,crowding,yield,range,goal,inside,interior`; these are native observations/features, not
permission to execute. Return a dense member-order integer array: `-1` releases, `0` holds,
positive`j-1` proposes Lua`candidates[j]` (Lua index1 is the current point). Unknown/out-of-range
output keys/values fail closed. Native final route/eligibility checks cannot be disabled by Lua.

Only restricted base + math libraries are opened; Lua decisions remain trusted administrative code,
not an adversarial process-isolation boundary.

Official Lua5.4.8 source is unchanged in PB `third_party/lua`; its MIT license is in
`doc/readme.html`, provenance in `PROVENANCE.md`. Archive SHA256:
`4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`.
Production and offline tests use that same CMake static-library target. Existing OpenSSL supplies
SHA256; no new global packages, Docker runtime packages, ALE or flakes are needed. ALE
co-installation is intentionally rejected instead of risking two conflicting Lua ABIs.

## Reload and operator handoff — parent deployment only

Native0033 and these directory mounts were installed in the September13 deployment. They are
also retained in the persistent `setup.sh` Compose template:

```
host runtime/playerbot-policies/      -> /opt/playerbot-policies       read-only
host runtime/playerbot-policy-status/ -> /opt/playerbot-policy-status  writable by worldserver
```

Mount whole directories, not individual replaceable files. Create restricted host directories with
appropriate worldserver read/status-write ownership. On this rootless Docker host, container acore
UID1000 maps to host UID100999, not the administrator's UID1000. The installed policy directories
have read/traverse ACLs for100999; status has write ACLs for100999 and inherited administrator access.
Do not blindly reuse this mapping on another host. To prepare directories on a running host:

```sh
umask 077
container_host_uid=$(docker exec ac-worldserver sh -c \
  'awk -v u="$(id -u)" "u >= \\$1 && u < \\$1 + \\$3 { print \\$2 + u - \\$1 }" /proc/self/uid_map')
host_uid=$(id -u)
mkdir -p runtime/playerbot-policies/{revisions,requests} runtime/playerbot-policy-status
for d in runtime/playerbot-policies runtime/playerbot-policies/revisions runtime/playerbot-policies/requests; do
  chmod 700 "$d"
  setfacl -m "u:$container_host_uid:r-x,d:u::rwx,d:u:$container_host_uid:r-x,d:g::---,d:m::r-x,d:o::---" "$d"
done
chmod 700 runtime/playerbot-policy-status
setfacl -m "u:$container_host_uid:rwx,d:u::rwx,d:u:$host_uid:rwx,d:g::---,d:m::rwx,d:o::---" \
  runtime/playerbot-policy-status
```

The deployment retained the checked source as an immutable revision and installed the offline checker at
`runtime/playerbot-policy-checker/policy-runtime-test`. Deployed0033 still requires generation-specific
publication; it has NOT gained automatic activation merely because0034 source exists. After the parent
finishes and deploys BOTH agreed phases, the installed-default lifecycle below replaces that requirement.
Initial installed publication is once per policy version, never once per map generation.

Native default configuration keys are
`AiPlayerbot.CthunPolicyDirectory` and `AiPlayerbot.CthunPolicyStatusDirectory`, with the paths above;
no env/runtime config change is otherwise required. Keep status and publisher access administrator-only.

### Phase1 source contract (not deployed; use only after combined readiness gate)

Publish a checked installed default ONCE/version, without a scope or active-generation selection:

```sh
python3 scripts/playerbot_policy.py publish-default raid-policies/aq40/cthun/policy.lua \
  --directory runtime/playerbot-policies \
  --checker runtime/playerbot-policy-checker/policy-runtime-test
```

This is future operator guidance, not an action performed in this lane. Ordinary editor saves do not
publish. The tool checks the exact immutable bytes and atomically installs `defaults/raid.txt`; all
relevant scopes discover it automatically. New map generations inherit it after recreation/restart/
natural reset; no raid-entry command. Later publications activate between pulls while players remain
logged in/in the raid. Initial observation in combat remains native until the same safe boundary.
Phase1 retains existing C'Thun approach/human-led eligibility; it does not yet offer generic raid tactics.

The native phase1 scope reads the fixed256-byte manifest and its own256-byte optional diagnostic request
once/second, with one extra bounded manifest observation per safe candidate attempt. Source remains
32KiB; Lua memory/instruction/path budgets are unchanged. No scans/daemon/globalVM. The manifest identity
is re-observed before attempting a safe commit; publication after that observation is handled at the
next poll, not an impossible filesystem-revocation lock. Reads may block on trusted local storage.

`publish`/`revert` remain optional diagnostic per-scope CAS, **not production initialization**. They bind
to the default publication observed in current status and expire on its replacement/removal/invalidity.
A new default automatically supersedes pending/adopted diagnostics; no travel/recreation to unpin.
The publisher rejects stale status/default observations; the native scope also checks identity and
expected active revision at safe commit. Old three-field requests are accepted only without a valid
default; new publisher wire is not compatible with the still-running0033 binary.

`status` distinguishes actual Lua/native backend, desired/queued revision and source, default observation
errors, diagnostic override, active-fault quarantine, source retry, safe boundary, adoption and heartbeat.
Destroyed/absent/stale scopes remain explicit. `publish-default --expect-default ID` optionally provides
host manifest CAS (`none` for first install); absent that flag, cooperating checked publishers serialize
and last publication wins. The resulting `nonce:digest` ID is returned by the tool and exposed by scopes.

See the companion guide for failure/retry rules, POSIX ACL installation and exact phase2 acceptance.

## Evidence and remaining gates

```sh
PYTHONPATH=scripts/tests python3 -m unittest test_raid_policy test_cthun_positioning \
  test_ouro_combat test_ouro_recovery test_bug_trio_reset test_twins_coordination test_twins_reset -v
```

40 tests pass. Real Lua tests cover syntax/API/loop/allocator OOM/forbidden APIs/output limits,
independent instance ownership/teardown, queued between-pull reload/CAS/rejection/rollback and active
fault quarantine. Actual C'Thun source + production launch/ownership bodies compile against API
doubles with UBSan/warnings-as-errors. Ten-player landing-to-interior fixture reached minimum bot
spacing15.5037y with mixed melee/ranged/healing roles; both glare bearings, casting/manual/phase/no-path/staleness cases pass. A Lua-only
conditional edit changes entry behavior in the same binary. The same constrained-entrance assertion
fails original0031 at `FindPosition` and passes0033 native fallback. An endpoint-repath retention
mutant fails the new intermediate-hazard regression while actual executing-spline validation passes.
The pre-correction Lua fails the distinct-entry-endpoint regression in the SAME integration binary;
corrected Lua prefers distinct progress points and still advances when only shared transit is available.
The targetless Eye/body transition remains covered without an engine rewrite or Ouro tactical changes.
Both P1 review regressions fail the preserved reviewed adapter and pass the correction: a path-specific
blocked best with unchanged positions reaches a lower-ranked checked progress endpoint, while finalized
same-ID ownership cleanup preserves a controlled fear generator and its CC state under root. Active
owned cancellation remains covered. Budget deferral, later-roster first opportunity, failed-pool
pruning, slow-actor exploration, exhaustion retry and origin/phase resets are also covered.

Historical0031 tests remain intact against the reverse0033 baseline; they are explicitly NOT claims
that its exterior/pre-pull/cast-stop behavior should survive or that it ever worked live. Current
behavior is covered by the separate real-Lua integration suite. Ouro/Bug Trio/Twins regressions pass.
0033 actual-baseline forward/reverse byte roundtrip passes. Changed-line native style passes; whole
legacy MovementActions files retain pre-existing qualifier-style findings, not silently repaired.

Preparation backup: `backups/lua-cthun-preparation-20260913-173226/`. Original actual1562-file baseline,
repository identities/statuses, geometry evidence, build/test logs, source hashes and preservation
checks are retained privately there. No commits/staging/push/deployment were performed in this lane.

A separate parent-accepted raid-eligibility change was mechanically integrated from its reviewed
private workspace: exactly12 allowlisted files, each prechecked against its private baseline and
postchecked against the accepted result hashes. It is canonical root-module publication plus a mirrored
build source, OUTSIDE0033; see `raid-scaling-creature-eligibility.md`. Its38-test suite also passes in
actual root. No copied contents were redesigned, and no mound count, speed or timer changes were made.

**Subsequent gates completed:** retained targeted re-review accepted both P1 corrections; the combined
Lua/scaling native image built successfully and was deployed after a fresh logout and preservation checks.
**Outstanding:** BOTH automatic default loading and the queued generic Lua-only boss tactics milestone
require combined review/build/deployment before any live gameplay/adoption test. Offline fixtures and compilation do not prove live navmesh, latency, HPS, recovery,
aggro timing, encounter success or difficulty. Deployment evidence separately confirms saved AQ state
and inventory preservation.
