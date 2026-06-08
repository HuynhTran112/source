import 'dart:async';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import '../ble_service.dart';
import '../user_profile_settings.dart';

class GpsTrackerScreen extends StatefulWidget {
  const GpsTrackerScreen({super.key});

  // Bá»™ Ä‘á»‡m tÄ©nh lÆ°u trá»¯ dá»¯ liá»‡u GPS Ä‘á»“ng bá»™ tá»« Smartwatch qua BLE
  static List<Offset> syncedCoordinates = [];
  static double totalDistanceKm = 0.0;
  static int latestDurationSec = 0;
  static int currentSessionId = 0;
  static String currentSportMode = 'none';
  static StreamController<void>? _trackUpdateController;
  static StreamController<void> get _trackUpdates {
    return _trackUpdateController ??= StreamController<void>.broadcast();
  }

  static Stream<void> get onTrackUpdate => _trackUpdates.stream;

  static void updateTrack(String rawData) {
    try {
      final points = rawData.split(';');
      List<Offset> newCoords = [];
      for (var pt in points) {
        if (pt.trim().isEmpty) continue;
        final latLon = pt.split(',');
        if (latLon.length == 2) {
          final lat = double.tryParse(latLon[0]) ?? 0.0;
          final lon = double.tryParse(latLon[1]) ?? 0.0;
          if (lat != 0.0 && lon != 0.0) {
            newCoords.add(Offset(lat, lon));
          }
        }
      }
      if (newCoords.isNotEmpty) {
        if (newCoords.length == 1 && syncedCoordinates.isNotEmpty) {
          final last = syncedCoordinates.last;
          if (last.dx != newCoords.first.dx || last.dy != newCoords.first.dy) {
            syncedCoordinates.add(newCoords.first);
          }
        } else {
          syncedCoordinates = newCoords;
        }
        // Giáº£ láº­p khoáº£ng cÃ¡ch ngáº«u nhiÃªn thá»±c táº¿ dá»±a trÃªn Ä‘iá»ƒm GPS
        totalDistanceKm = _estimateDistanceKm(syncedCoordinates);
        final controller = _trackUpdateController;
        if (controller != null && !controller.isClosed) {
          controller.add(null);
        }
      }
    } catch (e) {
      debugPrint('Error parsing GPS track: $e');
    }
  }

  static void updateLiveSession(int sessionId) {
    if (sessionId <= 0 || sessionId == currentSessionId) return;
    resetTrackData();
    currentSessionId = sessionId;
  }

  static void updateLiveSport(String sportMode) {
    if (sportMode.isNotEmpty) currentSportMode = sportMode;
  }

  static void updateActivityDuration(int durationSec) {
    if (durationSec <= 0) return;
    latestDurationSec = durationSec;
    final controller = _trackUpdateController;
    if (controller != null && !controller.isClosed) {
      controller.add(null);
    }
  }

  static void resetTrackData({bool preserveSession = false}) {
    final sessionId = currentSessionId;
    final sportMode = currentSportMode;
    syncedCoordinates.clear();
    totalDistanceKm = 0.0;
    latestDurationSec = 0;
    currentSportMode = preserveSession ? sportMode : 'none';
    currentSessionId = preserveSession ? sessionId : 0;
  }

  static void disposeTrackUpdates() {
    final controller = _trackUpdateController;
    _trackUpdateController = null;
    controller?.close();
  }

  static double _estimateDistanceKm(List<Offset> points) {
    if (points.length < 2) return 0.0;
    double total = 0.0;
    const earthKm = 6371.0;
    for (int i = 1; i < points.length; i++) {
      final a = points[i - 1];
      final b = points[i];
      final dLat = _degToRad(b.dx - a.dx);
      final dLon = _degToRad(b.dy - a.dy);
      final lat1 = _degToRad(a.dx);
      final lat2 = _degToRad(b.dx);
      final h =
          math.sin(dLat / 2) * math.sin(dLat / 2) +
          math.cos(lat1) *
              math.cos(lat2) *
              math.sin(dLon / 2) *
              math.sin(dLon / 2);
      total += earthKm * 2 * math.atan2(math.sqrt(h), math.sqrt(1 - h));
    }
    return total;
  }

  static double _degToRad(double deg) => deg * math.pi / 180.0;

  @override
  State<GpsTrackerScreen> createState() => _GpsTrackerScreenState();
}

