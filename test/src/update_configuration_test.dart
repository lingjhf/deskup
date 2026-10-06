import 'package:deskup/deskup.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('Update configuration', () {
    test('public repository policy includes all requested overrides', () {
      final config = WindowsUpdateConfiguration(
        source: GiteaUpdateSource(
          repositoryUrl: Uri.parse('https://git.example.com/team/app'),
          includePrereleases: true,
        ),
        expectedPackageId: 'App.Dev',
        channel: 'beta',
        allowDowngrade: true,
        maximumDeltas: -1,
        automaticCheckInterval: const Duration(minutes: 5),
        preferenceNamespace: 'App',
      ).toMap();
      expect(config['channel'], 'beta');
      expect(config['maximumDeltas'], -1);
      expect(config['automaticCheckIntervalSeconds'], 300);
      expect((config['source'] as Map)['includePrereleases'], true);
    });
    test('rejects remote HTTP, credentials and non-repository Gitea URLs', () {
      for (final url in [
        'http://git.example.com/team/app',
        'https://user:password@git.example.com/team/app',
        'https://git.example.com/gitea/team/app',
        'https://git.example.com/team/app.git',
        'https://git.example.com/team/app?token=a',
        'https://git.example.com/team/app#release',
        'https://git.example.com:999999/team/app',
      ]) {
        expect(
          () => GiteaUpdateSource(
            repositoryUrl: Uri.parse(url),
            allowLocalHttp: true,
          ).toMap(),
          throwsArgumentError,
        );
      }
    });
    test('loopback HTTP requires both explicit opt-in and port', () {
      final url = Uri.parse('http://127.0.0.1:8765/team/app');
      expect(
        () => GiteaUpdateSource(repositoryUrl: url).toMap(),
        throwsArgumentError,
      );
      expect(
        GiteaUpdateSource(
          repositoryUrl: url,
          allowLocalHttp: true,
        ).toMap()['type'],
        'gitea',
      );
      expect(
        () => WebUpdateSource(
          url: Uri.parse('http://localhost'),
          allowLocalHttp: true,
        ).toMap(),
        throwsArgumentError,
      );
    });
    test('rejects invalid timing and identity policies', () {
      final source = WebUpdateSource(
        url: Uri.parse('https://updates.example.com'),
      );
      for (final policy in [
        WindowsUpdateConfiguration(
          source: source,
          automaticCheckInterval: Duration.zero,
        ),
        WindowsUpdateConfiguration(source: source, maximumDeltas: -2),
        WindowsUpdateConfiguration(
          source: source,
          preferenceNamespace: r'App\Other',
        ),
        WindowsUpdateConfiguration(source: source, expectedPackageId: ''),
        WindowsUpdateConfiguration(source: source, channel: '../stable'),
      ]) {
        expect(policy.toMap, throwsArgumentError);
      }
      expect(
        () => WebUpdateSource(url: source.url, timeout: Duration.zero).toMap(),
        throwsArgumentError,
      );
      expect(
        () => const MacOSUpdateConfiguration(
          automaticCheckInterval: Duration(minutes: 5),
        ).toMap(),
        throwsArgumentError,
      );
    });
    test('release metadata round trips without dropping size or notes', () {
      const status = UpdateStatus(
        packageSize: 5000000000,
        releaseNotesMarkdown: '# Changes',
        releaseNotesHtml: '<h1>Changes</h1>',
      );
      final decoded = UpdateStatus.fromMap(status.toMap());
      expect(decoded.packageSize, status.packageSize);
      expect(decoded.releaseNotesMarkdown, status.releaseNotesMarkdown);
      expect(decoded.releaseNotesHtml, status.releaseNotesHtml);
    });
  });
}
