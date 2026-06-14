import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

void main() {
  runApp(const MotemaSensApp());
}

class MotemaSensApp extends StatelessWidget {
  const MotemaSensApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'MotemaSens',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF0F766E),
          brightness: Brightness.light,
        ),
        useMaterial3: true,
      ),
      home: const DeviceControllerScreen(),
    );
  }
}

enum ConnectionStateKind { offline, connecting, connected, demo }

enum RecordingMode { idle, ecg, microphone }

class DeviceSnapshot {
  const DeviceSnapshot({
    required this.greenLed,
    required this.blueLed,
    required this.sw1Pressed,
    required this.sw2Pressed,
    required this.batteryVolts,
    required this.signalQuality,
    required this.sampleRate,
    required this.sdReady,
    required this.recordingMode,
    required this.lastSeen,
    required this.version,
    required this.ip,
    required this.ledMode,
    required this.wifiLogging,
    required this.usbLogging,
    required this.selfTestActive,
  });

  factory DeviceSnapshot.demo({
    bool greenLed = false,
    bool blueLed = false,
    RecordingMode recordingMode = RecordingMode.idle,
  }) {
    return DeviceSnapshot(
      greenLed: greenLed,
      blueLed: blueLed,
      sw1Pressed: false,
      sw2Pressed: true,
      batteryVolts: 4.08,
      signalQuality: 92,
      sampleRate: 500,
      sdReady: false,
      recordingMode: recordingMode,
      lastSeen: DateTime.now(),
      version: 'demo',
      ip: 'not connected',
      ledMode: 'heartbeat',
      wifiLogging: false,
      usbLogging: false,
      selfTestActive: false,
    );
  }

  factory DeviceSnapshot.fromJson(Map<String, dynamic> json) {
    RecordingMode mode;
    final recordingMode = json['recordingMode'] ??
        ((json['wifi_logging'] == true) ? 'ecg' : 'idle');
    switch (recordingMode) {
      case 'ecg':
        mode = RecordingMode.ecg;
      case 'microphone':
        mode = RecordingMode.microphone;
      default:
        mode = RecordingMode.idle;
    }

    final signalQuality = (json['signalQuality'] as num?)?.toInt() ??
        ((json['ecg_ready'] == true) ? 100 : 35);
    final wifiRate = (json['wifi_rate_hz'] as num?)?.round() ?? 0;

    return DeviceSnapshot(
      greenLed: json['greenLed'] == true,
      blueLed: json['blueLed'] == true,
      sw1Pressed: json['sw1Pressed'] == true,
      sw2Pressed: json['sw2Pressed'] == true,
      batteryVolts: (json['batteryVolts'] as num?)?.toDouble() ?? 0,
      signalQuality: signalQuality.clamp(0, 100),
      sampleRate: (json['sampleRate'] as num?)?.toInt() ??
          (wifiRate > 0 ? wifiRate : 100),
      sdReady: json['sdReady'] == true,
      recordingMode: mode,
      lastSeen: DateTime.now(),
      version: (json['version'] as String?) ?? 'unknown',
      ip: (json['ip'] as String?) ?? '',
      ledMode: (json['ledMode'] as String?) ?? 'heartbeat',
      wifiLogging: json['wifi_logging'] == true,
      usbLogging: json['usb_logging'] == true,
      selfTestActive: json['selfTestActive'] == true,
    );
  }

  final bool greenLed;
  final bool blueLed;
  final bool sw1Pressed;
  final bool sw2Pressed;
  final double batteryVolts;
  final int signalQuality;
  final int sampleRate;
  final bool sdReady;
  final RecordingMode recordingMode;
  final DateTime lastSeen;
  final String version;
  final String ip;
  final String ledMode;
  final bool wifiLogging;
  final bool usbLogging;
  final bool selfTestActive;

