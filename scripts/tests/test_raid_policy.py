"""Focused offline Lua5.4.8 runtime and native Cthun adapter tests. No server/SQL operations."""
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import time
import unittest
from test_cthun_positioning import lua_layer

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
PATCH = ROOT / "patches/0033-playerbot-cthun-lua-policy.patch"
POLICY = ROOT / "raid-policies/aq40/cthun/policy.lua"


def run(args, cwd=None, success=True):
    result = subprocess.run(list(map(str, args)), cwd=cwd, text=True, capture_output=True,
                            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    if success and result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result


class RaidPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.build = Path(os.environ.get("PLAYERBOT_POLICY_TEST_BUILD", cls.temporary.name))
        args = ["cmake", "-S", ROOT / "scripts/tests/lua-runtime", "-B", cls.build,
                "-DCMAKE_BUILD_TYPE=Debug"]
        # Existing Nix store headers/libraries only; never install or alter system packages/flakes.
        if Path("/nix/store").exists() and not os.environ.get("OPENSSL_ROOT_DIR"):
            for dev in sorted(Path("/nix/store").glob("*-openssl-*-dev"), reverse=True):
                metadata = dev / "nix-support/propagated-build-inputs"
                if not metadata.exists():
                    continue
                libraries = [Path(p) / "lib/libcrypto.so" for p in metadata.read_text().split()]
                library = next((p for p in libraries if p.exists()), None)
                if library:
                    args += [f"-DOPENSSL_INCLUDE_DIR={dev}/include", f"-DOPENSSL_CRYPTO_LIBRARY={library}"]
                    break
        run(args)
        run(["cmake", "--build", cls.build, "-j4"])

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_real_runtime_budgets_and_forbidden_apis(self):
        self.assertIn("real Lua sandbox", run([self.build / "policy-runtime-test"]).stdout)
        run([self.build / "policy-runtime-test", POLICY])

    def test_two_instance_transaction_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertIn("real instance ownership", run([self.build / "policy-scope-test", directory]).stdout)

    def test_current_production_adapter_with_real_lua(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-integration-test", directory, POLICY])
            self.assertIn("ten-player landing -> interior entry passed", result.stdout)
            self.assertIn("manual leases and checked route passed", result.stdout)

    def test_specific_blocked_route_feedback_progress(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-integration-test", directory, POLICY, "blocked-route"])
            self.assertIn("safe Lua-directed alternative passed", result.stdout)

    def test_finalized_policy_spline_preserves_rooted_fear(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-integration-test", directory, POLICY, "fear-root"])
            self.assertIn("preserves rooted controlled fear passed", result.stdout)

    def test_entry_reservations_fail_pre_correction_lua(self):
        before = ROOT / "scripts/tests/fixtures/cthun/policy-before-entry-reservations.lua"
        self.assertEqual(hashlib.sha256(before.read_bytes()).hexdigest(),
                         "69d3d50daaf50d24b238e46720f9236022e45278b97b85c74d645ae748b0c25b")
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-integration-test", directory, before], success=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("plan.choices[0] == 1 && plan.choices[1] == 2", result.stderr)

    def test_retention_regression_fails_endpoint_repath(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-retention-mutant", directory, POLICY], success=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("p->moveStops > stops", result.stderr)

    def test_entrance_regression_fails_original(self):
        run([self.build / "policy-entrance-test"])
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            names = ["modules/mod-playerbots/src/Ai/Raid/Aq40/" + name
                     for name in ("Aq40Cthun.h", "Aq40Cthun.cpp")]
            for name in names:
                target = temp / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((CORE / name).read_bytes())
            lua_layer(temp, names, reverse=True)
            aq = temp / "modules/mod-playerbots/src/Ai/Raid/Aq40"
            binary = temp / "original"
            run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 f"-I{self.build}/doubles", f"-I{aq}", ROOT / "scripts/tests/cpp/CthunEntranceRegressionTest.cpp",
                 aq / "Aq40Cthun.cpp", "-o", binary])
            original = run([binary], success=False)
            self.assertNotEqual(original.returncode, 0)
            self.assertIn("FindPosition", original.stderr)

    def test_incremental_patch_byte_roundtrip(self):
        names = [line.split(" b/", 1)[1] for line in PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertTrue(names)
        self.assertFalse(any("boss_" in name or "instance_temple" in name or ".conf" in name for name in names))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for name in names:
                target = temp / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((CORE / name).read_bytes())
            run(["git", "apply", "--reverse", PATCH], cwd=temp)
            run(["git", "apply", "--check", PATCH], cwd=temp)
            run(["git", "apply", PATCH], cwd=temp)
            for name in names:
                self.assertEqual((temp / name).read_bytes(), (CORE / name).read_bytes(), name)

    def test_host_publish_status_revert_check(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            status = root / "status"
            status.mkdir()
            scope = "531-5670-123"
            record = {"scope": scope, "updated": int(time.time()), "active": "native", "queued": "",
                      "destroyed": False}
            (status / (scope + ".json")).write_text(json.dumps(record))
            cli = ["python3", ROOT / "scripts/playerbot_policy.py"]
            common = ["--directory", root, "--status-directory", status, "--scope", scope,
                      "--expect-active", "native", "--checker", self.build / "policy-runtime-test"]
            run(cli + ["publish", POLICY] + common)
            rev = hashlib.sha256(POLICY.read_bytes()).hexdigest()
            self.assertEqual((root / "revisions" / (rev + ".lua")).read_bytes(), POLICY.read_bytes())
            self.assertIn(rev, (root / "requests" / (scope + ".txt")).read_text())
            run(cli + ["revert", "--revision", rev] + common)
            record["queued"] = rev
            (status / (scope + ".json")).write_text(json.dumps(record))
            output = run(cli + ["status", "--status-directory", status, "--scope", scope])
            self.assertIn("queued between pulls", output.stdout)
            bad = root / "bad.lua"
            bad.write_text("return os.execute('false')")
            self.assertNotEqual(run(cli + ["publish", bad] + common, success=False).returncode, 0)
            bad.write_bytes(b" " * 32769)
            self.assertNotEqual(run(cli + ["publish", bad] + common, success=False).returncode, 0)
            bad.unlink()
            os.mkfifo(bad)
            self.assertNotEqual(run(cli + ["publish", bad] + common, success=False).returncode, 0)
            record["updated"] = 0
            (status / (scope + ".json")).write_text(json.dumps(record))
            self.assertNotEqual(run(cli + ["revert", "--revision", rev] + common, success=False).returncode, 0)


if __name__ == "__main__":
    unittest.main()
