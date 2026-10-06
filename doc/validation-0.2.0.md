# deskup 0.2.0 validation

Validated on Windows x64 with Flutter 3.47.5 / Dart 3.13.4 and the pinned
Velopack 1.2.161 SDK on 2026-10-06.

- Flutter analysis: no issues.
- Plugin Dart tests: 7 passed; example widget test: 1 passed.
- Windows release example: compiled successfully.
- Windows native integration smoke test: passed.
- Python configuration, publication and native SDK protocol regressions:
  19 passed, including mixed Release attachments, prerelease filtering,
  checksum rejection/retry and HTTP failure/retry.
- An isolated real Gitea 28.0.0 Docker instance hosted a public test repository
  with two published Releases, each containing an index, a full update package,
  an EXE and a ZIP. The pinned SDK discovered 1.1.0 with prereleases disabled,
  discovered 2.0.0-beta.1 with prereleases enabled, and downloaded and verified
  the expected package in both cases. The temporary container and its volumes
  were removed after validation.

The SDK tests and real-server check used synthetic installation metadata and
package bytes. They did not run an installer, apply an update, restart an
application, or verify preserved host state. Perform the real packaged-host
upgrade scenario in [Gitea setup](gitea.md#verification) before shipping an
application that uses this source. macOS behavior is unchanged; macOS builds and
tests are covered by repository CI rather than this Windows workstation.
