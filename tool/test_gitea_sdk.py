"""Windows ABI/protocol regressions against the pinned Velopack DLL.

Set DESKUP_TEST_VELOPACK_DLL to the built example's velopack_libc.dll.
The fixture speaks Gitea's public Releases API; it does not install an app.
"""
import ctypes as c
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch


class Locator(c.Structure):
    _fields_ = [(name, c.c_char_p) for name in
                ['root', 'update', 'packages', 'manifest', 'binary']] + [('portable', c.c_bool)]


class Asset(c.Structure):
    _fields_ = [(name, c.c_char_p) for name in
                ['id', 'version', 'type', 'filename', 'sha1', 'sha256']] + [
                ('size', c.c_uint64), ('markdown', c.c_char_p), ('html', c.c_char_p)]


class Update(c.Structure):
    _fields_ = [('target', c.POINTER(Asset)), ('base', c.POINTER(Asset)),
                ('deltas', c.POINTER(c.POINTER(Asset))), ('count', c.c_size_t), ('downgrade', c.c_bool)]


@unittest.skipUnless(os.name == 'nt' and os.environ.get('DESKUP_TEST_VELOPACK_DLL'),
                     'Requires Windows and DESKUP_TEST_VELOPACK_DLL')
class GiteaSdkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Keep loopback fixtures independent of developer/CI proxy settings.
        environment = {key: value for key, value in os.environ.items()
                       if key.lower() not in ['http_proxy', 'https_proxy', 'all_proxy']}
        proxy_patch = patch.dict(os.environ, environment, clear=True)
        proxy_patch.start()
        cls.addClassCleanup(proxy_patch.stop)
        cls.lib = c.CDLL(str(Path(os.environ['DESKUP_TEST_VELOPACK_DLL']).resolve()))
        signatures = {
            'vpkc_new_source_gitea': ([c.c_char_p, c.c_char_p, c.c_bool], c.c_void_p),
            'vpkc_new_update_manager_with_source': ([c.c_void_p, c.c_void_p, c.POINTER(Locator), c.POINTER(c.c_void_p)], c.c_bool),
            'vpkc_check_for_updates': ([c.c_void_p, c.POINTER(c.POINTER(Update))], c.c_int),
            'vpkc_download_updates': ([c.c_void_p, c.POINTER(Update), c.c_void_p, c.c_void_p], c.c_bool),
            'vpkc_get_last_error': ([c.c_char_p, c.c_size_t], c.c_size_t),
            'vpkc_free_update_info': ([c.POINTER(Update)], None),
            'vpkc_free_update_manager': ([c.c_void_p], None),
            'vpkc_free_source': ([c.c_void_p], None),
        }
        for name, (args, result) in signatures.items():
            function = getattr(cls.lib, name)
            function.argtypes, function.restype = args, result

    def error(self):
        buffer = c.create_string_buffer(4096)
        self.lib.vpkc_get_last_error(buffer, len(buffer))
        return buffer.value.decode()

    def scenario(self, prerelease=False, corrupt=False, unavailable=False):
        requests = []
        payload = b'deskup protocol fixture package'
        versions = [('1.1.0', False), ('2.0.0-beta.1', True)]

        class Handler(BaseHTTPRequestHandler):
            def do_GET(handler):
                requests.append(handler.path)
                origin = f'http://127.0.0.1:{handler.server.server_port}'
                if handler.path.startswith('/api/v1/repos/team/app/releases?'):
                    data = [{'name': version, 'prerelease': beta,
                             'published_at': f'2026-10-0{index + 1}T00:00:00Z',
                             'assets': [{'name': name, 'browser_download_url': origin + '/' + version + '/' + name}
                                        for name in ['releases.win-x64.json', version + '-full.nupkg', 'Setup.exe', 'portable.zip']]}
                            for index, (version, beta) in enumerate(versions)]
                    body = json.dumps(data).encode()
                elif handler.path.endswith('/releases.win-x64.json'):
                    version = handler.path.split('/')[1]
                    body = json.dumps({'Assets': [{'PackageId': 'DeskupTest', 'Version': version,
                        'Type': 'Full', 'FileName': version + '-full.nupkg',
                        'SHA1': hashlib.sha1(payload).hexdigest(), 'SHA256': hashlib.sha256(payload).hexdigest(),
                        'Size': len(payload), 'NotesMarkdown': '', 'NotesHtml': ''}]}).encode()
                elif handler.path.endswith('.nupkg'):
                    if unavailable:
                        handler.send_error(503)
                        return
                    body = b'corrupted' if corrupt else payload
                else:
                    handler.send_error(404)
                    return
                handler.send_response(200)
                handler.send_header('Content-Length', str(len(body)))
                handler.end_headers()
                handler.wfile.write(body)

            def log_message(self, *_):
                pass

        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'packages').mkdir()
                (root / 'Update.exe').touch()
                (root / 'sq.version').write_text('<package><metadata><id>DeskupTest</id><version>1.0.0</version>'
                    '<title>Test</title><authors>Test</authors><description>Test</description>'
                    '<mainExe>DeskupTest.exe</mainExe><channel>win-x64</channel></metadata></package>')
                locator = Locator(*[str(path).encode() for path in
                    [root, root / 'Update.exe', root / 'packages', root / 'sq.version', root]], True)
                source = self.lib.vpkc_new_source_gitea(
                    f'http://127.0.0.1:{server.server_port}/team/app'.encode(), None, prerelease)
                self.assertTrue(source, self.error())
                manager = c.c_void_p()
                update = c.POINTER(Update)()
                try:
                    self.assertTrue(self.lib.vpkc_new_update_manager_with_source(
                        source, None, c.byref(locator), c.byref(manager)), self.error())
                    self.assertEqual(self.lib.vpkc_check_for_updates(manager, c.byref(update)), 0, self.error())
                    self.assertTrue(update, self.error())
                    expected = '2.0.0-beta.1' if prerelease else '1.1.0'
                    self.assertEqual(update.contents.target.contents.version.decode(), expected)
                    downloaded = self.lib.vpkc_download_updates(manager, update, None, None)
                    self.assertEqual(downloaded, not (corrupt or unavailable), self.error())
                    if corrupt or unavailable:
                        # A failed request must remain retryable with the same update.
                        corrupt = unavailable = False
                        self.assertTrue(self.lib.vpkc_download_updates(manager, update, None, None), self.error())
                    self.assertIn('/' + expected + '/' + expected + '-full.nupkg', requests)
                    self.assertFalse(any(path.endswith(('Setup.exe', 'portable.zip')) for path in requests))
                finally:
                    if update:
                        self.lib.vpkc_free_update_info(update)
                    if manager:
                        self.lib.vpkc_free_update_manager(manager)
                    self.lib.vpkc_free_source(source)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_stable_release_with_mixed_attachments(self):
        self.scenario()

    def test_include_prereleases(self):
        self.scenario(prerelease=True)

    def test_corrupt_package_and_retry(self):
        self.scenario(corrupt=True)

    def test_download_failure_and_retry(self):
        self.scenario(unavailable=True)


if __name__ == '__main__':
    unittest.main()
