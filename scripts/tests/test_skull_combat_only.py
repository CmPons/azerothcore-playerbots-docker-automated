"""Offline regression checks; no worldserver configuration/build or live changes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"
PATCH = ROOT / "patches/0020-playerbot-skull-combat-only.patch"
METHODS = {
    "src/Ai/Base/Value/RtiTargetValue.cpp": [
        "int32 RtiTargetValue::GetRtiIndex(", "Unit* RtiTargetValue::Calculate("],
    "src/Ai/Base/Value/AttackersValue.cpp": [
        "GuidVector AttackersValue::Get(", "GuidVector AttackersValue::Calculate("],
    "src/Ai/Base/Value/TargetValue.cpp": ["bool FindTargetStrategy::IsHighPriority("],
    "src/Ai/Base/Actions/ChooseTargetActions.cpp": [
        "bool AttackRtiTargetAction::Execute(", "bool AttackRtiTargetAction::isUseful("],
    "src/Ai/Base/Actions/PullActions.cpp": ["Unit* PullRtiTargetAction::GetPullTarget("],
}


def method(text, signature):
    """Extract complete methods, including their original gates and fallback bodies."""
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    for end in range(opening, len(text)):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if not depth:
                return text[start:end + 1]
    raise AssertionError(f"Unclosed method: {signature}")


@unittest.skipUnless(BOT.is_dir(), "Optional playerbots checkout absent")
class SkullCombatOnlyTests(unittest.TestCase):
    def test_production_methods_with_game_doubles(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        methods = []
        for path, signatures in METHODS.items():
            source = (BOT / path).read_text()
            methods.extend(method(source, signature) for signature in signatures)
        harness = (ROOT / "scripts/tests/cpp/SkullCombatOnlyTest.cpp").read_text()
        harness = harness.replace("// PRODUCTION_METHODS", "\n\n".join(methods))
        with tempfile.TemporaryDirectory(prefix="skull-combat-test-") as tmp:
            source = Path(tmp) / "test.cpp"
            binary = Path(tmp) / "test"
            source.write_text(harness)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_shared_selectors_and_no_new_cached_gate(self):
        for path, signature in (
            ("DpsTargetValue.cpp", "Unit* DpsTargetValue::Calculate("),
            ("DpsTargetValue.cpp", "Unit* DpsAoeTargetValue::Calculate("),
            ("TankTargetValue.cpp", "Unit* TankTargetValue::Calculate("),
        ):
            body = method((BOT / "src/Ai/Base/Value" / path).read_text(), signature)
            self.assertIn("RtiTargetValue::Calculate()", body)
        header = (BOT / "src/Ai/Base/Value/TargetValue.h").read_text()
        self.assertIn('int checkInterval = 1)', header)
        source = (BOT / "src/Bot/Engine/Value/Value.cpp").read_text()
        body = method(source, "Unit* UnitCalculatedValue::Get(")
        self.assertIn("checkInterval < 2", body)
        self.assertIn("value = Calculate();", body.split("    else", 1)[0])
        self.assertIn("return value;", body)
        attackers = (BOT / "src/Ai/Base/Value/AttackersValue.h").read_text()
        self.assertIn('ObjectGuidListCalculatedValue(botAI, "attackers", 1 * 1000)', attackers)
        self.assertIn('GuidVector Get() override;', attackers)
        self.assertIn('bool wasInCombat = false;', attackers)

    def test_patch_round_trip_and_scope(self):
        patch = PATCH.read_text()
        touched = [line.removeprefix("+++ b/modules/mod-playerbots/")
                   for line in patch.splitlines() if line.startswith("+++ b/")]
        expected = set(METHODS) - {"src/Ai/Base/Actions/PullActions.cpp"}
        expected.add("src/Ai/Base/Value/AttackersValue.h")
        self.assertEqual(set(touched), expected)
        self.assertNotIn("PlayerbotAI.cpp", patch)
        with tempfile.TemporaryDirectory(prefix="skull-patch-test-") as tmp:
            subprocess.run(["git", "init", "-q", tmp], check=True)
            before = {}
            for path in touched:
                dest = Path(tmp) / "modules/mod-playerbots" / path
                dest.parent.mkdir(parents=True, exist_ok=True)
                before[path] = (BOT / path).read_bytes()
                dest.write_bytes(before[path])
            for args in (("--reverse", "--check"), ("--reverse",), ("--check",), (),
                         ("--reverse", "--check")):
                subprocess.run(["git", "-C", tmp, "apply", *args, str(PATCH)], check=True)
            for path in touched:
                self.assertEqual((Path(tmp) / "modules/mod-playerbots" / path).read_bytes(), before[path])


if __name__ == "__main__":
    unittest.main()
