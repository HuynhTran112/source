/**
 * @file watch_settings.c
 * @brief Hiện thực hệ thống quản lý, đọc/ghi cấu hình cài đặt đồng hồ vào Flash NVS
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Chi tiết thiết kế:
 * 1. Lưu trữ an toàn: Mọi thiết lập được đóng gói cùng với mã nhận dạng kiểm tra (Magic Header)
 *    và phiên bản cấu trúc (Version) giúp phần mềm phát hiện và ngăn chặn việc đọc dữ liệu lỗi thời
 *    sau khi cập nhật firmware.
 * 2. Ràng buộc an toàn (Sanitization): Hàm `watch_settings_sanitize` kiểm tra chéo các giá trị cài đặt
 *    nhận về từ người dùng (ví dụ: giới hạn độ sáng từ 5-100%, cường độ rung từ 0-2) nhằm tránh ghi đè
 *    các giá trị rác gây lỗi hệ thống.
 * 3. Hỗ trợ lưu trữ chuỗi nhị phân Blob và văn bản (WiFi SSID, Password, OTA URL).
 */

#include "watch_settings.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "SETTINGS_STORE";

#define WATCH_SETTINGS_NS       "watch"       // Tên không gian tên NVS cho toàn bộ đồng hồ
#define WATCH_SETTINGS_KEY      "settings"    // Khóa lưu cấu hình chính
#define WATCH_ALARMS_KEY        "alarms"      // Khóa lưu danh sách báo thức
#define WATCH_WIFI_SSID_KEY     "wifi_ssid"   // Khóa lưu tên mạng Wi-Fi
#define WATCH_WIFI_PASS_KEY     "wifi_pass"   // Khóa lưu mật khẩu Wi-Fi
#define WATCH_OTA_URL_KEY       "ota_url"     // Khóa lưu link máy chủ OTA
#define WATCH_SETTINGS_MAGIC    0x57435354U   // Mã xác thực Magic ("WCST")
#define WATCH_SETTINGS_VERSION  1U            // Phiên bản dữ liệu cấu hình

/* Cấu trúc đóng gói dữ liệu ghi Flash */
typedef struct {
    uint32_t magic;
    uint8_t version;
    watch_settings_t settings;
} watch_settings_record_t;

/* Cài đặt mặc định ban đầu của thiết bị */
static watch_settings_t s_settings = {
    .brightness_pct = 78,               // Độ sáng màn hình mặc định ~78%
    .screen_timeout_sec = 15,           // Màn hình tự tắt sau 15 giây
    .quick_wake_enabled = false,        // Mặc định tắt tính năng gõ màn hình thức dậy
    .vibration_enabled = true,          // Mặc định cho phép rung còi báo
    .vibration_strength = 1,            // Rung ở mức trung bình (1)
    .language = 0,                      // Ngôn ngữ mặc định: Tiếng Anh (0)
    .watchface_style = 1,               // Giao diện mặt đồng hồ số 1
    .icons_monochrome = false,          // Biểu tượng menu màu sắc đầy đủ
    .icon_color = WATCH_COLOR_DEFAULT,
};

/**
 * @brief Ràng buộc và chuẩn hóa các giá trị cấu hình đầu vào để tránh giá trị lỗi
 */
static watch_settings_t watch_settings_sanitize(watch_settings_t in) {
    if (in.brightness_pct < 5)   in.brightness_pct = 5;
    if (in.brightness_pct > 100) in.brightness_pct = 100;
    if (in.vibration_strength > 2) in.vibration_strength = 1;
    if (in.language > 1)         in.language = 0;
    if (in.watchface_style > 2)  in.watchface_style = 1;
    if (in.icon_color > WATCH_COLOR_ORANGE) in.icon_color = WATCH_COLOR_DEFAULT;
    if (!in.icons_monochrome)    in.icon_color = WATCH_COLOR_DEFAULT;
    return in;
}

/**
 * @brief Ghi toàn bộ dữ liệu cấu hình s_settings hiện hành vào NVS Flash
 */
