"""Production-body regression for off-tank raid threat references; no server operations."""
from pathlib import Path
import os
import resource
import subprocess
import tempfile
import unittest
from scripts.tests.test_dead_bot_loot import block

ROOT = Path(__file__).resolve().parents[2]
PB = ROOT / 'azerothcore-wotlk/modules/mod-playerbots'
PATH = 'src/Ai/Base/Util/RaidThreatUtils.cpp'
BEFORE = 'e04c1b785c40325c0d6d557cd002362871d9ddeb'

class RaidThreatOwnerTests(unittest.TestCase):
    def fixture(self, old=False):
        source = (PB/PATH).read_text()
        selection = block(source, 'Unit* GetHighestThreatTank(')
        if old:
            source = subprocess.check_output(['git','-C',str(PB),'show',f'{BEFORE}:{PATH}'],text=True)
        production = selection + '\n' + block(source,'bool ShouldHoldDamageOnTauntImmuneBoss(')
        text = (ROOT/'scripts/tests/cpp/RaidThreatOwnerTest.cpp').read_text().replace('// PRODUCTION',production)
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory); (d/'test.cpp').write_text(text)
            subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-fno-omit-frame-pointer','-no-pie',str(d/'test.cpp'),'-o',str(d/'test')],check=True)
            result=subprocess.run([str(d/'test')],capture_output=True,text=True,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1'),
                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
            if old:
                self.assertNotEqual(result.returncode,0)
                self.assertIn('!ShouldHoldDamageOnTauntImmuneBoss',result.stderr)
            else:
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_offtank_and_safety(self): self.fixture()
    def test_original_mt_only_gate_fails(self): self.fixture(True)
    def test_pull_authority_and_damage_stop_unchanged(self):
        before=subprocess.check_output(['git','-C',str(PB),'show',f'{BEFORE}:{PATH}'],text=True)
        after=(PB/PATH).read_text()
        for signature in ('Unit* GetMainTankTarget(', 'void StopDirectDamage(', 'bool IsTauntImmuneRaidBoss('):
            self.assertEqual(block(before,signature),block(after,signature))

if __name__=='__main__':unittest.main()
