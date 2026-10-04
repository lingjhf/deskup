import 'dart:async';

import 'package:deskup/deskup.dart';
import 'package:flutter/material.dart';

void main() => runApp(const MaterialApp(home: UpdateExample()));

class UpdateExample extends StatefulWidget {
  const UpdateExample({super.key});
  @override
  State<UpdateExample> createState() => _UpdateExampleState();
}

class _UpdateExampleState extends State<UpdateExample> {
  final updater = Deskup();
  StreamSubscription<UpdateStatus>? subscription;
  UpdateStatus status = const UpdateStatus();
  String error = '';

  @override
  void initState() {
    super.initState();
    subscription = updater.events.listen((value) {
      if (mounted) setState(() => status = value);
    });
    unawaited(
      run(() async {
        await updater.initialize();
      }),
    );
  }

  Future<void> run(Future<void> Function() action) async {
    try {
      await action();
      final next = await updater.status();
      if (mounted) {
        setState(() {
          status = next;
          error = '';
        });
      }
    } catch (e) {
      if (mounted) setState(() => error = '$e');
    }
  }

  @override
  void dispose() {
    unawaited(subscription?.cancel());
    updater.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Deskup')),
    body: Column(
      children: [
        Text(
          status.configured
              ? '${status.phase.name}: ${status.availableVersion}'
              : 'Configure a signed update feed to enable updates.',
        ),
        Text(error.isEmpty ? status.error : error),
        ElevatedButton(
          onPressed: status.canCheck ? () => run(updater.check) : null,
          child: const Text('Check'),
        ),
        if (status.supportsDownload)
          ElevatedButton(
            onPressed: status.phase == UpdatePhase.available
                ? () => run(updater.download)
                : null,
            child: const Text('Download'),
          ),
        if (status.supportsInstall)
          ElevatedButton(
            onPressed: status.phase == UpdatePhase.ready
                ? () => run(updater.install)
                : null,
            child: const Text('Install and restart'),
          ),
      ],
    ),
  );
}
