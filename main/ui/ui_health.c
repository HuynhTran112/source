/**
 * @file ui_health.c
 * @brief Định nghĩa giao diện đo đạc chỉ số Nhịp tim và nồng độ Oxy trong máu SpO2 (Health Monitoring UI)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Lớp thiết kế: Application / UI Layer
 * 
 * Mô tả: Module này xây dựng giao diện hiển thị các chỉ số sinh học của cơ thể thu nhận từ cảm biến
 * MAX30102 thông qua bus I2C. Giao diện chính bao gồm 2 thẻ đo lường (Metric Cards) cho Nhịp tim (Heart Rate)
 * và nồng độ Oxy (SpO2). Khi người dùng nhấn nút "Bắt đầu", hệ thống sẽ kích hoạt một màn hình trung gian 
 * yêu cầu đặt ngón tay lên cảm biến. Khi phát hiện tín hiệu PPG hợp lệ (Valid Signal), bộ định thì `lv_timer` 
 * tự động kích hoạt đếm ngược 15 giây đo đạc ổn định trước khi cập nhật kết quả và quay lại.
 */

#include "board_config.h"
#include "ui.h"
#include "ui_nav.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "hw_monitor.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>

LV_FONT_DECLARE(workout_health_icon);

/* Định nghĩa mã hex của các Icon y tế thuộc FontAwesome */
#define ICON_WORK_HEARTRATE "\xef\x88\x9e" // Quả tim nhịp đập
#define ICON_WORK_SPO2      "\xef\x81\x83" // Biểu tượng giọt máu/oxy

/**
 * @brief Khởi tạo một widget Thẻ đo lường chỉ số sinh học (Metric Card)
 * 
 * @param parent Container cha chứa thẻ này
 * @param icon Chuỗi ký tự biểu diễn glyph icon
 * @param color Màu sắc chủ đạo của icon đại diện
 * @param title Tiêu đề chỉ số (ví dụ: "Nhịp tim", "SpO2")
 * @param value Giá trị đo khởi tạo hiển thị mặc định
 * @param unit Đơn vị đo lường tương ứng (bpm, %)
 * @param out_value Con trỏ lưu nhãn hiển thị số để cập nhật giá trị động sau này
 * @return lv_obj_t* Nút Container thẻ được tạo lập
 */
static lv_obj_t *health_make_metric_card(lv_obj_t *parent, const char *icon,
                                         lv_color_t color, const char *title,
                                         const char *value, const char *unit,
                                         lv_obj_t **out_value)
{
  /* 1. Thiết lập Card nền Container xám tối */
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, WATCH_LCD_H_RES - 28, 78);
  lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 12, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  /* 2. Thiết lập Vòng tròn nền chứa Icon bên trái */
  lv_obj_t *ic_bg = lv_obj_create(card);
  lv_obj_set_size(ic_bg, 44, 44);
  lv_obj_set_style_radius(ic_bg, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ic_bg, color, 0);
  lv_obj_set_style_border_width(ic_bg, 0, 0);
  lv_obj_align(ic_bg, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_clear_flag(ic_bg, LV_OBJ_FLAG_SCROLLABLE);

  /* 3. Đưa Icon chữ vào vòng tròn */
  lv_obj_t *ic = lv_label_create(ic_bg);
  lv_label_set_text(ic, icon);
  lv_obj_set_style_text_font(ic, &workout_health_icon, 0);
  lv_obj_set_style_text_color(ic, lv_color_white(), 0);
  lv_obj_center(ic);

  /* 4. Nhãn tên chỉ số */
  lv_obj_t *name = lv_label_create(card);
  lv_label_set_text(name, title);
  lv_obj_set_style_text_font(name, UI_FONT_14, 0);
  lv_obj_set_style_text_color(name, COLOR_TEXT_MUTED, 0);
  lv_obj_align(name, LV_ALIGN_TOP_LEFT, 58, 2);

  /* 5. Nhãn hiển thị giá trị số đo được */
  lv_obj_t *val = lv_label_create(card);
  lv_label_set_text(val, value);
  lv_obj_set_style_text_font(val, UI_FONT_24, 0);
  lv_obj_set_style_text_color(val, COLOR_TEXT, 0);
  lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 58, -2);
  if (out_value) {
    *out_value = val; // Trả con trỏ ra ngoài để cập nhật động sau này
  }

  /* 6. Nhãn đơn vị đo đi kèm đặt cạnh số */
  lv_obj_t *unit_lbl = lv_label_create(card);
  lv_label_set_text(unit_lbl, unit);
  lv_obj_set_style_text_font(unit_lbl, UI_FONT_14, 0);
  lv_obj_set_style_text_color(unit_lbl, COLOR_TEXT_MUTED, 0);
  lv_obj_align_to(unit_lbl, val, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);

  return card;
}