  DeviceSnapshot copyWith({
    bool? greenLed,
    bool? blueLed,
    bool? sw1Pressed,
    bool? sw2Pressed,
    double? batteryVolts,
    int? signalQuality,
    int? sampleRate,
    bool? sdReady,
    RecordingMode? recordingMode,
    DateTime? lastSeen,
    String? version,
    String? ip,
    String? ledMode,
    bool? wifiLogging,
    bool? usbLogging,
    bool? selfTestActive,
  }) {
    return DeviceSnapshot(
      greenLed: greenLed ?? this.greenLed,
      blueLed: blueLed ?? this.blueLed,
      sw1Pressed: sw1Pressed ?? this.sw1Pressed,
      sw2Pressed: sw2Pressed ?? this.sw2Pressed,
      batteryVolts: batteryVolts ?? this.batteryVolts,
      signalQuality: signalQuality ?? this.signalQuality,
      sampleRate: sampleRate ?? this.sampleRate,
      sdReady: sdReady ?? this.sdReady,
      recordingMode: recordingMode ?? this.recordingMode,
      lastSeen: lastSeen ?? this.lastSeen,
      version: version ?? this.version,
      ip: ip ?? this.ip,
      ledMode: ledMode ?? this.ledMode,
      wifiLogging: wifiLogging ?? this.wifiLogging,
      usbLogging: usbLogging ?? this.usbLogging,
      selfTestActive: selfTestActive ?? this.selfTestActive,
    );
  }
}

class DeviceApi {
  DeviceApi({
    required this.baseUrl,
    http.Client? client,
  }) : _client = client ?? http.Client();

  final String baseUrl;
  final http.Client _client;

  Uri _uri(String path) => Uri.parse('$baseUrl$path');

  Future<DeviceSnapshot> fetchStatus() async {
    final response = await _client
        .get(_uri('/api/status'))
        .timeout(const Duration(seconds: 3));
    if (response.statusCode != 200) {
      throw Exception('Status failed: HTTP ${response.statusCode}');
    }
    return DeviceSnapshot.fromJson(
        jsonDecode(response.body) as Map<String, dynamic>);
  }

  Future<void> setLed({required String led, required bool enabled}) async {
    await _post('/api/led', {'led': led, 'enabled': enabled});
  }

  Future<void> setLedHeartbeat() async {
    await _post('/api/led-heartbeat', {});
  }

  Future<void> setRecording(RecordingMode mode) async {
    final value = switch (mode) {
      RecordingMode.ecg => 'ecg',
      RecordingMode.microphone => 'microphone',
      RecordingMode.idle => 'idle',
    };
    await _post('/api/recording', {'mode': value});
  }

  Future<void> runSelfTest() async {
    await _post('/api/self-test', {});
  }

  Future<void> _post(String path, Map<String, Object?> body) async {
    final response = await _client
        .post(
          _uri(path),
          headers: const {'content-type': 'application/json'},
          body: jsonEncode(body),
        )
        .timeout(const Duration(seconds: 3));
    if (response.statusCode < 200 || response.statusCode >= 300) {
      throw Exception('$path failed: HTTP ${response.statusCode}');
    }
  }
}

class DeviceControllerScreen extends StatefulWidget {
  const DeviceControllerScreen({super.key});

  @override
  State<DeviceControllerScreen> createState() => _DeviceControllerScreenState();
}

class _DeviceControllerScreenState extends State<DeviceControllerScreen> {
  final TextEditingController _baseUrlController =
      TextEditingController(text: 'http://192.168.5.29');
  final List<String> _events = <String>[
    'Ready. Connect to your ESP32 or use demo mode.'
  ];
  final List<double> _ecgSamples = <double>[
    0.12,
    0.16,
    0.09,
    0.42,
    -0.18,
    0.04,
    0.11,
    0.08,
    0.35,
    -0.12
  ];

  DeviceSnapshot _snapshot = DeviceSnapshot.demo();
  ConnectionStateKind _connection = ConnectionStateKind.demo;
  int _tabIndex = 0;
  Timer? _pollTimer;
  DeviceApi? _api;

  @override
  void dispose() {
    _pollTimer?.cancel();
    _baseUrlController.dispose();
    super.dispose();
  }

  void _log(String message) {
    setState(() {
      _events.insert(0, '${TimeOfDay.now().format(context)}  $message');
      if (_events.length > 40) {
        _events.removeLast();
      }
    });
  }

