/**
 * @file ble_notify.c
 * @brief Hiện thực hệ thống BLE GATT Server nhận thông báo và đồng bộ dữ liệu
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Chi tiết thiết kế:
 * 1. Chuyển đổi mã nguồn kết nối BLE từ môi trường Arduino cũ sang thư viện chuẩn Native ESP-IDF Bluedroid.
 * 2. Cung cấp GATT Server với một Service UUID 128-bit cố định tương thích với App Flutter điện thoại.
 * 3. Hỗ trợ các đặc tả (Characteristics) hỗ trợ cả ghi (Write - điện thoại truyền lệnh xuống),
 *    đọc (Read) và cảnh báo (Notify/Indicate - gửi telemetry cảm biến từ đồng hồ lên app).
 * 4. Giải thuật cắt truyền dữ liệu lớn (BLE Track Chunk Transmission): Khi gửi hành trình định vị lớn
 *    (vài chục KB tọa độ), BLE MTU giới hạn chỉ cho phép gửi tối đa khoảng 200 bytes mỗi gói để tránh rớt gói dữ liệu.
 *    Driver sẽ chia nhỏ chuỗi và sử dụng một Task FreeRTOS phụ để gửi tuần tự với khoảng trễ 30ms giữa các gói.
 */

#include "ble_notify.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sensors.h"
#include "watch_settings.h"

static const char *TAG = "BLE_NOTIFY";

#define WATCH_BLE_CONN_ID_NONE          0xFFFF
#define WATCH_BLE_NOTIFY_CMD_MAX_LEN    512
#define WATCH_BLE_MUTEX_TIMEOUT_MS      50

/* ===================================================================
 *  CẤU HÌNH UUID - Phục vụ định danh dịch vụ kết nối với ứng dụng Flutter
 * =================================================================== */
/* Service UUID: 12345678-1234-1234-1234-123456789abc (Little Endian) */
static const uint8_t service_uuid128[16] = {
    0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
};

/* Characteristic UUID: abcd1234-ab12-cd34-ef56-123456789abc (Little Endian) */
static const uint8_t char_uuid128[16] = {
    0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x56, 0xef,
    0x34, 0xcd, 0x12, 0xab, 0x34, 0x12, 0xcd, 0xab
};

/* Các biến trạng thái kết nối và bảo vệ tương tranh */
static bool s_inited = false;
static bool s_enabled = false;
static bool s_connected = false;
static uint32_t s_connection_generation = 0; // Số thứ tự phiên kết nối
static SemaphoreHandle_t s_mutex = NULL;      // Mutex bảo vệ mảng thông báo nhận được
static SemaphoreHandle_t s_track_send_mutex = NULL; // Mutex đồng bộ gửi hành trình lớn

/* Mảng bộ đệm thông báo, hoạt động theo cơ chế dịch chuyển phần tử */
static watch_ble_notification_t s_notifications[WATCH_BLE_NOTIF_MAX];
static int s_notif_count = 0;
static uint32_t s_notif_overflow_count = 0;
static uint32_t s_next_notif_seq = 1;
static watch_ble_nav_data_t s_nav_data = {0};

static watch_ble_notif_new_cb_t s_new_cb = NULL;

/* Các handle quản lý giao tiếp GATTS của ESP32-S3 */
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static uint16_t s_descr_handle = 0;
static uint16_t s_conn_id = WATCH_BLE_CONN_ID_NONE;
static bool s_notify_enabled = false;

/**
 * @brief Cập nhật trạng thái liên kết BLE
 */
static void ble_set_link_state(bool connected, uint16_t conn_id, bool notify_enabled) {
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (connected) {
            s_connection_generation++;
        }
        s_connected = connected;
        s_conn_id = conn_id;
        s_notify_enabled = notify_enabled;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi: Không xin được Mutex để đổi trạng thái liên kết BLE!");
    }
}

static void ble_set_notify_enabled(bool enabled) {
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_notify_enabled = enabled;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi: Không xin được Mutex cấu hình thông báo BLE!");
    }
}

