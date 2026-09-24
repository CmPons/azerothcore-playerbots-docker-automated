"""Offline production-method support scaling; no server, DB, build or config operations."""
from pathlib import Path
import subprocess
import unittest

from scripts.tests import test_twins_bug_scaling as base

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk/src/server/game"
MODULE = ROOT / "modules/mod-raid-scaling/src"


def harness(legacy=False):
    text = (ROOT / "scripts/tests/cpp/RaidSupportScalingTest.cpp").read_text()
    header = (MODULE / "RaidScalingSupport.h").read_text().replace('#include "SpellInfo.h"', '')
    manager = base.block((MODULE / "RaidScalingMgr.cpp").read_text(),
                         "float RaidScalingMgr::GetSupportScale(")
    loader = (MODULE / "RaidScalingLoader.cpp").read_text()
    methods = '\n'.join(base.block(loader, signature).replace(' override', '').replace(
        'AuraEffect const*', 'SupportAuraEffect const*') for signature in
        ('void ModifyHealReceived(', 'void OnAuraEffectCalculateAmount('))
    pipeline = base.block((CORE / "Entities/Unit/Unit.cpp").read_text(), 'int32 Unit::HealBySpell(')
    pipeline = pipeline.replace('Unit::HealBySpell', 'SupportPipeline::HealBySpell').replace(
        'Unit::DealHeal', 'SupportPipeline::DealHeal')
    if legacy:
        pipeline = pipeline.replace('healInfo.GetTarget(), healInfo.GetHealer(), heal,',
                                    'this, healInfo.GetTarget(), heal,')
    for key, value in [('SUPPORT_HEADER', header), ('SUPPORT_MANAGER', manager),
                       ('SUPPORT_METHODS', methods), ('HEAL_PIPELINE', pipeline)]:
        text = text.replace('/* ' + key + ' */', value)
    return text


class RaidSupportScalingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.extra_harness = harness()
        base.TwinsBugScalingTests.setUpClass.__func__(cls)

    def test_direct_hot_recipient_multiplier_absorption_overheal_logs_and_bounds(self):
        self.assertIn('Passed support heals', base.run([str(self.binary), 'heals']))

    def test_finite_shields_refresh_stacks_sentinels_and_area_exclusions(self):
        self.assertIn('Passed support shields', base.run([str(self.binary), 'shields']))

    def test_percentage_maxhealth_leech_and_mixed_heals_not_double_scaled(self):
        self.assertIn('Passed support percent', base.run([str(self.binary), 'percent']))

    def test_enemy_only_scope_pets_charms_players_friendly_maps_manual_off(self):
        self.assertIn('Passed support scope', base.run([str(self.binary), 'scope']))

    def test_historical_reversed_direct_heal_arguments_fail_after_successful_compile(self):
        class Legacy(unittest.TestCase):
            extra_harness = harness(legacy=True)
        try:
            base.TwinsBugScalingTests.setUpClass.__func__(Legacy)
            result = subprocess.run([str(Legacy.binary), 'heals'], capture_output=True, text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Assertion', result.stderr)
        finally:
            Legacy.doClassCleanups()

    def test_native_hooks_order_and_module_mirror(self):
        aura = (CORE / 'Spells/Auras/SpellAuraEffects.cpp').read_text()
        calc = base.block(aura, 'int32 AuraEffect::CalculateAmount(')
        self.assertLess(calc.index('CallScriptEffectCalcAmountHandlers'), calc.index('OnAuraEffectCalculateAmount'))
        self.assertLess(calc.index('amount *= GetBase()->GetStackAmount()'), calc.index('OnAuraEffectCalculateAmount'))
        change = base.block(aura, 'void AuraEffect::ChangeAmount(')
        self.assertNotIn('OnAuraEffectCalculateAmount', change)
        periodic = base.block(aura, 'void AuraEffect::HandlePeriodicHealAurasTick(')
        self.assertEqual(periodic.count('ModifyHealReceived(target, caster, heal, GetSpellInfo())'), 1)
        self.assertLess(periodic.index('ModifyHealReceived'), periodic.index('Unit::CalcHealAbsorb'))
        self.assertLess(periodic.index('ModifyHealReceived'), periodic.index('Unit::DealHeal'))
        script = (CORE / 'Scripting/ScriptDefines/UnitScript.cpp').read_text()
        self.assertIn('UNITHOOK_ON_AURA_EFFECT_CALCULATE_AMOUNT', script)
        self.assertIn('script->OnAuraEffectCalculateAmount(effect, caster, amount)', script)
        for name in ('RaidScalingSupport.h', 'RaidScalingMgr.cpp', 'RaidScalingMgr.h', 'RaidScalingLoader.cpp'):
            self.assertEqual((MODULE / name).read_bytes(),
                             (ROOT / 'azerothcore-wotlk/modules/mod-raid-scaling/src' / name).read_bytes())


if __name__ == '__main__':
    unittest.main()
