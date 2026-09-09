#!/usr/bin/env python3
"""Offline template-stat comparison. No SQL connection, server calls or config writes."""
import csv
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "Documents/data/raid-size-template-snapshot-20260909.tsv"
COHORTS = {
    "Vanilla-content 5 boss": [1853, 10440, 10813, 11501],
    "MC 40 boss": [11982, 11988, 12057],
    # Use one of the three identically tuned drakes, not three copies of that stat line.
    "BWL 40 boss": [14601, 14020, 11583],
    "Wrath 5 normal boss": [28923, 26861],
    "Wrath 5 heroic boss": [31538, 30788, 31368, 31386],
    "Naxx 10 boss": [15956, 15952, 16028],
    "Naxx 25 boss": [29249, 29278, 29324],
    "ICC 25 normal boss": [37957, 37504],
    "ICC 5 heroic boss": [37613, 37627, 36938],
    "Vanilla-content 5 trash": [10405, 10406, 10407],
    "MC 40 trash": [11658, 11659],
    "BWL 40 trash": [12460, 12463, 12467],
    "Wrath 5 normal trash": [28578, 28579],
    "Wrath 5 heroic trash": [30966, 30967],
    "Naxx 10 trash": [16017, 16029],
    "Naxx 25 trash": [29347, 29356],
}
COMPARISONS = [
    ("MC 40 boss", "Vanilla-content 5 boss"),
    ("BWL 40 boss", "Vanilla-content 5 boss"),
    ("Naxx 25 boss", "Wrath 5 normal boss"),
    ("Naxx 25 boss", "Wrath 5 heroic boss"),
    ("Naxx 10 boss", "Wrath 5 heroic boss"),
    ("ICC 25 normal boss", "ICC 5 heroic boss"),
    ("MC 40 trash", "Vanilla-content 5 trash"),
    ("BWL 40 trash", "Vanilla-content 5 trash"),
    ("Naxx 25 trash", "Wrath 5 heroic trash"),
    ("Naxx 10 trash", "Wrath 5 heroic trash"),
]


def calculate(row):
    expansion = int(row["exp"])
    # CreatureBaseStats::GenerateHealth and Creature::SelectLevel, rank HP rate = 1.
    health = math.ceil(float(row[f"basehp{expansion}"]) * float(row["HealthModifier"]))
    base = float(row[("damage_base", "damage_exp1", "damage_exp2")[expansion]])
    ap_term = float(row["attackpower"]) / 14 * float(row["BaseVariance"])
    multiplier = float(row["DamageModifier"])
    # ObjectMgr replaces zero BaseAttackTime with the standard 2000 ms.
    seconds = (int(row["BaseAttackTime"]) or 2000) / 1000
    # Creature::CalculateMinMaxDamage, neutral modifiers, main hand, no auras.
    low = (base + ap_term) * multiplier * seconds
    high = (1.5 * base + ap_term) * multiplier * seconds
    hit = (low + high) / 2
    return {"hp": health, "hit": hit, "dps": hit / seconds,
            "low": low, "high": high, "seconds": seconds}


def load():
    with DATA.open() as file:
        return {int(row["entry"]): row for row in csv.DictReader(file, delimiter="\t")}


def main():
    rows = load()
    stats = {entry: calculate(row) for entry, row in rows.items()}
    medians = {name: {key: statistics.median(stats[i][key] for i in ids)
                      for key in ("hp", "hit", "dps")} for name, ids in COHORTS.items()}
    print("# Computed unscaled template baselines\n")
    print("Raw main-hand melee only; no armor, avoidance, buffs, abilities, adds or raid scaling.\n")
    print("| Cohort | n | Median HP | Median mean swing | Median raw melee DPS |")
    print("|---|---:|---:|---:|---:|")
    for name, median in medians.items():
        print(f"| {name} | {len(COHORTS[name])} | {median['hp']:,.0f} | {median['hit']:,.1f} | {median['dps']:,.1f} |")
    print("\n## Ratios of cohort medians\n")
    print("| Numerator / denominator | HP | Mean swing | Melee DPS |")
    print("|---|---:|---:|---:|")
    for top, bottom in COMPARISONS:
        ratios = [medians[top][key] / medians[bottom][key] for key in ("hp", "hit", "dps")]
        print(f"| {top} / {bottom} | {ratios[0]:.2f}x | {ratios[1]:.2f}x | {ratios[2]:.2f}x |")
    print("\n## Individual inputs and outputs\n")
    print("| Cohort | Entry | Name | Level used | HP | Raw swing range | Seconds | Raw melee DPS |")
    print("|---|---:|---|---:|---:|---:|---:|---:|")
    for name, ids in COHORTS.items():
        for entry in ids:
            row, value = rows[entry], stats[entry]
            print(f"| {name} | {entry} | {row['name']} | {row['maxlevel']} | {value['hp']:,} | "
                  f"{value['low']:,.0f}–{value['high']:,.0f} | {value['seconds']:g} | {value['dps']:,.1f} |")
    print("\n## Current 10/40 scaling applied to the Vanilla-content ratios\n")
    factor = (10 / 40) ** 0.6
    for top, bottom in COMPARISONS:
        if top.startswith(("MC 40", "BWL 40")):
            hp = medians[top]["hp"] / medians[bottom]["hp"] * 0.25
            dps = medians[top]["dps"] / medians[bottom]["dps"] * factor
            print(f"- {top}: {hp:.2f}x dungeon HP; {dps:.2f}x dungeon raw melee DPS.")
    print("\n## Native Naxx 10/25 matched pairs\n")
    for small, large in ((15956, 29249), (15952, 29278), (16028, 29324)):
        a, b = stats[small], stats[large]
        print(f"- {rows[small]['name']}: 10-player HP {a['hp']/b['hp']:.4%}, "
              f"mean swing {a['hit']/b['hit']:.2%}, melee DPS {a['dps']/b['dps']:.2%} of 25-player.")


if __name__ == "__main__":
    main()
