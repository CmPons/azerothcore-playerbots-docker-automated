"""Offline production C++ / API doubles; no server, SQL changes, or live pulls."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
AQ = CORE / "modules/mod-playerbots/src/Ai/Raid/Aq40"
PATCH = ROOT / "patches/0031-playerbot-cthun-positioning.patch"


def run(args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


class CthunPositioningTests(unittest.TestCase):
    def test_production_planner_actions_and_multiplier(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            fixture = ROOT / "scripts/tests/fixtures/twins/Framework.h"
            for name in "MovementActions Multiplier Position Creature FollowActions GenericActions Group InstanceScript PathGenerator Playerbots ReachTargetActions".split():
                (temp / f"{name}.h").write_text(f'#include "{fixture}"\n')
            binary = temp / "test"
            run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-D_GLIBCXX_ASSERTIONS", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                 f"-I{temp}", f"-I{AQ}", str(ROOT / "scripts/tests/cpp/CthunPositioningTest.cpp"),
                 str(AQ / "Aq40Cthun.cpp"), "-o", str(binary)])
            self.assertIn("Cthun production positioning regressions passed", run([str(binary)]))

    def test_incremental_patch_roundtrip(self):
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(len(names), 4)
        self.assertTrue(all(name.startswith("modules/mod-playerbots/src/Ai/Raid/Aq40/") for name in names))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for name in names:
                p = temp / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes((CORE / name).read_bytes())
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            self.assertNotIn("cthun", (temp / names[-1]).read_text().lower())
            run(["git", "apply", str(PATCH)], cwd=temp)
            for name in names:
                self.assertEqual((temp / name).read_bytes(), (CORE / name).read_bytes())

    def test_registration_and_native_contracts(self):
        code = (AQ / "Aq40Cthun.cpp").read_text()
        self.assertIn('creators["aq40 cthun position"]', (AQ / "Aq40ActionContext.h").read_text())
        strategy = (AQ / "Aq40Strategy.cpp").read_text()
        self.assertIn('NextAction("aq40 cthun position", ACTION_EMERGENCY + 8)', strategy)
        self.assertIn('new CthunMovementMultiplier(botAI)', strategy)
        ai = (CORE / "modules/mod-playerbots/src/Bot/PlayerbotAI.cpp").read_text()
        for state in ("COMBAT", "NON_COMBAT"):
            self.assertIn(f'engines[BOT_STATE_{state}]->addStrategy(strategyName)', ai)
        for forbidden in ("TeleportTo(", "SetHealth(", "SetMaxHealth(", "SetSpeed(", "AddThreat(",
                          "CastSpell(", "SetBossState(", "ChangeStrategy(", "Arinerica", "Beliona"):
            self.assertNotIn(forbidden, code)
        boss = (CORE / "src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_cthun.cpp").read_text()
        self.assertIn("SPELL_RED_COLORATION                        = 22518", boss)
        self.assertIn("DarkGlareTick * float(M_PI) / 35", boss)
        self.assertIn("HasInLine(target, 5.0f)", boss)
        self.assertIn("IsWithinDist2d(who, 90.0f)", boss)
        self.assertIn("path.GetPathType() != PATHFIND_NORMAL", code)
        self.assertIn("attempts > 8", code)
        self.assertIn("IsWaitingForLastMove", code)
        self.assertIn("false, false, true, true, MovementPriority::MOVEMENT_COMBAT, true", code)
        jump = (CORE / "src/server/game/Spells/Spell.cpp").read_text()
        self.assertIn("chainSource->IsWithinDist(*itr, jumpRadius)", jump)
        reach = (CORE / "src/server/game/Entities/Object/Object.cpp").read_text()
        self.assertIn("maxdist += obj->GetObjectSize()", reach)


if __name__ == "__main__":
    unittest.main()
