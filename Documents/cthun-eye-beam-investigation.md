# C'Thun Eye Beam: read-only investigation — September 13, 2026

The user reports multiple group members being one-shot by "Eye Beam" and requested investigation
after Ouro. **No C'Thun code, spell data, scaling or live state was changed.** Ouro's recovery image
was built separately, not deployed; the user continued playing on the unchanged live image.

## Main findings

1. The installed playerbot AQ strategy contains dedicated **Twins** actions/triggers, not dedicated
   C'Thun entry, spacing or Dark Glare avoidance. Searches for C'Thun's names, creature IDs 15589/15727
   and spell IDs 26134/26029 in playerbot source found no corresponding handling. Generic combat
   behavior still exists, but is not an encounter strategy. The earlier claim that both remaining
   encounters had existing encounter strategies was too broad.
2. Green **Eye Beam (26134)** is a chain spell. On this server its DBC multiplier is **1.5 per hit**
   (50% growth), not an assumed doubling. It allows up to 60 targets; its attribute restricts it to
   players, which includes playerbot characters, not hunter pets.
3. Its native jump radius is nominally **10 yards**. The distance test includes both units' combat
   reach; normal player reach is 1.5 yards each. Thus a bare 10-yard center-distance formation is
   insufficient. Approximately **15-yard center spacing** provides a practical normal-size buffer;
   eventual AI should test the native predicate with a margin rather than hardcode model-size assumptions.
4. The main Eye (15589, rank 3 / boss flag) and Giant Eye Tentacle (15334, rank 1) qualify for the
   existing raid scaler. Chain targets are processed by the same spell/caster, with a per-target
   damage multiplier, and damage passes through the unit damage scaling hook. No evidence was found
   that Eye Beam generally bypasses scaling or only scales its first target.

This makes chained hits through a tightly grouped raid a strong explanation for multiple deaths,
even at the existing reduced damage. Actual hit order, positions and amounts were not recorded in
the inspected server logs, so this is not a reconstruction of the user's particular lethal cast.

## Verified spell data and damage illustration

The inspected DBC matches the running container's `Spell.dbc` byte-for-byte:
`d5cce1a83550dcfa9eb2f0251dbb11fd24c272534b2b1a9b230924a44d817ab3`.
Read-only database queries found no `spell_dbc` or jump-distance override for 26134/26029. The
registered Dark Glare target filter is present; no Eye Beam spell-script override was returned.

Eye Beam: nature damage, base points 2624 plus a 1–751 die roll: **2,625–3,375** before other modifiers.
With the configured default 10/40 damage factor `(10/40)^0.6 ≈ 0.435275`, illustrative noncritical
values before accounting for resistance/absorbs and other combat modifiers are:

| Hit in chain | Approximate damage |
|---|---:|
| First | 1,143–1,469 |
| Fourth | 3,856–4,958 |
| Fifth | 5,784–7,437 |
| Sixth | 8,677–11,156 |

These are calculations from source/data and the default scaling policy, **not captured combat-log
hits or a live read of a possible manual per-instance multiplier**. Later hops can plainly exceed
level-60 character health. More healing cannot reliably rescue an already-lethal hop.

The native opening sequence directs the first three scheduled green beams at the player who
engaged the Eye. A stacked entrance behind that puller is therefore especially risky.

## Distinguish the red sweep

**Dark Glare (26029)** is the separate red rotating beam. Native damage is 43,750–56,250 shadow,
still roughly 19,043–24,484 under the default factor. It filters a narrow line and sweeps about
180 degrees over 35 seconds. This is an avoidance mechanic, not something the raid should tank.
A player saying "eye beam" colloquially may mean this instead; color or combat-log spell name
would distinguish the two.

Small Eye Tentacles (15726) are normal-rank and currently outside generic raid scaling, but their
script casts **Mind Flay (26143)**, not Eye Beam. Any later audit of those adds should remain separate
from diagnosing the reported chain beam; no add scaling adjustment was made here.

## Recommended next work — at the time of investigation

Subsequently authorized positioning work is documented in [cthun-positioning.md](cthun-positioning.md).
The investigation above describes the pre-change live implementation.

Prepare C'Thun-specific bot coordination: safe entry and persistent spread with healer coverage,
movement that does not immediately collapse back into ordinary follow/melee formations, and
explicit red-sweep avoidance. Tentacle priorities and phase-two/stomach handling also need an
encounter audit before claiming full support. Preserve the human's control and existing passive,
CC, manual-order and movement-lease policies. Do not start with a blanket damage nerf.

## Local source references

- `modules/mod-playerbots/src/Ai/Raid/Aq40/Aq40Strategy.cpp` (core tree): installed Twins-only strategy.
- `src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_cthun.cpp`: Eye opening sequence, green casts,
  red sweep/filter, small-eye Mind Flay and giant-eye Eye Beam.
- `src/server/game/Spells/Spell.cpp`: `SelectImplicitChainTargets`, `SearchChainTargets`,
  `HandleLaunchPhase`, `DoAllEffectOnLaunchTarget`.
- `src/server/game/Entities/Object/Object.cpp`: `_IsWithinDist` and `GetObjectSize`.
- `src/server/game/Entities/Player/Player.h`, `Entities/Object/ObjectDefines.h`: normal player reach.
- `modules/mod-raid-scaling/src/RaidScalingMgr.cpp` and `RaidScalingLoader.cpp`: creature eligibility
  and final unit damage hook. Root and build-tree module copies remain unchanged.
