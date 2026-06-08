/**
 * @file battery_max17048.h
 * @brief Thư viện giao tiếp IC đo dung lượng pin MAX17048G (Fuel Gauge)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * MAX17048G là IC giám sát năng lượng pin Li-Po/Li-Ion qua thuật toán ModelGauge,
 * giúp đo điện áp và phần trăm dung lượng pin chính xác mà không cần trở điện trở shunt.
 */

#ifndef BATTERY_MAX17048_H
#define BATTERY_MAX17048_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Cấu trúc dữ liệu chứa thông tin chi tiết trạng thái pin
 */
typedef struct {
    float voltage_v;       // Điện áp hiện tại của viên pin (Đơn vị: V)
    float soc_percent;     // Trạng thái dung lượng pin (State of Charge, 0% đến 100%)
    float charge_rate;     // Tốc độ xả/nạp pin (% trên giờ, giá trị dương tương ứng đang sạc)
    bool  valid;           // Cờ báo dữ liệu đọc được từ IC là hợp lệ
} watch_battery_data_t;

/**
 * @brief Đọc thông số pin từ chip cảm biến MAX17048G qua bus I2C
 * @param[out] out Con trỏ lưu trữ dữ liệu pin đọc được
 * @return true Đọc thành công và dữ liệu hợp lệ
 * @return false Giao tiếp I2C thất bại hoặc dữ liệu lỗi
 */
bool battery_max17048_read(watch_battery_data_t *out);

#endif /* BATTERY_MAX17048_H */