  Future<void> _connect() async {
    final baseUrl =
        _baseUrlController.text.trim().replaceAll(RegExp(r'/$'), '');
    setState(() {
      _connection = ConnectionStateKind.connecting;
      _api = DeviceApi(baseUrl: baseUrl);
    });
    try {
      final snapshot = await _api!.fetchStatus();
      setState(() {
        _snapshot = snapshot;
        _connection = ConnectionStateKind.connected;
      });
      _log('Connected to $baseUrl');
      _startPolling();
    } catch (error) {
      setState(() {
        _connection = ConnectionStateKind.demo;
        _snapshot = DeviceSnapshot.demo(
          greenLed: _snapshot.greenLed,
          blueLed: _snapshot.blueLed,
          recordingMode: _snapshot.recordingMode,
        );
      });
      _log('ESP32 not reachable. Demo mode active.');
    }
  }

  void _disconnect() {
    _pollTimer?.cancel();
    setState(() {
      _connection = ConnectionStateKind.offline;
    });
    _log('Disconnected.');
  }

  void _startPolling() {
    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(
        const Duration(seconds: 4), (_) => _refreshStatus(silent: true));
  }

  Future<void> _refreshStatus({bool silent = false}) async {
    if (_connection != ConnectionStateKind.connected || _api == null) {
      if (!silent) {
        _log('Demo status refreshed.');
      }
      setState(() {
        _snapshot = _snapshot.copyWith(lastSeen: DateTime.now());
      });
      return;
    }

    try {
      final snapshot = await _api!.fetchStatus();
      setState(() {
        _snapshot = snapshot;
      });
      if (!silent) {
        _log('Status refreshed.');
      }
    } catch (error) {
      setState(() {
        _connection = ConnectionStateKind.demo;
      });
      _log('Connection lost. Demo mode active.');
    }
  }

  Future<void> _setLed(String led, bool enabled) async {
    final nextSnapshot = led == 'green'
        ? _snapshot.copyWith(greenLed: enabled, lastSeen: DateTime.now())
        : _snapshot.copyWith(blueLed: enabled, lastSeen: DateTime.now());
    setState(() {
      _snapshot = nextSnapshot;
    });

    if (_connection == ConnectionStateKind.connected && _api != null) {
      try {
        await _api!.setLed(led: led, enabled: enabled);
        _log('${_labelForLed(led)} LED ${enabled ? 'on' : 'off'}.');
      } catch (error) {
        _log('LED command failed. Check firmware endpoint.');
      }
    } else {
      _log('${_labelForLed(led)} LED ${enabled ? 'on' : 'off'} in demo mode.');
    }
  }

  Future<void> _setLedHeartbeat() async {
    setState(() {
      _snapshot = _snapshot.copyWith(
        ledMode: 'heartbeat',
        greenLed: true,
        blueLed: true,
        lastSeen: DateTime.now(),
      );
    });

    if (_connection == ConnectionStateKind.connected && _api != null) {
      try {
        await _api!.setLedHeartbeat();
        _log('LEDs returned to heartbeat mode.');
        await _refreshStatus(silent: true);
      } catch (error) {
        _log('LED heartbeat command failed.');
      }
    } else {
      _log('LED heartbeat mode selected in demo mode.');
    }
  }

  Future<void> _setRecording(RecordingMode mode) async {
    setState(() {
      _snapshot =
          _snapshot.copyWith(recordingMode: mode, lastSeen: DateTime.now());
      _ecgSamples
        ..removeAt(0)
        ..add(mode == RecordingMode.ecg ? 0.28 : 0.05);
    });

    if (_connection == ConnectionStateKind.connected && _api != null) {
      try {
        await _api!.setRecording(mode);
        _log('${_modeLabel(mode)} command sent.');
      } catch (error) {
        _log('Recording command failed. Check firmware endpoint.');
      }
    } else {
      _log('${_modeLabel(mode)} selected in demo mode.');
    }
  }

  Future<void> _runSelfTest() async {
    if (_connection == ConnectionStateKind.connected && _api != null) {
      try {
        await _api!.runSelfTest();
        _log('Self-test command sent.');
      } catch (error) {
        _log('Self-test failed. Check firmware endpoint.');
      }
    } else {
      _log('Demo self-test complete: LEDs, switches, ECG, mic checked.');
    }
  }

  String _labelForLed(String led) => led == 'green' ? 'Green' : 'Blue';

  String _modeLabel(RecordingMode mode) {
    return switch (mode) {
      RecordingMode.idle => 'Idle',
      RecordingMode.ecg => 'ECG recording',
      RecordingMode.microphone => 'Microphone recording',
    };
  }

