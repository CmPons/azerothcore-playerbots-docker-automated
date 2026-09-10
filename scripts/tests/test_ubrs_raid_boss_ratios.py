import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/analysis/ubrs_raid_boss_ratios.py"
sys.path.insert(0, str(SCRIPT.parent))
SPEC = importlib.util.spec_from_file_location("ubrs_ratios", SCRIPT)
RATIOS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RATIOS)


class UbrsRaidBossRatioTests(unittest.TestCase):
    def test_ubrs_unchanged_and_mc_bwl_scaled(self):
        rows = RATIOS.load()
        drakk = RATIOS.scaled(rows[10363], False)
        self.assertEqual(drakk["hp"], 80925)
        self.assertAlmostEqual(drakk["dps"], 219.6666875, places=3)
        original = RATIOS.calculate(rows[14020])
        chrom = RATIOS.scaled(rows[14020], True)
        self.assertEqual(chrom["hp"], 458013)  # 458012.5 rounds up, unlike Python round.
        self.assertAlmostEqual(chrom["dps"] / original["dps"], (10 / 40) ** 0.6)
        vael = RATIOS.scaled(rows[13020], True)
        self.assertEqual((vael["hp"], vael["max_hp"]), (249825, 832750))
        self.assertEqual(rows[12056]["dmgschool"], "2")  # Geddon: Fire, not physical.

    def test_source_contracts_for_scope_and_special_health(self):
        source = (ROOT / "modules/mod-raid-scaling/src/RaidScalingMgr.cpp").read_text()
        registry = source.split("_originalSizes =", 1)[1].split("};", 1)[0]
        self.assertNotIn("{229,", registry)
        self.assertIn("{409, 40}", registry)
        self.assertIn("{469, 40}", registry)
        self.assertIn("std::round(float(value) * scale)", source)
        vael = (ROOT / "azerothcore-wotlk/src/server/scripts/EasternKingdoms/BlackrockMountain/BlackwingLair/boss_vaelastrasz.cpp").read_text()
        self.assertIn("me->SetHealth(me->CountPctFromMaxHealth(30))", vael)

    def test_report_reproduces_and_covers_snapshot(self):
        rows = RATIOS.load()
        self.assertEqual(set(rows), {entry for ids in RATIOS.GROUPS.values() for entry in ids})
        output = subprocess.check_output([sys.executable, str(SCRIPT)], text=True)
        report = ROOT / "Documents/data/ubrs-raid-boss-comparison-20260910.md"
        self.assertEqual(output, report.read_text())
        self.assertIn("not spell-inclusive or observed combat DPS", output)
        self.assertIn("Rage scheduled one second", output)


if __name__ == "__main__":
    unittest.main()
