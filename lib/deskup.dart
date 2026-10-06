import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

export 'src/update_status.dart';
export 'src/update_configuration.dart';
import 'src/update_status.dart';
import 'src/update_configuration.dart';

/// Native desktop updates backed by Sparkle (macOS) and Velopack (Windows).
///
/// Own one instance per Flutter engine and dispose it when no longer needed.
class Deskup {
  /// Creates the engine's update client using [channel] for native communication.
  ///
  /// [windows] supplies the immutable Windows policy, or disables Windows
  /// updates when absent. [macos] overrides Sparkle settings independently.
  /// Invalid settings throw [ArgumentError] before native initialization.
  factory Deskup({
    WindowsUpdateConfiguration? windows,
    MacOSUpdateConfiguration macos = const MacOSUpdateConfiguration(),
    MethodChannel channel = const MethodChannel('deskup'),
  }) => Deskup._(channel, windows?.toMap(), macos.toMap());

  Deskup._(this._channel, this._windows, this._macos);

  final Map<String, Object?>? _windows;
  final Map<String, Object?> _macos;

  /// Whether the current platform is macOS or Windows.
  static bool get isSupported =>
      !kIsWeb &&
      (defaultTargetPlatform == TargetPlatform.macOS ||
          defaultTargetPlatform == TargetPlatform.windows);

  final MethodChannel _channel;
  final _events = StreamController<UpdateStatus>.broadcast();
  bool _started = false;
  Future<void>? _initialization;
  bool _disposed = false;

  /// The broadcast stream of Windows updater state changes.
  ///
  /// Subscribe before [initialize]. Sparkle uses its native UI on macOS and
  /// does not publish these operation events. Closing this client closes the
  /// stream, without cancelling native update work.
  Stream<UpdateStatus> get events => _events.stream;

  /// Installs the event handler and requests native updater startup.
  ///
  /// Repeated and concurrent calls share the same future. A failed startup
  /// request can be retried. On Windows, completion acknowledges startup rather
  /// than SDK readiness; observe [status] and [events] for configuration and
  /// availability. Missing configuration leaves the updater unavailable.
  ///
  /// Throws [StateError] after [dispose].
  Future<void> initialize() {
    if (_disposed) throw StateError('Deskup is disposed');
    return _initialization ??= _initialize();
  }

  Future<void> _initialize() async {
    _started = true;
    _channel.setMethodCallHandler((call) async {
      if (!_disposed && call.method == 'statusChanged') {
        _events.add(
          UpdateStatus.fromMap(
            Map<String, Object?>.from(call.arguments as Map),
          ),
        );
      } else if (call.method != 'statusChanged') {
        throw MissingPluginException();
      }
    });
    try {
      await _channel.invokeMethod<void>('ready', {
        'windows': _windows,
        'macos': _macos,
      });
    } catch (_) {
      _started = false;
      _initialization = null;
      _channel.setMethodCallHandler(null);
      rethrow;
    }
  }

  /// Retrieves a snapshot of native state and platform capabilities.
  ///
  /// Subscribe to [events] first and avoid replacing a newer event with an
  /// older snapshot. Throws [StateError] after [dispose].
  Future<UpdateStatus> status() async {
    if (_disposed) throw StateError('Deskup is disposed');
    return UpdateStatus.fromMap(
      await _channel.invokeMapMethod<String, Object?>('status') ?? const {},
    );
  }

  /// Requests an update check.
  ///
  /// Sparkle presents its native UI. Windows starts background work and reports
  /// completion and asynchronous errors through [events]. Native command errors
  /// throw [PlatformException].
  Future<void> check() => _invoke('check');

  /// Starts downloading the available Windows update.
  ///
  /// Requires an available update. Observe [events] for completion and errors.
  /// Throws [MissingPluginException] on macOS.
  Future<void> download() => _invoke('download');

  /// Checks whether a downloaded Windows update is ready to install.
  ///
  /// Call before saving host state. This does not reserve the update or install
  /// it. Throws [PlatformException] when unavailable and [MissingPluginException]
  /// on macOS.
  Future<void> validateInstall() => _invoke('validateInstall');

  /// Applies the downloaded Windows update after the host has saved its state.
  ///
  /// Successful scheduling closes the host window and lets Velopack apply the
  /// update and relaunch the app. Completion does not confirm that the relaunched
  /// app started successfully. Throws [PlatformException] on scheduling failure
  /// and [MissingPluginException] on macOS.
  /// [silent] suppresses Velopack UI where possible. [restart] controls relaunch;
  /// the host still exits when it is `false`. [restartArguments] are passed only
  /// to the new process and require [restart]. These options are Windows-only.
  Future<void> install({
    bool silent = true,
    bool restart = true,
    List<String> restartArguments = const [],
  }) {
    if ((!restart && restartArguments.isNotEmpty) ||
        restartArguments.any((value) => value.contains('\u0000'))) {
      throw ArgumentError('Invalid restart arguments');
    }
    return _invoke('install', {
      'silent': silent,
      'restart': restart,
      'restartArguments': List<String>.of(restartArguments),
    });
  }

  /// Persists whether the native updater checks automatically.
  ///
  /// Requires native update configuration. Timing follows the platform updater.
  Future<void> setAutomaticChecks(bool enabled) =>
      _invoke('automaticChecks', enabled);

  Future<void> _invoke(String method, [Object? arguments]) {
    if (_disposed) throw StateError('Deskup is disposed');
    return _channel.invokeMethod<void>(method, arguments);
  }

  /// Detaches status callbacks and closes the event stream.
  ///
  /// Repeated calls have no effect. Native work is not cancelled; an outstanding
  /// [initialize] call can still complete or fail. Subsequent operations throw
  /// [StateError].
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    if (_started) _channel.setMethodCallHandler(null);
    unawaited(_events.close());
  }
}
