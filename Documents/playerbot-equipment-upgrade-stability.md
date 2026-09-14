# Playerbot automatic equipment stability

Incremental source publication: `patches/0036-playerbot-equipment-upgrade-stability.patch`.
Apply after the actual post-0035 source, not an unmodified nested Git HEAD.

## Confirmed decision bug

Three interacting source behaviors explain the shoulder cycle:

1. `RandomItemMgr::CanEquipArmor` counts stat *types*, not magnitudes. All three hunter shoulders
   have agility, intellect, spirit and stamina, yielding `sp=3`, `ap=2`, `tank=2`. Its hunter
   heuristic therefore labels **all three unsuitable**, despite their native hunter legality.
2. `ItemUsageValue::QueryItemUsageForEquip` had an existing raw-stat anti-oscillation guard,
   but `(shouldEquipInSlot || !existingShouldEquip) && isBetter` bypassed that guard and the
   upgrade threshold when the incumbent was considered unsuitable.
3. `CalculateItemSetMod` awarded an unequipped candidate a threshold-completion premium that
   could disappear immediately after equipping it. Batch selection then equipped all previously
   selected item IDs without checking the new incumbent, also discarding instance identity.

This is not a demonstrated value-cache bug: the default calculated-value interval of one
recalculates on every `Get`. Batch selection alone was not proof of the recurring cycle.

## Focused reproduction

A private offline demonstration used the exported inventory/item templates and read-only installed
DBC set thresholds/item spell effects. Its C++ harness extracts the actual before/after set scorer
and armor-stat heuristic; relevant BM weights and selection predicates are transcribed.
No client assets or character exports are published with this change.

With other equipment frozen at the exported snapshot, equipped set counts are T1=3, T2=2,
T2.5=2. Assuming BM ranged PvE, no Careful Aim/Hunter vs Wild aura, threshold 1.05 and successful
native swaps:

| Shoulder | Raw score | Shared-baseline replacement score |
| --- | ---: | ---: |
| Giantstalker's, 16848 | 8713.15 | 12721.20 |
| Dragonstalker's, 16937 | 9869.34 | 13027.53 |
| Striker's, 21367 | 8948.88 | 13781.28 |

Before the correction, Striker's equipped score drops to 11544.06. The reproduced batch sequence
is **16937 → 21367 → 16848 → 16937**, repeating. Afterward, all three starting shoulders converge
to 21367 (zero or one swap in this fixture), with no further swaps over eight passes. This retains
a genuine set-aware improvement despite its lower raw score; it does not force any item or tier.
The class-restricted druid shoulder 21354 remains subject to unchanged native usability checks.

**Actual runtime threshold is 1.1, not the exploratory fixture's 1.05.** Parent reran the
same extracted reproduction at 1.1: the old cycle still occurs, but the corrected path
makes zero swaps from each starting shoulder across eight passes. Striker's approximately
5.8% modeled set-aware gain over Dragonstalker's is below the configured 10% threshold.
The fix therefore stops cycling without promising that she will select Striker's or
changing configuration. Evidence: `parent-threshold110.cpp` and `parent-threshold110.log`.

The precise live trigger/aura state was not observed: this establishes a source-backed reproduction,
not a captured live trace. Random noncombat, trade/loot and hunter no-ammo paths can invoke the
automatic action; no specific live trigger is claimed.

## Change boundaries

- Automatic upgrade actions evaluate each currently owned bag instance immediately before equip,
  with its own entry, random property and durability. Ownership and current inventory position are
  checked. An intact duplicate no longer hides a broken candidate.
- Single-slot armor alternatives share a set-count baseline excluding exactly the destination
  piece. Both incumbent and candidate receive the same threshold treatment. Same-set replacements
  do not add an extra piece; another equipped duplicate is not subtracted.
- Unsuitable-to-unsuitable replacements cannot bypass the threshold/set comparison. The existing
  suitable-for-unsuitable fallback remains.
- Manual `equip`, outfits and explicit raid `EquipItems` callers are unchanged. Native equip
  handlers, uniqueness checks, successful-change tells, and intact-for-broken fallback remain.
- Fingers, trinkets, weapons and offhand rearrangements retain their existing scoring policy.
  Generic loot valuation retains its default best-copy durability lookup; only owned automatic
  candidates use exact-instance durability.

This is not a whole-loadout optimizer. Changes in other slots, spec, auras or durability can change
legitimate upgrade decisions; convergence is established for the fixed-context single-slot case,
not arbitrary mixed-slot optimization. Existing paired-slot/weapon scoring limitations remain.

## Checks and delivery

- All three affected production-header translation units passed the existing offline syntax checker.
- Existing C++ style checks scoped to added lines passed, including 120-column/whitespace checks.
- Private extracted C++ checks passed for the cycle, convergence, same-set/duplicate counts,
  full-set threshold, lower-raw set upgrade and exact candidate durability/fallback flags.
- Patch 0036 applies and reverses byte-exactly against the six saved pre-edit files.
- Prior nested diffs outside those files and all three Git index snapshots were preserved;
  no files were staged. Patches 0001–0035 and the root `flake.lock` intent-to-add were not changed.

Private evidence: `backups/equipment-stability-20260914-185536/validation/` contains the
source hashes, reproduction inputs/harness, syntax logs and scoped style result.

Patch SHA-256: `7cff8b2163d7c6d20809e61c7842a02f4c4b6ffb8d201e858c0942fa6a78fb28`.

No full-image build, database operation, service operation, commit, push or deployment was performed
by the implementation worker. Parent review/build/deployment and an in-game stability check remain.
