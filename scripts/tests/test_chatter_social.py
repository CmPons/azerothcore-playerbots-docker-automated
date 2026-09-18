"""Offline social-fact collection/output tests; no database, Pi or server operations."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-playerbot-chatter"
CHANGED_SOURCE = ("PBChatterContext.h", "PBChatterSocial.cpp", "PBChatterObserver.cpp",
                  "PBChatterAmbientPrompt.h", "PBChatterAmbientPrompt.cpp", "PBChatterEvents.cpp")


class ChatterSocialTests(unittest.TestCase):
    def test_production_social_helper_and_ambient_event_output(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "Standalone C++20 compiler required")
        with tempfile.TemporaryDirectory(prefix="chatter-social-") as tmp:
            binary = str(Path(tmp) / "test")
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-DFMT_HEADER_ONLY", "-DFMT_USE_NONTYPE_TEMPLATE_ARGS=0",
                            "-I" + str(ROOT / "scripts/tests/fixtures/chatter-social"),
                            "-I" + str(MODULE / "src"),
                            "-I" + str(ROOT / "azerothcore-wotlk/deps/fmt/include"),
                            "-I" + str(ROOT / "azerothcore-wotlk/src/common/Utilities"),
                            "-I" + str(ROOT / "azerothcore-wotlk/src/common"),
                            str(ROOT / "scripts/tests/cpp/ChatterSocialTest.cpp"),
                            str(MODULE / "src/PBChatterSocial.cpp"),
                            str(MODULE / "src/PBChatterAmbientPrompt.cpp"),
                            "-o", binary], check=True)
            subprocess.run([binary], check=True, timeout=10)

    def test_reactive_collection_precedes_group_channel_gate(self):
        source = (MODULE / "src/PBChatterObserver.cpp").read_text()
        prompt = source.split("std::string BuildPrompt(", 1)[1].split("void Enqueue(", 1)[0]
        before_group = prompt.split("p += BuildGroupContext", 1)[0]
        self.assertIn("p += PBChatterContext::BuildSocialContext(bot, sender);", before_group)
        self.assertNotIn("if (", before_group)
        member = source.split("std::string MemberFactLine(", 1)[1].split("std::string BuildTacticalNeeds", 1)[0]
        self.assertIn("MemberSocialFacts(bot, member)", member)
        self.assertIn('return line + ", dead" + social;', member)
        self.assertIn("return line + social;", member)
        self.assertIn("if (++shown >= 10)", source)
        # Whisper's lore fallback and ordinary say/whisper all retain the same BuildPrompt path.
        self.assertIn("BuildPrompt(bot, sender, PBChatChannel::Whisper, nullptr, msg)", source)
        self.assertIn("Enqueue(bot, player, PBChatChannel::Say, msg)", source)
        self.assertIn("Enqueue(bot, player, PBChatChannel::Whisper, msg)", source)

    def test_join_focus_is_resolved_during_prompt_collection(self):
        source = (MODULE / "src/PBChatterEvents.cpp").read_text()
        self.assertIn("? FindByCounter(ev.joinedMemberGuid) : nullptr", source)
        self.assertIn("member->GetGroup() != group", source)
        social = (MODULE / "src/PBChatterSocial.cpp").read_text()
        self.assertNotIn("Database", social)
        self.assertNotIn("RaidRosterStore", social)
        self.assertNotIn("PBChatterMemory", social)

    def test_canonical_build_tree_matches(self):
        build = ROOT / "azerothcore-wotlk/modules/mod-playerbot-chatter/src"
        self.assertTrue(build.exists())
        for name in CHANGED_SOURCE:
            self.assertEqual((MODULE / "src" / name).read_bytes(), (build / name).read_bytes(), name)


if __name__ == "__main__":
    unittest.main()
