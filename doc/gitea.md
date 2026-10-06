# Windows updates from Gitea Releases

deskup 0.3.0 uses the official `vpkc_new_source_gitea` API in the pinned
Velopack 1.2.161 SDK. No extra update server or custom attachment resolver is
required. macOS continues to use a Sparkle appcast.

## Host configuration

Pass immutable Dart configuration when creating the client:

```dart
final updater = Deskup(
  windows: WindowsUpdateConfiguration(
    source: GiteaUpdateSource(
      repositoryUrl: Uri.parse('https://git.example.com/team/app'),
      includePrereleases: false,
    ),
    expectedPackageId: 'MyApp',
    // channel: 'beta', // Optional override of the installed channel.
  ),
);
```

The URL is the repository page, not a clone URL ending in `.git`, a Releases
page, an API endpoint, or a particular tag. HTTPS is required. Gitea must be
hosted at the origin root, with the API under `/api/v1`; subpath hosting is not
supported by the pinned SDK. Ports and a trailing slash are accepted.

Keep the Windows startup hook and installation save gate described in the
[README](../README.md#windows-setup). Native configuration is fixed for the engine lifetime. A normal Flutter
build is unconfigured without Velopack installation metadata (`sq.version`).

This version supports **public repositories and anonymously downloadable
attachments**. It does not expose private repository authentication or embed
access tokens in distributed applications.

## Release attachments

Use `vpk` 1.2.161 to package the audited complete application:

```sh
vpk pack --packId MyApp --packVersion 1.0.0 --packDir path/to/app --mainExe myapp.exe --runtime win-x64 --channel win-x64 --outputDir path/to/releases --delta none
```

Upload the generated `releases.win-x64.json` and every `.nupkg` referenced by
that index into the **same Gitea Release**. Do not rename these attachments.
The generated installer EXE and Portable ZIP may be uploaded alongside them;
the updater discovers the index and downloads the referenced package rather
than choosing an EXE or ZIP. A manually created ZIP is not a substitute for a
Velopack Portable distribution.

Create a draft Release, upload all files, verify the index and package checksums,
then publish it. If an index contains packages from previous versions, keep all
referenced packages accessible within the SDK discovery window. A simpler
full-package workflow uses a fresh output directory for each release, so its
index references only the packages uploaded to that release. Versions must
increase according to SemVer. Build metadata such as `+42` does not increase
version precedence; use a new version or a numeric prerelease component.

## Channels and SDK limits

- The installed package's Velopack channel determines the index filename.
  `--channel win-x64` reads `releases.win-x64.json`; Windows ARM64 packages
  should use their own channel and matching assets.
- `includePrereleases` defaults to false and filters out Gitea
  Releases marked prerelease. true includes both stable and prerelease releases;
  it does not mean prerelease-only and does not switch the package channel.
- Separate development and production package identities and channels, or use
  separate repositories. Gitea tags alone do not isolate package identity.
- The pinned SDK reads only the first **10 Releases** returned by
  `/api/v1/repos/owner/repo/releases?limit=10&page=1&draft=false`, filters
  prereleases, and merges matching channel indexes. It chooses updates by
  package version, not the Release tag text. Heavy publishing of unrelated
  packages can push a channel outside this window; use a dedicated update
  repository if necessary.
- Missing, inaccessible or malformed index attachments are skipped by the SDK.
  A check can report no update when publishing is incomplete. Release API
  failures and package download/checksum failures are surfaced as operation
  errors. Publish complete releases and monitor attachment availability.
- The Gitea constructor uses SDK HTTP defaults; deskup's 10-minute web-source
  timeout does not apply to this source. Endpoint trust, HTTPS certificates,
  and attachment access must be managed by the host operator.

## Migration to 0.3.0

Remove Windows `DESKUP_WINDOWS_UPDATE_*`, `DESKUP_UPDATE_PACKAGE_ID`,
`DESKUP_GITEA_INCLUDE_PRERELEASES`, `DESKUP_PREFERENCE_ROOT`, and
`DESKUP_ALLOW_LOCAL_HTTP` CMake settings. They are no longer read. Use the typed
Dart configuration and per-call `install` options. Source settings are fixed
at native initialization, and downloaded packages are always checked against
the actual installed identity. Distribution still needs a new application build.
Keep an old feed available long enough to deliver a source-changing version.

## Verification

Dart configuration and channel tests validate serialization and policy boundaries.
Windows native parser tests compile the actual parser with Flutter headers. Windows SDK
protocol tests use a loopback Gitea API fixture and the actual pinned DLL:

```powershell
$env:DESKUP_TEST_VELOPACK_DLL = (Resolve-Path example/build/windows/x64/runner/Release/velopack_libc.dll).Path
python -m unittest discover -s tool -p 'test_*.py'
```

These tests cover stable/prerelease selection, mixed attachments, checksum
failure and retry, and HTTP download failure and retry. They use a synthetic
installation locator and package bytes and do **not** apply an update.
The example desktop build and native integration smoke test separately verify
plugin registration, startup and status.

Before deploying a host application, test two actual Velopack-packaged versions
against a real Gitea repository: install the old version, publish the new one,
check/download, save application state, apply/relaunch, verify the running version
and preserved state. Repeat for Portable distribution and any channels you ship.
Do not describe protocol fixture tests as a real-server installation test.
