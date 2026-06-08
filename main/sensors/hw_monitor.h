/**
 * @file hw_monitor.h
 * @brief Thư viện giám sát tài nguyên phần cứng (Battery, IMU, Heart Rate, Nút bấm)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Quản lý đa nhiệm giám sát phần cứng thời gian thực bằng FreeRTOS Task chạy trên Core 0.
 * Cung cấp các hàm I2C helper dùng chung có cơ chế tự phục hồi lỗi nghẽn bus (I2C Bus Recovery),
 * và các API lấy dữ liệu an toàn đồng bộ (Thread-safe) bằng Mutex.
 */

#ifndef HW_MONITOR_H
#define HW_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#include "battery_max17048.h"
#include "imu_bmi270.h"
#include "hr_max30102.h"

/**
 * @brief Đọc khối dữ liệu từ thanh ghi của thiết bị ngoại vi I2C (Helper dùng chung)
 * @param addr Địa chỉ I2C của thiết bị slave
 * @param reg Địa chỉ thanh ghi cần đọc
 * @param[out] data Bộ đệm lưu trữ dữ liệu đọc được
 * @param len Số lượng byte cần đọc
 * @return esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công)
 */
esp_err_t hw_monitor_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

/**
 * @brief Ghi khối dữ liệu vào thanh ghi của thiết bị ngoại vi I2C (Helper dùng chung)
 * @param addr Địa chỉ I2C của thiết bị slave
 * @param reg Địa chỉ thanh ghi cần ghi
 * @param data Con trỏ dữ liệu cần ghi
 * @param len Số lượng byte cần ghi
 * @return esp_err_t Trạng thái thực hiện
 */
esp_err_t hw_monitor_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

/**
 * @brief Khởi chạy luồng giám sát phần cứng và khởi tạo bus I2C cho cảm biến
 * @return esp_err_t Trạng thái khởi động dịch vụ
 */
esp_err_t watch_hw_monitor_start(void);

/**
 * @brief Lấy dữ liệu pin mới nhất một cách an toàn giữa các luồng (Thread-safe)
 * @param[out] out Cấu trúc chứa dữ liệu pin đầu ra
 * @return true Dữ liệu hợp lệ
 * @return false Thiết bị chưa sẵn sàng hoặc dữ liệu lỗi
 */
bool watch_hw_monitor_get_battery(watch_battery_data_t *out);

/**
 * @brief Lấy dữ liệu cảm biến chuyển động IMU mới nhất (Thread-safe)
 * @param[out] out Cấu trúc chứa dữ liệu góc quay/gia tốc và bước chân đầu ra
 * @return true Dữ liệu hợp lệ
 */
bool watch_hw_monitor_get_imu(watch_imu_data_t *out);

/**
 * @brief Lấy dữ liệu đo nhịp tim & SpO2 mới nhất (Thread-safe)
 * @param[out] out Cấu trúc chứa dữ liệu nhịp sinh học đầu ra
 * @return true Có dữ liệu đo hợp lệ
 */
bool watch_hw_monitor_get_hr(watch_hr_data_t *out);

/**
 * @brief Bật hoặc tắt chức năng đo nhịp tim thủ công
 * @param enabled true: Bật nguồn cảm biến đo mạch, false: Tắt nguồn cảm biến
 */
void watch_hw_monitor_set_hr_measure_enabled(bool enabled);

/**
 * @brief Đặt lại bộ đếm bước đi của hệ thống về 0
 */
void watch_hw_monitor_reset_steps(void);

/**
 * @brief Truy vấn cấu hình tính năng Đánh thức màn hình nhanh (Quick Wake)
 * @return true Tính năng đang bật
 */
bool watch_hw_monitor_is_quick_wake_enabled(void);

/**
 * @brief Cài đặt bật hoặc tắt tính năng Đánh thức nhanh
 */
void watch_hw_monitor_set_quick_wake_enabled(bool enabled);

/**
 * @brief Lấy thời gian chờ tắt màn hình (giây)
 * @return int Số giây chờ tắt, 0 nghĩa là luôn luôn sáng
 */
int watch_hw_monitor_get_screen_timeout(void);

/**
 * @brief Cấu hình thời gian chờ tắt màn hình (giây)
 */
void watch_hw_monitor_set_screen_timeout(int seconds);

#endif /* HW_MONITOR_H */