static lv_obj_t *s_health_hr_value = NULL;
static lv_obj_t *s_health_spo2_value = NULL;
static lv_timer_t *s_health_measure_timer = NULL;
static lv_obj_t *s_health_measure_status = NULL;
static lv_obj_t *s_health_measure_screen = NULL;
static uint32_t s_health_measure_started_ms = 0;

#define HEALTH_MEASURE_TIMEOUT_MS 15000       // Thời gian đếm ngược đo ổn định (15 giây)
#define HEALTH_MEASURE_UPDATE_MS  500         // Tần suất làm tươi màn hình đếm ngược (0.5 giây)

static bool s_has_valid_reading = false;
static watch_hr_data_t s_latest_valid_hr;
static bool s_measure_running = false;

/**
 * @brief Cập nhật chuỗi kết quả nhịp tim và SpO2 lên giao diện chính
 */
static void health_update_values(const watch_hr_data_t *hr) {
  if (!hr || !hr->valid) return;

  char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)(hr->heart_rate + 0.5f));
  if (s_health_hr_value) {
    lv_label_set_text(s_health_hr_value, buf);
  }
  snprintf(buf, sizeof(buf), "%d", (int)(hr->spo2 + 0.5f));
  if (s_health_spo2_value) {
    lv_label_set_text(s_health_spo2_value, buf);
  }
}

/**
 * @brief Định kỳ quét kiểm tra trạng thái và đếm ngược thời gian đo
 */
static void health_measure_timer_cb(lv_timer_t *t) {
  watch_hr_data_t hr;
  bool found = watch_hw_monitor_get_hr(&hr); // Truy vấn dữ liệu xử lý DSP của MAX30102

  if (!s_measure_running) {
    if (found && hr.valid) {
      /* Phát hiện ngón tay đặt đúng chỗ và thu được dạng sóng hồng ngoại ổn định */
      s_measure_running = true;
      s_health_measure_started_ms = lv_tick_get();
      s_has_valid_reading = true;
      s_latest_valid_hr = hr;
      ESP_LOGI("ui_health", "Valid signal detected. Starting 15s countdown automatically.");
    } else {
      /* Cảnh báo người dùng đặt tay */
      if (s_health_measure_status) {
        lv_label_set_text(s_health_measure_status, ui_text("Place finger on sensor", "Đặt ngón tay lên cảm biến"));
      }
      return;
    }
  }

  if (found && hr.valid) {
    s_latest_valid_hr = hr;
    s_has_valid_reading = true;
  }

  /* Tính toán thời gian đếm ngược */
  uint32_t elapsed = lv_tick_elaps(s_health_measure_started_ms);
  int remaining = 15 - (int)(elapsed / 1000);
  if (remaining < 0) remaining = 0;

  if (s_health_measure_status) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s...\n%ds", ui_text("Measuring", "Đang đo"), remaining);
    lv_label_set_text(s_health_measure_status, buf);
  }

  /* Kết thúc đợt đo 15 giây */
  if (elapsed >= HEALTH_MEASURE_TIMEOUT_MS) {
    /* Tắt LED hồng ngoại/đỏ của MAX30102 để bảo vệ ngón tay và tiết kiệm pin */
    watch_hw_monitor_set_hr_measure_enabled(false);
    s_health_measure_timer = NULL;
    lv_timer_del(t);
    s_measure_running = false;

    /* Quay lại màn hình Health chính */
    if (s_health_measure_screen && lv_scr_act() == s_health_measure_screen) {
      ui_nav_pop();
    }

    /* Đổ giá trị đo ổn định cuối cùng lên UI */
    if (s_has_valid_reading) {
      health_update_values(&s_latest_valid_hr);
    }
  }
}

