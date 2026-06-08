/**
 * @file watch_activity_log.h
 * @brief Thư viện lưu trữ nhật ký hoạt động thể thao (Activity Logger)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hỗ trợ ghi lại lịch sử các phiên tập luyện (chế độ đạp xe, đi bộ) bao gồm thời lượng,
 * số bước chân, quãng đường tích lũy, tốc độ trung bình và vết chuỗi tọa độ bản đồ vào NVS Flash.
 */

#ifndef WATCH_ACTIVITY_LOG_H
#define WATCH_ACTIVITY_LOG_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define WATCH_ACTIVITY_LOG_MAX    3       // Số lượng phiên tập luyện lưu trữ tối đa trong Flash NVS
#define WATCH_ACTIVITY_ROUTE_MAX  49152   // Kích thước tối đa của chuỗi lưu trữ tọa độ hành trình trong RAM

/**
 * @brief Cấu trúc dữ liệu ghi chép một phiên tập luyện thể thao
 */
typedef struct {
    uint32_t timestamp;                         // Mốc thời gian bắt đầu tập luyện (Giây UTC)
    uint8_t sport_id;                           // Loại hình vận động (1: Đi bộ, 2: Đạp xe)
    uint32_t duration_sec;                      // Tổng thời gian luyện tập (giây)
    uint32_t steps;                             // Tổng số bước chân đi được
    float distance_km;                          // Tổng quãng đường tích lũy (km)
    float avg_speed_kmh;                        // Vận tốc di chuyển trung bình (km/h)
    char route[WATCH_ACTIVITY_ROUTE_MAX];       // Vết đường đi định dạng chuỗi văn bản gửi lên app
} watch_activity_record_t;

/**
 * @brief Khởi tạo vùng nhớ lưu trữ nhật ký và nạp lịch sử cũ từ Flash NVS
 * @return esp_err_t Trạng thái thực hiện
 */
esp_err_t watch_activity_log_init(void);

/**
 * @brief Lưu trữ thêm một bản ghi hoạt động thể thao mới
 * @param record Con trỏ chứa bản ghi hoạt động mới cần lưu
 */
esp_err_t watch_activity_log_add(const watch_activity_record_t *record);

/**
 * @brief Truy vấn tổng số lượng bản ghi hoạt động thể thao hiện có trong bộ nhớ
 */
size_t watch_activity_log_count(void);

/**
 * @brief Đọc danh sách các bản ghi hoạt động thể thao ra bộ đệm ngoài
 * @param[out] out Mảng sao chép dữ liệu xuất
 * @param max_records Số lượng bản ghi tối đa muốn lấy
 * @return size_t Số lượng bản ghi thực tế sao chép được
 */
size_t watch_activity_log_get(watch_activity_record_t *out, size_t max_records);

#endif // WATCH_ACTIVITY_LOG_H
