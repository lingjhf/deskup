/// A Windows update feed supported by the bundled Velopack SDK.
sealed class WindowsUpdateSource {
  const WindowsUpdateSource();

  /// Encodes and validates the source for the native bridge.
  Map<String, Object?> toMap();
}

void _validateUrl(Uri url, bool allowLocalHttp) {
  final local =
      allowLocalHttp &&
      url.scheme == 'http' &&
      url.hasPort &&
      (url.host == 'localhost' || url.host == '127.0.0.1');
  if ((!local && url.scheme != 'https') ||
      url.host.isEmpty ||
      !RegExp(r'^[A-Za-z0-9.-]+$').hasMatch(url.host) ||
      !RegExp(r'^[A-Za-z0-9_./%~-]*$').hasMatch(url.path) ||
      (url.hasPort && (url.port < 1 || url.port > 65535)) ||
      url.userInfo.isNotEmpty ||
      url.hasQuery ||
      url.hasFragment) {
    throw ArgumentError.value(
      url,
      'url',
      'Use HTTPS or explicitly enabled loopback HTTP',
    );
  }
}

/// An HTTP directory containing Velopack channel indexes and packages.
final class WebUpdateSource extends WindowsUpdateSource {
  /// Creates a directory source with a bounded network timeout.
  const WebUpdateSource({
    required this.url,
    this.timeout = const Duration(minutes: 10),
    this.allowLocalHttp = false,
  });

  /// The directory URL, without credentials, query or fragment.
  final Uri url;

  /// The network request timeout, from one millisecond to one hour.
  final Duration timeout;

  /// Whether HTTP with an explicit port is allowed on loopback for testing.
  final bool allowLocalHttp;

  @override
  Map<String, Object?> toMap() {
    _validateUrl(url, allowLocalHttp);
    if (timeout.inMilliseconds < 1 || timeout.inMilliseconds > 3600000) {
      throw ArgumentError.value(timeout, 'timeout');
    }
    return {
      'type': 'web',
      'url': url.toString(),
      'timeoutMilliseconds': timeout.inMilliseconds,
      'allowLocalHttp': allowLocalHttp,
    };
  }
}

/// A public Gitea repository whose releases contain Velopack packages.
final class GiteaUpdateSource extends WindowsUpdateSource {
  /// Creates a repository source with an explicit prerelease policy.
  const GiteaUpdateSource({
    required this.repositoryUrl,
    this.includePrereleases = false,
    this.allowLocalHttp = false,
  });

  /// The origin-root repository URL, such as `https://host/owner/repo`.
  final Uri repositoryUrl;

  /// Whether discovery includes releases marked as prereleases.
  final bool includePrereleases;

  /// Whether HTTP with an explicit port is allowed on loopback for testing.
  final bool allowLocalHttp;

  @override
  Map<String, Object?> toMap() {
    _validateUrl(repositoryUrl, allowLocalHttp);
    if (!RegExp(r'^/[A-Za-z0-9_.~-]+/[A-Za-z0-9_.~-]+/?$')
            .hasMatch(repositoryUrl.path) ||
        repositoryUrl.path.replaceFirst(RegExp(r'/$'), '').endsWith('.git')) {
      throw ArgumentError.value(
        repositoryUrl,
        'repositoryUrl',
        'Use an origin-root repository URL',
      );
    }
    return {
      'type': 'gitea',
      'url': repositoryUrl.toString(),
      'includePrereleases': includePrereleases,
      'allowLocalHttp': allowLocalHttp,
    };
  }
}

/// Immutable Windows settings applied once during native initialization.
final class WindowsUpdateConfiguration {
  /// Creates a Windows update policy without enabling automatic checks.
  const WindowsUpdateConfiguration({
    required this.source,
    this.expectedPackageId,
    this.channel,
    this.automaticCheckInterval = const Duration(hours: 24),
    this.allowDowngrade = false,
    this.maximumDeltas = 10,
    this.preferenceNamespace = 'Deskup',
  });

  /// The update feed and its source-specific options.
  final WindowsUpdateSource source;

  /// The optional identity to verify against installed and downloaded metadata.
  final String? expectedPackageId;

  /// The channel override, or `null` to use the installed package's channel.
  final String? channel;

  /// The minimum interval between automatic checks, from one minute to 30 days.
  final Duration automaticCheckInterval;

  /// Whether channel switches or retracted releases may offer a lower version.
  final bool allowDowngrade;

  /// The maximum delta count, or `-1` to disable delta downloads.
  final int maximumDeltas;

  /// The HKCU Software namespace used with the installed package identity.
  final String preferenceNamespace;

  /// Encodes validated settings for the native bridge.
  Map<String, Object?> toMap() {
    final id = RegExp(r'^[A-Za-z0-9][A-Za-z0-9_.-]*$');
    if ((expectedPackageId != null && !id.hasMatch(expectedPackageId!)) ||
        (channel != null && !id.hasMatch(channel!)) ||
        !id.hasMatch(preferenceNamespace) ||
        maximumDeltas < -1 ||
        maximumDeltas > 100 ||
        automaticCheckInterval.inSeconds < 60 ||
        automaticCheckInterval.inSeconds > 2592000) {
      throw ArgumentError('Invalid Windows update policy');
    }
    return {
      'source': source.toMap(),
      'expectedPackageId': expectedPackageId,
      'channel': channel,
      'automaticCheckIntervalSeconds': automaticCheckInterval.inSeconds,
      'allowDowngrade': allowDowngrade,
      'maximumDeltas': maximumDeltas,
      'preferenceNamespace': preferenceNamespace,
    };
  }
}

/// Sparkle settings; its signature key remains in the application's Info.plist.
final class MacOSUpdateConfiguration {
  /// Creates a Sparkle policy with optional overrides of bundled settings.
  const MacOSUpdateConfiguration({
    this.feedUrl,
    this.automaticCheckInterval,
    this.allowLocalHttp = false,
  });

  /// The appcast URL, or `null` to use the application's `SUFeedURL`.
  final Uri? feedUrl;

  /// The optional Sparkle check interval, at least one hour and at most 30 days.
  final Duration? automaticCheckInterval;

  /// Whether loopback HTTP with an explicit port is allowed for testing.
  final bool allowLocalHttp;

  /// Encodes validated Sparkle settings for the native bridge.
  Map<String, Object?> toMap() {
    if (feedUrl != null) _validateUrl(feedUrl!, allowLocalHttp);
    if (automaticCheckInterval != null &&
        (automaticCheckInterval!.inSeconds < 3600 ||
            automaticCheckInterval!.inSeconds > 2592000)) {
      throw ArgumentError.value(
        automaticCheckInterval,
        'automaticCheckInterval',
      );
    }
    return {
      'feedUrl': feedUrl?.toString(),
      'automaticCheckIntervalSeconds': automaticCheckInterval?.inSeconds,
      'allowLocalHttp': allowLocalHttp,
    };
  }
}
