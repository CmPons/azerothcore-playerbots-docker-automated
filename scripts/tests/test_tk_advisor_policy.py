"""Native ownership/coexistence adapters; real-Lua coverage is in test_raid_combat."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from scripts.tests.test_dead_bot_loot import block
ROOT=Path(__file__).resolve().parents[2]
PB=ROOT/'azerothcore-wotlk/modules/mod-playerbots'
POLICY=PB/'src/Ai/Raid/Policy'
BEFORE='e04c1b785c40325c0d6d557cd002362871d9ddeb'

class TKAdvisorPolicyTests(unittest.TestCase):
    def test_native_assignment_and_priority_adapters(self):
        actions=(PB/'src/Ai/Raid/TK/TKActions.cpp').read_text()
        method=block(actions,'bool KaelthasSunstriderAssignAdvisorDpsPriorityAction::Execute(')
        prefix=method[method.index('{')+1:method.index('    // Target priority 1:')]
        resolver='\n'.join(x for x in (POLICY/'RaidCombatAssignments.cpp').read_text().splitlines() if not x.startswith('#include'))
        helper=block((PB/'src/Ai/Raid/TK/Util/TKHelpers.cpp').read_text(),'Player* GetCapernianTank(')
        fixture=(ROOT/'scripts/tests/cpp/RaidPolicyAssignmentTest.cpp').read_text()
        fixture=fixture.replace('// RESOLVER',resolver).replace('// CAPERNIAN',helper).replace('// DPS_PREFIX',prefix)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(fixture)
            subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','-no-pie','-I'+str(POLICY),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1'))
    def test_legacy_priority_fallback_and_mechanics_unchanged(self):
        path='src/Ai/Raid/TK/TKActions.cpp'
        old=subprocess.check_output(['git','-C',str(PB),'show',f'{BEFORE}:{path}'],text=True)
        new=(PB/path).read_text()
        marker='    // Target priority 1:'
        sig='bool KaelthasSunstriderAssignAdvisorDpsPriorityAction::Execute('
        a,b=block(old,sig),block(new,sig)
        self.assertEqual(a[a.index(marker):],b[b.index(marker):])
        for sig in ('bool KaelthasSunstriderKiteThaladredAction::Execute(',
                    'bool KaelthasSunstriderSpreadAndMoveAwayFromCapernianAction::Execute(',
                    'bool KaelthasSunstriderHandlePhoenixesAndEggsAction::Execute(',
                    'bool KaelthasSunstriderBreakMindControlAction::Execute(',
                    'bool KaelthasSunstriderFirstAssistTankPositionTelonicusAction::Execute('):
            self.assertEqual(block(old,sig),block(new,sig))

if __name__=='__main__':unittest.main()
