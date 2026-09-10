#!/usr/bin/env python3
"""Offline UBRS baseline versus the configured ten-player MC/BWL template stats."""
import csv
import math
from pathlib import Path

from raid_size_ratios import calculate

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "Documents/data/ubrs-raid-boss-template-snapshot-20260910.tsv"
HP_FACTOR = 10 / 40
DAMAGE_FACTOR = HP_FACTOR ** 0.6
GROUPS = {
    "UBRS — unchanged": [9816, 10339, 10429, 10430, 10363],
    "UBRS — additional/rare bosses": [10264, 10899, 10509],
    "Molten Core — scaled to ten": [12118, 11982, 12259, 12057, 12056, 12264, 12098, 11988, 12018, 11502],
    "Blackwing Lair — scaled to ten": [12435, 13020, 12017, 11983, 14601, 11981, 14020, 11583],
}
NOTES = {
    10339: "Gyth's body only; Rend and the arena waves are separate.",
    10363: "Unbuffed melee excludes recurring Rage, Pierce Armor, Cleave, Conflagration and guards.",
    10429: "Rend's body only; excludes Gyth/waves and special attacks.",
    12056: "Template melee school is Fire; equal raw damage does not imply equal post-mitigation damage.",
    12018: "Listed HP is his body maximum, not a kill requirement: the encounter is won through his adds.",
    12435: "Body HP for phase two; eggs/adds and phase-one mechanics are excluded.",
    13020: "Listed HP is the 30% starting pool (249,825), not scaled maximum (832,750). Essence of the Red excluded.",
}


def load():
    with DATA.open() as file:
        return {int(row["entry"]): row for row in csv.DictReader(file, delimiter="\t")}


def scaled(row, raid):
    result = calculate(row)
    # Positive std::round, as RaidScalingMgr::ScaleHealth uses; not Python's ties-to-even round.
    result["max_hp"] = max(1, math.floor(result["hp"] * (HP_FACTOR if raid else 1) + 0.5))
    result["hp"] = result["max_hp"]
    if int(row["entry"]) == 13020:
        result["hp"] = result["max_hp"] * 30 // 100
    for key in ("low", "high", "hit", "dps"):
        result[key] *= DAMAGE_FACTOR if raid else 1
    return result


def main():
    rows = load()
    baseline = scaled(rows[10363], False)
    print("# UBRS versus ten-player MC/BWL: computed boss stats\n")
    print("Snapshot: 2026-09-10. Read-only world-template query; no server changes.\n")
    print("UBRS is not affected by the raid-scaling module. MC/BWL use 25% HP and "
          f"{DAMAGE_FACTOR:.6%} damage. Global creature HP/damage rates were verified as 1.\n")
    print("Damage is approximate **unbuffed main-hand auto-attack damage against a neutral target**, "
          "not spell-inclusive or observed combat DPS. Excludes armor, avoidance, block, crits, "
          "auras, enrage, casts and adds. Values are rounded; actual delivered hits undergo integer rounding.\n")
    print("Ratios use General Drakkisath's body as a clearly named reference, not an average of all UBRS encounters.\n")
    for group, ids in GROUPS.items():
        raid = not group.startswith("UBRS")
        print(f"## {group}\n")
        print("| Boss | HP* | Raw swing | Seconds | Raw melee DPS | HP / Drakk | DPS / Drakk |")
        print("|---|---:|---:|---:|---:|---:|---:|")
        for entry in ids:
            row = rows[entry]
            s = scaled(row, raid)
            marker = "†" if entry in NOTES else ""
            print(f"| {row['name']}{marker} | {s['hp']:,} | {s['low']:,.0f}–{s['high']:,.0f} | "
                  f"{s['seconds']:g} | {s['dps']:,.1f} | {s['hp']/baseline['hp']:.2f}x | "
                  f"{s['dps']/baseline['dps']:.2f}x |")
        print()
    print("## Interpretation and limits\n")
    print("- *HP means scaled maximum except Vaelastrasz, where the scripted starting health is shown.")
    for entry, text in NOTES.items():
        print(f"- {rows[entry]['name']}: {text}")
    print("- Ragnaros has slower 2.8-second baseline swings; his lower white-swing DPS is not his total "
          "damage output or an encounter-difficulty ranking.")
    print("- Several early MC bosses have virtually the same body HP as Drakkisath but stronger basic melee. "
          "MC body melee spans approximately 1.65–2.53x Drakk's unbuffed baseline; BWL spans 2.78–4.43x.")
    print("- BWL body/start HP spans approximately 1.39–6.69x Drakk's HP. Nefarian is 6.69x HP and 4.43x "
          "baseline melee DPS; Chromaggus is 5.66x and 3.80x respectively.")
    print("- Rend has substantially stronger baseline melee than Drakk. Compared with Rend instead, "
          "MC is approximately 0.89–1.36x melee DPS and BWL 1.50–2.39x. Thus 'UBRS damage' is not one number.")
    print("- These results describe the local 3.3.5a implementation, not verified historical Vanilla tuning. "
          "They cannot establish total healing demand, spell burst danger or whether a damage increase is appropriate.")
    print("\n## Reproduction and source contracts\n")
    print("- Input: `Documents/data/ubrs-raid-boss-template-snapshot-20260910.tsv`.")
    print("- Run: `python3 scripts/analysis/ubrs_raid_boss_ratios.py`.")
    print("- Tests: `python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v`.")
    print("- Base formula reused from `scripts/analysis/raid_size_ratios.py`; see "
          "[earlier study](../raid-size-ratio-study.md) for core formula references and caveats.")
    print("- `modules/mod-raid-scaling/src/RaidScalingMgr.cpp`: map 229 absent from original-size registry; "
          "MC 409/BWL 469 have original size 40; `MakeSettings` and `ScaleHealth` give the applied factors.")
    print("- `modules/mod-raid-scaling/src/RaidScalingLoader.cpp`: damage factor applied through "
          "`RaidScalingUnitScript::DealDamage`. Neutral-target figures are not a prediction of hit values "
          "after a particular player's armor/block.")
    print("- Core `boss_vaelastrasz.cpp::Reset`: sets 30% of max HP. `boss_drakkisath.cpp`: Rage scheduled "
          "one second after engagement and every 35 seconds thereafter; special attacks are not captured "
          "by his low unbuffed white-swing figure.")
    print("- Full boss damage requires encounter-specific spell/rotation analysis or live logs. "
          "This comparison does not pretend melee alone measures it.")


if __name__ == "__main__":
    main()
