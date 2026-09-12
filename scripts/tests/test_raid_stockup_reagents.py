"""Offline production stockup/reagent methods with API doubles; never runs live commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("modules/mod-raid-roster/src/RaidRosterCommand.cpp")
CORE = ROOT / "azerothcore-wotlk"


def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run(args):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


class RaidStockupReagentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        temp = Path(cls.directory.name)
        cls.source = (ROOT / SOURCE).read_text()
        factory = (CORE / "modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp").read_text()
        notifiers = (CORE / "src/server/game/Grids/Notifiers/GridNotifiers.h").read_text()
        visit = (CORE / "src/server/game/Grids/Notifiers/GridNotifiersImpl.h").read_text()
        replacements = {
            "FOR_EACH_ITEM": "template <typename Fn>\n" + block(cls.source, "void ForEachBagItem("),
            "FIND_BOT_VENDOR": block(cls.source, "ObjectGuid FindNearbyVendor(Player* bot,"),
            "FIND_PLAYER_VENDOR": block(cls.source, "ObjectGuid FindNearbyVendor(Player* player)"),
            "COUNT_ITEMS": block(cls.source, "uint32 CountRaidStockItems("),
            "INIT_REAGENTS": block(factory, "void PlayerbotFactory::InitReagents()"),
            "HANDLE_STOCKUP": block(cls.source, "bool RaidRosterCommand::HandleStockUp("),
            "SEARCHER": "template<class Check>\n" + block(notifiers, "struct CreatureSearcher\n") + ";",
            "SEARCHER_VISIT": "template<class Check>\n" + block(visit, "void Acore::CreatureSearcher<Check>::Visit("),
        }
        harness = (ROOT / "scripts/tests/cpp/RaidStockupReagentsTest.cpp").read_text()
        for marker, value in replacements.items():
            harness = harness.replace("/* " + marker + " */", value)
        cpp = temp / "test.cpp"
        cpp.write_text(harness)
        cls.binary = temp / "test"
        run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(cls.binary)])

    def test_caller_vendor_reagents_only_idempotence_full_bags_and_misc_counter(self):
        self.assertIn("passed", run([str(self.binary), "reagents"]))

    def test_vendor_distance_phase_flags_and_friendliness(self):
        self.assertIn("passed", run([str(self.binary), "vendor-gate"]))

    def test_existing_bot_vendor_full_stockup_and_sales_preserved(self):
        self.assertIn("passed", run([str(self.binary), "legacy-stockup"]))

    def test_disabled_console_group_guards_and_native_level_targets(self):
        self.assertIn("passed", run([str(self.binary), "guards-levels"]))

    def test_source_sync_and_no_gear_or_repair_shortcuts(self):
        self.assertEqual((ROOT / SOURCE).read_bytes(), (CORE / SOURCE).read_bytes())
        handler = block(self.source, "bool RaidRosterCommand::HandleStockUp(")
        for forbidden in ("Refresh(", "InitEquipment(", "InitTalents", "SyncBotToSpec", "DurabilityRepair",
                          "ResetStrategies", "CharacterDatabase", "SetMoney", "Teleport"):
            self.assertNotIn(forbidden, handler)
        self.assertEqual(handler.count("factory.InitReagents();"), 1)
        self.assertLess(handler.index("SellVendorItems("), handler.index("beforeStock ="))
        self.assertIn("Cell::VisitObjects", self.source)
        self.assertNotIn('"Arinerica"', handler)
        self.assertNotIn('"Meliah"', handler)


if __name__ == "__main__":
    unittest.main()
