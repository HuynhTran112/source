/**
 * @file sensors.h
 * @brief Bộ quản lý và phân tích số liệu định vị GPS toàn cục
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Quản lý đa nhiệm UART để đọc và xử lý bản tin GPS thô từ module GT-U8,
 * thực hiện tính toán khoảng cách di chuyển thực tế (theo giải thuật Haversine)
 * và lưu trữ vết đường đi (GPS Track Log) dạng chuỗi để đồng bộ sang điện thoại.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Cấu trúc dữ liệu chứa số liệu thống kê GPS trực quan (GPS Metrics)
 */
typedef struct {
    bool fix_valid;           // Tín hiệu định vị hợp lệ (đã khóa)
    bool uart_seen;           // Phát hiện có tín hiệu xung điện trên UART
    bool nmea_seen;           // Phát hiện nhận đúng cấu trúc chuỗi NMEA
    double latitude_deg;      // Vĩ độ hiện tại (Độ thập phân)
    double longitude_deg;     // Kinh độ hiện tại (Độ thập phân)
    float speed_kmh;          // Vận tốc tức thời đo được (km/h)
    float total_distance_km;  // Tổng quãng đường tích lũy được trong phiên luyện tập (km)
    uint32_t last_rx_ms;      // Mốc thời gian (TickCount) nhận byte cuối cùng từ UART
    uint32_t raw_bytes;       // Tổng số byte thô đã nhận được từ khi bật GPS
    uint32_t nmea_lines;      // Tổng số dòng bản tin NMEA đã phân tích
    unsigned int satellites;  // Số lượng vệ tinh đang kết nối
    unsigned int fix_quality; // Cấp độ định vị (0: No fix, 1: GPS, 2: DGPS)
    float pdop;               // Sai số không gian
    float hdop;               // Sai số mặt phẳng ngang
    float vdop;               // Sai số độ cao dọc
    int current_baud;         // Baudrate kết nối UART hiện tại (bps)
} watch_gps_metrics_t;

/**
 * @brief Cấu trúc điểm tọa độ đơn lẻ lưu vết hành trình
 */
typedef struct {
    double latitude_deg;  // Vĩ độ
    double longitude_deg; // Kinh độ
} watch_gps_track_point_t;

/* Cấu hình giới hạn bộ nhớ lưu trữ hành trình */
#define WATCH_GPS_TRACK_MAX_POINTS   2048   // Số điểm tọa độ tối đa có thể lưu trữ trong RAM
#define WATCH_GPS_TRACK_TEXT_MAX     49152  // Kích thước chuỗi văn bản tối đa để truyền dữ liệu (2048 điểm * ~24 ký tự)

/**
 * @brief Khởi chạy Task FreeRTOS đọc dữ liệu UART và bật nguồn module GPS
 */
void watch_sensors_start_gps_imu(void);

/**
 * @brief Dừng Task đọc GPS, tắt driver UART để tiết kiệm pin tối đa
 */
void watch_sensors_stop_gps_imu(void);

/**
 * @brief Lấy các số liệu thống kê GPS hiện hành (Thread-safe)
 * @param[out] out Con trỏ cấu trúc chứa dữ liệu xuất
 * @return true Đọc thành công
 */
bool watch_sensors_get_gps_metrics(watch_gps_metrics_t *out);

/**
 * @brief Đặt lại (Reset) quãng đường tích lũy và xóa sạch nhật ký vết đường đi
 */
void watch_sensors_reset_gps_track(void);

/**
 * @brief Kiểm tra xem module định vị GPS có đang hoạt động hay không
 * @return true Task GPS đang chạy
 */
bool watch_sensors_is_gps_active(void);

/**
 * @brief Lấy mảng các điểm tọa độ vết hành trình đã lưu (Thread-safe)
 * @param[out] out Bộ đệm chứa các điểm tọa độ xuất ra
 * @param max_points Số lượng điểm tối đa muốn lấy
 * @return size_t Số lượng điểm tọa độ thực tế đã sao chép
 */
size_t watch_sensors_get_gps_track(watch_gps_track_point_t *out, size_t max_points);

/**
 * @brief Định dạng mảng tọa độ thành chuỗi text ghép nối nhau bằng dấu chấm phẩy ";" để truyền BLE
 * @param[out] out Bộ đệm chuỗi xuất ra (định dạng: "lat1,lon1;lat2,lon2;...")
 * @param out_size Kích thước bộ đệm xuất
 * @return int Tổng số ký tự ghi vào chuỗi thành công
 */
int watch_sensors_format_gps_track(char *out, size_t out_size);

#endif // SENSORS_H
