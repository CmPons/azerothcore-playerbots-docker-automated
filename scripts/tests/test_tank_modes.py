"""Offline tests only; production bodies and native-header checks, no service operations."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
BOT = CORE / "modules/mod-playerbots"


def body(path):
    return re.sub(r'^#include .*$', '', path.read_text(), flags=re.MULTILINE)


class TankModeTests(unittest.TestCase):
    def test_production_commands_modes_and_persistence(self):
        self.run_fixture()

    def test_previous_group_code_reproduces_stale_mt_flags(self):
        old = subprocess.run(["git", "-C", str(CORE), "show",
                              "897c2c6d72632ad6e3f1c01a9e82815ce2c165a4:src/server/game/Groups/Group.cpp"],
                             text=True, capture_output=True)
        if old.returncode:
            self.skipTest("Historical baseline unavailable in this checkout")
        self.run_fixture(group_source=old.stdout, expect_failure=True)

    def run_fixture(self, group_source=None, expect_failure=False):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        playerbot = (BOT / "src/Bot/PlayerbotAI.cpp").read_text()
        group_source = group_source or (CORE / "src/server/game/Groups/Group.cpp").read_text()
        code = [
            body(BOT / "src/Ai/Base/Util/TankModes.h"),
            body(BOT / "src/Ai/Base/Actions/TankModeAction.h"),
            method(group_source, "void Group::SetGroupMemberFlag("),
            method(group_source, "void Group::RemoveUniqueGroupMemberFlag("),
            body(BOT / "src/Ai/Base/Util/TankModes.cpp"),
            *[method(playerbot, signature) for signature in (
                "ObjectGuid PlayerbotAI::GetMainTankGuid(", "bool PlayerbotAI::IsMainTank(",
                "bool PlayerbotAI::IsExplicitMainTank(", "bool PlayerbotAI::IsOffTank(",
                "bool PlayerbotAI::HasAggro(")],
            method((BOT / "src/Ai/Base/Value/AttackerCountValues.cpp").read_text(),
                   "bool HasAggroValue::Calculate("),
            body(BOT / "src/Ai/Base/Value/TankTargetValue.cpp"),
            body(BOT / "src/Ai/Base/Actions/TankModeAction.cpp"),
        ]
        harness = (ROOT / "scripts/tests/cpp/TankModesTest.cpp").read_text()
        harness = harness.replace("// PRODUCTION_CODE", "\n\n".join(code))
        with tempfile.TemporaryDirectory(prefix="tank-modes-test-") as tmp:
            source = Path(tmp) / "test.cpp"
            binary = Path(tmp) / "test"
            source.write_text(harness)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(CORE / "src/common/Utilities"),
                            str(source), "-o", str(binary)], check=True)
            import resource
            result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10,
                                    preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
            if expect_failure:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("CharacterDatabase.saved[red.guid.id] == MEMBER_FLAG_ASSISTANT", result.stderr)
            else:
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                print(result.stdout.strip())

    def test_command_and_update_registration(self):
        for name in ("ChatActionContext.h", "ChatTriggerContext.h"):
            self.assertIn('creators["tank strategy"]', (BOT / "src/Ai/Base" / name).read_text())
        commands = (BOT / "src/Ai/Base/Strategy/ChatCommandHandlerStrategy.cpp").read_text()
        self.assertIn('supported.push_back("tank strategy")', commands)
        script = (BOT / "src/Script/Playerbots.cpp").read_text()
        update = method(script, "void OnPlayerAfterUpdate(")
        self.assertLess(update.index("TankModes::Update(botAI, diff)"), update.index("botAI->UpdateAI(diff)"))

    def test_automatic_admission_not_global_taunt_ban(self):
        source = (BOT / "src/Ai/Base/Actions/GenericSpellActions.cpp").read_text()
        for signature in ("bool CastSpellAction::isUseful(", "bool CastSpellAction::Execute("):
            self.assertIn("TankModes::SuppressAutomaticSpell", method(source, signature))
        attacks = (BOT / "src/Ai/Base/Actions/AttackAction.cpp").read_text()
        self.assertIn("botAI->raidCombat.scheduled && !TankModes::CanAcquire",
                      method(attacks, "bool AttackAction::Execute("))
        self.assertNotIn("TankModes::", method(attacks, "bool AttackAction::Attack("))
        casts = (BOT / "src/Bot/PlayerbotAI.cpp").read_text()
        self.assertNotIn("TankModes::", method(casts, "bool PlayerbotAI::CastSpell(uint32 spellId, Unit*"))
        policy = (BOT / "src/Ai/Base/Util/TankModes.cpp").read_text()
        self.assertIn("!ai->raidCombat.scheduled", policy)
        self.assertNotIn("AllSpellScript", policy)
        self.assertFalse((BOT / "src/Script/PlayerbotsTankTaunt.cpp").exists())
        engine = (BOT / "src/Bot/Engine/Engine.cpp").read_text()
        direct = method(engine, "ActionResult Engine::ExecuteAction(")
        self.assertIn("scheduled(botAI->raidCombat.scheduled, false)", direct)
        self.assertIn("event.SetScheduled(false)", direct)
        self.assertIn("event.IsScheduled()", method(engine, "bool Engine::DoNextAction("))

    def test_ui_assignment_does_not_preclear_without_persistence(self):
        handler = method((CORE / "src/server/game/Handlers/GroupHandler.cpp").read_text(),
                         "void WorldSession::HandlePartyAssignmentOpcode(")
        self.assertNotIn("RemoveUniqueGroupMemberFlag", handler)
        self.assertIn("SetGroupMemberFlag(guid, apply, MEMBER_FLAG_MAINTANK)", handler)
        off = method((BOT / "src/Bot/PlayerbotAI.cpp").read_text(), "bool PlayerbotAI::IsOffTank(")
        self.assertNotIn("GetSubGroup", off)
        self.assertIn("GetMainTankGuid(group)", off)


if __name__ == "__main__":
    unittest.main()
