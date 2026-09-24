"""Source-only checks: disable Maulgar tactics without disabling Gruul's tactics."""
from pathlib import Path
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"


class MaulgarManualControlTests(unittest.TestCase):
    def test_maulgar_removed_but_gruul_preserved(self):
        source = (BOT / "src/Ai/Raid/Gruul/GruulStrategy.cpp").read_text()
        triggers = method(source, "void RaidGruulsLairStrategy::InitTriggers(")
        multipliers = method(source, "void RaidGruulsLairStrategy::InitMultipliers(")
        self.assertNotIn('"high king maulgar', triggers)
        self.assertNotIn("new HighKingMaulgar", multipliers)
        self.assertEqual(triggers.count("new TriggerNode("), 3)
        self.assertEqual(multipliers.count("new GruulTheDragonkiller"), 3)
        for action in ("tanks position boss", "spread ranged", "shatter spread"):
            self.assertIn('"gruul the dragonkiller ' + action + '"', triggers)


if __name__ == "__main__":
    unittest.main()
