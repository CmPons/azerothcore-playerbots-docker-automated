"""Source-bound shared cast execution/admission tests; core spell checks are injected doubles."""
from pathlib import Path
import os,re,resource,subprocess,tempfile,unittest

def block(text, signature):
 """Ignore braces in comments/strings, retaining the exact original function bytes."""
 start=text.index(signature); opening=text.index('{',start); depth=0
 tokens=re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',re.S)
 for match in tokens.finditer(text,opening):
  token=match.group()
  if token=='{':depth+=1
  elif token=='}':
   depth-=1
   if not depth:return text[start:match.end()]
 raise AssertionError('Unclosed function '+signature)
ROOT=Path(__file__).resolve().parents[2]
PB=ROOT/'azerothcore-wotlk/modules/mod-playerbots'
BEFORE='fa9f3eb52f3fbf1aa069773061cc00dcd1ebec78'
FILE='src/Bot/PlayerbotAI.cpp'
SIGNATURES=('bool PlayerbotAI::CanCastSpell(uint32 spellid, Unit*',
 'bool PlayerbotAI::CanCastSpell(uint32 spellid, float',
 'bool PlayerbotAI::CastSpell(uint32 spellId, Unit*',
 'bool PlayerbotAI::CastSpell(uint32 spellId, float')
class FlightCastingTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory(prefix='flight-casting-');cls.addClassCleanup(cls.tmp.cleanup)
  cls.path=Path(cls.tmp.name)
 def compile(self,old=False,mutant=None):
  source=subprocess.check_output(['git','-C',str(PB),'show',BEFORE+':'+FILE],text=True) if old else (PB/FILE).read_text()
  body='\n'.join(block(source,s) for s in SIGNATURES)
  helpers='\n'.join(l for l in (PB/'src/Bot/PlayerbotSpellCasting.cpp').read_text().splitlines() if not l.startswith('#include'))
  if mutant=='no-execution-recheck':
   body=body.replace('!IsSpellFlightAllowed(spellId, target, ai::casting::Stage::Execute)','false').replace('!IsSpellFlightAllowed(spellId, nullptr, ai::casting::Stage::Execute, &destination)','false')
  if mutant=='ignore-ground-rejection':
   # Only the position-targeted method uses a const local result.
   body=body.replace('SpellCastResult const result = spell->prepare(&targets);','spell->prepare(&targets);\n    SpellCastResult const result = SPELL_CAST_OK;')
  fixture=(ROOT/'scripts/tests/cpp/FlightCastingTest.cpp').read_text().replace('// HELPERS',helpers).replace('// PRODUCTION',body)
  unit=(ROOT/'azerothcore-wotlk/src/server/game/Entities/Unit/Unit.h').read_text()
  definitions=(ROOT/'azerothcore-wotlk/src/server/game/Entities/Unit/UnitDefines.h').read_text()
  constants=[]
  for flag in ('MOVEMENTFLAG_FLYING','MOVEMENTFLAG_DISABLE_GRAVITY'):
   value=re.search(r'\b'+flag+r'\s*=\s*(0x[0-9a-fA-F]+)',definitions)[1]
   constants.append('constexpr uint32 '+flag+'='+value+';')
  aura=(ROOT/'azerothcore-wotlk/src/server/game/Spells/Auras/SpellAuraDefines.h').read_text()
  constants.append('constexpr int SPELL_AURA_FLY='+re.search(r'\bSPELL_AURA_FLY\s*=\s*(\d+)',aura)[1]+';')
  fixture=fixture.replace('// FLIGHT_CONSTANTS','\n'.join(constants)).replace('// IS_FLYING',block(unit,'bool IsFlying() const'))
  fixture=fixture.replace('// HAS_FLY_AURA',block(unit,'bool HasFlyAura()'))
  name=mutant or ('old' if old else 'current');cpp=self.path/(name+'.cpp');exe=self.path/name;cpp.write_text(fixture)
  p=subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined',
   '-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie','-I'+str(PB/'src/Bot'),str(cpp),'-o',str(exe)],capture_output=True,text=True)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr)
  return exe
 def run_case(self,exe,case='all',red=False):
  p=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=30,
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1'),preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  if red:
   self.assertNotEqual(p.returncode,0);self.assertIn('Assertion',p.stderr)
  else:self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def test_production_paths_and_diagnostics(self):self.run_case(self.compile())
 def test_original_flying_rejection(self):self.run_case(self.compile(old=True),red=True)
 def test_original_ground_false_success(self):self.run_case(self.compile(old=True),'ground-reject',red=True)
 def test_execution_recheck_mutant(self):self.run_case(self.compile(mutant='no-execution-recheck'),red=True)
 def test_ground_rejection_mutant(self):self.run_case(self.compile(mutant='ignore-ground-rejection'),'ground-reject',red=True)
 def test_scope_and_untouched_advisor_movement_threat(self):
  for path in ('src/Ai/Raid/TK/TKActions.cpp','src/Ai/Raid/TK/TKMultipliers.cpp','src/Ai/Raid/TK/TKTriggers.cpp',
   'src/Ai/Base/Actions/MovementActions.cpp','src/Ai/Base/Strategy/ThreatStrategy.cpp','src/Ai/Base/Util/RaidThreatUtils.cpp'):
   self.assertEqual((PB/path).read_text(),subprocess.check_output(['git','-C',str(PB),'show',BEFORE+':'+path],text=True))
  source=(PB/FILE).read_text()
  self.assertIn('if (command == "castfail")\n        return DescribeSpellCastFailures();',source)
  self.assertEqual(source.count('IsSpellFlightAllowed('),4)
  old=subprocess.check_output(['git','-C',str(PB),'show',BEFORE+':'+FILE],text=True)
  for sig in ('bool PlayerbotAI::CanCastVehicleSpell(','bool PlayerbotAI::CanCastSpell(uint32 spellid, GameObject*'):
   self.assertEqual(block(source,sig),block(old,sig))
if __name__=='__main__':unittest.main()
