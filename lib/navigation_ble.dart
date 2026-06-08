// Giao thức đồng bộ chỉ đường sang ESP32 (khớp ble_notify.c / ui_apps.c).
//
// Chuỗi gửi:
// NAV|current_turn|distance_to_turn_m|primary_road|next_turn|next_road|total_remaining_m
//
// *_turn: straight | left | right | slight_left | slight_right |
// keep_left | keep_right | uturn | arrive
//
// Kết thúc: NAV_END

String sanitizeRoadForBle(String road) => road.replaceAll('|', ' ').trim();
