/**
 * @file ui.h
 * @brief Khai báo các thư viện, phông chữ, bảng màu và các hàm điều khiển giao diện LVGL
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Thiết lập các định nghĩa phông chữ (Fonts) được chuyển đổi từ font hệ thống, định nghĩa bảng màu
 * giao diện chế độ nền tối (Dark Theme) sang trọng, và các API điều khiển giao diện lõi.
 */

#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include <stdbool.h>

/* --- KHAI BÁO PHÔNG CHỮ HỆ THỐNG (Fonts) --- */
LV_FONT_DECLARE(font_12);
LV_FONT_DECLARE(font_14);
LV_FONT_DECLARE(font_16);
LV_FONT_DECLARE(font_20);
LV_FONT_DECLARE(font_24);
LV_FONT_DECLARE(font_36);
LV_FONT_DECLARE(font_48);

#define UI_FONT_12 (&font_12) // Phông chữ kích thước nhỏ 12px
#define UI_FONT_14 (&font_14) // Phông chữ thông thường 14px
#define UI_FONT_16 (&font_16) // Phông chữ trung bình 16px
#define UI_FONT_20 (&font_20) // Phông chữ tiêu đề nhỏ 20px
#define UI_FONT_24 (&font_24) // Phông chữ tiêu đề lớn 24px
#define UI_FONT_36 (&font_36) // Phông chữ số hiển thị 36px
#define UI_FONT_48 (&font_48) // Phông chữ số hiển thị cực lớn 48px (Watchface/Alarm)

/* ===================================================================
 *  BẢNG MÀU CHỦ ĐỀ GIAO DIỆN (Color Palette - Dark Theme)
 * =================================================================== */
#define COLOR_BG            lv_color_hex(0x000000)  // Nền đen tuyệt đối (OLED tối ưu điện năng)
#define COLOR_SURFACE       lv_color_hex(0x1C1C1E)  // Nền xám tối của thẻ (Card background)
#define COLOR_SURFACE_LT    lv_color_hex(0x2C2C2E)  // Nền xám sáng hơn khi nhấn giữ
#define COLOR_TEXT          lv_color_hex(0xFFFFFF)  // Màu chữ trắng chính
#define COLOR_TEXT_MUTED    lv_color_hex(0x888888)  // Màu chữ xám mờ phụ
#define COLOR_PRIMARY       lv_color_hex(0x0A84FF)  // Màu xanh dương chủ đạo của hệ thống
#define COLOR_GREEN         lv_color_hex(0x30D158)  // Màu xanh lá (dành cho chỉ số sức khỏe/vận động)
#define COLOR_RED           lv_color_hex(0xFF453A)  // Màu đỏ (dành cho cảnh báo/đo nhịp tim)
#define COLOR_ORANGE        lv_color_hex(0xFF9F0A)  // Màu cam (dành cho bấm giờ)
#define COLOR_PURPLE        lv_color_hex(0xBF5AF2)  // Màu tím (dành cho báo thức)
#define COLOR_CYAN          lv_color_hex(0x00BFFF)  // Màu xanh ngọc (dành cho bản đồ định vị GPS)
#define COLOR_FB_BLUE       lv_color_hex(0x1877F2)  // Màu xanh dương Facebook (dành cho thông báo mạng xã hội)
#define COLOR_GRAY          lv_color_hex(0xA9A9A9)  // Màu xám (cài đặt chung)
#define COLOR_BATTERY       lv_color_hex(0x32D74B)  // Màu xanh lá tươi chỉ báo pin đầy

/* ===================================================================
 *  CÁC HÀM GIAO DIỆN CHÍNH
 * =================================================================== */

/**
 * @brief Khởi tạo toàn bộ kiến trúc giao diện đồ họa cho Smartwatch
 * 
 * Hàm này được gọi trong app_main.c ngay khi luồng hiển thị LVGL port được khóa an toàn.
 */
void ui_init(void);

/**
 * @brief Gửi tín hiệu yêu cầu hệ thống đi vào chế độ ngủ sâu tắt màn hình
 */
void ui_request_deep_sleep(void);

/**
 * @brief Đặt giá trị độ sáng đèn nền LCD hiệu dụng hiện hành (duty cycle)
 */
void ui_backlight_set_active_duty(uint32_t duty);

/**
 * @brief Truy vấn giá trị độ sáng đèn nền LCD hiện hành
 */
uint32_t ui_backlight_get_active_duty(void);

/**
 * @brief Cấu hình chế độ hiển thị màu sắc biểu tượng trong Menu
 * @param mono true: hiển thị đơn sắc chủ đề, false: đa sắc mặc định
 * @param color Màu chủ đề đơn sắc được chọn
 */
void ui_menu_set_system_color(bool mono, lv_color_t color);

/**
 * @brief Cấu hình kiểu màn hình mặt đồng hồ chính (Watchface Style)
 * @param style_id Mã định danh kiểu mặt hiển thị
 */
void ui_watchface_set_style(int style_id);

/**
 * @brief Lấy mã kiểu màn hình mặt đồng hồ chính đang chạy
 */
int ui_watchface_get_style(void);

#endif // UI_H
