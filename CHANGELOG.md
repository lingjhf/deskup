## 0.1.0

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
