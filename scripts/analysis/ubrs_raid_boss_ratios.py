#!/usr/bin/env python3
"""Offline UBRS baseline versus the configured ten-player MC/BWL/AQ40 template stats."""
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
    "Temple of Ahn'Qiraj (AQ40) — scaled to ten": [
        15263, 15511, 15543, 15544, 15516, 15510, 15299, 15509, 15275, 15276, 15517, 15589, 15727,
    ],
}
NOTES = {
    10339: "Gyth's body only; Rend and the arena waves are separate.",
    10363: "Unbuffed melee excludes recurring Rage, Pierce Armor, Cleave, Conflagration and guards.",
    10429: "Rend's body only; excludes Gyth/waves and special attacks.",
    12056: "Template melee school is Fire; equal raw damage does not imply equal post-mitigation damage.",
    12018: "Listed HP is his body maximum, not a kill requirement: the encounter is won through his adds.",
    12435: "Body HP for phase two; eggs/adds and phase-one mechanics are excluded.",
    13020: "Listed HP is the 30% starting pool (249,825), not scaled maximum (832,750). Essence of the Red excluded.",
    15263: "Original Skeram body only; summoned copies and their added damage/HP are excluded.",
    15511: "Bug Trio member, not a standalone encounter. All three can attack; survivors heal when consuming a fallen boss.",
    15543: "Bug Trio member; healing and fear are not represented by the body stat line.",
    15544: "Bug Trio member; charge and death-triggered changes are excluded.",
    15516: "Sartura's body only; guards, whirlwind, enrage and movement are excluded.",
    15510: "Fankriss's body only; adds and healing-reduction attacks are excluded.",
    15299: "Viscidus's maximum body HP is not a plain damage budget; frost-hit, shatter and glob phases govern the fight.",
    15509: "Unbuffed Huhuran melee only; poison attacks, frenzy and berserk can change healing pressure substantially.",
    15275: "Melee emperor; shares percentage damage with Vek'lor. Do not add their HP as independent kill pools.",
    15276: "Caster emperor; script suppresses his auto-attacks. Template melee stats are not useful encounter DPS.",
    15517: "Ouro's baseline melee only; Sweep, Sand Blast, submerging and berserk are excluded.",
    15589: "Eye phase HP only. Omit the template melee summary: beams and tentacles dominate the encounter.",
    15727: "Body phase HP only; its UpdateAI does not auto-attack. Carapace/weakness phases and tentacles govern the fight.",
}
# Do not publish a misleading melee-DPS number for caster/phase-driven bosses.
# The Eye inherits a generic melee path; this is an interpretation exclusion,
# unlike Vek'lor and the body, whose scripts omit auto-attacks explicitly.
OMIT_MELEE = {15276, 15589, 15727}


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


def melee_columns(entry, result, baseline):
    if entry in OMIT_MELEE:
        return ("—", "—", "—", "—")
    return (f"{result['low']:,.0f}–{result['high']:,.0f}", f"{result['seconds']:g}",
            f"{result['dps']:,.1f}", f"{result['dps']/baseline['dps']:.2f}x")


def main():
    rows = load()
    baseline = scaled(rows[10363], False)
    print("# UBRS versus ten-player MC/BWL/AQ40: computed boss stats\n")
    print("Snapshot: 2026-09-10. Read-only world-template query; no server changes.\n")
    print("UBRS is not affected by the raid-scaling module. MC/BWL/AQ40 use 25% HP and "
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
            swing, seconds, dps, ratio = melee_columns(entry, s, baseline)
            print(f"| {row['name']}{marker} | {s['hp']:,} | {swing} | "
                  f"{seconds} | {dps} | {s['hp']/baseline['hp']:.2f}x | {ratio} |")
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
    print("- AQ40 does not uniformly exceed Nefarian's baseline melee. Skeram, Sartura and Fankriss sit "
          "near 500–560 DPS; Huhuran matches the BWL drakes at 695; Vek'nilash matches Chromaggus at 834. "
          "Ouro stands out at 1,315 DPS, about 1.35x Nefarian, with 3,053–4,047 raw swings.")
    print("- The Bug Trio sums to roughly 2,051 raw melee DPS if all three continuously attack, before "
          "casts, movement, mitigation or buffs. That is distributed across their victims, not necessarily "
          "one tank. Their initial HP sum is not the full damage budget because survivors can heal.")
    print("- A dash in the AQ40 damage columns is not zero encounter damage: Vek'lor and C'Thun's body "
          "do not use scripted auto-attacks; the Eye's template melee is deliberately omitted because "
          "it is a poor summary of a beam/tentacle encounter. Spell-inclusive DPS is not calculated here.")
    print("- These results describe the local 3.3.5a implementation, not verified historical Vanilla tuning. "
          "They cannot establish total healing demand, spell burst danger or whether a damage increase is appropriate.")
    print("\n## Reproduction and source contracts\n")
    print("- Input: `Documents/data/ubrs-raid-boss-template-snapshot-20260910.tsv`.")
    print("- Run: `python3 scripts/analysis/ubrs_raid_boss_ratios.py`.")
    print("- Tests: `python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v`.")
    print("- Base formula reused from `scripts/analysis/raid_size_ratios.py`; see "
          "[earlier study](../raid-size-ratio-study.md) for core formula references and caveats.")
    print("- `modules/mod-raid-scaling/src/RaidScalingMgr.cpp`: map 229 absent from original-size registry; "
          "MC 409/BWL 469/AQ40 531 have original size 40; `MakeSettings` and `ScaleHealth` give the applied factors.")
    print("- `modules/mod-raid-scaling/src/RaidScalingLoader.cpp`: damage factor applied through "
          "`RaidScalingUnitScript::DealDamage`. Neutral-target figures are not a prediction of hit values "
          "after a particular player's armor/block.")
    print("- Core `boss_vaelastrasz.cpp::Reset`: sets 30% of max HP. `boss_drakkisath.cpp`: Rage scheduled "
          "one second after engagement and every 35 seconds thereafter; special attacks are not captured "
          "by his low unbuffed white-swing figure.")
    print("- Core `TempleOfAhnQiraj/boss_twinemperors.cpp`: `DamageTaken` mirrors damage as a fraction "
          "of each twin's maximum HP; `UpdateAI` only calls melee for the non-Vek'lor twin. "
          "`boss_cthun.cpp::boss_cthun::UpdateAI` updates its scheduler without a melee call; the Eye "
          "inherits `BossAI::UpdateAI`, so the Eye exclusion is not a claim of impossible melee.")
    print("- Core `boss_skeram.cpp::JustSummoned` sets copy health from Skeram; "
          "`boss_bug_trio.cpp::MovementInform` casts Full Heal after consumption; "
          "`boss_viscidus.cpp::DamageTaken` implements the hit-count/phase gates. "
          "Source inspection here explains limitations; it does not validate a live AQ40 encounter.")
    print("- Full boss damage requires encounter-specific spell/rotation analysis or live logs. "
          "This comparison does not pretend melee alone measures it.")


if __name__ == "__main__":
    main()
