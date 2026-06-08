import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

class UserProfileSettings {
  static const double defaultWeightKg = 60.0;
  static const _weightKey = 'user_weight_kg';

  static final ValueNotifier<double> weightKg = ValueNotifier<double>(
    defaultWeightKg,
  );

  static Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    weightKg.value = prefs.getDouble(_weightKey) ?? defaultWeightKg;
  }

  static Future<void> setWeightKg(double value) async {
    final normalized = value.clamp(30.0, 200.0).toDouble();
    weightKg.value = normalized;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setDouble(_weightKey, normalized);
  }

  static int calcCalories({
    required String sportMode,
    required int durationSec,
  }) {
    if (durationSec <= 0) return 0;
    final met = sportMode == 'cycling' ? 7.0 : 3.5;
    return (met * weightKg.value * durationSec / 3600).round();
  }
}