static bool ble_get_link_state(uint16_t *conn_id, bool *notify_enabled) {
    bool connected = false;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        connected = s_connected;
        if (conn_id) *conn_id = s_conn_id;
        if (notify_enabled) *notify_enabled = s_notify_enabled;
        xSemaphoreGive(s_mutex);
    } else {
        if (conn_id) *conn_id = WATCH_BLE_CONN_ID_NONE;
        if (notify_enabled) *notify_enabled = false;
    }
    return connected;
}

#define GATTS_APP_ID        0
#define GATTS_NUM_HANDLES   4  // Số lượng thuộc tính: Service, Char Decl, Char Val, CCCD
#define WATCH_BLE_RX_BUF_SIZE 512

/* ===================================================================
 *  CẤU HÌNH QUẢNG BÁ (BLE Advertising)
 * =================================================================== */
static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min       = 0x20,   // Khoảng cách phát quảng bá tối thiểu 20ms
    .adv_int_max       = 0x40,   // Khoảng cách phát quảng bá tối đa 40ms
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp    = false,
    .include_name    = true,      // Gửi kèm tên thiết bị quảng bá
    .include_txpower = true,      // Gửi kèm cường độ phát sóng TX Power
    .min_interval    = 0x0006,
    .max_interval    = 0x0010,
    .appearance      = 0x00C1,    // Mã định dạng thiết bị: Đồng hồ đeo tay (Generic Watch)
    .service_uuid_len = sizeof(service_uuid128),
    .p_service_uuid  = (uint8_t *)service_uuid128,
    .flag            = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* ===================================================================
 *  GIẢI MÃ DỮ LIỆU BLE NHẬN ĐƯỢC (BLE Data Parsing)
 * =================================================================== */

/**
 * @brief Tìm vị trí của ký tự phân tách phân vùng dữ liệu
 */
static int ble_find_char(const char *str, int len, char sep, int start) {
    for (int i = start; i < len; i++) {
        if (str[i] == sep) return i;
    }
    return -1;
}

/**
 * @brief Sao chép an toàn chuỗi con từ bộ đệm BLE thô
 */
static void ble_safe_copy(char *dst, int dst_size, const char *src, int start, int end) {
    int copy_len = end - start;
    if (copy_len <= 0) { dst[0] = '\0'; return; }
    if (copy_len >= dst_size) copy_len = dst_size - 1;
    memcpy(dst, src + start, copy_len);
    dst[copy_len] = '\0';
}

static void ble_copy_cstr(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/**
 * @brief Quy đổi tên hành động rẽ từ App thành mã enum tương ứng
 */
static watch_ble_nav_turn_t ble_parse_nav_turn(const char *turn) {
    if (!turn || turn[0] == '\0') return WATCH_BLE_NAV_TURN_UNKNOWN;
    if (strcmp(turn, "straight") == 0) return WATCH_BLE_NAV_TURN_STRAIGHT;
    if (strcmp(turn, "left") == 0) return WATCH_BLE_NAV_TURN_LEFT;
    if (strcmp(turn, "right") == 0) return WATCH_BLE_NAV_TURN_RIGHT;
    if (strcmp(turn, "slight_left") == 0) return WATCH_BLE_NAV_TURN_SLIGHT_LEFT;
    if (strcmp(turn, "slight_right") == 0) return WATCH_BLE_NAV_TURN_SLIGHT_RIGHT;
    if (strcmp(turn, "keep_left") == 0) return WATCH_BLE_NAV_TURN_KEEP_LEFT;
    if (strcmp(turn, "keep_right") == 0) return WATCH_BLE_NAV_TURN_KEEP_RIGHT;
    if (strcmp(turn, "uturn") == 0) return WATCH_BLE_NAV_TURN_UTURN;
    if (strcmp(turn, "roundabout") == 0) return WATCH_BLE_NAV_TURN_ROUNDABOUT;
    if (strcmp(turn, "arrive") == 0) return WATCH_BLE_NAV_TURN_ARRIVE;
    return WATCH_BLE_NAV_TURN_UNKNOWN;
}

/**
 * @brief Giải mã gói tin chỉ dẫn đường đi bản đồ: NAV|hướng_rẽ|tên_đường|khoảng_cách
 */
static void ble_parse_nav_data(const char *data, int len) {
    int sep1 = ble_find_char(data, len, '|', 0);
    int sep2 = (sep1 >= 0) ? ble_find_char(data, len, '|', sep1 + 1) : -1;
    int sep3 = (sep2 >= 0) ? ble_find_char(data, len, '|', sep2 + 1) : -1;
    bool has_eta = sep3 >= 0;

    if (sep2 < 0) {
        ESP_LOGW(TAG, "Định dạng NAV không hợp lệ: %.*s", len, data);
        return;
    }

    char current_turn[24];
    int road_end = has_eta ? sep3 : len;
    int eta_start = has_eta ? sep3 + 1 : len;
    int eta_end = len;

    ble_safe_copy(current_turn, sizeof(current_turn), data, sep1 + 1, sep2);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_nav_data.active = true;
        s_nav_data.current_turn = ble_parse_nav_turn(current_turn);
        ble_safe_copy(s_nav_data.primary_road, WATCH_BLE_NAV_ROAD_LEN, data, sep2 + 1, road_end);
        ble_safe_copy(s_nav_data.eta_to_turn, sizeof(s_nav_data.eta_to_turn), data, eta_start, eta_end);
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi Mutex khi phân tích dữ liệu NAV!");
    }

    ESP_LOGI(TAG, "Cập nhật NAV: Hướng=%s, Đường=%.*s, ETA=%.*s",
             current_turn, road_end - sep2 - 1, data + sep2 + 1,
             eta_end - eta_start, data + eta_start);
}

