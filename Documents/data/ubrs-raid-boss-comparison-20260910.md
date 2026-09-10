# UBRS versus ten-player MC/BWL: computed boss stats

Snapshot: 2026-09-10. Read-only world-template query; no server changes.

UBRS is not affected by the raid-scaling module. MC/BWL use 25% HP and 43.527528% damage. Global creature HP/damage rates were verified as 1.

Damage is approximate **unbuffed main-hand auto-attack damage against a neutral target**, not spell-inclusive or observed combat DPS. Excludes armor, avoidance, block, crits, auras, enrage, casts and adds. Values are rounded; actual delivered hits undergo integer rounding.

Ratios use General Drakkisath's body as a clearly named reference, not an average of all UBRS encounters.

## UBRS — unchanged

| Boss | HP* | Raw swing | Seconds | Raw melee DPS | HP / Drakk | DPS / Drakk |
|---|---:|---:|---:|---:|---:|---:|
| Pyroguard Emberseer | 24,416 | 364–483 | 2 | 211.6 | 0.30x | 0.96x |
| Gyth† | 32,370 | 378–501 | 2 | 219.7 | 0.40x | 1.00x |
| Warchief Rend Blackhand† | 32,370 | 842–1,117 | 2.4 | 408.0 | 0.40x | 1.86x |
| The Beast | 80,925 | 378–501 | 2 | 219.7 | 1.00x | 1.00x |
| General Drakkisath† | 80,925 | 378–501 | 2 | 219.7 | 1.00x | 1.00x |

## UBRS — additional/rare bosses

| Boss | HP* | Raw swing | Seconds | Raw melee DPS | HP / Drakk | DPS / Drakk |
|---|---:|---:|---:|---:|---:|---:|
| Solakar Flamewreath | 24,416 | 364–483 | 2 | 211.6 | 0.30x | 0.96x |
| Goraluk Anvilcrack | 12,576 | 371–492 | 2 | 215.7 | 0.16x | 0.98x |
| Jed Runewatcher | 8,883 | 764–1,013 | 2 | 444.1 | 0.11x | 2.02x |

## Molten Core — scaled to ten

| Boss | HP* | Raw swing | Seconds | Raw melee DPS | HP / Drakk | DPS / Drakk |
|---|---:|---:|---:|---:|---:|---:|
| Lucifron | 87,945 | 712–942 | 2 | 413.6 | 1.09x | 1.88x |
| Magmadar | 206,522 | 813–1,077 | 2 | 472.6 | 2.55x | 2.15x |
| Gehennas | 87,945 | 712–942 | 2 | 413.6 | 1.09x | 1.88x |
| Garr | 164,885 | 861–1,141 | 2 | 500.4 | 2.04x | 2.28x |
| Baron Geddon† | 146,564 | 669–887 | 2 | 389.2 | 1.81x | 1.77x |
| Shazzrah | 87,945 | 712–942 | 2 | 413.6 | 1.09x | 1.88x |
| Sulfuron Harbinger | 109,923 | 765–1,014 | 2 | 444.8 | 1.36x | 2.02x |
| Golemagg the Incinerator | 206,522 | 956–1,268 | 2 | 555.9 | 2.55x | 2.53x |
| Majordomo Executus† | 166,550 | 669–887 | 2 | 389.2 | 2.06x | 1.77x |
| Ragnaros | 274,808 | 870–1,154 | 2.8 | 361.4 | 3.40x | 1.65x |

## Blackwing Lair — scaled to ten

