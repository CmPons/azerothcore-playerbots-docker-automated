"""Offline safety checks: python3 -m unittest discover -s scripts/tests -v."""
import gzip
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "upload-backup-github.sh"
FAKE_GH = r'''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
a = sys.argv[1:]
with open(os.environ['TRACE'], 'a') as f:
    f.write(json.dumps(a) + '\n')
mode = os.environ.get('MODE', '')
if a[:2] == ['repo', 'view']:
    print('false' if mode == 'public' else 'true')
elif a[:2] == ['release', 'create']:
    if mode == 'upload-failure':
        sys.exit(1)
    assets = {Path(p).name: Path(p).stat().st_size for p in a[3:5]}
    Path(os.environ['ASSETS']).write_text(json.dumps(assets))
elif a[:2] == ['release', 'view']:
    sizes = json.loads(Path(os.environ['ASSETS']).read_text())
    query = a[a.index('--jq') + 1]
    name = next(n for n in sizes if '"' + n + '"' in query)
    print(0 if mode == 'size-mismatch' else sizes[name])
elif a[:2] == ['release', 'list']:
    if mode == 'list-failure':
        sys.exit(1)
    print('acore-20260101-040000')
elif a[:2] not in (['release', 'edit'], ['release', 'delete']):
    sys.exit('unexpected command: ' + repr(a))
'''


class UploadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / 'acore-20260906-180000.tar'
        with tarfile.open(self.bundle, 'w') as archive:
            for name, data in [('env', b'FAKE_SECRET=test\n'),
                               ('database.sql.gz', gzip.compress(b'-- test dump\n'))]:
                member = tarfile.TarInfo(name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        gh = self.root / 'gh'
        gh.write_text(FAKE_GH)
        gh.chmod(0o700)
        self.trace = self.root / 'trace'
        self.env = dict(os.environ, BACKUP_GITHUB_REPO='owner/private-backups',
                        BACKUP_GITHUB_KEEP='14', BACKUP_GH_BIN=str(gh),
                        TRACE=str(self.trace), ASSETS=str(self.root / 'assets'), MODE='')

    def run_upload(self, mode=''):
        self.env['MODE'] = mode
        result = subprocess.run(['bash', str(SCRIPT), str(self.bundle)],
                                env=self.env, capture_output=True, text=True)
        calls = [json.loads(line) for line in self.trace.read_text().splitlines()] \
            if self.trace.exists() else []
        return result, calls

    def test_success_publishes_before_pruning(self):
        result, calls = self.run_upload()
        self.assertEqual(result.returncode, 0, result.stderr)
        operations = [c[:2] for c in calls]
        self.assertLess(operations.index(['release', 'edit']),
                        operations.index(['release', 'delete']))
        create = next(c for c in calls if c[:2] == ['release', 'create'])
        self.assertIn('--draft', create)
        listing = next(c for c in calls if c[:2] == ['release', 'list'])
        self.assertIn('.isDraft == false', listing[-1])
        self.assertIn('.[14:]', listing[-1])
        deletion = next(c for c in calls if c[:2] == ['release', 'delete'])
        self.assertEqual(deletion[2], 'acore-20260101-040000')
        self.assertIn('--cleanup-tag', deletion)

    def test_public_repository_refused(self):
        result, calls = self.run_upload('public')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(len(calls), 1)

    def test_failed_upload_or_size_check_never_publishes_or_prunes(self):
        for mode in ('upload-failure', 'size-mismatch'):
            with self.subTest(mode=mode):
                self.trace.unlink(missing_ok=True)
                result, calls = self.run_upload(mode)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(any(c[:2] in (['release', 'edit'], ['release', 'delete'])
                                     for c in calls))
                self.assertTrue(self.bundle.exists())

    def test_listing_failure_is_not_silenced(self):
        result, calls = self.run_upload('list-failure')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(c[:2] == ['release', 'delete'] for c in calls))

    def test_invalid_retention_never_contacts_github(self):
        self.env['BACKUP_GITHUB_KEEP'] = '0'
        result, calls = self.run_upload()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_broken_archive_never_uploads(self):
        self.bundle.write_bytes(b'not a tar archive')
        result, calls = self.run_upload()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(c[:2] == ['release', 'create'] for c in calls))


if __name__ == '__main__':
    unittest.main()