/**
 * @brief Giải mã gói tin đồng bộ thời gian từ GPS điện thoại: TIME|HH:MM:SS|DD/MM/YYYY
 */
static void ble_parse_time_sync(const char *data, int len) {
    int sep1 = ble_find_char(data, len, '|', 0);
    if (sep1 < 0) return;

    int sep2 = ble_find_char(data, len, '|', sep1 + 1);
    if (sep2 < 0) return;

    int sep3 = ble_find_char(data, len, '|', sep2 + 1);

    char time_str[16];
    ble_safe_copy(time_str, sizeof(time_str), data, sep1 + 1, sep2);

    int hour = 0, minute = 0, second = 0;
    if (sscanf(time_str, "%d:%d:%d", &hour, &minute, &second) != 3 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        ESP_LOGW(TAG, "Định dạng thời gian nhận được lỗi: %s", time_str);
        return;
    }

    int date_end = (sep3 > 0) ? sep3 : len;
    char date_str[16];
    ble_safe_copy(date_str, sizeof(date_str), data, sep2 + 1, date_end);

    int day = 1, month = 1, year = 2026;
    if (sscanf(date_str, "%d/%d/%d", &day, &month, &year) != 3 ||
        year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
        ESP_LOGW(TAG, "Định dạng ngày tháng nhận được lỗi: %s", date_str);
        return;
    }

    /* Thiết lập đồng bộ thời gian vào RTC hệ thống của ESP32-S3 */
    struct tm tm_info = {0};
    tm_info.tm_hour = hour;
    tm_info.tm_min  = minute;
    tm_info.tm_sec  = second;
    tm_info.tm_mday = day;
    tm_info.tm_mon  = month - 1;   // Định dạng tm: Tháng từ 0-11
    tm_info.tm_year = year - 1900; // Định dạng tm: Năm tính từ mốc 1900

    time_t t = mktime(&tm_info);
    if (t < 0) {
        ESP_LOGW(TAG, "Chuyển đổi thời gian mktime thất bại!");
        return;
    }

    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL); // Đặt thời gian cho hệ điều hành
    ESP_LOGI(TAG, "Đồng bộ thời gian thành công từ điện thoại: %02d:%02d:%02d  %02d/%02d/%04d",
             hour, minute, second, day, month, year);
}

/**
 * @brief Đẩy một thông báo mới vào đầu hàng đợi và xóa thông báo cũ nhất
 */
