"""Offline checks for the opt-in AH stock profile; no SQL or server commands."""
import csv
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest

from scripts.ahbot_stock import CATALOG, KEY, merge_tbc_gem_multipliers, tbc_cut_gem_ids


class AuctionHouseStockTest(unittest.TestCase):
    def test_curated_catalog_covers_colors_and_metas(self):
        with CATALOG.open() as source:
            rows = list(csv.DictReader(source, delimiter="\t"))
        ids = tbc_cut_gem_ids()
        self.assertEqual(len(ids), 127)
        self.assertEqual({int(row["subclass"]) for row in rows}, {0, 1, 2, 3, 4, 5, 6, 8})
        self.assertEqual({int(row["Quality"]) for row in rows}, {2, 3, 4})
        self.assertTrue({23095, 24027, 24033, 24054, 32193, 32409, 34220}.issubset(ids))
        # Raw gems, Wrath cuts, and unused/debug gems must not get this boost.
        self.assertTrue(set(ids).isdisjoint({23077, 23436, 32227, 39900, 39996, 34142, 34143, 34627}))
        self.assertTrue(all(item <= 35761 for item in ids))

    def test_merge_preserves_unrelated_entries_and_replaces_profile_entries(self):
        config = f'{KEY} = "2589:10,23436:10,118:5,24027:2,39996:3"\n'
        result = merge_tbc_gem_multipliers(config, 5)
        entries = dict(token.split(":") for token in result.split(","))
        self.assertEqual(entries["2589"], "10")
        self.assertEqual(entries["23436"], "10")
        self.assertEqual(entries["118"], "5")
        self.assertEqual(entries["39996"], "3")
        self.assertEqual(entries["24027"], "5")
        self.assertEqual(len(entries), 131)
        self.assertEqual(result, merge_tbc_gem_multipliers(f"{KEY} = {result}\n", 5))

    def test_zero_removes_only_managed_entries(self):
        base = "2589:10,23436:10,118:5"
        applied = merge_tbc_gem_multipliers(f"{KEY} = {base}\n", 5)
        self.assertEqual(merge_tbc_gem_multipliers(f"{KEY} = {applied}\n", 0), base)

    def test_missing_or_empty_setting(self):
        self.assertEqual(merge_tbc_gem_multipliers("", 0), "")
        self.assertEqual(merge_tbc_gem_multipliers(f"# {KEY} = bad\n{KEY} = \n", 0), "")
        self.assertEqual(len(merge_tbc_gem_multipliers("", 5).split(",")), 127)

    def test_invalid_input_is_rejected(self):
        for value in ("2589:0", "0:5", "2589:10,2589:5", "broken", "2589:2.5", "2589:10,"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                merge_tbc_gem_multipliers(f"{KEY} = {value}\n", 5)
        with self.assertRaises(ValueError):
            merge_tbc_gem_multipliers(f"{KEY} = \n{KEY} = \n", 5)
        for multiplier in (-1, 21):
            with self.assertRaises(ValueError):
                merge_tbc_gem_multipliers("", multiplier)

    def test_cli_does_not_modify_config(self):
        with tempfile.TemporaryDirectory() as temp:
            config = Path(temp) / "ah.conf"
            original = f"{KEY} = 2589:10\nOther.Setting = unchanged\n"
            config.write_text(original)
            helper = Path(__file__).resolve().parents[1] / "ahbot_stock.py"
            result = subprocess.run([sys.executable, str(helper), "--config", str(config),
                                     "--tbc-cut-gem-multiplier", "5"],
                                    text=True, capture_output=True, check=True)
            self.assertIn("24027:5", result.stdout)
            self.assertEqual(config.read_text(), original)


if __name__ == "__main__":
    unittest.main()
