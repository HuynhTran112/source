import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'ble_service.dart';
import 'notification_handler.dart';
import 'user_profile_settings.dart';
import 'screens/dashboard_screen.dart';
import 'screens/health_screen.dart';
import 'screens/workout_screen.dart';
import 'screens/ota_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await UserProfileSettings.init();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmartWatch Companion',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF3B82F6),
          surface: Color(0xFF0F172A),
        ),
        useMaterial3: true,
      ),
      home: const HomeScreen(),
    );
  }
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final BleService _bleService = BleService();
  final NotificationHandler _notifHandler = NotificationHandler();

  int _currentIndex = 0;

  @override
  void initState() {
    super.initState();
    _notifHandler.init();

    _bleService.statusStream.listen((state) {
      if (!mounted) return;
      if (state == DeviceConnectionState.connected) {
        _notifHandler.init();
      }
    });
  }

  Future<void> _showDevicePicker() async {
    final hasPermission = await _bleService.requestPermissions();
    if (!hasPermission) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Vui lòng cấp quyền Bluetooth và Vị trí'),
            backgroundColor: Colors.redAccent,
          ),
        );
      }
      return;
    }

    if (!mounted) return;
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: const Color(0xFF0F172A),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      builder: (context) => const DevicePickerSheet(),
    );
  }

  @override
  Widget build(BuildContext context) {
    final screens = <Widget>[
      DashboardScreen(onConnectPressed: _showDevicePicker),
      const HealthScreen(),
      const WorkoutScreen(),
      const OtaScreen(),
    ];

    return Scaffold(
      backgroundColor: const Color(0xFF020617),
      body: screens[_currentIndex],
      bottomNavigationBar: Container(
        decoration: BoxDecoration(
          color: const Color(0xFF0F172A),
          border: Border(
            top: BorderSide(
              color: Colors.white.withValues(alpha: 0.06),
              width: 1,
            ),
          ),
        ),
        child: BottomNavigationBar(
          currentIndex: _currentIndex,
          onTap: (index) => setState(() => _currentIndex = index),
          backgroundColor: const Color(0xFF0F172A),
          selectedItemColor: const Color(0xFF3B82F6),
          unselectedItemColor: Colors.white30,
          type: BottomNavigationBarType.fixed,
          elevation: 0,
          selectedFontSize: 11,
          unselectedFontSize: 11,
          selectedLabelStyle: const TextStyle(fontWeight: FontWeight.bold),
          items: const [
            BottomNavigationBarItem(
              icon: Icon(Icons.dashboard_rounded),
              label: 'Dashboard',
            ),
            BottomNavigationBarItem(
              icon: Icon(Icons.favorite_rounded),
              label: 'Sức khỏe',
            ),
            BottomNavigationBarItem(
              icon: Icon(Icons.fitness_center_rounded),
              label: 'Workout',
            ),
            BottomNavigationBarItem(
              icon: Icon(Icons.system_update_alt_rounded),
              label: 'OTA',
            ),
          ],
        ),
      ),
    );
  }
}

class DevicePickerSheet extends StatefulWidget {
  const DevicePickerSheet({super.key});

  @override
  State<DevicePickerSheet> createState() => _DevicePickerSheetState();
}

class _DevicePickerSheetState extends State<DevicePickerSheet> {
  final BleService _bleService = BleService();
  final List<DiscoveredDevice> _devices = [];
  late StreamSubscription<DiscoveredDevice> _scanSub;

  @override
  void initState() {
    super.initState();
    _scanSub = _bleService.scanDevices().listen((device) {
      if (device.name.isNotEmpty && !_devices.any((d) => d.id == device.id)) {
        setState(() => _devices.add(device));
      }
    });
  }

  @override
  void dispose() {
    _scanSub.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      height: MediaQuery.of(context).size.height * 0.6,
      padding: const EdgeInsets.symmetric(vertical: 20),
      child: Column(
        children: [
          const Text(
            'Thiết bị khả dụng',
            style: TextStyle(
              fontSize: 18,
              fontWeight: FontWeight.bold,
              color: Colors.white,
            ),
          ),
          const SizedBox(height: 16),
          if (_devices.isEmpty)
            const Expanded(
              child: Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CircularProgressIndicator(color: Color(0xFF3B82F6)),
                    SizedBox(height: 16),
                    Text(
                      'Đang dò tìm SmartWatch...',
                      style: TextStyle(color: Colors.white38),
                    ),
                  ],
                ),
              ),
            )
          else
            Expanded(
              child: ListView.builder(
                itemCount: _devices.length,
                padding: const EdgeInsets.symmetric(horizontal: 10),
                itemBuilder: (context, index) {
                  final device = _devices[index];
                  return Container(
                    margin: const EdgeInsets.symmetric(vertical: 6),
                    decoration: BoxDecoration(
                      color: Colors.white.withValues(alpha: 0.02),
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(
                        color: Colors.white.withValues(alpha: 0.04),
                      ),
                    ),
                    child: ListTile(
                      leading: const Icon(
                        Icons.watch_rounded,
                        color: Color(0xFF3B82F6),
                      ),
                      title: Text(
                        device.name,
                        style: const TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      subtitle: Text(
                        device.id,
                        style: const TextStyle(
                          color: Colors.white30,
                          fontSize: 12,
                        ),
                      ),
                      trailing: const Icon(
                        Icons.link_rounded,
                        color: Color(0xFF3B82F6),
                      ),
                      onTap: () {
                        _bleService.connect(device);
                        Navigator.pop(context);
                      },
                    ),
                  );
                },
              ),
            ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20),
            child: TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text(
                'Hủy',
                style: TextStyle(color: Colors.redAccent),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