static void ble_add_notification(const char *app, const char *title, const char *content) {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Bỏ qua thông báo: Không xin được Mutex!");
        return;
    }
    bool dropped_oldest = (s_notif_count >= WATCH_BLE_NOTIF_MAX);

    /* Dịch toàn bộ thông báo hiện có xuống 1 chỉ mục để chừa vị trí 0 */
    for (int i = WATCH_BLE_NOTIF_MAX - 1; i > 0; i--) {
        s_notifications[i] = s_notifications[i - 1];
    }

    /* Điền dữ liệu thông báo mới vào vị trí đầu tiên (0) */
    ble_copy_cstr(s_notifications[0].app_name, WATCH_BLE_NOTIF_APP_LEN, app);
    ble_copy_cstr(s_notifications[0].title, WATCH_BLE_NOTIF_TITLE_LEN, title);
    ble_copy_cstr(s_notifications[0].content, WATCH_BLE_NOTIF_CONTENT_LEN, content);
    s_notifications[0].timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    s_notifications[0].seq_id = s_next_notif_seq++;
    if (s_next_notif_seq == 0) s_next_notif_seq = 1;
    s_notifications[0].is_new = true;

    if (s_notif_count < WATCH_BLE_NOTIF_MAX) s_notif_count++;
    if (dropped_oldest) s_notif_overflow_count++;

    watch_ble_notification_t copy = s_notifications[0];
    watch_ble_notif_new_cb_t cb = s_new_cb;

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "--- Nhận thông báo mới ---");
    ESP_LOGI(TAG, "Ứng dụng: %s", copy.app_name);
    ESP_LOGI(TAG, "Người gửi: %s", copy.title);
    ESP_LOGI(TAG, "Nội dung: %s", copy.content);

    /* Gọi hàm callback hiển thị UI ngoài vùng Mutex để tránh treo hệ thống */
    if (cb) {
        cb(&copy);
    }
}

/**
 * @brief Đọc và gửi dữ liệu tọa độ hành trình GPS (vết đường đi) lên điện thoại
 */
static void ble_send_current_track(void) {
    char *track = heap_caps_malloc(WATCH_GPS_TRACK_TEXT_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!track) track = malloc(WATCH_GPS_TRACK_TEXT_MAX);
    if (!track) {
        ESP_LOGE(TAG, "Lỗi cấp phát RAM lớn để đóng gói hành trình tọa độ!");
        return;
    }

    int len = watch_sensors_format_gps_track(track, WATCH_GPS_TRACK_TEXT_MAX);
    if (len > 0) {
        watch_ble_send_track_chunks(track);
    }
    free(track);
}

/**
 * @brief Định tuyến bóc tách gói dữ liệu thô ghi xuống từ điện thoại
 */
static void ble_parse_rx_data(const char *data, int len) {
    if (len == 9 && strncmp(data, "TRACK_REQ", 9) == 0) {
        ble_send_current_track(); // Điện thoại yêu cầu đồng bộ tọa độ GPS
        return;
    }

    if (len > 8 && strncmp(data, "OTA_URL|", 8) == 0) {
        char url[256];
        int copy_len = len - 8;
        if (copy_len >= (int)sizeof(url)) copy_len = sizeof(url) - 1;
        memcpy(url, data + 8, copy_len);
        url[copy_len] = '\0';
        esp_err_t err = watch_settings_set_ota_url(url); // Lưu link nạp phần mềm OTA mới nhận
        ESP_LOGI(TAG, "Cập nhật OTA URL qua BLE: %s", err == ESP_OK ? "Thành công" : esp_err_to_name(err));
        return;
    }

    if (len == 7 && strncmp(data, "NAV_END", 7) == 0) {
        watch_ble_notify_clear_nav(); // Điện thoại tắt bản đồ chỉ đường
        ESP_LOGI(TAG, "Phiên chỉ đường đã kết thúc");
        return;
    }

    int sep1 = ble_find_char(data, len, '|', 0);
    if (sep1 < 0) return;

    if (sep1 == 4 && strncmp(data, "TIME", 4) == 0) {
        ble_parse_time_sync(data, len); // Đồng bộ thời gian thực
        return;
    }

    if (sep1 == 3 && strncmp(data, "NAV", 3) == 0) {
        ble_parse_nav_data(data, len); // Chỉ đường bản đồ
        return;
    }

    int sep2 = ble_find_char(data, len, '|', sep1 + 1);
    if (sep2 < 0) return;

    char app[WATCH_BLE_NOTIF_APP_LEN];
    char title[WATCH_BLE_NOTIF_TITLE_LEN];
    char content[WATCH_BLE_NOTIF_CONTENT_LEN];

    ble_safe_copy(app, sizeof(app), data, 0, sep1);
    ble_safe_copy(title, sizeof(title), data, sep1 + 1, sep2);
    ble_safe_copy(content, sizeof(content), data, sep2 + 1, len);

    ble_add_notification(app, title, content); // Tin nhắn/thông báo thường
}

