"""Offline spec-aware token loot tests; no server, DB, CMake or client files needed."""
import json
from pathlib import Path
import re
import resource
import shutil
import subprocess
import tempfile
import unittest

from scripts.tests.test_skull_combat_only import method

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
BOT = CORE / "modules/mod-playerbots"
ITEM = BOT / "src/Mgr/Item"
BASELINE = "39923c6843a75185f8d613a39edc576b8863a6ce"


def no_includes(text):
    return re.sub(r'^#include .*\n', '', text, flags=re.M)


def native_enums():
    sources = {
        CORE / "src/server/shared/SharedDefines.h": ["Classes", "Stats", "Powers", "SpellSchools",
                                                       "SpellSchoolMask", "SpellEffects", "ItemQualities"],
        CORE / "src/server/game/Entities/Item/ItemTemplate.h": ["ItemModType", "ItemSpelltriggerType",
            "InventoryType", "ItemClass", "ItemSubclassWeapon"],
        CORE / "src/server/game/Entities/Unit/Unit.h": ["CombatRating"],
        CORE / "src/server/game/Spells/Auras/SpellAuraDefines.h": ["AuraType"],
        ITEM / "StatsCollector.h": ["StatsType", "CollectorType"],
        BOT / "src/Bot/PlayerbotAI.h": [c + "_TABS" for c in
            ("HUNTER", "ROGUE", "PRIEST", "DEATH_KNIGHT", "DRUID", "MAGE", "SHAMAN", "PALADIN", "WARLOCK", "WARRIOR")],
    }
    return '\n'.join(method(path.read_text(), "enum " + name) + ';' for path, names in sources.items() for name in names)


def fixture_data():
    data = json.loads((ROOT / "scripts/tests/fixtures/direct-token-loot/t4-items.json").read_text())
    lines = []
    for item in data["items"]:
        lines += ['{ ItemTemplate p;']
        for source, dest in {"entry": "ItemId", "name": "Name1", "class": "Class", "subclass": "SubClass",
                "Quality": "Quality", "InventoryType": "InventoryType", "AllowableClass": "AllowableClass",
                "ItemLevel": "ItemLevel", "RequiredLevel": "RequiredLevel", "RequiredHonorRank": "RequiredHonorRank",
                "armor": "Armor", "block": "Block", "socketBonus": "socketBonus", "itemset": "ItemSet"}.items():
            lines.append(f'p.{dest} = {json.dumps(item[source])};')
        lines.append(f'p.StatsCount = {len(item["stats"])};')
        for i, (typ, val) in enumerate(item["stats"]):
            lines.append(f'p.ItemStat[{i}] = {{{typ}, {val}}};')
        for i, (spell, trigger) in enumerate(item["spells"]):
            lines.append(f'p.Spells[{i}].SpellId = {spell}; p.Spells[{i}].SpellTrigger = {trigger};')
        for i, color in enumerate(item["sockets"]):
            lines.append(f'p.Socket[{i}].Color = {color};')
        lines.append('objects.items[p.ItemId] = p; }')
    for id, costs in data["costs"].items():
        lines.append(f'sItemExtendedCostStore.rows[{id}] = {{{costs["reqhonorpoints"]}, '
                     f'{costs["reqarenapoints"]}, {costs["reqpersonalarenarating"]}}};')
    for id, effects in data["spells"].items():
        for e in effects:
            vals = ', '.join(str(e[k]) for k in ("Effect", "ApplyAuraName", "BasePoints", "DieSides", "MiscValue", "TriggerSpell"))
            lines.append(f'spells[{id}].push_back({{{vals}}});')
    for id, effects in data["enchants"].items():
        for i, e in enumerate(effects):
            for key in ("type", "amount", "spellid"):
                lines.append(f'sSpellItemEnchantmentStore.rows[{id}].{key}[{i}] = {e[key]};')
    for token, rewards in data["rewards"].items():
        for r in rewards:
            lines.append('{ TokenRewardCandidate c; '
                         f'c.tokenItemId = {token}; c.rewardItemId = {r["item"]}; c.extendedCost = {r["cost"]}; '
                         f'TokenItemResolver::rewards[{token}].push_back(c); }}')
    return '\n'.join(lines)


