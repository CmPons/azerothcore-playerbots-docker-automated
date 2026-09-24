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
            env = {"PATH": os.environ["PATH"], "MODETC": temp, "ROOT": str(ROOT), **settings}
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

    def test_level_seventy_and_cut_gem_profile(self):
        values = self.apply({
            "AHBOT_GUIDS": "2502", "AHBOT_MIN_ITEMS": "10000", "AHBOT_MAX_ITEMS": "10000",
            "AHBOT_ITEMS_PER_CYCLE": "500", "AHBOT_BUY_CANDIDATES": "10",
            "AHBOT_LEVEL_RESTRICT": "true", "AHBOT_MAX_REQUIRED_LEVEL": "70",
            "AHBOT_ITEM_LEVEL_RESTRICT": "true", "AHBOT_MAX_ITEM_LEVEL": "164",
            "AHBOT_TBC_CUT_GEM_MULTIPLIER": "5",
        })
        for house in ("Alliance", "Horde", "Neutral"):
            self.assertEqual(values[f"AuctionHouseBot.{house}.MinItems"], "10000")
            self.assertEqual(values[f"AuctionHouseBot.{house}.MaxItems"], "10000")
        self.assertEqual(values["AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.MaxLevel"], "70")
        self.assertEqual(values["AuctionHouseBot.ListedItemLevelRestrict.MaxItemLevel"], "164")
        multipliers = dict(entry.split(":") for entry in
                           values["AuctionHouseBot.ListProportion.ListMultipliedItemIDs"].split(","))
        self.assertEqual(len(multipliers), 127)
        self.assertEqual(multipliers["24027"], "5")  # Bold Living Ruby
        self.assertEqual(multipliers["32409"], "5")  # Relentless Earthstorm Diamond
        self.assertEqual(values["AuctionHouseBot.Buyer.AcceptablePriceModifier"], "1")

    def test_temporary_armor_profile_with_gems(self):
        values = self.apply({
            "AHBOT_GUIDS": "2502", "AHBOT_MIN_ITEMS": "12000", "AHBOT_MAX_ITEMS": "12000",
            "AHBOT_TBC_CUT_GEM_MULTIPLIER": "5", "AHBOT_LEVEL70_ARMOR_MULTIPLIER": "10",
            "AHBOT_ARMOR_UNCOMMON_WEIGHT": "40", "AHBOT_ARMOR_RARE_WEIGHT": "40",
            "AHBOT_ARMOR_EPIC_WEIGHT": "12",
        })
        for house in ("Alliance", "Horde", "Neutral"):
            self.assertEqual(values[f"AuctionHouseBot.{house}.MinItems"], "12000")
            self.assertEqual(values[f"AuctionHouseBot.{house}.MaxItems"], "12000")
        entries = dict(token.split(":") for token in
                       values["AuctionHouseBot.ListProportion.ListMultipliedItemIDs"].split(","))
        self.assertEqual(entries["24027"], "5")
        self.assertEqual(entries["23517"], "20")
        self.assertEqual(entries["40668"], "20")
        self.assertEqual(len(entries), 297)
        for quality, weight in (("Uncommon", "40"), ("Rare", "40"), ("Epic", "12")):
            self.assertEqual(values[f"AuctionHouseBot.ListProportion.CategoryArmor.Quality{quality}"], weight)
        self.assertEqual(values["AuctionHouseBot.ListProportion.CategoryGem.QualityRare"], "20")
        self.assertEqual(values["AuctionHouseBot.Buyer.AcceptablePriceModifier"], "1")

    def test_armor_off_and_default_weights(self):
        values = self.apply({"AHBOT_GUIDS": "2502", "AHBOT_LEVEL70_ARMOR_MULTIPLIER": "0"})
        self.assertEqual(values["AuctionHouseBot.ListProportion.ListMultipliedItemIDs"], "")
        for quality, weight in (("Uncommon", "20"), ("Rare", "10"), ("Epic", "3")):
            self.assertEqual(values[f"AuctionHouseBot.ListProportion.CategoryArmor.Quality{quality}"], weight)
        with self.assertRaises(subprocess.CalledProcessError):
            self.apply({"AHBOT_GUIDS": "2502", "AHBOT_LEVEL70_ARMOR_MULTIPLIER": "11"})

    def test_blank_profile_does_not_replace_multipliers(self):
        values = self.apply({"AHBOT_GUIDS": "2502", "AHBOT_TBC_CUT_GEM_MULTIPLIER": ""})
        self.assertNotIn("AuctionHouseBot.ListProportion.ListMultipliedItemIDs", values)

    def test_invalid_profile_fails(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.apply({"AHBOT_GUIDS": "2502", "AHBOT_TBC_CUT_GEM_MULTIPLIER": "-1"})


if __name__ == "__main__":
    unittest.main()
