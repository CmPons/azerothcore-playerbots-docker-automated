"""Actual-root combat VM/native-adapter checks. No server/runtime publication operations."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import test_raid_policy as legacy
from test_raid_policy import run

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
PB = CORE / "modules/mod-playerbots/src"
POLICY = PB / "Ai/Raid/Policy"


def body(path, signature):
    text = path.read_text()
    start = text.index(signature)
    position = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[position] == "{") - (text[position] == "}")
        position += 1
    return text[start:position]


def native_includes():
    paths = set()
    for source in ("src/common", "src/server/game", "src/server/shared", "src/server/database",
                   "modules/mod-playerbots/src"):
        paths.update(p.parent for p in (CORE / source).rglob("*.h"))
    for source in ("deps/fmt/include", "deps/g3dlite/include", "deps/recastnavigation/Detour/Include",
                   "deps/recastnavigation/Recast/Include", "deps/SFMT", "deps/utf8cpp", "deps/fkYAML/include"):
        paths.add(CORE / source)
    for name in ("boost", "openssl"):
        found = sorted(Path("/nix/store").glob(f"*-{name}-*-dev"))
        if found:
            paths.add(found[-1] / "include")
    return ["-I" + str(p) for p in sorted(paths)]


class RaidCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        legacy.RaidPolicyTests.setUpClass.__func__(cls)
        cache = (cls.build / "CMakeCache.txt").read_text().splitlines()
        cls.crypto = next(line.split("=", 1)[1] for line in cache if line.startswith("OPENSSL_CRYPTO_LIBRARY:"))

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_real_combat_runtime_and_initial_policy(self):
        self.assertIn("finite budgets passed", run([self.build / "raid-combat-check"]).stdout)
        result = run([self.build / "raid-combat-check", ROOT / "raid-policies/aq40/combat.lua", "cthun"])
        self.assertIn("real Lua budget passed", result.stdout)
        print(result.stdout.strip())

    def test_exact_action_adapter_and_red_regressions(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            source = (POLICY / "RaidCombatActions.cpp").read_text()
            # The production adapter body is unmodified; only includes are supplied by explicit API doubles.
            fragment = "\n".join(line for line in source.splitlines() if not line.startswith("#include"))
            (out / "RaidCombatActions.inc").write_text(fragment)
            (out / "CurrentTargetGet.inc").write_text(body(PB / "Ai/Base/Value/CurrentTargetValue.cpp",
                "Unit* CurrentTargetValue::Get()"))
            command = ["g++", "-std=gnu++20", "-DMOD_PLAYERBOTS", "-Wall", "-Wextra", "-Werror",
                       "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(out),
                       "-I" + str(ROOT / "scripts/tests/fixtures/raid-combat"), "-I" + str(POLICY),
                       "-I" + str(CORE / "src/common/Utilities"),
                       ROOT / "scripts/tests/cpp/RaidCombatActionsTest.cpp", self.build / "libpolicy-core.a",
                       self.build / "lua/libplayerbot_lua.a", self.crypto, "-o", out / "actions"]
            run(command)
            for case in ([], ["engagement"], ["expiry"]):
                print(run([out / "actions"] + case).stdout.strip())
            # Restore the two reviewed admission defects independently. These are ordering/gate mutants,
            # not a claim that unrelated deferred interaction code was replayed or passed.
            for case, old, new in (
                ("engagement", "return !harmful || engaged;", "(void)harmful; (void)engaged; return true;"),
                ("expiry", "if (!validate())\n        return false;\n    if (!current())",
                 "if (!current())\n        return false;\n    if (!validate())"),
            ):
                header = (POLICY / "RaidCombatAdmission.h").read_text()
                self.assertIn(old, header)
                (out / "RaidCombatAdmission.h").write_text(header.replace(old, new))
                run(command)
                result = run([out / "actions", case], success=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("admitted == 0", result.stderr)
                print("Expected original-order/gate red:", case, result.stderr.strip())
                (out / "RaidCombatAdmission.h").unlink()

    def test_exact_generic_dispatch_with_real_vm_and_reload(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            fragment = body(POLICY / "RaidCombatPolicy.cpp", "void Update(Map* map,")
            (out / "RaidCombatUpdate.inc").write_text("namespace RaidCombat {\n" + fragment + "\n}\n")
            binary = out / "dispatch"
            run(["g++", "-std=gnu++20", "-DMOD_PLAYERBOTS", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(out), "-I" + str(POLICY),
                 "-I" + str(CORE / "src/common/Utilities"), ROOT / "scripts/tests/cpp/RaidCombatDispatchTest.cpp",
                 self.build / "libpolicy-core.a", self.build / "lua/libplayerbot_lua.a", self.crypto,
                 "-o", binary])
            print(run([binary, out / "mailbox"]).stdout.strip())
            for path in (out / "mailbox/status").glob("*.json"):
                status = json.loads(path.read_text())
                self.assertEqual(status["api"], 2)
                self.assertTrue(status["destroyed"])
                self.assertIn("receipts", status)

    def test_checked_api2_publish_default_workflow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = ["python3", ROOT / "scripts/playerbot_policy.py", "publish-default",
                       ROOT / "raid-policies/aq40/combat.lua", "--directory", root,
                       "--checker", self.build / "raid-combat-check", "--expect-default", "none"]
            result = json.loads(run(command).stdout)
            self.assertEqual((root / "defaults/raid.txt").read_text().split()[3], result["requested"])
            self.assertEqual(list((root / "requests").iterdir()), [])
            before = (root / "defaults/raid.txt").read_bytes()
            bad = root / "bad.lua"
            bad.write_text("return {api=2,plan=function(s) error('broken edit') end}")
            command[3] = bad
            failed = run(command, success=False)
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn("broken edit", failed.stderr)
            self.assertEqual((root / "defaults/raid.txt").read_bytes(), before)

    def test_exact_ground_lifecycle_and_scheduled_handoff(self):
        from raid_combat_ground_fixture import execute
        with tempfile.TemporaryDirectory() as directory:
            print(execute(ROOT, CORE, Path(directory), self.build, self.crypto, run, body).strip())

    def test_exact_native_collector_and_grid_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            fixture = ROOT / "scripts/tests/fixtures/raid-combat-observation"
            fragments = [body(CORE / path, signature) for path, signature in (
                ("src/server/game/Grids/Cells/CellImpl.h", "inline Cell::Cell(CellCoord const& p)"),
                ("src/server/game/Maps/Map.h", "template<class T, class CONTAINER>\ninline void Map::Visit"),
                ("src/server/game/Entities/Object/Object.cpp", "bool WorldObject::CanSeeOrDetect("),
                ("src/server/game/Entities/Object/Object.cpp", "bool WorldObject::CanNeverSee("),
                ("src/server/game/Entities/Unit/Unit.cpp", "bool Unit::IsNonMeleeSpellCast("),
                ("src/server/game/Instances/InstanceScript.cpp", "bool InstanceScript::IsEncounterInProgress() const"),
            )]
            (out / "Fragments.inc").write_text("\n\n".join(fragments) + "\n")
            for name, path, signature in (
                ("GridObject.inc", "Entities/Object/Object.h", "template<class T>\nclass GridObject"),
                ("MapReference.inc", "Maps/MapReference.h", "class MapReference :"),
            ):
                (out / name).write_text(body(CORE / "src/server/game" / path, signature) + ";\n")
            source = (POLICY / "GenericObservation.cpp").read_text()
            (out / "Candidate.inc").write_text("\n".join(
                line for line in source.splitlines() if not line.startswith("#include")) + "\n")
            for name in ("ObservationTest", "GridRefMgrTest"):
                binary = out / name
                run(["g++", "-std=gnu++20", "-DMOD_PLAYERBOTS", "-O1", "-Wall", "-Wextra", "-Werror",
                     "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(out),
                     "-I" + str(fixture), *native_includes(), fixture / (name + ".cpp"), "-o", binary])
                print(run([binary]).stdout.strip())


if __name__ == "__main__":
    unittest.main()
