import 'dart:async';
import 'dart:convert';
import 'dart:math' as math;

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
    required this.micTrace,
    required this.ecgCh1,
    required this.ecgCh2,
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
      micTrace: 0,
      ecgCh1: 0,
      ecgCh2: 0,
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
      micTrace: (json['mic_trace'] as num?)?.toDouble() ?? 0,
      ecgCh1: (json['ecg_ch1'] as num?)?.toDouble() ?? 0,
      ecgCh2: (json['ecg_ch2'] as num?)?.toDouble() ?? 0,
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
  final double micTrace;
  final double ecgCh1;
  final double ecgCh2;

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
    double? micTrace,
    double? ecgCh1,
    double? ecgCh2,
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
      micTrace: micTrace ?? this.micTrace,
      ecgCh1: ecgCh1 ?? this.ecgCh1,
      ecgCh2: ecgCh2 ?? this.ecgCh2,
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
  final List<double> _micSamples = List<double>.filled(180, 0);

  DeviceSnapshot _snapshot = DeviceSnapshot.demo();
  ConnectionStateKind _connection = ConnectionStateKind.demo;
  int _tabIndex = 0;
  Timer? _pollTimer;
  Timer? _livePaintTimer;
  Timer? _livePollTimer;
  DeviceApi? _api;
  bool _liveStreaming = false;
  String _liveMessage = 'Live stream idle';
  int _liveSamples = 0;
  double _ecgBaseline = 0;
  double _ecgScale = 90000;
  double _micPeak = 0.05;

  @override
  void dispose() {
    _pollTimer?.cancel();
    _livePaintTimer?.cancel();
    _livePollTimer?.cancel();
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
    _stopLiveStream(sendStop: true);
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
        if (mode == RecordingMode.idle && _liveStreaming) {
          await _stopLiveStream(sendStop: false);
        }
      } catch (error) {
        _log('Recording command failed. Check firmware endpoint.');
      }
    } else {
      _log('${_modeLabel(mode)} selected in demo mode.');
    }
  }

  Future<void> _toggleLiveStream() async {
    if (_liveStreaming) {
      await _stopLiveStream(sendStop: true);
      return;
    }
    await _startLiveStream();
  }

  Future<void> _startLiveStream() async {
    if (_connection != ConnectionStateKind.connected || _api == null) {
      _log('Connect to ESP32 before starting live view.');
      setState(() => _liveMessage = 'Connect to ESP32 first');
      return;
    }

    await _stopLiveStream(sendStop: false);
    setState(() {
      _liveStreaming = true;
      _liveMessage = 'Starting live view...';
      _liveSamples = 0;
    });

    _livePaintTimer?.cancel();
    _livePaintTimer =
        Timer.periodic(const Duration(milliseconds: 33), (_) {
      if (mounted && _liveStreaming) {
        setState(() {});
      }
    });

    try {
      await _api!.setRecording(RecordingMode.ecg);
      final first = await _api!.fetchStatus();
      _appendLiveSnapshot(first);
      setState(() => _snapshot = first);
      _livePollTimer = Timer.periodic(
        const Duration(milliseconds: 80),
        (_) => _pollLiveSample(),
      );
      _log('Live waveform view started.');
      setState(() => _liveMessage = 'Live stream running');
    } catch (error) {
      _livePaintTimer?.cancel();
      if (mounted) {
        setState(() {
          _liveStreaming = false;
          _liveMessage = 'Live stream failed';
        });
      }
      _log('Live stream failed. Check ESP32 WiFi.');
    }
  }

  Future<void> _stopLiveStream({required bool sendStop}) async {
    _livePaintTimer?.cancel();
    _livePaintTimer = null;
    _livePollTimer?.cancel();
    _livePollTimer = null;
    final wasStreaming = _liveStreaming;
    if (mounted) {
      setState(() {
        _liveStreaming = false;
        _liveMessage = 'Live stream stopped';
      });
    }
    if (sendStop && wasStreaming && _api != null) {
      try {
        await _api!.setRecording(RecordingMode.idle);
      } catch (_) {
        // The stream may already have closed on the ESP32 side.
      }
    }
  }

  Future<void> _pollLiveSample() async {
    if (!_liveStreaming || _api == null) {
      return;
    }
    try {
      final snapshot = await _api!.fetchStatus();
      _appendLiveSnapshot(snapshot);
      if (mounted) {
        setState(() {
          _snapshot = snapshot;
          _liveMessage = 'Live stream running';
        });
      }
    } catch (_) {
      _livePollTimer?.cancel();
      _livePaintTimer?.cancel();
      if (mounted) {
        setState(() {
          _liveStreaming = false;
          _liveMessage = 'Live stream stopped';
        });
      }
      _log('Live view stopped. ESP32 status not reachable.');
    }
  }

  void _appendLiveSnapshot(DeviceSnapshot snapshot) {
    final mic = snapshot.micTrace.clamp(-1.0, 1.0);
    _micPeak = math.max(0.02, (_micPeak * 0.985) + (mic.abs() * 0.015));
    _appendBounded(
      _micSamples,
      (mic / math.max(_micPeak, 0.04)).clamp(-1.0, 1.0),
    );

    final ecgRaw = snapshot.ecgCh1 - snapshot.ecgCh2;
    _ecgBaseline += (ecgRaw - _ecgBaseline) * 0.006;
    final ecgFiltered = ecgRaw - _ecgBaseline;
    _ecgScale =
        math.max(30000, (_ecgScale * 0.992) + (ecgFiltered.abs() * 0.008));
    _appendBounded(
      _ecgSamples,
      (ecgFiltered / _ecgScale).clamp(-1.0, 1.0),
    );
    ++_liveSamples;
  }

  void _appendBounded(List<double> samples, double value) {
    if (samples.length >= 180) {
      samples.removeAt(0);
    }
    samples.add(value);
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
      _TelemetryView(
        snapshot: _snapshot,
        connection: _connection,
        ecgSamples: _ecgSamples,
        micSamples: _micSamples,
        liveStreaming: _liveStreaming,
        liveMessage: _liveMessage,
        liveSamples: _liveSamples,
        onToggleLive: _toggleLiveStream,
      ),
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
  const _TelemetryView({
    required this.snapshot,
    required this.connection,
    required this.ecgSamples,
    required this.micSamples,
    required this.liveStreaming,
    required this.liveMessage,
    required this.liveSamples,
    required this.onToggleLive,
  });

  final DeviceSnapshot snapshot;
  final ConnectionStateKind connection;
  final List<double> ecgSamples;
  final List<double> micSamples;
  final bool liveStreaming;
  final String liveMessage;
  final int liveSamples;
  final Future<void> Function() onToggleLive;

  @override
  Widget build(BuildContext context) {
    final connected = connection == ConnectionStateKind.connected;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _SectionCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _SectionTitle(
                icon: Icons.sensors,
                title: 'Live view',
                trailing: _StatusPill(
                  label: liveStreaming ? 'Live' : _connectionLabel(connection),
                  tone: liveStreaming ? _Tone.good : _Tone.warn,
                ),
              ),
              const SizedBox(height: 12),
              FilledButton.icon(
                onPressed: connected ? onToggleLive : null,
                icon: Icon(liveStreaming
                    ? Icons.stop_circle_outlined
                    : Icons.play_circle_outline),
                label: Text(liveStreaming ? 'Stop live view' : 'Start live view'),
              ),
              const SizedBox(height: 8),
              Text(
                '$liveMessage  samples $liveSamples  ${snapshot.ip.isEmpty ? '' : snapshot.ip}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _WaveformCard(
          title: 'ECG waveform',
          icon: Icons.monitor_heart_outlined,
          samples: ecgSamples,
          color: const Color(0xFF10B981),
          centerLabel: 'ECG',
          footer: liveStreaming
              ? 'Lead differential, auto scaled'
              : 'Press Start live view to stream ECG',
        ),
        const SizedBox(height: 12),
        _WaveformCard(
          title: 'Microphone waveform',
          icon: Icons.mic_none,
          samples: micSamples,
          color: const Color(0xFF2563EB),
          centerLabel: 'MIC',
          footer: liveStreaming
              ? 'I2S mic trace, auto gain'
              : 'Press Start live view to stream mic',
        ),
      ],
    );
  }
}

