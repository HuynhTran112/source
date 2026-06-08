import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import '../ble_service.dart';

class OtaScreen extends StatefulWidget {
  const OtaScreen({super.key});

  @override
  State<OtaScreen> createState() => _OtaScreenState();
}

class _OtaScreenState extends State<OtaScreen> {
  final BleService _bleService = BleService();
  DeviceConnectionState _connectionState = DeviceConnectionState.disconnected;

  // Trạng thái cho OTA Update
  double _otaProgress = 0.0;
  bool _isUpdatingOta = false;
  String _otaStatusText = 'Chưa bắt đầu';

  String? _selectedFileName;
  int? _selectedFileSizeKB;

  void _showFilePickerDialog() {
    showModalBottomSheet(
      context: context,
      backgroundColor: const Color(0xFF0F172A),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      builder: (context) => Container(
        padding: const EdgeInsets.all(24),
        height: 330,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'CHỌN FILE FIRMWARE (.BIN)',
              style: TextStyle(
                color: Colors.white,
                fontSize: 15,
                fontWeight: FontWeight.bold,
                letterSpacing: 1,
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: ListView(
                children: [
                  _buildFileItem('smartwatch_v2.0.4.bin', 1240),
                  _buildFileItem('smartwatch_v2.0.5_beta.bin', 1256),
                  _buildFileItem('esp32_gps_st7789_v1.0.bin', 1320),
                  _buildFileItem('firmware_low_power_v1.2.bin', 1198),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildFileItem(String name, int sizeKB) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 6),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.02),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: Colors.white.withValues(alpha: 0.04)),
      ),
      child: ListTile(
        leading: const Icon(Icons.code_rounded, color: Colors.purpleAccent),
        title: Text(
          name,
          style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold, fontSize: 13),
        ),
        subtitle: Text(
          'Dung lượng: $sizeKB KB',
          style: const TextStyle(color: Colors.white30, fontSize: 11),
        ),
        trailing: const Icon(Icons.chevron_right_rounded, color: Colors.white30),
        onTap: () {
          setState(() {
            _selectedFileName = name;
            _selectedFileSizeKB = sizeKB;
          });
          Navigator.pop(context);
        },
      ),
    );
  }

  @override
  void initState() {
    super.initState();

    _connectionState = _bleService.connectionState;
    _bleService.statusStream.listen((state) {
      if (!mounted) return;
      setState(() => _connectionState = state);
    });
  }

  void _simulateOta() {
    if (_isUpdatingOta || _selectedFileName == null) return;
    setState(() {
      _isUpdatingOta = true;
      _otaProgress = 0.0;
      _otaStatusText = 'Đang khởi động kết nối...';
    });

    int step = 0;

    Timer.periodic(const Duration(milliseconds: 300), (timer) {
      if (!mounted) {
        timer.cancel();
        return;
      }
      setState(() {
        step++;
        if (step < 5) {
          _otaStatusText = 'Đang khởi chạy dịch vụ...';
        } else if (step < 12) {
          _otaStatusText = 'Đang kết nối Wi-Fi...';
        } else if (step < 42) {
          _otaProgress = (step - 12) / 30;
          _otaStatusText =
              'Đang truyền: ${(_otaProgress * 100).toStringAsFixed(0)}%';
        } else if (step < 48) {
          _otaProgress = 1.0;
          _otaStatusText = 'Đang cài đặt & Khởi động lại...';
        } else {
          _isUpdatingOta = false;
          _otaProgress = 0.0;
          _otaStatusText = 'Cập nhật thành công!';
          timer.cancel();
        }
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    final isConnected = _connectionState == DeviceConnectionState.connected;

    return Scaffold(
      backgroundColor: const Color(0xFF0F172A),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Color(0xFF0F172A), Color(0xFF0F172A), Color(0xFF020617)],
          ),
        ),
        child: SafeArea(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'FIRMWARE OTA',
                  style: TextStyle(
                    color: Colors.white38,
                    fontSize: 12,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1.5,
                  ),
                ),
                const SizedBox(height: 4),
                const Text(
                  'Cập nhật Phần mềm',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 22,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 24),

                // WI-FI OTA FIRMWARE CONTROLLER
                Container(
                  padding: const EdgeInsets.all(20),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.02),
                    borderRadius: BorderRadius.circular(24),
                    border: Border.all(
                      color: Colors.white.withValues(alpha: 0.05),
                    ),
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Row(
                        children: [
                          Icon(
                            Icons.wifi_tethering_rounded,
                            color: Colors.purpleAccent,
                            size: 24,
                          ),
                          SizedBox(width: 12),
                          Text(
                            'Cập nhật phần mềm (OTA)',
                            style: TextStyle(
                              color: Colors.white,
                              fontSize: 16,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 14),
                      const Text(
                        'Cập nhật phiên bản firmware mới nhất cho SmartWatch qua mạng Wi-Fi cục bộ.',
                        style: TextStyle(
                          color: Colors.white30,
                          fontSize: 12,
                          height: 1.4,
                        ),
                      ),
                      const SizedBox(height: 20),

                      // TRÌNH DUYỆT CHỌN FILE FIRMWARE
                      const Text(
                        'FILE FIRMWARE (.BIN) ĐÃ CHỌN:',
                        style: TextStyle(
                          color: Colors.white54,
                          fontSize: 10,
                          fontWeight: FontWeight.bold,
                          letterSpacing: 1,
                        ),
                      ),
                      const SizedBox(height: 8),
                      GestureDetector(
                        onTap: _isUpdatingOta ? null : _showFilePickerDialog,
                        child: Container(
                          width: double.infinity,
                          padding: const EdgeInsets.all(16),
                          decoration: BoxDecoration(
                            color: _selectedFileName != null
                                ? Colors.purpleAccent.withValues(alpha: 0.02)
                                : Colors.black.withValues(alpha: 0.2),
                            borderRadius: BorderRadius.circular(16),
                            border: Border.all(
                              color: _selectedFileName != null
                                  ? Colors.purpleAccent.withValues(alpha: 0.2)
                                  : Colors.white.withValues(alpha: 0.05),
                              width: 1,
                            ),
                          ),
                          child: _selectedFileName != null
                              ? Row(
                                  children: [
                                    const Icon(Icons.code_rounded, color: Colors.purpleAccent, size: 24),
                                    const SizedBox(width: 14),
                                    Expanded(
                                      child: Column(
                                        crossAxisAlignment: CrossAxisAlignment.start,
                                        children: [
                                          Text(
                                            _selectedFileName!,
                                            style: const TextStyle(
                                              color: Colors.white,
                                              fontWeight: FontWeight.bold,
                                              fontSize: 13,
                                            ),
                                          ),
                                          const SizedBox(height: 4),
                                          Text(
                                            'Kích thước: $_selectedFileSizeKB KB',
                                            style: const TextStyle(
                                              color: Colors.white38,
                                              fontSize: 11,
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                    const Icon(Icons.check_circle_rounded, color: Color(0xFF10B981), size: 20),
                                  ],
                                )
                              : const Row(
                                  mainAxisAlignment: MainAxisAlignment.center,
                                  children: [
                                    Icon(Icons.drive_folder_upload_rounded, color: Colors.white30, size: 22),
                                    SizedBox(width: 10),
                                    Flexible(
                                      child: Text(
                                        'Chọn file firmware (.bin)',
                                        style: TextStyle(color: Colors.white30, fontSize: 12),
                                        overflow: TextOverflow.ellipsis,
                                      ),
                                    ),
                                  ],
                                ),
                        ),
                      ),
                      const SizedBox(height: 20),

                      // Trạng thái hiện tại
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          const Text(
                            'Trạng thái OTA:',
                            style: TextStyle(
                              color: Colors.white54,
                              fontSize: 13,
                            ),
                          ),
                          Flexible(
                            child: Text(
                              _otaStatusText,
                              textAlign: TextAlign.right,
                              style: const TextStyle(
                                color: Colors.purpleAccent,
                                fontSize: 13,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),

                      // Thanh tiến trình tải OTA
                      if (_isUpdatingOta) ...[
                        Stack(
                          children: [
                            Container(
                              height: 8,
                              decoration: BoxDecoration(
                                color: Colors.white.withValues(alpha: 0.05),
                                borderRadius: BorderRadius.circular(100),
                              ),
                            ),
                            FractionallySizedBox(
                              widthFactor: _otaProgress,
                              child: Container(
                                height: 8,
                                decoration: BoxDecoration(
                                  gradient: const LinearGradient(
                                    colors: [
                                      Colors.purpleAccent,
                                      Colors.deepPurple,
                                    ],
                                  ),
                                  borderRadius: BorderRadius.circular(100),
                                ),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 20),
                      ],

                      ElevatedButton.icon(
                        onPressed: (isConnected && _selectedFileName != null && !_isUpdatingOta)
                            ? _simulateOta
                            : null,
                        icon: const Icon(Icons.system_update_alt_rounded),
                        label: Text(
                          _isUpdatingOta
                              ? 'Đang cập nhật...'
                              : 'Khởi chạy Cập nhật OTA',
                        ),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: Colors.purpleAccent.withValues(
                            alpha: 0.15,
                          ),
                          foregroundColor: Colors.purpleAccent,
                          minimumSize: const Size(double.infinity, 50),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(14),
                          ),
                          side: const BorderSide(
                            color: Colors.purpleAccent,
                            width: 1,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 30),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
