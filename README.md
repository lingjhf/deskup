# deskup

Desktop application updates using **Sparkle 2.10.0** on macOS and the official
**Velopack 1.2.161 C API** on Windows. The Windows SDK archive is pinned and checked
with SHA-256. No .NET runtime is required on user machines; `vpk` is a release tool.

## Dart API

Own one `Deskup` instance per Flutter engine. Subscribe before initialization:

```dart
final updater = Deskup(
  windows: WindowsUpdateConfiguration(
    source: GiteaUpdateSource(
      repositoryUrl: Uri.parse('https://git.example.com/team/app'),
    ),
    expectedPackageId: 'MyApp',
  ),
);
final subscription = updater.events.listen((status) {
  print('${status.phase.name}: ${status.progress}%');
  if (status.error.isNotEmpty) print(status.error);
});
await updater.initialize();
final status = await updater.status();
if (status.canCheck) await updater.check();
await updater.setAutomaticChecks(true);
// When finished:
await subscription.cancel();
updater.dispose();
```

### Initialization and lifecycle

`initialize()` installs the event handler and requests native startup. Its future
completing does not guarantee that the Windows SDK has finished initializing;
observe `configured` and `canCheck` through `status()` and `events`. A missing feed
or unpackaged Windows build remains unconfigured. On macOS, Sparkle owns its native
UI and does not emit the Windows operation status stream.

Repeated and concurrent initialization calls share one request. A failed request
can be retried. Use exactly one instance per Flutter engine: instances on the same
channel share a native event handler and must not compete for it.

`dispose()` is idempotent, detaches the event handler and closes the event stream.
It does not cancel native background work or an outstanding initialization request.
That request may still complete or fail. After disposal, calls on the instance fail
with `StateError`. Native configuration remains fixed for the lifetime of the engine;
recreating a Dart client does not reset or reconfigure the native updater.

`UpdateStatus` reports configuration, current version/build, automatic checking,
phase, available version, release notes (Markdown/HTML), full package size in bytes,
progress, error, and platform capabilities. Treat release notes as untrusted content. Missing feeds
and ordinary unpackaged Windows builds report `configured: false`.

| Capability | macOS | Windows |
|---|---|---|
| Check / automatic checks | Sparkle native UI | Status events |
| Download / install from Dart | Not supported | Supported |
| Progress and operation errors | Sparkle native UI | Status events |

`check()` and `download()` on Windows start background operations. Observe `events`
for completion and `UpdateStatus.error` for asynchronous failures. Invalid commands
and installation failures throw `PlatformException` with the native error code.
Calling Windows-only operations on macOS throws `MissingPluginException`.
`status()` returns a snapshot; subscribe to events before requesting it so consumers
can avoid replacing a newer event with an older snapshot.

## macOS setup

The plugin supports Swift Package Manager and CocoaPods. Set these keys in the
**host application's** Info.plist before signing:

- `SUFeedURL`: HTTPS Sparkle appcast URL.
- `SUPublicEDKey`: base64 Ed25519 public key (32 decoded bytes).
- `SUEnableAutomaticChecks`: `false` to keep automatic checks disabled initially.

Use `MacOSUpdateConfiguration(feedUrl: ..., automaticCheckInterval: ...)` to
override the appcast and Sparkle interval. A null URL uses the bundled `SUFeedURL`.
The Ed25519 key always remains in Info.plist. URL overrides use a Sparkle delegate,
without persisting a new feed URL. An explicit interval overrides Sparkle's saved
interval; omit it to retain Sparkle's bundle/user policy. Automatic-check consent
is never overwritten during initialization.
Local test feeds require `allowLocalHttp: true`, localhost/127.0.0.1 and an explicit
port. The old `DeskupAllowLocalHTTP` key is not read.
Sparkle owns download, verification, installation and relaunch UI.

