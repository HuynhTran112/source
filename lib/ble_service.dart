import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';

import 'navigation_ble.dart';
import 'notification_handler.dart';
import 'screens/gps_tracker_screen.dart';

class WatchTelemetry {
  final int steps;
  final double distanceKm;
  final double speedKmh;
  final int batteryPercent;
  final double heartRate;
  final double spo2;
  final String sportMode;
  final bool gpsFix;
  final double latitude;
  final double longitude;
  final int sessionId;

  const WatchTelemetry({
    required this.steps,
    required this.distanceKm,
    required this.speedKmh,
    required this.batteryPercent,
    required this.heartRate,
    required this.spo2,
    required this.sportMode,
    required this.gpsFix,
    required this.latitude,
    required this.longitude,
    required this.sessionId,
  });

  factory WatchTelemetry.fromBle(String msg) {
    final parts = msg.split('|');
    return WatchTelemetry(
      steps: int.tryParse(parts.length > 1 ? parts[1] : '') ?? 0,
      distanceKm: double.tryParse(parts.length > 2 ? parts[2] : '') ?? 0.0,
      speedKmh: double.tryParse(parts.length > 3 ? parts[3] : '') ?? 0.0,
      batteryPercent: int.tryParse(parts.length > 4 ? parts[4] : '') ?? -1,
      heartRate: double.tryParse(parts.length > 5 ? parts[5] : '') ?? 0.0,
      spo2: double.tryParse(parts.length > 6 ? parts[6] : '') ?? 0.0,
      sportMode: parts.length > 7 ? parts[7] : 'none',
      gpsFix: parts.length > 8 && parts[8] == '1',
      latitude: double.tryParse(parts.length > 9 ? parts[9] : '') ?? 0.0,
      longitude: double.tryParse(parts.length > 10 ? parts[10] : '') ?? 0.0,
      sessionId: int.tryParse(parts.length > 11 ? parts[11] : '') ?? 0,
    );
  }
}

class WatchActivitySession {
  final int sessionId;
  final DateTime time;
  final int sportId;
  final int durationSec;
  final int steps;
  final double distanceKm;
  final double avgSpeedKmh;
  final String routeRaw;

  const WatchActivitySession({
    required this.sessionId,
    required this.time,
    required this.sportId,
    required this.durationSec,
    required this.steps,
    required this.distanceKm,
    required this.avgSpeedKmh,
    required this.routeRaw,
  });

  factory WatchActivitySession.fromBle(String msg) {
    final parts = msg.split('|');
    final possibleTimestampWithSession =
        int.tryParse(parts.length > 2 ? parts[2] : '') ?? 0;
    final hasSessionId = possibleTimestampWithSession > 1000000000;
    final sessionId = hasSessionId
        ? int.tryParse(parts.length > 1 ? parts[1] : '') ?? 0
        : 0;
    final offset = hasSessionId ? 1 : 0;
    final ts =
        int.tryParse(parts.length > 1 + offset ? parts[1 + offset] : '') ?? 0;
    return WatchActivitySession(
      sessionId: sessionId,
      time: ts > 0
          ? DateTime.fromMillisecondsSinceEpoch(ts * 1000)
          : DateTime.now(),
      sportId:
          int.tryParse(parts.length > 2 + offset ? parts[2 + offset] : '') ?? 0,
      durationSec:
          int.tryParse(parts.length > 3 + offset ? parts[3 + offset] : '') ?? 0,
      steps:
          int.tryParse(parts.length > 4 + offset ? parts[4 + offset] : '') ?? 0,
      distanceKm:
          double.tryParse(parts.length > 5 + offset ? parts[5 + offset] : '') ??
          0.0,
      avgSpeedKmh:
          double.tryParse(parts.length > 6 + offset ? parts[6 + offset] : '') ??
          0.0,
      routeRaw: parts.length > 7 + offset
          ? parts.sublist(7 + offset).join('|')
          : '',
    );
  }
}

class BleService {
  static final BleService _instance = BleService._internal();
  factory BleService() => _instance;
  BleService._internal();

  final _ble = FlutterReactiveBle();
  StreamSubscription<ConnectionStateUpdate>? _connection;

  final _serviceUuid = Uuid.parse("12345678-1234-1234-1234-123456789abc");
  final _charUuid = Uuid.parse("abcd1234-ab12-cd34-ef56-123456789abc");

  DeviceConnectionState connectionState = DeviceConnectionState.disconnected;
  String? connectedDeviceId;
  String? connectedDeviceName;

  final _statusController = StreamController<DeviceConnectionState>.broadcast();
  Stream<DeviceConnectionState> get statusStream => _statusController.stream;
  final _cmdController = StreamController<String>.broadcast();
  Stream<String> get cmdStream => _cmdController.stream;
  final _telemetryController = StreamController<WatchTelemetry>.broadcast();
  Stream<WatchTelemetry> get telemetryStream => _telemetryController.stream;
  WatchTelemetry? latestTelemetry;
  final _activityController =
      StreamController<WatchActivitySession>.broadcast();
  Stream<WatchActivitySession> get activityStream => _activityController.stream;
  StreamSubscription<List<int>>? _notifySub;
  final StringBuffer _trackBuffer = StringBuffer();
  int _expectedTrackOffset = 0;
  bool _trackMissingChunk = false;