class _GpsTrackerScreenState extends State<GpsTrackerScreen>
    with SingleTickerProviderStateMixin {
  late AnimationController _animController;
  List<Offset> _displayPoints = [];
  double _distanceSim = 0.0;
  int _secondsSim = 0;
  int _caloriesSim = 0;
  final Stopwatch _stopwatch = Stopwatch();
  Timer? _durationTimer;
  StreamSubscription? _updateSub;

  @override
  void initState() {
    super.initState();
    _animController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat(reverse: true);

    _displayPoints = List.from(GpsTrackerScreen.syncedCoordinates);
    _distanceSim = GpsTrackerScreen.totalDistanceKm;
    _secondsSim = GpsTrackerScreen.latestDurationSec;
    _caloriesSim = _calcCalories();
    if (_displayPoints.isNotEmpty && _secondsSim == 0) {
      _startLiveDuration();
    }
    unawaited(BleService().requestTrack());

    _updateSub = GpsTrackerScreen.onTrackUpdate.listen((_) {
      if (mounted) {
        setState(() {
          _displayPoints = List.from(GpsTrackerScreen.syncedCoordinates);
          _distanceSim = GpsTrackerScreen.totalDistanceKm;
          if (GpsTrackerScreen.latestDurationSec > 0) {
            _secondsSim = GpsTrackerScreen.latestDurationSec;
            _stopwatch.stop();
          } else if (_displayPoints.isNotEmpty) {
            _startLiveDuration();
            _secondsSim = _stopwatch.elapsed.inSeconds;
          }
          _caloriesSim = _calcCalories();
        });
      }
    });

    if (_displayPoints.isEmpty) {
      _displayPoints = [const Offset(10.7629, 106.6822)];
    }
  }

  @override
  void dispose() {
    _animController.dispose();
    _durationTimer?.cancel();
    _updateSub?.cancel();
    GpsTrackerScreen.disposeTrackUpdates();
    super.dispose();
  }

  void _startLiveDuration() {
    if (!_stopwatch.isRunning) _stopwatch.start();
    _durationTimer ??= Timer.periodic(const Duration(seconds: 1), (_) {
      if (!mounted ||
          !_stopwatch.isRunning ||
          GpsTrackerScreen.latestDurationSec > 0) {
        return;
      }
      setState(() {
        _secondsSim = _stopwatch.elapsed.inSeconds;
        _caloriesSim = _calcCalories();
      });
    });
  }

  int _calcCalories() {
    return UserProfileSettings.calcCalories(
      sportMode: GpsTrackerScreen.currentSportMode,
      durationSec: _secondsSim,
    );
  }

  String _formatDuration(int seconds) {
    int m = seconds ~/ 60;
    int s = seconds % 60;
    return '${m.toString().padLeft(2, '0')}:${s.toString().padLeft(2, '0')}';
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0F172A),
      appBar: AppBar(
        backgroundColor: Colors.transparent,
        elevation: 0,
        leading: IconButton(
          icon: const Icon(
            Icons.arrow_back_ios_new_rounded,
            color: Colors.white,
          ),
          onPressed: () => Navigator.pop(context),
        ),
        title: const Text(
          'LỘ TRÌNH GPS',
          style: TextStyle(
            color: Colors.white,
            fontSize: 16,
            fontWeight: FontWeight.bold,
            letterSpacing: 1.5,
          ),
        ),
        centerTitle: true,
      ),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Color(0xFF0F172A), Color(0xFF020617)],
          ),
        ),
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Column(
            children: [
              // â”€â”€ Báº¢N Äá»’ VECTOR Lá»˜ TRÃŒNH PHÃT SÃNG â”€â”€
              Expanded(
                child: Container(
                  width: double.infinity,
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.02),
                    borderRadius: BorderRadius.circular(32),
                    border: Border.all(
                      color: Colors.white.withValues(alpha: 0.05),
                    ),
                  ),
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(32),
                    child: Stack(
                      children: [
                        // LÆ°á»›i tá»a Ä‘á»™ Radar
                        Positioned.fill(
                          child: CustomPaint(painter: RadarGridPainter()),
                        ),
                        Positioned.fill(
                          child: CustomPaint(
                            painter: RadarPulsePainter(
                              animationValue: _animController.value,
                            ),
                          ),
                        ),
                        // Váº½ lá»™ trÃ¬nh Vector
                        Positioned.fill(
                          child: Padding(
                            padding: const EdgeInsets.all(40.0),
                            child: CustomPaint(
                              painter: RoutePainter(points: _displayPoints),
                            ),
                          ),
                        ),
                        // Tháº» Tráº¡ng thÃ¡i GPS
                        Positioned(
                          top: 16,
                          left: 16,
                          child: Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 6,
                            ),
                            decoration: BoxDecoration(
                              color: const Color(
                                0xFF10B981,
                              ).withValues(alpha: 0.15),
                              borderRadius: BorderRadius.circular(100),
                              border: Border.all(
                                color: const Color(
                                  0xFF10B981,
                                ).withValues(alpha: 0.3),
                              ),
                            ),
                            child: const Row(
                              children: [
                                Icon(
                                  Icons.gps_fixed_rounded,
                                  color: Color(0xFF10B981),
                                  size: 14,
                                ),
                                SizedBox(width: 6),
                                Text(
                                  'GPS HOẠT ĐỘNG',
                                  style: TextStyle(
                                    color: Color(0xFF10B981),
                                    fontSize: 10,
                                    fontWeight: FontWeight.bold,
                                  ),
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
              const SizedBox(height: 20),

              // â”€â”€ DASHBOARD CHá»ˆ Sá» THá»‚ THAO Äá»’NG Bá»˜ â”€â”€
              Container(
                padding: const EdgeInsets.all(24),
                decoration: BoxDecoration(
                  color: Colors.white.withValues(alpha: 0.02),
                  borderRadius: BorderRadius.circular(28),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.05),
                  ),
                ),
                child: Column(
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        _buildStatItem(
                          label: 'QUÃNG ĐƯỜNG',
                          value: '${_distanceSim.toStringAsFixed(2)} km',
                          color: const Color(0xFF3B82F6),
                        ),
                        _buildStatItem(
                          label: 'THỜI GIAN',
                          value: _formatDuration(_secondsSim),
                          color: Colors.purpleAccent,
                        ),
                        _buildStatItem(
                          label: 'NĂNG LƯỢNG',
                          value: '$_caloriesSim kcal',
                          color: Colors.orangeAccent,
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildStatItem({
    required String label,
    required String value,
    required Color color,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label,
          style: const TextStyle(
            color: Colors.white38,
            fontSize: 10,
            fontWeight: FontWeight.bold,
            letterSpacing: 1,
          ),
        ),
        const SizedBox(height: 6),
        Text(
          value,
          style: TextStyle(
            color: color,
            fontSize: 20,
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }
}

class RadarGridPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.white.withValues(alpha: 0.03)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;

    double step = 30.0;
    for (double i = 0; i < size.width; i += step) {
      canvas.drawLine(Offset(i, 0), Offset(i, size.height), paint);
    }
    for (double i = 0; i < size.height; i += step) {
      canvas.drawLine(Offset(0, i), Offset(size.width, i), paint);
    }
  }

  @override
  bool shouldRepaint(covariant RadarGridPainter oldDelegate) => false;
}

class RadarPulsePainter extends CustomPainter {
  final double animationValue;
  RadarPulsePainter({required this.animationValue});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final circlePaint = Paint()
      ..color = const Color(
        0xFF3B82F6,
      ).withValues(alpha: 0.04 * (1.0 - animationValue))
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;

    canvas.drawCircle(center, 50 * (1 + animationValue), circlePaint);
    canvas.drawCircle(center, 100 * (1 + animationValue), circlePaint);
  }

  @override
  bool shouldRepaint(covariant RadarPulsePainter oldDelegate) =>
      oldDelegate.animationValue != animationValue;
}

class RoutePainter extends CustomPainter {
  final List<Offset> points;
  RoutePainter({required this.points});

  @override
  void paint(Canvas canvas, Size size) {
    if (points.isEmpty) return;

    double minX = points[0].dx;
    double maxX = points[0].dx;
    double minY = points[0].dy;
    double maxY = points[0].dy;

    for (var p in points) {
      if (p.dx < minX) minX = p.dx;
      if (p.dx > maxX) maxX = p.dx;
      if (p.dy < minY) minY = p.dy;
      if (p.dy > maxY) maxY = p.dy;
    }

    double rangeX = maxX - minX;
    double rangeY = maxY - minY;
    if (rangeX == 0) rangeX = 0.001;
    if (rangeY == 0) rangeY = 0.001;

    List<Offset> canvasPoints = [];
    for (var p in points) {
      double x = ((p.dx - minX) / rangeX) * size.width;
      double y = size.height - (((p.dy - minY) / rangeY) * size.height);
      canvasPoints.add(Offset(x, y));
    }

    final glowPaint = Paint()
      ..color = const Color(0xFF10B981).withValues(alpha: 0.3)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 8.0
      ..strokeCap = StrokeCap.round
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 5);

    final path = Path();
    path.moveTo(canvasPoints[0].dx, canvasPoints[0].dy);
    for (int i = 1; i < canvasPoints.length; i++) {
      path.lineTo(canvasPoints[i].dx, canvasPoints[i].dy);
    }
    canvas.drawPath(path, glowPaint);

    final pathPaint = Paint()
      ..color = const Color(0xFF10B981)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3.0
      ..strokeCap = StrokeCap.round;
    canvas.drawPath(path, pathPaint);

    final startPaint = Paint()
      ..color = const Color(0xFF3B82F6)
      ..style = PaintingStyle.fill;
    canvas.drawCircle(canvasPoints.first, 8, startPaint);

    final startInnerPaint = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    canvas.drawCircle(canvasPoints.first, 3, startInnerPaint);

    if (canvasPoints.length > 1) {
      final endPaint = Paint()
        ..color = const Color(0xFFEF4444)
        ..style = PaintingStyle.fill;
      canvas.drawCircle(canvasPoints.last, 8, endPaint);

      final endInnerPaint = Paint()
        ..color = Colors.white
        ..style = PaintingStyle.fill;
      canvas.drawCircle(canvasPoints.last, 3, endInnerPaint);
    }
  }

  @override
  bool shouldRepaint(covariant RoutePainter oldDelegate) =>
      oldDelegate.points != points;
}
