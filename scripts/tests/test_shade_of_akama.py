"""Offline production-bound Shade channel regression; no server/DB writes or CMake build."""
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import unittest

from scripts.tests.test_flight_casting import block

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
BOSS = Path("src/server/scripts/Outland/BlackTemple/boss_shade_of_akama.cpp")
BEFORE = "7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72"
OLD_GUARD = "            if (!me->HasUnitState(UNIT_STATE_CASTING))"
NEW_GUARD = """            // Action-allowed channels do not set UNIT_STATE_CASTING. Recasting an active
            // channel refreshes its aura but adds another slow stack that outlives it.
            if (!me->IsNonMeleeSpellCast(false))"""


class ShadeOfAkamaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="shade-channel-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.source = (CORE / BOSS).read_text()
        cls.original = subprocess.check_output(
            ["git", "-C", str(CORE), "show", f"{BEFORE}:{BOSS}"], text=True)
        cls.unit = (CORE / "src/server/game/Entities/Unit/Unit.cpp").read_text()
        cls.spell = (CORE / "src/server/game/Spells/Spell.cpp").read_text()
        cls.info = (CORE / "src/server/game/Spells/SpellInfo.cpp").read_text()
        cls.data = json.loads((ROOT / "scripts/tests/fixtures/shade-of-akama/spells.json").read_text())
        cls.binaries = {}

    def compile(self, name="current", source=None):
        if name in self.binaries:
            return self.binaries[name]
        source = self.source if source is None else source
        enum_files = {
            "src/server/shared/SharedDefines.h": ("SpellAttr1", "SpellAttr2", "SpellAttr5"),
            "src/server/game/Entities/Unit/Unit.h": ("CurrentSpellTypes",),
            "src/server/game/Entities/Unit/UnitDefines.h": ("UnitState",),
            "src/server/game/Spells/Spell.h": ("SpellState",),
            "src/server/game/Spells/Auras/SpellAuraDefines.h": ("AuraEffectHandleModes",),
        }
        enums = [block((CORE / path).read_text(), "enum " + enum) + ";"
                 for path, names in enum_files.items() for enum in names]
        enums += [block(source, "enum Spells") + ";", block(source, "enum Misc") + ";"]
        replacements = {
            "// NATIVE_ENUMS": "\n".join(enums),
            "// NATIVE_SPELL_PREDICATES": "\n".join(block(self.info, signature) for signature in (
                "bool SpellInfo::IsChanneled()", "bool SpellInfo::IsActionAllowedChannel()")),
            "// NATIVE_CURRENT_SPELL": block(self.unit, "void Unit::SetCurrentCastedSpell("),
            "// NATIVE_ACTIVE_CAST": block(self.unit, "bool Unit::IsNonMeleeSpellCast("),
            "// NATIVE_CHANNEL_REMOVAL": block(source, "void HandleEffectRemove("),
            "// NATIVE_CHANNELER": block(source, "struct npc_ashtongue_channeler") + ";",
            "// NATIVE_SORCERER": block(source, "struct npc_ashtongue_sorcerer") + ";",
            "// INSTALLED_SPELL_DATA": "\n".join((
                "channelInfo.Id = 40401;",
                f'channelInfo.AttributesEx = {self.data["40401"]["AttributesEx"]};',
                f'channelInfo.AttributesEx5 = {self.data["40401"]["AttributesEx5"]};',
                f'channelInfo.Effects[1].TriggerSpell = {self.data["40401"]["EffectTriggerSpell_2"]};')),
        }
        fixture = (ROOT / "scripts/tests/cpp/ShadeChannelTest.cpp").read_text()
        for marker, value in replacements.items():
            self.assertEqual(fixture.count(marker), 1, marker)
            fixture = fixture.replace(marker, value)
        cpp, binary = self.directory / (name + ".cpp"), self.directory / name
        cpp.write_text(fixture)
        result = subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-g",
                                 "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                                 "-fno-omit-frame-pointer", "-no-pie", str(cpp), "-o", str(binary)],
                                text=True, capture_output=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.binaries[name] = binary
        return binary

    def run_case(self, case, name="current", source=None, expect_failure=False):
        result = subprocess.run([str(self.compile(name, source)), case], text=True, capture_output=True,
                                timeout=15, env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"),
                                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
        if expect_failure:
            self.assertNotEqual(result.returncode, 0, "Defective channel guard unexpectedly passed")
            self.assertIn("Assertion", result.stderr)
        else:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout.strip())

    def test_repeated_callbacks_and_channeler_deaths(self):
        self.run_case("lifecycle")

    def test_interruption_and_rechannel(self):
        self.run_case("interrupt")

    def test_failed_cast_retries(self):
        self.run_case("retry")

    def test_ordinary_channel_still_blocks_recast(self):
        self.run_case("ordinary")

    def test_other_active_cast_is_preserved(self):
        self.run_case("other-cast")

    def test_twenty_repulls(self):
        self.run_case("repull")

    def test_sorcerer_contribution_preserved(self):
        self.run_case("sorcerer")

    def test_old_guard_reproduces_orphan_stacks_and_fails_lifecycle(self):
        self.run_case("legacy", "old", self.original)
        self.run_case("lifecycle", "old", self.original, expect_failure=True)

    def test_skipping_channel_check_rejected(self):
        source = self.source.replace("!me->IsNonMeleeSpellCast(false)",
                                     "!me->IsNonMeleeSpellCast(false, true)")
        self.assertNotEqual(source, self.source)
        self.run_case("lifecycle", "skip-channel", source, expect_failure=True)

    def test_disabling_channel_cast_rejected(self):
        source = self.source.replace("!me->IsNonMeleeSpellCast(false)", "false")
        self.assertNotEqual(source, self.source)
        self.run_case("lifecycle", "never-cast", source, expect_failure=True)

    def test_only_channel_admission_changed(self):
        self.assertEqual(self.source.count(NEW_GUARD), 1)
        self.assertEqual(self.source.replace(NEW_GUARD, OLD_GUARD), self.original)
        slow = self.data["40520"]
        self.assertEqual(slow["StackAmount"], 99)
        self.assertEqual(slow["EffectApplyAuraName_1"], 33)
        self.assertEqual(slow["EffectBasePoints_1"] + slow["EffectDieSides_1"], -30)

    def test_cancellation_double_matches_native_self_replacement_contract(self):
        cancel = block(self.spell, "void Spell::cancel(bool bySelf)")
        non_self = block(cancel, "if (!bySelf)")
        self.assertIn("unit->RemoveOwnedAura(m_spellInfo->Id, m_originalCasterGUID", non_self)
        self.assertEqual(cancel.count("RemoveOwnedAura("), 1)
        stack = block(self.unit, "Aura* Unit::_TryStackingOrRefreshingExistingAura(")
        self.assertIn("SPELL_ATTR0_CU_SINGLE_AURA_STACK", stack)
        self.assertIn("foundAura->ModStackAmount(1", stack)


if __name__ == "__main__":
    unittest.main()
