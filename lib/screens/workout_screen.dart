import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import '../ble_service.dart';
import '../user_profile_settings.dart';

class WorkoutScreen extends StatefulWidget {
  const WorkoutScreen({super.key});

  @override
  State<WorkoutScreen> createState() => _WorkoutScreenState();
}

class _WorkoutScreenState extends State<WorkoutScreen> {
  final BleService _bleService = BleService();
  DeviceConnectionState _connectionState = DeviceConnectionState.disconnected;

  bool _hasAcceptedAlert = false;
  bool _isMapExpanded = false;

  // Controller Ä‘á»ƒ Ä‘iá»u khiá»ƒn báº£n Ä‘á»“ thá»±c táº¿
  final MapController _mapController = MapController();
  final MapController _fullscreenMapController = MapController();
  final Stopwatch _stopwatch = Stopwatch();
  Timer? _durationTimer;

  // ThÃ´ng sá»‘ thá»ƒ thao thá»i gian thá»±c hiá»‡n táº¡i
  int _steps = 0;
  double _distance = 0.0;
  double _speed = 0.0;
  int _calories = 0;
  int _seconds = 0;
  int _activeSessionId = 0;
  String _currentSportMode = 'none';
  StreamSubscription<WatchTelemetry>? _telemetrySub;
  StreamSubscription<WatchActivitySession>? _activitySub;

  // Tuyáº¿n Ä‘Æ°á»ng thá»±c táº¿ xung quanh Há»“ XuÃ¢n HÆ°Æ¡ng, ÄÃ  Láº¡t (Kinh Ä‘á»™, VÄ© Ä‘á»™ tháº­t)
  final List<LatLng> _routePoints = [];

  // Danh sÃ¡ch lá»‹ch sá»­ cÃ¡c phiÃªn táº­p luyá»‡n Ä‘Æ°á»£c lÆ°u
  final List<WorkoutSession> _savedSessions = [];

  @override
  void initState() {
    super.initState();

    _connectionState = _bleService.connectionState;
    _bleService.statusStream.listen((state) {
      if (!mounted) return;
      setState(() {
        _connectionState = state;
        if (state == DeviceConnectionState.disconnected) {
          _hasAcceptedAlert = false;
          _isMapExpanded = false;
          _routePoints.clear();
        }
      });
    });

    _telemetrySub = _bleService.telemetryStream.listen((tel) {
      if (!mounted) return;
      setState(() {
        if (tel.sportMode != 'none' &&
            tel.sessionId > 0 &&
            tel.sessionId != _activeSessionId) {
          _activeSessionId = tel.sessionId;
          _steps = 0;
          _distance = 0.0;
          _speed = 0.0;
          _calories = 0;
          _resetWorkoutClock();
          _routePoints.clear();
        }
        _steps = tel.steps;
        _distance = tel.distanceKm;
        _speed = tel.speedKmh;
        _currentSportMode = tel.sportMode;
        _syncWorkoutClock(tel.sportMode != 'none');
        _calories = _calcCalories(tel.sportMode, _seconds);
        if (tel.latitude != 0.0 && tel.longitude != 0.0) {
          final next = LatLng(tel.latitude, tel.longitude);
          if (_routePoints.isEmpty ||
              _routePoints.last.latitude != next.latitude ||
              _routePoints.last.longitude != next.longitude) {
            _routePoints.add(next);
          }
        }
        _hasAcceptedAlert = tel.sportMode != 'none';
      });
    });

    _activitySub = _bleService.activityStream.listen((session) {
      if (!mounted) return;
      final newId = session.sessionId > 0
          ? 'watch_${session.sessionId}'
          : 'watch_${session.time.millisecondsSinceEpoch}';
      if (_isDuplicateWatchSession(newId, session)) return;
      final route = session.routeRaw.isNotEmpty
          ? _routeFromRaw(session.routeRaw)
          : List<LatLng>.from(_routePoints);
      setState(() {
        _savedSessions.insert(
          0,
          WorkoutSession(
            id: newId,
            time: session.time,
            steps: session.steps,
            distance: session.distanceKm,
            speed: session.avgSpeedKmh,
            calories: _calcCalories(
              session.sportId == 5 ? 'cycling' : 'walking',
              session.durationSec,
            ),
            seconds: session.durationSec,
            route: route,
          ),
        );
      });
    });
  }

