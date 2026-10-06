"""Compile and exercise the actual Windows channel configuration parser."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = Path(os.environ.get('DESKUP_TEST_FLUTTER_INCLUDE',
    ROOT / 'example/windows/flutter/ephemeral/cpp_client_wrapper/include'))


@unittest.skipUnless(os.name == 'nt' and shutil.which('cmake') and
                    (INCLUDE / 'flutter/encodable_value.h').exists(),
                    'Requires Windows, CMake and a built Flutter example')
class RuntimeConfigurationTests(unittest.TestCase):
    def test_native_parser(self):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run(['cmake', '-S', str(ROOT / 'test/native'), '-B', directory,
                            '-DFLUTTER_WRAPPER_INCLUDE=' + INCLUDE.as_posix()], check=True)
            subprocess.run(['cmake', '--build', directory, '--config', 'Release'], check=True)
            subprocess.run(['ctest', '--test-dir', directory, '-C', 'Release', '--output-on-failure'], check=True)


if __name__ == '__main__':
    unittest.main()
