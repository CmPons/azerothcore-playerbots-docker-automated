"""Offline source-derived lifecycle tests; no server, config or database access."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PB = ROOT / "azerothcore-wotlk/modules/mod-playerbots"


def block(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    tokens = r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|[{}]'
    for match in re.finditer(tokens, source[opening:], re.DOTALL):
        token = match.group()
        depth += (token == "{") - (token == "}")
        if token == "}" and depth == 0:
            return source[start:opening + match.end()]
    raise AssertionError(f"Unterminated function: {signature}")


class RosterWorldBotsTests(unittest.TestCase):
    def test_production_lifecycle_bodies(self):
        source = (PB / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        harness = (ROOT / "scripts/tests/cpp/RosterWorldBotsTest.cpp").read_text()
        signatures = [
            "void RandomPlayerbotMgr::LoadWorldBotGuids()",
            "void RandomPlayerbotMgr::GetBots()",
            "bool RandomPlayerbotMgr::IsWorldBot(",
            "bool RandomPlayerbotMgr::AddWorldBot(",
            "uint32 RandomPlayerbotMgr::GetMaxAllowedBotCount()",
            "bool RandomPlayerbotMgr::IsRandomBot(ObjectGuid::LowType",
            "bool RandomPlayerbotMgr::ProcessBot(Player*",
            "void RandomPlayerbotMgr::OnPlayerLogout(",
        ]
        harness = harness.replace("// PRODUCTION_METHODS", "\n\n".join(block(source, s) for s in signatures))
        # Execute the actual population-budget and authorized routing code too.
        budget = source[source.index("    uint32 ordinaryBotCount ="):source.index(
            "        // Single RNG instance", source.index("uint32 ordinaryBotCount ="))]
        harness = harness.replace("// POPULATION_BUDGET", budget + "\n    }\n    else maxAllowedBotCount = 0;")
        login = (PB / "src/Bot/PlayerbotMgr.cpp").read_text()
        routing = block(login, "    if (sRandomPlayerbotMgr.IsWorldBot(playerGuid.GetCounter()))")
        harness = harness.replace("// AUTHORIZED_ROUTING", routing)
        ai = (PB / "src/Bot/PlayerbotAI.cpp").read_text()
        harness = harness.replace("// GROUP_TRANSITIONS", block(ai, "void PlayerbotAI::UpdateAIGroupMaster()"))
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "test.cpp"
            binary = Path(directory) / "test"
            cpp.write_text(harness)
            subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)],
                           check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn("all lifecycle cases passed", result.stdout)

    def test_startup_initialization_order_and_existing_event_cleanup(self):
        core = ROOT / "azerothcore-wotlk"
        world = block((core / "src/server/game/World/World.cpp").read_text(),
                      "void World::SetInitialWorldSettings()")
        self.assertLess(world.index("sCharacterCache->LoadCharacterCacheStorage();"),
                        world.index("sScriptMgr->OnBeforeWorldInitialized();"))
        script = block((PB / "src/Script/Playerbots.cpp").read_text(),
                       "void OnBeforeWorldInitialized() override")
        self.assertIn("sPlayerbotAIConfig.Initialize();", script)
        config = block((PB / "src/PlayerbotAIConfig.cpp").read_text(), "bool PlayerbotAIConfig::Initialize()")
        self.assertLess(config.index("RandomPlayerbotFactory::CreateRandomBots();"),
                        config.index("sRandomPlayerbotMgr.AssignAccountTypes();"))
        self.assertLess(config.index("sRandomPlayerbotMgr.AssignAccountTypes();"),
                        config.index("sRandomPlayerbotMgr.Init();"))
        source = (PB / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        assign = block(source, "void RandomPlayerbotMgr::AssignAccountTypes()")
        self.assertIn("addClassTypeAccounts.push_back(accountId)", assign)
        init = block(source, "void RandomPlayerbotMgr::Init()")
        self.assertIn("LoadWorldBotGuids();", init)
        # Existing startup cleanup makes a new type2 event filter unnecessary for removal.
        self.assertIn("DELETE FROM playerbots_random_bots WHERE event = 'add'", init)

    def test_integration_and_unchanged_group_semantics(self):
        source = (PB / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        update = block(source, "void RandomPlayerbotMgr::UpdateAIInternal(")
        self.assertLess(update.index("!sPlayerbotAIConfig.randomBotAutologin"), update.index("AddWorldBot(bot)"))
        self.assertIn("maxAllowedBotCount += worldBotGuids.size();", update)
        self.assertEqual(update.count("AddWorldBot(bot);"), 2)  # human-required and human-independent branches
        self.assertIn("if (realPlayerIsLogged && DelayLoginBotsTimer", update)
        self.assertIn("if (loginBots && botLoading.empty())", update)
        login = (PB / "src/Bot/PlayerbotMgr.cpp").read_text()
        add = block(login, "void PlayerbotHolder::AddPlayerBot(")
        self.assertLess(add.index("if (!allowed)"), add.index("if (sRandomPlayerbotMgr.IsWorldBot"))
        self.assertLess(add.index("botLoading.find"), add.index("if (sRandomPlayerbotMgr.IsWorldBot"))
        self.assertIn('return "player already logged in";', login)
        self.assertIn("addClassBot && !sRandomPlayerbotMgr.IsWorldBot(bot->GetGUID().GetCounter()) && master", login)
        ai = block((PB / "src/Bot/PlayerbotAI.cpp").read_text(), "void PlayerbotAI::UpdateAIGroupMaster()")
        self.assertIn("master && sRandomPlayerbotMgr.IsRandomBot(bot)", ai)
        self.assertIn("SetMaster(nullptr);", ai)
        self.assertIn("Player* newMaster = FindNewMaster();", ai)
        logout = block(source, "void RandomPlayerbotMgr::OnPlayerLogout(")
        self.assertIn("GetPlayerBotsBegin()", logout)
        self.assertIn("botAI->SetMaster(nullptr);", logout)
        self.assertNotIn("LogoutPlayerBot", logout)
        invite = (PB / "src/Ai/Base/Actions/AcceptInvitationAction.cpp").read_text()
        self.assertLess(invite.index("CheckLevelFor(PLAYERBOT_SECURITY_INVITE"), invite.index("botAI->SetMaster(inviter)"))
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(bot)", invite)
        self.assertIn('AiPlayerbot.WorldBotGuids = ""', (PB / "conf/playerbots.conf.dist").read_text())


if __name__ == "__main__":
    unittest.main()
