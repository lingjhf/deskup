import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

export 'src/update_status.dart';
import 'src/update_status.dart';

/// Native desktop updates backed by Sparkle (macOS) and Velopack (Windows).
/// Own one instance per Flutter engine and dispose it when no longer needed.
class Deskup {
  factory Deskup({MethodChannel channel = const MethodChannel('deskup')}) =>
      Deskup._(channel);

  Deskup._(this._channel);

  static bool get isSupported =>
      !kIsWeb &&
      (defaultTargetPlatform == TargetPlatform.macOS ||
          defaultTargetPlatform == TargetPlatform.windows);

  final MethodChannel _channel;
  final _events = StreamController<UpdateStatus>.broadcast();
  bool _started = false;
  Future<void>? _initialization;
  bool _disposed = false;

  Stream<UpdateStatus> get events => _events.stream;

  /// Initializes updates after installing the status event handler.
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
      await _channel.invokeMethod<void>('ready');
    } catch (_) {
      _started = false;
      _initialization = null;
      _channel.setMethodCallHandler(null);
      rethrow;
    }
  }

  Future<UpdateStatus> status() async {
    if (_disposed) throw StateError('Deskup is disposed');
    return UpdateStatus.fromMap(
      await _channel.invokeMapMethod<String, Object?>('status') ?? const {},
    );
  }

  /// Sparkle presents its native update UI; Windows publishes status events.
  Future<void> check() => _invoke('check');

  /// Windows only. Observe [events] for download completion and errors.
  Future<void> download() => _invoke('download');
  Future<void> validateInstall() => _invoke('validateInstall');

  /// Windows only. Save application state before calling this method.
  /// A successful installation closes the host window and relaunches the app.
  Future<void> install() => _invoke('install');
  Future<void> setAutomaticChecks(bool enabled) =>
      _invoke('automaticChecks', enabled);

  Future<void> _invoke(String method, [Object? arguments]) {
    if (_disposed) throw StateError('Deskup is disposed');
    return _channel.invokeMethod<void>(method, arguments);
  }

  void dispose() {
    if (_disposed) return;
    _disposed = true;
    if (_started) _channel.setMethodCallHandler(null);
    unawaited(_events.close());
  }
}
