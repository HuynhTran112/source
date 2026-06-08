/**
 * @file hr_max30102.h
 * @brief Thư viện giao tiếp và xử lý dữ liệu cảm biến nhịp tim/SpO2 MAX30102
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Driver điều khiển cảm biến đo nồng độ oxy trong máu và nhịp tim quang học MAX30102 (Maxim Integrated)
 * qua giao tiếp I2C. Dữ liệu được đo đạc theo phương pháp quang phổ thể tích đồ (PPG).
 */

#ifndef HR_MAX30102_H
#define HR_MAX30102_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Cấu trúc dữ liệu chứa kết quả đo nhịp tim và nồng độ oxy trong máu (SpO2)
 */
typedef struct {
    float heart_rate; // Giá trị nhịp tim (BPM - Nhịp trên phút)
    float spo2;       // Nồng độ oxy trong máu (% SpO2)
    uint32_t ir_raw;  // Cường độ ánh sáng hồng ngoại (IR) thô đọc được từ ADC cảm biến
    uint32_t red_raw; // Cường độ ánh sáng đỏ (RED) thô đọc được từ ADC cảm biến
    uint8_t quality;  // Đánh giá chất lượng tín hiệu PPG (0% - 100%)
    bool valid;       // Cờ báo kết quả đo hợp lệ (đã phát hiện áp ngón tay ổn định)
} watch_hr_data_t;

/**
 * @brief Khởi tạo cảm biến MAX30102, thiết lập dòng LED và tần số lấy mẫu FIFO
 * @return true Khởi tạo cảm biến thành công
 * @return false Lỗi giao tiếp I2C hoặc sai lệch mã định danh PART_ID
 */
bool hr_max30102_init(void);

/**
 * @brief Đọc gói dữ liệu mới từ bộ đệm FIFO cảm biến và chạy thuật toán tính toán nhịp tim/SpO2
 * @param[out] out Con trỏ cấu trúc lưu kết quả đo đạc gần nhất
 * @return true Tính toán thành công và có dữ liệu hợp lệ
 * @return false Không có ngón tay chạm cảm biến hoặc dữ liệu nhiễu chưa hội tụ
 */
bool hr_max30102_read(watch_hr_data_t *out);

/**
 * @brief Đưa cảm biến về chế độ ngủ sâu (Shutdown) để tiết kiệm năng lượng và dọn dẹp bộ đệm
 */
void hr_max30102_shutdown(void);

#endif /* HR_MAX30102_H */