/* ===================================================================
 *  TRÌNH XỬ LÝ SỰ KIỆN GAP (GAP Event Handler)
 * =================================================================== */
static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "GAP: Cấu hình dữ liệu Adv thành công, bắt đầu phát sóng quảng bá...");
            esp_ble_gap_start_advertising(&s_adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP: Phát sóng quảng bá BLE thành công");
            } else {
                ESP_LOGE(TAG, "GAP: Phát sóng quảng bá thất bại! Mã lỗi: %d", param->adv_start_cmpl.status);
            }
            break;
        default:
            break;
    }
}

/* ===================================================================
 *  TRÌNH XỬ LÝ SỰ KIỆN GATTS (GATTS Event Handler)
 * =================================================================== */
static void ble_gatts_event_handler(esp_gatts_cb_event_t event,
                                    esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATTS: Đăng ký App thành công, app_id=%d, if=%d", param->reg.app_id, gatts_if);
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("Smartwatch_DATN"); // Đặt tên Bluetooth thiết bị hiển thị khi quét
            esp_ble_gap_config_adv_data(&s_adv_data);

            /* Khai báo Service UUID */
            static esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.uuid.len = ESP_UUID_LEN_128;
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid128, 16);
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLES);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(TAG, "GATTS: Khởi tạo Service thành công, handle=%d", param->create.service_handle);
            s_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(s_service_handle);

            /* Đăng ký Characteristic thuộc tính Đọc/Ghi/Báo */
            static esp_bt_uuid_t char_uuid;
            char_uuid.len = ESP_UUID_LEN_128;
            memcpy(char_uuid.uuid.uuid128, char_uuid128, 16);

            esp_gatt_char_prop_t prop = ESP_GATT_CHAR_PROP_BIT_READ |
                                        ESP_GATT_CHAR_PROP_BIT_WRITE |
                                        ESP_GATT_CHAR_PROP_BIT_NOTIFY;
     
            static uint8_t char_val_buffer[512] = {0};
            static esp_attr_value_t char_val = {
                .attr_max_len = sizeof(char_val_buffer),
                .attr_len     = 0,
                .attr_value   = char_val_buffer,
            };

            esp_ble_gatts_add_char(s_service_handle,
                                   &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   prop,
                                   &char_val,
                                   NULL);
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(TAG, "GATTS: Thêm đặc tính Characteristic thành công, handle=%d", param->add_char.attr_handle);
            s_char_handle = param->add_char.attr_handle;

            /* Cấu hình CCCD Descriptor cho phép Client bật cơ chế Notify */
            static esp_bt_uuid_t descr_uuid;
            descr_uuid.len = ESP_UUID_LEN_16;
            descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(s_service_handle,
                                          &descr_uuid,
                                          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                          NULL, NULL);
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            ESP_LOGI(TAG, "GATTS: Thêm thanh ghi CCCD thành công, handle=%d",
                     param->add_char_descr.attr_handle);
            s_descr_handle = param->add_char_descr.attr_handle;
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Thiết bị điện thoại đã kết nối! conn_id=%d", param->connect.conn_id);
            ble_set_link_state(true, param->connect.conn_id, false);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Ngắt kết nối với điện thoại. Lý do: 0x%x", param->disconnect.reason);
            ble_set_link_state(false, WATCH_BLE_CONN_ID_NONE, false);
            esp_ble_gap_start_advertising(&s_adv_params); // Trở lại quảng bá chờ kết nối lại
            break;

        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_descr_handle && param->write.len == 2) {
                uint16_t cccd = param->write.value[1] << 8 | param->write.value[0];
                bool notify_enabled = (cccd & 0x0001) != 0;
                ble_set_notify_enabled(notify_enabled);
                ESP_LOGI(TAG, "Bật/Tắt Notify từ Client: 0x%04X, notify=%s", cccd, notify_enabled ? "BẬT" : "TẮT");
            }

            /* Tiếp nhận dữ liệu văn bản ghi xuống đặc tính Characteristic */
            if (param->write.handle == s_char_handle && param->write.len > 0) {
                char buf[WATCH_BLE_RX_BUF_SIZE];
                int copy_len = param->write.len;
                if (copy_len >= (int)sizeof(buf)) copy_len = sizeof(buf) - 1;
                memcpy(buf, param->write.value, copy_len);
                buf[copy_len] = '\0';

                ESP_LOGI(TAG, "Dữ liệu BLE nhận được (%d bytes): %s", param->write.len, buf);
                ble_parse_rx_data(buf, copy_len);
            }

            if (param->write.need_rsp) {
                esp_gatt_rsp_t rsp = {0};
                rsp.attr_value.handle = param->write.handle;
                rsp.attr_value.len = 0;
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id,
                                            ESP_GATT_OK, &rsp);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "GATTS: Kích thước gói dữ liệu MTU được cấu hình = %d bytes", param->mtu.mtu);
            break;
        default:
            break;
    }
}

