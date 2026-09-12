# Playerbot durability display

## Status

Prepared September 12, 2026. The user authorized the display fix and deployment; deployment
verification will be recorded here once complete.

## Display-only change

`StatsAction::ListRepairCost` previously averaged only damaged items, including items in the
backpack. Fully repaired gear left the count at zero, producing a division-by-zero/invalid
integer-conversion path instead of a reliable 100% display.

The percentage now:

- Averages **all equipped items with durability**, including fully repaired items.
- Excludes empty slots, non-durable items (e.g. rings), bags and backpack equipment.
- Displays 100% if there is no repairable equipped gear, without dividing by zero.
- Retains the existing upward rounding and color thresholds. Each eligible item has equal
  weight, rather than weighting by its maximum durability points. As before, rounding can
  display 100% for a fractional average just below 100%.

Examples: full + half-worn = 75%; full + broken = 50%; all durable equipped items broken = 0%.
A broken spare weapon in the backpack cannot make a fully repaired equipped set display 0%.

The parenthesized **repair-cost estimate keeps its previous scope and calculation** (equipment
and main backpack). This change does not repair items, spend money, modify gear stats, change
summon/release repairs, or fix the separate missing vendor-repair confirmation. No boss AI,
Twins coordination, scaling, loot, configuration or database changes.

## Source and tests

- Production: `azerothcore-wotlk/modules/mod-playerbots/src/Ai/Base/Actions/StatsAction.cpp`.
- Canonical patch: `patches/0027-playerbot-durability-display.patch`.
- Tests: `scripts/tests/test_durability_display.py` and `cpp/DurabilityDisplayTest.cpp`.

Four offline tests compile the actual display/percentage methods against API doubles with
undefined-behavior, floating divide-by-zero and float-to-integer overflow sanitizers. Cover
full/empty/non-durable gear, mixed/broken gear, unequal maximum durability, rounding/colors,
backpack exclusion, unchanged cost-estimate visits and read-only rendering. Patch round-trip
checks prove that only the display method changed; the cost and per-item helpers are unchanged.
These tests are not a live bot-command test or a complete server build.

Pre-edit source/status/diff/hash backup:
`backups/durability-display-preparation-20260912-114056/`.
Unrelated module work and root `flake.lock` are preserved.
