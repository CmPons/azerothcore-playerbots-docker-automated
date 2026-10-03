"""Offline TK trash-spin regression checks; no server configure/build or live writes."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / "azerothcore-wotlk/modules/mod-playerbots"
BASE = BOT / "src/Ai/Base"
OLD = "cef0162a7202f7a10688e989c0dba5d76828dbb1"


class TrashWhirlwindTests(unittest.TestCase):
    def run_fixture(self, old_detection=False, sanitizer=False):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        header = (BASE / "Util/TrashWhirlwind.h").read_text()
        helper = (BASE / "Util/TrashWhirlwind.cpp").read_text()
        movement = (BASE / "Actions/MovementActions.cpp").read_text()
        detector = movement
        if old_detection:
            detector = subprocess.check_output([
                "git", "-C", str(BOT), "show", OLD + ":src/Ai/Base/Actions/MovementActions.cpp"
            ], text=True)
        bodies = "\n\n".join([
            method(detector, "bool AvoidAoeAction::isUseful("),
            method(detector, "bool AvoidAoeAction::Execute("),
            method(movement, "bool AvoidAoeAction::AvoidTrashWhirlwind("),
        ])
        fixture = (ROOT / "scripts/tests/cpp/TrashWhirlwindTest.cpp").read_text()
        fixture = fixture.replace("// PRODUCTION_HEADER", re.sub(r'^#include ".*"\n', "", header, flags=re.M))
        fixture = fixture.replace("// PRODUCTION_SOURCE", re.sub(r'^#include ".*"\n', "", helper, flags=re.M))
        fixture = fixture.replace("// PRODUCTION_METHODS", bodies)
        with tempfile.TemporaryDirectory(prefix="trash-whirlwind-") as temp:
            cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
            cpp.write_text(fixture)
            flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"] if sanitizer else []
            result = subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", *flags,
                                     str(cpp), "-o", str(binary)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return subprocess.run([str(binary)], text=True, capture_output=True)

    def test_real_helper_and_avoidance_methods(self):
        result = self.run_fixture()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("360-angle checks passed", result.stdout)

    def test_sanitized_helper_and_avoidance(self):
        result = self.run_fixture(sanitizer=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_old_detector_compiles_but_misses_selectable_spin(self):
        result = self.run_fixture(old_detection=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("action.isUseful()", result.stderr)

    def test_automatic_movement_admission_is_wired(self):
        movement = (BASE / "Actions/MovementActions.cpp").read_text()
        move = method(movement, "bool MovementAction::MoveTo(uint32")
        self.assertLess(move.index("TrashWhirlwind::AllowsMove"), move.index("UpdateMovementState();"))
        for signature in ("bool MovementAction::ReachCombatTo(",
                          "bool MovementAction::Follow(Unit* target, float distance, float angle)",
                          "bool MovementAction::ChaseTo("):
            body = method(movement, signature)
            self.assertIn("TrashWhirlwind::AllowsApproach", body)
            self.assertIn("RaidCombat::HasMovementClaim", body)
        gap_closer = method((BASE / "Actions/ReachTargetActions.cpp").read_text(),
                            "bool CastReachTargetSpellAction::isUseful(")
        self.assertIn('TrashWhirlwind::AllowsApproach(botAI, AI_VALUE(Unit*, "current target"), 0.0f)', gap_closer)
        self.assertIn('HasStrategy("stay"', gap_closer)

    def test_existing_hazard_and_geometry_methods_are_unchanged(self):
        current = (BASE / "Actions/MovementActions.cpp").read_text()
        old = subprocess.check_output(["git", "-C", str(BOT), "show",
            OLD + ":src/Ai/Base/Actions/MovementActions.cpp"], text=True)
        for signature in ("bool AvoidAoeAction::AvoidAuraWithDynamicObj(",
                          "bool AvoidAoeAction::AvoidGameObjectWithDamage(",
                          "bool AvoidAoeAction::AvoidUnitWithDamageAura(",
                          "Position MovementAction::BestPositionForMeleeToFlee(",
                          "Position MovementAction::BestPositionForRangedToFlee(",
                          "void MovementAction::DoMovePoint("):
            self.assertEqual(method(current, signature), method(old, signature))


if __name__ == "__main__":
    unittest.main()