**The host owns safe termination.** If documents must be saved, implement
`applicationShouldTerminate` in the host AppDelegate: return `.terminateLater`,
await Flutter's save/cleanup acknowledgement, then call
`reply(toApplicationShouldTerminate:)`. Reply `false` on a failed save. This gate
also protects Sparkle installation/relaunch. Do not override the application
AppDelegate from a plugin.

## Windows setup

Windows configuration is supplied through `Deskup(windows: ...)`, not CMake.
Omit it to disable Windows updates. Each client snapshots its immutable settings
before initialization; native code rejects invalid settings and reconfiguration.
The host still links startup integration after `flutter/generated_plugins.cmake`:

```cmake
target_link_libraries(${BINARY_NAME} PRIVATE deskup_startup)
```

| Setting | Default / bounds |
| --- | --- |
| `source` | Required `WebUpdateSource` or `GiteaUpdateSource` |
| `expectedPackageId` | Optional assertion; actual identity comes from installation metadata |
| `channel` | Installed channel unless explicitly overridden |
| `automaticCheckInterval` | 24 hours; one minute to 30 days |
| `allowDowngrade` | false |
| `maximumDeltas` | 10; -1 disables deltas, otherwise 0–100 |
| `preferenceNamespace` | Deskup; ASCII identity-style name |
| Web `timeout` | 10 minutes; one millisecond to one hour |
| Gitea `includePrereleases` | false; includes stable releases too when true |
| Source `allowLocalHttp` | false; loopback and explicit port only |

Gitea uses SDK HTTP defaults and has no configurable timeout in this SDK.
No private repository credentials are accepted. Hosts own source selection and
must not accept arbitrary remote/user-provided update origins without a policy.

At the beginning of `wWinMain`, before Flutter and COM initialization:

```cpp
#include <deskup/deskup_startup.h>

int APIENTRY wWinMain(/* your existing arguments */) {
  DeskupRunStartup();
  // Existing application initialization follows.
}
```

This startup hook handles Velopack installation events. It disables implicit
application of pending updates on startup. The hook must run before plugin
registration, so it cannot be invoked from Dart.

Use `vpk` 1.2.161 to package the application and publish a matching update feed.
Test installed or Velopack Portable packages; a normal Flutter build has no
`sq.version` installation metadata and cannot update. Package identity is checked
against both the installation and downloaded release metadata.

Use different package identities and feed directories for development and
production. Automatic checks default to off. When enabled they check at startup;
a minute timer enforces the configured minimum interval. Preferences use
`HKCU\Software\<preferenceNamespace>\<installed identity>\Updates`; changing
namespace intentionally isolates preferences. Initialization retains the saved
on/off choice. SDK and startup hooks remain build-time concerns. For offline
builds, set `DESKUP_VELOPACK_ARCHIVE` to the official SDK ZIP; its SHA-256 is checked.
For Gitea release requirements, see [Gitea Releases](doc/gitea.md).

Before installation, validate availability and save host application state:

```dart
await updater.validateInstall();
await saveDocumentsAndPauseTasks(); // Host responsibility; failure stops here.
try {
  await updater.install(
    silent: true,
    restart: true,
    restartArguments: ['--restore-workspace'],
  );
} catch (_) {
  await restoreWorkspace();
  rethrow;
}
```

`install` defaults to `silent: true`, `restart: true`, and no restart arguments.
On successful scheduling the host window closes. `restart: false` still exits the
host but does not relaunch it; nonempty arguments require `restart: true`. Argument
strings are passed to the SDK, not through a shell. Silent means the SDK attempts
to suppress its UI; it does not guarantee that OS or dependency prompts never appear.
The host owns confirmation/progress UI and saves state before installation.
Scheduling completion cannot report errors that occur after the host exits.
On scheduling failure the future throws so the host can restore
its workspace. Network work runs off the window thread, and destroying the plugin
does not wait for an in-flight request.

## Ownership and validation

