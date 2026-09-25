"""Offline production-body regression: a selected raid boss is not an automatic pull order."""
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"
BASELINE = "75ac48b5c824217d2e683b2b990308c2aad3ba0b"
HELPER = "src/Ai/Base/Util/RaidThreatUtils.cpp"


class RaidBossPullFocusTests(unittest.TestCase):
    def run_fixture(self, old=False):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        source = (BOT / HELPER).read_text()
        if old:
            result = subprocess.run(["git", "-C", str(BOT), "show", BASELINE + ":" + HELPER],
                                    capture_output=True, text=True)
            if result.returncode:
                self.skipTest("Historical baseline unavailable")
            source = result.stdout
        fixture = (ROOT / "scripts/tests/cpp/RaidBossPullFocusTest.cpp").read_text()
        fixture = fixture.replace("// PRODUCTION_FOCUS", method(source, "Unit* GetMainTankTarget("))
        fixture = fixture.replace("// PRODUCTION_SELECTOR", method(
            (BOT / "src/Ai/Base/Value/DpsTargetValue.cpp").read_text(), "Unit* DpsTargetValue::Calculate("))
        with tempfile.TemporaryDirectory(prefix="raid-pull-focus-") as tmp:
            cpp = Path(tmp) / "test.cpp"
            binary = Path(tmp) / "test"
            cpp.write_text(fixture)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(cpp), "-o", str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10,
                                    preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
            if old:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("!selector.Calculate()", result.stderr)
            else:
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                print(result.stdout.strip())

    def test_selected_boss_does_not_initiate_pull(self):
        self.run_fixture()

    def test_previous_focus_reproduces_selection_only_pull(self):
        self.run_fixture(old=True)

    def test_explicit_attack_and_pull_commands_unchanged(self):
        for path, signatures in {
            "src/Ai/Base/Actions/AttackAction.cpp": ["bool AttackMyTargetAction::Execute("],
            "src/Ai/Base/Actions/PullActions.cpp": ["bool PullRequestAction::Execute(",
                                                   "bool PullAction::Execute("],
            "src/Ai/Base/Actions/CastCustomSpellAction.cpp": ["bool CastCustomSpellAction::Execute("],
        }.items():
            before = subprocess.check_output(["git", "-C", str(BOT), "show", BASELINE + ":" + path], text=True)
            after = (BOT / path).read_text()
            for signature in signatures:
                self.assertEqual(method(before, signature), method(after, signature))


if __name__ == "__main__":
    unittest.main()
