"""Exercise only the companion-cap setter in a temporary file, never full setup."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
KEY = "AiPlayerbot.PersistentCompanionMaxPerAccount"


class CompanionCapSetupTests(unittest.TestCase):
    def apply(self, value, initial):
        setup = (ROOT / "setup.sh").read_text()
        setter = setup[setup.index("set_conf () {"):setup.index('\necho "==> 6/10')]
        calls = [line for line in setup.splitlines() if line.startswith(f'set_conf "{KEY}"')]
        self.assertEqual(len(calls), 1)
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "playerbots.conf"
            config.write_text(initial)
            env = {"PATH": os.environ["PATH"], "PB_CONF": str(config)}
            if value is not None:
                env["PERSISTENT_COMPANION_MAX_PER_ACCOUNT"] = value
            # Repeated application must replace, not duplicate the setting.
            subprocess.run(["bash", "-c", "set -eu\n" + setter + "\n" +
                            calls[0] + "\n" + calls[0]], env=env, check=True)
            return config.read_text()

    def test_raise_to_nine_preserves_other_settings(self):
        initial = f"{KEY} = 5\nAiPlayerbot.RandomBotJoinBG = 1\n"
        self.assertEqual(self.apply("9", initial),
                         f"{KEY} = 9\nAiPlayerbot.RandomBotJoinBG = 1\n")

    def test_omitted_keeps_native_default(self):
        self.assertEqual(self.apply(None, ""), f"{KEY} = 5\n")

    def test_zero_keeps_unlimited_semantics(self):
        self.assertEqual(self.apply("0", ""), f"{KEY} = 0\n")


if __name__ == "__main__":
    unittest.main()
