/// Phases reported by the Windows updater. Sparkle manages its own native UI.
enum UpdatePhase {
  idle,
  checking,
  current,
  available,
  downloading,
  ready,
  installing,
}

/// A snapshot of native updater state and platform capabilities.
class UpdateStatus {
  const UpdateStatus({
    this.configured = false,
    this.canCheck = false,
    this.automaticChecks = false,
    this.supportsDownload = false,
    this.supportsInstall = false,
    this.version = '',
    this.build = '',
    this.phase = UpdatePhase.idle,
    this.availableVersion = '',
    this.progress = 0,
    this.error = '',
  });

  /// Decodes a native snapshot, defaulting absent fields and unknown phases.
  factory UpdateStatus.fromMap(Map<String, Object?> map) => UpdateStatus(
    configured: map['configured'] as bool? ?? false,
    canCheck: map['canCheck'] as bool? ?? false,
    automaticChecks: map['automaticChecks'] as bool? ?? false,
    supportsDownload: map['supportsDownload'] as bool? ?? false,
    supportsInstall: map['supportsInstall'] as bool? ?? false,
    version: map['version'] as String? ?? '',
    build: map['build'] as String? ?? '',
    phase: UpdatePhase.values.firstWhere(
      (value) => value.name == map['phase'],
      orElse: () => UpdatePhase.idle,
    ),
    availableVersion: map['availableVersion'] as String? ?? '',
    progress: map['progress'] as int? ?? 0,
    error: map['error'] as String? ?? '',
  );

  /// Whether the native updater has usable configuration and installation metadata.
  final bool configured;

  /// Whether a check can be requested in the current native state.
  final bool canCheck;

  /// Whether automatic update checking is enabled.
  final bool automaticChecks;

  /// Whether the platform supports downloading through the Dart API.
  final bool supportsDownload;

  /// Whether the platform supports installing through the Dart API.
  final bool supportsInstall;

  /// The running application's version, or an empty string if unavailable.
  final String version;

  /// The running application's build, or an empty string if unavailable.
  final String build;

  /// The available or downloaded Windows update version.
  final String availableVersion;

  /// The latest asynchronous Windows operation error, or an empty string.
  ///
  /// This is a native message rather than a stable machine-readable error code.
  /// Command errors are delivered separately as platform exceptions.
  final String error;

  /// The Windows operation phase; macOS defaults to [UpdatePhase.idle].
  final UpdatePhase phase;

  /// The Windows download percentage from zero to one hundred.
  final int progress;

  /// Encodes this snapshot using the native channel field names.
  Map<String, Object?> toMap() => {
    'configured': configured,
    'canCheck': canCheck,
    'automaticChecks': automaticChecks,
    'supportsDownload': supportsDownload,
    'supportsInstall': supportsInstall,
    'version': version,
    'build': build,
    'phase': phase.name,
    'availableVersion': availableVersion,
    'progress': progress,
    'error': error,
  };
}
