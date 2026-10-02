"""In-place respawn scaling regression; no full server build or live mutations."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from scripts.tests import test_twins_bug_scaling as shared

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / 'azerothcore-wotlk/src/server/game'
MODULE = ROOT / 'modules/mod-raid-scaling/src'
HARNESS = (ROOT / 'scripts/tests/cpp/RaidRespawnScalingTest.cpp').read_text()


def harness(loader):
    return HARNESS.replace('/* LOADER */', shared.block(loader, 'class RaidScalingCreatureScript') + ';')


class RaidRespawnScalingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.extra_harness = harness((MODULE / 'RaidScalingLoader.cpp').read_text())
        shared.TwinsBugScalingTests.setUpClass.__func__(cls)

    def test_twenty_in_place_cycles_no_compounding_or_previous_life_hp(self):
        self.assertIn('Passed cycles', shared.run([str(self.binary), 'cycles']))

    def test_new_native_baseline_and_ai_starting_fraction(self):
        self.assertIn('Passed baseline', shared.run([str(self.binary), 'baseline']))

    def test_manual_off_disabled_reenable_preserve_settings(self):
        self.assertIn('Passed settings', shared.run([str(self.binary), 'settings']))

    def test_inactive_nonraid_noninstance_player_origin_and_dead_exclusions(self):
        self.assertIn('Passed isolation', shared.run([str(self.binary), 'isolation']))

    def test_dynamic_recreation_still_scales_once_via_world_entry(self):
        self.assertIn('Passed dynamic', shared.run([str(self.binary), 'dynamic']))

    def test_twins_mutation_health_aura_survives_fresh_baseline_capture(self):
        self.assertIn('Passed mutation-respawn', shared.run([str(self.binary), 'mutation-respawn']))

    def test_reapply_preserves_current_hp_at_scaled_native_and_increased_maximum(self):
        self.assertIn('Passed percent', shared.run([str(self.binary), 'percent']))

    def test_old_loader_and_stale_cache_mutants_fail_at_runtime(self):
        source = (MODULE / 'RaidScalingMgr.cpp').read_text()
        loader = (MODULE / 'RaidScalingLoader.cpp').read_text()
        # Exact published pre-fix loader, not a fabricated old implementation.
        old_loader = subprocess.check_output([
            'git', 'show', '1b161c27d646ea83cdf7bdb0885d0f8c8c305127:modules/mod-raid-scaling/src/RaidScalingLoader.cpp'
        ], cwd=ROOT, text=True)
        stale = source.replace('mapItr->second.erase(creature->GetGUID());', '(void)creature;')
        self.assertNotEqual(stale, source)
        old_mgr = subprocess.check_output([
            'git', 'show', '1b161c27d646ea83cdf7bdb0885d0f8c8c305127:modules/mod-raid-scaling/src/RaidScalingMgr.cpp'
        ], cwd=ROOT, text=True)
        old_percent = source.replace(shared.block(source, 'void RaidScalingMgr::ApplyToCreature('),
                                     shared.block(old_mgr, 'void RaidScalingMgr::ApplyToCreature('))
        self.assertNotEqual(old_percent, source)
        for name, code, hook, scenario in [('old-loader', source, old_loader, 'cycles'),
                                           ('stale-baseline', stale, loader, 'baseline'),
                                           ('old-cached-hp', old_percent, loader, 'percent')]:
            with self.subTest(name=name):
                class Mutant(unittest.TestCase):
                    source_override = code
                    extra_harness = harness(hook)
                try:
                    shared.TwinsBugScalingTests.setUpClass.__func__(Mutant)
                    result = subprocess.run([str(Mutant.binary), scenario], capture_output=True, text=True, timeout=15)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('Assertion', result.stderr)
                    print(f'Detected {name}: {result.stderr.strip()}')
                finally:
                    Mutant.doClassCleanups()

    def test_native_hook_is_after_rebuild_alive_ai_reset_and_before_visibility(self):
        source = (CORE / 'Entities/Creature/Creature.cpp').read_text()
        body = shared.block(source, 'void Creature::Respawn(bool force)')
        self.assertEqual(body.count('OnCreatureRespawn(this)'), 1)
        compat = shared.block(body, 'if (_respawnCompatibilityMode)')
        successful = shared.block(compat, 'if (getDeathState() == DeathState::Dead)')
        for call in ('SelectLevel();', 'setDeathState(DeathState::JustRespawned);',
                     'AI()->Reset();', 'InitializeReactState();'):
            self.assertLess(successful.index(call), successful.index('OnCreatureRespawn(this)'))
        self.assertLess(body.index('OnCreatureRespawn(this)'), body.index('UpdateObjectVisibility(false)'))
        self.assertIn('AddObjectToRemoveList();', body)  # Dynamic path left native.
        native_stats = shared.block(source, 'void Creature::SelectLevel(')
        self.assertIn('SetMaxHealth(health);', native_stats)
        dispatch = shared.block((CORE / 'Scripting/ScriptDefines/AllCreatureScript.cpp').read_text(),
                                'void ScriptMgr::OnCreatureRespawn(')
        self.assertIn('ExecuteScript<AllCreatureScript>', dispatch)
        self.assertIn('script->OnCreatureRespawn(creature);', dispatch)
        self.assertIn('virtual void OnCreatureRespawn(Creature* /*creature*/) { }',
                      (CORE / 'Scripting/ScriptDefines/AllCreatureScript.h').read_text())
        mgr = shared.block((MODULE / 'RaidScalingMgr.cpp').read_text(), 'void RaidScalingMgr::OnCreatureRespawn(')
        for forbidden in ('EnableForMap', '_state.Set', '_state.Initialize', 'SetBossState', 'RespawnTime',
                          '21964', '21965', '21966'):
            self.assertNotIn(forbidden, mgr)

    def test_complete_native_respawn_body_hook_branch_execution_and_old_source_red(self):
        current = (CORE / 'Entities/Creature/Creature.cpp').read_text()
        old = subprocess.check_output([
            'git', 'show', 'ab00bd5c78071f80a69c06d06ac2bb33b6bbd880:src/server/game/Entities/Creature/Creature.cpp'
        ], cwd=ROOT / 'azerothcore-wotlk', text=True)
        fixture = (ROOT / 'scripts/tests/cpp/CreatureRespawnHookTest.cpp').read_text()
        with tempfile.TemporaryDirectory(prefix='native-respawn-hook-') as directory:
            for label, source in [('current', current), ('old', old)]:
                cpp = Path(directory) / (label + '.cpp')
                binary = Path(directory) / label
                cpp.write_text(fixture.replace('/* PRODUCTION */', shared.block(source, 'void Creature::Respawn(bool force)')))
                shared.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-sanitize-recover=all', str(cpp), '-o', str(binary)])
                result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
                if label == 'current':
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn('10 branches', result.stdout)
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('Assertion', result.stderr)
                    print('Detected pre-fix native respawn: ' + result.stderr.strip())

    def test_module_mirrors_match(self):
        for name in ('RaidScalingMgr.cpp', 'RaidScalingMgr.h', 'RaidScalingLoader.cpp'):
            self.assertEqual((MODULE / name).read_bytes(),
                             (ROOT / 'azerothcore-wotlk/modules/mod-raid-scaling/src' / name).read_bytes())


if __name__ == '__main__':
    unittest.main()
