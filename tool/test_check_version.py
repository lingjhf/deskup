import tempfile
from pathlib import Path
import unittest

from check_version import validate


class VersionChecks(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / 'macos').mkdir()

    def metadata(self, version='0.0.1', pod=None, changelog=None):
        (self.root / 'pubspec.yaml').write_text('version: ' + version + '\n')
        (self.root / 'macos/deskup.podspec').write_text(
            "s.version = '" + (pod or version) + "'\n")
        (self.root / 'CHANGELOG.md').write_text('## ' + (changelog or version) + '\n')

    def test_stable_and_prerelease_tags(self):
        for version in ['0.0.1', '1.2.3', '1.0.0-beta.1', '1.0.0+build.001']:
            with self.subTest(version=version):
                self.metadata(version)
                self.assertEqual(validate(self.root, 'v' + version), version)

    def test_invalid_semver_is_rejected(self):
        for version in ['1.2', '01.2.3', '1.0.0-beta.01', '1.0.0-', '1.0.0+', 'v1.0.0']:
            with self.subTest(version=version):
                self.metadata(version)
                with self.assertRaises(ValueError):
                    validate(self.root)

    def test_mismatching_tag_is_rejected(self):
        self.metadata()
        for tag in ['0.0.1', 'v0.0.2', 'v0.0.1-beta.1']:
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                validate(self.root, tag)

    def test_mismatching_pod_version_is_rejected(self):
        self.metadata(pod='0.0.2')
        with self.assertRaises(ValueError):
            validate(self.root)

    def test_missing_changelog_entry_is_rejected(self):
        self.metadata(changelog='0.0.2')
        with self.assertRaises(ValueError):
            validate(self.root)


if __name__ == '__main__':
    unittest.main()
