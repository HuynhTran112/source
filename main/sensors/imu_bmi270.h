/**
 * @file imu_bmi270.h
 * @brief Thư viện điều khiển cảm biến gia tốc và con quay hồi chuyển IMU BMI270
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Driver quản lý cảm biến chuyển động 6-trục BMI270 (Bosch Sensortec) kết nối qua I2C,
 * cung cấp dữ liệu gia tốc (accel), tốc độ góc (gyro) và số bước chân (đếm bước mềm).
 */

#ifndef IMU_BMI270_H
#define IMU_BMI270_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Cấu trúc lưu trữ dữ liệu đo lường từ cảm biến IMU BMI270
 */
typedef struct {
    float accel_x;       // Gia tốc hướng X (Đơn vị: G, 1G = 9.81m/s^2)
    float accel_y;       // Gia tốc hướng Y (Đơn vị: G)
    float accel_z;       // Gia tốc hướng Z (Đơn vị: G)
    float gyro_x;        // Tốc độ góc quanh trục X (Đơn vị: dps - độ trên giây)
    float gyro_y;        // Tốc độ góc quanh trục Y (Đơn vị: dps)
    float gyro_z;        // Tốc độ góc quanh trục Z (Đơn vị: dps)
    uint32_t step_count; // Số bước chân người dùng đã đi (Tính bằng thuật toán lọc động)
    bool valid;          // Cờ báo dữ liệu đọc được từ cảm biến là hợp lệ
} watch_imu_data_t;

/**
 * @brief Khởi tạo phần cứng, nạp firmware cấu hình ASIC 8KB cho BMI270
 * @return true Khởi tạo và nạp firmware thành công
 * @return false Thất bại (sai Chip ID, lỗi giao tiếp I2C hoặc lỗi ASIC)
 */
bool imu_bmi270_init(void);

/**
 * @brief Đọc giá trị gia tốc, con quay hồi chuyển thô và cập nhật giải thuật đếm bước chân
 * @param[out] out Con trỏ lưu trữ dữ liệu IMU đã quy đổi đơn vị vật lý
 * @return true Đọc thành công
 * @return false Lỗi giao tiếp I2C hoặc thiết bị chưa khởi tạo
 */
bool imu_bmi270_read(watch_imu_data_t *out);

/**
 * @brief Đặt lại (Reset) bộ đếm bước chân và các biến lọc nhiễu về trạng thái ban đầu
 */
void imu_bmi270_reset_steps(void);

#endif /* IMU_BMI270_H */
