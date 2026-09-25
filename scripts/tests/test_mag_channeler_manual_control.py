"""Remove automatic channeler tactics, preserving Magtheridon's boss mechanics."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"
MAG = BOT / "src/Ai/Raid/Mag"
BASELINE = "292669a0b536c3e988a31ec874996d85ee5dc2e2"


class MagChannelerManualControlTests(unittest.TestCase):
    def test_only_boss_triggers_remain(self):
        source = (MAG / "MagStrategy.cpp").read_text()
        triggers = method(source, "void RaidMagtheridonStrategy::InitTriggers(")
        self.assertEqual(re.findall(r'TriggerNode\("([^"]+)"', triggers), [
            "magtheridon boss engaged by main tank",
            "magtheridon boss engaged by ranged",
            "magtheridon standing in debris",
            "magtheridon incoming blast nova",
            "magtheridon need to manage timers and assignments",
            "magtheridon bot is not in combat",
        ])
        self.assertEqual(re.findall(r'NextAction\("([^"]+)"', triggers), [
            "magtheridon main tank position boss",
            "magtheridon spread ranged",
            "magtheridon move out of debris",
            "magtheridon use manticron cube",
            "magtheridon manage timers and assignments",
            "magtheridon erase timers and trackers",
        ])

    def test_boss_multipliers_remain(self):
        source = (MAG / "MagStrategy.cpp").read_text()
        multipliers = method(source, "void RaidMagtheridonStrategy::InitMultipliers(")
        self.assertEqual(re.findall(r'new (\w+)\(botAI\)', multipliers), [
            "MagtheridonUseManticronCubeMultiplier",
            "MagtheridonWaitToAttackMultiplier",
            "MagtheridonControlTankActionsMultiplier",
            "MagtheridonDebrisDangerMultiplier",
        ])

    def run_multiplier(self, source):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        body = method(source, "float MagtheridonControlTankActionsMultiplier::GetValue(")
        fixture = (ROOT / "scripts/tests/cpp/MagChannelerControlTest.cpp").read_text()
        with tempfile.TemporaryDirectory(prefix="mag-channeler-test-") as temp:
            cpp = Path(temp) / "test.cpp"
            binary = Path(temp) / "test"
            cpp.write_text(fixture.replace("// PRODUCTION_METHOD", body))
            compiled = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(binary)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            return subprocess.run([str(binary)], capture_output=True, text=True)

    def test_channeler_freedom_and_boss_phase_behavior(self):
        result = self.run_multiplier((MAG / "MagMultipliers.cpp").read_text())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1728 tank multiplier checks passed", result.stdout)

    def test_regression_rejects_original_blocker(self):
        old = subprocess.run([
            "git", "-C", str(BOT), "show",
            BASELINE + ":src/Ai/Raid/Mag/MagMultipliers.cpp",
        ], capture_output=True, text=True)
        self.assertEqual(old.returncode, 0, old.stderr)
        result = self.run_multiplier(old.stdout)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Tank multiplier mismatch:", result.stderr)
        self.assertIn("active=0", result.stderr)


if __name__ == "__main__":
    unittest.main()