  @override
  void dispose() {
    _telemetrySub?.cancel();
    _activitySub?.cancel();
    _durationTimer?.cancel();
    super.dispose();
  }

  void _syncWorkoutClock(bool isActive) {
    if (isActive) {
      if (!_stopwatch.isRunning) _stopwatch.start();
      _durationTimer ??= Timer.periodic(const Duration(seconds: 1), (_) {
        if (!mounted || !_stopwatch.isRunning) return;
        setState(() {
          _seconds = _stopwatch.elapsed.inSeconds;
          _calories = _calcCalories(_currentSportMode, _seconds);
        });
      });
    } else {
      _stopwatch.stop();
      _durationTimer?.cancel();
      _durationTimer = null;
    }
    _seconds = _stopwatch.elapsed.inSeconds;
  }

  void _resetWorkoutClock() {
    _stopwatch
      ..stop()
      ..reset();
    _durationTimer?.cancel();
    _durationTimer = null;
    _seconds = 0;
  }

  bool _isDuplicateWatchSession(String newId, WatchActivitySession session) {
    final existingIds = _savedSessions.map((s) => s.id).toSet();
    if (existingIds.contains(newId)) return true;

    return _savedSessions.any((s) {
      final sameStats =
          s.steps == session.steps &&
          (s.distance - session.distanceKm).abs() < 0.01 &&
          (s.speed - session.avgSpeedKmh).abs() < 0.1 &&
          (s.seconds - session.durationSec).abs() <= 2;
      return sameStats && (session.steps > 0 || session.distanceKm > 0);
    });
  }

  int _calcCalories(String sportMode, int durationSec) {
    return UserProfileSettings.calcCalories(
      sportMode: sportMode,
      durationSec: durationSec,
    );
  }

  List<LatLng> _routeFromRaw(String raw) {
    final result = <LatLng>[];
    for (final pt in raw.split(';')) {
      final p = pt.split(',');
      if (p.length != 2) continue;
      final lat = double.tryParse(p[0]);
      final lon = double.tryParse(p[1]);
      if (lat != null && lon != null && lat != 0.0 && lon != 0.0) {
        result.add(LatLng(lat, lon));
      }
    }
    return result;
  }

