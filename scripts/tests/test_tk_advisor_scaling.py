"""Native advisor health aura + scoped scaling regression; never touches a running server."""
import json
import math
import re
from pathlib import Path
import resource
import subprocess
import unittest

from scripts.tests import test_twins_bug_scaling as shared

ROOT = shared.ROOT
MGR = ROOT / 'modules/mod-raid-scaling/src/RaidScalingMgr.cpp'
BOSS = shared.CORE / 'src/server/scripts/Outland/TempestKeep/Eye/boss_kaelthas.cpp'
BASELINE = '77aca5ab0adaa718322da0c36c5dbeeec0c197f1'
DATA = json.loads((ROOT / 'scripts/tests/fixtures/raid-scaling/tk-advisors.json').read_text())


def transform(harness):
    def replace(old, new):
        nonlocal harness
        assert harness.count(old) == 1, old
        harness = harness.replace(old, new)
    replace('    bool IsPlayer() const { return player; }',
            '    virtual uint32 GetEntry() const { return 0; }\n    bool IsPlayer() const { return player; }')
    replace('    void UpdateMaxHealth() override;', '''    void UpdateMaxHealth() override;
    uint32 removedAura = 0, standState = 1, reactState = 0;
    bool stopped = false, zoneCombat = false;
    void SetFullHealth() { SetHealth(GetMaxHealth()); }
    void RemoveAurasDueToSpell(uint32 id) { removedAura = id; }
    void SetStandState(uint32 state) { standState = state; }
    void RemoveUnitFlag(uint32 flag) { flags &= ~flag; }
    void SetUnitFlag(uint32 flag) { flags |= flag; }
    void SetInCombatWithZone() { zoneCombat = true; }
    void SetReactState(uint32 state) { reactState = state; }
    void AttackStop() { stopped = true; }''')
    replace('    int GetAmount() const { return 300; } // unchanged spell 802 health percentage',
            '    int amount = 300; // Default retains the existing Twins mutation fixture.\n    int GetAmount() const { return amount; }')
    boss = BOSS.read_text()
    body = shared.block(boss, 'struct advisor_baseAI :')
    methods = []
    for signature in ['void DamageTaken(', 'void SpellHit(']:
        method = shared.block(body, signature).replace(' override', '')
        method = method.replace(signature, 'void advisor_baseAI::' + signature.removeprefix('void '), 1)
        methods.append(method)
    extra = (ROOT / 'scripts/tests/cpp/TKAdvisorScalingTest.cpp').read_text()
    declarations, scenarios = extra.split('// SCENARIOS\n')
    declarations = declarations.replace('/* ADVISOR_METHODS */', '\n\n'.join(methods))
    records = ',\n'.join('    {' + ', '.join(str(r[k]) for k in ('entry', 'nativeHp', 'scaledHp', 'revivedScaledHp')) + '}' for r in DATA['advisors'])
    declarations = declarations.replace('/* ADVISOR_DATA */', records)
    # Declarations follow production scaling methods so they can call the real aura handler.
    harness = harness.replace('int main(', 'int TwinsScalingUnusedMain(')
    return harness + '\n' + declarations + '\n' + scenarios


