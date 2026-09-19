"""Exercise safe source operations against disposable local Git repositories."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "scripts/source-repos.sh"


class SourceReposTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.origin = self.root / "origin"
        self.dest = self.root / "checkout"
        self.pins = self.root / "pins"
        self.git(self.root, "init", "-b", "main", str(self.origin))
        self.git(self.origin, "config", "user.name", "Test")
        self.git(self.origin, "config", "user.email", "test@example.invalid")
        self.git(self.origin, "config", "commit.gpgsign", "false")
        self.a = self.commit(self.origin, "one")
        self.b = self.commit(self.origin, "two")
        self.pin(self.a)

    def git(self, directory, *args):
        return subprocess.check_output(
            ["git", "-C", str(directory), *args], text=True, stderr=subprocess.PIPE
        ).strip()

    def commit(self, directory, text):
        (directory / "source.txt").write_text(text)
        self.git(directory, "add", "source.txt")
        self.git(directory, "-c", "user.name=Test", "-c",
                 "user.email=test@example.invalid", "-c", "commit.gpgsign=false",
                 "commit", "-m", text)
        return self.git(directory, "rev-parse", "HEAD")

    def pin(self, sha):
        self.pins.write_text(f"checkout {sha}\n")

    def shell(self, command):
        return subprocess.run(
            ["bash", "-c", 'set -euo pipefail; source "$HELPER"; ' + command],
            env={**os.environ, "HELPER": str(HELPER), "ROOT": str(self.root),
                 "AC_DIR": str(self.root / "native"), "PINS_FILE": str(self.pins),
                 "DEST": str(self.dest), "ORIGIN": str(self.origin)},
            text=True, capture_output=True,
        )

    def sync(self):
        return self.shell('sync_source_repo "$DEST" "$ORIGIN" local-playerbot')

    def clone(self):
        result = self.sync()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_fresh_clone_uses_exact_pin_not_newer_branch_tip(self):
        self.clone()
        self.assertEqual(self.git(self.dest, "rev-parse", "HEAD"), self.a)
        self.assertEqual(self.git(self.dest, "branch", "--show-current"), "local-playerbot")
        self.assertEqual(self.sync().returncode, 0)

    def test_fast_forward(self):
        self.clone()
        self.pin(self.b)
        result = self.sync()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.git(self.dest, "rev-parse", "HEAD"), self.b)

    def test_tracked_dirty_refused_even_at_current_pin(self):
        self.clone()
        (self.dest / "source.txt").write_text("unsaved work")
        self.assertNotEqual(self.sync().returncode, 0)
        self.assertEqual((self.dest / "source.txt").read_text(), "unsaved work")

    def test_untracked_source_refused(self):
        self.clone()
        (self.dest / "new.cpp").write_text("untracked work")
        self.pin(self.b)
        self.assertNotEqual(self.sync().returncode, 0)
        self.assertEqual((self.dest / "new.cpp").read_text(), "untracked work")
        self.assertEqual(self.git(self.dest, "rev-parse", "HEAD"), self.a)

    def test_staged_work_refused(self):
        self.clone()
        (self.dest / "source.txt").write_text("staged work")
        self.git(self.dest, "add", "source.txt")
        self.assertNotEqual(self.sync().returncode, 0)
        self.assertEqual(self.git(self.dest, "show", ":source.txt"), "staged work")

    def test_ahead_and_divergent_work_never_rewound(self):
        self.clone()
        c = self.commit(self.dest, "local commit")
        for pin in [self.a, self.b]:
            with self.subTest(pin=pin):
                self.pin(pin)
                self.assertNotEqual(self.sync().returncode, 0)
                self.assertEqual(self.git(self.dest, "rev-parse", "HEAD"), c)

    def test_nonrepo_directory_preserved(self):
        self.dest.mkdir()
        (self.dest / "important").write_text("keep")
        self.assertNotEqual(self.sync().returncode, 0)
        self.assertEqual((self.dest / "important").read_text(), "keep")
        self.assertFalse((self.dest / ".git").exists())

    def test_missing_pin_refused(self):
        self.pins.write_text("")
        self.assertNotEqual(self.sync().returncode, 0)
        self.assertFalse(self.dest.exists())

    def test_mirrors_copy_only_when_missing_and_refuse_differences(self):
        src = self.root / "modules/demo"
        src.mkdir(parents=True)
        (src / "code.cpp").write_text("root version")
        result = self.shell("sync_local_module demo")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.shell("sync_local_module demo").returncode, 0)
        dst = self.root / "native/modules/demo/code.cpp"
        dst.write_text("unpublished native change")
        self.assertNotEqual(self.shell("sync_local_module demo").returncode, 0)
        self.assertEqual(dst.read_text(), "unpublished native change")
        self.assertEqual((src / "code.cpp").read_text(), "root version")

    def test_update_never_starts_services_and_source_failure_blocks_build(self):
        shutil.copy(ROOT / "update.sh", self.root / "update.sh")
        (self.root / "azerothcore-wotlk").mkdir()
        setup = self.root / "setup.sh"
        docker = self.root / "docker"
        docker.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$CALLS"\n')
        docker.chmod(0o755)
        calls = self.root / "calls"
        env = {**os.environ, "PATH": str(self.root) + os.pathsep + os.environ["PATH"],
               "CALLS": str(calls)}
        for failure, args, expected in [(True, [], None), (False, ["--sources-only"], None),
                                        (False, [], "compose build ac-worldserver\n")]:
            with self.subTest(failure=failure, args=args):
                setup.write_text('#!/bin/sh\n[ "$1" = --sources-only ] || exit 99\n' +
                                 ("exit 42\n" if failure else "exit 0\n"))
                setup.chmod(0o755)
                result = subprocess.run(["bash", str(self.root / "update.sh"), *args],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 42 if failure else 0, result.stderr)
                self.assertEqual(calls.read_text() if calls.exists() else None, expected)


if __name__ == "__main__":
    unittest.main()