/* ===================================================================
 *  API CÔNG CỘNG (Public APIs)
 * =================================================================== */

esp_err_t watch_ble_notify_init(void) {
    if (s_inited) return ESP_OK;

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return ESP_ERR_NO_MEM;
    }
    if (!s_track_send_mutex) {
        s_track_send_mutex = xSemaphoreCreateMutex();
        if (!s_track_send_mutex) return ESP_ERR_NO_MEM;
    }

    /* Khởi động Bluetooth Controller thô ở chế độ BLE */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo BT Controller thất bại: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Kích hoạt BLE Mode thất bại: %s", esp_err_to_name(err));
        return err;
    }

    /* Khởi tạo thư viện giao thức Bluedroid */
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo Bluedroid thất bại: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Kích hoạt Bluedroid thất bại: %s", esp_err_to_name(err));
        return err;
    }

    /* Đăng ký các hàm sự kiện */
    esp_ble_gap_register_callback(ble_gap_event_handler);
    esp_ble_gatts_register_callback(ble_gatts_event_handler);
    esp_ble_gatts_app_register(GATTS_APP_ID);
 
    /* Đặt kích thước gói truyền tối đa 512 bytes */
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(512);
    if (local_mtu_ret != ESP_OK) {
        ESP_LOGE(TAG, "Cấu hình local MTU thất bại: %x", local_mtu_ret);
    }

    s_inited = true;
    ESP_LOGI(TAG, "Giao tiếp BLE GATT Server đã trực tuyến.");
    return ESP_OK;
}

esp_err_t watch_ble_notify_deinit(void) {
    if (!s_inited) return ESP_OK;
    s_inited = false;

    esp_ble_gap_stop_advertising();
    esp_bluedroid_disable();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            memset(&s_nav_data, 0, sizeof(s_nav_data));
            s_connected = false;
            s_conn_id = WATCH_BLE_CONN_ID_NONE;
            s_notify_enabled = false;
            s_notif_count = 0;
            s_notif_overflow_count = 0;
            memset(s_notifications, 0, sizeof(s_notifications));
            xSemaphoreGive(s_mutex);
        } else {
            ESP_LOGW(TAG, "Giải phóng BLE quá hạn Mutex; ép buộc đặt lại các biến trạng thái.");
            memset(&s_nav_data, 0, sizeof(s_nav_data));
            s_connected = false;
            s_conn_id = WATCH_BLE_CONN_ID_NONE;
            s_notify_enabled = false;
            s_notif_count = 0;
            s_notif_overflow_count = 0;
            memset(s_notifications, 0, sizeof(s_notifications));
        }
    }

    ESP_LOGI(TAG, "Bộ phát sóng BLE đã ngừng hoạt động để tiết kiệm pin.");
    return ESP_OK;
}

