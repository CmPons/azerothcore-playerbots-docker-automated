"""TK weapon recovery: production eligibility, action, trigger and DPS gate offline."""
from pathlib import Path
import os
import resource
import subprocess
import tempfile
import unittest

from scripts.tests.test_dead_bot_loot import fixture as equipment_fixture, block

ROOT = Path(__file__).resolve().parents[2]
PB = ROOT / 'azerothcore-wotlk/modules/mod-playerbots'
BEFORE = 'b63dd8d9cb4f7493c8cd397412ed0f10e06abf2b'
TK = 'src/Ai/Raid/TK/'


def source(path, old=False):
    if old:
        return subprocess.check_output(['git', '-C', str(PB), 'show', f'{BEFORE}:{path}'], text=True)
    return (PB / path).read_text()


def make_fixture(old=False, mutant=None, monday=False):
    # Reuse the equipment fixture's real core/PB validators and full equip valuation.
    text = equipment_fixture(old=monday).split('int main()\n{', 1)[0]
    additions = (ROOT / 'scripts/tests/cpp/TKWeaponRecoveryTest.cpp').read_text()
    before, after = additions.split('// AFTER_EQUIPMENT_METHODS\n')
    timer = (ROOT / 'azerothcore-wotlk/src/common/Utilities/Timer.h').read_text()
    before = before.replace('uint32 getMSTimeDiff(uint32 old, uint32 now) { return now - old; }',
                            block(timer, 'inline uint32 getMSTimeDiff(uint32 oldMSTime, uint32 newMSTime)'))
    text = text.replace('#include <cassert>', '#include <array>\n#include <ctime>\n#include <functional>\n#include <limits>\n#include <cassert>')
    text = text.replace('struct Player;\nstruct Group;', before + '\nstruct Player;\nstruct Group;')
    text = text.replace('bool operator<(Guid const& other)', 'bool operator==(Guid const&) const = default;\n    bool operator<(Guid const& other)')
    text = text.replace('    bool m_lootGenerated = false;', '''    uint8 bag = INVENTORY_SLOT_BAG_0, inventorySlot = 23;
    uint8 GetBagSlot() const { return bag; }
    Guid GetOwnerGUID() const;
    int32 GetItemRandomPropertyId() const { return 0; }
    uint8 GetBagSize() const { return 32; }
    bool m_lootGenerated = false;''')
    text = text.replace('uint8 GetSlot() const { return proto->slot; }', 'uint8 GetSlot() const { return inventorySlot; }')
    text = text.replace('    bool alive = true,', '''    Map* map = &scene;
    float distance = 10.0f;
    uint32 identity = 1297;
    mutable unsigned coreEquipChecks = 0;
    std::map<std::pair<uint8, uint8>, Item*> bags;
    Map* GetMap() const { return map; }
    uint32 GetMapId() const { return map ? map->id : TEMPEST_KEEP_MAP_ID; }
    float GetExactDist2d(Creature*) const { return distance; }
    Bag* GetBagByPos(uint8 slot) const { return GetItemByPos(INVENTORY_SLOT_BAG_0, slot); }
    static bool IsInventoryPos(uint8 bag, uint8 slot) { return bag != INVENTORY_SLOT_BAG_0 || slot >= 23; }
    InventoryResult CanEquipItem(uint8, uint16&, Item*, bool) const;
    bool alive = true,''')
    text = text.replace('Guid GetGUID() const { return {}; }', 'Guid GetGUID() const { return Guid{identity}; }')
    text = text.replace('uint32 getClass() const { return CLASS_SHAMAN; }',
                        'uint32 getClass() const { return CLASS_HUNTER; }')
    text = text.replace('Item* GetItemByPos(uint8, uint8 slot) const\n    {', '''Item* GetItemByPos(uint8 bag, uint8 slot) const
    {
        auto stored = bags.find({bag, slot});
        if (stored != bags.end()) return stored->second;''')
    text = text.replace('    Group* group = nullptr;', '''    Group* group = nullptr;
    Player* GetBot() const { return bot; }
    unsigned equipAttempts = 0;
    std::function<void()> equipEffect{};
    bool DoSpecificAction(std::string const&, Event, bool);
    bool IsMainTank(Player*) const { return false; }
    bool IsAssistTankOfIndex(Player*, uint8, bool) const { return false; }
    Unit* Find(std::string const& name) const
    {
        auto it = targets.find(name); return it == targets.end() ? nullptr : it->second;
    }''')
    text = text.replace('struct PlayerbotAI\n{', 'struct Event;\nstruct PlayerbotAI\n{')
    text = text.replace('uint8 FindEquipSlot(ItemTemplate const* proto, uint32, bool) const { return proto->slot; }', '''uint8 FindEquipSlot(ItemTemplate const* proto, uint32 requested, bool) const
    {
        if (requested == NULL_SLOT) return proto->slot;
        if (requested == proto->slot) return requested;
        if (requested == EQUIPMENT_SLOT_OFFHAND && proto->InventoryType == INVTYPE_WEAPON)
            return requested;
        return NULL_SLOT;
    }''')
    text += '\n' + after
    classes = [block(source(TK+'TKActions.h', old), 'class KaelthasSunstriderReequipGearAction :', True),
               block(source(TK+'TKTriggers.h', old), 'class KaelthasSunstriderLegendaryWeaponsWereLostTrigger :', True)]
    helper = block(source(TK+'Util/TKHelpers.cpp', old), '    bool HasEquippableItemForSlot(')
    methods = [block(source('src/Ai/Base/Value/ItemUsageValue.cpp'), 'ItemUsage ItemUpgradeValue::CalculateForItem('),
               block(source('src/Ai/Base/Value/ItemUsageValue.cpp'), 'ItemUsage ItemUpgradeValue::CalculateUsage('),
               'namespace TempestKeepHelpers\n{\n'+helper+'\n}',
               block(source('src/Ai/Base/Actions/EquipAction.cpp'), 'bool EquipUpgradeAction::Execute('),
               block(source(TK+'TKActions.cpp', old), 'bool KaelthasSunstriderReequipGearAction::Execute('),
               block(source(TK+'TKTriggers.cpp', old), 'bool KaelthasSunstriderLegendaryWeaponsWereLostTrigger::IsActive('),
               block(source(TK+'TKActions.cpp', old), 'bool KaelthasSunstriderManageAdvisorDpsTimerAction::Execute('),
               block(source(TK+'TKMultipliers.cpp', old), 'float KaelthasSunstriderWaitForDpsMultiplier::GetValue(')]
    if not old:
        methods.append(block(source(TK+'TKActions.cpp'), 'bool KaelthasSunstriderReequipGearAction::isUseful('))
    declaration = helper.split('{', 1)[0].strip() + ';'
    text = text.replace('// RECOVERY_CLASSES', '\n'.join(classes))
    text = text.replace('// HELPER_DECLARATION', declaration)
    text = text.replace('// RECOVERY_METHODS', '\n\n'.join(methods))
    if mutant == 'always-success':
        needle = '    botAI->DoSpecificAction("equip upgrade", Event(), true);'
        assert text.count(needle) == 1
        text = text.replace(needle, needle + '\n    return true;')
    elif mutant == 'no-backoff':
        needle = '(!_hasAttempted || getMSTimeDiff(_lastAttempt, getMSTime()) >= 5000)'
        assert text.count(needle) == 1
        text = text.replace(needle, 'true')
    return text


class TKWeaponRecoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='tk-weapon-recovery-')
        cls.path = Path(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_fixture(self, text, name, scenario, sanitize=False):
        cpp, exe = self.path/(name+'.cpp'), self.path/name
        cpp.write_text(text)
        flags = ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-implicit-fallthrough', '-g']
        if sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
        result = subprocess.run(['g++', *flags, str(cpp), '-o', str(exe)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return subprocess.run([str(exe), scenario], text=True, capture_output=True,
                              env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1'),
                              preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))

    def test_production_recovery(self):
        result = self.run_fixture(make_fixture(), 'current', 'all')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sanitizers(self):
        result = self.run_fixture(make_fixture(), 'sanitized', 'all', True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_old_false_positive(self):
        result = self.run_fixture(make_fixture(True), 'old', 'knife')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('!f.trigger.IsActive()', result.stderr)

    def test_monday_equipment_code_reproduces_false_positive(self):
        result = self.run_fixture(make_fixture(old=True, monday=True), 'monday', 'knife')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('!f.trigger.IsActive()', result.stderr)
        # The TK methods under test did not change between Monday and today's deployment.
        for path in ['TKActions.cpp', 'TKActions.h', 'TKTriggers.cpp', 'TKTriggers.h',
                     'TKMultipliers.cpp', 'Util/TKHelpers.cpp', 'Util/TKHelpers.h']:
            monday = subprocess.check_output(['git', '-C', str(PB), 'show',
                                             f'8275e8f8f9998136047ee034458bb1e2bc49dd2f:{TK+path}'], text=True)
            self.assertEqual(monday, source(TK+path, True))

    def test_old_noop_starves_commands_and_timer(self):
        result = self.run_fixture(make_fixture(True), 'old-starvation', 'fairness')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('advisorDpsWaitTimer.count(scene.instance) == 1', result.stderr)

    def test_false_success_mutant(self):
        result = self.run_fixture(make_fixture(mutant='always-success'), 'success-mutant', 'fairness')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('advisorDpsWaitTimer.count(scene.instance) == 1', result.stderr)

    def test_backoff_mutant(self):
        result = self.run_fixture(make_fixture(mutant='no-backoff'), 'backoff-mutant', 'retry')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('f.ai.equipAttempts == 1 && !f.recovery.isUseful()', result.stderr)

    def test_encounter_policy_unchanged(self):
        for path in ['TKStrategy.cpp', 'TKMultipliers.cpp']:
            self.assertEqual(source(TK+path), source(TK+path, True))
        for signature in ['bool KaelthasSunstriderManageAdvisorDpsTimerAction::Execute(',
                          'bool KaelthasSunstriderAssignAdvisorDpsPriorityAction::Execute(',
                          'bool KaelthasSunstriderLootLegendaryWeaponsAction::ShouldBotLootWeapon(']:
            self.assertEqual(block(source(TK+'TKActions.cpp'), signature),
                             block(source(TK+'TKActions.cpp', True), signature))


if __name__ == '__main__':
    unittest.main()
