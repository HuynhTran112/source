/**
 * @file watch_settings.h
 * @brief Thư viện quản lý cấu hình và cài đặt hệ thống của đồng hồ
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Quản lý lưu trữ/nạp dữ liệu cấu hình người dùng (độ sáng, độ rung, thời gian tắt màn hình,
 * ngôn ngữ, giao diện watchface) và thông tin mạng Wi-Fi, danh sách báo thức vào phân vùng Flash NVS.
 */

#ifndef WATCH_SETTINGS_H
#define WATCH_SETTINGS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Định nghĩa các mã màu chủ đề của đồng hồ
 */
typedef enum {
    WATCH_COLOR_DEFAULT = 0, // Mặc định (Đen nền sáng xanh)
    WATCH_COLOR_GREEN,       // Xanh lá
    WATCH_COLOR_RED,         // Đỏ
    WATCH_COLOR_PURPLE,      // Tím
    WATCH_COLOR_ORANGE,      // Cam
} watch_color_id_t;

/**
 * @brief Cấu trúc lưu trữ các thông số cài đặt hệ thống (Watch Settings)
 */
typedef struct {
    uint8_t brightness_pct;       // Độ sáng đèn nền màn hình (5% đến 100%)
    uint16_t screen_timeout_sec;  // Thời gian chờ tắt màn hình (giây, 0: không tắt)
    bool quick_wake_enabled;      // Bật/tắt tính năng gõ màn hình thức dậy
    bool vibration_enabled;       // Bật/tắt động cơ rung
    uint8_t vibration_strength;   // Cường độ rung (0: yếu, 1: trung bình, 2: mạnh)
    uint8_t language;             // Ngôn ngữ hệ thống (0: Tiếng Anh, 1: Tiếng Việt)
    uint8_t watchface_style;      // Kiểu mặt đồng hồ hiển thị
    bool icons_monochrome;        // Bật/tắt hiển thị biểu tượng đơn sắc
    watch_color_id_t icon_color;  // Màu sắc chủ đề được chọn
} watch_settings_t;

/**
 * @brief Khởi động bộ nhớ cài đặt, nạp cấu hình cũ từ Flash NVS, nếu chưa có thì tạo mới
 * @return esp_err_t Trạng thái thực thi
 */
esp_err_t watch_settings_init(void);

/**
 * @brief Lấy con trỏ tham chiếu đến cấu trúc cài đặt hiện hành
 * @return const watch_settings_t* Con trỏ cấu hình
 */
const watch_settings_t *watch_settings_get(void);

/* === CÁC API THAY ĐỔI CẤU HÌNH HỆ THỐNG === */
esp_err_t watch_settings_set_brightness_pct(uint8_t pct);
esp_err_t watch_settings_set_screen_timeout(uint16_t seconds);
esp_err_t watch_settings_set_quick_wake(bool enabled);
esp_err_t watch_settings_set_vibration_enabled(bool enabled);
esp_err_t watch_settings_set_vibration_strength(uint8_t strength);
esp_err_t watch_settings_set_language(uint8_t language);
esp_err_t watch_settings_set_watchface_style(uint8_t style);
esp_err_t watch_settings_set_icon_color(bool monochrome, watch_color_id_t color);

/* === CÁC API LƯU TRỮ DANH SÁCH BÁO THỨC (ALARM BLOB) === */
esp_err_t watch_settings_load_alarm_blob(void *data, size_t len);
esp_err_t watch_settings_save_alarm_blob(const void *data, size_t len);

/* === CÁC API QUẢN LÝ TÀI KHOẢN WI-FI === */
esp_err_t watch_settings_get_wifi_credentials(char *ssid, size_t ssid_size,
                                              char *password, size_t password_size);
esp_err_t watch_settings_set_wifi_credentials(const char *ssid, const char *password);
esp_err_t watch_settings_clear_wifi_credentials(void);

/* === CÁC API QUẢN LÝ LINK NẠP OTA === */
esp_err_t watch_settings_get_ota_url(char *url, size_t url_size);
esp_err_t watch_settings_set_ota_url(const char *url);

#endif // WATCH_SETTINGS_H