  /// ATT mặc định chỉ ~20 byte/payload → chuỗi NAV dài bị cắt & parse lỗi.
  /// Đặt MTU lớn hơn sau khi kết nối (Android; iOS thường tự thoả thuận).
  int? _negotiatedMtu;

  Future<bool> requestPermissions() async {
    Map<Permission, PermissionStatus> statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.location,
    ].request();

    return statuses.values.every((status) => status.isGranted);
  }

  Stream<DiscoveredDevice> scanDevices() {
    return _ble
        .scanForDevices(
          withServices: [_serviceUuid],
          scanMode: ScanMode.lowLatency,
        )
        .timeout(
          const Duration(seconds: 12),
          onTimeout: (sink) => sink.close(),
        );
  }

  void connect(DiscoveredDevice device) {
    _negotiatedMtu = null;
    connectedDeviceId = device.id;
    connectedDeviceName = device.name;

    _connection?.cancel();
    _connection = _ble
        .connectToDevice(
          id: device.id,
          connectionTimeout: const Duration(seconds: 10),
        )
        .listen(
          (update) {
            connectionState = update.connectionState;
            if (update.connectionState == DeviceConnectionState.disconnected) {
              _negotiatedMtu = null;
              _notifySub?.cancel();
              _notifySub = null;
            }
            _statusController.add(update.connectionState);
            debugPrint('Connection state: ${update.connectionState}');
            if (update.connectionState == DeviceConnectionState.connected) {
              _ensureLargeMtu().then((_) async {
                await syncTime();
                _startCommandNotify();
              });
            }
          },
          onError: (error) {
            debugPrint('Connection error: $error');
            _negotiatedMtu = null;
            connectionState = DeviceConnectionState.disconnected;
            _statusController.add(DeviceConnectionState.disconnected);
          },
        );
  }

  Future<void> _ensureLargeMtu() async {
    if (_negotiatedMtu != null) return;
    final id = connectedDeviceId;
    if (id == null) return;
    try {
      final m = await _ble.requestMtu(deviceId: id, mtu: 247);
      _negotiatedMtu = m;
      debugPrint('BLE MTU negotiated: $m');
    } catch (e) {
      _negotiatedMtu = -1;
      debugPrint('BLE MTU request failed (payload dài có thể vẫn bị cắt): $e');
    }
  }

  void disconnect() {
    _connection?.cancel();
    _notifySub?.cancel();
    _notifySub = null;
    _negotiatedMtu = null;
    connectedDeviceId = null;
    connectedDeviceName = null;
    connectionState = DeviceConnectionState.disconnected;
    _statusController.add(DeviceConnectionState.disconnected);
  }

  QualifiedCharacteristic? _qualifiedChar() {
    if (connectedDeviceId == null) return null;
    return QualifiedCharacteristic(
      serviceId: _serviceUuid,
      characteristicId: _charUuid,
      deviceId: connectedDeviceId!,
    );
  }

  void _startCommandNotify() {
    final ch = _qualifiedChar();
    if (ch == null) return;
    _notifySub?.cancel();
    _notifySub = _ble
        .subscribeToCharacteristic(ch)
        .listen(
          (data) async {
            try {
              final msg = utf8.decode(data).trim();
              if (msg.isNotEmpty) {
                debugPrint('BLE cmd recv: $msg');
                _cmdController.add(msg);
                if (msg.startsWith('REPLY|')) {
                  debugPrint("BLE cmd recv REPLY payload: $msg");
                  final parts = msg.split('|');
                  if (parts.length >= 4) {
                    final appName = parts[1];
                    final sender = parts[2];
                    final content = parts.sublist(3).join('|');
                    NotificationHandler().replyToNotification(
                      appName,
                      sender,
                      content,
                    );
                  }
                } else if (msg == 'TRACK_BEGIN') {
                  _trackBuffer.clear();
                  _expectedTrackOffset = 0;
                  _trackMissingChunk = false;
                  GpsTrackerScreen.resetTrackData(preserveSession: true);
                } else if (msg.startsWith('TRACK_CHUNK|')) {
                  final parts = msg.split('|');
                  if (parts.length >= 3) {
                    final offset = int.tryParse(parts[1]) ?? -1;
                    final payload = parts.sublist(2).join('|');
                    if (offset == _expectedTrackOffset) {
                      _trackBuffer.write(payload);
                      _expectedTrackOffset += payload.length;
                    } else {
                      _trackMissingChunk = true;
                      debugPrint(
                        'TRACK chunk missing: expected $_expectedTrackOffset got $offset',
                      );
                    }
                  }
                } else if (msg == 'TRACK_END') {
                  final trackData = _trackBuffer.toString();
                  _trackBuffer.clear();
                  _expectedTrackOffset = 0;
                  if (_trackMissingChunk) {
                    _trackMissingChunk = false;
                    await requestTrack();
                    return;
                  }
                  if (trackData.isNotEmpty) {
                    GpsTrackerScreen.updateTrack(trackData);
                  }
                } else if (msg.startsWith('TRACK|')) {
                  debugPrint("BLE cmd recv TRACK payload: $msg");
                  final trackData = msg.substring(6);
                  GpsTrackerScreen.updateTrack(trackData);
                } else if (msg.startsWith('TEL|')) {
                  final tel = WatchTelemetry.fromBle(msg);
                  latestTelemetry = tel;
                  _telemetryController.add(tel);
                  GpsTrackerScreen.updateLiveSport(tel.sportMode);
                  if (tel.sportMode != 'none') {
                    GpsTrackerScreen.updateLiveSession(tel.sessionId);
                  }
                  if (tel.latitude != 0.0 && tel.longitude != 0.0) {
                    GpsTrackerScreen.updateTrack(
                      '${tel.latitude},${tel.longitude}',
                    );
                  }
                } else if (msg.startsWith('ACT|')) {
                  final session = WatchActivitySession.fromBle(msg);
                  _activityController.add(session);
                  GpsTrackerScreen.updateLiveSession(session.sessionId);
                  GpsTrackerScreen.updateActivityDuration(session.durationSec);
                  if (session.routeRaw.isNotEmpty) {
                    GpsTrackerScreen.updateTrack(session.routeRaw);
                  }
                } else {
                  switch (msg) {
                    case 'GPS_START':
                      debugPrint("BLE cmd recv: GPS_START");
                      // Yêu cầu Native Android cập nhật lại toàn bộ notification đang có
                      NotificationHandler().forceFetchActiveNotifications();
                      break;
                  }
                }
              }
            } catch (e) {
              debugPrint('BLE cmd decode error: $e');
            }
          },
          onError: (e) {
            debugPrint('BLE notify stream error: $e');
          },
        );
  }

  Future<void> _writeRaw(String data) async {
    if (connectionState != DeviceConnectionState.connected) return;
    final ch = _qualifiedChar();
    if (ch == null) return;

    try {
      await _ensureLargeMtu();
      final bytes = utf8.encode(data);
      await _ble.writeCharacteristicWithResponse(ch, value: bytes);
      debugPrint(
        'BLE sent ${bytes.length} byte UTF-8 (${data.runes.length} ký tự): $data',
      );
    } catch (e) {
      debugPrint('BLE write error: $e');
    }
  }

  Future<void> sendNotification(
    String appName,
    String sender,
    String content,
  ) async {
    if (connectionState != DeviceConnectionState.connected ||
        connectedDeviceId == null) {
      return;
    }

    final safeAppName = _truncateUtf8(appName, 20);
    final safeSender = _truncateUtf8(sender, 40);
    final safeContent = _truncateUtf8(content, 90);
    final data = "$safeAppName|$safeSender|$safeContent";
    await _writeRaw(data);
  }

  String _truncateUtf8(String value, int maxBytes) {
    final bytes = utf8.encode(value);
    if (bytes.length <= maxBytes) return value;

    for (var end = maxBytes; end > 0; end--) {
      try {
        return utf8.decode(bytes.sublist(0, end), allowMalformed: false);
      } catch (_) {
        continue;
      }
    }
    return '';
  }

  /// Gửi một bản cập nhật chỉ đường (Google Maps không cần — dữ liệu do app của bạn tính/API).
  Future<void> sendNavUpdate({
    required String currentTurn,
    required int distanceToTurnMeters,
    required String primaryRoad,
    String nextTurn = '',
    String nextRoad = '',
    required int totalRemainingMeters,
  }) async {
    if (connectionState != DeviceConnectionState.connected ||
        connectedDeviceId == null) {
      return;
    }

    final p = sanitizeRoadForBle(primaryRoad);
    final n = sanitizeRoadForBle(nextRoad);
    final nt = nextTurn.trim();
    final payload =
        'NAV|$currentTurn|$distanceToTurnMeters|$p|$nt|$n|$totalRemainingMeters';
    await _writeRaw(payload);
  }

  Future<void> sendNavEnd() async {
    await _writeRaw('NAV_END');
  }

  Future<void> requestTrack() async {
    await _writeRaw('TRACK_REQ');
  }

  Future<void> sendOtaUrl(String url) async {
    final trimmed = url.trim();
    if (trimmed.isEmpty) return;
    await _writeRaw('OTA_URL|$trimmed');
  }

  /// Đồng bộ ngày giờ thực tế từ điện thoại sang đồng hồ
  Future<void> syncTime() async {
    if (connectionState != DeviceConnectionState.connected ||
        connectedDeviceId == null) {
      return;
    }
    final now = DateTime.now();
    final timeStr =
        "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
    final dateStr =
        "${now.day.toString().padLeft(2, '0')}/${now.month.toString().padLeft(2, '0')}/${now.year}";
    final weekday = now.weekday; // 1 (Mon) - 7 (Sun)
    final payload = "TIME|$timeStr|$dateStr|$weekday";
    await _writeRaw(payload);
  }
}