  @override
  Widget build(BuildContext context) {
    final pages = [
      _DashboardView(
        snapshot: _snapshot,
        connection: _connection,
        baseUrlController: _baseUrlController,
        ecgSamples: _ecgSamples,
        onConnect: _connect,
        onDisconnect: _disconnect,
        onRefresh: _refreshStatus,
        onSetLed: _setLed,
        onSetLedHeartbeat: _setLedHeartbeat,
        onSetRecording: _setRecording,
        onSelfTest: _runSelfTest,
      ),
      _TelemetryView(snapshot: _snapshot, ecgSamples: _ecgSamples),
      _HardwareView(),
      _LogView(events: _events),
    ];

    return Scaffold(
      appBar: AppBar(
        title: const Text('MotemaSens Control'),
        actions: [
          IconButton(
            tooltip: 'Refresh',
            onPressed: () => _refreshStatus(),
            icon: const Icon(Icons.refresh),
          ),
        ],
      ),
      body: pages[_tabIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _tabIndex,
        onDestinationSelected: (index) => setState(() => _tabIndex = index),
        destinations: const [
          NavigationDestination(
              icon: Icon(Icons.dashboard_outlined),
              selectedIcon: Icon(Icons.dashboard),
              label: 'Control'),
          NavigationDestination(
              icon: Icon(Icons.monitor_heart_outlined),
              selectedIcon: Icon(Icons.monitor_heart),
              label: 'Signals'),
          NavigationDestination(
              icon: Icon(Icons.memory_outlined),
              selectedIcon: Icon(Icons.memory),
              label: 'Hardware'),
          NavigationDestination(
              icon: Icon(Icons.receipt_long_outlined),
              selectedIcon: Icon(Icons.receipt_long),
              label: 'Log'),
        ],
      ),
    );
  }
}

class _DashboardView extends StatelessWidget {
  const _DashboardView({
    required this.snapshot,
    required this.connection,
    required this.baseUrlController,
    required this.ecgSamples,
    required this.onConnect,
    required this.onDisconnect,
    required this.onRefresh,
    required this.onSetLed,
    required this.onSetLedHeartbeat,
    required this.onSetRecording,
    required this.onSelfTest,
  });

  final DeviceSnapshot snapshot;
  final ConnectionStateKind connection;
  final TextEditingController baseUrlController;
  final List<double> ecgSamples;
  final VoidCallback onConnect;
  final VoidCallback onDisconnect;
  final Future<void> Function({bool silent}) onRefresh;
  final Future<void> Function(String led, bool enabled) onSetLed;
  final Future<void> Function() onSetLedHeartbeat;
  final Future<void> Function(RecordingMode mode) onSetRecording;
  final VoidCallback onSelfTest;

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _ConnectionPanel(
          connection: connection,
          baseUrlController: baseUrlController,
          onConnect: onConnect,
          onDisconnect: onDisconnect,
        ),
        const SizedBox(height: 12),
        _HealthPanel(snapshot: snapshot, connection: connection),
        const SizedBox(height: 12),
        _LedPanel(
          snapshot: snapshot,
          onSetLed: onSetLed,
          onSetLedHeartbeat: onSetLedHeartbeat,
        ),
        const SizedBox(height: 12),
        _RecordingPanel(snapshot: snapshot, onSetRecording: onSetRecording),
        const SizedBox(height: 12),
        _SwitchPanel(snapshot: snapshot),
        const SizedBox(height: 12),
        _QuickActionsPanel(onRefresh: onRefresh, onSelfTest: onSelfTest),
        const SizedBox(height: 12),
        _SafetyPanel(),
      ],
    );
  }
}

class _ConnectionPanel extends StatelessWidget {
  const _ConnectionPanel({
    required this.connection,
    required this.baseUrlController,
    required this.onConnect,
    required this.onDisconnect,
  });

  final ConnectionStateKind connection;
  final TextEditingController baseUrlController;
  final VoidCallback onConnect;
  final VoidCallback onDisconnect;

