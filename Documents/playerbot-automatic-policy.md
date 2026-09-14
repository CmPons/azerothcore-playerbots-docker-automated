# Automatic policy loading — phase1 implementation reference

**Deployed together with the first-playable raid-combat API2 on September14,2026.**
Use the [current operator workflow](raid-combat-lua.md) and
[deployment evidence](raid-combat-lua-deployment-20260914.md).
The user narrowed first playable to combat positioning/avoidance; the older exhaustive phase2
roadmap below is historical, not a current release gate. Phase1 was not deployed alone.

Phase1 incremental patch: `patches/0034-playerbot-automatic-policy-default.patch`, against the preserved
ACTUAL0033 deployed source at root HEAD `8ddbb858a5e8d96c07787ef5e770b5277222c5fc`. Never rewrite0033,
reset to pins or run setup to replay this delta. Native allowlist is only
`modules/mod-playerbots/src/Ai/Raid/Policy/CthunPolicyScope.{h,cpp}` inside core. The default control plane
is a fixed raid bundle. API2 now supplies the generic raid-instance consumer.

## Installed-default lifecycle

`defaults/raid.txt` is a bounded256-byte text record: `1 1 NONCE SHA256` (transport format/version1,16 lowercase
hex nonce,64 lowercase hex digest). Source is immutable `revisions/SHA256.lua`, at most32KiB. Its bytes
are checked with the production pinned Lua offline checker before publication. The whole-directory
policy mount remains read-only to worldserver, status remains separate/writable. No new config/mount,
watcher, service or automatic editor-save execution.

Every relevant instance scope discovers the same default, without a scope-specific publication. Existing
map-serialized hook observes creation/load/login/recreation naturally: an in-world human-led raid with
bots on the verified C'Thun approach/interior creates the scope. Unrelated random-bot raids, distant
areas and other bosses do NOT activate this phase1 consumer. No grid load or extra creature search is
introduced. Scope teardown/recreation/restart/natural reset reuses the installed default, not old
requests. If first observed during combat, it queues while native fallback continues until safe.

Existing safety boundary is unchanged: C'Thun not IN_PROGRESS and no observed instance player in combat,
including trash/human combat. One-second discovery,250ms planning; at a safe candidate attempt one extra
manifest read revalidates the observed identity. Publication after that read is handled on the next poll;
this is a bounded filesystem snapshot, not instantaneous race-free revocation. One active instance-owned
VM plus one transient candidate, no AI/globalVM locks or retained live-object pointers. The existing
2MiB/state,300,000-instruction protected-call, movement/path/failure-feedback budgets are unchanged.

| Condition | Behavior |
|---|---|
| Valid new default, safe | Validate hash/text/LuaAPI/initial plan, then adopt and reset Lua state/adoption token |
| Same good bytes already active | No needless VM replacement or state reset; expire old diagnostic binding if publication changed |
| New default during combat | Desired/queued changes; active VM and current pull unchanged until safe |
| Missing/unreadable/invalid/oversized manifest | Preserve active last-good or native; cancel obsolete pending work; `default_error` visible; no Lua retry |
| Missing/unavailable source | Keep queued and last-good, retry source at most once/30s only when safe; no candidate VM before valid hash |
| Hash mismatch/oversize/nonregular source, syntax/API/budget/init-plan failure | Destroy candidate, keep last-good/token; suppress reattempt for unchanged failed publication; publish corrected immutable bytes/version |
| Active Lua evaluation fault | Retire active VM/owned intents through existing generation cleanup; explicit native fallback and persistent fault digest/detail |
| Same active-fault digest republished/requested | Quarantined, including nonce changes; only successful adoption of distinct bytes clears quarantine |
| Default disappears and reappears | Pending work canceled at disappearance; unattempted/transient work may requeue on reappearance; unchanged rejected publication stays suppressed |

Successful code replacement is not gameplay/save/loot rollback. Native rotations, healing, threat,
movement/manual/CC guards, fear/finalized-spline cleanup and boss sources remain untouched.

## Publication and permissions — parent operation after BOTH phases

Use the existing installed checker, or build the focused offline checker without worldserver/Docker:

```sh
PLAYERBOT_POLICY_TEST_BUILD=/tmp/playerbot-policy-checker PYTHONPATH=scripts/tests \
  python3 -m unittest test_raid_policy -v
python3 scripts/playerbot_policy.py check raid-policies/aq40/cthun/policy.lua \
  --checker /tmp/playerbot-policy-checker/policy-runtime-test
python3 scripts/playerbot_policy.py publish-default raid-policies/aq40/cthun/policy.lua \
  --directory runtime/playerbot-policies \
  --checker /tmp/playerbot-policy-checker/policy-runtime-test
```

The publication command is ONCE per version, not per instance. It creates absent mailbox subdirectories
privately, preserves existing directory modes/ACLs, locks cooperating publishers, links fully written
immutable revisions, fsyncs bytes/directories and atomically replaces the manifest. It emits JSON with
publication identity `NONCE:SHA256`; checker diagnostics go to stderr. Optional `--expect-default none`
(first install) or `--expect-default NONCE:SHA256` supplies manifest CAS. Without CAS a checked new
publication replaces even an invalid manifest; cooperating concurrent publishers serialize/last wins.
Publish an older retained source through `publish-default` for code-only rollback. Do not modify a
hash-named source in place, and do not treat an editor save as publication.

Policy directories must permit mapped-acore read/traverse, never policy writes. The prior deployment
records container UID1000 mapping to host100999; do not infer this on another host. Directory creation
inherits parent default ACLs. For a verified mapping, parent can prepare (not executed here):

```sh
umask 077
# Set container_host_uid from a VERIFIED deployment UID mapping, not the host login UID.
mkdir -p runtime/playerbot-policies/{revisions,requests,defaults}
for d in runtime/playerbot-policies runtime/playerbot-policies/{revisions,requests,defaults}; do
  setfacl -m "u:$container_host_uid:r-x,d:u::rwx,d:u:$container_host_uid:r-x,d:g::---,d:m::r-x,d:o::---" "$d"
done
```

Retain existing status-directory write ACLs and parent read access as documented in the original guide.
Check ACLs on both newly created and replaced manifests/revisions; don't chmod existing directories in
ways that remove effective named-user access. The focused test verifies inherited100999 ACL metadata
on temporary directories; it does NOT impersonate the deployed UID or test actual mounts. Parent must
verify actual read-only policy/status-write permissions at combined deployment. No runtime files were
read/written by this implementation lane.

## Optional diagnostic CAS, never a production pin

Four fields in `requests/MAP-INSTANCE-GENERATION.txt`:
`REQUEST_NONCE EXPECT_ACTIVE NEXT_DIGEST OBSERVED_DEFAULT_ID`.
The last field is `none` without a valid default, otherwise `NONCE:SHA256`. Host `publish`/`revert` uses
current status and checks the installed manifest matches it. Native processing is default-first and
checks expected-active both at acceptance and safe commit. Legacy three-field requests only work when
no valid default is observed; otherwise they fail visibly.

A successful diagnostic overrides only that observed publication; next default replacement/removal/
invalidity expires it, and pending diagnostic work too. Current good VM is retained until the default
can safely adopt. No recreate/travel/unpin command needed. Failed diagnostics do not disable default
following. Observed request bytes are consumed once, never replayed because an old file remains. A
request aimed at another map generation is never read. New publisher wire is incompatible with the
still-running0033 binary; these examples are NOT permission to publish now.

Status reports `active`, `active_source`, `desired`, `desired_source`, `queued`, `default_publication`,
`default_revision`, `default_error`, `diagnostic_override`, `error`, `faulted_revision`, `fault_error`,
`retry_source`, `safe_boundary`, generation/adoption/exclusions and heartbeat/destroyed state.
`diagnostic_error` separately retains the last diagnostic rejection/failure/expiration, even when a
successful automatic adoption clears `error` before status is written. It clears on an accepted new
diagnostic or successful diagnostic adoption, and is replaced by subsequent diagnostic failures or
expiration; default-only updates cannot erase it. Like other error text it is escaped/capped at200 bytes.
Missing
valid default makes desired empty rather than pretending stale bytes are still installed. Last-good
active remains separate. `retry_source` means30s safe-boundary retry, not immediate adoption. The CLI
labels `active=native` as native fallback, never active Lua. Adoption is guarded bot observation, not
proof of simultaneous movement or encounter success.

