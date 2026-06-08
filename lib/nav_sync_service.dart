import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:geolocator/geolocator.dart';
import 'package:http/http.dart' as http;

import 'ble_service.dart';

class NavSyncService {
  NavSyncService(this._bleService);

  final BleService _bleService;
  final _statusController = StreamController<String>.broadcast();
  Stream<String> get statusStream => _statusController.stream;

  static const String _envApiKey = String.fromEnvironment(
    'GOOGLE_MAPS_API_KEY',
    defaultValue: '',
  );

  String _manualApiKey = '';

  void setManualApiKey(String key) {
    _manualApiKey = key.trim();
  }

  String get _effectiveApiKey =>
      _manualApiKey.isNotEmpty ? _manualApiKey : _envApiKey.trim();

  StreamSubscription<Position>? _positionSub;
  Timer? _routeRefreshTimer;

  double? _destLat;
  double? _destLng;
  int _totalRemainingM = 0;
  List<_RouteStep> _steps = [];
  bool _running = false;
  bool _routeRefreshInFlight = false;
  DateTime _lastRouteRefreshAttempt = DateTime(2000);
  static const _minRouteRefreshInterval = Duration(seconds: 12);

  bool get isRunning => _running;

  Future<bool> start({required double destLat, required double destLng}) async {
    if (_effectiveApiKey.isEmpty) {
      _setStatus(
        'Chua co API key Directions. Nhap vao o tren hoac chay: '
        '--dart-define=GOOGLE_MAPS_API_KEY=...',
      );
      return false;
    }

    final perm = await _ensureLocationPermission();
    if (!perm) {
      _setStatus('Chua co quyen vi tri.');
      return false;
    }

    _destLat = destLat;
    _destLng = destLng;

    await _positionSub?.cancel();
    _positionSub = Geolocator.getPositionStream(
      locationSettings: const LocationSettings(
        accuracy: LocationAccuracy.bestForNavigation,
        distanceFilter: 5,
      ),
    ).listen(_onPosition, onError: (e) => _setStatus('GPS loi: $e'));

    _routeRefreshTimer?.cancel();
    _routeRefreshTimer = Timer.periodic(const Duration(seconds: 12), (_) {
      _refreshRoute();
    });

    _running = true;
    _setStatus('Da bat dau dong bo NAV.');
    return true;
  }

  Future<void> stop() async {
    _running = false;
    _routeRefreshTimer?.cancel();
    _routeRefreshTimer = null;
    await _positionSub?.cancel();
    _positionSub = null;
    _steps = [];
    _totalRemainingM = 0;
    await _bleService.sendNavEnd();
    _setStatus('Da dung dong bo NAV.');
  }

  Future<void> _onPosition(Position pos) async {
    if (!_running || _destLat == null || _destLng == null) return;

    if (_steps.isEmpty) {
      await _refreshRoute(fromLat: pos.latitude, fromLng: pos.longitude);
    }
    if (_steps.isEmpty) return;

    final closestIndex = _closestStepIndex(pos.latitude, pos.longitude);
    final current = _steps[closestIndex];
    final next = (closestIndex + 1 < _steps.length)
        ? _steps[closestIndex + 1]
        : null;

    final toTurn = _distanceMeters(
      pos.latitude,
      pos.longitude,
      current.endLat,
      current.endLng,
    );

    final remain = _distanceMeters(
      pos.latitude,
      pos.longitude,
      _destLat!,
      _destLng!,
    );
    _totalRemainingM = remain;

    await _bleService.sendNavUpdate(
      currentTurn: current.turnToken,
      distanceToTurnMeters: toTurn,
      primaryRoad: current.roadName,
      nextTurn: next?.turnToken ?? '',
      nextRoad: next?.roadName ?? '',
      totalRemainingMeters: _totalRemainingM,
    );
    _setStatus(
      'NAV gui: ${current.turnToken}, ${toTurn}m, con ${_totalRemainingM}m',
    );
  }

