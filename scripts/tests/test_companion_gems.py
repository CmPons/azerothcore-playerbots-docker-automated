"""Production planner/maintenance fixtures; no running server, SQL, or CMake build."""
import csv
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "modules/mod-raid-roster/src"
NATIVE = ROOT / "azerothcore-wotlk"


class CompanionGemTests(unittest.TestCase):
    def compile_and_run(self, name, runtime=False):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            if runtime:
                production = (SOURCE / "CompanionGemMaintenance.cpp").read_text()
                production = re.sub(r'^#include[^\n]*\n', '', production, flags=re.M)
                (output / "CompanionGemMaintenance.production.inc").write_text(production)
            command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O1",
                       "-fsanitize=undefined", "-fno-sanitize-recover=all", "-pthread",
                       "-I", str(SOURCE), "-I", str(output),
                       "-I", str(NATIVE / "src/common/Utilities"),
                       str(ROOT / "scripts/tests/cpp" / (name + ".cpp")), "-o", str(output / name)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(output / name)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_production_planner(self):
        self.compile_and_run("CompanionGemPlannerTest")

    def test_production_runtime_and_hooks(self):
        self.compile_and_run("CompanionGemRuntimeTest", runtime=True)

    def test_catalog_matches_curated_bc_rare_and_epic_gems(self):
        with (ROOT / "config/ahbot/tbc-cut-gems.tsv").open() as source:
            rows = list(csv.DictReader(source, delimiter="\t"))
        expected = {int(row["entry"]) for row in rows if int(row["Quality"]) >= 3}
        actual = list(map(int, re.findall(r'^\s*(\d+),', (SOURCE / "CompanionGemCatalog.h").read_text(), re.M)))
        self.assertEqual(set(actual), expected)
        self.assertEqual(len(actual), len(expected))
        self.assertEqual(len(actual), 95)

    def test_isolated_setup_mapping(self):
        setup = (ROOT / "setup.sh").read_text()
        block = setup[setup.index('RAID_CONF="$MODETC/mod_raid_roster.conf"'):
                      setup.index("# ── AH price lookup")]
        script = 'set -eu\nset_conf() { printf "%s=%s\\n" "$1" "$2"; }\n' + block
        with tempfile.TemporaryDirectory() as temp:
            (Path(temp) / "mod_raid_roster.conf").touch()
            for enabled in ("0", "1"):
                env = {"PATH": os.environ["PATH"], "MODETC": temp,
                       "COMPANION_SOCKET_GEMS_ENABLE": enabled}
                result = subprocess.run(["bash", "-c", script], env=env, text=True,
                                        capture_output=True, check=True)
                values = dict(line.split("=", 1) for line in result.stdout.splitlines())
                self.assertEqual(values["CompanionMaintenance.SocketGems.Enable"], enabled)
                self.assertEqual(values["CompanionMaintenance.SocketGems.IntervalSeconds"], "300")
                self.assertEqual(values["CompanionMaintenance.SocketGems.MinLevel"], "61")
                self.assertEqual(values["CompanionMaintenance.SocketGems.EpicPercent"], "20")
                self.assertEqual(values["RaidRoster.Enable"], "0")
                self.assertFalse(any(key.startswith("AiPlayerbot.") for key in values))

    def test_mirrors_and_opt_in_default(self):
        module = ROOT / "modules/mod-raid-roster"
        for relative in ["src/CompanionGemPlanner.h", "src/CompanionGemCatalog.h",
                         "src/CompanionGemMaintenance.cpp", "src/RaidRosterLoader.cpp",
                         "conf/mod_raid_roster.conf.dist"]:
            self.assertEqual((module / relative).read_bytes(),
                             (NATIVE / "modules/mod-raid-roster" / relative).read_bytes(), relative)
        config = (module / "conf/mod_raid_roster.conf.dist").read_text()
        self.assertIn("CompanionMaintenance.SocketGems.Enable = 0", config)
        self.assertIn("CompanionMaintenance.SocketGems.EpicPercent = 20", config)
        source = (SOURCE / "CompanionGemMaintenance.cpp").read_text()
        self.assertIn('GetOption<bool>("CompanionMaintenance.SocketGems.Enable", false)', source)
        for forbidden in ["InitEquipment", "InitTalents", "ResetStrategies", "ApplyEnchantAndGemsNew",
                          "CharacterDatabase.Execute", "DestroyItem", "SaveToDB"]:
            self.assertNotIn(forbidden, source)
        self.assertIn("AddCompanionGemMaintenanceScripts();", (SOURCE / "RaidRosterLoader.cpp").read_text())


if __name__ == "__main__":
    unittest.main()
