import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../ble_service.dart';

class HealthScreen extends StatefulWidget {
  const HealthScreen({super.key});

  @override
  State<HealthScreen> createState() => _HealthScreenState();
}

class _HealthScreenState extends State<HealthScreen>
    with TickerProviderStateMixin {
  final BleService _bleService = BleService();
  DeviceConnectionState _connectionState = DeviceConnectionState.disconnected;

  bool _hasAcceptedAlert = false;

  // Danh sách lịch sử lâu dài (ngày/tuần)
  final List<HeartRateRecord> _hrHistory = [];
  final List<SpO2Record> _spo2History = [];

  int _currentHeartRate = 0;
  double _currentSpO2 = 0.0;
  StreamSubscription<WatchTelemetry>? _telemetrySub;
  late AnimationController _chartAnimationController;

  // Quản lý hết hạn chỉ số đo sau 1 giờ không nhận được dữ liệu
  DateTime? _lastUpdateTime;
  Timer? _timeoutTimer;

  // Điểm đo đang được tương tác (chạm/vuốt) trên đồ thị
  int? _selectedHrIndex;
  int? _selectedSpo2Index;

  @override
  void initState() {
    super.initState();

    _chartAnimationController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..forward();

    _connectionState = _bleService.connectionState;
    _bleService.statusStream.listen((state) {
      if (!mounted) return;
      setState(() => _connectionState = state);
    });

    _telemetrySub = _bleService.telemetryStream.listen((tel) {
      if (!mounted || tel.heartRate <= 0 || tel.spo2 <= 0) return;
      setState(() {
        _currentHeartRate = tel.heartRate.round();
        _currentSpO2 = tel.spo2;
        _lastUpdateTime =
            DateTime.now(); // Ghi nhận thời điểm nhận dữ liệu cuối
        _hasAcceptedAlert =
            true; // Nhận telemetry => Kích hoạt hiển thị tự động

        final now = DateTime.now();
        _hrHistory.add(HeartRateRecord(now, _currentHeartRate));
        _spo2History.add(SpO2Record(now, _currentSpO2));

        // Chỉ dọn dẹp các bản ghi quá khứ cũ hơn 30 ngày (giữ lịch sử nhiều tuần)
        _hrHistory.removeWhere((r) => now.difference(r.time).inDays >= 30);
        _spo2History.removeWhere((r) => now.difference(r.time).inDays >= 30);
      });
    });

    // Quét định kỳ mỗi 15 giây để kiểm tra hết hạn dữ liệu (1 giờ không nhận gói mới)
    _timeoutTimer = Timer.periodic(const Duration(seconds: 15), (timer) {
      if (!mounted) return;
      if (_lastUpdateTime != null && _hasAcceptedAlert) {
        final diff = DateTime.now().difference(_lastUpdateTime!);
        if (diff.inHours >= 1) {
          setState(() {
            _currentHeartRate = 0;
            _currentSpO2 = 0.0;
            _hasAcceptedAlert = false; // Revert lại cảnh báo và hiển thị N/A
          });
        }
      }
    });

    // Hiển thị Popup cảnh báo kích hoạt tính năng sức khỏe sau khi frame đầu tiên được vẽ
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _showHealthFeatureAlert();
    });
  }

  @override
  void dispose() {
    _timeoutTimer?.cancel();
    _chartAnimationController.dispose();
    _telemetrySub?.cancel();
    super.dispose();
  }

  void _showHealthFeatureAlert() {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF0F172A),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(24),
          side: const BorderSide(color: Color(0xFFEF4444), width: 1.5),
        ),
        title: const Row(
          children: [
            Icon(
              Icons.favorite_rounded,
              color: Color(0xFFEF4444),
              size: 28,
            ),
            SizedBox(width: 10),
            Text(
              'ĐO SỨC KHỎE',
              style: TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.5,
              ),
            ),
          ],
        ),
        content: const Text(
          'Hãy mở màn hình đo sức khỏe trên đồng hồ để bắt đầu đồng bộ dữ liệu.',
          style: TextStyle(color: Colors.white70, fontSize: 14, height: 1.4),
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              setState(() {
                _hasAcceptedAlert = true;
              });
            },
            style: TextButton.styleFrom(
              backgroundColor: const Color(0xFFEF4444),
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

  @override
  Widget build(BuildContext context) {
    final isConnected = _connectionState == DeviceConnectionState.connected;
    // Äang cÃ³ dá»¯ liá»‡u phÃ¡t sÃ¡ng khi: ÄÃ£ káº¿t ná»‘i VÃ€ Ä‘Ã£ báº­t tÃ­nh nÄƒng (Ä‘Ã£ áº¥n ÄÃ£ hiá»ƒu)
    final isHealthActive = isConnected && _hasAcceptedAlert;

    return DefaultTabController(
      length: 2,
      child: Scaffold(
        backgroundColor: const Color(0xFF0F172A),
        appBar: AppBar(
          backgroundColor: const Color(0xFF0F172A),
          elevation: 0,
          title: const Text(
            'SỨC KHỎE',
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
                  color: const Color(0xFF3B82F6),
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
                  Tab(text: 'Chỉ số đo được'),
                  Tab(text: 'Lịch sử'),
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
          child: TabBarView(
            physics:
                const NeverScrollableScrollPhysics(), // Táº¯t vuá»‘t ngang Ä‘á»ƒ trÃ¡nh cháº¡m Ä‘á»™t ngá»™t
            children: [_buildStatsTab(isHealthActive), _buildHistoryTab()],
          ),
        ),
      ),
    );
  }

  // TAB 1: CHá»ˆ Sá» ÄO ÄÆ¯á»¢C
  Widget _buildStatsTab(bool isActive) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(20.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Banner cáº£nh bÃ¡o náº¿u chÆ°a káº¿t ná»‘i hoáº·c chÆ°a báº­t tÃ­nh nÄƒng
          if (!isActive)
            Container(
              margin: const EdgeInsets.only(bottom: 20),
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.redAccent.withValues(alpha: 0.08),
                borderRadius: BorderRadius.circular(18),
                border: Border.all(
                  color: Colors.redAccent.withValues(alpha: 0.2),
                ),
              ),
              child: Row(
                children: [
                  const Icon(
                    Icons.info_outline_rounded,
                    color: Colors.redAccent,
                    size: 20,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      _connectionState == DeviceConnectionState.connected
                          ? 'Vui lòng kích hoạt tính năng sức khỏe trên đồng hồ.'
                          : 'Đồng hồ chưa kết nối. Vui lòng kết nối ở màn hình chính.',
                      style: const TextStyle(
                        color: Colors.redAccent,
                        fontSize: 12,
                        height: 1.3,
                      ),
                    ),
                  ),
                ],
              ),
            ),

          const Text(
            'CHỈ SỐ THỜI GIAN THỰC',
            style: TextStyle(
              color: Colors.white38,
              fontSize: 11,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.5,
            ),
          ),
          const SizedBox(height: 12),

          // Card Nhịp tim
          _buildHealthIndicatorCard(
            title: 'NHỊP TIM',
            value: isActive ? '$_currentHeartRate' : '--',
            unit: 'bpm',
            description:
                'Nhịp tim thời gian thực đo qua cảm biến.',
            icon: Icons.favorite_rounded,
            color: const Color(0xFFEF4444),
            isActive: isActive,
          ),
          const SizedBox(height: 16),

          // Card SpO2
          _buildHealthIndicatorCard(
            title: 'NỒNG ĐỘ OXY MÁU (SPO2)',
            value: isActive ? _currentSpO2.toStringAsFixed(1) : '--',
            unit: '%',
            description:
                'Độ bão hòa oxy trong máu thời gian thực.',
            icon: Icons.opacity_rounded,
            color: Colors.cyanAccent.shade400,
            isActive: isActive,
          ),
          const SizedBox(height: 24),

          Container(
            padding: const EdgeInsets.all(20),
            decoration: BoxDecoration(
              color: Colors.white.withValues(alpha: 0.01),
              borderRadius: BorderRadius.circular(24),
              border: Border.all(color: Colors.white.withValues(alpha: 0.04)),
            ),
            child: const Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  '💡 HƯỚNG DẪN ĐO',
                  style: TextStyle(
                    color: Colors.white70,
                    fontSize: 13,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                SizedBox(height: 10),
                Text(
                  '• Đeo đồng hồ vừa vặn trên cổ tay.\n'
                  '• Mở ứng dụng Đo sức khỏe trên đồng hồ.\n'
                  '• Giữ yên tay trong quá trình đo (khoảng 15s).\n'
                  '• Dữ liệu được tự động đồng bộ sang điện thoại.',
                  style: TextStyle(
                    color: Colors.white30,
                    fontSize: 11.5,
                    height: 1.6,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  // Táº O THáºº CHá»ˆ Sá» Sá»¨C KHá»ŽE (CÃ“ KHáº¢ NÄ‚NG GRAY OUT - MÃ€U ÄEN Má»œ)
  Widget _buildHealthIndicatorCard({
    required String title,
    required String value,
    required String unit,
    required String description,
    required IconData icon,
    required Color color,
    required bool isActive,
  }) {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 400),
      padding: const EdgeInsets.all(22),
      decoration: BoxDecoration(
        color: isActive
            ? color.withValues(alpha: 0.03)
            : const Color(
                0xFF1E293B,
              ).withValues(alpha: 0.2), // MÃ u Ä‘en má» khi inactive
        borderRadius: BorderRadius.circular(28),
        border: Border.all(
          color: isActive
              ? color.withValues(alpha: 0.25)
              : Colors.white.withValues(
                  alpha: 0.04,
                ), // Viá»n má» khi inactive
          width: 1.5,
        ),
        boxShadow: isActive
            ? [
                BoxShadow(
                  color: color.withValues(alpha: 0.05),
                  blurRadius: 15,
                  spreadRadius: 1,
                ),
              ]
            : [],
      ),
      child: Row(
        children: [
          // Icon phÃ¡t sÃ¡ng
          AnimatedContainer(
            duration: const Duration(milliseconds: 400),
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              color: isActive
                  ? color.withValues(alpha: 0.12)
                  : const Color(0xFF0F172A), // Ná»n tá»‘i khi inactive
              shape: BoxShape.circle,
            ),
            child: Icon(
              icon,
              color: isActive
                  ? color
                  : Colors.white24, // Icon má» Ä‘en khi inactive
              size: 28,
            ),
          ),
          const SizedBox(width: 20),

          // ThÃ´ng tin chá»‰ sá»‘
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: TextStyle(
                    color: isActive ? Colors.white60 : Colors.white30,
                    fontSize: 11,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1,
                  ),
                ),
                const SizedBox(height: 6),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.baseline,
                  textBaseline: TextBaseline.alphabetic,
                  children: [
                    Text(
                      value,
                      style: TextStyle(
                        color: isActive
                            ? Colors.white
                            : Colors.white12, // Sá»‘ má» Ä‘en khi inactive
                        fontSize: 34,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(width: 4),
                    Text(
                      unit,
                      style: TextStyle(
                        color: isActive ? color : Colors.white12,
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 6),
                Text(
                  description,
                  style: TextStyle(
                    color: isActive ? Colors.white38 : Colors.white24,
                    fontSize: 10.5,
                    height: 1.3,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  // TAB 2: LỊCH SỬ ĐỒ THỊ ĐƯỜNG CONG TÁCH BIỆT (CÓ HỖ TRỢ TƯƠNG TÁC)
  Widget _buildHistoryTab() {
    double avgHr = 75.0;
    if (_hrHistory.isNotEmpty) {
      avgHr =
          _hrHistory.map((e) => e.bpm).reduce((a, b) => a + b) /
          _hrHistory.length;
    }
    double avgSpo2 = 98.5;
    if (_spo2History.isNotEmpty) {
      avgSpo2 =
          _spo2History.map((e) => e.spo2).reduce((a, b) => a + b) /
          _spo2History.length;
    }

    return SingleChildScrollView(
      padding: const EdgeInsets.all(20.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── ĐỒ THỊ NHỊP TIM ──
          _buildSingleChartContainer(
            title: 'Lịch sử Nhịp tim (bpm)',
            icon: Icons.favorite_rounded,
            color: const Color(0xFFEF4444),
            values: _hrHistory.map((d) => d.bpm.toDouble()).toList(),
            dates: _hrHistory.map((d) => d.time).toList(),
            minScale: 60,
            maxScale: 120,
            averageValue: '${avgHr.toStringAsFixed(1)} bpm',
            averageLabel: 'Nhịp tim TB',
            selectedIndex: _selectedHrIndex,
            onIndexSelected: (idx) {
              setState(() {
                _selectedHrIndex = idx;
              });
            },
          ),
          const SizedBox(height: 20),

          // ── ĐỒ THỊ SPO2 ──
          _buildSingleChartContainer(
            title: 'Lịch sử Oxy máu SpO2 (%)',
            icon: Icons.opacity_rounded,
            color: Colors.cyanAccent.shade400,
            values: _spo2History.map((d) => d.spo2).toList(),
            dates: _spo2History.map((d) => d.time).toList(),
            minScale: 94,
            maxScale: 100,
            averageValue: '${avgSpo2.toStringAsFixed(1)} %',
            averageLabel: 'SpO2 TB',
            selectedIndex: _selectedSpo2Index,
            onIndexSelected: (idx) {
              setState(() {
                _selectedSpo2Index = idx;
              });
            },
          ),
          const SizedBox(height: 12),
        ],
      ),
    );
  }

  // WIDGET DỰNG CONTAINER BIỂU ĐỒ ĐƠN TIÊU CHUẨN PREMIUM (CÓ HỖ TRỢ TƯƠNG TÁC)
  Widget _buildSingleChartContainer({
    required String title,
    required IconData icon,
    required Color color,
    required List<double> values,
    required List<DateTime> dates,
    required double minScale,
    required double maxScale,
    required String averageValue,
    required String averageLabel,
    required int? selectedIndex,
    required ValueChanged<int?> onIndexSelected,
  }) {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.02),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Expanded(
                child: Row(
                  children: [
                    Icon(icon, color: color, size: 20),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(
                          color: Colors.white70,
                          fontSize: 14,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 8),
              Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 10,
                  vertical: 4,
                ),
                decoration: BoxDecoration(
                  color: color.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Text(
                  '$averageLabel: $averageValue',
                  style: TextStyle(
                    color: color,
                    fontSize: 11,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 24),

          // Vùng biểu đồ vẽ CustomPaint kèm GestureDetector bắt sự kiện & floating tooltip
          LayoutBuilder(
            builder: (context, constraints) {
              double chartWidth = constraints.maxWidth;
              double chartHeight = 150.0;

              double? tooltipLeft;
              double? tooltipTop;
              String? tooltipText;
              String? tooltipTime;

              if (selectedIndex != null &&
                  selectedIndex < values.length &&
                  values.isNotEmpty) {
                double spacingX = chartWidth / (values.length - 1);
                double scaleRange = maxScale - minScale;
                if (scaleRange <= 0) scaleRange = 1.0;

                double pct = (values[selectedIndex] - minScale) / scaleRange;
                double selectedY =
                    chartHeight - (pct.clamp(0.0, 1.0) * chartHeight);
                double selectedX = selectedIndex * spacingX;

                // Căn chỉnh tooltip ở giữa bên trên điểm được chọn
                tooltipLeft = (selectedX - 65).clamp(0.0, chartWidth - 130);
                tooltipTop = (selectedY - 65).clamp(0.0, chartHeight);

                tooltipText =
                    '${values[selectedIndex].toStringAsFixed(1)} ${averageLabel.contains("Tim") ? "bpm" : "%"}';

                if (selectedIndex < dates.length) {
                  final t = dates[selectedIndex];
                  tooltipTime =
                      '${t.day.toString().padLeft(2, '0')}/${t.month.toString().padLeft(2, '0')} ${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
                }
              }

              void handleGesture(Offset localPosition) {
                if (values.length < 2) return;
                double spacingX = chartWidth / (values.length - 1);
                int index = (localPosition.dx / spacingX).round().clamp(
                  0,
                  values.length - 1,
                );
                onIndexSelected(index);
              }

              return Stack(
                clipBehavior: Clip.none,
                children: [
                  GestureDetector(
                    onTapDown: (details) =>
                        handleGesture(details.localPosition),
                    onPanUpdate: (details) =>
                        handleGesture(details.localPosition),
                    onPanDown: (details) =>
                        handleGesture(details.localPosition),
                    onTapUp: (_) => onIndexSelected(null),
                    onPanEnd: (_) => onIndexSelected(null),
                    onPanCancel: () => onIndexSelected(null),
                    child: Container(
                      height: chartHeight,
                      width: double.infinity,
                      color: Colors.transparent, // Vùng hứng sự kiện chạm vuốt
                      child: AnimatedBuilder(
                        animation: _chartAnimationController,
                        builder: (context, child) {
                          return CustomPaint(
                            painter: SingleLineHistoryPainter(
                              values: values,
                              color: color,
                              minScale: minScale,
                              maxScale: maxScale,
                              animValue: _chartAnimationController.value,
                              selectedIndex: selectedIndex,
                            ),
                          );
                        },
                      ),
                    ),
                  ),

                  // Floating Glass Tooltip
                  if (tooltipText != null && tooltipTime != null)
                    Positioned(
                      left: tooltipLeft,
                      top: tooltipTop,
                      child: IgnorePointer(
                        child: Container(
                          padding: const EdgeInsets.symmetric(
                            horizontal: 12,
                            vertical: 6,
                          ),
                          decoration: BoxDecoration(
                            color: const Color(
                              0xFF0F172A,
                            ).withValues(alpha: 0.95), // Slate 900
                            borderRadius: BorderRadius.circular(12),
                            border: Border.all(
                              color: color.withValues(alpha: 0.3),
                              width: 1.2,
                            ),
                            boxShadow: [
                              BoxShadow(
                                color: color.withValues(alpha: 0.15),
                                blurRadius: 10,
                                spreadRadius: 1,
                              ),
                            ],
                          ),
                          child: Column(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              Text(
                                tooltipText,
                                style: TextStyle(
                                  color: color,
                                  fontSize: 13,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                              const SizedBox(height: 3),
                              Text(
                                tooltipTime,
                                style: const TextStyle(
                                  color: Colors.white38,
                                  fontSize: 9,
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                ],
              );
            },
          ),
          const SizedBox(height: 12),

          // Trục thời gian hoành: Ngày đầu tiên (Quá khứ) -> Ngày cuối cùng (Bây giờ)
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                dates.isNotEmpty
                    ? '${dates.first.day.toString().padLeft(2, '0')}/${dates.first.month.toString().padLeft(2, '0')}'
                    : 'Lịch sử',
                style: const TextStyle(color: Colors.white24, fontSize: 10),
              ),
              Text(
                dates.isNotEmpty
                    ? '${dates.last.day.toString().padLeft(2, '0')}/${dates.last.month.toString().padLeft(2, '0')}'
                    : 'Bây giờ',
                style: const TextStyle(
                  color: Colors.white30,
                  fontSize: 10,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

// Struct lưu trữ record
class HeartRateRecord {
  final DateTime time;
  final int bpm;
  HeartRateRecord(this.time, this.bpm);
}

class SpO2Record {
  final DateTime time;
  final double spo2;
  SpO2Record(this.time, this.spo2);
}

// VẼ BIỂU ĐỒ 1 ĐƯỜNG CONG LIÊN TỤC LÊN TOÀN CHIỀU RỘNG GRAPH (CÓ HỖ TRỢ TƯƠNG TÁC CHẠM)
class SingleLineHistoryPainter extends CustomPainter {
  final List<double> values;
  final Color color;
  final double minScale;
  final double maxScale;
  final double animValue;
  final int? selectedIndex; // Nhận thêm điểm chạm/pan được chọn

  SingleLineHistoryPainter({
    required this.values,
    required this.color,
    required this.minScale,
    required this.maxScale,
    required this.animValue,
    this.selectedIndex,
  });

  @override
  void paint(Canvas canvas, Size size) {
    if (values.isEmpty) return;

    // 1. Vẽ lưới Radar & Trục ngang
    final gridPaint = Paint()
      ..color = Colors.white.withValues(alpha: 0.03)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;

    double gridLines = 4;
    double gridStepY = size.height / gridLines;
    for (int i = 0; i <= gridLines; i++) {
      double y = i * gridStepY;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }

    if (values.length < 2) {
      final y = size.height / 2;
      final linePaint = Paint()
        ..color = color.withValues(alpha: 0.3)
        ..strokeWidth = 2.0
        ..style = PaintingStyle.stroke;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), linePaint);
      return;
    }

    // 2. Cấu hình cọ vẽ
    final linePaint = Paint()
      ..color = color
      ..strokeWidth = 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    final shadowPaint = Paint()
      ..color = color.withValues(alpha: 0.15)
      ..strokeWidth = 6.0
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    final path = Path();
    final fillPath = Path();

    double spacingX = size.width / (values.length - 1);
    double scaleRange = maxScale - minScale;
    if (scaleRange <= 0) scaleRange = 1.0;

    double getY(double val) {
      double pct = (val - minScale) / scaleRange;
      pct = pct.clamp(0.0, 1.0);
      return size.height - (pct * size.height);
    }

    double firstY = getY(values[0]);
    path.moveTo(0, firstY);
    fillPath.moveTo(0, size.height);
    fillPath.lineTo(0, firstY);

    for (int i = 1; i < values.length; i++) {
      double x = i * spacingX * animValue;
      double y = getY(values[i]);

      double prevX = (i - 1) * spacingX * animValue;
      double prevY = getY(values[i - 1]);
      double cx = (prevX + x) / 2;

      path.cubicTo(cx, prevY, cx, y, x, y);
      fillPath.cubicTo(cx, prevY, cx, y, x, y);
    }

    // Kết thúc path gradient
    double lastX = (values.length - 1) * spacingX * animValue;
    fillPath.lineTo(lastX, size.height);
    fillPath.close();

    // 3. Tô màu Gradient dưới đường cong
    final fillPaint = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topCenter,
        end: Alignment.bottomCenter,
        colors: [color.withValues(alpha: 0.15), color.withValues(alpha: 0.0)],
      ).createShader(Rect.fromLTWH(0, 0, size.width, size.height))
      ..style = PaintingStyle.fill;

    canvas.drawPath(fillPath, fillPaint);

    // 4. Vẽ đường cong chính và Glow Neon
    canvas.drawPath(path, shadowPaint);
    canvas.drawPath(path, linePaint);

    // 5. Nếu đang chạm/pan ở một điểm đo, vẽ đường căn dọc đứt nét và zoom mốc tròn neon nổi lên
    if (selectedIndex != null && selectedIndex! < values.length) {
      double selectedX = selectedIndex! * spacingX * animValue;
      double selectedY = getY(values[selectedIndex!]);

      // Vẽ đường căn thẳng đứng nét đứt
      final dashPaint = Paint()
        ..color = Colors.white.withValues(alpha: 0.12)
        ..strokeWidth = 1.2
        ..style = PaintingStyle.stroke;

      double dashSpace = 4.0;
      double dashHeight = 6.0;
      double currentY = 0.0;
      while (currentY < size.height) {
        canvas.drawLine(
          Offset(selectedX, currentY),
          Offset(selectedX, (currentY + dashHeight).clamp(0, size.height)),
          dashPaint,
        );
        currentY += dashHeight + dashSpace;
      }

      // Vẽ vòng tròn phát sáng Neon to nổi bật bên ngoài (Zoom mốc mờ)
      final glowPaint = Paint()
        ..color = color.withValues(alpha: 0.3)
        ..style = PaintingStyle.fill;
      canvas.drawCircle(Offset(selectedX, selectedY), 10.0, glowPaint);

      // Vẽ vòng tròn chính neon nhỏ bên trong
      final mainDotPaint = Paint()
        ..color = color
        ..style = PaintingStyle.fill;
      canvas.drawCircle(Offset(selectedX, selectedY), 5.0, mainDotPaint);

      // Viền trắng tinh tế tạo cảm giác cao cấp 3D
      final borderPaint = Paint()
        ..color = Colors.white
        ..strokeWidth = 2.0
        ..style = PaintingStyle.stroke;
      canvas.drawCircle(Offset(selectedX, selectedY), 5.0, borderPaint);
    }
  }

  @override
  bool shouldRepaint(covariant SingleLineHistoryPainter oldDelegate) {
    return oldDelegate.values != values ||
        oldDelegate.color != color ||
        oldDelegate.minScale != minScale ||
        oldDelegate.maxScale != maxScale ||
        oldDelegate.animValue != animValue ||
        oldDelegate.selectedIndex != selectedIndex;
  }
}
