/**
 * @file ble_notify.h
 * @brief Thư viện giao tiếp Bluetooth Low Energy (BLE) GATT Server
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Quản lý kết nối BLE GATT Server để nhận thông báo (cuộc gọi, tin nhắn), đồng bộ thời gian (RTC),
 * nhận chỉ đường chỉ dẫn bản đồ (Google Maps) từ điện thoại gửi xuống, và đồng bộ dữ liệu hành trình
 * GPS ngược trở lại điện thoại.
 */

#ifndef BLE_NOTIFY_H
#define BLE_NOTIFY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Cấu hình giới hạn bộ nhớ thông báo === */
#define WATCH_BLE_NOTIF_MAX           10   // Số lượng thông báo tối đa lưu giữ trong bộ đệm RAM đồng hồ
#define WATCH_BLE_NOTIF_APP_LEN       32   // Độ dài tối đa của tên ứng dụng (ví dụ: Zalo, Facebook)
#define WATCH_BLE_NOTIF_TITLE_LEN     64   // Độ dài tối đa tên người gửi tin nhắn hoặc cuộc gọi
#define WATCH_BLE_NOTIF_CONTENT_LEN   256  // Độ dài tối đa nội dung thông báo hiển thị

/**
 * @brief Cấu trúc thông tin của một thông báo nhận được
 */
typedef struct {
    char     app_name[WATCH_BLE_NOTIF_APP_LEN];
    char     title[WATCH_BLE_NOTIF_TITLE_LEN];
    char     content[WATCH_BLE_NOTIF_CONTENT_LEN];
    uint32_t timestamp;                         // Mốc thời gian hệ thống lúc nhận được thông báo
    uint32_t seq_id;                            // Mã ID tăng dần duy nhất để phân loại thông báo
    bool     is_new;                            // Cờ báo thông báo mới chưa được người dùng đọc
} watch_ble_notification_t;

/**
 * @brief Con trỏ hàm callback xử lý sự kiện có thông báo mới đổ chuông
 */
typedef void (*watch_ble_notif_new_cb_t)(const watch_ble_notification_t *notif);

/**
 * @brief Danh mục các hình mũi tên chỉ đường điều hướng (Navigation Turn Type)
 */
typedef enum {
    WATCH_BLE_NAV_TURN_UNKNOWN = 0,      // Không xác định
    WATCH_BLE_NAV_TURN_STRAIGHT,         // Đi thẳng
    WATCH_BLE_NAV_TURN_LEFT,             // Rẽ trái
    WATCH_BLE_NAV_TURN_RIGHT,            // Rẽ phải
    WATCH_BLE_NAV_TURN_SLIGHT_LEFT,      // Chếch trái nhẹ
    WATCH_BLE_NAV_TURN_SLIGHT_RIGHT,     // Chếch phải nhẹ
    WATCH_BLE_NAV_TURN_KEEP_LEFT,        // Đi bám bên trái
    WATCH_BLE_NAV_TURN_KEEP_RIGHT,       // Đi bám bên phải
    WATCH_BLE_NAV_TURN_UTURN,            // Quay đầu xe
    WATCH_BLE_NAV_TURN_ROUNDABOUT,       // Đi vào vòng xuyến
    WATCH_BLE_NAV_TURN_ARRIVE            // Đã tới điểm đến
} watch_ble_nav_turn_t;

#define WATCH_BLE_NAV_ROAD_LEN 64

/**
 * @brief Cấu trúc lưu trữ dữ liệu chỉ đường đồng bộ từ điện thoại
 */
typedef struct {
    bool active;                                // Trạng thái điều hướng bản đồ đang chạy
    watch_ble_nav_turn_t current_turn;          // Hướng rẽ hiện hành
    char primary_road[WATCH_BLE_NAV_ROAD_LEN];  // Tên con đường hiện tại đang di chuyển
    char eta_to_turn[32];                       // Khoảng cách hoặc thời gian chờ rẽ tiếp theo (ví dụ: "200m", "2 phút")
} watch_ble_nav_data_t;

