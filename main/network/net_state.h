/**
 * @file net_state.h
 * @brief Thư viện quản lý kết nối và trạng thái mạng Wi-Fi
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Quản lý khởi tạo ngăn xếp TCP/IP, điều khiển kết nối mạng Station (STA),
 * thực hiện quét điểm truy cập (Wi-Fi Scan) và nạp cấu hình Wi-Fi tự động để phục vụ nạp OTA.
 */

#ifndef NET_STATE_H
#define NET_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#define WATCH_NET_WIFI_SCAN_TASK_NAME "wifi_scan" // Tên tác vụ thực hiện quét mạng Wi-Fi

/**
 * @brief Định nghĩa các trạng thái kết nối mạng Wi-Fi
 */
typedef enum {
    WATCH_NET_WIFI_CONNECT_IDLE = 0,      // Trạng thái rảnh (chưa kết nối)
    WATCH_NET_WIFI_CONNECT_CONNECTING,    // Đang trong tiến trình bắt tay kết nối
    WATCH_NET_WIFI_CONNECT_SUCCESS,       // Kết nối thành công và đã nhận được IP
    WATCH_NET_WIFI_CONNECT_FAILED,        // Kết nối thất bại (sai mật khẩu, ngoài vùng phủ sóng)
} watch_net_wifi_connect_state_t;

/**
 * @brief Khởi tạo hệ thống mạng TCP/IP và đăng ký các Event Handler Wi-Fi/IP
 * 
 * Hàm này được gọi duy nhất 1 lần khi đồng hồ khởi động (app_main.c).
 */
void watch_net_init(void);

/**
 * @brief Quét danh sách các điểm truy cập Wi-Fi khả dụng (Hàm chặn luồng)
 * @param[out] out_records Mảng chứa danh sách các AP quét được
 * @param[in,out] out_num Khai báo dung lượng mảng ban đầu, trả về số AP thực tế tìm thấy
 * @return esp_err_t Trạng thái quét (ESP_OK nếu thành công)
 */
esp_err_t watch_net_wifi_scan(wifi_ap_record_t *out_records, uint16_t *out_num);

/**
 * @brief Ra lệnh kết nối đến một mạng Wi-Fi chỉ định
 * @param ssid Tên điểm phát Wi-Fi
 * @param password Mật khẩu truy cập
 * @return esp_err_t Trạng thái gửi lệnh thành công
 */
esp_err_t watch_net_wifi_connect(const char *ssid, const char *password);

/**
 * @brief Tự động kết nối đến mạng Wi-Fi đã lưu lần cuối trong NVS Flash
 */
esp_err_t watch_net_wifi_connect_saved(void);

/**
 * @brief Ngắt kết nối Wi-Fi hiện hành và dọn dẹp các cờ trạng thái liên quan
 */
esp_err_t watch_net_wifi_disconnect(void);

/**
 * @brief Đọc thông tin mạng Wi-Fi đã kết nối lần cuối từ bộ nhớ đệm (Thread-safe)
 * @param[out] ssid Bộ đệm chứa SSID trả về
 * @param ssid_size Kích thước bộ đệm SSID
 * @param[out] password Bộ đệm chứa mật khẩu trả về
 * @param password_size Kích thước bộ đệm mật khẩu
 * @return true Có thông tin Wi-Fi đã lưu trước đó
 */
bool watch_net_get_last_wifi_credentials(char *ssid, size_t ssid_size,
                                         char *password, size_t password_size);

/**
 * @brief Bắt đầu ghi nhận tiến trình kết nối Wi-Fi mới
 */
void watch_net_begin_wifi_connect_attempt(const char *ssid, const char *password);

/**
 * @brief Kiểm tra xem đồng hồ hiện tại có đang kết nối mạng Wi-Fi ổn định hay không
 * @return true Đã có kết nối Wi-Fi và có IP hợp lệ
 */
bool watch_net_is_wifi_connected(void);

/**
 * @brief Lấy trạng thái kết nối Wi-Fi chi tiết hiện hành (Thread-safe)
 */
watch_net_wifi_connect_state_t watch_net_get_wifi_connect_state(void);

/**
 * @brief Dọn dẹp trạng thái kết nối Wi-Fi sau khi xử lý xong sự kiện
 */
void watch_net_clear_wifi_connect_state(void);

/**
 * @brief Lấy tên SSID của mạng Wi-Fi đang kết nối hiện hành
 * @return const char* Con trỏ chứa tên mạng SSID
 */
const char *watch_net_get_connected_ssid(void);

/**
 * @brief So sánh xem SSID hiện hành có khớp với SSID truyền vào không
 */
bool watch_net_is_connected_ssid(const char *ssid);

#endif // NET_STATE_H