The plugin owns native dependency integration, channels and updating. The host owns
settings UI, project persistence, task coordination, package identity, signing,
release packaging, feed hosting, and data compatibility.

```sh
flutter analyze
flutter test
cd example
flutter test
flutter test integration_test/plugin_integration_test.dart -d macos
```

The native integration smoke test expects an example build without a configured
feed. Full update testing requires two signed/packaged versions and a test feed.
Cutdex retains its publication scripts and platform update scenarios, including
checksum failure, retry, saving before installation and recovery after failure.

## Continuous integration

`.github/workflows/ci.yml` runs on pull requests to `main`, pushes to `main`, version tags (`v*`), and
manual dispatch. It follows Cutdex Agent's `quality` / `test` / `checks` structure:

- **Quality (Ubuntu):** formatting and analysis with Flutter 3.47.5 / Dart 3.13.4,
  matching the package's current minimum Dart SDK.
- **Tests:** plugin and example widget tests on Ubuntu (Flutter 3.47.5 and latest
  stable), macOS, and Windows.
- **Desktop validation:** macOS and Windows build the release example, then run
  the native integration smoke test against their respective platform. This
  verifies registration, native dependencies, the Windows startup hook, and status
  retrieval without requiring a signed feed or an installed update package.
- **CI passed:** a single aggregate check suitable for branch protection; it fails if any job fails,
  is cancelled, or is skipped.

Actions are pinned to commit SHAs, checkout credentials are not persisted, and
validation jobs have read-only permissions; the publication job can request an OIDC token,
and only the release job can write repository contents. The plugin does not commit a root lockfile,
so dependency resolution uses `flutter pub get` rather than `--enforce-lockfile`.

These jobs validate the plugin independently of the Cutdex application. They do
not exercise downloading and applying a real update: that requires two separately
packaged versions and a dedicated test feed. After a valid version tag passes CI, the workflow publishes to pub.dev using
GitHub OIDC, then creates a GitHub Release with automatically generated notes.

## Branch protection and versions

`main` changes go through pull requests. Merging requires `CI passed`, an up-to-date
branch, and resolved review conversations. The approving review count is zero to
support a single-maintainer repository. Deleting `main` and force pushes are
blocked, with no administrator bypass.

Tags matching `v*` can be created only by repository administrators. A separate
ruleset prevents anyone from updating or deleting existing version tags.

The current package version is **0.3.0**. Use semantic versions and update
`pubspec.yaml`, `macos/deskup.podspec`, and `CHANGELOG.md` together in a pull request.
The quality job checks their consistency. After merging and passing CI, an
administrator can tag the main commit as `v<version>` (for example `v0.1.0`). The
publication job verifies the exact version and main ancestry before publishing.
The release job runs only after publication succeeds. A prerelease version such as `0.1.0-beta.1` creates a prerelease.



## License

MIT. See [LICENSE](LICENSE). The bundled Velopack SDK retains its own license
notice in `windows/velopack-LICENSE.txt`; Sparkle is distributed under its
upstream license.

### Automatic publication

Push a new `v<version>` tag to run the complete CI, validate its package version
and main ancestry, publish to pub.dev, and finally create its GitHub Release.
Publishing is restricted to tag **push** events; manual CI dispatches do not publish.
Enable GitHub Actions automated publishing on pub.dev with repository `lingjhf/deskup`
and tag pattern `v{{version}}`, allowing push events. These settings have been
verified for this package. Temporary OIDC credentials are provisioned immediately
before upload; no long-lived publishing secret is stored in GitHub.

On retries, the publication job checks the exact version on pub.dev. If it exists,
it verifies the archive checksum and compares published files with this commit,
including all tracked Dart/native source files. Matching content skips upload;
different content or registry errors fail the job and block GitHub Release creation.
Do not move existing tags; fix changed package contents with a new version and tag.
This workflow change does not create a new package version or tag.
