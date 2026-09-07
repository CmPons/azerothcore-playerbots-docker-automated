"""Standalone state tests and isolated setup checks; no live server mutations."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-raid-scaling"


class RaidScalingDefaultTests(unittest.TestCase):
    def test_cpp_state_lifecycle_and_concurrency(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A C++20 compiler is required for the standalone state test")
        with tempfile.TemporaryDirectory(prefix="raid-scaling-state-") as tmp:
            binary = str(Path(tmp) / "state-test")
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
                            str(MODULE / "tests/RaidScalingStateTest.cpp"), "-o", binary], check=True)
            subprocess.run([binary], check=True, timeout=15)

    def apply_setup_block(self, initial, target=None):
        setup = (ROOT / "setup.sh").read_text()
        start = setup.index("set_conf () {")
        end = setup.index("\n}\n", start) + 3
        helper = setup[start:end]
        block = setup.split("# BEGIN RAID SCALING DEFAULT\n", 1)[1].split(
            "# END RAID SCALING DEFAULT", 1)[0]
        with tempfile.TemporaryDirectory(prefix="raid-scaling-config-") as tmp:
            config = Path(tmp) / "mod_raid_scaling.conf"
            if initial is not None:
                config.write_text(initial)
            env = os.environ.copy()
            env["MODETC"] = tmp
            env.pop("RAID_SCALING_DEFAULT_PLAYERS", None)
            if target is not None:
                env["RAID_SCALING_DEFAULT_PLAYERS"] = str(target)
            subprocess.run(["bash", "-euc", helper + "\n" + block], env=env, check=True)
            first = config.read_text() if config.exists() else None
            subprocess.run(["bash", "-euc", helper + "\n" + block], env=env, check=True)
            self.assertEqual(first, config.read_text() if config.exists() else None)
            return first

    def test_setup_defaults_to_ten_without_touching_other_settings(self):
        initial = "RaidScaling.Enable = 0\nRaidScaling.DamageExponent = 0.7\n"
        result = self.apply_setup_block(initial)
        self.assertEqual(result, initial + "RaidScaling.DefaultTargetPlayers = 10\n")

    def test_setup_allows_opt_out_and_larger_default(self):
        for target in (0, 20, 40):
            with self.subTest(target=target):
                result = self.apply_setup_block(
                    "RaidScaling.DefaultTargetPlayers = 10\n# keep me\n", target)
                self.assertEqual(result, f"RaidScaling.DefaultTargetPlayers = {target}\n# keep me\n")

    def test_setup_skips_absent_module_config(self):
        self.assertIsNone(self.apply_setup_block(None))

    def test_fresh_setup_syncs_module_and_defaults_agree(self):
        setup = (ROOT / "setup.sh").read_text()
        local_modules = next(line for line in setup.splitlines() if line.startswith("LOCAL_MODULES="))
        self.assertIn('"mod-raid-scaling"', local_modules)
        update_modules = next(line for line in (ROOT / "update.sh").read_text().splitlines()
                              if line.startswith("for lm in "))
        self.assertIn(" mod-raid-scaling ", update_modules)
        self.assertIn("RAID_SCALING_DEFAULT_PLAYERS=10\n", (ROOT / ".env.example").read_text())
        self.assertIn("RaidScaling.DefaultTargetPlayers = 10\n",
                      (MODULE / "conf/mod_raid_scaling.conf.dist").read_text())
        self.assertIn('GetOption<int32>("RaidScaling.DefaultTargetPlayers", 10)',
                      (MODULE / "src/RaidScalingMgr.cpp").read_text())

    def test_entry_and_add_hooks_do_not_reinitialize_scaling(self):
        # Source contracts supplement the executable state test. These are not a
        # substitute for compiling/running the complete module in worldserver.
        loader = (MODULE / "src/RaidScalingLoader.cpp").read_text()
        entry = loader.split("void OnPlayerEnterAll", 1)[1].split("class RaidScalingCreatureScript", 1)[0]
        self.assertNotIn("EnableForMap", entry)
        self.assertNotIn("OnMapCreate", entry)
        self.assertIn("session->IsBot()", entry)
        self.assertIn("scaled for {} players", entry)
        mgr = (MODULE / "src/RaidScalingMgr.cpp").read_text()
        create = mgr.split("void RaidScalingMgr::OnMapCreate", 1)[1].split(
            "void RaidScalingMgr::OnMapDestroy", 1)[0]
        self.assertIn("!_defaultTargetPlayers", create)
        self.assertIn("!map->IsRaid()", create)
        self.assertIn("!map->GetInstanceId()", create)
        self.assertIn("if (!original)", create)
        self.assertIn("_state.Initialize", create)
        self.assertIn("ApplyToMap(map)", create)
        spawn = mgr.split("void RaidScalingMgr::OnCreatureAddWorld", 1)[1].split(
            "void RaidScalingMgr::ApplyToMap", 1)[0]
        self.assertIn("ApplyToCreature(creature)", spawn)
        self.assertNotIn("Initialize", spawn)
        self.assertNotIn("EnableForMap", spawn)

    def test_build_tree_matches_canonical(self):
        build = ROOT / "azerothcore-wotlk/modules/mod-raid-scaling"
        if not build.exists():
            self.skipTest("Optional core checkout absent")
        for folder in ("src", "conf", "tests"):
            for source in (MODULE / folder).iterdir():
                if source.is_file():
                    with self.subTest(path=source.name):
                        self.assertEqual(source.read_bytes(),
                                         (build / folder / source.name).read_bytes())


if __name__ == "__main__":
    unittest.main()
