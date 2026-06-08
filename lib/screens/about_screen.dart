import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../ble_service.dart';

class AboutScreen extends StatefulWidget {
  const AboutScreen({super.key});

  @override
  State<AboutScreen> createState() => _AboutScreenState();
}

class _AboutScreenState extends State<AboutScreen> {
  final BleService _bleService = BleService();
  DeviceConnectionState _state = DeviceConnectionState.disconnected;
  WatchTelemetry? _telemetry;

  @override
  void initState() {
    super.initState();
    _state = _bleService.connectionState;
    _telemetry = _bleService.latestTelemetry;
    _bleService.statusStream.listen((state) {
      if (mounted) setState(() => _state = state);
    });
    _bleService.telemetryStream.listen((tel) {
      if (mounted) setState(() => _telemetry = tel);
    });
  }

  @override
  Widget build(BuildContext context) {
    final connected = _state == DeviceConnectionState.connected;
    return Scaffold(
      backgroundColor: const Color(0xFF0F172A),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(20),
          children: [
            const Text(
              'GIỚI THIỆU',
              style: TextStyle(
                color: Colors.white38,
                fontSize: 12,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 6),
            const Text(
              'ESP32-S3 SmartWatch',
              style: TextStyle(
                color: Colors.white,
                fontSize: 24,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 20),
            _infoCard(
              Icons.watch_rounded,
              'Thiết bị',
              _bleService.connectedDeviceName ?? 'ESP_WATCH',
            ),
            _infoCard(
              Icons.memory_rounded,
              'Phần cứng',
              'ESP32-S3 N8R2, ST7789 240x284, CST816S, GT-U8, MAX30102, LSM6DSOX, MAX17048G',
            ),
            _infoCard(
              Icons.bluetooth_connected_rounded,
              'BLE',
              connected ? 'Đã kết nối' : 'Chưa kết nối',
            ),
            _infoCard(
              Icons.battery_charging_full_rounded,
              'Pin',
              _telemetry?.batteryPercent != null &&
                      _telemetry!.batteryPercent >= 0
                  ? '${_telemetry!.batteryPercent}%'
                  : '--',
            ),
            _infoCard(
              Icons.route_rounded,
              'GPS',
              _telemetry?.gpsFix == true ? 'Đã có fix' : 'Chưa có fix',
            ),
            _infoCard(
              Icons.system_update_alt_rounded,
              'Firmware',
              'OTA qua WiFi, dữ liệu đồng bộ qua BLE',
            ),
          ],
        ),
      ),
    );
  }

  Widget _infoCard(IconData icon, String title, String value) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.03),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: Colors.white.withValues(alpha: 0.06)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: const Color(0xFF3B82F6)),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(color: Colors.white60, fontSize: 12),
                ),
                const SizedBox(height: 4),
                Text(
                  value,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 14,
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
}
