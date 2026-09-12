# Twin Emperors bugs: participate in existing raid scaling

## Status / scope

Prepared September 12, 2026; **not built or deployed**. The user reported that tank separation was
mostly working, but mutated bugs were an excessive wall for ten players. They authorized fixing
add scaling, **not** changing the encounter or mutation's native mechanics.

No server interruption, live commands, inventory changes, SQL writes, resets, configuration changes
or Pi generations were performed. Both this change and the separately prepared
[stockup reagent fix](raidroster-stockup-reagents.md) need an authorized build/deployment.

## Findings

- Qiraji Scarab **15316** and Qiraji Scorpion **15317** have normal rank (0), creature type 10,
  level 60, and template health multiplier 1. The old scaler accepts bosses/elites, not these entries.
  Its damage hook therefore returned 1.0 and its health application skipped them entirely.
- Mutation **802** keeps its native **+300% HP (4x)** and **+1800% physical damage (19x)**. The core
  also retains its poison-proc application, hostile faction/react state and zone-combat behavior.
- The same entries occur outside the encounter. Read-only world DB checks found **50 scarabs and
  50 scorpions linked to Vek'lor**, plus **47 scarabs and 36 scorpions without that link**. An
  entry-only/map-only exception would unnecessarily change corridor mobs too.
- There is a second health issue: `Creature::UpdateMaxHealth()` rebuilds health from
  `UNIT_MOD_HEALTH`, **not** `CreateHealth`. The existing scaler writes create/max/current health
  without adjusting that underlying base. Adding eligibility alone would let mutation recalculate
  these bugs back to unscaled HP.

## Implementation

Canonical source:

- `modules/mod-raid-scaling/src/RaidScalingMgr.cpp`
- `modules/mod-raid-scaling/src/RaidScalingMgr.h`

Both are synchronized to `azerothcore-wotlk/modules/mod-raid-scaling/src/`. This is a root-maintained
module; no extra patch to the regenerated core/playerbots repositories is required.

### Identification and damage

`IsTwinsEncounterBug` requires map **531**, one of the two entries, real creature spawn data, and
its native linked-respawn target entry **15276 (Vek'lor)**. This matches the identification already
used by `npc_ahnqiraji_critter::Reset`; no spawn IDs or character names are hardcoded.

These bugs become eligible **before mutation** so the existing spawn/map-load health hooks work.
Their normal rank is retained, so they use **trash** multipliers. Pets, triggers, true critters and
civilians remain excluded. There is no aura/encounter-active eligibility gate that could disappear
on mutation removal or prevent restoration.

Damage uses the existing outgoing-damage hook and instance's current settings. Player and
player-controlled victims receive scaled damage; NPC-vs-NPC damage remains untouched. Default,
manual trash overrides, explicit off and separate-instance state retain their existing behavior.
No multiplier is hardcoded into this exception, and mutation damage is not scaled twice.

### Health that survives mutation

The original-stat cache also records the creature's unit-mod health base. **Only the identified
Twins bugs** use the new health path:

1. Scale the cached original base once, not the previously scaled or mutated maximum.
2. Keep native health aura modifiers and call `UpdateMaxHealth()` to rebuild the maximum.
3. Preserve the current health fraction; repeated application cannot heal an injured mutated bug
   merely because its scaled mutated maximum happens to equal its original unmutated maximum.
4. On scaling-off, restore the original base while retaining the current aura state and health
   fraction. Dead bugs stay at zero health; no respawn/revival is introduced.

All other creatures keep their existing health-application/restoration path. This deliberately
avoids a broad rewrite affecting other bosses' phase health or buffs.

## Expected effect at the current 10/40 default

| Quantity | Before | After deployment |
|---|---:|---:|
| Base HP, using current 3,052-HP template baseline | 3,052 | 763 |
| Fully mutated maximum HP | 12,208 | 3,052 |
| Damage multiplier relative to the same unscaled attack | 1.0 | `(10/40)^0.6`, about 0.4353 |
| Mutation tooltip | +300% HP / +1800% physical damage | **Unchanged** |

That is one quarter of the previous mutated HP and approximately **56.5% less outgoing damage**
under the current default. This is not a promise of a kill: correct swaps, separation, local add
focus and healer coverage still require live validation.

No changes to emperor health/damage, Heal Brother, teleport, immunities, mutation count/timers,
explosion spell/fuse/radius, poison mechanics, spawn/respawn counts, AI roles or bot targeting.
The encounter bugs' own attacks use their now-correct scaling eligibility; their native spells
are not rewritten. Corridor bugs and unrelated normal-rank raid creatures stay unscaled.

## Verification

```bash
python3 -m unittest discover -s scripts/tests -p test_twins_bug_scaling.py -v
```

Six offline tests compile actual manager methods/header/state, the production damage hook and
core health-total calculation, creature health update and health-percent aura handler against API
doubles. C++20, warnings-as-errors and undefined/float-division/float-conversion sanitizers are used.

Coverage includes both entries; mutation apply/remove/reapply; injured-bug reapplication;
mutation already present on first scaling; manual trash HP/damage overrides; off/re-enable;
instance isolation; player/pet/NPC damage; dead-bug restoration; corridor/wrong-link/map/entry and
missing-data exclusions; legacy eligibility-only health failure; unchanged ordinary boss behavior;
and byte-identical source copies.

Full selected regression suite: **78 passed, one optional connection-local MySQL test skipped**
(79 total), including six new tests and the existing stockup, durability, Twins/AQ40, pet, skull,
chatter/bridge, scaling/reset and statistics tests. Scoped official C++ style checks and added-line
120-column checks passed.

Preparation backup: `backups/twins-bug-scaling-preparation-20260912-160109/`.
These tests do **not** substitute for a full server build or prove live pathfinding, add-switch
latency, healing throughput or encounter difficulty. No current raid was interrupted to test them.