int watch_ble_notify_get_count(void) {
    if (!s_mutex) return 0;
    int count = 0;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        count = s_notif_count;
        xSemaphoreGive(s_mutex);
    }
    return count;
}

uint32_t watch_ble_notify_get_overflow_count(void) {
    if (!s_mutex) return 0;
    uint32_t count = 0;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        count = s_notif_overflow_count;
        xSemaphoreGive(s_mutex);
    }
    return count;
}

bool watch_ble_notify_get(int index, watch_ble_notification_t *out_notif) {
    if (!out_notif || !s_mutex) return false;

    bool ok = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (index >= 0 && index < s_notif_count) {
            *out_notif = s_notifications[index];
            ok = true;
        }
        xSemaphoreGive(s_mutex);
    }
    return ok;
}

bool watch_ble_notify_has_new(void) {
    if (!s_mutex) return false;
    bool has_new = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        for (int i = 0; i < s_notif_count; i++) {
            if (s_notifications[i].is_new) {
                has_new = true;
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return has_new;
}

void watch_ble_notify_mark_read(int index) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (index >= 0 && index < s_notif_count) {
            s_notifications[index].is_new = false;
        }
        xSemaphoreGive(s_mutex);
    }
}

bool watch_ble_notify_delete_by_seq_id(uint32_t seq_id) {
    if (!s_mutex) return false;

    bool deleted = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        for (int i = 0; i < s_notif_count; i++) {
            if (s_notifications[i].seq_id != seq_id) continue;

            /* Dồn các tin nhắn phía sau lên trên để lấp khoảng trống */
            for (int j = i; j < s_notif_count - 1; j++) {
                s_notifications[j] = s_notifications[j + 1];
            }
            s_notif_count--;
            memset(&s_notifications[s_notif_count], 0, sizeof(s_notifications[s_notif_count]));
            deleted = true;
            break;
        }
        xSemaphoreGive(s_mutex);
    }
    return deleted;
}

void watch_ble_notify_clear_all(void) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_notif_count = 0;
        s_notif_overflow_count = 0;
        memset(s_notifications, 0, sizeof(s_notifications));
        xSemaphoreGive(s_mutex);
    }
}

bool watch_ble_notify_is_connected(void) {
    return ble_get_link_state(NULL, NULL);
}

esp_err_t watch_bt_set_enabled(bool enabled) {
    if (enabled && !s_enabled) {
        ESP_LOGI(TAG, "Đang mở Bluetooth...");
        esp_err_t err = watch_ble_notify_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Mở Bluetooth thất bại: %s", esp_err_to_name(err));
            return err;
        }
        s_enabled = true;
        ESP_LOGI(TAG, "Bluetooth đã mở thành công");
    } else if (!enabled && s_enabled) {
        ESP_LOGI(TAG, "Đang tắt Bluetooth...");
        watch_ble_notify_deinit();
        s_enabled = false;
        ESP_LOGI(TAG, "Bluetooth đã tắt");
    }
    return ESP_OK;
}

bool watch_bt_is_enabled(void) {
    return s_enabled;
}

bool watch_bt_is_connected(void) {
    return s_enabled && watch_ble_notify_is_connected();
}

uint32_t watch_bt_get_connection_generation(void) {
    uint32_t generation = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        generation = s_connection_generation;
        xSemaphoreGive(s_mutex);
    }
    return generation;
}

void watch_ble_notify_set_new_cb(watch_ble_notif_new_cb_t cb) {
    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            s_new_cb = cb;
            xSemaphoreGive(s_mutex);
        } else {
            s_new_cb = cb;
        }
        return;
    }
    s_new_cb = cb;
}

bool watch_ble_notify_get_nav(watch_ble_nav_data_t *out_nav) {
    if (!out_nav || !s_mutex) return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        *out_nav = s_nav_data;
        xSemaphoreGive(s_mutex);
        return out_nav->active;
    }
    memset(out_nav, 0, sizeof(*out_nav));
    return false;
}

