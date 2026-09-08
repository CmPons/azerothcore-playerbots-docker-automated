"""No live generation, configuration reload, server build or service changes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "modules/mod-playerbot-chatter"


class ChatterRaidRoutingTests(unittest.TestCase):
    def test_production_director_with_game_api_doubles(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Standalone C++20 compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="chatter-raid-routing-") as tmp:
            binary = str(Path(tmp) / "test")
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "scripts/tests/fixtures/chatter-ambient"),
                            "-I" + str(MODULE / "src"),
                            str(ROOT / "scripts/tests/cpp/ChatterAmbientRoutingTest.cpp"),
                            "-o", binary], check=True)
            subprocess.run([binary], check=True, timeout=10)

    def test_stale_group_delivery_is_checked_before_sending(self):
        source = (MODULE / "src/PBChatterWorld.cpp").read_text()
        gate = source.split("if (r.ambient && r.ambientKind == AMB_GROUP)", 1)[1].split("bool sent = false", 1)[0]
        self.assertIn("PBChatterChannelPolicy::SameGroup", gate)
        self.assertIn("anchor->IsInWorld()", gate)
        self.assertIn("anchorAI->IsRealPlayer()", gate)
        self.assertIn("group->isRaidGroup()", gate)
        self.assertIn("continue;", gate)
        self.assertNotIn("SayTo", gate)
        # The gate is ambient-only: reactive replies stay on their requested channel.
        self.assertIn("case PBChatChannel::Whisper: ai->Whisper", source)
        self.assertIn("case PBChatChannel::Raid:    sent = ai->SayToRaid", source)

    def test_defaults_and_invalid_values(self):
        source = (MODULE / "src/PBChatterConfig.cpp").read_text()
        self.assertIn('GetOption<int32_t>("PlayerbotChatter.AmbientRaidPreferenceChance", 80)', source)
        self.assertIn("raidPreference < 0 || raidPreference > 100", source)
        self.assertIn("raidPreference = 80;", source)
        self.assertIn("PlayerbotChatter.AmbientRaidPreferenceChance = 80\n",
                      (MODULE / "conf/mod_playerbot_chatter.conf.dist").read_text())

    def test_setup_isolated_idempotent_and_configurable(self):
        setup = (ROOT / "setup.sh").read_text()
        start = setup.index("set_conf () {")
        helper = setup[start:setup.index("\n}\n", start) + 3]
        block = setup.split("# BEGIN CHATTER RAID PREFERENCE\n", 1)[1].split(
            "# END CHATTER RAID PREFERENCE", 1)[0]
        for chance in (None, 0, 80, 100):
            with self.subTest(chance=chance), tempfile.TemporaryDirectory() as tmp:
                config = Path(tmp) / "chatter.conf"
                config.write_text("PlayerbotChatter.AmbientGuild = 1\n# keep\n")
                env = os.environ.copy()
                env["PBCHAT_CONF"] = str(config)
                env.pop("CHATTER_AMBIENT_RAID_PREFERENCE_CHANCE", None)
                if chance is not None:
                    env["CHATTER_AMBIENT_RAID_PREFERENCE_CHANCE"] = str(chance)
                for _ in range(2):
                    subprocess.run(["bash", "-euc", helper + "\n" + block], env=env, check=True)
                    self.assertEqual(config.read_text(), "PlayerbotChatter.AmbientGuild = 1\n# keep\n"
                                     f"PlayerbotChatter.AmbientRaidPreferenceChance = {80 if chance is None else chance}\n")

    def test_canonical_build_tree_matches(self):
        build = ROOT / "azerothcore-wotlk/modules/mod-playerbot-chatter"
        if not build.exists():
            self.skipTest("Optional build checkout absent")
        for name in ("src/PBChatterAmbient.cpp", "src/PBChatterWorld.cpp", "src/PBChatterConfig.h",
                     "src/PBChatterConfig.cpp", "src/PBChatterChannelPolicy.h",
                     "conf/mod_playerbot_chatter.conf.dist"):
            self.assertEqual((MODULE / name).read_bytes(), (build / name).read_bytes(), name)


if __name__ == "__main__":
    unittest.main()
