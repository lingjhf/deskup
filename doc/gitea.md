# Windows updates from Gitea Releases

deskup 0.2.0 uses the official `vpkc_new_source_gitea` API in the pinned
Velopack 1.2.161 SDK. No extra update server or custom attachment resolver is
required. macOS continues to use a Sparkle appcast.

## Host configuration

Set these variables in the host `windows/CMakeLists.txt` before including
`flutter/generated_plugins.cmake`:

```cmake
set(DESKUP_WINDOWS_UPDATE_SOURCE "gitea")
set(DESKUP_WINDOWS_UPDATE_URL "https://git.example.com/team/app")
set(DESKUP_UPDATE_PACKAGE_ID "MyApp")
set(DESKUP_GITEA_INCLUDE_PRERELEASES OFF)
```

The URL is the repository page, not a clone URL ending in `.git`, a Releases
page, an API endpoint, or a particular tag. HTTPS is required. Gitea must be
hosted at the origin root, with the API under `/api/v1`; subpath hosting is not
supported by the pinned SDK. Ports and a trailing slash are accepted.

Keep the Windows startup hook and installation save gate described in the
[README](../README.md#windows-setup). Dart APIs do not change. A normal Flutter
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
- `DESKUP_GITEA_INCLUDE_PRERELEASES` defaults to OFF and filters out Gitea
  Releases marked prerelease. ON includes both stable and prerelease releases;
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

## Migration from 0.1.0

All configured Windows hosts must now explicitly choose
`DESKUP_WINDOWS_UPDATE_SOURCE`. Existing static directories use `web` and keep
their existing URL. Gitea hosts use `gitea` and set a repository URL. Configuration
is compiled into the binary, so changing the update origin requires distributing
a rebuilt application. For an existing deployed population, keep its old feed
available long enough to distribute the version that changes sources.

## Verification

The CMake configuration tests run on any machine with CMake. Windows SDK
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
