"""Exercise actual CMake validation without Flutter or an SDK download."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required')
class ConfigurationTests(unittest.TestCase):
    def configure(self, **settings):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'config.cmake'
            script.write_text(
                '\n'.join(f'set(DESKUP_{key} "{value}")' for key, value in settings.items())
                + f'\ninclude("{ROOT.as_posix()}/windows/update_configuration.cmake")\n'
                + f'configure_file("{ROOT.as_posix()}/windows/update_config.h.in" '
                + f'"{Path(directory).as_posix()}/config.h" @ONLY)\n')
            result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
            header = Path(directory) / 'config.h'
            return result, header.read_text() if header.exists() else ''

    def test_web_and_unconfigured_builds(self):
        for settings in [{}, {'WINDOWS_UPDATE_SOURCE': 'web', 'WINDOWS_UPDATE_URL': 'https://updates.example.com/x64',
                              'UPDATE_PACKAGE_ID': 'Example'}]:
            result, header = self.configure(**settings)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('#define DESKUP_SOURCE_GITEA 0', header)

    def test_gitea_and_prereleases(self):
        for prerelease in ['OFF', 'ON']:
            result, header = self.configure(WINDOWS_UPDATE_SOURCE='gitea',
                WINDOWS_UPDATE_URL='https://git.example.com:443/team/app/',
                UPDATE_PACKAGE_ID='Example.Dev', GITEA_INCLUDE_PRERELEASES=prerelease)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('#define DESKUP_SOURCE_GITEA 1', header)
            self.assertIn('#define DESKUP_INCLUDE_PRERELEASES ' + str(int(prerelease == 'ON')), header)

    def test_reject_invalid_sources_and_repository_urls(self):
        for url in ['https://git.example.com/team/app/releases',
                    'https://git.example.com/api/v1/repos/team/app',
                    'https://git.example.com/gitea/team/app',
                    'https://git.example.com/team/app.git',
                    'https://git.example.com/team/app?token=secret',
                    'http://git.example.com/team/app']:
            result, _ = self.configure(WINDOWS_UPDATE_SOURCE='gitea',
                WINDOWS_UPDATE_URL=url, UPDATE_PACKAGE_ID='Example')
            self.assertNotEqual(result.returncode, 0, url)
        for settings in [{'WINDOWS_UPDATE_SOURCE': 'unknown'},
                         {'WINDOWS_UPDATE_URL': 'https://example.com', 'UPDATE_PACKAGE_ID': 'Example'},
                         {'GITEA_INCLUDE_PRERELEASES': 'ON'},
                         {'WINDOWS_UPDATE_URL': 'https://example.com', 'UPDATE_PACKAGE_ID': ''}]:
            result, _ = self.configure(**settings)
            self.assertNotEqual(result.returncode, 0)

    def test_loopback_requires_explicit_opt_in(self):
        for allowed in ['ON', 'OFF']:
            result, _ = self.configure(WINDOWS_UPDATE_SOURCE='gitea',
                WINDOWS_UPDATE_URL='http://127.0.0.1:8765/team/app',
                UPDATE_PACKAGE_ID='Example', ALLOW_LOCAL_HTTP=allowed)
            self.assertEqual(result.returncode == 0, allowed == 'ON', result.stderr)


if __name__ == '__main__':
    unittest.main()