def scorer_bodies():
    source = (ITEM / "StatsWeightCalculator.cpp").read_text()
    collector = (ITEM / "StatsCollector.cpp").read_text()
    constants = '\n'.join(re.findall(r'constexpr uint32 [\s\S]*?;', source))
    methods = ["StatsWeightCalculator::StatsWeightCalculator(", "void StatsWeightCalculator::Reset(",
               "float StatsWeightCalculator::CalculateItem(", "void StatsWeightCalculator::GenerateWeights(",
               "void StatsWeightCalculator::GenerateBasicWeights(", "void StatsWeightCalculator::GenerateAdditionalWeights(",
               "void StatsWeightCalculator::ApplyWeightFinetune(", "void StatsWeightCalculator::CalculateSocketBonus(",
               "void StatsWeightCalculator::CalculateItemTypePenalty("]
    bodies = constants + '\ntemplate <size_t Size>\n' + method(source, "bool HasAnySpell(")
    bodies += '\n' + '\n'.join(method(source, name) for name in methods)
    bodies += '\n' + '\n'.join(method(collector, name) for name in [
        "void StatsCollector::Reset(", "void StatsCollector::CollectItemStats(",
        "void StatsCollector::CollectByItemStatType(", "void StatsCollector::HandleApplyAura(",
        "float StatsCollector::AverageValue("])
    bodies += '\n' + method((BOT / "src/Bot/PlayerbotAI.cpp").read_text(),
                             "float PlayerbotAI::GetItemScoreMultiplier(")
    bodies += '\n' + method((BOT / "src/Bot/Factory/PlayerbotFactory.cpp").read_text(),
                             "uint32 PlayerbotFactory::CalcMixedGearScore(")
    return bodies


class DirectTokenLootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            raise unittest.SkipTest("C++20 compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="direct-token-loot-")
        cls.addClassCleanup(cls.temp.cleanup)
        tmp = Path(cls.temp.name)
        fixture = (ROOT / "scripts/tests/cpp/DirectTokenLootTest.cpp").read_text()
        historical = subprocess.check_output(["git", "-C", str(BOT), "show",
            BASELINE + ":src/Mgr/Item/DirectTokenLootScript.cpp"], text=True)
        replacements = {
            "// PRODUCTION_ENUMS": native_enums(),
            "// PRODUCTION_SCORER_HEADER": no_includes((ITEM / "StatsWeightCalculator.h").read_text()),
            "// PRODUCTION_SCORER": scorer_bodies(),
            "// PRODUCTION_CANDIDATE_HEADER": method((ITEM / "TokenItemResolver.h").read_text(), "struct TokenRewardCandidate") + ';',
            "// PRODUCTION_LOOT": no_includes((ITEM / "DirectTokenLootScript.cpp").read_text()),
            "// HISTORICAL_POOL": method(historical, "std::vector<uint32> FindDirectRewardItemIds(").replace(
                "FindDirectRewardItemIds", "OldFindDirectRewardItemIds"),
            "// FIXTURE_DATA": fixture_data(),
        }
        for marker, value in replacements.items():
            fixture = fixture.replace(marker, value)
        cpp = tmp / "test.cpp"
        cpp.write_text(fixture)
        cls.binary = tmp / "test"
        subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-implicit-fallthrough",
                        "-Wno-unused-function", str(cpp), "-o", str(cls.binary)], check=True)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], text=True, capture_output=True, timeout=15,
                                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        print(result.stdout.strip())

    def test_class_first_unique_spec_winners(self): self.run_case("selection")
    def test_pvp_exclusion_and_old_pool_regression(self): self.run_case("pvp")
    def test_present_humans_bots_and_eligibility(self): self.run_case("context")
    def test_hook_scope_gates_and_loot_preservation(self): self.run_case("hook")
    def test_quest_masks_quality_and_safe_fallbacks(self): self.run_case("safety")
    def test_loot_scoring_independent_of_current_gear_and_ai_role(self): self.run_case("scoring")
    def test_installed_t4_all_classes_specs_slots(self): self.run_case("tier")

    def test_existing_equipment_scoring_bodies_preserved(self):
        path = "src/Mgr/Item/StatsWeightCalculator.cpp"
        before = subprocess.check_output(["git", "-C", str(BOT), "show", BASELINE + ":" + path], text=True)
        after = (BOT / path).read_text()
        for name in ("CalculateItem", "CalculateItemSetMod", "CalculateSocketBonus", "ApplyOverflowPenalty",
                     "GenerateAdditionalWeights", "ApplyWeightFinetune", "ApplyPreferredSpecWeapons"):
            signature = ("float " if name in ("CalculateItem", "ApplyPreferredSpecWeapons") else "void ")
            signature += "StatsWeightCalculator::" + name + "("
            self.assertEqual(method(before, signature), method(after, signature))
        # Changed shared functions are byte-identical on the ordinary-equipment branch.
        basic = method(after, "void StatsWeightCalculator::GenerateBasicWeights(")
        self.assertEqual(basic.replace("(forLoot_ || !PlayerbotAI::IsTank(player))", "!PlayerbotAI::IsTank(player)"),
                         method(before, "void StatsWeightCalculator::GenerateBasicWeights("))
        penalty = method(after, "void StatsWeightCalculator::CalculateItemTypePenalty(")
        self.assertEqual(penalty.replace("!forLoot_ && ", ""),
                         method(before, "void StatsWeightCalculator::CalculateItemTypePenalty("))


if __name__ == "__main__":
    unittest.main()
