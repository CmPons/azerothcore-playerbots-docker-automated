"""Kael MC: production scaling hook, loot approach, hand pairing and rescue adapters."""
from pathlib import Path
import json
import os
import resource
import subprocess
import tempfile
import unittest

from scripts.tests.test_dead_bot_loot import block

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / 'azerothcore-wotlk'
PB = CORE / 'modules/mod-playerbots'
TK = 'src/Ai/Raid/TK/'
BEFORE = '68556d789ee3420ff0eadaca11d5e9b03828f473'


def source(path, old=False):
    if old:
        return subprocess.check_output(['git', '-C', str(PB), 'show', f'{BEFORE}:{path}'], text=True)
    return (PB / path).read_text()


def rescue_fixture(old=False, mutant=None):
    text = (ROOT / 'scripts/tests/cpp/TKMindControlTest.cpp').read_text()
    classes = [block(source(TK+'TKActions.h', old), 'class '+name+' :', True) for name in (
        'KaelthasSunstriderBreakMindControlAction', 'KaelthasSunstriderLootLegendaryWeaponsAction')]
    classes.append(block(source(TK+'TKTriggers.h', old),
                         'class KaelthasSunstriderRaidMemberIsMindControlledTrigger :', True))
    helper = source(TK+'Util/TKMindControl.cpp')
    helper = helper[helper.index('namespace TempestKeepHelpers'):]
    methods = [block(source(TK+'TKActions.cpp', old), sig) for sig in (
        'bool KaelthasSunstriderBreakMindControlAction::Execute(',
        'bool KaelthasSunstriderLootLegendaryWeaponsAction::Execute(',
        'bool KaelthasSunstriderLootLegendaryWeaponsAction::LootWeapon(')]
    methods.append(block(source(TK+'TKTriggers.cpp', old),
                         'bool KaelthasSunstriderRaidMemberIsMindControlledTrigger::IsActive('))
    text = text.replace('// CLASSES', '\n'.join(classes)).replace('// HELPERS', helper)
    text = text.replace('// METHODS', '\n'.join(methods))
    if mutant == 'wrong-hand':
        text = text.replace('if (!HasReadyInfinityBlade(botAI))', 'if (false)')
    if mutant == 'loot-radius':
        text = text.replace('std::min(sPlayerbotAIConfig.lootDistance, INTERACTION_DISTANCE - 2.0f)',
                            'sPlayerbotAIConfig.lootDistance')
    if mutant == 'boss-threat':
        text = text.replace('class KaelthasSunstriderBreakMindControlAction : public MovementAction',
                            'class KaelthasSunstriderBreakMindControlAction : public AttackAction')
        text = text.replace('"kael\'thas sunstrider break mind control") : MovementAction',
                            '"kael\'thas sunstrider break mind control") : AttackAction')
    return text


def scaling_fixture():
    text = (ROOT / 'scripts/tests/cpp/TKMindControlScalingTest.cpp').read_text()
    loader = (ROOT / 'modules/mod-raid-scaling/src/RaidScalingLoader.cpp').read_text()
    return text.replace('// PRODUCTION', block(loader, 'class RaidScalingMindControlScript :', True))


def proc_fixture():
    text = (ROOT / 'scripts/tests/cpp/TKInfinityBladeProcTest.cpp').read_text()
    data = json.loads((ROOT / 'scripts/tests/fixtures/tk-mind-control/spells.json').read_text())
    assert data['mind_control']['dispel_type'] == data['disruption']['misc_0'] == 8
    assert data['disruption']['effect_0'] == 38 and data['disruption']['basepoints_0'] + 1 == 1
    text = text.replace('ProcChance=101', 'ProcChance='+str(data['disruption']['proc_chance']))
    text = text.replace('SpellPPMRate=60', 'SpellPPMRate='+str(data['blade']['ppm']))
    player = (CORE / 'src/server/game/Entities/Player/Player.cpp').read_text()
    dispatch = block(player, 'void Player::CastItemCombatSpell(Unit* target, WeaponAttackType attType, uint32 procVictim, uint32 procEx)')
    intrinsic = block(player, 'void Player::CastItemCombatSpell(Unit* target, WeaponAttackType attType, uint32 procVictim, uint32 procEx, Item*')
    # Only the built-in item-proc prefix is under test, not unrelated enchantment procs.
    intrinsic = intrinsic[:intrinsic.index('    // item combat enchantments')] + '}'
    intrinsic = intrinsic.replace('{', '{\n    (void)procEx;', 1)
    dispel = block((CORE / 'src/server/game/Spells/SpellEffects.cpp').read_text(), 'void Spell::EffectDispel(')
    return text.replace('// PRODUCTION', '\n'.join([dispatch, intrinsic, dispel]))


class TKMindControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='tk-mc-')
        cls.path = Path(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_fixture(self, text, name, scenario='all', sanitize=False):
        cpp, exe = self.path/(name+'.cpp'), self.path/name
        cpp.write_text(text)
        flags = ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-g']
        if sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
        result = subprocess.run(['g++', *flags, str(cpp), '-o', str(exe)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        return subprocess.run([str(exe), scenario], text=True, capture_output=True,
                              env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1'),
                              preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))

    def test_production_scaling_scope(self):
        r = self.run_fixture(scaling_fixture(), 'scaling')
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)

    def test_production_rescue(self):
        r = self.run_fixture(rescue_fixture(), 'rescue')
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)

    def test_native_item_proc_and_dispel(self):
        r = self.run_fixture(proc_fixture(), 'native-proc')
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)

    def test_sanitizers(self):
        for name, fixture in [('scaling-asan', scaling_fixture()), ('rescue-asan', rescue_fixture()),
                              ('proc-asan', proc_fixture())]:
            r = self.run_fixture(fixture, name, sanitize=True)
            self.assertEqual(r.returncode, 0, r.stdout+r.stderr)

    def test_old_loot_radius_fails(self):
        r = self.run_fixture(rescue_fixture(True), 'old-loot', 'loot')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('s.loot.Execute', r.stderr)

    def test_old_unusable_dagger_still_casts(self):
        r = self.run_fixture(rescue_fixture(True), 'old-hand', 'hand')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('!s.rescue.Execute', r.stderr)

    def test_old_threat_only_discovery_fails(self):
        r = self.run_fixture(rescue_fixture(True), 'old-discovery', 'discovery')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('s.trigger.IsActive', r.stderr)

    def test_mutants(self):
        for mutant, scenario in [('wrong-hand', 'hand'), ('loot-radius', 'loot'), ('boss-threat', 'all')]:
            r = self.run_fixture(rescue_fixture(mutant=mutant), mutant, scenario)
            self.assertNotEqual(r.returncode, 0, mutant)

    def test_spell_data_contract(self):
        data = json.loads((ROOT / 'scripts/tests/fixtures/tk-mind-control/spells.json').read_text())
        self.assertEqual(data['mind_control']['duration_ms'], 30000)
        for key in ['wing_clip', 'shiv_triggered']:
            self.assertEqual(data[key]['damage_class'], 2)
            self.assertFalse(data[key]['attributes'] & 0x00100000)  # cancels auto attack
            self.assertFalse(data[key]['attributes_ex4'] & 0x00800000)  # suppress weapon procs
        shiv = (CORE / 'src/server/scripts/Spells/spell_rogue.cpp').read_text()
        self.assertIn('SPELL_ROGUE_SHIV_TRIGGERED                  = 5940', shiv)
        self.assertIn('caster->CastSpell(unitTarget, SPELL_ROGUE_SHIV_TRIGGERED, true)', shiv)

    def test_native_contracts_and_registration(self):
        spell = (CORE / 'src/server/game/Spells/Spell.cpp').read_text()
        prepare = block(spell, 'SpellCastResult Spell::prepare(')
        self.assertLess(prepare.index('CheckCast(true)'), prepare.index('cast(true)'))
        # OnSpellPrepare is too late for instant spells. The selected check-cast hook is before casting.
        self.assertGreater(prepare.index('sScriptMgr->OnSpellPrepare'), prepare.index('cast(true)'))
        check = block(spell, 'SpellCastResult Spell::CheckCast(')
        self.assertIn('sScriptMgr->OnSpellCheckCast(this, strict, res)', check)
        self.assertIn('procVictim | PROC_FLAG_TAKEN_DAMAGE', spell)  # landed non-damaging melee abilities
        loader = (ROOT / 'modules/mod-raid-scaling/src/RaidScalingLoader.cpp').read_text()
        self.assertIn('new RaidScalingMindControlScript();', loader)
        self.assertIn('ALLSPELLHOOK_ON_SPELL_CHECK_CAST', loader)
        self.assertEqual(loader, (CORE / 'modules/mod-raid-scaling/src/RaidScalingLoader.cpp').read_text())
        self.assertIn('command == "tkmc"', source('src/Bot/PlayerbotAI.cpp'))
        for path in ['TKStrategy.cpp', 'TKMultipliers.cpp']:
            self.assertEqual(source(TK+path), source(TK+path, True))
        for sig in ['bool KaelthasSunstriderHandlePhoenixesAndEggsAction::Execute(',
                    'bool KaelthasSunstriderLootLegendaryWeaponsAction::ShouldBotLootWeapon(',
                    'bool KaelthasSunstriderReequipGearAction::Execute(']:
            self.assertEqual(block(source(TK+'TKActions.cpp'), sig), block(source(TK+'TKActions.cpp', True), sig))
        self.assertIn('SPELLVALUE_MAX_TARGETS, 3',
                      (CORE / 'src/server/scripts/Outland/TempestKeep/Eye/boss_kaelthas.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
