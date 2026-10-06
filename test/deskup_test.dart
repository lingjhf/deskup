import 'dart:async';

import 'package:deskup/deskup.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('deskup-test');
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
  Future<void> event(Map<String, Object?> value) async {
    await messenger.handlePlatformMessage(
      channel.name,
      const StandardMethodCodec().encodeMethodCall(
        MethodCall('statusChanged', value),
      ),
      (_) {},
    );
  }

  tearDown(() => messenger.setMockMethodCallHandler(channel, null));

  test(
    'initializes once, receives typed download state and closes stream',
    () async {
      var starts = 0;
      messenger.setMockMethodCallHandler(channel, (call) async {
        if (call.method == 'ready') starts++;
        return null;
      });
      final updater = Deskup(channel: channel);
      await updater.initialize();
      await updater.initialize();
      expect(starts, 1);
      final next = updater.events.first;
      await event({
        'phase': 'downloading',
        'progress': 42,
        'supportsDownload': true,
      });
      final status = await next;
      expect(status.phase, UpdatePhase.downloading);
      expect(status.progress, 42);
      expect(status.supportsDownload, isTrue);
      final done = expectLater(updater.events, emitsDone);
      updater.dispose();
      await done;
      expect(updater.check, throwsStateError);
    },
  );

  test('concurrent initialization shares one native request', () async {
    final ready = Completer<void>();
    var starts = 0;
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'ready') {
        starts++;
        await ready.future;
      }
      return null;
    });
    final updater = Deskup(channel: channel);
    addTearDown(updater.dispose);
    final first = updater.initialize();
    final second = updater.initialize();
    expect(identical(first, second), isTrue);
    ready.complete();
    await Future.wait([first, second]);
    expect(starts, 1);
  });

  for (final fail in [false, true]) {
    test('dispose during initialization with failure=$fail', () async {
      final ready = Completer<void>();
      messenger.setMockMethodCallHandler(channel, (call) async {
        await ready.future;
        if (fail) throw PlatformException(code: 'startupFailed');
        return null;
      });
      final updater = Deskup(channel: channel);
      final initialization = updater.initialize();
      final outcome = fail
          ? expectLater(initialization, throwsA(isA<PlatformException>()))
          : initialization;
      final closed = expectLater(updater.events, emitsDone);
      updater.dispose();
      updater.dispose();
      await event({'phase': 'available'});
      ready.complete();
      await outcome;
      await closed;
      expect(updater.initialize, throwsStateError);
      await expectLater(updater.status(), throwsStateError);
      expect(updater.check, throwsStateError);
    });
  }

  test('failed initialization can be retried', () async {
    var attempts = 0;
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (++attempts == 1) throw PlatformException(code: 'startupFailed');
      return null;
    });
    final updater = Deskup(channel: channel);
    addTearDown(updater.dispose);
    await expectLater(updater.initialize(), throwsA(isA<PlatformException>()));
    await updater.initialize();
    expect(attempts, 2);
  });

  test(
    'macOS status exposes native UI capabilities and unknown phase is idle',
    () async {
      messenger.setMockMethodCallHandler(
        channel,
        (_) async => {
          'configured': true,
          'canCheck': true,
          'phase': 'futurePhase',
          'supportsDownload': false,
          'supportsInstall': false,
        },
      );
      final updater = Deskup(channel: channel);
      addTearDown(updater.dispose);
      final status = await updater.status();
      expect(status.configured, isTrue);
      expect(status.supportsDownload, isFalse);
      expect(status.supportsInstall, isFalse);
      expect(status.phase, UpdatePhase.idle);
    },
  );

  test('installation failure retains native error code', () async {
    messenger.setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'install') {
        throw PlatformException(code: 'installFailed');
      }
      return null;
    });
    final updater = Deskup(channel: channel);
    addTearDown(updater.dispose);
    await expectLater(
      updater.install(),
      throwsA(
        isA<PlatformException>().having((e) => e.code, 'code', 'installFailed'),
      ),
    );
  });

  test(
    'initialization snapshots configuration and install passes options',
    () async {
      final calls = <MethodCall>[];
      messenger.setMockMethodCallHandler(channel, (call) async {
        calls.add(call);
        return null;
      });
      final updater = Deskup(
        channel: channel,
        windows: WindowsUpdateConfiguration(
          source: WebUpdateSource(
            url: Uri.parse('https://updates.example.com/app'),
            timeout: const Duration(seconds: 40),
          ),
          expectedPackageId: 'App',
        ),
      );
      addTearDown(updater.dispose);
      await updater.initialize();
      final windows = (calls.single.arguments as Map)['windows'] as Map;
      expect((windows['source'] as Map)['timeoutMilliseconds'], 40000);
      await updater.install(
        silent: false,
        restartArguments: ['--project', '文档 with spaces'],
      );
      expect(calls.last.arguments, {
        'silent': false,
        'restart': true,
        'restartArguments': ['--project', '文档 with spaces'],
      });
      await updater.install(restart: false);
      expect(calls.last.arguments, {
        'silent': true,
        'restart': false,
        'restartArguments': <String>[],
      });
      expect(
        () => updater.install(restart: false, restartArguments: ['--open']),
        throwsArgumentError,
      );
      expect(
        () => updater.install(restartArguments: ['bad\u0000argument']),
        throwsArgumentError,
      );
    },
  );
}
