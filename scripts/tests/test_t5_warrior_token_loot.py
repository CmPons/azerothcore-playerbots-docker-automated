"""Read-only T5 audit regression: actual conversion/scoring code, frozen installed item data."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from scripts.tests import test_direct_token_loot as common

ROOT = common.ROOT
DATA = ROOT / 'scripts/tests/fixtures/direct-token-loot/t5-items.json'

class T5WarriorTokenTests(unittest.TestCase):
    def test_production_t5_warrior_rewards(self):
        source = (ROOT / 'scripts/tests/cpp/DirectTokenLootTest.cpp').read_text().split('void SelectionTests()')[0]
        replacements = {
            '// PRODUCTION_ENUMS': common.native_enums(),
            '// PRODUCTION_SCORER_HEADER': common.no_includes((common.ITEM / 'StatsWeightCalculator.h').read_text()),
            '// PRODUCTION_SCORER': common.scorer_bodies(),
            '// PRODUCTION_CANDIDATE_HEADER': common.method((common.ITEM / 'TokenItemResolver.h').read_text(), 'struct TokenRewardCandidate') + ';',
            '// PRODUCTION_LOOT': common.no_includes((common.ITEM / 'DirectTokenLootScript.cpp').read_text()),
            '// HISTORICAL_POOL': '',
            '// FIXTURE_DATA': common.fixture_data(DATA),
        }
        for marker, value in replacements.items():
            source = source.replace(marker, value)
        source += (ROOT / 'scripts/tests/cpp/T5WarriorTokenTest.cpp').read_text()
        with tempfile.TemporaryDirectory(prefix='t5-warrior-') as tmp:
            cpp, binary = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            cpp.write_text(source)
            subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-implicit-fallthrough',
                '-Wno-unused-function', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)

if __name__ == '__main__':
    unittest.main()