  @override
  Widget build(BuildContext context) {
    final connected = connection == ConnectionStateKind.connected;
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _SectionTitle(
            icon: Icons.wifi_tethering,
            title: 'Device connection',
            trailing: _StatusPill(
                label: _connectionLabel(connection),
                tone: connected ? _Tone.good : _Tone.warn),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: baseUrlController,
            keyboardType: TextInputType.url,
            decoration: const InputDecoration(
              labelText: 'ESP32 base URL',
              prefixIcon: Icon(Icons.link),
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: FilledButton.icon(
                  onPressed: onConnect,
                  icon: const Icon(Icons.cable),
                  label: Text(connected ? 'Reconnect' : 'Connect'),
                ),
              ),
              const SizedBox(width: 8),
              IconButton.outlined(
                tooltip: 'Disconnect',
                onPressed: onDisconnect,
                icon: const Icon(Icons.link_off),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _HealthPanel extends StatelessWidget {
  const _HealthPanel({required this.snapshot, required this.connection});

  final DeviceSnapshot snapshot;
  final ConnectionStateKind connection;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const _SectionTitle(
              icon: Icons.favorite_border, title: 'Device health'),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                  child: _MetricTile(
                      label: 'Battery',
                      value: '${snapshot.batteryVolts.toStringAsFixed(2)} V',
                      icon: Icons.battery_5_bar)),
              const SizedBox(width: 8),
              Expanded(
                  child: _MetricTile(
                      label: 'Signal',
                      value: '${snapshot.signalQuality}%',
                      icon: Icons.show_chart)),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                  child: _MetricTile(
                      label: 'Sample rate',
                      value: '${snapshot.sampleRate} Hz',
                      icon: Icons.speed)),
              const SizedBox(width: 8),
              Expanded(
                  child: _MetricTile(
                      label: 'microSD',
                      value: snapshot.sdReady ? 'Ready' : 'Blocked',
                      icon: Icons.sd_card)),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                  child: _MetricTile(
                      label: 'Firmware',
                      value: snapshot.version,
                      icon: Icons.new_releases_outlined)),
              const SizedBox(width: 8),
              Expanded(
                  child: _MetricTile(
                      label: 'WiFi log',
                      value: snapshot.wifiLogging ? 'Running' : 'Stopped',
                      icon: Icons.wifi_tethering)),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            'IP ${snapshot.ip.isEmpty ? 'not connected' : snapshot.ip}  USB ${snapshot.usbLogging ? 'logging' : 'idle'}',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }
}

class _LedPanel extends StatelessWidget {
  const _LedPanel({
    required this.snapshot,
    required this.onSetLed,
    required this.onSetLedHeartbeat,
  });

  final DeviceSnapshot snapshot;
  final Future<void> Function(String led, bool enabled) onSetLed;
  final Future<void> Function() onSetLedHeartbeat;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const _SectionTitle(
              icon: Icons.lightbulb_outline, title: 'LED controls'),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Green LED'),
            subtitle: const Text('GPIO14, active high'),
            secondary: const Icon(Icons.circle, color: Color(0xFF16A34A)),
            value: snapshot.greenLed,
            onChanged: (value) => onSetLed('green', value),
          ),
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Blue LED'),
            subtitle: const Text('GPIO15, active high'),
            secondary: const Icon(Icons.circle, color: Color(0xFF2563EB)),
            value: snapshot.blueLed,
            onChanged: (value) => onSetLed('blue', value),
          ),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            onPressed: onSetLedHeartbeat,
            icon: const Icon(Icons.favorite_outline),
            label: Text(snapshot.ledMode == 'heartbeat'
                ? 'Heartbeat mode active'
                : 'Return to heartbeat mode'),
          ),
        ],
      ),
    );
  }
}

class _RecordingPanel extends StatelessWidget {
  const _RecordingPanel({required this.snapshot, required this.onSetRecording});

  final DeviceSnapshot snapshot;
  final Future<void> Function(RecordingMode mode) onSetRecording;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const _SectionTitle(
              icon: Icons.radio_button_checked, title: 'Acquisition'),
          const SizedBox(height: 12),
          SegmentedButton<RecordingMode>(
            segments: const [
              ButtonSegment(
                  value: RecordingMode.idle,
                  icon: Icon(Icons.stop_circle_outlined),
                  label: Text('Idle')),
              ButtonSegment(
                  value: RecordingMode.ecg,
                  icon: Icon(Icons.monitor_heart_outlined),
                  label: Text('ECG')),
              ButtonSegment(
                  value: RecordingMode.microphone,
                  icon: Icon(Icons.mic_none),
                  label: Text('Mic')),
            ],
            selected: {snapshot.recordingMode},
            onSelectionChanged: (selection) => onSetRecording(selection.first),
          ),
          const SizedBox(height: 12),
          Text(
            'ADS1294 ECG and I2S microphone controls are prepared for firmware endpoints.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }
}

class _SwitchPanel extends StatelessWidget {
  const _SwitchPanel({required this.snapshot});

