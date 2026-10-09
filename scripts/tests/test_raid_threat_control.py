"""Offline per-instance command/state tests; no server, SQL, configuration or live commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / 'azerothcore-wotlk/modules/mod-playerbots'


class RaidThreatControlTests(unittest.TestCase):
    def fixture(self, name):
        with tempfile.TemporaryDirectory(prefix='raid-threat-control-') as directory:
            tmp = Path(directory)
            for header in ('AllMapScript', 'Chat', 'Config', 'Map', 'Player', 'ScriptMgr', 'WorldSession'):
                (tmp / (header + '.h')).write_text(
                    '#include "' + str(ROOT / 'scripts/tests/fixtures/raid-threat/Runtime.h') + '"\n')
            binary = tmp / 'test'
            result = subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                                     '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie',
                                     '-pthread', '-I' + str(tmp), '-I' + str(BOT / 'src/Ai/Base/Util'),
                                     '-I' + str(BOT / 'src/Script'), str(ROOT / 'scripts/tests/cpp' / name),
                                     '-o', str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('passed', result.stdout)

    def test_policy_parser_isolation_reset_stale_observations_and_concurrency(self):
        self.fixture('RaidThreatControlStateTest.cpp')

    def test_complete_command_handler_runtime_adapter_and_map_destruction(self):
        self.fixture('RaidThreatControlRuntimeTest.cpp')

    def test_registration_and_no_encounter_or_spell_mutation(self):
        registration = (BOT / 'src/Script/PlayerbotCommandScript.cpp').read_text()
        self.assertIn('AddRaidThreatControlScripts();', registration)
        source = (BOT / 'src/Script/RaidThreatControl.cpp').read_text()
        for forbidden in ('CastSpell(', 'DoSpecificAction(', 'SetHealth(', 'SetMaxHealth(', 'ChangeStrategy(',
                          'CharacterDatabase', 'WorldDatabase', 'SetTalent', 'TeleportTo('):
            self.assertNotIn(forbidden, source)
        self.assertIn('SEC_GAMEMASTER, Acore::ChatCommands::Console::No', source)
        self.assertIn('ALLMAPHOOK_ON_DESTROY_MAP', source)
        self.assertIn('settings.healing = HealingMode::Normal', source)


if __name__ == '__main__':
    unittest.main()
