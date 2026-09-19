"""Offline companion BG policy and race guards; never touch the running server."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.tests.test_roster_world_bots import ROOT, PB, block


class SoloBattlegroundTests(unittest.TestCase):
    def test_production_policy_hooks_and_cancellation(self):
        manager = (PB / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        script = (PB / "src/Script/Playerbots.cpp").read_text()
        actions = (PB / "src/Ai/Base/Actions/BattleGroundJoinAction.cpp").read_text()
        harness = (ROOT / "scripts/tests/cpp/SoloBattlegroundsTest.cpp").read_text()
        harness = harness.replace("// POLICY_METHODS", "\n".join(block(manager, name) for name in (
            "bool RandomPlayerbotMgr::IsBattlegroundCompanion(",
            "bool RandomPlayerbotMgr::CanAutoJoinBattleground(")))
        harness = harness.replace("// PACKET_HOOKS", "\n".join(block(script, name) for name in (
            "bool OnPlayerCanJoinInBattlegroundQueue(", "bool OnPlayerCanJoinInArenaQueue(",
            "bool OnPlayerCanBattleFieldPort(")))
        status = block(actions, "bool BGStatusAction::Execute(")
        guard = block(status, "if ((statusid == STATUS_WAIT_QUEUE || statusid == STATUS_WAIT_JOIN)")
        harness = harness.replace("// STATUS_GUARD", guard)
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "solo_bg.cpp"
            binary = Path(directory) / "solo_bg"
            cpp.write_text(harness)
            subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)],
                           check=True)
            subprocess.run([str(binary)], check=True, timeout=10)

    def test_all_queue_and_invitation_paths_use_guard(self):
        source = (PB / "src/Ai/Base/Actions/BattleGroundJoinAction.cpp").read_text()
        for method, before in (
            ("bool BGJoinAction::isUseful()", "bgList.clear()"),
            ("bool BGJoinAction::Execute(", 'AI_VALUE(uint32, "bg type")'),
            ("bool BGJoinAction::canJoinBg(", "GetBattlegroundTemplate"),
            ("bool BGJoinAction::JoinQueue(", "QueuePacket"),
            ("bool BGJoinAction::gatherArenaTeam(", "GetArenaTeamByCaptain"),
            ("bool BGStatusAction::Execute(", "// bot is in queue"),
        ):
            body = block(source, method)
            self.assertLess(body.index("CanAutoJoinBattleground"), body.index(before), method)
        gather = block(source, "bool BGJoinAction::gatherArenaTeam(")
        self.assertLess(gather.index("IsBattlegroundCompanion(itr->Guid.GetCounter())"),
                        gather.index("FindConnectedPlayer"))
        legacy = (PB / "src/Ai/Base/Actions/AcceptBattlegroundInvitationAction.cpp").read_text()
        self.assertLess(legacy.index("CanAutoJoinBattleground"), legacy.index("HandleBattleFieldPortOpcode"))
        script = (PB / "src/Script/Playerbots.cpp").read_text()
        constructor = script[script.index('PlayerScript("PlayerbotsPlayerScript", {'):script.index("    void OnPlayerLogin")]
        for hook in ("PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE", "PLAYERHOOK_CAN_JOIN_IN_ARENA_QUEUE",
                     "PLAYERHOOK_CAN_BATTLEFIELD_PORT"):
            self.assertIn(hook, constructor)
        # Guard at actual packet consumption, not only at AI action selection.
        core = (ROOT / "azerothcore-wotlk/src/server/game/Handlers/BattleGroundHandler.cpp").read_text()
        port = block(core, "void WorldSession::HandleBattleFieldPortOpcode(")
        self.assertIn("if (!sScriptMgr->OnPlayerCanBattleFieldPort", port)


if __name__ == "__main__":
    unittest.main()