static esp_err_t watch_settings_save_all(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    watch_settings_record_t rec = {
        .magic = WATCH_SETTINGS_MAGIC,
        .version = WATCH_SETTINGS_VERSION,
        .settings = watch_settings_sanitize(s_settings),
    };
    
    err = nvs_set_blob(nvs, WATCH_SETTINGS_KEY, &rec, sizeof(rec));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_init(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Lỗi mở không gian lưu trữ NVS: %s", esp_err_to_name(err));
        return err;
    }

    watch_settings_record_t rec;
    size_t len = sizeof(rec);
    err = nvs_get_blob(nvs, WATCH_SETTINGS_KEY, &rec, &len);
    nvs_close(nvs);

    /* Xác thực kiểm tra dữ liệu đọc được */
    if (err == ESP_OK && len == sizeof(rec) &&
        rec.magic == WATCH_SETTINGS_MAGIC &&
        rec.version == WATCH_SETTINGS_VERSION) {
        s_settings = watch_settings_sanitize(rec.settings);
        ESP_LOGI(TAG, "Nạp cấu hình cài đặt thành công từ Flash NVS!");
        return ESP_OK;
    }

    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Lỗi nạp cấu hình cài đặt cũ: %s. Khởi tạo lại cấu hình mặc định...", esp_err_to_name(err));
    }
    return watch_settings_save_all(); // Ghi đè cấu hình mặc định nếu lỗi hoặc chưa có
}

const watch_settings_t *watch_settings_get(void) {
    return &s_settings;
}

esp_err_t watch_settings_set_brightness_pct(uint8_t pct) {
    s_settings.brightness_pct = pct;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_screen_timeout(uint16_t seconds) {
    s_settings.screen_timeout_sec = seconds;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_quick_wake(bool enabled) {
    s_settings.quick_wake_enabled = enabled;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_vibration_enabled(bool enabled) {
    s_settings.vibration_enabled = enabled;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_vibration_strength(uint8_t strength) {
    s_settings.vibration_strength = strength;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_language(uint8_t language) {
    s_settings.language = language;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_watchface_style(uint8_t style) {
    s_settings.watchface_style = style;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_icon_color(bool monochrome, watch_color_id_t color) {
    s_settings.icons_monochrome = monochrome;
    s_settings.icon_color = color;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_load_alarm_blob(void *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t stored_len = len;
    err = nvs_get_blob(nvs, WATCH_ALARMS_KEY, data, &stored_len);
    nvs_close(nvs);
    
    if (err == ESP_OK && stored_len != len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return err;
}

esp_err_t watch_settings_save_alarm_blob(const void *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, WATCH_ALARMS_KEY, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_get_wifi_credentials(char *ssid, size_t ssid_size,
                                              char *password, size_t password_size) {
    if (!ssid || ssid_size == 0) return ESP_ERR_INVALID_ARG;
    ssid[0] = '\0';
    if (password && password_size > 0) password[0] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t len = ssid_size;
    err = nvs_get_str(nvs, WATCH_WIFI_SSID_KEY, ssid, &len);
    
    if (err == ESP_OK && password && password_size > 0) {
        len = password_size;
        esp_err_t pass_err = nvs_get_str(nvs, WATCH_WIFI_PASS_KEY, password, &len);
        if (pass_err == ESP_ERR_NVS_NOT_FOUND) {
            password[0] = '\0';
        } else if (pass_err != ESP_OK) {
            err = pass_err;
        }
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_set_wifi_credentials(const char *ssid, const char *password) {
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, WATCH_WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, WATCH_WIFI_PASS_KEY, password ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_clear_wifi_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    esp_err_t ssid_err = nvs_erase_key(nvs, WATCH_WIFI_SSID_KEY);
    esp_err_t pass_err = nvs_erase_key(nvs, WATCH_WIFI_PASS_KEY);
    
    if (ssid_err == ESP_ERR_NVS_NOT_FOUND) ssid_err = ESP_OK;
    if (pass_err == ESP_ERR_NVS_NOT_FOUND) pass_err = ESP_OK;
    
    if (ssid_err == ESP_OK && pass_err == ESP_OK) {
        err = nvs_commit(nvs);
    } else {
        err = ssid_err != ESP_OK ? ssid_err : pass_err;
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_get_ota_url(char *url, size_t url_size) {
    if (!url || url_size == 0) return ESP_ERR_INVALID_ARG;
    url[0] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t len = url_size;
    err = nvs_get_str(nvs, WATCH_OTA_URL_KEY, url, &len);
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_set_ota_url(const char *url) {
    if (!url || url[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, WATCH_OTA_URL_KEY, url);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
