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
- A separate real Gitea instance hosted the actual `vpk`-generated 1.0.1
  installer, Portable ZIP, full `.nupkg` and channel index. A Flutter test host
  using deskup 0.2.0 was launched from the 1.0.0 Velopack Portable distribution.
  Through the public Dart API it discovered 1.0.1, downloaded it, validated
  installation, saved state, applied the update and relaunched. The new process
  reported version 1.0.1/build 2 and `phase: current`, with no error; the saved
  state was preserved. These were unsigned test packages over explicitly
  enabled loopback HTTP. The temporary container and volumes were removed.
- Repository CI passed formatting/analysis, plugin/example tests on Linux,
  Windows and macOS, and Windows/macOS native builds and integration smoke tests.

The SDK protocol regressions used synthetic metadata and package bytes; the
separate Portable end-to-end test used actual packaged Flutter applications and
applied an update. Installing through Setup.exe, production signing/HTTPS,
private repositories and application-specific persistence were not exercised.
Perform the host-specific scenario in [Gitea setup](gitea.md#verification) for
each application and distribution you ship. macOS behavior is unchanged;
macOS builds and tests were validated by repository CI.
