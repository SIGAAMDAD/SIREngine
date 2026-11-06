#!/usr/bin/env python3
"""Assert concurrent cooker publication is not a CMake stdout pipeline."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "Unit/SIRLangCookerRace.py"
spec = importlib.util.spec_from_file_location("sirlang_cooker_race", SCRIPT)
race = importlib.util.module_from_spec(spec)
spec.loader.exec_module(race)


class CookerRaceTests(unittest.TestCase):
    def test_independent_concurrent_publishers(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            mock = path / "mock_cooker.py"
            mock.write_text("""import os, sys
try:
    fd = os.open(sys.argv[1], os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
except FileExistsError:
    sys.exit(6)
with os.fdopen(fd, 'wb') as handle:
    handle.write(b'SIRLBC01')
""")
            output = path / "result.sirbc"
            results = race.run_pair([sys.executable, str(mock), str(output)])
            self.assertTrue(race.unique_publisher([r.returncode for r in results]))
            self.assertEqual(output.read_bytes(), b"SIRLBC01")

    def test_rejects_double_success_or_signal_failure(self):
        self.assertFalse(race.unique_publisher([0, 0]))
        self.assertFalse(race.unique_publisher([-13, 6]))
        self.assertFalse(race.unique_publisher([0, -13]))
        self.assertTrue(race.unique_publisher([0, 6]))


if __name__ == "__main__":
    unittest.main()
