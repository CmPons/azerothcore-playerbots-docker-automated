"""Offline production-method regressions; never starts or mutates a live server."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
BASE = Path("src/server/scripts/Kalimdor/TempleOfAhnQiraj")
FILES = (BASE / "boss_bug_trio.cpp", BASE / "instance_temple_of_ahnqiraj.cpp", BASE / "temple_of_ahnqiraj.h")
PATCH = ROOT / "patches/0022-core-aq40-bug-trio-reset.patch"


def block(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def command(args, cwd=None):
    return subprocess.run(args, cwd=cwd, check=True, text=True, capture_output=True)


class BugTrioResetTests(unittest.TestCase):
    def test_production_reset_state_and_death_methods(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for path in FILES:
                destination = temp / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(CORE / path, destination)
            command(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            old = (temp / FILES[0]).read_text()
            boss, instance, header = [(CORE / path).read_text() for path in FILES]
            enums = "\n".join(block(text, signature) + ";" for text, signature in (
                (header, "enum DataTypes"), (header, "enum Creatures"), (boss, "enum Misc")))
            instance_methods = "\n".join(block(instance, signature) for signature in (
                "void Initialize()", "uint32 GetData(uint32 type)", "void SetData(uint32 type, uint32 data)"))
            trio_methods = "\n".join(block(boss, signature) for signature in (
                "void EvadeAllBosses(EvadeReason why)", "void EnterEvadeMode(EvadeReason why)",
                "void Reset() override", "void JustDied(Unit* killer)"))
            legacy = block(old, "void EvadeAllBosses(EvadeReason why)").replace(
                "EvadeAllBosses", "LegacyEvadeAllBosses", 1)
            harness = (ROOT / "scripts/tests/cpp/BugTrioResetTest.cpp").read_text()
            for marker, code in (("ENUMS", enums), ("INSTANCE_METHODS", instance_methods),
                                 ("TRIO_METHODS", trio_methods), ("LEGACY_EVADE", legacy)):
                harness = harness.replace(f"/* {marker} */", code)
            source = temp / "test.cpp"
            source.write_text(harness)
            binary = temp / "test"
            compiler = os.environ.get("CXX", "g++")
            build = subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                                    str(source), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = command([str(binary)])
            self.assertIn("Production Bug Trio reset/state regression tests passed", result.stdout)

    def test_patch_roundtrip_and_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for path in FILES:
                (temp / path).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(CORE / path, temp / path)
            command(["git", "apply", "--reverse", "--check", str(PATCH)], cwd=temp)
            command(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            command(["git", "apply", "--check", str(PATCH)], cwd=temp)
            command(["git", "apply", str(PATCH)], cwd=temp)
            for path in FILES:
                self.assertEqual((temp / path).read_bytes(), (CORE / path).read_bytes())
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines() if line.startswith("diff --git ")]
        self.assertEqual(set(names), {str(path) for path in FILES})

    def test_native_respawn_and_instance_local_consume_contracts(self):
        boss = (CORE / FILES[0]).read_text()
        self.assertNotIn("_creatureDying", boss)
        self.assertIn("GetCreature(instance->GetData(DATA_BUG_TRIO_CONSUME_TARGET))", boss)
        for bug in ("DATA_KRI", "DATA_YAUJ", "DATA_VEM"):
            self.assertIn(f"SetData(DATA_BUG_TRIO_CONSUME_TARGET, {bug})", boss)
        reset = block(boss, "void EvadeAllBosses(EvadeReason why)")
        self.assertNotIn("SummonCreature", reset)
        self.assertNotIn("LoadCreatureFromDB", reset)
        self.assertLess(reset.index("respawns.push_back(spawnId)"), reset.index("SaveCreatureRespawnTime"))
        creature = (CORE / "src/server/game/Entities/Creature/Creature.cpp").read_text()
        corpse = block(creature, "void Creature::RemoveCorpse(")
        self.assertIn("SaveRespawnTime();", corpse)
        self.assertIn("AddObjectToRemoveList();", corpse)
        map_source = (CORE / "src/server/game/Maps/Map.cpp").read_text()
        respawn = block(map_source, "void Map::ProcessCreatureRespawn(")
        self.assertIn("if (itr->second->IsAlive())", respawn)
        self.assertIn("LoadCreatureFromDB(spawnId, this, true, true)", respawn)
        save = block(map_source, "void Map::SaveCreatureRespawnTime(")
        self.assertIn("GetInstanceId()", save)
        self.assertIn("_respawnQueue.insert", save)


if __name__ == "__main__":
    unittest.main()