| Boss | HP* | Raw swing | Seconds | Raw melee DPS | HP / Drakk | DPS / Drakk |
|---|---:|---:|---:|---:|---:|---:|
| Razorgore the Untamed† | 112,421 | 1,052–1,394 | 2 | 611.5 | 1.39x | 2.78x |
| Vaelastrasz the Corrupt† | 249,825 | 1,673–2,218 | 2 | 972.9 | 3.09x | 4.43x |
| Broodlord Lashlayer | 229,006 | 1,195–1,585 | 2 | 694.9 | 2.83x | 3.16x |
| Firemaw | 249,825 | 1,195–1,585 | 2 | 694.9 | 3.09x | 3.16x |
| Ebonroc | 249,825 | 1,195–1,585 | 2 | 694.9 | 3.09x | 3.16x |
| Flamegor | 249,825 | 1,195–1,585 | 2 | 694.9 | 3.09x | 3.16x |
| Chromaggus | 458,013 | 1,434–1,901 | 2 | 833.9 | 5.66x | 3.80x |
| Nefarian | 541,288 | 1,673–2,218 | 2 | 972.9 | 6.69x | 4.43x |

## Interpretation and limits

- *HP means scaled maximum except Vaelastrasz, where the scripted starting health is shown.
- Gyth: Gyth's body only; Rend and the arena waves are separate.
- General Drakkisath: Unbuffed melee excludes recurring Rage, Pierce Armor, Cleave, Conflagration and guards.
- Warchief Rend Blackhand: Rend's body only; excludes Gyth/waves and special attacks.
- Baron Geddon: Template melee school is Fire; equal raw damage does not imply equal post-mitigation damage.
- Majordomo Executus: Listed HP is his body maximum, not a kill requirement: the encounter is won through his adds.
- Razorgore the Untamed: Body HP for phase two; eggs/adds and phase-one mechanics are excluded.
- Vaelastrasz the Corrupt: Listed HP is the 30% starting pool (249,825), not scaled maximum (832,750). Essence of the Red excluded.
- Ragnaros has slower 2.8-second baseline swings; his lower white-swing DPS is not his total damage output or an encounter-difficulty ranking.
- Several early MC bosses have virtually the same body HP as Drakkisath but stronger basic melee. MC body melee spans approximately 1.65–2.53x Drakk's unbuffed baseline; BWL spans 2.78–4.43x.
- BWL body/start HP spans approximately 1.39–6.69x Drakk's HP. Nefarian is 6.69x HP and 4.43x baseline melee DPS; Chromaggus is 5.66x and 3.80x respectively.
- Rend has substantially stronger baseline melee than Drakk. Compared with Rend instead, MC is approximately 0.89–1.36x melee DPS and BWL 1.50–2.39x. Thus 'UBRS damage' is not one number.
- These results describe the local 3.3.5a implementation, not verified historical Vanilla tuning. They cannot establish total healing demand, spell burst danger or whether a damage increase is appropriate.

## Reproduction and source contracts

- Input: `Documents/data/ubrs-raid-boss-template-snapshot-20260910.tsv`.
- Run: `python3 scripts/analysis/ubrs_raid_boss_ratios.py`.
- Tests: `python3 -m unittest discover -s scripts/tests -p test_ubrs_raid_boss_ratios.py -v`.
- Base formula reused from `scripts/analysis/raid_size_ratios.py`; see [earlier study](../raid-size-ratio-study.md) for core formula references and caveats.
- `modules/mod-raid-scaling/src/RaidScalingMgr.cpp`: map 229 absent from original-size registry; MC 409/BWL 469 have original size 40; `MakeSettings` and `ScaleHealth` give the applied factors.
- `modules/mod-raid-scaling/src/RaidScalingLoader.cpp`: damage factor applied through `RaidScalingUnitScript::DealDamage`. Neutral-target figures are not a prediction of hit values after a particular player's armor/block.
- Core `boss_vaelastrasz.cpp::Reset`: sets 30% of max HP. `boss_drakkisath.cpp`: Rage scheduled one second after engagement and every 35 seconds thereafter; special attacks are not captured by his low unbuffed white-swing figure.
- Full boss damage requires encounter-specific spell/rotation analysis or live logs. This comparison does not pretend melee alone measures it.
