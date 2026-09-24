"""Offline production-body tests: no server build, config reload or live mutation."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"


def body(path):
    return re.sub(r'^#include .*$', '', (BOT / path).read_text(), flags=re.MULTILINE)


class TankTargetProtectionTests(unittest.TestCase):
    def test_production_selection_and_taunt_policy(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        helper = BOT / "src/Ai/Base/Util/TankTargetProtection.cpp"
        code = []
        if helper.exists():
            code.extend([
                "#define HAVE_TAUNT_GUARD",
                body("src/Ai/Base/Util/TankTargetProtection.h"),
                body("src/Ai/Base/Util/TankTargetProtection.cpp"),
                body("src/Script/PlayerbotsTankTaunt.cpp"),
            ])
        code.extend([
            method((BOT / "src/Bot/PlayerbotAI.cpp").read_text(), "bool PlayerbotAI::HasAggro("),
            method((BOT / "src/Ai/Base/Value/AttackerCountValues.cpp").read_text(),
                   "bool HasAggroValue::Calculate("),
            body("src/Ai/Base/Value/TankTargetValue.cpp"),
            method((BOT / "src/Ai/Base/Trigger/GenericTriggers.cpp").read_text(),
                   "bool TankAssistTrigger::IsActive("),
        ])
        harness = (ROOT / "scripts/tests/cpp/TankTargetProtectionTest.cpp").read_text()
        harness = harness.replace("// PRODUCTION_CODE", "\n\n".join(code))
        with tempfile.TemporaryDirectory(prefix="tank-target-protection-") as tmp:
            source = Path(tmp) / "test.cpp"
            binary = Path(tmp) / "test"
            source.write_text(harness)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

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

    def test_registered_guard_and_production_spec_check(self):
        source = (BOT / "src/Script/Playerbots.cpp").read_text()
        self.assertIn("void AddPlayerbotsTankTauntScripts();", source)
        self.assertIn("    AddPlayerbotsTankTauntScripts();", source)
        helper = (BOT / "src/Ai/Base/Util/TankTargetProtection.cpp").read_text()
        self.assertIn("PlayerbotAI::IsTank(tank, true)", helper)
        for forbidden in ("Database", "IsExplicitMainTank", "GetSubGroup", "prioritized targets"):
            self.assertNotIn(forbidden, helper)
        # Both planning/check and actual preparation hooks are necessary for direct/triggered casts.
        hook = (BOT / "src/Script/PlayerbotsTankTaunt.cpp").read_text()
        self.assertIn("ALLSPELLHOOK_ON_SPELL_CHECK_CAST", hook)
        self.assertIn("ALLSPELLHOOK_CAN_PREPARE", hook)
        self.assertIn("botAI->IsRealPlayer()", hook)
        self.assertIn("caster->GetSession()->IsBot()", hook)


if __name__ == "__main__":
    unittest.main()