class _WaveformCard extends StatelessWidget {
  const _WaveformCard({
    required this.title,
    required this.icon,
    required this.samples,
    required this.color,
    required this.centerLabel,
    required this.footer,
  });

  final String title;
  final IconData icon;
  final List<double> samples;
  final Color color;
  final String centerLabel;
  final String footer;

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _SectionTitle(icon: icon, title: title),
          const SizedBox(height: 12),
          SizedBox(
            height: 180,
            child: CustomPaint(
              painter: _SignalPainter(
                samples,
                color: color,
                centerLabel: centerLabel,
              ),
              child: const SizedBox.expand(),
            ),
          ),
          const SizedBox(height: 8),
          Text(footer, style: Theme.of(context).textTheme.bodySmall),
        ],
      ),
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
  _SignalPainter(
    this.samples, {
    required this.color,
    required this.centerLabel,
  });

  final List<double> samples;
  final Color color;
  final String centerLabel;

  @override
  void paint(Canvas canvas, Size size) {
    final plotRect = RRect.fromRectAndRadius(
      Offset.zero & size,
      const Radius.circular(8),
    );
    final bgPaint = Paint()..color = const Color(0xFF071114);
    canvas.drawRRect(plotRect, bgPaint);

    final gridPaint = Paint()
      ..color = const Color(0x2238BDF8)
      ..strokeWidth = 1;
    for (var i = 1; i < 6; i++) {
      final y = size.height * i / 6;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }
    for (var i = 1; i < 8; i++) {
      final x = size.width * i / 8;
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gridPaint);
    }

    final centerPaint = Paint()
      ..color = const Color(0x55E5E7EB)
      ..strokeWidth = 1.2;
    canvas.drawLine(
      Offset(0, size.height / 2),
      Offset(size.width, size.height / 2),
      centerPaint,
    );

    final glowPaint = Paint()
      ..color = color.withValues(alpha: 0.22)
      ..strokeWidth = 8
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final linePaint = Paint()
      ..color = color
      ..strokeWidth = 2.4
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final path = Path();
    if (samples.length < 2) {
      return;
    }
    for (var i = 0; i < samples.length; i++) {
      final x = size.width * i / (samples.length - 1);
      final sample = samples[i].clamp(-1.0, 1.0);
      final y = size.height / 2 - sample * size.height * 0.42;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    canvas.drawPath(path, glowPaint);
    canvas.drawPath(path, linePaint);

    final labelPainter = TextPainter(
      text: TextSpan(
        text: centerLabel,
        style: const TextStyle(
          color: Color(0x99FFFFFF),
          fontSize: 11,
          fontWeight: FontWeight.w700,
        ),
      ),
      textDirection: TextDirection.ltr,
    )..layout();
    labelPainter.paint(canvas, Offset(10, size.height / 2 + 8));
  }

  @override
  bool shouldRepaint(covariant _SignalPainter oldDelegate) {
    return oldDelegate.samples != samples ||
        oldDelegate.color != color ||
        oldDelegate.centerLabel != centerLabel;
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