  final DeviceSnapshot snapshot;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const _SectionTitle(
              icon: Icons.toggle_on_outlined, title: 'User inputs'),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                  child: _InputTile(
                      label: 'SW1',
                      gpio: 'GPIO42',
                      pressed: snapshot.sw1Pressed)),
              const SizedBox(width: 8),
              Expanded(
                  child: _InputTile(
                      label: 'SW2',
                      gpio: 'GPIO41',
                      pressed: snapshot.sw2Pressed)),
            ],
          ),
        ],
      ),
    );
  }
}

class _QuickActionsPanel extends StatelessWidget {
  const _QuickActionsPanel({required this.onRefresh, required this.onSelfTest});

  final Future<void> Function({bool silent}) onRefresh;
  final VoidCallback onSelfTest;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Row(
        children: [
          Expanded(
            child: OutlinedButton.icon(
              onPressed: () => onRefresh(),
              icon: const Icon(Icons.sync),
              label: const Text('Refresh'),
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: OutlinedButton.icon(
              onPressed: onSelfTest,
              icon: const Icon(Icons.fact_check_outlined),
              label: const Text('Self-test'),
            ),
          ),
        ],
      ),
    );
  }
}

class _SafetyPanel extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      color: const Color(0xFFFFFBEB),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Icon(Icons.health_and_safety_outlined,
              color: Color(0xFFB45309)),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              'ECG inputs are human-contact hardware. Use test signals during bring-up until isolation, protection, leakage, and regulatory requirements are confirmed.',
              style: Theme.of(context)
                  .textTheme
                  .bodySmall
                  ?.copyWith(color: const Color(0xFF78350F)),
            ),
          ),
        ],
      ),
    );
  }
}

class _TelemetryView extends StatelessWidget {
  const _TelemetryView({required this.snapshot, required this.ecgSamples});

  final DeviceSnapshot snapshot;
  final List<double> ecgSamples;

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _SectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const _SectionTitle(
                  icon: Icons.monitor_heart_outlined, title: 'ECG preview'),
              const SizedBox(height: 16),
              SizedBox(
                height: 160,
                child: CustomPaint(
                  painter: _SignalPainter(ecgSamples,
                      color: Theme.of(context).colorScheme.primary),
                  child: const SizedBox.expand(),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _SectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const _SectionTitle(icon: Icons.mic_none, title: 'Microphone'),
              const SizedBox(height: 12),
              LinearProgressIndicator(
                  value: snapshot.recordingMode == RecordingMode.microphone
                      ? 0.64
                      : 0.08),
              const SizedBox(height: 8),
              Text(snapshot.recordingMode == RecordingMode.microphone
                  ? 'Audio stream active'
                  : 'Audio stream idle'),
            ],
          ),
        ),
      ],
    );
  }
}

class _HardwareView extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: const [
        _PinGroup(
          title: 'ADS1294 ECG',
          pins: [
            'MOSI GPIO36',
            'MISO GPIO17',
            'SCLK GPIO18',
            'CS GPIO21',
            'DRDY GPIO16',
            'PWDN GPIO35',
            'RESET GPIO34',
            'START GPIO33',
          ],
        ),
        SizedBox(height: 12),
        _PinGroup(
          title: 'User IO',
          pins: [
            'Green LED GPIO14',
            'Blue LED GPIO15',
            'SW1 GPIO42',
            'SW2 GPIO41'
          ],
        ),
        SizedBox(height: 12),
        _PinGroup(
          title: 'I2S microphone',
          pins: ['DATA GPIO3', 'BCLK GPIO4', 'WS GPIO5'],
        ),
        SizedBox(height: 12),
        _PinGroup(
          title: 'microSD note',
          pins: [
            'CS GPIO39',
            'MOSI GPIO38',
            'SCLK GPIO37',
            'MISO requires PCB fix before use'
          ],
        ),
      ],
    );
  }
}

class _LogView extends StatelessWidget {
  const _LogView({required this.events});

