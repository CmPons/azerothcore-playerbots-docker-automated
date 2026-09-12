"""Offline Twins-add scaling and real health-aura regression tests; no server/DB mutations."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-raid-scaling"
CORE = ROOT / "azerothcore-wotlk"


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


class TwinsBugScalingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="twins-bug-scaling-")
        cls.addClassCleanup(cls.directory.cleanup)
        temp = Path(cls.directory.name)
        source = (MODULE / "src/RaidScalingMgr.cpp").read_text()
        signatures = ["bool IsTwinsEncounterBug(", "RaidScalingMgr& RaidScalingMgr::Instance()",
                      "uint64 RaidScalingMgr::MakeKey(", "bool RaidScalingMgr::HasScaling(",
                      "RaidScaleSettings RaidScalingMgr::MakeSettings(",
                      "float RaidScalingMgr::ClampHealth(", "float RaidScalingMgr::ClampDamage(",
                      "std::optional<RaidScaleSettings> RaidScalingMgr::GetSettings(",
                      "bool RaidScalingMgr::IsScalableCreature(",
                      "RaidScaleCreatureKind RaidScalingMgr::ClassifyCreature(",
                      "float RaidScalingMgr::HealthScaleFor(", "float RaidScalingMgr::DamageScaleFor(",
                      "uint32 RaidScalingMgr::ScaleHealth(", "void RaidScalingMgr::OnCreatureAddWorld(",
                      "void RaidScalingMgr::ApplyToCreature(", "void RaidScalingMgr::RestoreCreature(",
                      "void RaidScalingMgr::ApplyToMap(", "void RaidScalingMgr::RestoreMap(",
                      "bool RaidScalingMgr::DisableForMap(", "float RaidScalingMgr::GetDamageScale("]
        parts = [block(source, sig) for sig in signatures]
        for path, sig in [
            ("Entities/Unit/Unit.cpp", "float Unit::GetTotalAuraModValue("),
            ("Entities/Unit/StatSystem.cpp", "void Creature::UpdateMaxHealth()"),
            ("Spells/Auras/SpellAuraEffects.cpp", "void AuraEffect::HandleAuraModIncreaseHealthPercent(")
        ]:
            parts.append(block((CORE / "src/server/game" / path).read_text(), sig))
        loader = (MODULE / "src/RaidScalingLoader.cpp").read_text()
        parts.append(block(loader, "uint32 DealDamage(").replace(
            "uint32 DealDamage(", "uint32 RaidScalingUnitScript::DealDamage(").replace(" override", ""))
        harness = (ROOT / "scripts/tests/cpp/TwinsBugScalingTest.cpp").read_text()
        (temp / "test.cpp").write_text(harness.replace("/* PRODUCTION */", "\n\n".join(parts)))
        # Expose manager internals only in the temporary test copy; production header stays private.
        header = (MODULE / "src/RaidScalingMgr.h").read_text().replace("private:", "public:")
        (temp / "RaidScalingMgr.h").write_text(header)
        (temp / "Define.h").write_text(
            "#pragma once\n#include <cstdint>\nusing uint8=std::uint8_t; using uint32=std::uint32_t; "
            "using uint64=std::uint64_t;\n")
        (temp / "ObjectGuid.h").write_text('''#pragma once
#include "Define.h"
#include <compare>
#include <functional>
enum class HighGuid { Unit };
struct ObjectGuid
{
    uint64 value=0;
    template<HighGuid> static ObjectGuid Create(uint32 entry,uint32 spawn)
    { return {(uint64(entry)<<32)|spawn}; }
    uint32 GetEntry() const { return value>>32; }
    auto operator<=>(ObjectGuid const&) const = default;
};
namespace std { template<> struct hash<ObjectGuid>
{ size_t operator()(ObjectGuid guid) const { return hash<uint64>{}(guid.value); } }; }
''')
        cls.binary = temp / "test"
        run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
             "-fsanitize=undefined,float-divide-by-zero,float-cast-overflow", "-fno-sanitize-recover=all",
             "-I" + str(temp), "-I" + str(MODULE / "src"), str(temp / "test.cpp"), "-o", str(cls.binary)])

    def test_native_mutation_apply_remove_and_reapply_do_not_undo_scaling_or_heal(self):
        self.assertIn("Passed", run([str(self.binary), "mutation"]))

    def test_outgoing_damage_player_pet_npc_and_manual_trash_multiplier(self):
        self.assertIn("Passed", run([str(self.binary), "damage"]))

    def test_linked_room_bugs_only_preserving_normal_creatures_and_exclusions(self):
        self.assertIn("Passed", run([str(self.binary), "scope"]))

    def test_active_mutation_enable_override_disable_reenable_and_instance_isolation(self):
        self.assertIn("Passed", run([str(self.binary), "lifecycle"]))

    def test_reproduce_legacy_health_recalculation_and_preserve_other_bosses(self):
        self.assertIn("Passed", run([str(self.binary), "legacy"]))

    def test_source_sync_and_no_aura_or_encounter_state_gate(self):
        source = (MODULE / "src/RaidScalingMgr.cpp").read_text()
        helper = block(source, "bool IsTwinsEncounterBug(")
        self.assertIn("GetLinkedRespawnGuid", helper)
        self.assertNotIn("HasAura", helper)
        self.assertNotIn("GetBossState", helper)
        for name in ("RaidScalingMgr.cpp", "RaidScalingMgr.h"):
            self.assertEqual((MODULE / "src" / name).read_bytes(),
                             (CORE / "modules/mod-raid-scaling/src" / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
