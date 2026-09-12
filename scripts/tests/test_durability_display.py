"""Display-only StatsAction regressions; offline C++ API doubles, no gameplay writes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
SOURCE = Path("modules/mod-playerbots/src/Ai/Base/Actions/StatsAction.cpp")
PATCH = ROOT / "patches/0027-playerbot-durability-display.patch"


def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run(args, cwd=None):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


class DurabilityDisplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        temp = Path(cls.directory.name)
        cls.current = (CORE / SOURCE).read_text()
        harness = (ROOT / "scripts/tests/cpp/DurabilityDisplayTest.cpp").read_text()
        for marker, signature in (("LIST_REPAIR_COST", "void StatsAction::ListRepairCost("),
                                  ("REPAIR_PERCENT", "double StatsAction::RepairPercent(")):
            harness = harness.replace("/* " + marker + " */", block(cls.current, signature))
        cpp = temp / "test.cpp"
        cpp.write_text(harness)
        cls.binary = temp / "test"
        run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=undefined,float-divide-by-zero,float-cast-overflow", "-fno-sanitize-recover=all",
             str(cpp), "-o", str(cls.binary)])

    def test_fully_repaired_empty_and_non_durable(self):
        self.assertIn("passed", run([str(self.binary), "full-empty"]))

    def test_mixed_broken_colors_and_rounding(self):
        self.assertIn("passed", run([str(self.binary), "mixed-broken"]))

    def test_backpack_excluded_but_cost_estimate_preserved(self):
        self.assertIn("passed", run([str(self.binary), "inventory-cost"]))

    def test_patch_roundtrip_changes_only_percentage_reporting(self):
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(names, [str(SOURCE)])
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            p = temp / SOURCE
            p.parent.mkdir(parents=True)
            p.write_text(self.current)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            old = p.read_text()
            signature = "void StatsAction::ListRepairCost("
            self.assertEqual(self.current.replace(block(self.current, signature), block(old, signature)), old)
            self.assertIn("repairPercent /= repairCount;", old)
            self.assertIn("if (repair < 100)", old)
            run(["git", "apply", str(PATCH)], cwd=temp)
            self.assertEqual(p.read_bytes(), (CORE / SOURCE).read_bytes())


if __name__ == "__main__":
    unittest.main()
