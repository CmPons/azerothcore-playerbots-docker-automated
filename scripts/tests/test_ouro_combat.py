"""Offline production Ouro methods + native scheduler; no server or database access."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_bug_trio_reset import block

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
REL = Path("src/server/scripts/Kalimdor/TempleOfAhnQiraj/boss_ouro.cpp")
PATCH = ROOT / "patches/0032-core-aq40-ouro-combat-lifecycle.patch"


def run(args, cwd=None):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result


def build_fixture(temp, boss):
    header = (CORE / REL.parent / "temple_of_ahnqiraj.h").read_text()
    threat = (CORE / "src/server/game/Combat/ThreatManager.cpp").read_text()
    ai = (CORE / "src/server/game/AI/CreatureAI.cpp").read_text()
    enums = "\n".join(block(text, signature) + ";" for text, signature in (
        (header, "enum DataTypes"), (header, "enum Creatures"), (boss, "enum Spells"), (boss, "enum Misc")))
    native = "\n".join(block(threat, signature) for signature in (
        "bool ThreatReference::FlagsAllowFighting", "bool ThreatReference::ShouldBeOffline() const",
        "bool ThreatManager::CompareReferencesLT", "ThreatReference const* ThreatManager::ReselectVictim()"))
    native += "\n" + block(ai, "bool CreatureAI::UpdateVictim()")
    native += "\n" + block(ai, "void CreatureAI::EngagementStart(Unit* who)")
    code = (ROOT / "scripts/tests/cpp/OuroCombatTest.cpp").read_text()
    classes = "\n".join(block(boss, signature) + ";" for signature in (
        "struct npc_ouro_spawner", "struct boss_ouro", "struct npc_dirt_mound"))
    for marker, value in (("ENUMS", enums), ("NATIVE_METHODS", native), ("OURO_CLASSES", classes)):
        code = code.replace(f"/* {marker} */", value)
    (temp / "test.cpp").write_text(code)
    for name in ("TaskScheduler.h", "TaskScheduler.cpp"):
        shutil.copyfile(CORE / "src/common/Utilities" / name, temp / name)
    (temp / "Util.h").write_text("#pragma once\n#include <algorithm>\n#include <cstdint>\n#include <chrono>\nusing Milliseconds = std::chrono::milliseconds;\n"
                                 "using uint32 = uint32_t;\ninline uint32 urand(uint32 low, uint32) { return low; }\n")
    (temp / "Errors.h").write_text("#pragma once\n#include <cassert>\n#define ASSERT(x) assert(x)\n")
    binary = temp / "test"
    run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-D_GLIBCXX_DEBUG",
         "-fsanitize=undefined", "-fno-sanitize-recover=all", str(temp / "test.cpp"),
         str(temp / "TaskScheduler.cpp"), "-o", str(binary)])
    return binary


class OuroCombatTests(unittest.TestCase):
    def test_production_combat_lifecycle(self):
        with tempfile.TemporaryDirectory(prefix="ouro-combat-") as directory:
            binary = build_fixture(Path(directory), (CORE / REL).read_text())
            result = run([str(binary)])
            self.assertIn("Ouro production combat regressions passed", result.stdout)

    def test_legacy_red_before(self):
        with tempfile.TemporaryDirectory(prefix="ouro-combat-legacy-") as directory:
            temp = Path(directory)
            (temp / REL).parent.mkdir(parents=True)
            shutil.copyfile(CORE / REL, temp / REL)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            binary = build_fixture(temp, (temp / REL).read_text())
            for scenario, failure in (
                    ("ranged", "CHECK failed: f.instance.state == IN_PROGRESS"),
                    ("knockback", "CHECK failed: f.instance.state == IN_PROGRESS && f.ouro.despawns == 0")):
                result = subprocess.run([str(binary), scenario], capture_output=True, text=True)
                self.assertEqual(result.returncode, 1, f"Legacy unexpected failure in {scenario}: {result.stderr}")
                self.assertIn(failure, result.stderr)

    def test_incremental_patch_roundtrip(self):
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(names, [str(REL)])
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            (temp / REL).parent.mkdir(parents=True)
            shutil.copyfile(CORE / REL, temp / REL)
            run(["git", "apply", "--reverse", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            baseline = (temp / REL).read_text()
            run(["git", "apply", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", str(PATCH)], cwd=temp)
            self.assertEqual((temp / REL).read_bytes(), (CORE / REL).read_bytes())
            current = (CORE / REL).read_text()
            self.assertEqual(block(current, "struct npc_dirt_mound"), block(baseline, "struct npc_dirt_mound"))
            self.assertEqual(block(current, "struct npc_ouro_spawner"), block(baseline, "struct npc_ouro_spawner"))
            current = block(current, "struct boss_ouro")
            baseline = block(baseline, "struct boss_ouro")
            for signature in ("void DamageTaken", "void Submerge()", "void CastGroundRupture()",
                              "void SpellHitTarget", "void Reset() override",
                              "void EnterEvadeMode", "void JustEngagedWith"):
                actual = block(current, signature)
                if signature == "void Reset() override":
                    actual = actual.replace("        _sandBlastOrientation.reset();\n", "")
                self.assertEqual(actual, block(baseline, signature), signature)
            # The only intentional event/mechanic change is Sand Blast's cast-start facing snapshot.
            emerge = block(current, "void Emerge()")
            for added in (
                    "                        me->SetInFront(target);\n",
                    "                        me->SetFacingTo(me->GetOrientation());\n",
                    "                    // Sand Blast is a caster-relative cone, not an explicit-target spell with native focus.\n",
                    "                    _sandBlastOrientation = me->GetOrientation();\n"):
                self.assertIn(added, emerge)
                emerge = emerge.replace(added, "")
            self.assertEqual(emerge, block(baseline, "void Emerge()"))

    def test_sand_blast_initial_turn_alone_is_insufficient(self):
        current = (CORE / REL).read_text()
        guard = ("        if (_sandBlastOrientation)\n"
                 "            me->SetOrientation(*_sandBlastOrientation);\n"
                 "        else if (!me->HasSpellFocus())")
        self.assertIn(guard, current)
        mutant = current.replace(guard, "        if (!me->HasSpellFocus())")
        with tempfile.TemporaryDirectory(prefix="ouro-cone-mutant-") as directory:
            binary = build_fixture(Path(directory), mutant)
            result = subprocess.run([str(binary), "sand-blast-continuity"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("CHECK failed: f.ouro.GetOrientation() == cone && f.ouro.victim == &f.tank", result.stderr)

    def test_native_contracts(self):
        threat = (CORE / "src/server/game/Combat/ThreatManager.cpp").read_text()
        header = (CORE / "src/server/game/Combat/ThreatManager.h").read_text()
        creature = (CORE / "src/server/game/Entities/Creature/Creature.cpp").read_text()
        unit = (CORE / "src/server/game/Entities/Unit/Unit.cpp").read_text()
        ai = (CORE / "src/server/game/AI/CreatureAI.h").read_text()
        scripted = (CORE / "src/server/game/AI/ScriptedAI/ScriptedCreature.cpp").read_text()
        self.assertIn("bool IsThreatListEmpty(bool includeOffline = false)", header)
        self.assertIn("!AI()->CanAIAttack(victim)", block(creature, "bool Creature::CanCreatureAttack"))
        self.assertIn("RegisterForAIUpdate", block(threat, "void ThreatReference::UpdateOffline()"))
        self.assertIn("ProcessAIUpdates()", block(threat, "void ThreatManager::UpdateVictim()"))
        self.assertIn("if (!IsEngaged()) EngagementStart(who)", ai)
        self.assertIn("SetInFront(target)", block(unit, "Unit* Creature::SelectVictim()"))
        taunt = block(threat, "void ThreatManager::TauntUpdate()")
        self.assertIn("ThreatReference::TauntState(state++)", taunt)
        self.assertIn("tauntStates[(*it)->GetCasterGUID()]", taunt)
        self.assertIn("scheduler.SetValidator([this]", scripted)
        self.assertIn("return !me->HasUnitState(UNIT_STATE_CASTING)", scripted)
        self.assertIn("DoZoneInCombat()", block(scripted, "void BossAI::_JustEngagedWith()"))
        spell = (CORE / "src/server/game/Spells/Spell.cpp").read_text()
        unit_ai = (CORE / "src/server/game/AI/CoreAI/UnitAI.cpp").read_text()
        self.assertIn("me->CastSpell((Unit*)nullptr, spellId, triggered)", block(unit_ai, "SpellCastResult UnitAI::DoCastAOE"))
        self.assertNotIn("SetOrientation", block(creature, "void Creature::SetTarget"))
        explicit = block(spell, "void Spell::InitExplicitTargets")
        self.assertIn("m_spellInfo->GetExplicitTargetMask()", explicit)
        self.assertIn("m_targets.RemoveObjectTarget()", explicit)
        cone = block(spell, "void Spell::SelectImplicitConeTargets")
        self.assertIn("TARGET_REFERENCE_TYPE_CASTER", cone)
        self.assertIn("WorldObjectSpellConeTargetCheck check(coneAngle, radius, m_caster", cone)
        self.assertIn("m_targets.GetObjectTarget() && m_caster != m_targets.GetObjectTarget()", spell)
        current = (CORE / REL).read_text()
        boss = block(current, "struct boss_ouro")
        self.assertNotIn("bool CanAIAttack", boss)
        for guard in ("!me->IsEngaged()", "!me->IsAlive()", "me->IsCharmed()", "REACT_PASSIVE", "!me->HasSpellFocus()"):
            self.assertIn(guard, block(boss, "bool UpdateOuroVictim()"))


if __name__ == "__main__":
    unittest.main()
