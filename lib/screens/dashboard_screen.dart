import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../ble_service.dart';
import '../user_profile_settings.dart';

class DashboardScreen extends StatefulWidget {
  final VoidCallback onConnectPressed;
  const DashboardScreen({super.key, required this.onConnectPressed});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  final BleService _bleService = BleService();
  DeviceConnectionState _connectionState = DeviceConnectionState.disconnected;

  int _battery = -1;
  StreamSubscription<WatchTelemetry>? _telemetrySub;

  @override
  void initState() {
    super.initState();

    // Ä á»“ng bá»™ tráº¡ng thÃ¡i káº¿t ná»‘i BLE
    _connectionState = _bleService.connectionState;
    _bleService.statusStream.listen((state) {
      if (!mounted) return;
      setState(() {
        _connectionState = state;
      });
    });

    _telemetrySub = _bleService.telemetryStream.listen((tel) {
      if (!mounted) return;
      setState(() {
        if (tel.batteryPercent >= 0) _battery = tel.batteryPercent;
      });
    });
  }

  @override
  void dispose() {
    _telemetrySub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    bool isConnected = _connectionState == DeviceConnectionState.connected;

    return Scaffold(
      backgroundColor: const Color(0xFF0F172A), // Slate 900
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              Color(0xFF0F172A),
              Color(0xFF1E1B4B), // Indigo 950
              Color(0xFF020617),
            ],
          ),
        ),
        child: SafeArea(
          child: CustomScrollView(
            slivers: [
              SliverFillRemaining(
                hasScrollBody: false,
                child: Padding(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 20,
                    vertical: 15,
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      // ── HEADER ──
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              const Text(
                                'XIN CHÀO 👋',
                                style: TextStyle(
                                  color: Colors.white38,
                                  fontSize: 12,
                                  fontWeight: FontWeight.bold,
                                  letterSpacing: 1.5,
                                ),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                isConnected
                                    ? 'Đã kết nối Watch'
                                    : 'Sẵn sàng kết nối',
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 22,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                            ],
                          ),
                          _buildConnectionBadge(isConnected),
                        ],
                      ),
                      const SizedBox(height: 24),

                      // ── GLASS CARD KẾT NỐI CHÍNH ──
                      Expanded(child: _buildMainConnectionCard(isConnected)),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  // CONNECTION STATUS BADGE
  Widget _buildConnectionBadge(bool isConnected) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: isConnected
            ? const Color(0xFF10B981).withValues(alpha: 0.15)
            : Colors.white.withValues(alpha: 0.05),
        borderRadius: BorderRadius.circular(100),
        border: Border.all(
          color: isConnected
              ? const Color(0xFF10B981).withValues(alpha: 0.3)
              : Colors.white.withValues(alpha: 0.1),
        ),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              color: isConnected ? const Color(0xFF10B981) : Colors.white30,
              shape: BoxShape.circle,
              boxShadow: isConnected
                  ? [
                      const BoxShadow(
                        color: Color(0xFF10B981),
                        blurRadius: 6,
                        spreadRadius: 2,
                      ),
                    ]
                  : [],
            ),
          ),
          const SizedBox(width: 8),
          Text(
            isConnected ? 'Connected' : 'Offline',
            style: TextStyle(
              color: isConnected ? const Color(0xFF10B981) : Colors.white60,
              fontSize: 12,
              fontWeight: FontWeight.bold,
            ),
          ),
        ],
      ),
    );
  }

  // MAIN CONNECTION CARD
  Widget _buildMainConnectionCard(bool isConnected) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 32),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.02),
        borderRadius: BorderRadius.circular(32),
        border: Border.all(color: Colors.white.withValues(alpha: 0.06)),
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            Colors.white.withValues(alpha: 0.04),
            Colors.white.withValues(alpha: 0.01),
          ],
        ),
      ),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Spacer(),
          // Nút nguồn tròn Neon rực rỡ như ảnh mẫu
          GestureDetector(
            onTap: isConnected
                ? _bleService.disconnect
                : widget.onConnectPressed,
            child: Center(
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 500),
                width: 140,
                height: 140,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: isConnected
                      ? const Color(0xFF0F172A)
                      : const Color(0xFF020617),
                  border: Border.all(
                    color: isConnected
                        ? const Color(0xFF3B82F6)
                        : const Color(0xFF334155),
                    width: 4,
                  ),
                  boxShadow: isConnected
                      ? [
                          BoxShadow(
                            color: const Color(
                              0xFF3B82F6,
                            ).withValues(alpha: 0.8),
                            blurRadius: 30,
                            spreadRadius: 2,
                          ),
                          BoxShadow(
                            color: const Color(
                              0xFF3B82F6,
                            ).withValues(alpha: 0.4),
                            blurRadius: 10,
                            spreadRadius: 1,
                          ),
                        ]
                      : [
                          BoxShadow(
                            color: Colors.black.withValues(alpha: 0.6),
                            blurRadius: 12,
                            spreadRadius: 1,
                          ),
                        ],
                ),
                child: Center(
                  child: Icon(
                    Icons.power_settings_new_rounded,
                    size: 68,
                    color: isConnected
                        ? const Color(0xFF60A5FA)
                        : const Color(0xFF475569),
                    shadows: isConnected
                        ? [
                            const Shadow(
                              color: Color(0xFF3B82F6),
                              blurRadius: 20,
                            ),
                          ]
                        : [],
                  ),
                ),
              ),
            ),
          ),
          const Spacer(),

          // Tên thiết bị và trạng thái kết nối
          Text(
            isConnected
                ? (_bleService.connectedDeviceName ?? 'ESP32 SmartWatch')
                : 'Đồng hồ chưa kết nối',
            style: const TextStyle(
              color: Colors.white,
              fontSize: 19,
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),
          Text(
            isConnected
                ? 'Đang đồng bộ dữ liệu thời gian thực'
                : 'Chạm biểu tượng nguồn để kết nối thiết bị',
            textAlign: TextAlign.center,
            style: TextStyle(
              color: Colors.white.withValues(alpha: 0.4),
              fontSize: 12.5,
              height: 1.4,
            ),
          ),
          const Spacer(),

          ValueListenableBuilder<double>(
            valueListenable: UserProfileSettings.weightKg,
            builder: (context, weightKg, _) {
              return OutlinedButton.icon(
                onPressed: () => _showWeightDialog(weightKg),
                icon: const Icon(Icons.monitor_weight_rounded),
                label: Text(
                  'Cân nặng: ${weightKg.toStringAsFixed(1)} kg',
                  style: const TextStyle(fontWeight: FontWeight.bold),
                ),
                style: OutlinedButton.styleFrom(
                  foregroundColor: Colors.orangeAccent,
                  side: const BorderSide(color: Colors.orangeAccent),
                  minimumSize: const Size(double.infinity, 46),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(14),
                  ),
                ),
              );
            },
          ),
          const SizedBox(height: 12),

          if (isConnected) ...[
            // Hiển thị thông số Pin và BLE
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                _buildWatchDetailItem(
                  icon: Icons.battery_charging_full_rounded,
                  value: '$_battery%',
                  label: 'Dung lượng Pin',
                  color: Colors.greenAccent.shade400,
                ),
                _buildWatchDetailItem(
                  icon: Icons.bluetooth_connected_rounded,
                  value: 'Ổn định',
                  label: 'Trạng thái',
                  color: Colors.blueAccent.shade400,
                ),
                _buildWatchDetailItem(
                  icon: Icons.sync_rounded,
                  value: 'Realtime',
                  label: 'Dữ liệu',
                  color: Colors.purpleAccent.shade400,
                ),
              ],
            ),
            const Spacer(),

            // Nút đồng bộ thời gian (Sync Time)
            ElevatedButton.icon(
              onPressed: () async {
                await _bleService.syncTime();
                if (mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Row(
                        children: [
                          Icon(Icons.check_circle_rounded, color: Colors.white),
                          SizedBox(width: 8),
                          Text("Đồng bộ thời gian thành công!"),
                        ],
                      ),
                      backgroundColor: Color(0xFF10B981),
                    ),
                  );
                }
              },
              icon: const Icon(Icons.sync_rounded),
              label: const Text(
                'Sync Time (Đồng bộ giờ)',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF3B82F6),
                foregroundColor: Colors.white,
                minimumSize: const Size(double.infinity, 50),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16),
                ),
                elevation: 4,
                shadowColor: const Color(0xFF3B82F6).withValues(alpha: 0.4),
              ),
            ),
            const SizedBox(height: 12),

            // Nút ngắt kết nối tinh tế
            OutlinedButton(
              onPressed: _bleService.disconnect,
              style: OutlinedButton.styleFrom(
                foregroundColor: Colors.redAccent,
                side: const BorderSide(color: Colors.redAccent, width: 1.2),
                minimumSize: const Size(double.infinity, 46),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(14),
                ),
              ),
              child: const Text(
                "Ngắt kết nối thiết bị",
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
          ] else ...[
            // Nút kết nối dự phòng
            ElevatedButton.icon(
              onPressed: widget.onConnectPressed,
              icon: const Icon(Icons.search_rounded),
              label: const Text(
                'Tìm kiếm SmartWatch',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1E293B),
                foregroundColor: Colors.white70,
                minimumSize: const Size(double.infinity, 52),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16),
                ),
                side: BorderSide(
                  color: Colors.white.withValues(alpha: 0.08),
                  width: 1,
                ),
              ),
            ),
          ],
          const Spacer(),
        ],
      ),
    );
  }

  Future<void> _showWeightDialog(double currentWeight) async {
    final controller = TextEditingController(
      text: currentWeight.toStringAsFixed(1),
    );
    final result = await showDialog<double>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF0F172A),
        title: const Text('Cân nặng'),
        content: TextField(
          controller: controller,
          keyboardType: const TextInputType.numberWithOptions(decimal: true),
          autofocus: true,
          decoration: const InputDecoration(
            suffixText: 'kg',
            hintText: 'Nhập số cân nặng',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Hủy'),
          ),
          FilledButton(
            onPressed: () {
              final value = double.tryParse(
                controller.text.trim().replaceAll(',', '.'),
              );
              Navigator.pop(context, value);
            },
            child: const Text('Lưu'),
          ),
        ],
      ),
    );
    controller.dispose();

    if (result == null) return;
    await UserProfileSettings.setWeightKg(result);
  }

  // WATCH DETAIL SUB-ITEM
  Widget _buildWatchDetailItem({
    required IconData icon,
    required String value,
    required String label,
    required Color color,
  }) {
    return Column(
      children: [
        Icon(icon, color: color, size: 22),
        const SizedBox(height: 8),
        Text(
          value,
          style: const TextStyle(
            color: Colors.white,
            fontSize: 15,
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 2),
        Text(
          label,
          style: const TextStyle(color: Colors.white38, fontSize: 10),
        ),
      ],
    );
  }
}