/* === CÁC API PHẦN CỨNG VÀ KẾT NỐI BLE === */

/**
 * @brief Khởi tạo phần cứng Bluetooth Controller, Bluedroid, đăng ký GAP/GATTS
 * @return esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công)
 */
esp_err_t watch_ble_notify_init(void);

/**
 * @brief Hủy đăng ký và tắt nguồn hoàn toàn bộ phát sóng Bluetooth để tiết kiệm pin
 */
esp_err_t watch_ble_notify_deinit(void);

/**
 * @brief Đếm tổng số thông báo đang có trong bộ nhớ tạm
 * @return int Số lượng thông báo (0 đến WATCH_BLE_NOTIF_MAX)
 */
int watch_ble_notify_get_count(void);

/**
 * @brief Lấy số lượng thông báo cũ đã bị đẩy ghi đè do đầy bộ đệm
 */
uint32_t watch_ble_notify_get_overflow_count(void);

/**
 * @brief Lấy thông báo theo vị trí xếp hạng (0: thông báo mới nhất)
 * @param index Vị trí chỉ mục
 * @param[out] out_notif Con trỏ cấu trúc sao chép dữ liệu ra
 * @return true Sao chép thành công
 */
bool watch_ble_notify_get(int index, watch_ble_notification_t *out_notif);

/**
 * @brief Kiểm tra xem đồng hồ có tin nhắn/thông báo nào chưa đọc hay không
 */
bool watch_ble_notify_has_new(void);

/**
 * @brief Đánh dấu một thông báo là đã đọc
 */
void watch_ble_notify_mark_read(int index);

/**
 * @brief Xóa một thông báo cụ thể dựa vào mã ID duy nhất seq_id
 * @return true Xóa thành công
 */
bool watch_ble_notify_delete_by_seq_id(uint32_t seq_id);

/**
 * @brief Xóa toàn bộ thông báo đã lưu trong bộ đệm RAM của đồng hồ
 */
void watch_ble_notify_clear_all(void);

/**
 * @brief Kiểm tra xem đồng hồ hiện tại có liên kết BLE thành công với điện thoại không
 */
bool watch_ble_notify_is_connected(void);

/**
 * @brief Thiết lập hàm callback khi có thông báo cuộc gọi/tin nhắn mới đổ xuống
 */
void watch_ble_notify_set_new_cb(watch_ble_notif_new_cb_t cb);

/**
 * @brief Đọc thông tin hướng dẫn chỉ đường điều hướng hiện hành (Thread-safe)
 * @param[out] out_nav Con trỏ cấu trúc dữ liệu xuất ra
 * @return true Đang chạy chỉ đường
 */
bool watch_ble_notify_get_nav(watch_ble_nav_data_t *out_nav);

/**
 * @brief Xóa trạng thái chỉ đường điều hướng khi kết thúc hành trình
 */
void watch_ble_notify_clear_nav(void);

/**
 * @brief Gửi lệnh hoặc chuỗi dữ liệu ngược từ đồng hồ lên app điện thoại qua cơ chế GATTS Notify
 * @param cmd Chuỗi ký tự lệnh cần truyền đi
 */
esp_err_t watch_ble_notify_send_cmd(const char *cmd);

/**
 * @brief Tách chuỗi vết tọa độ hành trình thành từng mảnh nhỏ để truyền tải BLE an toàn
 */
void watch_ble_send_track_chunks(const char *track);

/**
 * @brief Bật hoặc tắt trạng thái Bluetooth của hệ thống
 */
esp_err_t watch_bt_set_enabled(bool enabled);

/**
 * @brief Kiểm tra trạng thái Bluetooth có đang bật hay không
 */
bool watch_bt_is_enabled(void);

/**
 * @brief Kiểm tra trạng thái Bluetooth đã kết nối thành công với điện thoại hay chưa
 */
bool watch_bt_is_connected(void);

/**
 * @brief Lấy thế hệ/thứ tự kết nối Bluetooth hiện hành để quản lý trạng thái
 */
uint32_t watch_bt_get_connection_generation(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_NOTIFY_H