## Queued phase2 acceptance — required before gameplay

The user's first usable milestone is: **pull -> alt-tab -> straightforward Lua edit in1–2minutes ->
checked once/version publication -> automatic adoption BETWEEN PULLS -> return/pull**. No per-boss C++,
rebuild/restart/logout or scope command after one foundational integration. Concise helpers, clear
validation/action rejection diagnostics and practical edit ergonomics are part of this milestone.

Current API1 is **C'Thun position-only**, with native Eye/body/phase selection, room/floor/approach
geometry, glare/crowding/range candidates and stomach exclusion. Dropping in arbitrary `boss.lua` does
not work. Another positioning heuristic is easy; another boss still needs the queued foundation.
Reusable runtime ownership/sandbox/checker/atomic default transactions/last-good/freshness and checked
movement provenance should be retained, not replaced by a second control plane.

Phase2 must expose boss-neutral copied observations and checked generic tactical intents. Lua owns map/
entry dispatch, observed phase/hazard interpretation, geometry, roles, timing and tactics, with no new
boss-ID C++ branches or wrappers around bespoke native boss tactics:

- Loaded creatures/GOs/observable hazards, roster/roles, positions/facing/reach, health/power, ordinary
  cast/aura/target/threat-relevant facts and bounded observed events; no hidden AI timers/grid loads.
- Checked move/hold/release plus Lua geometry; target/assist priority with protection/CC and AoE/pet
  exclusions; tactical interrupt/dispel and actor-known/usable spell/ability requests.
- Support/healing target hints (including friendly NPCs) preserving native spell choice/triage;
  legitimate GO/spell-click and owned-item unit/GO/item targeting; owned pet/vehicle control.
- All ordinary range/LOS/cooldown/GCD/resource/reagent/seat/control checks, no triggered casts, free
  heals/teleports/item creation/manual-lease bypass. Normal rotations/healing remain authoritative.
- One instance VM plus one bounded candidate, serialized checked execution, fresh state at between-pull
  replacement, explicit overflow/unknown/failure diagnostics. Multi-boss source/memory/instruction/
  entity/event budgets must be measured and approved; phase1 did NOT silently enlarge them.
- Demonstrate Lua-only tactics across unlike bosses in the SAME native binary: C'Thun entry/spread/
  glare/stomach/adds; Ouro moving mounds/targetless phase; Twins roles/restricted targets; Magtheridon
  cube interaction; Vashj core-item targeting. Prove friendly-NPC healing and vehicle primitives before
  claiming full Wrath raid coverage. Policies need not all be complete during interface design, but
  interface authorability cannot rely on later per-boss native adapters.

Source-grounded feasibility risks: `UseItemAction::UseGameObject` currently calls GO Use with insufficient
exposed-boundary checks; `PartyMemberToHeal` is player/group-centric and has native Twins preferences;
vehicle actions contain IOC selection and incomplete movement behavior. These require generic audited
foundation seams, not unrestricted direct calls. Observable visuals may not encode exact hazard shapes;
Lua must interpret mechanics. Literal unlimited future mechanics is not promised by a finite API.

Private queued phase2 design/API matrix/representative challenges and proposed bounds are retained at
`backups/lua-automatic-policy-20260913-214404/queued-phase2-design.md`. Its original design-only status is
historical; phase1 implementation then resumed by explicit supervisor approval. No phase2 code exists yet.

## Offline evidence and gates

The focused suite adds automatic-start regression that FAILS original0033, actual-adapter first-in-
combat/recreation/no-human/approach eligibility, two-scope default/CAS supersession/disappearance/fault
quarantine/30s-retry tests with real pinned Lua, atomic concurrent host CAS and inherited ACL checks.
Existing candidate-failure fairness and finalized-spline/fear regressions remain unchanged and pass.
0034 forward/reverse actual-byte roundtrip and layered0033 roundtrip are checked. Lua/affected and
scaling suites overlap; fixtures are not native full-header or live navmesh/latency/encounter evidence.

No native/Docker build, live changes, service restart, DB/gameplay or runtime publication was performed.
Independent phase1 review, phase2 implementation/review, then parent-controlled combined build/deploy
and live acceptance remain required. The user resting is not permission for an intermediate restart.