class TKAdvisorScalingTests(unittest.TestCase):
    transform_harness = staticmethod(transform)

    @classmethod
    def setUpClass(cls):
        shared.TwinsBugScalingTests.setUpClass.__func__(cls)

    def scenario(self, name):
        self.assertIn('Passed ' + name, shared.run([str(self.binary), name]))

    def test_first_phase_fake_death_native_resurrection_and_second_death(self):
        self.scenario('resurrection')

    def test_reapplication_retains_buff_and_injured_health_fraction(self):
        self.scenario('reapply')

    def test_manual_multiplier_disable_and_reenable_with_buff_active(self):
        self.scenario('settings')

    def test_twenty_respawns_and_fresh_same_guid_baselines(self):
        self.scenario('respawn')

    def test_scope_and_existing_eligibility_guards(self):
        self.scenario('scope')

    def test_first_application_while_buff_active_and_no_cross_instance_state(self):
        self.scenario('late')

    def test_old_code_reproduces_inflation_and_manual_buff_clobber(self):
        old = subprocess.check_output(['git', 'show', BASELINE+':modules/mod-raid-scaling/src/RaidScalingMgr.cpp'], cwd=ROOT, text=True)
        class Old(unittest.TestCase):
            source_override = old
            transform_harness = staticmethod(transform)
        try:
            shared.TwinsBugScalingTests.setUpClass.__func__(Old)
            self.assertIn('Passed old', shared.run([str(Old.binary), 'old']))
            failed = subprocess.run([str(Old.binary), 'resurrection'], capture_output=True, text=True, timeout=15,
                                    preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn('advisor.GetMaxHealth() == row.revivedScaled', failed.stderr)
        finally:
            Old.doClassCleanups()

    def test_restore_base_and_prebuff_gate_mutants(self):
        source = MGR.read_text()
        restore = shared.block(source, 'void RaidScalingMgr::RestoreCreature(')
        broken_restore = source.replace(restore, restore.replace('UsesAuraAwareHealthScaling(creature)', 'IsTwinsEncounterBug(creature)'))
        helper = shared.block(source, 'bool UsesAuraAwareHealthScaling(')
        # Gate on existing TOTAL_PCT to mimic waiting until resurrection to scale its base.
        broken_gate = source.replace(helper, helper.replace('switch (creature->GetEntry())',
            'if (creature->GetPctModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT) == 1) return false;\n        switch (creature->GetEntry())'))
        for name, text, scenario in [('restore', broken_restore, 'settings'), ('gate', broken_gate, 'resurrection')]:
            with self.subTest(name=name):
                self.assertNotEqual(text, source)
                class Mutant(unittest.TestCase):
                    source_override = text
                    transform_harness = staticmethod(transform)
                try:
                    shared.TwinsBugScalingTests.setUpClass.__func__(Mutant)
                    failed = subprocess.run([str(Mutant.binary), scenario], capture_output=True, text=True, timeout=15,
                                            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
                    self.assertNotEqual(failed.returncode, 0)
                    self.assertIn('Assertion', failed.stderr)
                finally:
                    Mutant.doClassCleanups()

    def test_audited_data_and_native_spell_identity(self):
        self.assertEqual(DATA['resurrection'], {'spell': 36450, 'effectIndex': 1,
                                               'aura': 133, 'basePoints': 99, 'amount': 100})
        self.assertEqual(DATA['map'], 550)
        self.assertEqual(DATA['healthScale'], 0.4)
        eye = (BOSS.parent/'the_eye.h').read_text()
        names = ['NPC_LORD_SANGUINAR', 'NPC_CAPERNIAN', 'NPC_TELONICUS', 'NPC_THALADRED']
        for name, row in zip(names, DATA['advisors']):
            self.assertRegex(eye, rf'{name}\s*=\s*{row["entry"]}\b')
            self.assertEqual(math.ceil(row['baseHp1'] * row['healthModifier']), row['nativeHp'])
            self.assertEqual(int(row['nativeHp'] * 0.4), row['scaledHp'])
            self.assertEqual(int(row['nativeHp'] * 0.4 * 2), row['revivedScaledHp'])
        self.assertRegex(BOSS.read_text(), r'SPELL_RESURRECTION\s*=\s*36450\b')
        self.assertIn('DoCastSelf(SPELL_RESURRECTION)', BOSS.read_text())

    def test_mirrors_and_no_encounter_or_damage_changes(self):
        self.assertEqual(MGR.read_bytes(), (shared.CORE/'modules/mod-raid-scaling/src/RaidScalingMgr.cpp').read_bytes())
        before = subprocess.check_output(['git', 'show', BASELINE+':modules/mod-raid-scaling/src/RaidScalingMgr.cpp'], cwd=ROOT, text=True)
        for signature in ['bool IsTwinsEncounterBug(', 'bool RaidScalingMgr::IsScalableCreature(',
                          'float RaidScalingMgr::GetDamageScale(', 'void RaidScalingMgr::OnCreatureRespawn(']:
            self.assertEqual(shared.block(before, signature), shared.block(MGR.read_text(), signature))
        native = subprocess.check_output(['git', 'show', '7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72:src/server/scripts/Outland/TempestKeep/Eye/boss_kaelthas.cpp'], cwd=shared.CORE)
        self.assertEqual(native, BOSS.read_bytes())
        helper = shared.block(MGR.read_text(), 'bool UsesAuraAwareHealthScaling(')
        self.assertNotIn('HasAura', helper)
        self.assertNotIn('GetPhase', helper)
        self.assertNotIn('GetBossState', helper)


if __name__ == '__main__':
    unittest.main()
