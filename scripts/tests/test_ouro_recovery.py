"""Offline Ouro recovery regressions; no live commands, database writes or server startup."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_bug_trio_reset import block

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
REL = Path("src/server/scripts/Kalimdor/TempleOfAhnQiraj/instance_temple_of_ahnqiraj.cpp")
PATCH = ROOT / "patches/0030-core-aq40-ouro-spawner-recovery.patch"


def run(args, cwd=None):
    return subprocess.run(args, cwd=cwd, check=True, capture_output=True, text=True)


class OuroRecoveryTests(unittest.TestCase):
    def test_production_methods(self):
        with tempfile.TemporaryDirectory(prefix="ouro-recovery-") as directory:
            temp = Path(directory)
            (temp / REL).parent.mkdir(parents=True)
            shutil.copyfile(CORE / REL, temp / REL)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            old = (temp / REL).read_text()
            current = (CORE / REL).read_text()
            header = (CORE / REL.parent / "temple_of_ahnqiraj.h").read_text()
            enums = "\n".join(block(header, signature) + ";" for signature in ("enum DataTypes", "enum Creatures"))
            methods = "\n".join(block(current, signature) for signature in (
                "void OnPlayerEnter(Player* player)", "void OnCreatureRemove(Creature* creature)",
                "void OnCreatureCreate(Creature* creature)", "bool SetBossState(uint32 type, EncounterState state)",
                "bool OuroCanRecover() const", "void ScheduleOuroRecovery()", "void RecoverOuroSpawner()"))
            legacy = block(old, "bool SetBossState(uint32 type, EncounterState state)")
            legacy = legacy.replace("SetBossState(", "LegacySetBossState(", 1).replace(" override", "", 1)
            mound = re.search(r"constexpr uint32 NPC_OURO_DIRT_MOUND = \d+;", current).group()
            code = (ROOT / "scripts/tests/cpp/OuroRecoveryTest.cpp").read_text()
            for marker, value in (("ENUMS", enums), ("METHODS", methods), ("LEGACY", legacy), ("MOUND_ENTRY", mound)):
                code = code.replace(f"/* {marker} */", value)
            source = temp / "test.cpp"
            source.write_text(code)
            binary = temp / "test"
            built = subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                                    "-D_GLIBCXX_DEBUG", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                                    str(source), "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Ouro production recovery regressions passed", result.stdout)

    def test_patch_roundtrip_and_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            (temp / REL).parent.mkdir(parents=True)
            shutil.copyfile(CORE / REL, temp / REL)
            run(["git", "apply", "--reverse", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            run(["git", "apply", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", str(PATCH)], cwd=temp)
            self.assertEqual((temp / REL).read_bytes(), (CORE / REL).read_bytes())
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines() if line.startswith("diff --git ")]
        self.assertEqual(names, [str(REL)])

    def test_focused_pinned_core_replay(self):
        pin = next(line.split()[1] for line in (ROOT / "repo-pins.txt").read_text().splitlines()
                   if line.startswith("azerothcore-wotlk "))
        blob = subprocess.run(["git", "-C", str(CORE), "show", pin + ":" + str(REL)], capture_output=True)
        if blob.returncode:
            self.skipTest("Pinned core object unavailable locally; no network fetch")
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            (temp / REL).parent.mkdir(parents=True)
            (temp / REL).write_bytes(blob.stdout)
            for patch in sorted((ROOT / "patches").glob("*.patch")):
                if f"diff --git a/{REL} b/{REL}" in patch.read_text():
                    run(["git", "apply", "--include=" + str(REL), str(patch)], cwd=temp)
            self.assertEqual((temp / REL).read_bytes(), (CORE / REL).read_bytes())

    def test_native_lifecycle_and_no_gameplay_shortcuts(self):
        current = (CORE / REL).read_text()
        recovery = block(current, "void RecoverOuroSpawner()")
        for forbidden in ("SummonCreature", "LoadCreatureFromDB", "SetHealth", "SetBossState", "SetCompleted",
                          "ResetInstance", "CharacterDatabase", "WorldDatabase", "ProcessRespawns", "LoadGrid"):
            self.assertNotIn(forbidden, recovery)
        self.assertLess(recovery.index("spawns.push_back"), recovery.index("SaveCreatureRespawnTime"))
        native = (CORE / "src/server/game/Maps/Map.cpp").read_text()
        save = block(native, "void Map::SaveCreatureRespawnTime(")
        self.assertIn("time_t& respawnTime", save)
        self.assertIn("GetInstanceId()", save)
        self.assertIn("_respawnQueue.insert", save)
        process = block(native, "void Map::ProcessCreatureRespawn(")
        for check in ("IsSpawnGroupActive", "IsGridLoaded", "IsAlive()", "LoadCreatureFromDB(spawnId, this, true, true)"):
            self.assertIn(check, process)
        base = (CORE / "src/server/game/Instances/InstanceScript.cpp").read_text()
        self.assertIn("_objectGuids.erase", block(base, "void InstanceScript::AddObject(WorldObject*"))
        self.assertIn("scheduler.Update(diff)", block(base, "void InstanceScript::Update(uint32 diff)"))
        boss = (CORE / REL.parent / "boss_ouro.cpp").read_text()
        self.assertIn("me->DespawnOrUnsummon();", block(boss, "void JustSummoned(Creature* creature)"))
        self.assertIn("90s, GROUP_PHASE_TRANSITION", block(boss, "void Emerge()"))
        self.assertIn("me->GetThreatMgr().IsThreatListEmpty()", block(boss, "void EnterEvadeMode(EvadeReason"))


if __name__ == "__main__":
    unittest.main()
