## 0.3.0

- Breaking: replace Windows CMake settings with immutable typed Dart source and policy configuration.
- Add source, channel, prerelease, automatic interval, web timeout, downgrade and delta options.
- Add per-install silent, restart and restart-argument options.
- Expose release notes and full package size in Windows status.
- Add explicit Sparkle feed and interval overrides, retaining the bundled signature key.

## 0.2.0

- Add an opt-in Windows Gitea Releases source using the pinned official Velopack C API.
- Breaking: configured Windows hosts must explicitly choose `web` or `gitea` with `DESKUP_WINDOWS_UPDATE_SOURCE`.
- Support stable-only or prerelease-inclusive Gitea discovery with existing packaged channels.
- Validate repository URLs and document release assets, public repository scope, and SDK limitations.
- Add build configuration and native SDK Gitea protocol regression tests.

## 0.1.0

- License the plugin under MIT and finalize maintainer metadata.

- Document the public API contract for initialization, platform capabilities,
  asynchronous operations, status snapshots, and instance disposal.
- Separate Windows SDK resource ownership, environment helpers, and channel
  coordination without changing update behavior or native error codes.
- Cover concurrent initialization and disposal during startup with lifecycle tests.
- Validate macOS and Windows Release builds and native integration tests in CI.

## 0.0.1

- Migrate the Cutdex Sparkle 2.10.0 and Velopack 1.2.161 implementations.
- Add typed update status, platform capabilities, automatic checks and Windows download/install operations.
- Provide a Windows startup hook and documented host lifecycle integration.
