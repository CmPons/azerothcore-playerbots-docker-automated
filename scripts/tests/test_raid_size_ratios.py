import importlib.util
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("raid_size_ratios", ROOT / "scripts/analysis/raid_size_ratios.py")
RATIOS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RATIOS)


class RaidSizeRatioTests(unittest.TestCase):
    def test_health_and_public_wotlk_melee_crosschecks(self):
        rows = RATIOS.load()
        loken = RATIOS.calculate(rows[31538])
        self.assertEqual(loken["hp"], 512278)
        self.assertEqual(round(loken["low"]), 5951)
        self.assertEqual(round(loken["high"]), 8200)
        anub = RATIOS.calculate(rows[29249])
        self.assertEqual(anub["hp"], 6763325)
        self.assertEqual(round(anub["low"]), 32840)
        self.assertEqual(round(anub["high"]), 45236)

    def test_attack_speed_changes_hit_not_baseline_dps(self):
        row = RATIOS.load()[31538]
        fast = dict(row, BaseAttackTime="1000")
        slow = dict(row, BaseAttackTime="2000")
        a, b = RATIOS.calculate(fast), RATIOS.calculate(slow)
        self.assertAlmostEqual(a["hit"] * 2, b["hit"])
        self.assertAlmostEqual(a["dps"], b["dps"])
        self.assertEqual(RATIOS.calculate(dict(row, BaseAttackTime="0")), b)

    def test_snapshot_and_report_are_reproducible(self):
        rows = RATIOS.load()
        self.assertEqual(set(rows), {entry for ids in RATIOS.COHORTS.values() for entry in ids})
        result = subprocess.check_output(["python3", str(ROOT / "scripts/analysis/raid_size_ratios.py")], text=True)
        self.assertEqual(result, (ROOT / "Documents/data/raid-size-computed-ratios-20260909.md").read_text())


if __name__ == "__main__":
    unittest.main()
