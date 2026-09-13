"""Focused offline Lua5.4.8 runtime and native Cthun adapter tests. No server/SQL operations."""
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile
import time
import unittest
from test_cthun_positioning import lua_layer

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"
PATCH = ROOT / "patches/0033-playerbot-cthun-lua-policy.patch"
AUTOMATIC_PATCH = ROOT / "patches/0034-playerbot-automatic-policy-default.patch"
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

    def test_automatic_default_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-Default-test", directory])
            self.assertIn("default lifecycle precedence/failure/quarantine/recreation passed", result.stdout)
            status = run(["python3", ROOT / "scripts/playerbot_policy.py", "status",
                          "--status-directory", Path(directory) / "status", "--scope", "531-3-3"])
            self.assertEqual(json.loads(status.stdout)["diagnostic_error"],
                             "diagnostic expired: default publication changed")

    def test_automatic_current_native_adapter(self):
        with tempfile.TemporaryDirectory() as directory:
            result = run([self.build / "policy-integration-test", directory, POLICY, "automatic"])
            self.assertIn("automatic actual adapter fresh/combat/recreated/eligibility passed", result.stdout)

    def test_automatic_start_fails_original_0033(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run([self.build / "policy-AutomaticStart-test", root / "current"])
            names = [line.split(" b/", 1)[1] for line in AUTOMATIC_PATCH.read_text().splitlines()
                     if line.startswith("diff --git ")]
            for name in names:
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((CORE / name).read_bytes())
            run(["git", "apply", "--reverse", AUTOMATIC_PATCH], cwd=root)
            scope = root / "modules/mod-playerbots/src/Ai/Raid/Policy"
            self.assertEqual(hashlib.sha256((scope / "CthunPolicyScope.cpp").read_bytes()).hexdigest(),
                             "94666a0ccd75af5c6f5960e00b3505ea5e71a6b5319bdee9116b4128b57345bb")
            cache = (self.build / "CMakeCache.txt").read_text().splitlines()
            crypto = next(line.split("=", 1)[1] for line in cache
                          if line.startswith("OPENSSL_CRYPTO_LIBRARY:"))
            includes = next(line.split("=", 1)[1] for line in cache
                            if line.startswith("OPENSSL_INCLUDE_DIR:"))
            run([os.environ.get("CXX", "g++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=undefined", f"-I{scope}", f"-I{includes}",
                 f"-I{CORE}/modules/mod-playerbots/src/Ai/Raid/Policy",
                 f"-I{CORE}/src/common/Utilities", ROOT / "scripts/tests/cpp/CthunPolicyAutomaticStartTest.cpp",
                 scope / "CthunPolicyScope.cpp", self.build / "libpolicy-core.a",
                 self.build / "lua/libplayerbot_lua.a", crypto, "-o", root / "original"])
            result = run([root / "original", root / "original-files"], success=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("scope.active && scope.revision == revision", result.stderr)
            print("Expected original0033 automatic-start failure:", result.stderr.strip())

    def test_automatic_incremental_patch_roundtrip(self):
        names = [line.split(" b/", 1)[1] for line in AUTOMATIC_PATCH.read_text().splitlines()
                 if line.startswith("diff --git ")]
        self.assertEqual(set(names), {"modules/mod-playerbots/src/Ai/Raid/Policy/CthunPolicyScope." + suffix
                                     for suffix in ("h", "cpp")})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in names:
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((CORE / name).read_bytes())
            run(["git", "apply", "--check", "--reverse", AUTOMATIC_PATCH], cwd=root)
            run(["git", "apply", "--reverse", AUTOMATIC_PATCH], cwd=root)
            run(["git", "apply", "--check", AUTOMATIC_PATCH], cwd=root)
            run(["git", "apply", AUTOMATIC_PATCH], cwd=root)
            for name in names:
                self.assertEqual((root / name).read_bytes(), (CORE / name).read_bytes(), name)

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
            run(["git", "apply", "--reverse", AUTOMATIC_PATCH], cwd=temp)
            run(["git", "apply", "--reverse", PATCH], cwd=temp)
            run(["git", "apply", "--check", PATCH], cwd=temp)
            run(["git", "apply", PATCH], cwd=temp)
            run(["git", "apply", AUTOMATIC_PATCH], cwd=temp)
            for name in names:
                self.assertEqual((temp / name).read_bytes(), (CORE / name).read_bytes(), name)

    def test_host_installed_default_atomic_cas_and_diagnostic_binding(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            policies = root / "policies"
            status = root / "status"
            status.mkdir()
            cli = ["python3", ROOT / "scripts/playerbot_policy.py"]
            common = ["--directory", policies, "--checker", self.build / "policy-runtime-test"]
            result = run(cli + ["publish-default", POLICY, "--expect-default", "none"] + common)
            installed = json.loads(result.stdout)
            manifest = policies / "defaults/raid.txt"
            initial = manifest.read_bytes()
            self.assertTrue(initial.startswith(b"1 1 "))
            self.assertTrue((policies / "requests").is_dir())
            self.assertEqual(list((policies / "requests").iterdir()), [])
            self.assertNotEqual(run(cli + ["publish-default", POLICY, "--expect-default", "none"] + common,
                                    success=False).returncode, 0)
            self.assertEqual(manifest.read_bytes(), initial)
            bad = root / "bad.lua"
            for source in (b"return os.execute('false')", b" " * 32769):
                bad.write_bytes(source)
                self.assertNotEqual(run(cli + ["publish-default", bad] + common, success=False).returncode, 0)
                self.assertEqual(manifest.read_bytes(), initial)
            scope = "531-1-999"
            record = {"scope": scope, "updated": int(time.time()), "active": "native", "queued": "",
                      "destroyed": False, "default_publication": installed["publication"]}
            (status / (scope + ".json")).write_text(json.dumps(record))
            diagnostic = ["--status-directory", status, "--scope", scope, "--expect-active", "native"]
            run(cli + ["publish", POLICY] + common + diagnostic)
            request = (policies / "requests" / (scope + ".txt")).read_text().split()
            self.assertEqual(request[3], installed["publication"])
            self.assertLessEqual(len(" ".join(request)), 256)
            # Cooperating concurrent writers are serialized; CAS admits exactly one winner.
            args = list(map(str, cli + ["publish-default", POLICY, "--expect-default", installed["publication"]]
                            + common))
            children = [subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                        for _ in range(2)]
            for child in children:
                child.communicate(timeout=20)
            self.assertEqual(sorted(child.returncode for child in children), [0, 1])
            fields = manifest.read_text().split()
            self.assertEqual(len(fields), 4)
            self.assertEqual(fields[3], hashlib.sha256(POLICY.read_bytes()).hexdigest())
            self.assertNotEqual(run(cli + ["publish", POLICY] + common + diagnostic,
                                    success=False).returncode, 0)  # Scope has not observed new default.
            self.assertFalse(list(policies.rglob("*.tmp")))
            manifest.write_text("invalid")
            run(cli + ["publish-default", POLICY] + common)  # Explicit checked repair, no per-scope commands.

    def test_default_installer_preserves_inherited_mapped_reader_acl(self):
        if not shutil.which("setfacl") or not shutil.which("getfacl"):
            self.skipTest("POSIX ACL tools unavailable; mapped container identity remains a deployment gate")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run(["setfacl", "-m", "u:100999:r-x,d:u::rwx,d:u:100999:r-x,d:g::---,d:m::r-x,d:o::---", root])
            before = run(["getfacl", "-cpn", root]).stdout
            cli = ["python3", ROOT / "scripts/playerbot_policy.py", "publish-default", POLICY,
                   "--directory", root, "--checker", self.build / "policy-runtime-test"]
            run(cli)
            run(cli)  # Atomic replacement must inherit read ACL again, not lose it after first install.
            self.assertEqual(run(["getfacl", "-cpn", root]).stdout, before)
            for path in [root / "defaults", root / "defaults/raid.txt", *list((root / "revisions").glob("*.lua"))]:
                acl = run(["getfacl", "-cpn", path]).stdout
                self.assertIn("user:100999:r-x", acl)
                self.assertNotIn("effective:---", acl)
                self.assertNotIn("effective:-", acl)
            # ACL metadata test only: this process did not impersonate the deployed container UID.

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
