/// Parse Google Maps navigation notification thành dữ liệu có cấu trúc
/// để gửi cho đồng hồ qua BLE.
///
/// Hỗ trợ cả tiếng Việt và tiếng Anh.
library;

class GmapsNavData {
  final String turnToken; // straight, left, right, uturn, arrive, ...
  final int distanceMeters; // khoảng cách đến lượt rẽ tiếp theo
  final String roadName; // tên đường

  const GmapsNavData({
    required this.turnToken,
    required this.distanceMeters,
    required this.roadName,
  });

  bool get isValid => turnToken.isNotEmpty;

  @override
  String toString() =>
      'GmapsNavData(turn=$turnToken, dist=${distanceMeters}m, road=$roadName)';
}

class GmapsNavParser {
  /// Parse notification title + content thành nav data.
  /// Trả về null nếu notification không phải dẫn đường.
  static GmapsNavData? parse(String? title, String? content) {
    final t = (title ?? '').trim();
    final c = (content ?? '').trim();
    if (t.isEmpty && c.isEmpty) return null;

    // Ghép lại để parse linh hoạt
    final combined = '$t  $c';

    final turn = _extractTurn(combined);
    final distance = _extractDistance(t) ?? _extractDistance(c);
    final road = _extractRoad(combined);

    // Phải có ít nhất hướng rẽ HOẶC khoảng cách
    if (turn == null && distance == null) return null;

    return GmapsNavData(
      turnToken: turn ?? 'straight',
      distanceMeters: distance ?? 0,
      roadName: road ?? '',
    );
  }

  /// Kiểm tra notification có phải Google Maps navigation không
  static bool isNavNotification(String? title, String? content) {
    final combined = '${title ?? ''} ${content ?? ''}'.toLowerCase();
    return _hasNavKeyword(combined) || _hasDistancePattern(combined);
  }

  static bool _hasNavKeyword(String text) {
    const keywords = [
      // Tiếng Việt
      'rẽ phải', 'rẽ trái', 'đi thẳng', 'quay đầu',
      'giữ bên', 'điểm đến', 'bạn đã đến', 'tiếp tục đi',
      'đi về hướng', 'hướng về', 'rẽ nhẹ',
      // English
      'turn right', 'turn left', 'continue on', 'u-turn',
      'keep right', 'keep left', 'head north', 'head south',
      'head east', 'head west', 'slight right', 'slight left',
      'destination', 'you have arrived', 'merge onto',
    ];
    return keywords.any((k) => text.contains(k));
  }

  static bool _hasDistancePattern(String text) {
    return RegExp(
      r'\d+[\.,]?\d*\s*(m|km)\b',
      caseSensitive: false,
    ).hasMatch(text);
  }

  // ── Extract hướng rẽ ──────────────────────────────────────────

  static String? _extractTurn(String text) {
    final lower = text.toLowerCase();

    // Tiếng Việt (check pattern dài trước)
    if (lower.contains('bạn đã đến') || lower.contains('điểm đến')) {
      return 'arrive';
    }
    if (lower.contains('quay đầu')) return 'uturn';
    if (lower.contains('rẽ nhẹ sang phải') || lower.contains('rẽ nhẹ phải')) {
      return 'slight_right';
    }
    if (lower.contains('rẽ nhẹ sang trái') || lower.contains('rẽ nhẹ trái')) {
      return 'slight_left';
    }
    if (lower.contains('giữ bên phải')) return 'keep_right';
    if (lower.contains('giữ bên trái')) return 'keep_left';
    if (lower.contains('rẽ phải')) return 'right';
    if (lower.contains('rẽ trái')) return 'left';
    if (lower.contains('đi thẳng')) return 'straight';
    if (lower.contains('tiếp tục đi') || lower.contains('tiếp tục')) {
      return 'straight';
    }
    if (lower.contains('đi về hướng') || lower.contains('hướng về')) {
      return 'straight';
    }

    // English
    if (lower.contains('you have arrived') || lower.contains('destination')) {
      return 'arrive';
    }
    if (lower.contains('u-turn') || lower.contains('u turn')) return 'uturn';
    if (lower.contains('slight right') || lower.contains('bear right')) {
      return 'slight_right';
    }
    if (lower.contains('slight left') || lower.contains('bear left')) {
      return 'slight_left';
    }
    if (lower.contains('keep right')) return 'keep_right';
    if (lower.contains('keep left')) return 'keep_left';
    if (lower.contains('turn right')) return 'right';
    if (lower.contains('turn left')) return 'left';
    if (lower.contains('merge')) return 'straight';
    if (RegExp(
      r'head\s+(north|south|east|west)',
      caseSensitive: false,
    ).hasMatch(lower)) {
      return 'straight';
    }
    if (lower.contains('continue')) return 'straight';

    return null;
  }

  // ── Extract khoảng cách ───────────────────────────────────────

  static int? _extractDistance(String text) {
    // Match: "200 m", "1,2 km", "1.2 km", "500m"
    final match = RegExp(
      r'(\d+[\.,]?\d*)\s*(m|km)\b',
      caseSensitive: false,
    ).firstMatch(text);
    if (match == null) return null;

    final numStr = match.group(1)!.replaceAll(',', '.');
    final unit = match.group(2)!.toLowerCase();
    final value = double.tryParse(numStr);
    if (value == null) return null;

    if (unit == 'km') return (value * 1000).round();
    return value.round();
  }

  // ── Extract tên đường ─────────────────────────────────────────

  static String? _extractRoad(String text) {
    String? road;

    // Tiếng Việt: "Rẽ phải vào Đường Nguyễn Huệ"
    final viMatch = RegExp(
      r'(?:vào|trên)\s+(.+)',
      caseSensitive: false,
    ).firstMatch(text);
    if (viMatch != null) {
      road = viMatch.group(1)?.trim();
    }

    // English: "Turn right onto Main Street"
    if (road == null || road.isEmpty) {
      final enMatch = RegExp(
        r'(?:onto|on)\s+(.+)',
        caseSensitive: false,
      ).firstMatch(text);
      if (enMatch != null) {
        road = enMatch.group(1)?.trim();
      }
    }

    if (road != null && road.isNotEmpty) {
      // Bỏ phần khoảng cách ở cuối
      road = road.replaceAll(RegExp(r'\s*\d+[\.,]?\d*\s*(m|km)\b.*$'), '');
      // Bỏ ký tự | (dùng trong BLE protocol)
      road = road.replaceAll('|', ' ').trim();
    }

    return (road != null && road.isNotEmpty) ? road : null;
  }
}
