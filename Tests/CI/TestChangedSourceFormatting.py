#!/usr/bin/env python3
"""Native local hook must inspect staged owned files, never the whole tree."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'Build/Scripts/FormatAll.sh'


class FormatHookTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.root = Path(self.dir.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', '-C', str(self.root), 'config', 'user.name', 'CI Test'], check=True)
        subprocess.run(['git', '-C', str(self.root), 'config', 'user.email', 'test@example.com'], check=True)
        (self.root / 'Source').mkdir()
        (self.root / 'Tests').mkdir()
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        stub = self.bin / 'clang-format-23'
        stub.write_text('''#!/usr/bin/env python3
import sys
from pathlib import Path
if '--version' in sys.argv:
    print('clang-format version 23.1.5')
    sys.exit(0)
if '-i' in sys.argv:
    path = Path(sys.argv[-1])
    path.write_text(path.read_text().replace('BADFORMAT', 'GOODFORMAT'))
else:
    print(sys.stdin.read().replace('BADFORMAT', 'GOODFORMAT'), end='')
''')
        stub.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ['PATH'])

    def git(self, *args):
        subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True)

    def check(self, mode='--pre-commit'):
        return subprocess.run(['bash', str(SCRIPT), mode], cwd=self.root,
                              env=self.env, text=True, capture_output=True)

    def test_unstaged_and_unrelated_sources_are_not_formatted(self):
        (self.root / 'Source/old.cpp').write_text('BADFORMAT\n')
        (self.root / 'Source/new.cpp').write_text('GOODFORMAT\n')
        (self.root / 'Tests/notes.txt').write_text('unrelated')
        self.git('add', '.')
        self.git('commit', '-qm', 'baseline')
        (self.root / 'Source/new.cpp').write_text('GOODFORMAT\nGOODFORMAT\n')
        self.git('add', 'Source/new.cpp')
        self.assertEqual(self.check().returncode, 0)
        self.assertEqual((self.root / 'Source/old.cpp').read_text(), 'BADFORMAT\n')

    def test_rejects_invalid_staged_blob_even_if_worktree_is_fixed(self):
        path = self.root / 'Source/new.cpp'
        path.write_text('BADFORMAT\n')
        self.git('add', 'Source/new.cpp')
        path.write_text('GOODFORMAT\n')
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('staged source requires formatting', result.stderr)

    def test_no_staged_cpp_is_a_noop(self):
        (self.root / 'Tests/README.txt').write_text('text')
        self.git('add', 'Tests/README.txt')
        self.assertEqual(self.check().returncode, 0)

    def test_apply_does_not_mutate_unstaged_changes(self):
        path = self.root / 'Source/new.cpp'
        path.write_text('BADFORMAT\n')
        self.git('add', 'Source/new.cpp')
        path.write_text('BADFORMAT\nunstaged\n')
        self.assertNotEqual(self.check('--apply').returncode, 0)
        self.assertEqual(path.read_text(), 'BADFORMAT\nunstaged\n')


if __name__ == '__main__':
    unittest.main()
