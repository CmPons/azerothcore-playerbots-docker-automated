"""Offline regression for healer DPS leaving travel forms, without changing swim travel."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from scripts.tests.test_roster_world_bots import PB, ROOT, block


class DruidHealerFormsTests(unittest.TestCase):
    def test_production_transitions_and_healing_gates(self):
        base = PB / "src/Ai/Class/Druid"
        actions = (base / "Action/DruidShapeshiftActions.cpp").read_text()
        header = (base / "Action/DruidShapeshiftActions.h").read_text()
        context = (base / "DruidAiObjectContext.cpp").read_text()
        generic = (PB / "src/Ai/Base/Trigger/GenericTriggers.cpp").read_text()
        harness = (ROOT / "scripts/tests/cpp/DruidHealerFormsTest.cpp").read_text()
        classes = ["CastCancelDruidAction", "CastCancelTreeFormAction",
                   "CastCancelTravelFormAction", "CastCancelAquaticFormAction"]
        harness = harness.replace("// PRODUCTION_ACTION_CLASSES", "\n".join(
            block(header, f"class {name} :") + ";" for name in classes))
        methods = [block(actions, "bool CastCancelDruidAction::Execute("),
                   block(actions, "bool CastCancelDruidAction::isUseful("),
                   block(generic, "bool HealerShouldAttackTrigger::IsActive("),
                   block((base / "DruidTriggers.cpp").read_text(), "bool AquaticFormTrigger::IsActive("),
                   block((base / "Strategy/GenericDruidStrategy.cpp").read_text(),
                         "void DruidHealerDpsStrategy::InitTriggers("),
                   block((base / "Strategy/RestoDruidStrategy.cpp").read_text(),
                         "void RestoDruidStrategy::InitTriggers(")]
        for form in ("tree", "travel", "aquatic"):
            methods.append(block(context, f"static Action* cancel_{form}_form("))
        harness = harness.replace("// PRODUCTION_METHODS", "\n\n".join(methods))
        registration = []
        for form in ("tree", "travel", "aquatic"):
            line = f'creators["cancel {form} form"] = &DruidAiObjectContextInternal::cancel_{form}_form;'
            self.assertIn(line, context)
            registration.append(line.replace("DruidAiObjectContextInternal::", ""))
        harness = harness.replace("// PRODUCTION_REGISTRATION", "\n".join(registration))
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "forms.cpp"
            binary = Path(directory) / "forms"
            cpp.write_text(harness)
            subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)],
                           check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("druid healer form transitions and healing gates passed", result.stdout)

    def test_strategy_registration_and_swimming_are_retained(self):
        base = PB / "src/Ai/Class/Druid"
        context = (base / "DruidAiObjectContext.cpp").read_text()
        self.assertIn('creators["healer dps"] = &DruidStrategyFactoryInternal::healer_dps;', context)
        self.assertIn('return new DruidHealerDpsStrategy(botAI);', context)
        nc = (base / "Strategy/GenericDruidNonCombatStrategy.cpp").read_text()
        self.assertIn('new TriggerNode("aquatic form", { NextAction("aquatic form", 10.0f) })', nc)


if __name__ == "__main__":
    unittest.main()
