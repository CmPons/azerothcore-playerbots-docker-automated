# Twin Emperors: fixed physical-station healing

## Status

Prepared and offline-tested September 12, 2026. This intermediate allocation was **not deployed
by itself**. The [0029 follow-up](twins-caster-victim-coverage.md) keeps Meliah on Ari, puts Keilmere
on Redshift, and has Ailina cover the actual caster victim. The user reported a legitimate kill
before either update was deployed, then authorized deploying both strategy improvements together.
Both are now [deployed and verified](twins-learning-deployment-20260912.md); no encounter reset was performed.

## Change

One production file: `modules/mod-playerbots/src/Ai/Raid/Aq40/Aq40Coordination.cpp` in the core tree.
Canonical incremental patch: `patches/0028-playerbot-twins-station-healers.patch` (after 0026/0027).
The file was already untracked in the nested module, so the patch is derived against the preserved
pre-edit working source, not against nested HEAD. Unrelated nested work is retained.

With the usual roster, the intermediate 0028 healing/movement anchors were (superseded by 0029):

| Healer | Stable coverage |
|---|---|
| Meliah | Ari's physical station, including incoming Shadow Bolts after a swap |
| Ailina | Redshift's physical station, including incoming melee after a swap |
| Keilmere | Beliona's caster-tank coverage, plus normal nearby emergency healing |

No names are hardcoded. Existing deterministic healer ordering and physical tank selection are
retained. The helper undoes the teleport ownership swap when selecting dedicated station anchors:
**boss ownership changes, but the physical tank's healer does not**. These are anchors to the tank
players, not fixed coordinates or raid subgroup numbers.

The old code assigned Meliah to the current melee owner and Ailina to the caster owner, moving
coverage across the room at the exact moment physical tanks could take Shadow Bolts or melee
bursts. The extra healer preferred already-reachable targets and could remain away from Beliona.
The new extra-healer anchor therefore prioritizes the caster tank instead of drifting toward a
healthy nearer station while that caster tank is remote.

Existing behavior retained:

- Stay in any safe, in-range, LOS position (34-yard-or-smaller movement buffer); move locally when
  range/LOS or hazards require it. No central-room camping order and no forced ring while in range.
- Tank healing preference is not exclusive; nearby critically injured players remain eligible.
- Finish useful in-range casts when appropriate. Hazards, passive state, CC and movement leases
  still apply. No injected heals, free health, threat, teleporting or damage/HP/timer changes.
- If a dedicated healer dies, the extra healer fills that station without reassigning the surviving
  dedicated healer. If a physical tank dies, coverage falls back to a surviving station, then the
  caster; if the caster tank dies, native class/role fallback is covered.
- The on-demand `do aq40 twins status` command reports the new anchor/range/LOS without new spam.

## Verification

Four focused Python tests pass, including compiled production helper/actions/triage code with
warnings-as-errors and UBSan, patch round-trip and focused pinned replay. Full pinned regeneration
still has the unrelated older 0021 limitation; the focused replay explicitly excludes it.

Tests compile the prior production healing helper as a separate namespace and reproduce its
post-swap Meliah->Redshift / Ailina->Beliona assignments. The correction keeps Meliah->Ari and
Ailina->Redshift through two swaps, requires no unnecessary movement from safe in-range positions,
and selects the injured physical tanks using actual production healing triage. Healer deaths,
physical tank deaths, caster fallback, local emergencies, range/LOS behavior, other raid policy
and updated status output are also covered.

Offline tests do not prove live pathfinding, reaction latency or healing throughput. Deployment is
complete; a subsequent real pull is still needed to validate those. This is a positioning/assignment correction, not
another encounter nerf.

Preparation backup: `backups/twins-station-healers-preparation-20260912-200547/`.
