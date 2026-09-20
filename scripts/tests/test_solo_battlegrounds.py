"""Offline companion BG policy and race guards; never touch the running server."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.tests.test_roster_world_bots import ROOT, PB, block


class SoloBattlegroundTests(unittest.TestCase):
    def test_production_policy_hooks_and_cancellation(self):
        def source(path):
            revision = os.environ.get("SOLO_BG_REVISION")
            if revision:
                return subprocess.check_output(["git", "-C", str(PB), "show", f"{revision}:{path}"], text=True)
            return (PB / path).read_text()

        manager = source("src/Bot/RandomPlayerbotMgr.cpp")
        script = source("src/Script/Playerbots.cpp")
        actions = source("src/Ai/Base/Actions/BattleGroundJoinAction.cpp")
        harness = (ROOT / "scripts/tests/cpp/SoloBattlegroundsTest.cpp").read_text()
        methods = ["bool RandomPlayerbotMgr::IsBattlegroundCompanion(",
                   "bool RandomPlayerbotMgr::CanAutoJoinBattleground("]
        if "bool RandomPlayerbotMgr::CanAcceptBattlegroundQueue(" in manager:
            methods += ["bool RandomPlayerbotMgr::CanAcceptBattlegroundQueue(",
                        "void RandomPlayerbotMgr::CancelCompanionBattlegroundQueue("]
            harness = harness.replace("// CANCEL_OPERATION", block(
                manager, "class CancelCompanionBattlegroundQueueOperation") + ";")
        else:
            harness = "#define LEGACY_POLICY 1\n" + harness
        harness = harness.replace("// POLICY_METHODS", "\n".join(block(manager, name) for name in methods))
        core = ROOT / "azerothcore-wotlk/src/server/game/Battlegrounds"
        harness = harness.replace("// QUEUE_INFO", block((core / "BattlegroundQueue.h").read_text(),
                                                       "struct GroupQueueInfo") + ";")
        queue = (core / "BattlegroundQueue.cpp").read_text()
        provenance = "\n".join(line for line in block(queue, "GroupQueueInfo* BattlegroundQueue::AddGroup(").splitlines()
                               if "ginfo->QueuedGroupGuid" in line or "ginfo->QueuedLeaderGuid" in line)
        self.assertEqual(len(provenance.splitlines()), 2)
        harness = harness.replace("// QUEUE_PROVENANCE", provenance.replace("leader->", "leaderPtr->"))
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
        ):
            body = block(source, method)
            self.assertLess(body.index("CanAutoJoinBattleground"), body.index(before), method)
        status = block(source, "bool BGStatusAction::Execute(")
        self.assertLess(status.index("CanAcceptBattlegroundQueue"), status.index("// bot is in queue"))
        guard = block(status, "if ((statusid == STATUS_WAIT_QUEUE || statusid == STATUS_WAIT_JOIN)")
        self.assertIn("CancelCompanionBattlegroundQueue", guard)
        self.assertNotIn("CMSG_BATTLEFIELD_PORT", guard)
        gather = block(source, "bool BGJoinAction::gatherArenaTeam(")
        self.assertLess(gather.index("IsBattlegroundCompanion(itr->Guid.GetCounter())"),
                        gather.index("FindConnectedPlayer"))
        legacy = (PB / "src/Ai/Base/Actions/AcceptBattlegroundInvitationAction.cpp").read_text()
        self.assertLess(legacy.index("CanAcceptBattlegroundQueue(bot, BATTLEGROUND_QUEUE_WS)"),
                        legacy.index("HandleBattleFieldPortOpcode"))
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
