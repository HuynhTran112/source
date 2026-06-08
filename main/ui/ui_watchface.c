/**
 * @file ui_watchface.c
 * @brief Định nghĩa giao diện Mặt đồng hồ chính (Watchface) đa chủ đề
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Lớp thiết kế: Application / UI Layer
 * 
 * Mô tả: File này xây dựng giao diện hiển thị giờ chính của Smartwatch hỗ trợ 3 kiểu giao diện:
 * 1. Số đơn giản (Digital Simple): Hiển thị giờ lớn, ngày tháng, đếm bước chân (IMU BMI270) và dung lượng pin.
 * 2. Số hiện đại (Gradient Modern): Hiển thị giờ dạng thẻ xếp dọc trên nền chuyển sắc gradient chuyển động, bao quanh bởi cung tròn dung lượng pin.
 * 3. Kim cổ điển (Analog Minimal): Mô phỏng mặt đồng hồ kim tròn với kim Giờ, Phút, Giây xoay quanh trục chính giữa dựa trên góc tính toán lượng giác.
 * Hỗ trợ cử chỉ vuốt sang trái để truy cập Menu ứng dụng và gõ 3 lần (Triple-tap) để khôi phục nhanh màn hình trước đó.
 */

#include "ui.h"
#include "ui_nav.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "hw_monitor.h"
#include <stdio.h>

/* Các con trỏ lưu trữ đối tượng hiển thị của mặt đồng hồ */
static lv_obj_t *s_watchface_time_label = NULL;
static lv_obj_t *s_watchface_date_label = NULL;
static lv_obj_t *s_watchface_battery_info_label = NULL;
static lv_obj_t *s_watchface_steps_label = NULL;
static lv_obj_t *s_watchface_battery_arc = NULL;
static lv_obj_t *s_watchface_second_hand = NULL;
static lv_obj_t *s_watchface_minute_hand = NULL;
static lv_obj_t *s_watchface_hour_hand = NULL;

static lv_timer_t *s_watchface_time_timer = NULL; // Bộ định thời cập nhật màn hình
static lv_obj_t *s_watchface_scr = NULL;
static int s_watchface_style = 1;                  /* Mặc định style: 1 (0: Simple, 1: Gradient, 2: Analog) */

#define WATCHFACE_DIGITAL_REFRESH_MS 60000        // Chế độ số chỉ cần làm tươi mỗi phút (Tiết kiệm pin)
#define WATCHFACE_ANALOG_REFRESH_MS 1000          // Chế độ kim cần chạy kim giây trôi mỗi 1 giây
#define WATCHFACE_RESTORE_TAP_COUNT 3             // Số lần gõ yêu cầu để khôi phục nhanh
#define WATCHFACE_RESTORE_TAP_WINDOW_MS 1500      // Khung thời gian gõ tối đa (1.5 giây)

