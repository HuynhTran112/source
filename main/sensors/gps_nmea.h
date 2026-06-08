/**
 * @file gps_nmea.h
 * @brief Thư viện phân tích cú pháp chuỗi dữ liệu GPS chuẩn NMEA 0183
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hỗ trợ trích xuất thông tin định vị từ các bản tin GPS phổ biến như GPRMC, GPGGA, GPGSA
 * của module GPS GT-U8 truyền qua cổng UART.
 */

#ifndef GPS_NMEA_H
#define GPS_NMEA_H

#include <stdbool.h>

/**
 * @brief Cấu trúc lưu trữ dữ liệu định vị sau khi phân tích cú pháp NMEA
 */
typedef struct {
    bool has_rmc;             // Đã nhận được bản tin $GPRMC (chứa vĩ độ, kinh độ, tốc độ)
    bool has_gga;             // Đã nhận được bản tin $GPGGA (chứa số vệ tinh, chất lượng kết nối)
    bool fix_valid;           // Tín hiệu định vị hợp lệ (đã khóa được vệ tinh)
    double latitude_deg;      // Vĩ độ (Đơn vị: Độ thập phân - Decimal Degrees)
    double longitude_deg;     // Kinh độ (Đơn vị: Độ thập phân - Decimal Degrees)
    float speed_kmh;          // Tốc độ di chuyển thực tế (Đơn vị: km/h)
    unsigned int fix_quality; // Chất lượng định vị (0: Không khóa, 1: GPS Fix, 2: DGPS,...)
    unsigned int satellites;  // Số lượng vệ tinh đang kết nối thành công
    float pdop;               // Chỉ số sai số vị trí không gian (Position Dilution of Precision)
    float hdop;               // Chỉ số sai số vị trí mặt phẳng ngang (Horizontal Dilution of Precision)
    float vdop;               // Chỉ số sai số vị trí độ cao dọc (Vertical Dilution of Precision)
} watch_gps_nmea_data_t;

/**
 * @brief Kiểm tra tính hợp lệ của mã kiểm lỗi checksum (XOR) cuối chuỗi NMEA
 * @param line Chuỗi bản tin NMEA cần kiểm tra (Bắt đầu bằng '$')
 * @return true Checksum hợp lệ
 * @return false Checksum bị sai lệch do nhiễu đường truyền UART
 */
bool watch_gps_nmea_has_valid_checksum(const char *line);

/**
 * @brief Phân tích cú pháp một dòng bản tin NMEA cụ thể
 * @param[in] line Bản tin NMEA thô (ví dụ: "$GPRMC,083559.00,A,2101.5432,N,10548.1234,E,0.027,...*6C")
 * @param[out] out Con trỏ cấu trúc chứa dữ liệu đầu ra sau phân tích
 * @return true Phân tích thành công bản tin được hỗ trợ (RMC, GGA, GSA)
 * @return false Bản tin không hợp lệ, lỗi checksum hoặc không hỗ trợ
 */
bool watch_gps_nmea_parse_line(char *line, watch_gps_nmea_data_t *out);

#endif // GPS_NMEA_H
