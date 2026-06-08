import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'ble_service.dart';
import 'gmaps_nav_parser.dart';

class NotificationHandler {
  static final NotificationHandler _instance = NotificationHandler._internal();
  factory NotificationHandler() => _instance;
  NotificationHandler._internal();

  bool _isListening = false;
  bool _isNavigating = false;
  StreamSubscription<dynamic>? _eventSub;

  final _navStatusController = StreamController<String>.broadcast();
  Stream<String> get navStatusStream => _navStatusController.stream;

  String _lastSentTurn = '';
  int _lastSentDistance = -1;
  String _lastSentRoad = '';
  DateTime _lastSentTime = DateTime(2000);
  static const _minSendInterval = Duration(seconds: 2);

  bool get isNavigating => _isNavigating;

  // ── Bộ lọc ứng dụng thông báo (Notification Filter Switches) ──
  static bool enableMessenger = true;
  static bool enableZalo = true;
  static bool enableTelegram = true;
  static bool enableWhatsApp = true;
  static bool enableSMS = true;

  static const MethodChannel _methodChannel = MethodChannel(
    'com.example.app/notif_method',
  );
  static const EventChannel _eventChannel = EventChannel(
    'com.example.app/notif_event',
  );

  Future<void> init() async {
    if (_isListening) {
      debugPrint('[NOTIF] Already listening, skip init');
      return;
    }

    debugPrint('[NOTIF] init() called (Native Mode)...');

    bool status = false;
    try {
      status =
          await _methodChannel.invokeMethod('isPermissionGranted') ?? false;
      debugPrint('[NOTIF] isPermissionGranted = $status');
    } catch (e) {
      debugPrint('[NOTIF] isPermissionGranted error: $e');
    }

    if (!status) {
      debugPrint('[NOTIF] Permission NOT granted! Cannot listen.');
      _setNavStatus('Chưa cấp quyền đọc thông báo!');
      return;
    }

    debugPrint('[NOTIF] Permission OK, starting stream...');

    try {
      await _eventSub?.cancel();
      _eventSub = _eventChannel.receiveBroadcastStream().listen(
        (event) {
          if (event is Map) {
            _handleNotificationMap(event);
          }
        },
        onError: (e) {
          debugPrint('[NOTIF] Stream error: $e');
          _isListening = false;
          _eventSub?.cancel();
          _eventSub = null;
        },
        onDone: () {
          debugPrint('[NOTIF] Stream closed');
          _isListening = false;
          _eventSub = null;
        },
      );
      _isListening = true;
      debugPrint('[NOTIF] Listener started OK!');

      // Fetch active notifications right away to catch ongoing Google Maps navigation!
      _methodChannel.invokeMethod('getActiveNotifications');
    } catch (e) {
      debugPrint('[NOTIF] Failed to start stream: $e');
    }
  }

  void _handleNotificationMap(Map event) {
    final pkg = event['packageName']?.toString() ?? '';
    final title = event['title']?.toString() ?? '';
    final content = event['content']?.toString() ?? '';
    final ticker = event['ticker']?.toString() ?? '';
    final removed = event['isRemoved'] == true;

    debugPrint(
      '[NOTIF] pkg=$pkg | title=$title | content=$content | ticker=$ticker',
    );

    // ── Google Maps ─────────────────────────────────────────
    if (pkg == 'com.google.android.apps.maps') {
      if (removed) {
        if (_isNavigating) {
          _isNavigating = false;
          _resetThrottle();
          BleService().sendNavEnd();
          _setNavStatus('Đã kết thúc dẫn đường.');
          debugPrint('[NAV] Navigation ended');
        }
        return;
      }

      if (!GmapsNavParser.isNavNotification(title, content)) {
        debugPrint('[NAV] Maps notif but NOT navigation');
        return;
      }

      final navData = GmapsNavParser.parse(title, content);
      debugPrint('[NAV] Parsed: $navData');

      if (navData == null || !navData.isValid) {
        debugPrint('[NAV] Parse failed');
        return;
      }

      _isNavigating = true;
      _sendNavIfChanged(navData);
      return;
    }

    // ── Social apps ─────────────────────────────────────────
    if (removed) return;
    if (content.isEmpty && title.isEmpty) return;

    String appName = _getAppName(pkg);
    if (appName == 'Other') return;

    // Áp dụng bộ lọc cấu hình từ người dùng
    if (appName == 'Messenger' && !enableMessenger) {
      debugPrint('[FILTER] Messenger blocked by user setting');
      return;
    }
    if (appName == 'Zalo' && !enableZalo) {
      debugPrint('[FILTER] Zalo blocked by user setting');
      return;
    }
    if (appName == 'Telegram' && !enableTelegram) {
      debugPrint('[FILTER] Telegram blocked by user setting');
      return;
    }
    if (appName == 'WhatsApp' && !enableWhatsApp) {
      debugPrint('[FILTER] WhatsApp blocked by user setting');
      return;
    }
    if (appName == 'SMS' && !enableSMS) {
      debugPrint('[FILTER] SMS blocked by user setting');
      return;
    }

    String t = title.replaceAll('|', '-').replaceAll('\n', ' ');
    String c = content.replaceAll('|', '-').replaceAll('\n', ' ');
    if (t.length > 30) t = t.substring(0, 30);
    if (c.length > 100) c = c.substring(0, 100);

    BleService().sendNotification(appName, t, c);
  }