  void _showWorkoutFeatureAlert() {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF0F172A),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(24),
          side: const BorderSide(color: Color(0xFF10B981), width: 1.5),
        ),
        title: const Row(
          children: [
            Icon(
              Icons.directions_run_rounded,
              color: Color(0xFF10B981),
              size: 28,
            ),
            SizedBox(width: 10),
            Text(
              'LUYỆN TẬP',
              style: TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.5,
              ),
            ),
          ],
        ),
        content: const Text(
          'Vui lòng bật chế độ thể thao trên đồng hồ để bắt đầu đồng bộ lộ trình.',
          style: TextStyle(color: Colors.white70, fontSize: 14, height: 1.4),
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              setState(() {
                _hasAcceptedAlert = true;
                // Khởi tạo lại giá trị ban đầu khi bấm đo mới
                _steps = 0;
                _distance = 0.0;
                _speed = 0.0;
                _calories = 0;
                _resetWorkoutClock();
                _activeSessionId = 0;
                _currentSportMode = 'none';
                _routePoints.clear();
              });
            },
            style: TextButton.styleFrom(
              backgroundColor: const Color(0xFF10B981),
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
              ),
            ),
            child: const Text(
              'Đã hiểu',
              style: TextStyle(fontWeight: FontWeight.bold, fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }

  // LÆ¯U PHIÃŠN Táº¬P THÃ€NH CÃ”NG VÃ€O Lá»ŠCH Sá»¬
  void _saveCurrentSession() {
    if (_steps == 0 && _distance == 0.0) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text("ChÆ°a cÃ³ thÃ´ng sá»‘ luyá»‡n táº­p nÃ o Ä‘á»ƒ lÆ°u!"),
          backgroundColor: Colors.redAccent,
        ),
      );
      return;
    }

    final newSession = WorkoutSession(
      id: 'session_${DateTime.now().millisecondsSinceEpoch}',
      time: DateTime.now(),
      steps: _steps,
      distance: _distance,
      speed: _speed,
      calories: _calories,
      seconds: _seconds,
      route: List.from(_routePoints),
    );

    setState(() {
      _savedSessions.insert(0, newSession); // LÆ°u lÃªn Ä‘áº§u danh sÃ¡ch
      _hasAcceptedAlert = false; // Táº¯t tráº¡ng thÃ¡i Ä‘ang Ä‘o

      // XÃ³a sáº¡ch thÃ´ng sá»‘ Ä‘á»ƒ chuáº©n bá»‹ cho phiÃªn má»›i
      _steps = 0;
      _distance = 0.0;
      _speed = 0.0;
      _calories = 0;
      _resetWorkoutClock();
      _activeSessionId = 0;
      _currentSportMode = 'none';
      _routePoints.clear();
    });

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Row(
          children: [
            Icon(Icons.check_circle_rounded, color: Colors.white),
            SizedBox(width: 10),
            Text("ÄÃ£ lÆ°u phiÃªn táº­p vÃ o lá»‹ch sá»­ thÃ nh cÃ´ng!"),
          ],
        ),
        backgroundColor: Color(0xFF10B981),
      ),
    );
  }

  // XÃ“A Sáº CH VÃ€ Báº®T Äáº¦U PHIÃŠN Táº¬P Má»šI
  void _resetWorkout() {
    setState(() {
      _steps = 0;
      _distance = 0.0;
      _speed = 0.0;
      _calories = 0;
      _resetWorkoutClock();
      _activeSessionId = 0;
      _currentSportMode = 'none';
      _routePoints.clear();
      _hasAcceptedAlert = false;
    });

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text("ÄÃ£ xÃ³a dá»¯ liá»‡u luyá»‡n táº­p hiá»‡n táº¡i."),
        backgroundColor: Colors.orangeAccent,
      ),
    );
  }

  String _formatDuration(int totalSeconds) {
    int minutes = totalSeconds ~/ 60;
    int seconds = totalSeconds % 60;
    return '${minutes.toString().padLeft(2, '0')}:${seconds.toString().padLeft(2, '0')}';
  }

  @override
  Widget build(BuildContext context) {
    final isConnected = _connectionState == DeviceConnectionState.connected;
    final isWorkoutActive = isConnected && _hasAcceptedAlert;

    return DefaultTabController(
      length: 2,
      child: Scaffold(
        backgroundColor: const Color(0xFF0F172A),
        appBar: AppBar(
          backgroundColor: const Color(0xFF0F172A),
          elevation: 0,
          title: const Text(
            'WORKOUT',
            style: TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.bold,
              letterSpacing: 2,
              fontSize: 16,
            ),
          ),
          centerTitle: true,
          bottom: PreferredSize(
            preferredSize: const Size.fromHeight(48),
            child: Container(
              margin: const EdgeInsets.symmetric(horizontal: 20, vertical: 4),
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.03),
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
              ),
              child: TabBar(
                indicator: BoxDecoration(
                  color: const Color(0xFF10B981),
                  borderRadius: BorderRadius.circular(12),
                ),
                indicatorSize: TabBarIndicatorSize.tab,
                labelColor: Colors.white,
                unselectedLabelColor: Colors.white30,
                labelStyle: const TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 12,
                ),
                unselectedLabelStyle: const TextStyle(fontSize: 12),
                dividerColor: Colors.transparent,
                tabs: const [
                  Tab(text: 'Luyện tập'),
                  Tab(text: 'Lịch sử phiên'),
                ],
              ),
            ),
          ),
        ),
        body: Container(
          decoration: const BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: [Color(0xFF0F172A), Color(0xFF020617)],
            ),
          ),
          child: Stack(
            children: [
              TabBarView(
                physics:
                    const NeverScrollableScrollPhysics(), // TrÃ¡nh vuá»‘t ngang cháº¡m Ä‘á»™t ngá»™t
                children: [
                  _buildExerciseTab(isConnected, isWorkoutActive),
                  _buildHistoryTab(),
                ],
              ),

              // GIAO DIá»†N Báº¢N Ä á»’ PHÃ“NG TO TOÃ€N MÃ€N HÃŒNH (FULL-SCREEN MAP OVERLAY)
              if (_isMapExpanded)
                Positioned.fill(
                  child: Container(
                    color: const Color(0xFF0F172A),
                    child: SafeArea(
                      child: Stack(
                        children: [
                          Positioned.fill(
                            child: _buildRealGeographicMap(
                              16.0,
                              _routePoints,
                              controller: _fullscreenMapController,
                            ),
                          ),
                          Positioned(
                            top: 20,
                            right: 20,
                            child: GestureDetector(
                              onTap: () {
                                setState(() {
                                  _isMapExpanded = false;
                                });
                              },
                              child: Container(
                                padding: const EdgeInsets.all(12),
                                decoration: BoxDecoration(
                                  color: Colors.black.withValues(alpha: 0.6),
                                  shape: BoxShape.circle,
                                  border: Border.all(
                                    color: Colors.white.withValues(alpha: 0.1),
                                  ),
                                ),
                                child: const Icon(
                                  Icons.close_rounded,
                                  color: Colors.white,
                                  size: 24,
                                ),
                              ),
                            ),
                          ),
                          Positioned(
                            bottom: 40,
                            left: 20,
                            right: 20,
                            child: Container(
                              padding: const EdgeInsets.all(20),
                              decoration: BoxDecoration(
                                color: const Color(
                                  0xFF0F172A,
                                ).withValues(alpha: 0.85),
                                borderRadius: BorderRadius.circular(24),
                                border: Border.all(
                                  color: Colors.white.withValues(alpha: 0.08),
                                ),
                              ),
                              child: Row(
                                mainAxisAlignment:
                                    MainAxisAlignment.spaceBetween,
                                children: [
                                  _buildExpandedMapStat(
                                    'QUÃNG ĐƯỜNG',
                                    '${_distance.toStringAsFixed(2)} km',
                                  ),
                                  _buildExpandedMapStat(
                                    'THỜI GIAN',
                                    _formatDuration(_seconds),
                                  ),
                                  _buildExpandedMapStat(
                                    'VẬN TỐC',
                                    '${_speed.toStringAsFixed(1)} km/h',
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }

  // SUB-TAB 1: LUYỆN TẬP
  Widget _buildExerciseTab(bool isConnected, bool isWorkoutActive) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(20.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Banner cảnh báo yêu cầu chọn chức năng / kết nối
          if (!isWorkoutActive)
            Container(
              margin: const EdgeInsets.only(bottom: 20),
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color:
                    (isConnected ? const Color(0xFF10B981) : Colors.redAccent)
                        .withValues(alpha: 0.08),
                borderRadius: BorderRadius.circular(18),
                border: Border.all(
                  color:
                      (isConnected ? const Color(0xFF10B981) : Colors.redAccent)
                          .withValues(alpha: 0.2),
                ),
              ),
              child: Row(
                children: [
                  Icon(
                    Icons.info_outline_rounded,
                    color: isConnected
                        ? const Color(0xFF10B981)
                        : Colors.redAccent,
                    size: 20,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      isConnected
                          ? 'Hãy bắt đầu chế độ thể thao trên đồng hồ.'
                          : 'Đồng hồ chưa kết nối. Vui lòng kết nối ở màn hình chính.',
                      style: TextStyle(
                        color: isConnected
                            ? const Color(0xFF10B981)
                            : Colors.redAccent,
                        fontSize: 12,
                        height: 1.3,
                      ),
                    ),
                  ),
                  if (isConnected) ...[
                    const SizedBox(width: 8),
                    ElevatedButton(
                      onPressed: _showWorkoutFeatureAlert,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF10B981),
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(
                          horizontal: 12,
                          vertical: 4,
                        ),
                        minimumSize: Size.zero,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                      ),
                      child: const Text(
                        'Đo ngay',
                        style: TextStyle(
                          fontSize: 11,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ],
                ],
              ),
            ),

          // Khá»‘i thÃ´ng sá»‘ (N/A khi chÆ°a Ä‘o)
          _buildWorkoutStatsGrid(isWorkoutActive),
          const SizedBox(height: 20),

          // Bá»™ nÃºt lÆ°u hoáº·c lÃ m má»›i phiÃªn Ä‘o náº¿u cÃ³ káº¿t ná»‘i hoáº¡t Ä‘á»™ng
          if (isWorkoutActive) ...[
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: _saveCurrentSession,
                    icon: const Icon(Icons.save_rounded),
                    label: const Text('LÆ°u phiÃªn táº­p'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: const Color(0xFF10B981),
                      foregroundColor: Colors.white,
                      minimumSize: const Size(double.infinity, 50),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(14),
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: OutlinedButton.icon(
                    onPressed: _resetWorkout,
                    icon: const Icon(Icons.refresh_rounded),
                    label: const Text('XÃ³a & Váº½ má»›i'),
                    style: OutlinedButton.styleFrom(
                      foregroundColor: Colors.orangeAccent,
                      side: const BorderSide(color: Colors.orangeAccent),
                      minimumSize: const Size(double.infinity, 50),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(14),
                      ),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 24),
          ],

          // VÃ¹ng báº£n Ä‘á»“ OSM (áº¨n hoÃ n toÃ n khi ngáº¯t káº¿t ná»‘i)
          if (isConnected) ...[
            const Text(
              'BẢN ĐỒ LỘ TRÌNH',
              style: TextStyle(
                color: Colors.white38,
                fontSize: 11,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.5,
              ),
            ),
            const SizedBox(height: 12),

            GestureDetector(
              onTap: () {
                setState(() {
                  _isMapExpanded = true;
                });
              },
              child: Container(
                height: 240,
                width: double.infinity,
                decoration: BoxDecoration(
                  color: Colors.black.withValues(alpha: 0.2),
                  borderRadius: BorderRadius.circular(28),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.05),
                  ),
                ),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(28),
                  child: Stack(
                    children: [
                      // Náº¾U CÃ“ Káº¾T Ná»I MÃ€ CHÆ¯A KÃCH HOáº T WIFI/GPS (CHÆ¯A ÄO): YÃŠU Cáº¦U Káº¾T Ná»I
                      if (!isWorkoutActive)
                        Positioned.fill(
                          child: Container(
                            color: const Color(
                              0xFF1E293B,
                            ).withValues(alpha: 0.4),
                            padding: const EdgeInsets.all(24),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Container(
                                  padding: const EdgeInsets.all(12),
                                  decoration: BoxDecoration(
                                    color: Colors.orangeAccent.withValues(
                                      alpha: 0.1,
                                    ),
                                    shape: BoxShape.circle,
                                  ),
                                  child: const Icon(
                                    Icons.wifi_off_rounded,
                                    color: Colors.orangeAccent,
                                    size: 32,
                                  ),
                                ),
                                const SizedBox(height: 16),
                                const Text(
                                  'BẢN ĐỒ LỘ TRÌNH',
                                  textAlign: TextAlign.center,
                                  style: TextStyle(
                                    color: Colors.white,
                                    fontWeight: FontWeight.bold,
                                    fontSize: 13,
                                    letterSpacing: 1,
                                  ),
                                ),
                                const SizedBox(height: 6),
                                const Text(
                                  'Hãy kích hoạt chế độ GPS trên đồng hồ để hiển thị lộ trình di chuyển.',
                                  textAlign: TextAlign.center,
                                  style: TextStyle(
                                    color: Colors.white38,
                                    fontSize: 11,
                                    height: 1.4,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        )
                      // Náº¾U ÄÃƒ KÃCH HOáº T ÄO: HIá»‚N THá»Š Báº¢N Äá»’ THá»°C Táº¾
                      else ...[
                        Positioned.fill(
                          child: _buildRealGeographicMap(14.5, _routePoints),
                        ),
                        Positioned(
                          bottom: 12,
                          right: 12,
                          child: Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 10,
                              vertical: 6,
                            ),
                            decoration: BoxDecoration(
                              color: Colors.black.withValues(alpha: 0.6),
                              borderRadius: BorderRadius.circular(10),
                            ),
                            child: const Row(
                              children: [
                                Icon(
                                  Icons.zoom_out_map_rounded,
                                  color: Colors.white70,
                                  size: 14,
                                ),
                                SizedBox(width: 4),
                                Text(
                                  'Phóng to',
                                  style: TextStyle(
                                    color: Colors.white70,
                                    fontSize: 10,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                        Positioned(
                          top: 12,
                          left: 12,
                          child: Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 8,
                              vertical: 4,
                            ),
                            decoration: BoxDecoration(
                              color: const Color(
                                0xFF10B981,
                              ).withValues(alpha: 0.8),
                              borderRadius: BorderRadius.circular(6),
                            ),
                            child: const Text(
                              'GPS HOẠT ĐỘNG',
                              style: TextStyle(
                                color: Colors.white,
                                fontSize: 9,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
              ),
            ),
          ] else ...[
            // Náº¾U CHÆ¯A Káº¾T Ná»I: Báº¢N Äá»’ áº¨N HOÃ€N TOÃ€N
            const SizedBox.shrink(),
          ],
        ],
      ),
    );
  }

  // SUB-TAB 2: Lá»ŠCH Sá»¬ PHIÃŠN ÄÃƒ LÆ¯U
  Widget _buildHistoryTab() {
    if (_savedSessions.isEmpty) {
      return const Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.history_rounded, color: Colors.white24, size: 50),
            SizedBox(height: 12),
            Text(
              'Chưa có phiên tập nào được lưu.',
              style: TextStyle(color: Colors.white30, fontSize: 13),
            ),
          ],
        ),
      );
    }

    return ListView.builder(
      itemCount: _savedSessions.length,
      padding: const EdgeInsets.all(20),
      itemBuilder: (context, index) {
        final session = _savedSessions[index];
        final dateStr =
            '${session.time.day.toString().padLeft(2, '0')}/${session.time.month.toString().padLeft(2, '0')}/${session.time.year} ${session.time.hour.toString().padLeft(2, '0')}:${session.time.minute.toString().padLeft(2, '0')}';

        return Container(
          margin: const EdgeInsets.symmetric(vertical: 8),
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.02),
            borderRadius: BorderRadius.circular(20),
            border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
          ),
          child: ListTile(
            contentPadding: const EdgeInsets.symmetric(
              horizontal: 20,
              vertical: 10,
            ),
            leading: Container(
              padding: const EdgeInsets.all(10),
              decoration: const BoxDecoration(
                color: Color(0xFF10B981),
                shape: BoxShape.circle,
              ),
              child: const Icon(
                Icons.directions_run_rounded,
                color: Colors.white,
                size: 20,
              ),
            ),
            title: Text(
              'Quãng đường: ${session.distance.toStringAsFixed(2)} km',
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                fontSize: 14,
              ),
            ),
            subtitle: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const SizedBox(height: 6),
                Row(
                  children: [
                    const Icon(
                      Icons.timer_rounded,
                      color: Colors.white30,
                      size: 12,
                    ),
                    const SizedBox(width: 4),
                    Text(
                      _formatDuration(session.seconds),
                      style: const TextStyle(
                        color: Colors.white30,
                        fontSize: 11,
                      ),
                    ),
                    const SizedBox(width: 14),
                    const Icon(
                      Icons.directions_walk_rounded,
                      color: Colors.white30,
                      size: 12,
                    ),
                    const SizedBox(width: 4),
                    Text(
                      '${session.steps} bước',
                      style: const TextStyle(
                        color: Colors.white30,
                        fontSize: 11,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                Text(
                  dateStr,
                  style: const TextStyle(color: Colors.white24, fontSize: 10),
                ),
              ],
            ),
            trailing: const Icon(
              Icons.arrow_forward_ios_rounded,
              color: Colors.white30,
              size: 14,
            ),
            onTap: () => _showSessionDetails(session),
          ),
        );
      },
    );
  }

  // MODAL HIỂN THỊ CHI TIẾT PHIÊN ĐÃ LƯU KÈM BẢN ĐỒ TRACKING
  void _showSessionDetails(WorkoutSession session) {
    final dateStr =
        '${session.time.day.toString().padLeft(2, '0')}/${session.time.month.toString().padLeft(2, '0')}/${session.time.year} ${session.time.hour.toString().padLeft(2, '0')}:${session.time.minute.toString().padLeft(2, '0')}';

    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: const Color(0xFF0F172A),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(28)),
      ),
      builder: (context) => DraggableScrollableSheet(
        initialChildSize: 0.85,
        maxChildSize: 0.9,
        minChildSize: 0.5,
        expand: false,
        builder: (context, scrollController) => Column(
          children: [
            const SizedBox(height: 12),
            Container(
              width: 40,
              height: 4,
              decoration: BoxDecoration(
                color: Colors.white24,
                borderRadius: BorderRadius.circular(10),
              ),
            ),
            const SizedBox(height: 18),
            Text(
              'CHI TIẾT PHIÊN - $dateStr',
              style: const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                fontSize: 13,
                letterSpacing: 1.5,
              ),
            ),
            const SizedBox(height: 18),

            // Bản đồ OSM tracking phiên đã lưu
            Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 20),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(24),
                  child: Container(
                    decoration: BoxDecoration(
                      border: Border.all(
                        color: Colors.white.withValues(alpha: 0.08),
                      ),
                      borderRadius: BorderRadius.circular(24),
                    ),
                    child: _buildRealGeographicMap(15.5, session.route),
                  ),
                ),
              ),
            ),
            const SizedBox(height: 20),

            // Chỉ số tổng hợp của phiên
            Container(
              margin: const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.01),
                borderRadius: BorderRadius.circular(24),
                border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
              ),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      _buildExpandedMapStat(
                        'QUÃNG ĐƯỜNG',
                        '${session.distance.toStringAsFixed(2)} km',
                      ),
                      _buildExpandedMapStat('SỐ BƯỚC', '${session.steps}'),
                      _buildExpandedMapStat(
                        'THỜI GIAN',
                        _formatDuration(session.seconds),
                      ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      _buildExpandedMapStat(
                        'VẬN TỐC TB',
                        '${session.speed.toStringAsFixed(1)} km/h',
                      ),
                      _buildExpandedMapStat(
                        'NĂNG LƯỢNG',
                        '${session.calories} kcal',
                      ),
                      const SizedBox(width: 80), // Cân bằng không gian
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 10),

            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 10),
              child: ElevatedButton(
                onPressed: () => Navigator.pop(context),
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF1E293B),
                  foregroundColor: Colors.white,
                  minimumSize: const Size(double.infinity, 50),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(14),
                  ),
                ),
                child: const Text(
                  'Đóng',
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
              ),
            ),
            const SizedBox(height: 12),
          ],
        ),
      ),
    );
  }

  // WIDGET DỰNG BẢN ĐỒ ĐỊA LÀ THỰC TẾ SỬ DỤNG OPENSTREETMAP VÀ FLUTTER MAP
  Widget _buildRealGeographicMap(
    double zoom,
    List<LatLng> route, {
    MapController? controller,
  }) {
    final LatLng mapCenter = route.isNotEmpty
        ? route.last
        : const LatLng(11.9422, 108.4411);

    return FlutterMap(
      mapController: controller ?? _mapController,
      options: MapOptions(
        initialCenter: mapCenter,
        initialZoom: zoom,
        minZoom: 10,
        maxZoom: 18,
      ),
      children: [
        TileLayer(
          urlTemplate: 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
          subdomains: const ['a', 'b', 'c'],
          tileBuilder: (context, tileWidget, tile) {
            return ColorFiltered(
              colorFilter: const ColorFilter.matrix([
                -0.2, -0.2, -0.2, 0.0, 255.0, // Đảo màu sang Dark Map
                -0.2, -0.2, -0.2, 0.0, 255.0,
                -0.2, -0.2, -0.2, 0.0, 255.0,
                0.0, 0.0, 0.0, 1.0, 0.0,
              ]),
              child: tileWidget,
            );
          },
        ),
        PolylineLayer(
          polylines: [
            Polyline(
              points: route,
              strokeWidth: 9.0,
              color: const Color(0xFF10B981).withValues(alpha: 0.35),
              strokeCap: StrokeCap.round,
            ),
            Polyline(
              points: route,
              strokeWidth: 4.0,
              color: const Color(0xFF10B981),
              strokeCap: StrokeCap.round,
            ),
          ],
        ),
        MarkerLayer(
          markers: [
            if (route.isNotEmpty) ...[
              Marker(
                point: route.first,
                width: 18,
                height: 18,
                child: Container(
                  decoration: BoxDecoration(
                    color: const Color(0xFF3B82F6),
                    shape: BoxShape.circle,
                    border: Border.all(color: Colors.white, width: 2.5),
                    boxShadow: [
                      BoxShadow(
                        color: const Color(0xFF3B82F6).withValues(alpha: 0.5),
                        blurRadius: 8,
                        spreadRadius: 2,
                      ),
                    ],
                  ),
                ),
              ),
              Marker(
                point: route.last,
                width: 22,
                height: 22,
                child: Container(
                  decoration: BoxDecoration(
                    color: const Color(0xFFEF4444),
                    shape: BoxShape.circle,
                    border: Border.all(color: Colors.white, width: 3.0),
                    boxShadow: [
                      BoxShadow(
                        color: const Color(0xFFEF4444).withValues(alpha: 0.6),
                        blurRadius: 10,
                        spreadRadius: 3,
                      ),
                    ],
                  ),
                  child: Center(
                    child: Container(
                      width: 6,
                      height: 6,
                      decoration: const BoxDecoration(
                        color: Colors.white,
                        shape: BoxShape.circle,
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ],
        ),
      ],
    );
  }

  // KHỞI THÔNG SỐ GRID THỂ THAO
  Widget _buildWorkoutStatsGrid(bool isActive) {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.01),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
      ),
      child: Column(
        children: [
          Row(
            children: [
              Expanded(
                child: _buildSingleStatItem(
                  label: 'SỐ BƯỚC CHÂN',
                  value: isActive ? '$_steps' : 'N/A',
                  icon: Icons.directions_walk_rounded,
                  color: const Color(0xFF10B981),
                ),
              ),
              const SizedBox(width: 16),
              Expanded(
                child: _buildSingleStatItem(
                  label: 'QUÃNG ĐƯỜNG',
                  value: isActive
                      ? '${_distance.toStringAsFixed(2)} km'
                      : 'N/A',
                  icon: Icons.social_distance_rounded,
                  color: const Color(0xFF3B82F6),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: _buildSingleStatItem(
                  label: 'VẬN TỐC',
                  value: isActive ? '${_speed.toStringAsFixed(1)} km/h' : 'N/A',
                  icon: Icons.speed_rounded,
                  color: Colors.orangeAccent,
                ),
              ),
              const SizedBox(width: 16),
              Expanded(
                child: _buildSingleStatItem(
                  label: 'NĂNG LƯỢNG',
                  value: isActive
                      ? '$_calories kcal'
                      : 'N/A', // Tráº£ vá» N/A khi chÆ°a Ä‘o
                  icon: Icons.local_fire_department_rounded,
                  color: Colors.redAccent,
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          _buildSingleStatItem(
            label: 'THỜI GIAN LUYỆN TẬP',
            value: isActive
                ? _formatDuration(_seconds)
                : 'N/A', // Tráº£ vá»  N/A khi chÆ°a Ä‘o
            icon: Icons.timer_rounded,
            color: Colors.purpleAccent,
            fullWidth: true,
          ),
        ],
      ),
    );
  }

  Widget _buildSingleStatItem({
    required String label,
    required String value,
    required IconData icon,
    required Color color,
    bool fullWidth = false,
  }) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF1E293B).withValues(alpha: 0.15),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: Colors.white.withValues(alpha: 0.03)),
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: color.withValues(alpha: 0.1),
              shape: BoxShape.circle,
            ),
            child: Icon(icon, color: color, size: 18),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  label,
                  style: const TextStyle(
                    color: Colors.white30,
                    fontSize: 9,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  value,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildExpandedMapStat(String label, String value) {
    return Column(
      children: [
        Text(
          label,
          style: const TextStyle(
            color: Colors.white30,
            fontSize: 10,
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 6),
        Text(
          value,
          style: const TextStyle(
            color: Colors.white,
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }
}

// Struct Ä‘á»ƒ Ä‘Ã³ng gÃ³i phiÃªn dá»¯ liá»‡u Ä‘Ã£ lÆ°u
class WorkoutSession {
  final String id;
  final DateTime time;
  final int steps;
  final double distance;
  final double speed;
  final int calories;
  final int seconds;
  final List<LatLng> route;

  WorkoutSession({
    required this.id,
    required this.time,
    required this.steps,
    required this.distance,
    required this.speed,
    required this.calories,
    required this.seconds,
    required this.route,
  });
}