  final List<String> events;

  @override
  Widget build(BuildContext context) {
    return ListView.separated(
      padding: const EdgeInsets.all(16),
      itemCount: events.length,
      separatorBuilder: (_, __) => const Divider(height: 1),
      itemBuilder: (context, index) {
        return ListTile(
          dense: true,
          leading: const Icon(Icons.terminal, size: 20),
          title: Text(events[index]),
        );
      },
    );
  }
}

class _PinGroup extends StatelessWidget {
  const _PinGroup({required this.title, required this.pins});

  final String title;
  final List<String> pins;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _SectionTitle(icon: Icons.account_tree_outlined, title: title),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: pins.map((pin) => Chip(label: Text(pin))).toList(),
          ),
        ],
      ),
    );
  }
}

class _SectionCard extends StatelessWidget {
  const _SectionCard({required this.child, this.color});

  final Widget child;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 0,
      color: color ?? Theme.of(context).colorScheme.surfaceContainerLow,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: child,
      ),
    );
  }
}

class _SectionTitle extends StatelessWidget {
  const _SectionTitle({required this.icon, required this.title, this.trailing});

  final IconData icon;
  final String title;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(icon, size: 22),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            title,
            style: Theme.of(context)
                .textTheme
                .titleMedium
                ?.copyWith(fontWeight: FontWeight.w700),
          ),
        ),
        if (trailing != null) trailing!,
      ],
    );
  }
}

enum _Tone { good, warn }

class _StatusPill extends StatelessWidget {
  const _StatusPill({required this.label, required this.tone});

  final String label;
  final _Tone tone;

  @override
  Widget build(BuildContext context) {
    final good = tone == _Tone.good;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: good ? const Color(0xFFDCFCE7) : const Color(0xFFFEF3C7),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: TextStyle(
          color: good ? const Color(0xFF166534) : const Color(0xFF92400E),
          fontWeight: FontWeight.w700,
          fontSize: 12,
        ),
      ),
    );
  }
}

class _MetricTile extends StatelessWidget {
  const _MetricTile(
      {required this.label, required this.value, required this.icon});

  final String label;
  final String value;
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(minHeight: 82),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surface,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, size: 20),
          const SizedBox(height: 12),
          Text(value,
              style: Theme.of(context)
                  .textTheme
                  .titleMedium
                  ?.copyWith(fontWeight: FontWeight.w800)),
          Text(label, style: Theme.of(context).textTheme.bodySmall),
        ],
      ),
    );
  }
}

class _InputTile extends StatelessWidget {
  const _InputTile(
      {required this.label, required this.gpio, required this.pressed});

  final String label;
  final String gpio;
  final bool pressed;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(minHeight: 82),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surface,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(pressed
              ? Icons.radio_button_checked
              : Icons.radio_button_unchecked),
          const SizedBox(height: 12),
          Text(label,
              style: Theme.of(context)
                  .textTheme
                  .titleMedium
                  ?.copyWith(fontWeight: FontWeight.w800)),
          Text('$gpio, ${pressed ? 'pressed' : 'released'}',
              style: Theme.of(context).textTheme.bodySmall),
        ],
      ),
    );
  }
}

class _SignalPainter extends CustomPainter {
  _SignalPainter(this.samples, {required this.color});

  final List<double> samples;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final gridPaint = Paint()
      ..color = const Color(0xFFE5E7EB)
      ..strokeWidth = 1;
    for (var i = 0; i < 5; i++) {
      final y = size.height * i / 4;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }

    final signalPaint = Paint()
      ..color = color
      ..strokeWidth = 3
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final path = Path();
    for (var i = 0; i < samples.length; i++) {
      final x = size.width * i / (samples.length - 1);
      final y = size.height / 2 - samples[i] * size.height * 0.72;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    canvas.drawPath(path, signalPaint);
  }

  @override
  bool shouldRepaint(covariant _SignalPainter oldDelegate) {
    return oldDelegate.samples != samples || oldDelegate.color != color;
  }
}

String _connectionLabel(ConnectionStateKind connection) {
  return switch (connection) {
    ConnectionStateKind.offline => 'Offline',
    ConnectionStateKind.connecting => 'Connecting',
    ConnectionStateKind.connected => 'Connected',
    ConnectionStateKind.demo => 'Demo',
  };
}
