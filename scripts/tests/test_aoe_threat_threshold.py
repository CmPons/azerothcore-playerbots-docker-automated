"""Offline production-body tests for the narrow generic AoE threat threshold adjustment."""
from pathlib import Path
import os
import resource
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
BOT = ROOT / 'azerothcore-wotlk/modules/mod-playerbots'
SOURCE = 'src/Ai/Base/Strategy/ThreatStrategy.cpp'
BASELINE = '8d73b1a5721848071cd5e3048c7ad84a8f1c8194'
COMMENT = '        // Allow more headroom on secondary enemies; the current-target and boss gates still apply.\n'


class AoeThreatThresholdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='aoe-threat-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.source = (BOT / SOURCE).read_text()
        cls.old = subprocess.check_output(['git', '-C', str(BOT), 'show', BASELINE + ':' + SOURCE], text=True)
        cls.binary = cls.compile(cls.source, 'current')

    @classmethod
    def compile(cls, source, name):
        fixture = (ROOT / 'scripts/tests/cpp/AoeThreatThresholdTest.cpp').read_text()
        header = (BOT / 'src/Ai/Base/Actions/GenericSpellActions.h').read_text()
        healing = header[header.index('class CastHealingSpellAction :'):]
        fixture = fixture.replace('// HEAL_THREAT_TYPE', method(healing, 'ActionThreatType getThreatType()'))
        fixture = fixture.replace('// PRODUCTION', method(source, 'float ThreatMultiplier::GetValue('))
        cpp = cls.directory / (name + '.cpp')
        binary = cls.directory / name
        cpp.write_text(fixture)
        result = subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                                 '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie',
                                 str(cpp), '-o', str(binary)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return binary

    def execute(self, binary, scenario, fails=False):
        result = subprocess.run([str(binary), scenario], text=True, capture_output=True, timeout=20,
                                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
        if fails:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('multiplier.GetValue(&heal) == expected', result.stderr)
        else:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('Passed ' + scenario, result.stdout)

    def test_all_uint8_threshold_pairs_and_action_classes(self):
        self.execute(self.binary, 'boundaries')

    def test_recorded_inputs_and_remaining_healing_limitations(self):
        self.execute(self.binary, 'limitations')

    def test_boss_hold_twins_exemptions_and_group_guards_unchanged(self):
        self.execute(self.binary, 'guards')

    def test_old_threshold_reproduced_and_rejected(self):
        old = self.compile(self.old, 'old')
        self.execute(old, 'old')
        self.execute(old, 'boundaries', fails=True)

    def test_off_by_one_and_unrequested_current_target_change_rejected(self):
        for name, before, after in [('boundary', 'threat >= 90', 'threat >= 91'),
                                    ('current', 'threat >= 80', 'threat >= 90')]:
            mutant = self.compile(self.source.replace(before, after), 'mutant-' + name)
            self.execute(mutant, 'boundaries', fails=True)

    def test_production_diff_is_only_requested_threshold_and_comment(self):
        restored = self.source.replace(COMMENT, '').replace('threat >= 90', 'threat >= 50')
        self.assertEqual(restored, self.old)


if __name__ == '__main__':
    unittest.main()
