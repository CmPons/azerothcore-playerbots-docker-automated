"""Expansion raid reset coverage against production enums, writers and Chess bodies.

Offline only: no DB writes, CMake configuration, server build or service operations.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
SCRIPTS = CORE / "src/server/scripts"
INSTANCES = CORE / "src/server/game/Instances"
FIXTURES = ROOT / "scripts/tests/fixtures/raid-reset/layouts.json"
KARA = SCRIPTS / "EasternKingdoms/Karazhan/instance_karazhan.cpp"


def braced(source, marker):
    start = source.index("{", source.index(marker))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def compile_run(work, source, name):
    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise unittest.SkipTest("C++20 compiler unavailable")
    binary = work / name
    subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(INSTANCES), "-I" + str(work), str(source),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)


class ExpansionRaidResetTests(unittest.TestCase):
    def test_all_expansion_layouts_match_native_enums_and_writers(self):
        specs = json.loads(FIXTURES.read_text())
        self.assertEqual({r["map"] for r in specs},
                         {532, 534, 544, 548, 550, 564, 565, 568, 580,
                          533, 603, 615, 616, 624, 631, 649, 724})
        chunks = ['#include "ProgressionRaidReset.h"', '#include <cassert>',
                  'using uint32 = uint32_t;', 'using namespace ProgressionRaidReset;']
        for spec in specs:
            with self.subTest(raid=spec["name"]):
                header_path = SCRIPTS / spec["path"]
                header = header_path.read_text()
                instance = (header_path.parent / spec["instance"]).read_text()
                self.assertIn("SetHeaders(DataHeader)", instance)
                self.assertNotRegex(instance, r"\bGetSaveData\s*\(")
                prefix = re.search(r'#define DataHeader "([A-Z]+)"', header)[1]
                declaration = re.search(r'enum ' + spec["enum"] +
                                        r'\b[^{}]*\{[^{}]*\};', header, re.S)[0]
                chunks += [f'namespace Raid{spec["map"]} {{', declaration]
                assertions = [f'auto layout = GetLayout({spec["map"]});',
                              f'assert(layout && layout->header == "{prefix}");']
                if spec.get("checkpoint"):
                    self.assertNotIn("SetBossNumber(", instance)
                    self.assertNotIn("SetPersistentDataCount(", instance)
                    self.assertIn("data << InstanceProgress", instance)
                    assertions += ['assert(layout->format == SaveFormat::TrialCheckpoint);']
                    for stage, started, cleared in [("fresh", "false", "false"),
                                                    ("partial", "true", "false"),
                                                    ("cleared", "true", "true")]:
                        for symbol in spec[stage]:
                            assertions += [f'{{ auto progress = ReadProgress({spec["map"]}, '
                                           f'"{prefix} " + std::to_string({symbol}) + " 50 1 1");',
                                           f'assert(progress && progress->started == {started} '
                                           f'&& progress->cleared == {cleared}); }}']
                else:
                    count = spec["count"]
                    constant = re.search(r'uint32 const\s+' + count + r'\s*=\s*\d+;', header)
                    if constant:
                        chunks.append(constant[0])
                    if spec.get("legacy"):
                        self.assertNotIn("SetBossNumber(", instance)
                        self.assertNotIn("SetPersistentDataCount(", instance)
                        writer = braced(instance, "void WriteSaveDataMore")
                        self.assertEqual(re.findall(r'm_auiEncounter\[(\d)\]', writer), ["0", "1", "2", "3"])
                    else:
                        self.assertIn(f"SetBossNumber({count})", instance)
                    def mask(names):
                        return " | ".join(f"(uint32_t(1) << {s})" for s in names) or "0"
                    required = spec["required"]
                    encounters = required + spec.get("optional", [])
                    all_slots = encounters + spec.get("ignored", [])
                    assertions += [f'assert(layout->slots == {count});',
                                   f'assert(layout->requiredMask == ({mask(required)}));',
                                   f'assert(layout->encounterMask == ({mask(encounters)}));',
                                   f'assert(({mask(all_slots)}) == ((uint32_t(1) << {count}) - 1));']
                chunks += ['void Check() {', *assertions, '}', '}']
        chunks += ['int main() {', *(f'Raid{s["map"]}::Check();' for s in specs), '}']
        with tempfile.TemporaryDirectory(prefix="expansion-reset-enums-") as tmp:
            work = Path(tmp)
            source = work / "contracts.cpp"
            source.write_text("\n".join(chunks))
            compile_run(work, source, "contracts")

    def test_trial_checkpoints_only_advance_on_completed_encounters(self):
        path = SCRIPTS / "Northrend/CrusadersColiseum/TrialOfTheCrusader/instance_trial_of_the_crusader.cpp"
        source = path.read_text()
        data = braced(source, "void SetData(")
        for case, state in [("TYPE_NORTHREND_BEASTS_ALL", "BEASTS_DEAD"), ("TYPE_JARAXXUS", "JARAXXUS_DEAD"),
                            ("TYPE_FACTION_CHAMPIONS", "FACTION_CHAMPIONS_DEAD"),
                            ("TYPE_VALKYR", "VALKYR_DEAD"), ("TYPE_ANUBARAK", "DONE")]:
            block = data.split("case " + case + ":", 1)[1].split("case TYPE_", 1)[0]
            self.assertIn("data == DONE", block)
            self.assertIn("InstanceProgress = INSTANCE_PROGRESS_" + state, block)
            self.assertIn("SaveToDB()", block)
        icehowl = data.split("case TYPE_ICEHOWL:", 1)[1].split("case TYPE_", 1)[0]
        self.assertIn("(northrendBeastsMask & 7) == 7", icehowl)
        self.assertIn("SetData(TYPE_NORTHREND_BEASTS_ALL, DONE)", icehowl)
        self.assertIn("bool IsEncounterInProgress() const override", source)
        vault = (SCRIPTS / "Northrend/VaultOfArchavon/instance_vault_of_archavon.cpp").read_text()
        self.assertIn("bool IsEncounterInProgress() const override", vault)
        self.assertIn("if (data == DONE)", braced(vault, "void SetData("))
        self.assertIn("SaveToDB()", braced(vault, "void SetData("))

    def test_production_chess_checkpoint_and_reset_deferral(self):
        # Override permits a red regression run against the pre-change production source.
        source = Path(os.environ.get("AC_TEST_KARA_SOURCE", str(KARA))).read_text()
        data = braced(source, "void SetData(")
        chess = braced(data, "case DATA_CHESS_EVENT:")
        if "bool IsEncounterInProgress() const override" in source:
            predicate = braced(source, "bool IsEncounterInProgress() const override")
        else:
            predicate = "{ return InstanceScript::IsEncounterInProgress(); }"
        injected = ("void SetData(uint32 type, uint32 data) { switch (type) { case DATA_CHESS_EVENT: " +
                    chess + " break; } }\nbool IsEncounterInProgress() const override " + predicate)
        with tempfile.TemporaryDirectory(prefix="karazhan-reset-checkpoint-") as tmp:
            work = Path(tmp)
            (work / "KarazhanResetProduction.inc").write_text(injected)
            compile_run(work, ROOT / "scripts/tests/cpp/KarazhanResetCheckpointTest.cpp", "chess")

    def test_adoption_extensions_and_difficulty_scope_unchanged(self):
        source = (INSTANCES / "InstanceSaveMgr.cpp").read_text()
        load = braced(source, "void InstanceSaveMgr::LoadInstanceSaves()")
        self.assertIn("ProgressionRaidReset::GetLayout(mapId)", load)
        self.assertIn("m_progressionEnabled || stage != 0", load)
        self.assertIn("save->SetResetTime(deadline)", load)
        self.assertIn("save->SetExtendedResetTime(extendedDeadline)", load)
        self.assertIn("CharacterDatabase.DirectCommitTransaction(transaction)", load)
        self.assertNotIn("RAID_DIFFICULTY", load)
        binds = braced(source, "void InstanceSaveMgr::LoadCharacterBinds()")
        self.assertIn("bind.extended = extended", binds)
        self.assertIn("bind.perm = perm", binds)
        # The decoder does not read/modify a character's equipment, spec, progression or binds.
        policy = (INSTANCES / "ProgressionRaidReset.h").read_text()
        self.assertNotRegex(policy, r"CharacterDatabase|WorldDatabase|Player\*|Map\*")


if __name__ == "__main__":
    unittest.main()
