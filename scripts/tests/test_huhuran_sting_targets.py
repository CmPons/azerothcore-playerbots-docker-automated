"""Offline Huhuran target-filter/API-double and patch regressions; no live actions."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
SOURCE = Path("src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_huhuran.cpp")
PATCH = ROOT / "patches/0024-core-huhuran-ten-player-sting.patch"
CAP = """// Fixed ten-player tuning; independent of attendance and raid-scaling overrides.
constexpr uint32 WYVERN_STING_MAX_TARGETS = 3;

"""
OLD_CAST = "me->CastCustomSpell(SPELL_WYVERN_STING, SPELLVALUE_MAX_TARGETS, 10, me, true);"
NEW_CAST = """me->CastCustomSpell(SPELL_WYVERN_STING, SPELLVALUE_MAX_TARGETS,
                        WYVERN_STING_MAX_TARGETS, me, true);"""
OLD_LIMIT = "        uint32 const maxTargets = GetSpellInfo()->MaxAffectedTargets;"
NEW_LIMIT = """        // Trim Sting to its local cap here as well, preserving closest-target selection.
        // Otherwise the core would randomly reduce the DBC-selected ten to three afterwards.
        uint32 const maxTargets = GetSpellInfo()->Id == SPELL_WYVERN_STING
            ? WYVERN_STING_MAX_TARGETS : GetSpellInfo()->MaxAffectedTargets;"""


def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run(args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


def reverse_patch(temp, current):
    path = temp / SOURCE
    path.parent.mkdir(parents=True)
    path.write_text(current)
    run(["git", "apply", "--reverse", "--check", str(PATCH)], cwd=temp)
    run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
    return path.read_text()


class HuhuranStingTargetTests(unittest.TestCase):
    def test_production_filter_and_comparator(self):
        current = (CORE / SOURCE).read_text()
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            old = reverse_patch(temp, current)
            object_header = (CORE / "src/server/game/Entities/Object/Object.h").read_text()
            harness = (ROOT / "scripts/tests/cpp/HuhuranStingTargetsTest.cpp").read_text()
            values = {
                "SPELLS_AND_CAP": block(current, "enum Spells") + ";\n" + re.search(
                    r"constexpr uint32 WYVERN_STING_MAX_TARGETS = \d+;", current).group(),
                "COMPARATOR": block(object_header, "class ObjectDistanceOrderPred") + ";",
                "FILTER": block(current, "void FilterTargets("),
                "LEGACY_FILTER": block(old, "void FilterTargets(").replace(
                    "FilterTargets", "LegacyFilterTargets", 1),
            }
            for name, value in values.items():
                harness = harness.replace(f"/* {name} */", value)
            source = temp / "test.cpp"
            source.write_text(harness)
            binary = temp / "test"
            run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 str(source), "-o", str(binary)])
            self.assertIn("Production Huhuran target-filter regressions passed", run([str(binary)]))

    def test_cast_and_filter_share_three_target_cap(self):
        current = (CORE / SOURCE).read_text()
        self.assertIn(CAP, current)
        self.assertIn(NEW_CAST, current)
        self.assertIn(NEW_LIMIT, current)
        self.assertIn("events.Repeat(25s, 43s);", current)
        self.assertIn("TARGET_UNIT_SRC_AREA_ENEMY", current)
        self.assertEqual(int(re.search(r"WYVERN_STING_MAX_TARGETS = (\d+)", current)[1]), 3)

    def test_core_area_filter_precedes_random_cast_cap(self):
        source = (CORE / "src/server/game/Spells/Spell.cpp").read_text()
        area = block(source, "void Spell::SelectImplicitAreaTargets(")
        self.assertLess(area.index("CallScriptObjectAreaTargetSelectHandlers"),
                        area.index("Acore::Containers::RandomResize"))
        self.assertIn("m_spellValue->MaxAffectedTargets", area)

    def test_patch_roundtrip_and_damage_scope(self):
        current = (CORE / SOURCE).read_text()
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(names, [str(SOURCE)])
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            original = reverse_patch(temp, current)
            restored = current.replace(CAP, "").replace(NEW_CAST, OLD_CAST).replace(NEW_LIMIT, OLD_LIMIT)
            # No changes to damage, radius, berserk, poison, dispel backlash or other AI behavior.
            self.assertEqual(restored, original)
            run(["git", "apply", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", str(PATCH)], cwd=temp)
            self.assertEqual((temp / SOURCE).read_bytes(), (CORE / SOURCE).read_bytes())


if __name__ == "__main__":
    unittest.main()
