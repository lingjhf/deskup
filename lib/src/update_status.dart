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

  final bool configured,
      canCheck,
      automaticChecks,
      supportsDownload,
      supportsInstall;
  final String version, build, availableVersion, error;
  final UpdatePhase phase;
  final int progress;

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
