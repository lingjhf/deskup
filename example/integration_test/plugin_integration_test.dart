import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:deskup/deskup.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  testWidgets('native registration reports host version without a feed', (
    tester,
  ) async {
    final updater = Deskup();
    addTearDown(updater.dispose);
    await updater.initialize();
    final status = await updater.status();
    expect(status.version, isNotEmpty);
    expect(status.configured, isFalse);
  });
}