/**
 * @brief Xóa bỏ màn hình tiến trình đo nhịp tim
 */
static void health_measure_delete_cb(lv_event_t *e) {
  (void)e;
  if (s_health_measure_timer) {
    lv_timer_del(s_health_measure_timer);
    s_health_measure_timer = NULL;
  }
  s_health_measure_status = NULL;
  s_health_measure_screen = NULL;
  watch_hw_monitor_set_hr_measure_enabled(false);
  s_measure_running = false;
}

/**
 * @brief Khởi chạy quy trình đo nhịp tim (Đẩy màn hình đo đếm ngược)
 */
static void health_start_cb(lv_event_t *e) {
  (void)e;
  s_has_valid_reading = false;
  s_measure_running = false;
  memset(&s_latest_valid_hr, 0, sizeof(s_latest_valid_hr));

  lv_obj_t *scr = ui_nav_create_screen();
  s_health_measure_screen = scr;
  lv_obj_add_event_cb(scr, health_measure_delete_cb, LV_EVENT_DELETE, NULL);

  /* Thẻ thông báo trạng thái đo đạc */
  lv_obj_t *box = lv_obj_create(scr);
  lv_obj_set_size(box, 200, 86);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(box, 18, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  s_health_measure_status = lv_label_create(box);
  lv_label_set_text(s_health_measure_status, ui_text("Place finger on sensor", "Đặt ngón tay lên cảm biến"));
  lv_obj_set_width(s_health_measure_status, 176);
  lv_obj_set_style_text_align(s_health_measure_status, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(s_health_measure_status, UI_FONT_14, 0);
  lv_obj_set_style_text_color(s_health_measure_status, COLOR_TEXT, 0);
  lv_obj_center(s_health_measure_status);

  ui_nav_add_gesture(scr);
  ui_nav_push(scr);
  
  if (s_health_measure_timer) {
    lv_timer_del(s_health_measure_timer);
  }
  
  /* Bật nguồn cấp cho phần cứng IC MAX30102 thông qua chân LDO Enable */
  watch_hw_monitor_set_hr_measure_enabled(true);
  s_health_measure_started_ms = lv_tick_get();
  
  /* Tạo timer lặp lại quét tín hiệu */
  s_health_measure_timer = lv_timer_create(health_measure_timer_cb, HEALTH_MEASURE_UPDATE_MS, NULL);
  health_measure_timer_cb(s_health_measure_timer);
}

static void health_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_health_hr_value = NULL;
  s_health_spo2_value = NULL;
}

/**
 * @brief Khởi tạo đối tượng màn hình Health (Danh sách thẻ thông số sức khỏe)
 */
lv_obj_t *ui_health_screen_create(void)
{
  lv_obj_t *scr = ui_nav_create_screen_with_id(UI_SCREEN_HEALTH);
  lv_obj_add_event_cb(scr, health_screen_delete_cb, LV_EVENT_DELETE, NULL);

  /* Tạo container chứa danh sách thẻ dọc */
  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, WATCH_LCD_H_RES, 178);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 8, 0);
  lv_obj_set_style_pad_row(list, 10, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /* Thẻ Nhịp tim */
  health_make_metric_card(list, ICON_WORK_HEARTRATE, COLOR_RED,
                          ui_tr(UI_TXT_HEART_RATE), "--", "bpm", &s_health_hr_value);
  /* Thẻ SpO2 */
  health_make_metric_card(list, ICON_WORK_SPO2, COLOR_CYAN,
                          "SpO2", "--", "%", &s_health_spo2_value);

  /* Nút bấm khởi chạy phép đo */
  lv_obj_t *start = lv_btn_create(scr);
  lv_obj_set_size(start, 150, 42);
  lv_obj_set_style_radius(start, 18, 0);
  lv_obj_set_style_bg_color(start, COLOR_PRIMARY, 0);
  lv_obj_set_style_shadow_width(start, 0, 0);
  lv_obj_align(start, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_add_event_cb(start, health_start_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *start_lbl = lv_label_create(start);
  lv_label_set_text(start_lbl, ui_text("Start", "Bắt đầu"));
  lv_obj_set_style_text_font(start_lbl, UI_FONT_14, 0);
  lv_obj_center(start_lbl);

  ui_nav_add_gesture(scr);
  return scr;
}