/* Tên viết tắt các ngày trong tuần phục vụ chuyển đổi ngôn ngữ */
static const char *s_weekday_names_en[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *s_weekday_names_vi[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};

/**
 * @brief Chuẩn hóa mã kiểu mặt đồng hồ luôn nằm trong giới hạn [0, 2]
 */
static int watchface_normalize_style(int style_id) {
  int style = style_id % 3;
  return (style < 0) ? style + 3 : style;
}

/**
 * @brief Xác định chu kỳ timer dựa trên kiểu mặt đồng hồ đang chạy
 */
static uint32_t watchface_timer_period_ms(void) {
  return (s_watchface_style == 2) ? WATCHFACE_ANALOG_REFRESH_MS : WATCHFACE_DIGITAL_REFRESH_MS;
}

/**
 * @brief Điều chỉnh lại tần số quét của timer khi đổi kiểu mặt đồng hồ
 */
static void watchface_sync_timer_period(void) {
  if (!s_watchface_time_timer) return;
  lv_timer_set_period(s_watchface_time_timer, watchface_timer_period_ms());
  lv_timer_reset(s_watchface_time_timer);
}

/**
 * @brief Hàm callback cập nhật dữ liệu hiển thị (Thực hiện chu kỳ quét)
 */
static void watchface_update_time_cb(lv_timer_t *t) {
  (void)t;
  ui_time_snapshot_t ti;
  ui_time_get_snapshot(&ti); // Đọc thời gian thực hiện tại

  char buf[32];
  
  if (s_watchface_style == 0) { /* 1. Mặt số đơn giản (Digital Simple) */
    ui_time_format_hhmm(buf, sizeof(buf), &ti);
    ui_label_set_text_if_changed(s_watchface_time_label, buf);
  } 
  else if (s_watchface_style == 1) { /* 2. Mặt số hiện đại (Gradient Modern) */
    snprintf(buf, sizeof(buf), "%02d\n%02d", ti.hour, ti.minute);
    ui_label_set_text_if_changed(s_watchface_time_label, buf);
  } 
  else if (s_watchface_style == 2) { /* 3. Mặt kim cổ điển (Analog Minimal) */
    /*
     * Tính toán góc xoay dựa trên lượng giác của LVGL:
     * - LVGL sử dụng góc đơn vị là 0.1 độ (tức 3600 đơn vị tương đương 360 độ).
     * - Kim giây xoay: Mỗi giây đi 6 độ = 60 đơn vị xoay.
     * - Kim phút xoay: Mỗi phút đi 6 độ + kim giây bổ trợ.
     * - Kim giờ xoay: Mỗi giờ đi 30 độ + kim phút bổ trợ.
     */
    if (s_watchface_second_hand) {
        lv_obj_set_style_transform_angle(s_watchface_second_hand, ti.second * 60, 0);
    }
    if (s_watchface_minute_hand) {
        lv_obj_set_style_transform_angle(s_watchface_minute_hand, (ti.minute * 60) + ti.second, 0);
    }
    if (s_watchface_hour_hand) {
        lv_obj_set_style_transform_angle(s_watchface_hour_hand, ((ti.hour % 12) * 300) + (ti.minute * 5), 0);
    }
  }

  /* Định dạng và vẽ chuỗi ngày tháng đa ngôn ngữ */
  const char *const *weekday_names = (ui_language_get() == UI_LANG_VI) ? s_weekday_names_vi : s_weekday_names_en;
  snprintf(buf, sizeof(buf), "%s, %02d/%02d", weekday_names[ti.weekday], ti.day, ti.month);
  ui_label_set_text_if_changed(s_watchface_date_label, buf);

  /* Cập nhật các thông số cảm biến tương thích theo từng style */
  if (s_watchface_style == 0) {
    /* Đọc thông số pin sạc */
    watch_battery_data_t bat;
    if (watch_hw_monitor_get_battery(&bat)) {
      snprintf(buf, sizeof(buf), "Pin: %d%%", (int)bat.soc_percent);
      ui_label_set_text_if_changed(s_watchface_battery_info_label, buf);
    }
    /* Đọc số bước chân đếm được từ gia tốc kế IMU BMI270 */
    watch_imu_data_t imu;
    if (watch_hw_monitor_get_imu(&imu)) {
      snprintf(buf, sizeof(buf), "Bước: %lu", (unsigned long)imu.step_count);
      ui_label_set_text_if_changed(s_watchface_steps_label, buf);
    }
  } 
  else if (s_watchface_style == 1) {
    /* Cập nhật thanh cung tròn pin bao quanh */
    watch_battery_data_t bat;
    if (watch_hw_monitor_get_battery(&bat) && s_watchface_battery_arc) {
      lv_arc_set_value(s_watchface_battery_arc, (int)bat.soc_percent);
    }
  }
}

/**
 * @brief Bắt sự kiện cử chỉ vuốt (Gesture) để chuyển hướng màn hình
 */
static void watchface_gesture_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir == LV_DIR_LEFT) {
    /* Vuốt trái -> Đẩy màn hình menu lưới (Menu Screen) vào stack */
    lv_obj_t *scr_menu = ui_menu_get_screen();
    ui_statusbar_set_time_visible(true); // Hiển thị giờ trên status bar ở các màn hình con
    ui_nav_push(scr_menu);
  }
}

/**
 * @brief Khôi phục lại phiên làm việc cũ khi click nhanh 3 lần (Triple-tap) lên Watchface
 */
static void watchface_restore_last_cb(lv_event_t *e) {
  (void)e;
  static uint8_t tap_count = 0;
  static uint32_t first_tap_ms = 0;
  uint32_t now_ms = lv_tick_get();

  /* Kiểm tra cửa sổ thời gian gõ */
  if (tap_count == 0 ||
      (uint32_t)(now_ms - first_tap_ms) > WATCHFACE_RESTORE_TAP_WINDOW_MS) {
    first_tap_ms = now_ms;
    tap_count = 1;
    return;
  }

  tap_count++;
  if (tap_count < WATCHFACE_RESTORE_TAP_COUNT) {
    return;
  }

  /* Kích hoạt khôi phục */
  tap_count = 0;
  first_tap_ms = 0;
  if (ui_nav_restore_last_screen()) {
    ui_statusbar_set_time_visible(true);
  }
}

/**
 * @brief Xóa bỏ màn hình dọn dẹp tài nguyên
 */
static void watchface_screen_delete_cb(lv_event_t *e) {
  if (s_watchface_time_timer) { 
    lv_timer_del(s_watchface_time_timer); 
    s_watchface_time_timer = NULL; 
  }
  s_watchface_time_label = NULL;
  s_watchface_date_label = NULL;
  s_watchface_battery_info_label = NULL;
  s_watchface_steps_label = NULL;
  s_watchface_battery_arc = NULL;
  s_watchface_second_hand = NULL;
  s_watchface_minute_hand = NULL;
  s_watchface_hour_hand = NULL;
  s_watchface_scr = NULL;
}