void watch_ble_notify_clear_nav(void) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLE_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        memset(&s_nav_data, 0, sizeof(s_nav_data));
        xSemaphoreGive(s_mutex);
    }
}

esp_err_t watch_ble_notify_send_cmd(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return ESP_ERR_INVALID_ARG;
    uint16_t conn_id = WATCH_BLE_CONN_ID_NONE;
    bool notify_enabled = false;
    
    if (!ble_get_link_state(&conn_id, &notify_enabled) || conn_id == WATCH_BLE_CONN_ID_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!notify_enabled) {
        ESP_LOGW(TAG, "Không thể truyền dữ liệu vì Client chưa kích hoạt cổng Notify!");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_gatts_if == ESP_GATT_IF_NONE || s_char_handle == 0) return ESP_ERR_INVALID_STATE;

    size_t raw_len = strlen(cmd);
    if (raw_len > WATCH_BLE_NOTIFY_CMD_MAX_LEN) return ESP_ERR_INVALID_ARG;
    uint16_t len = (uint16_t)raw_len;
    
    /* Gửi dữ liệu không đồng bộ lên điện thoại qua cơ chế Indicate */
    esp_err_t err = esp_ble_gatts_send_indicate(
        s_gatts_if, conn_id, s_char_handle, len, (uint8_t *)cmd, false);
        
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Đã gửi dữ liệu qua BLE: %s", cmd);
    } else {
        ESP_LOGW(TAG, "Gửi dữ liệu BLE thất bại: %s", esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Đóng gói dữ liệu chuỗi tọa độ lớn và truyền tải bằng các gói nhỏ chặn đồng bộ
 */
static void ble_send_track_chunks_blocking(const char *track) {
    if (!track || track[0] == '\0') return;

    const size_t chunk_size = 200; // Cắt nhỏ mỗi gói tối đa 200 bytes để truyền an toàn
    size_t len = strlen(track);
    size_t offset = 0;
    char *msg = malloc(512);
    if (!msg) {
        ESP_LOGE(TAG, "Lỗi cấp phát bộ nhớ đệm gửi gói hành trình!");
        return;
    }

    watch_ble_notify_send_cmd("TRACK_BEGIN"); // Gửi tín hiệu mở đầu đồng bộ
    while (offset < len) {
        size_t n = len - offset;
        if (n > chunk_size) n = chunk_size;
        
        /* Đóng gói mảnh dữ liệu hành trình: TRACK_CHUNK|offset|đoạn_chuỗi */
        snprintf(msg, 512, "TRACK_CHUNK|%lu|%.*s", (unsigned long)offset, (int)n, track + offset);
        watch_ble_notify_send_cmd(msg);
        
        offset += n;
        vTaskDelay(pdMS_TO_TICKS(30)); // Delay 30ms giữa các gói tránh nghẽn hàng đợi BLE
    }
    watch_ble_notify_send_cmd("TRACK_END"); // Gửi tín hiệu kết thúc đồng bộ
    free(msg);
}

/**
 * @brief Đa nhiệm FreeRTOS truyền dữ liệu hành trình song song với luồng chính
 */
static void ble_track_send_task(void *arg) {
    char *track = (char *)arg;
    if (s_track_send_mutex && xSemaphoreTake(s_track_send_mutex, portMAX_DELAY) == pdTRUE) {
        ble_send_track_chunks_blocking(track);
        xSemaphoreGive(s_track_send_mutex);
    }
    free(track);
    vTaskDelete(NULL); // Hủy tác vụ khi hoàn tất
}

void watch_ble_send_track_chunks(const char *track) {
    if (!track || track[0] == '\0') return;

    size_t len = strlen(track) + 1;
    char *copy = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) copy = malloc(len);
    if (!copy) {
        ESP_LOGE(TAG, "Lỗi cấp phát RAM lưu bản sao hành trình!");
        return;
    }
    memcpy(copy, track, len);

    /* Khởi chạy luồng bất đồng bộ truyền dữ liệu lớn để tránh khóa cứng UI đồng hồ */
    if (xTaskCreate(ble_track_send_task, "ble_track_send_task", 4096, copy, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Lỗi: Không thể khởi tạo Task gửi hành trình!");
        free(copy);
    }
}
