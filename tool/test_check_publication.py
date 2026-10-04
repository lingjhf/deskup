import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from check_publication import already_published, verify_archive


class PublicationChecks(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.files = {
            'pubspec.yaml': b'name: example\nversion: 1.0.0\n',
            'README.md': b'readme', 'CHANGELOG.md': b'changes', 'LICENSE': b'MIT',
            'lib/example.dart': b'void main() {}',
        }
        for name, data in self.files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        mock = patch('check_publication.subprocess.check_output', return_value=b'lib/example.dart\0')
        mock.start()
        self.addCleanup(mock.stop)

    def archive(self, files=None):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w:gz') as bundle:
            for name, data in (files or self.files).items():
                info = tarfile.TarInfo(name)
                info.size = len(data)
                bundle.addfile(info, io.BytesIO(data))
        return stream.getvalue()

    def test_matching_publication_can_be_skipped(self):
        verify_archive(self.root, self.archive())

    def test_hidden_git_metadata_is_not_required_in_pub_archive(self):
        with patch('check_publication.subprocess.check_output',
                   return_value=b'lib/example.dart\0windows/.gitignore\0'):
            verify_archive(self.root, self.archive())

    def test_different_published_bytes_are_rejected(self):
        (self.root / 'lib/example.dart').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'differs'):
            verify_archive(self.root, self.archive())

    def test_missing_source_is_rejected(self):
        files = dict(self.files)
        del files['lib/example.dart']
        with self.assertRaisesRegex(ValueError, 'missing source'):
            verify_archive(self.root, self.archive(files))

    def test_unsafe_archive_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            verify_archive(self.root, self.archive({'../outside': b'bad'}))

    def test_not_found_is_the_only_missing_version_response(self):
        for code in [404, 403, 500]:
            with self.subTest(code=code), patch('check_publication.urlopen',
                    side_effect=HTTPError('https://pub.dev', code, 'error', {}, None)):
                if code == 404:
                    self.assertFalse(already_published(self.root))
                else:
                    with self.assertRaises(HTTPError):
                        already_published(self.root)


if __name__ == '__main__':
    unittest.main()
