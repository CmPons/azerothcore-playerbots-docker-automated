"""Test only the AH setup block; never run Docker or the full setup script."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AuctionHouseSetupTest(unittest.TestCase):
    def apply(self, settings):
        setup = (ROOT / "setup.sh").read_text()
        block = setup[setup.index("# Auction-house economy (mod-ah-bot-plus)"):
                      setup.index("# Bot chattiness")]
        # Capture setter calls rather than modifying any runtime configuration.
        script = 'set -eu\nset_conf() { printf "%s=%s\\n" "$1" "$2"; }\n' + block
        with tempfile.TemporaryDirectory() as temp:
            (Path(temp) / "mod_ahbot.conf").touch()
            env = {"PATH": os.environ["PATH"], "MODETC": temp, **settings}
            result = subprocess.run(["bash", "-c", script], env=env,
                                    text=True, capture_output=True, check=True)
        return dict(line.split("=", 1) for line in result.stdout.splitlines()
                    if line.startswith("AuctionHouseBot."))

    def test_disabled_without_identity(self):
        values = self.apply({})
        self.assertEqual(values["AuctionHouseBot.EnableSeller"], "false")
        self.assertEqual(values["AuctionHouseBot.Buyer.Enabled"], "false")
        self.assertNotIn("AuctionHouseBot.GUIDs", values)

    def test_defaults_preserve_existing_stock_policy(self):
        values = self.apply({"AHBOT_GUIDS": "123"})
        for house in ("Alliance", "Horde", "Neutral"):
            self.assertEqual(values[f"AuctionHouseBot.{house}.MinItems"], "15000")
            self.assertEqual(values[f"AuctionHouseBot.{house}.MaxItems"], "25000")
        self.assertEqual(values["AuctionHouseBot.ItemsPerCycle"], "150")
        self.assertEqual(values["AuctionHouseBot.ListedItemLevelRestrict.Enabled"], "false")

    def test_level_sixty_profile(self):
        values = self.apply({
            "AHBOT_GUIDS": "2502", "AHBOT_MIN_ITEMS": "5000", "AHBOT_MAX_ITEMS": "5000",
            "AHBOT_ITEMS_PER_CYCLE": "500", "AHBOT_BUY_CANDIDATES": "10",
            "AHBOT_LEVEL_RESTRICT": "true", "AHBOT_MAX_REQUIRED_LEVEL": "60",
            "AHBOT_ITEM_LEVEL_RESTRICT": "true", "AHBOT_MAX_ITEM_LEVEL": "92",
        })
        self.assertEqual(values["AuctionHouseBot.EnableSeller"], "true")
        self.assertEqual(values["AuctionHouseBot.Buyer.Enabled"], "true")
        for house in ("Alliance", "Horde", "Neutral"):
            self.assertEqual(values[f"AuctionHouseBot.{house}.MinItems"], "5000")
            self.assertEqual(values[f"AuctionHouseBot.{house}.MaxItems"], "5000")
        self.assertEqual(values["AuctionHouseBot.ItemsPerCycle"], "500")
        self.assertEqual(values["AuctionHouseBot.Buyer.BuyCandidatesPerBuyCycle"], "10")
        self.assertEqual(values["AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.Enabled"], "true")
        self.assertEqual(values["AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.MaxLevel"], "60")
        self.assertEqual(values["AuctionHouseBot.ListedItemLevelRestrict.Enabled"], "true")
        self.assertEqual(values["AuctionHouseBot.ListedItemLevelRestrict.MaxItemLevel"], "92")


if __name__ == "__main__":
    unittest.main()