/**
 * @brief Khởi tạo đối tượng màn hình Watchface và các widget con theo style đã chọn
 */
lv_obj_t *ui_watchface_screen_create(void) {
  lv_obj_t *scr = ui_nav_create_screen_with_id(UI_SCREEN_WATCHFACE);
  s_watchface_scr = scr;
  lv_obj_add_event_cb(scr, watchface_screen_delete_cb, LV_EVENT_DELETE, NULL);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

  if (s_watchface_style == 0) { /* Style 1: SỐ ĐƠN GIẢN */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* Nhãn giờ lớn đặt ở chính giữa */
    s_watchface_time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_time_label, UI_FONT_48, 0);
    lv_obj_set_style_text_color(s_watchface_time_label, lv_color_white(), 0);
    lv_obj_align(s_watchface_time_label, LV_ALIGN_CENTER, 0, -35);

    /* Nhãn ngày tháng phụ ở dưới nhãn giờ */
    s_watchface_date_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_date_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_watchface_date_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_watchface_date_label, LV_ALIGN_CENTER, 0, 15);

    /* Số bước chân (Góc trái bên dưới) */
    s_watchface_steps_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_steps_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_watchface_steps_label, lv_color_hex(0x10B981), 0); // Màu xanh lục tươi
    lv_obj_align(s_watchface_steps_label, LV_ALIGN_CENTER, -50, 55);

    /* Chỉ số pin (Góc phải bên dưới) */
    s_watchface_battery_info_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_battery_info_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_watchface_battery_info_label, lv_color_hex(0x3B82F6), 0); // Màu xanh dương dịu
    lv_obj_align(s_watchface_battery_info_label, LV_ALIGN_CENTER, 50, 55);
  } 
  else if (s_watchface_style == 1) { /* Style 2: SỐ HIỆN ĐẠI GRADIENT */
    /* Đặt nền chuyển sắc Gradient (từ xanh ngọc sang tím hồng) */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x44c5cd), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x954dcf), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

    /* Nhãn giờ và phút xếp dọc chồng lên nhau */
    s_watchface_time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_time_label, UI_FONT_48, 0);
    lv_obj_set_style_text_color(s_watchface_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_line_space(s_watchface_time_label, -10, 0); // Thu hẹp khoảng cách dòng
    lv_obj_set_style_text_align(s_watchface_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_watchface_time_label, LV_ALIGN_CENTER, 0, -15);

    /* Vòng tròn cung chỉ báo dung lượng Pin bao quanh */
    s_watchface_battery_arc = lv_arc_create(scr);
    lv_obj_set_size(s_watchface_battery_arc, 220, 220);
    lv_arc_set_bg_angles(s_watchface_battery_arc, 0, 360);
    lv_arc_set_angles(s_watchface_battery_arc, 270, 270);
    lv_obj_set_style_arc_width(s_watchface_battery_arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_watchface_battery_arc, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_watchface_battery_arc, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_watchface_battery_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_watchface_battery_arc, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_remove_style(s_watchface_battery_arc, NULL, LV_PART_KNOB); // Xóa núm vặn tròn của Arc mặc định
    lv_obj_center(s_watchface_battery_arc);

    s_watchface_date_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_date_label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(s_watchface_date_label, lv_color_white(), 0);
    lv_obj_align(s_watchface_date_label, LV_ALIGN_BOTTOM_MID, 0, -45);
  }
  else if (s_watchface_style == 2) { /* Style 3: KIM CỔ ĐIỂN (Analog Minimal) */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* Khung tròn mặt đồng hồ (Dial Container) */
    lv_obj_t *dial = lv_obj_create(scr);
    lv_obj_set_size(dial, 210, 210);
    lv_obj_set_style_bg_opa(dial, 0, 0);
    lv_obj_set_style_border_width(dial, 0, 0);
    lv_obj_center(dial);
    lv_obj_clear_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

    /* Vẽ các vạch chia nhỏ tại 4 góc chính (12, 3, 6, 9 giờ) */
    for (int i = 0; i < 4; i++) {
      lv_obj_t *tick = lv_obj_create(dial);
      lv_obj_set_size(tick, (i % 2 == 0) ? 2 : 10, (i % 2 == 0) ? 10 : 2);
      lv_obj_set_style_bg_color(tick, COLOR_SURFACE_LT, 0);
      lv_obj_set_style_border_width(tick, 0, 0);
      lv_obj_set_style_radius(tick, 1, 0);
      
      if (i == 0) lv_obj_align(tick, LV_ALIGN_TOP_MID, 0, 5);    /* 12 Giờ */
      else if (i == 1) lv_obj_align(tick, LV_ALIGN_RIGHT_MID, -5, 0); /* 3 Giờ */
      else if (i == 2) lv_obj_align(tick, LV_ALIGN_BOTTOM_MID, 0, -5); /* 6 Giờ */
      else if (i == 3) lv_obj_align(tick, LV_ALIGN_LEFT_MID, 5, 0);  /* 9 Giờ */
    }

    /* Thiết lập Kim Phút (Màu trắng, dài và thanh mảnh) */
    s_watchface_minute_hand = lv_obj_create(dial);
    lv_obj_set_size(s_watchface_minute_hand, 4, 90);
    lv_obj_set_style_bg_color(s_watchface_minute_hand, lv_color_white(), 0);
    lv_obj_set_style_radius(s_watchface_minute_hand, 2, 0);
    lv_obj_set_style_border_width(s_watchface_minute_hand, 0, 0);
    lv_obj_align(s_watchface_minute_hand, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_transform_pivot_x(s_watchface_minute_hand, 2, 0); // Đặt tâm xoay trên trục X
    lv_obj_set_style_transform_pivot_y(s_watchface_minute_hand, 80, 0); // Đặt tâm xoay trên trục Y cách mép

    /* Thiết lập Kim Giờ (Màu trắng, ngắn và dày hơn) */
    s_watchface_hour_hand = lv_obj_create(dial);
    lv_obj_set_size(s_watchface_hour_hand, 6, 65);
    lv_obj_set_style_bg_color(s_watchface_hour_hand, lv_color_white(), 0);
    lv_obj_set_style_radius(s_watchface_hour_hand, 3, 0);
    lv_obj_set_style_border_width(s_watchface_hour_hand, 0, 0);
    lv_obj_align(s_watchface_hour_hand, LV_ALIGN_CENTER, 0, -28);
    lv_obj_set_style_transform_pivot_x(s_watchface_hour_hand, 3, 0);
    lv_obj_set_style_transform_pivot_y(s_watchface_hour_hand, 55, 0);

    /* Thiết lập Kim Giây (Màu đỏ làm điểm nhấn, chạy trôi trơn) */
    s_watchface_second_hand = lv_obj_create(dial);
    lv_obj_set_size(s_watchface_second_hand, 2, 100);
    lv_obj_set_style_bg_color(s_watchface_second_hand, COLOR_RED, 0);
    lv_obj_set_style_border_width(s_watchface_second_hand, 0, 0);
    lv_obj_align(s_watchface_second_hand, LV_ALIGN_CENTER, 0, -45);
    lv_obj_set_style_transform_pivot_x(s_watchface_second_hand, 1, 0);
    lv_obj_set_style_transform_pivot_y(s_watchface_second_hand, 90, 0);

    /* Nắp chốt kim đồng hồ ở trung tâm (Premium Center Center Cap) */
    lv_obj_t *dot_bg = lv_obj_create(dial);
    lv_obj_set_size(dot_bg, 12, 12);
    lv_obj_set_style_radius(dot_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_bg, lv_color_black(), 0);
    lv_obj_set_style_border_width(dot_bg, 2, 0);
    lv_obj_set_style_border_color(dot_bg, lv_color_white(), 0);
    lv_obj_center(dot_bg);

    s_watchface_date_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_date_label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(s_watchface_date_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_watchface_date_label, LV_ALIGN_BOTTOM_MID, 0, -35);
  }

  /* Đăng ký sự kiện điều phối cử chỉ và gõ */
  lv_obj_add_event_cb(scr, watchface_gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_add_event_cb(scr, watchface_restore_last_cb, LV_EVENT_CLICKED, NULL);
  
  /* Ẩn nhãn giờ hệ thống trên Status Bar do watchface đã có hiển thị thời gian chính */
  ui_statusbar_set_time_visible(false);
  
  s_watchface_time_timer = lv_timer_create(watchface_update_time_cb, watchface_timer_period_ms(), NULL);
  watchface_update_time_cb(NULL); // Làm tươi nội dung hiển thị ngay lập tức

  return scr;
}

/**
 * @brief Thiết lập kiểu giao diện hiển thị cho mặt đồng hồ
 */
void ui_watchface_set_style(int style_id) {
  int new_style = watchface_normalize_style(style_id);
  if (new_style == s_watchface_style) return;

  s_watchface_style = new_style;
  watchface_sync_timer_period();
  
  if (s_watchface_scr) {
    /* Dựng lại màn hình gốc theo phong cách mới */
    ui_nav_recreate_root(ui_watchface_screen_create);
  } else {
    watchface_update_time_cb(NULL);
  }
}

/**
 * @brief Truy vấn kiểu mặt đồng hồ hiện tại đang chạy
 */
int ui_watchface_get_style(void) { 
  return s_watchface_style; 
}

