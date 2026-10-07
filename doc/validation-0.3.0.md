# deskup 0.3.0 validation

Validated locally on Windows with Flutter 3.47.5, Dart 3.13.4 and Velopack 1.2.161.

- 13 Dart tests passed, including source validation, channel/interval policies,
  install options and 64-bit package-size/release-note serialization.
- 16 Python tests passed, including the actual SDK Gitea fixture and the compiled
  Windows runtime parser. The parser target also passes with MSVC warnings treated
  as errors.
- Static analysis and Dart formatting passed.
- The Windows example release build and native integration test passed. Invalid
  channel configuration is rejected, and native initialization is immutable.
- Cutdex production compiled successfully against this source. Its 18 update
  integration/recovery/UI tests passed.
- Two real packaged Cutdex test runners completed the local HTTP update sequence:
  corrupted package rejection, download retry, silent application and relaunch.
  Restart arguments containing Chinese text and spaces survived the SDK boundary.
  The status exposed the downloaded full-package size.

The local HTTP update fixture is not a real Gitea server installation test. The
Gitea protocol tests exercise the official SDK with a synthetic installation and
do not apply an update. Existing 0.2.0 real-server evidence is recorded separately.
macOS API wiring was reviewed against Sparkle 2.10.0 headers but could not be built
or exercised on this Windows host; macOS CI remains required before publication.
No package tag or public release was created during these checks.
