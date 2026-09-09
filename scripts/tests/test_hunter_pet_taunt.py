"""Offline tests only: no live pets, SQL changes, config reloads or server builds."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
BOT = CORE / "modules/mod-playerbots"
SOURCE = BOT / "src/Script/PlayerbotsHunterPetTaunt.cpp"
PATCH = ROOT / "patches/0021-playerbot-hunter-pet-taunt-policy.patch"


@unittest.skipUnless(BOT.is_dir(), "Optional playerbots checkout absent")
class HunterPetTauntTests(unittest.TestCase):
    def test_complete_production_hook_with_api_doubles(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="pet-taunt-test-") as tmp:
            binary = Path(tmp) / "test"
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "scripts/tests/fixtures/hunter-pet-taunt"),
                            "-I" + str(SOURCE.parent),
                            str(ROOT / "scripts/tests/cpp/HunterPetTauntTest.cpp"),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_registration_and_no_autocast_setting_mutation(self):
        loader = (BOT / "src/Script/Playerbots.cpp").read_text()
        self.assertIn("void AddPlayerbotsHunterPetTauntScripts();", loader)
        self.assertIn("    AddPlayerbotsHunterPetTauntScripts();", loader)
        source = SOURCE.read_text()
        self.assertIn("PlayerbotAI::IsTank(tank, true)", source)
        for forbidden in ("ToggleAutocast", "m_autospells", "GetSubGroup", "IsExplicitMainTank", "Database"):
            self.assertNotIn(forbidden, source)
        header = (CORE / "src/server/game/Scripting/ScriptDefines/AllSpellScript.h").read_text()
        self.assertIn("IsDatabaseBound() const override { return false; }", header)

    def test_core_calls_hooks_for_autocast_and_preparation(self):
        source = (CORE / "src/server/game/Spells/Spell.cpp").read_text()
        pet_cast = method(source, "SpellCastResult Spell::CheckPetCast(")
        self.assertLess(pet_cast.index("m_targets.SetUnitTarget(target)"), pet_cast.index("return CheckCast(true)"))
        auto_cast = method(source, "bool Spell::CanAutoCast(")
        self.assertIn("CheckPetCast(target)", auto_cast)
        self.assertIn("result == SPELL_CAST_OK || result == SPELL_FAILED_UNIT_NOT_INFRONT", auto_cast)
        check = method(source, "SpellCastResult Spell::CheckCast(")
        self.assertIn("sScriptMgr->OnSpellCheckCast(this, strict, res)", check)
        self.assertIn("if (res != SPELL_CAST_OK)\n        return res;", check)
        prepare = method(source, "SpellCastResult Spell::prepare(")
        self.assertLess(prepare.index("InitExplicitTargets(*targets)"), prepare.index("sScriptMgr->CanPrepare"))
        self.assertIn("finish(false);\n        return SPELL_FAILED_UNKNOWN;", prepare)

    def test_patch_round_trip_and_isolated_scope(self):
        patch = PATCH.read_text()
        paths = [line.removeprefix("+++ b/modules/mod-playerbots/")
                 for line in patch.splitlines() if line.startswith("+++ b/")]
        self.assertEqual(set(paths), {"src/Script/Playerbots.cpp", "src/Script/PlayerbotsHunterPetTaunt.cpp"})
        with tempfile.TemporaryDirectory(prefix="pet-taunt-patch-") as tmp:
            subprocess.run(["git", "init", "-q", tmp], check=True)
            before = {}
            for path in paths:
                dest = Path(tmp) / "modules/mod-playerbots" / path
                dest.parent.mkdir(parents=True, exist_ok=True)
                before[path] = (BOT / path).read_bytes()
                dest.write_bytes(before[path])
            for flags in (("--reverse", "--check"), ("--reverse",), ("--check",), (), ("--reverse", "--check")):
                subprocess.run(["git", "-C", tmp, "apply", *flags, str(PATCH)], check=True)
            for path in paths:
                self.assertEqual((Path(tmp) / "modules/mod-playerbots" / path).read_bytes(), before[path])


if __name__ == "__main__":
    unittest.main()