  Future<void> _refreshRoute({double? fromLat, double? fromLng}) async {
    if (_destLat == null || _destLng == null || !_running) return;
    if (_routeRefreshInFlight) return;

    final now = DateTime.now();
    if (now.difference(_lastRouteRefreshAttempt) < _minRouteRefreshInterval) {
      return;
    }
    _lastRouteRefreshAttempt = now;
    _routeRefreshInFlight = true;

    try {
      double? oLat = fromLat;
      double? oLng = fromLng;
      if (oLat == null || oLng == null || (oLat == 0 && oLng == 0)) {
        try {
          final p = await Geolocator.getCurrentPosition();
          oLat = p.latitude;
          oLng = p.longitude;
        } catch (e) {
          _setStatus('Khong lay duoc GPS hien tai: $e');
          return;
        }
      }

      final uri = Uri.parse(
        'https://maps.googleapis.com/maps/api/directions/json'
        '?origin=$oLat,$oLng'
        '&destination=${_destLat!},${_destLng!}'
        '&mode=driving'
        '&language=vi'
        '&key=$_effectiveApiKey',
      );

      final res = await http.get(uri);
      if (res.statusCode != 200) {
        _setStatus('Directions HTTP ${res.statusCode}');
        return;
      }

      final body = jsonDecode(res.body) as Map<String, dynamic>;
      final apiStatus = (body['status'] as String?) ?? '';
      if (apiStatus != 'OK') {
        final err = body['error_message'] as String? ?? '';
        _setStatus(
          'Directions: $apiStatus ${err.isNotEmpty ? "- $err" : ""}'.trim(),
        );
        return;
      }

      final routes = (body['routes'] as List?) ?? const [];
      if (routes.isEmpty) {
        _setStatus('Khong co route (ZERO_RESULTS?).');
        return;
      }

      final leg =
          ((routes.first as Map<String, dynamic>)['legs'] as List).first
              as Map<String, dynamic>;
      _totalRemainingM =
          ((leg['distance'] as Map<String, dynamic>)['value'] as num).toInt();

      final rawSteps = (leg['steps'] as List?) ?? const [];
      _steps = rawSteps
          .map((e) => _RouteStep.fromGoogle(e as Map<String, dynamic>))
          .toList();
      _setStatus('Route cap nhat: ${_steps.length} buoc.');
    } catch (e) {
      _setStatus('Directions loi: $e');
    } finally {
      _routeRefreshInFlight = false;
    }
  }

  int _closestStepIndex(double lat, double lng) {
    if (_steps.isEmpty) return 0;
    var bestIdx = 0;
    var best = 1 << 30;
    for (var i = 0; i < _steps.length; i++) {
      final d = _distanceMeters(lat, lng, _steps[i].endLat, _steps[i].endLng);
      if (d < best) {
        best = d;
        bestIdx = i;
      }
    }
    return bestIdx;
  }

  int _distanceMeters(double lat1, double lon1, double lat2, double lon2) {
    const r = 6371000.0;
    final dLat = _deg2rad(lat2 - lat1);
    final dLon = _deg2rad(lon2 - lon1);
    final a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(_deg2rad(lat1)) *
            cos(_deg2rad(lat2)) *
            sin(dLon / 2) *
            sin(dLon / 2);
    final c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return (r * c).round();
  }

  double _deg2rad(double v) => v * pi / 180.0;

  Future<bool> _ensureLocationPermission() async {
    final enabled = await Geolocator.isLocationServiceEnabled();
    if (!enabled) return false;
    var p = await Geolocator.checkPermission();
    if (p == LocationPermission.denied) {
      p = await Geolocator.requestPermission();
    }
    return p == LocationPermission.always || p == LocationPermission.whileInUse;
  }

  void _setStatus(String msg) {
    if (!_statusController.isClosed) _statusController.add(msg);
  }

  Future<void> dispose() async {
    await stop();
    await _statusController.close();
  }
}

class _RouteStep {
  _RouteStep({
    required this.turnToken,
    required this.roadName,
    required this.endLat,
    required this.endLng,
  });

  final String turnToken;
  final String roadName;
  final double endLat;
  final double endLng;

  factory _RouteStep.fromGoogle(Map<String, dynamic> step) {
    final maneuver = (step['maneuver'] as String?) ?? '';
    final instruction = _stripHtml(
      (step['html_instructions'] as String?) ?? '',
    );
    final end = step['end_location'] as Map<String, dynamic>;
    return _RouteStep(
      turnToken: _mapManeuverToToken(maneuver),
      roadName: instruction.isEmpty ? 'Duong tiep theo' : instruction,
      endLat: (end['lat'] as num).toDouble(),
      endLng: (end['lng'] as num).toDouble(),
    );
  }

  static String _stripHtml(String s) {
    return s
        .replaceAll(RegExp(r'<[^>]*>'), ' ')
        .replaceAll(RegExp(r'\s+'), ' ')
        .trim();
  }

  static String _mapManeuverToToken(String m) {
    if (m.contains('uturn')) return 'uturn';
    if (m.contains('turn-left')) return 'left';
    if (m.contains('turn-right')) return 'right';
    if (m.contains('keep-left')) return 'keep_left';
    if (m.contains('keep-right')) return 'keep_right';
    if (m.contains('merge')) return 'slight_left';
    if (m.contains('fork-left')) return 'slight_left';
    if (m.contains('fork-right')) return 'slight_right';
    if (m.contains('roundabout')) return 'roundabout';
    return 'straight';
  }
}
