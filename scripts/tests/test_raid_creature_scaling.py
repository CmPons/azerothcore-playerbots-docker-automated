"""Offline eligibility regression: extracted production methods + native faction/DataMap code.

Object lookup, control flags and health storage are doubles, not native/live encounter proof.
"""
from pathlib import Path
import subprocess
import unittest

from scripts.tests import test_twins_bug_scaling as twins

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk/src/server/game"
MODULE = ROOT / "modules/mod-raid-scaling/src"
HARNESS = (ROOT / "scripts/tests/cpp/RaidCreatureScalingTest.cpp").read_text()


class RaidCreatureScalingTests(unittest.TestCase):
    extra_harness = HARNESS

    @classmethod
    def setUpClass(cls):
        twins.TwinsBugScalingTests.setUpClass.__func__(cls)

    def test_normal_rank_mound_and_neutral_trigger_damage_after_ouro_despawn(self):
        self.assertIn("Passed mound", twins.run([str(self.binary), "mound"]))

    def test_normal_combat_add_health_and_legacy_elite_boss_intro_behavior(self):
        self.assertIn("Passed health", twins.run([str(self.binary), "health"]))

    def test_friendly_critter_utility_and_scripted_npc_exclusions_player_pet_victim(self):
        self.assertIn("Passed exclusions", twins.run([str(self.binary), "exclusions"]))

    def test_player_origin_charm_guardian_gameobject_nested_and_despawned_ancestry(self):
        self.assertIn("Passed origins", twins.run([str(self.binary), "origins"]))

    def test_ancestry_cycles_and_depth_bound(self):
        self.assertIn("Passed bounds", twins.run([str(self.binary), "bounds"]))

    def test_default_manual_off_disabled_nonraid_and_instance_isolation(self):
        self.assertIn("Passed settings", twins.run([str(self.binary), "settings"]))

    def test_zero_spawn_id_loaded_summon_manual_off_reenable_dead_and_low_health(self):
        self.assertIn("Passed runtime-lifecycle", twins.run([str(self.binary), "runtime-lifecycle"]))

    def test_runtime_guid_snapshot_reresolves_after_removal_during_apply_and_restore(self):
        self.assertIn("Passed runtime-removal", twins.run([str(self.binary), "runtime-removal"]))

    def test_original_red_and_mutant_detection(self):
        source = (MODULE / "RaidScalingMgr.cpp").read_text()
        # Exact pre-change methods, not a rewritten model or a dependency on private git history.
        baseline = (ROOT / "scripts/tests/fixtures/raid-scaling/OriginalEligibility.cpp").read_text()
        original = source
        for signature in ("bool RaidScalingMgr::IsScalableCreature(",
                          "float RaidScalingMgr::GetDamageScale(",
                          "void RaidScalingMgr::OnCreatureAddWorld("):
            original = original.replace(twins.block(source, signature), twins.block(baseline, signature))
        reviewed = (ROOT / "scripts/tests/fixtures/raid-scaling/SpawnIdOnlyTraversal.cpp").read_text()
        old_walkers = source
        for signature in ("void RaidScalingMgr::ApplyToMap(", "void RaidScalingMgr::RestoreMap("):
            old_walkers = old_walkers.replace(twins.block(source, signature), twins.block(reviewed, signature))
        restore_signature = "void RaidScalingMgr::RestoreMap("
        mutants = [
            ("reviewed-spawn-id-only", old_walkers, ("runtime-lifecycle",)),
            ("restore-spawn-id-only", source.replace(twins.block(source, restore_signature),
                                                    twins.block(reviewed, restore_signature)),
             ("runtime-lifecycle",)),
            ("normal-dead-min-one", source.replace(
                "creature->SetMaxHealth(original.maxHealth);\n    creature->SetHealth(creature->isDead() ? 0 :",
                "creature->SetMaxHealth(original.maxHealth);\n    creature->SetHealth("), ("runtime-lifecycle",)),
            ("original", original, ("mound", "health")),
            ("damage-reuses-health", source.replace(
                "if (!creature)\n        return 1.0f;",
                "if (!creature || !IsScalableCreature(creature))\n        return 1.0f;"), ("mound",)),
            ("no-spawn-origin-cache", source.replace(
                "RaidCreatureEligibility::CaptureSummonOrigin(creature);", ""), ("origins",)),
        ]
        for name, mutant, scenarios in mutants:
            with self.subTest(mutant=name):
                self.assertNotEqual(mutant, source)
                class Mutant(unittest.TestCase):
                    source_override = mutant
                    extra_harness = HARNESS
                try:
                    twins.TwinsBugScalingTests.setUpClass.__func__(Mutant)
                    for scenario in scenarios:
                        result = subprocess.run([str(Mutant.binary), scenario],
                                                capture_output=True, text=True, timeout=10)
                        self.assertNotEqual(result.returncode, 0, name + " survived")
                        self.assertIn("Assertion", result.stderr)  # compiled/runs; not a compile-error red
                        print(f"Detected {name}/{scenario}: {result.stderr.strip()}")
                finally:
                    Mutant.doClassCleanups()

    def test_native_api_and_summon_order_contracts(self):
        unit = (CORE / "Entities/Unit/Unit.h").read_text()
        summon = (CORE / "Entities/Creature/TemporarySummon.cpp").read_text()
        obj = (CORE / "Entities/Object/Object.cpp").read_text()
        creature = (CORE / "Entities/Creature/Creature.cpp").read_text()
        self.assertIn("return m_ControlledByPlayer;", unit)
        self.assertIn("return m_CreatedByPlayer;", unit)
        self.assertIn("return GetGuidValue(UNIT_FIELD_CREATEDBY);", unit)
        init = twins.block(summon, "void TempSummon::InitStats(")
        self.assertIn("if (owner->IsPlayer())\n            m_CreatedByPlayer = true;", init)
        self.assertIn("m_summonerGUID = owner;", summon)
        factory = twins.block(obj, "TempSummon* Map::SummonCreature(")
        self.assertLess(factory.index("summon->InitStats(duration)"), factory.index("AddToMap(summon"))
        self.assertLess(factory.index("AddToMap(summon"), factory.index("summon->InitSummon()"))
        add = twins.block(creature, "void Creature::AddToWorld()")
        self.assertLess(add.index("AIM_Initialize()"), add.index("OnCreatureAddWorld(this)"))
        self.assertIn("GetObjectsStore().Insert<Creature>(GetGUID(), this)", add)
        self.assertIn("if (m_spawnId)", add)
        native_map = (CORE / "Maps/Map.cpp").read_text()
        self.assertIn("return _objectsStore.Find<Creature>(guid);",
                      twins.block(native_map, "Creature* Map::GetCreature("))
        remove = twins.block(creature, "void Creature::RemoveFromWorld()")
        self.assertIn("GetObjectsStore().Remove<Creature>(GetGUID())", remove)
        minion = twins.block(summon, "void Minion::InitStats(")
        self.assertIn("SetCreatorGUID(owner->GetGUID())", minion)
        self.assertIn("SetFaction(owner->GetFaction())", minion)
        unit_cpp = (CORE / "Entities/Unit/Unit.cpp").read_text()
        deal = twins.block(unit_cpp, "uint32 Unit::DealDamage(")
        self.assertIn("sScriptMgr->DealDamage(attacker, victim, damage, damagetype)", deal)
        spell_damage = twins.block(unit_cpp, "void Unit::DealSpellDamage(")
        self.assertIn("Unit::DealDamage(this, victim", spell_damage)
        periodic = (CORE / "Spells/Auras/SpellAuraEffects.cpp").read_text()
        self.assertIn("Unit::DealDamage(caster, target, damage, &cleanDamage, DOT,", periodic)
        ouro = (ROOT / "azerothcore-wotlk/src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_ouro.cpp").read_text()
        self.assertIn("me->DespawnOrUnsummon(1s)", ouro)
        self.assertIn("SPELL_DIRTMOUND_PASSIVE     = 26092", ouro)

    def test_no_new_encounter_allowlist_and_private_mirror(self):
        source = (MODULE / "RaidScalingMgr.cpp").read_text()
        for signature in ("bool RaidScalingMgr::IsScalableCreature(", "float RaidScalingMgr::GetDamageScale("):
            method = twins.block(source, signature)
            for entry in ("15712", "15718", "15517"):
                self.assertNotIn(entry, method)
        for name in ("RaidScalingMgr.cpp", "RaidCreatureEligibility.h"):
            self.assertEqual((MODULE / name).read_bytes(),
                             (ROOT / "azerothcore-wotlk/modules/mod-raid-scaling/src" / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
