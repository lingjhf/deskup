import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:deskup/deskup.dart';
import 'package:flutter/services.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  testWidgets('native registration reports host version without a feed', (
    tester,
  ) async {
    const channel = MethodChannel('deskup');
    await expectLater(
      channel.invokeMethod<void>('ready', {
        'windows': {
          'source': {'type': 'unknown'},
        },
        'macos': {},
      }),
      throwsA(
        isA<PlatformException>().having(
          (error) => error.code,
          'code',
          'invalidConfiguration',
        ),
      ),
    );
    final updater = Deskup();
    addTearDown(updater.dispose);
    await updater.initialize();
    final status = await updater.status();
    expect(status.version, isNotEmpty);
    expect(status.configured, isFalse);
    await expectLater(
      channel.invokeMethod<void>('ready', {
        'windows': null,
        'macos': {'allowLocalHttp': false},
      }),
      throwsA(
        isA<PlatformException>().having(
          (error) => error.code,
          'code',
          'alreadyInitialized',
        ),
      ),
    );
  });
}