  void _sendNavIfChanged(GmapsNavData data) {
    final now = DateTime.now();
    final turnChanged = data.turnToken != _lastSentTurn;
    final roadChanged = data.roadName != _lastSentRoad;
    final distChanged = (data.distanceMeters - _lastSentDistance).abs() >= 10;
    final timeOk = now.difference(_lastSentTime) >= _minSendInterval;

    if (!turnChanged && !roadChanged && !distChanged && !timeOk) return;

    _lastSentTurn = data.turnToken;
    _lastSentDistance = data.distanceMeters;
    _lastSentRoad = data.roadName;
    _lastSentTime = now;

    BleService().sendNavUpdate(
      currentTurn: data.turnToken,
      distanceToTurnMeters: data.distanceMeters,
      primaryRoad: data.roadName,
      nextTurn: '',
      nextRoad: '',
      totalRemainingMeters: 0,
    );

    final statusMsg =
        '${data.turnToken} — ${data.distanceMeters}m — ${data.roadName}';
    _setNavStatus(statusMsg);
    debugPrint('[NAV] SENT: $statusMsg');
  }

  void _resetThrottle() {
    _lastSentTurn = '';
    _lastSentDistance = -1;
    _lastSentRoad = '';
    _lastSentTime = DateTime(2000);
  }

  void _setNavStatus(String msg) {
    if (!_navStatusController.isClosed) {
      _navStatusController.add(msg);
    }
  }

  String _getAppName(String packageName) {
    if (packageName.contains("facebook.orca") ||
        packageName.contains("facebook.mlite")) {
      return "Messenger";
    }
    if (packageName.contains("zing.zalo")) return "Zalo";
    if (packageName.contains("telegram")) return "Telegram";
    if (packageName.contains("whatsapp")) return "WhatsApp";
    if (packageName.contains("mms") ||
        packageName.contains("sms") ||
        packageName.contains("messaging")) {
      return "SMS";
    }
    return "Other";
  }

  Future<void> requestPermission() async {
    try {
      await _methodChannel.invokeMethod('requestPermission');
    } catch (e) {
      debugPrint('[NOTIF] requestPermission error: $e');
    }
  }

  void forceFetchActiveNotifications() {
    debugPrint('[NOTIF] Forcing fetch of active notifications...');
    _methodChannel.invokeMethod('getActiveNotifications');
  }

  Future<bool> replyToNotification(
    String appName,
    String title,
    String message,
  ) async {
    try {
      debugPrint(
        '[NOTIF] Replying to notification: app=$appName, title=$title, msg=$message',
      );
      final bool? success = await _methodChannel.invokeMethod<bool>(
        'sendReply',
        {'appName': appName, 'title': title, 'message': message},
      );
      debugPrint('[NOTIF] Reply success status: $success');
      return success ?? false;
    } catch (e) {
      debugPrint('[NOTIF] Error invoking sendReply: $e');
      return false;
    }
  }
}
