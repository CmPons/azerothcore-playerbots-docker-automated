"""Compile production eligibility/usage/vote methods against offline state doubles."""
from pathlib import Path
import os
import re
import resource
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
PB = CORE / "modules/mod-playerbots"
BEFORE = "8275e8f8f9998136047ee034458bb1e2bc49dd2f"
AI = "src/Bot/PlayerbotAI.cpp"
HEADER = "src/Bot/PlayerbotAI.h"
USAGE = "src/Ai/Base/Value/ItemUsageValue.cpp"
ROLL = "src/Ai/Base/Actions/LootRollAction.cpp"


def block(text, signature, enum=False):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + (";" if enum else "")


def read_pb(path, old=False):
    if old:
        return subprocess.check_output(["git", "-C", str(PB), "show", f"{BEFORE}:{path}"], text=True)
    return (PB / path).read_text()


def fixture(old=False):
    core = (CORE / "src/server/game/Entities/Player/PlayerStorage.cpp").read_text()
    methods = [block(core, "InventoryResult Player::CanUseItem(Item*"),
               block(core, "InventoryResult Player::CanUseItem(ItemTemplate const*"),
               block(read_pb(AI, old), "InventoryResult PlayerbotAI::CanEquipItem("),
               block(read_pb(USAGE, old), "ItemUsage ItemUsageValue::QueryItemUsageForEquip("),
               block(read_pb(ROLL, old), "bool LootRollAction::Execute("),
               block(read_pb(ROLL, old), "RollVote LootRollAction::CalculateRollVote("),
               block(read_pb(ROLL, old), "bool RollUniqueCheck(")]
    constants = "\n".join([
        block((CORE / "src/server/game/Entities/Item/Item.h").read_text(), "enum InventoryResult :", True),
        block((CORE / "src/server/game/Groups/Group.h").read_text(), "enum RollVote :", True),
        block((PB / "src/Ai/Base/Value/ItemUsageValue.h").read_text(), "enum ItemUsage :", True),
    ])
    values = dict(ITEM_CLASS_ARMOR=4, ITEM_CLASS_WEAPON=2, ITEM_CLASS_CONTAINER=1,
                  ITEM_CLASS_QUIVER=11, ITEM_CLASS_MISC=15, ITEM_CLASS_RECIPE=9,
                  ITEM_SUBCLASS_JUNK=0, ITEM_SUBCLASS_CONTAINER=0,
                  ITEM_SUBCLASS_WEAPON_MISC=14, ITEM_SUBCLASS_WEAPON_AXE2=1,
                  ITEM_SUBCLASS_WEAPON_SWORD2=8, ITEM_SUBCLASS_WEAPON_MACE2=5,
                  ITEM_SUBCLASS_WEAPON_POLEARM=6, ITEM_SUBCLASS_WEAPON_FISHING_POLE=20,
                  ITEM_SUBCLASS_AMMO_POUCH=3, ITEM_SUBCLASS_ARMOR_PLATE=4,
                  ITEM_SUBCLASS_ARMOR_MAIL=3, ITEM_QUALITY_HEIRLOOM=7, ITEM_QUALITY_EPIC=4,
                  ITEM_FLAG_UNIQUE_EQUIPPABLE=524288, ITEM_FLAG2_FACTION_HORDE=1,
                  ITEM_FLAG2_FACTION_ALLIANCE=2, TEAM_HORDE=1, TEAM_ALLIANCE=0,
                  CLASS_PALADIN=2, CLASS_WARRIOR=1, CLASS_SHAMAN=7, CLASS_HUNTER=3,
                  CLASS_CONTEXT_EQUIP_ARMOR_CLASS=0, SKILL_PLATE_MAIL=293, SKILL_MAIL=413,
                  DEFAULT_MAX_LEVEL=80, NULL_SLOT=255, NULL_BAG=255, INVENTORY_SLOT_BAG_0=255,
                  INVENTORY_SLOT_BAG_START=19, INVENTORY_SLOT_BAG_END=23,
                  EQUIPMENT_SLOT_FINGER1=10, EQUIPMENT_SLOT_FINGER2=11,
                  EQUIPMENT_SLOT_TRINKET1=12, EQUIPMENT_SLOT_TRINKET2=13,
                  EQUIPMENT_SLOT_MAINHAND=15, EQUIPMENT_SLOT_OFFHAND=16, EQUIPMENT_SLOT_END=19,
                  INVTYPE_NON_EQUIP=0, INVTYPE_2HWEAPON=17, INVTYPE_FINGER=11,
                  INVTYPE_TRINKET=12, INVTYPE_WEAPON=13, INVTYPE_WEAPONOFFHAND=22,
                  ITEM_FIELD_RANDOM_PROPERTIES_ID=1, ITEM_FIELD_DURABILITY=2,
                  ITEM_FIELD_MAXDURABILITY=3, GROUP_LOOT=3, MASTER_LOOT=2, FREE_FOR_ALL=0,
                  BIND_WHEN_PICKED_UP=1)
    constants += "\n" + "\n".join(f"constexpr uint32 {k} = {v};" for k, v in values.items())
    header = read_pb(HEADER, old)
    declaration = re.search(r"    InventoryResult CanEquipItem\([^;]+;", header).group(0)
    text = (ROOT / "scripts/tests/cpp/DeadBotLootTest.cpp").read_text()
    return (text.replace("// CONSTANTS", constants).replace("// EQUIP_DECLARATION", declaration)
            .replace("// PRODUCTION_METHODS", "\n\n".join(methods)))


class DeadBotLootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="dead-bot-loot-")
        cls.directory = Path(cls.temp.name)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_fixture(self, source, name, sanitize=False):
        cpp = self.directory / (name + ".cpp")
        binary = self.directory / name
        cpp.write_text(source)
        flags = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-implicit-fallthrough", "-g"]
        if sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
        result = subprocess.run(["g++", *flags, str(cpp), "-o", str(binary)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return subprocess.run([str(binary)], capture_output=True, text=True,
                              env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"),
                              preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))

    def test_eligibility_and_need_pass(self):
        result = self.run_fixture(fixture(), "current")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sanitizers(self):
        result = self.run_fixture(fixture(), "sanitized", True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_old_code_reproduces_rejection(self):
        result = self.run_fixture(fixture(True), "before")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("vote=0", result.stdout)  # Production roll submitted PASS, not a timeout.
        self.assertIn("Assertion", result.stderr)

    def test_loading_flag_shortcut_is_rejected(self):
        text = fixture()
        expected = "CanEquipItem(NULL_SLOT, dest, pItem, true, true, false)"
        self.assertEqual(text.count(expected), 1)
        result = self.run_fixture(text.replace(expected, "CanEquipItem(NULL_SLOT, dest, pItem, true, false)"),
                                  "loading-shortcut")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Assertion", result.stderr)

    def test_only_valuation_opts_out(self):
        cpp = read_pb(AI)
        self.assertIn("bool checkAlive = true", read_pb(HEADER))
        self.assertIn("OnPlayerCanEquipItem(bot, slot, dest, pItem, swap, not_loading)", cpp)
        self.assertIn("CanUseItem(pItem, not_loading && checkAlive)", cpp)
        self.assertIn("CanEquipItem(NULL_SLOT, dest, pItem, true, true, false)", read_pb(USAGE))
        core = (CORE / "src/server/game/Entities/Player/PlayerStorage.cpp").read_text()
        instance_use = block(core, "InventoryResult Player::CanUseItem(Item*")
        self.assertEqual(instance_use.count("not_loading"), 2)
        self.assertIn("if (!IsAlive() && not_loading)", instance_use)
        actual_equip = block(core, "InventoryResult Player::CanEquipItem(")
        self.assertIn("CanUseItem(pItem, not_loading)", actual_equip)
        self.assertEqual(read_pb(ROLL), read_pb(ROLL, True))


if __name__ == "__main__":
    unittest.main()
