"""Offline regression of the production Leotheras AI; no server or database writes."""
from pathlib import Path
import os
import resource
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
SOURCE = Path("src/server/scripts/Outland/CoilfangReservoir/SerpentShrine/boss_leotheras_the_blind.cpp")
BEFORE = "8b8b7bcf8615c62b4aedaa00c33713a0c1f6df0e"


def boss_source(text):
    return text[text.index("enum Talk"):text.index("struct npc_inner_demon")]


class LeotherasResetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="leotheras-reset-")
        cls.directory = Path(cls.temp.name)
        cls.fixture = (ROOT / "scripts/tests/cpp/LeotherasResetTest.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def compile_and_run(self, text, name, sanitizers=False):
        cpp = self.directory / (name + ".cpp")
        binary = self.directory / name
        cpp.write_text(self.fixture.replace("// PRODUCTION_BOSS", boss_source(text)))
        flags = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-g"]
        if sanitizers:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
        subprocess.run(["g++", *flags, str(cpp), "-o", str(binary)], check=True,
                       capture_output=True, text=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")
        return subprocess.run([str(binary)], capture_output=True, text=True, env=env,
                              preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))

    def test_production_lifecycle(self):
        result = self.compile_and_run((CORE / SOURCE).read_text(), "current")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sanitized_lifecycle(self):
        result = self.compile_and_run((CORE / SOURCE).read_text(), "sanitized", True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_pre_fix_rejected(self):
        old = subprocess.check_output(["git", "-C", str(CORE), "show", f"{BEFORE}:{SOURCE}"], text=True)
        result = self.compile_and_run(old, "old")
        self.assertNotEqual(result.returncode, 0, "Pre-fix AI unexpectedly passed the lifecycle regression")
        self.assertIn("Assertion", result.stderr)

    def test_missing_reengagement_rejected(self):
        text = (CORE / SOURCE).read_text()
        start = "        if (SpellbindersDefeated())\n            StartEncounter();"
        self.assertEqual(text.count(start), 1)
        result = self.compile_and_run(text.replace(start, "        // Mutant: no restart on engagement."),
                                      "no-reengagement")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Assertion", result.stderr)

    def test_stale_start_latch_rejected(self):
        text = (CORE / SOURCE).read_text()
        reset = "        _combatStarted = false;"
        self.assertEqual(text.count(reset), 1)
        result = self.compile_and_run(text.replace(reset, "        // Mutant: stale attempt latch."), "stale-latch")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Assertion", result.stderr)

    def test_no_formation_or_timer_tuning(self):
        text = (CORE / SOURCE).read_text()
        self.assertNotIn("Respawn(", text)
        self.assertNotIn("RespawnFormation(", text)
        self.assertIn("scheduler.Schedule(10min", text)
        self.assertIn("scheduler.Schedule(25050ms, 32550ms, GROUP_COMBAT", text)
        self.assertIn("Schedule(60350ms, GROUP_DEMON", text)
        self.assertIn("ScheduleHealthCheckEvent(15", text)


if __name__ == "__main__":
    unittest.main()
