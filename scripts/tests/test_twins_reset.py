"""Compile the production Twins reset and dispatcher against offline map/API doubles."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-raid-scaling"


def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run(args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


class TwinsResetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="twins-reset-")
        cls.addClassCleanup(cls.directory.cleanup)
        tmp = Path(cls.directory.name)
        source = (MODULE / "src/RaidScalingTwinsReset.cpp").read_text()
        production = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
        dispatcher = block((MODULE / "src/RaidScalingMgr.cpp").read_text(), "bool RaidScalingMgr::ResetBoss(")
        harness = (ROOT / "scripts/tests/cpp/TwinsResetTest.cpp").read_text()
        (tmp / "test.cpp").write_text(harness.replace("/* PRODUCTION */", production).replace(
            "/* DISPATCHER */", dispatcher))
        (tmp / "RaidScalingMgr.h").write_text((MODULE / "src/RaidScalingMgr.h").read_text().replace(
            "private:", "public:"))
        (tmp / "Define.h").write_text("#pragma once\n#include <cstdint>\nusing uint8=std::uint8_t; "
                                      "using uint32=std::uint32_t; using uint64=std::uint64_t;\n")
        (tmp / "ObjectGuid.h").write_text('''#pragma once
#include "Define.h"
#include <compare>
#include <functional>
enum class HighGuid { Unit };
struct ObjectGuid
{
    using LowType=uint32;
    uint64 value=0;
    template<HighGuid> static ObjectGuid Create(uint32 entry,uint32 spawn)
    { return {(uint64(entry)<<32)|spawn}; }
    uint32 GetEntry() const { return value>>32; }
    explicit operator bool() const { return value!=0; }
    auto operator<=>(ObjectGuid const&) const = default;
};
namespace std { template<> struct hash<ObjectGuid>
{ size_t operator()(ObjectGuid guid) const { return hash<uint64>{}(guid.value); } }; }
''')
        cls.binary = tmp / "test"
        run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
             "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(tmp),
             "-I" + str(MODULE / "src"), str(tmp / "test.cpp"), "-o", str(cls.binary)])

    def test_full_original_spawns_bugs_doors_intro_and_repeat(self):
        self.assertIn("Passed", run([str(self.binary), "complete"]))

    def test_compatibility_ai_reinit_and_scaling_off(self):
        self.assertIn("Passed", run([str(self.binary), "compatibility"]))

    def test_combat_and_control_guards_before_state_mutation(self):
        self.assertIn("Passed", run([str(self.binary), "guards"]))

    def test_missing_metadata_refuses_without_guessing_completion_bit(self):
        self.assertIn("Passed", run([str(self.binary), "catalogue"]))

    def test_resolved_mask_and_native_failure_reporting_retry(self):
        self.assertIn("Passed", run([str(self.binary), "mask-failure"]))

    def test_other_boss_reset_path_unchanged(self):
        self.assertIn("Passed", run([str(self.binary), "generic"]))

    def test_source_sync_and_native_only_contracts(self):
        for name in ("RaidScalingMgr.cpp", "RaidScalingMgr.h", "RaidScalingTwinsReset.cpp"):
            self.assertEqual((MODULE / "src" / name).read_bytes(),
                             (ROOT / "azerothcore-wotlk/modules/mod-raid-scaling/src" / name).read_bytes())
        source = (MODULE / "src/RaidScalingTwinsReset.cpp").read_text()
        for forbidden in ("Unit::Kill(", "SummonCreature(", "CharacterDatabase", "WorldDatabase",
                          "DeleteRespawnTimes(", "RemoveAllObjectsInRemoveList(", "ProcessRespawns(",
                          "ApplyToMap(", "ResetGroupBinds(", "EnterEvadeMode("):
            self.assertNotIn(forbidden, source)
        self.assertIn("ProcessCreatureRespawn(spawnId)", source)
        self.assertIn("SetCompletedEncountersMask", source)
        self.assertIn("ResetAreaTriggerDone(TwinsIntroTrigger)", source)
        # The preexisting Twin evade override can recurse through a dead peer, so use native respawn instead.
        core = ROOT / "azerothcore-wotlk/src/server/game"
        self.assertIn("ThreatManager& GetThreatMgr()", (core / "Entities/Unit/Unit.h").read_text())
        self.assertIn("creature->GetThreatMgr().ClearAllThreat()", source)
        native = (core / "Entities/Creature/Creature.cpp").read_text()
        respawn = block(native, "void Creature::Respawn(bool force)")
        self.assertIn("SaveCreatureRespawnTime", respawn)
        self.assertIn("AddObjectToRemoveList", respawn)
        self.assertNotIn("Unit::Kill(", respawn)
        identity = block((core / "Instances/InstanceScript.cpp").read_text(),
                         "void InstanceScript::AddObject(WorldObject*")
        self.assertIn("i->second == obj->GetGUID()", identity)


if __name__ == "__main__":
    unittest.main()
