"""Offline production C++ / API-double tests. No server build, SQL, or live actions."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "azerothcore-wotlk/modules/mod-playerbots"
PATCH = ROOT / "patches/0026-playerbot-twins-coordination.patch"
AQ = MODULE / "src/Ai/Raid/Aq40"


def run(args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


def block(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class TwinsCoordinationTests(unittest.TestCase):
    def test_production_helpers_actions_and_policies(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            fixture = ROOT / "scripts/tests/fixtures/twins/Framework.h"
            headers = """Define Common ObjectGuid Position Creature DynamicObject Group InstanceScript Map
                ObjectAccessor Pet Playerbots SharedDefines Timer Action AttackAction MovementActions
                CharmInfo CreatureAI Config AiObjectContext Player PlayerbotAI Spell SpellMgr ThreatManager
                Unit CreatureData Strategy Multiplier GenericActions GenericSpellActions FollowActions
                ChooseTargetActions ReachTargetActions""".split()
            for name in headers:
                (temp / f"{name}.h").write_text(f'#include "{fixture}"\n')
            baseline = temp / "baseline"
            baseline.mkdir()
            names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                     if line.startswith("diff --git ")]
            for name in names:
                p = baseline / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes((ROOT / "azerothcore-wotlk" / name).read_bytes())
            run(["git", "apply", "--reverse", str(PATCH)], cwd=baseline)
            old = baseline / "modules/mod-playerbots/src"
            legacy_helpers = (old / "Ai/Raid/Aq40/Aq40Helpers.cpp").read_text().replace(
                "namespace TempleOfAhnQirajHelpers\n{", "namespace legacy\n{\n"
                "using namespace TempleOfAhnQirajHelpers;\n"
                "constexpr float TWINS_HEALER_RADIUS = 18.0f;\n"
                "constexpr uint32 TWINS_HEALER_SLOTS = 6;\n")
            (temp / "legacy_helpers.cpp").write_text(legacy_helpers)
            legacy_threat = (old / "Ai/Base/Util/RaidThreatUtils.cpp").read_text().replace(
                "namespace ai::threat", "namespace legacyThreat")
            (temp / "legacy_threat.cpp").write_text(legacy_threat)
            code = (ROOT / "scripts/tests/cpp/TwinsCoordinationTest.cpp").read_text()
            threat = (MODULE / "src/Ai/Base/Value/ThreatValues.cpp").read_text()
            healing = (MODULE / "src/Ai/Base/Value/PartyMemberToHeal.cpp").read_text()
            code = code.replace("/* THREAT_VALUE */", block(threat, "uint8 ThreatValue::Calculate(Unit* target)"))
            code = code.replace("/* HEAL_CALCULATE */", block(healing, "Unit* PartyMemberToHeal::Calculate()"))
            code = code.replace("/* HEAL_CHECK */", block(healing, "bool PartyMemberToHeal::Check(Unit* player)"))
            (temp / "test.cpp").write_text(code)
            sources = [AQ / name for name in (
                "Aq40Helpers.cpp", "Aq40Coordination.cpp", "Aq40Multipliers.cpp",
                "Aq40Actions_Twins.cpp", "Aq40Actions_Coordination.cpp")]
            sources += [temp / "legacy_helpers.cpp", temp / "legacy_threat.cpp",
                        MODULE / "src/Ai/Base/Util/RaidThreatUtils.cpp",
                        MODULE / "src/Ai/Base/Strategy/ThreatStrategy.cpp"]
            binary = temp / "test"
            run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 f"-I{temp}", f"-I{AQ}", f"-I{MODULE / 'src/Ai/Base/Util'}",
                 f"-I{MODULE / 'src/Ai/Base/Strategy'}", str(temp / "test.cpp"),
                 *map(str, sources), "-o", str(binary)])
            self.assertIn("Twins production coordination regressions passed", run([str(binary)]))

    def test_patch_round_trip_and_scope(self):
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertTrue(names)
        self.assertTrue(all(name.startswith("modules/mod-playerbots/src/") for name in names))
        self.assertFalse(any("conf" in name or "sql" in name for name in names))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for name in names:
                p = temp / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes((ROOT / "azerothcore-wotlk" / name).read_bytes())
            run(["git", "apply", "--reverse", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", "--reverse", str(PATCH)], cwd=temp)
            # The prior role picker selected both the mage and the warlock; the old movement
            # gate explicitly left caster tanks to native movement and tanks chased adds.
            old = (temp / "modules/mod-playerbots/src/Ai/Raid/Aq40/Aq40Helpers.cpp").read_text()
            self.assertIn("TwinsTankRank(bot, true) < 2", old)
            self.assertIn("if (role == TwinsRole::WarlockTank)\n        return false;", old)
            run(["git", "apply", "--check", str(PATCH)], cwd=temp)
            run(["git", "apply", str(PATCH)], cwd=temp)
            for name in names:
                self.assertEqual((temp / name).read_bytes(), (ROOT / "azerothcore-wotlk" / name).read_bytes())

    def test_pinned_replay_of_twins_sources(self):
        # Focused reproducibility, NOT a claim that the whole server can be regenerated. Existing
        # 0021 has a Playerbots.cpp registration dependency outside these source files; see docs.
        pin = next(line.split()[1] for line in (ROOT / "repo-pins.txt").read_text().splitlines()
                   if line.startswith("mod-playerbots "))
        available = subprocess.run(["git", "-C", str(MODULE), "cat-file", "-e", pin], capture_output=True)
        if available.returncode:
            self.skipTest("Pinned module object not available locally; no network fetch in tests")
        patches = [p for p in sorted((ROOT / "patches").glob("*.patch"))
                   if not p.name.startswith("0021-")]
        prefix = "modules/mod-playerbots/"
        names = {line.split(" b/", 1)[1] for p in patches for line in p.read_text().splitlines()
                 if line.startswith("diff --git a/" + prefix)}
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for name in names:
                blob = subprocess.run(["git", "-C", str(MODULE), "show", pin + ":" + name[len(prefix):]],
                                      capture_output=True)
                if blob.returncode == 0:
                    p = temp / name
                    p.parent.mkdir(parents=True, exist_ok=True)
                    p.write_bytes(blob.stdout)
            for p in patches:
                if any(line.startswith("diff --git a/" + prefix) for line in p.read_text().splitlines()):
                    run(["git", "apply", "--include=" + prefix + "*", str(p)], cwd=temp)
            owned = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                     if line.startswith("diff --git ")]
            for name in owned:
                self.assertEqual((temp / name).read_bytes(), (ROOT / "azerothcore-wotlk" / name).read_bytes(), name)

    def test_registration_and_no_gameplay_shortcuts(self):
        actions = (AQ / "Aq40ActionContext.h").read_text()
        strategy = (AQ / "Aq40Strategy.cpp").read_text()
        for name in ("aq40 twins caster tank", "aq40 twins clear arcane"):
            self.assertIn(f'creators["{name}"]', actions)
            self.assertIn(f'NextAction("{name}"', strategy)
        self.assertIn('creators["aq40 twins status"]', actions)
        code = (AQ / "Aq40Coordination.cpp").read_text() + (AQ / "Aq40Actions_Coordination.cpp").read_text()
        for forbidden in ("AddThreat(", "SetHealth(", "SetMaxHealth(", "TeleportTo(", "ChangeStrategy(",
                          "SetTalent", "SetGroup", "Arinerica", "Beliona"):
            self.assertNotIn(forbidden, code)
        self.assertIn('CanCastSpell(spell, boss)', code)
        self.assertIn('CastSpell(spell, boss)', code)


if __name__ == "__main__":
    unittest.main()
