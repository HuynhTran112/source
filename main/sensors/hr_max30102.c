/**
 * @file hr_max30102.c
 * @brief Hiện thực thuật toán xử lý tín hiệu PPG từ cảm biến MAX30102
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hướng dẫn thuật toán chi tiết:
 * 1. Đọc dữ liệu từ hàng đợi FIFO của MAX30102 qua giao tiếp I2C ở tần số lấy mẫu 50 Hz.
 * 2. Lưu trữ dữ liệu ánh sáng Đỏ (RED) và Hồng ngoại (IR) thô vào cửa sổ trượt (Rolling Window) chứa 100 mẫu (tương đương 2 giây dữ liệu).
 * 3. Tách thành phần một chiều (DC) bằng cách tính trung bình cộng (Mean) của cửa sổ trượt.
 * 4. Phát hiện sự diện diện ngón tay (Finger Detection): Nếu cường độ trung bình IR < 12,000, coi như không chạm cảm biến.
 * 5. Tách thành phần xoay chiều (AC) đại diện cho nhịp mạch đập bằng độ lệch chuẩn (RMS).
 * 6. Tính toán chỉ số SpO2 dựa trên tỉ lệ hấp thụ quang phổ R = (AC_red / DC_red) / (AC_ir / DC_ir),
 *    quy đổi bằng công thức hiệu chỉnh thực nghiệm: SpO2 = 110.0f - 25.0f * R.
 * 7. Phát hiện đỉnh xung mạch (Peak Detection): Lọc đỉnh cục bộ (Local Maxima) trên trục IR kết hợp với khoảng
 *    cách tối thiểu (Dead-time) giữa hai nhịp đập để tính tần số nhịp tim (BPM).
 */

#include "hr_max30102.h"
#include "hw_monitor.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

#define MAX30102_I2C_ADDR          0x57  // Địa chỉ giao tiếp I2C của chip MAX30102
#define MAX30102_REG_INTR_STATUS_1 0x00  // Thanh ghi báo ngắt 1 (Mất kết nối, FIFO đầy)
#define MAX30102_REG_INTR_STATUS_2 0x01  // Thanh ghi báo ngắt 2 (Cảnh báo nhiệt độ)
#define MAX30102_REG_FIFO_WR_PTR   0x04  // Con trỏ ghi dữ liệu FIFO (viết bởi IC)
#define MAX30102_REG_OVF_COUNTER   0x05  // Bộ đếm tràn bộ đệm FIFO
#define MAX30102_REG_FIFO_RD_PTR   0x06  // Con trỏ đọc dữ liệu FIFO (đọc bởi MCU)
#define MAX30102_REG_FIFO_DATA     0x07  // Cổng đọc/ghi dữ liệu từ hàng đợi FIFO
#define MAX30102_REG_FIFO_CONFIG   0x08  // Thanh ghi cấu hình chia sẻ bộ đệm FIFO
#define MAX30102_REG_MODE_CONFIG   0x09  // Cấu hình chế độ hoạt động (Tắt nguồn, chỉ đo nhịp tim, đo cả SpO2)
#define MAX30102_REG_SPO2_CONFIG   0x0A  // Cấu hình tần số lấy mẫu và độ phân giải ADC
#define MAX30102_REG_LED1_PA       0x0C  // Cấu hình cường độ dòng điện phát của LED hồng ngoại
#define MAX30102_REG_LED2_PA       0x0D  // Cấu hình cường độ dòng điện phát của LED đỏ
#define MAX30102_REG_PART_ID       0xFF  // Thanh ghi đọc phiên bản chip ID cảm biến

#define MAX30102_PART_ID_VAL       0x15  // Giá trị PART_ID mặc định khẳng định sự tồn tại của MAX30102
#define MAX30102_FIFO_DEPTH        32    // Kích thước bộ đệm FIFO tối đa của chip (32 mẫu)
#define MAX30102_SAMPLE_RATE_HZ    50    // Tần số lấy mẫu cấu hình: 50 Hz (50 mẫu / giây)
#define MAX30102_WINDOW_SAMPLES    100   // Kích thước cửa sổ lưu trữ dữ liệu (100 mẫu tương đương 2 giây)
#define MAX30102_MIN_FINGER_IR     12000UL // Ngưỡng cường độ IR tối thiểu để phát hiện ngón tay chạm cảm biến
#define MAX30102_MIN_AC_IR         60.0  // Độ lệch xoay chiều tối thiểu để tránh nhiễu đứng yên
#define MAX30102_LED_CURRENT_PA    0xC8  // Giá trị dòng điện LED cấp (Cấu hình ~40mA giúp xuyên thấu mô ngón tay tốt)

static const char *TAG = "HR_MAX30102";

/* Các mảng cửa sổ trượt lưu mẫu dữ liệu cho xử lý DSP */
static uint32_t s_ir_window[MAX30102_WINDOW_SAMPLES];
static uint32_t s_red_window[MAX30102_WINDOW_SAMPLES];
static size_t s_window_count;        // Số lượng mẫu hiện có trong bộ đệm
static size_t s_window_pos;          // Con trỏ vị trí chèn mẫu mới
static bool s_initialized;           // Cờ báo trạng thái khởi tạo cảm biến
static uint32_t s_latest_red;        // Giá trị Red thô cập nhật gần nhất
static uint32_t s_latest_ir;         // Giá trị IR thô cập nhật gần nhất

/**
 * @brief Ghi giá trị 8-bit vào thanh ghi của MAX30102
 */
static esp_err_t max30102_write_u8(uint8_t reg, uint8_t value) {
    return hw_monitor_i2c_write_reg(MAX30102_I2C_ADDR, reg, &value, 1);
}

/**
 * @brief Đọc giá trị 8-bit từ thanh ghi của MAX30102
 */
static esp_err_t max30102_read_u8(uint8_t reg, uint8_t *value) {
    return hw_monitor_i2c_read_reg(MAX30102_I2C_ADDR, reg, value, 1);
}

/**
 * @brief Đẩy cặp mẫu dữ liệu quang PPG mới thu được vào cửa sổ trượt
 */
static void max30102_push_sample(uint32_t red, uint32_t ir) {
    s_latest_red = red;
    s_latest_ir = ir;
    s_red_window[s_window_pos] = red;
    s_ir_window[s_window_pos] = ir;
    s_window_pos = (s_window_pos + 1U) % MAX30102_WINDOW_SAMPLES;
    if (s_window_count < MAX30102_WINDOW_SAMPLES) {
        s_window_count++;
    }
}

/**
 * @brief Sắp xếp lại dữ liệu trong cửa sổ trượt theo thứ tự thời gian từ cũ đến mới
 * @return size_t Số lượng mẫu được trả về
 */
static size_t max30102_ordered_samples(uint32_t *red, uint32_t *ir, size_t max_len) {
    size_t count = s_window_count < max_len ? s_window_count : max_len;
    size_t start = (s_window_count == MAX30102_WINDOW_SAMPLES) ? s_window_pos : 0;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % MAX30102_WINDOW_SAMPLES;
        red[i] = s_red_window[idx];
        ir[i] = s_ir_window[idx];
    }
    return count;
}

/**
 * @brief Đọc toàn bộ mẫu sẵn có từ hàng đợi FIFO phần cứng của MAX30102
 * @return true Đọc thành công
 * @return false Lỗi truyền thông I2C
 */
static bool max30102_read_fifo(void) {
    uint8_t wr = 0;
    uint8_t rd = 0;
    
    /* Đọc con trỏ ghi (Write Pointer) và con trỏ đọc (Read Pointer) của FIFO */
    if (max30102_read_u8(MAX30102_REG_FIFO_WR_PTR, &wr) != ESP_OK ||
        max30102_read_u8(MAX30102_REG_FIFO_RD_PTR, &rd) != ESP_OK) {
        return false;
    }

    /* Tính toán số lượng mẫu dữ liệu chưa được xử lý trong FIFO */
    uint8_t samples = (wr >= rd) ? (wr - rd) : (MAX30102_FIFO_DEPTH + wr - rd);
    if (samples > MAX30102_FIFO_DEPTH) samples = MAX30102_FIFO_DEPTH;

    for (uint8_t i = 0; i < samples; i++) {
        uint8_t raw[6] = {0};
        /* Mỗi mẫu dữ liệu PPG gồm 6 byte (3 byte ánh sáng RED, 3 byte ánh sáng IR) */
        if (hw_monitor_i2c_read_reg(MAX30102_I2C_ADDR, MAX30102_REG_FIFO_DATA,
                                    raw, sizeof(raw)) != ESP_OK) {
            return false;
        }

        /* Giải mã dữ liệu ADC 18-bit chứa trong 3 bytes */
        uint32_t red = (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2]) & 0x03FFFF;
        uint32_t ir = (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5]) & 0x03FFFF;
        max30102_push_sample(red, ir);
    }

    return true;
}

/**
 * @brief Giải thuật xử lý tín hiệu PPG để trích xuất nhịp tim và trị số SpO2
 * @param[out] out Lưu trữ cấu trúc kết quả đo
 * @return true Dữ liệu hợp lệ và hội tụ
 * @return false Dữ liệu chưa ổn định hoặc không có tay đặt trên cảm biến
 */
static bool max30102_estimate(watch_hr_data_t *out) {
    uint32_t red[MAX30102_WINDOW_SAMPLES];
    uint32_t ir[MAX30102_WINDOW_SAMPLES];
    size_t n = max30102_ordered_samples(red, ir, MAX30102_WINDOW_SAMPLES);
    
    /* Yêu cầu ít nhất 25 mẫu đầu tiên (0.5 giây dữ liệu) để bắt đầu ước lượng sơ bộ */
    if (n < 25) return false;

    out->red_raw = s_latest_red;
    out->ir_raw = s_latest_ir;
    out->quality = 0;
    out->valid = false;

    /* 1. Tính toán thành phần một chiều (DC) là trị số trung bình của dải mẫu */
    double red_mean = 0.0;
    double ir_mean = 0.0;
    uint32_t ir_min = UINT32_MAX;
    uint32_t ir_max = 0;

    for (size_t i = 0; i < n; i++) {
        red_mean += red[i];
        ir_mean += ir[i];
        if (ir[i] < ir_min) ir_min = ir[i];
        if (ir[i] > ir_max) ir_max = ir[i];
    }
    red_mean /= (double)n;
    ir_mean /= (double)n;

    /* Kiểm tra có ngón tay đặt lên không bằng cách so sánh cường độ tia hồng ngoại hấp thụ */
    if (ir_mean < MAX30102_MIN_FINGER_IR || ir_max <= ir_min) {
        return false;
    }

    /* 2. Tính toán thành phần xoay chiều (AC) bằng cách tính độ lệch chuẩn RMS */
    double red_ac_sq = 0.0;
    double ir_ac_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        double red_delta = (double)red[i] - red_mean;
        double ir_delta = (double)ir[i] - ir_mean;
        red_ac_sq += red_delta * red_delta;
        ir_ac_sq += ir_delta * ir_delta;
    }

    double red_ac = sqrt(red_ac_sq / (double)n);
    double ir_ac = sqrt(ir_ac_sq / (double)n);
    
    /* Lọc nhiễu: Nếu biên độ dao động AC quá nhỏ thì bỏ qua dữ liệu (coi như ngón tay không đập mạch) */
    if (red_mean <= 0.0 || ir_mean <= 0.0 || ir_ac < MAX30102_MIN_AC_IR) {
        return false;
    }

    /* 3. Tính toán tỉ số hấp thụ R và quy đổi giá trị SpO2 */
    double ratio = (red_ac / red_mean) / (ir_ac / ir_mean);
    float spo2 = (float)(110.0 - (25.0 * ratio));
    
    /* Ràng buộc giá trị sinh lý thực tế nồng độ oxy */
    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 70.0f)  spo2 = 70.0f;

    /* 4. Thuật toán phát hiện đỉnh xung (Peak Detection) trên kênh hồng ngoại (IR) */
    uint32_t threshold = ir_min + ((ir_max - ir_min) * 50U / 100U); // Ngưỡng so sánh động 50%
    int peaks[8];
    int peak_count = 0;
    int last_peak = -MAX30102_SAMPLE_RATE_HZ;

    for (size_t i = 1; i + 1 < n && peak_count < (int)(sizeof(peaks) / sizeof(peaks[0])); i++) {
        /* Điều kiện đỉnh cục bộ: ir[i] lớn hơn 2 mẫu lân cận, vượt ngưỡng động và cách biệt
         * đỉnh trước ít nhất 0.36s (tương ứng nhịp tim tối đa ~166 BPM để lọc nhiễu sóng dicrotic) */
        if (ir[i] > threshold && ir[i] > ir[i - 1] && ir[i] >= ir[i + 1] &&
            ((int)i - last_peak) >= (MAX30102_SAMPLE_RATE_HZ * 36 / 100)) {
            peaks[peak_count++] = (int)i;
            last_peak = (int)i;
        }
    }

    /* Đòi hỏi phát hiện ít nhất 2 mạch đập để tính nhịp tim */
    if (peak_count < 2) return false;

    /* 5. Tính toán nhịp tim trung bình trong cửa sổ đo */
    float interval_sum = 0.0f;
    for (int i = 1; i < peak_count; i++) {
        interval_sum += (float)(peaks[i] - peaks[i - 1]);
    }
    float avg_interval = interval_sum / (float)(peak_count - 1);
    float bpm = 60.0f * (float)MAX30102_SAMPLE_RATE_HZ / avg_interval;
    
    /* Giới hạn lọc sinh lý nhịp tim người */
    if (bpm < 35.0f || bpm > 220.0f) return false;

    /* 6. Đánh giá chất lượng tín hiệu dựa trên tỉ lệ AC/DC của tia IR */
    float signal_quality = (float)((ir_ac / ir_mean) * 10000.0);
    if (signal_quality > 100.0f) signal_quality = 100.0f;
    if (signal_quality < 1.0f)   signal_quality = 1.0f;

    out->heart_rate = bpm;
    out->spo2 = spo2;
    out->quality = (uint8_t)signal_quality;
    out->valid = true;
    return true;
}

bool hr_max30102_init(void) {
    if (s_initialized) return true;

    uint8_t part_id = 0;
    if (max30102_read_u8(MAX30102_REG_PART_ID, &part_id) != ESP_OK) {
        ESP_LOGE(TAG, "Giao tiếp lỗi: Không thể đọc thanh ghi PART_ID!");
        return false;
    }

    if (part_id != MAX30102_PART_ID_VAL) {
        ESP_LOGE(TAG, "Sai ID thiết bị: Mong muốn 0x%02X, Đọc được 0x%02X",
                 MAX30102_PART_ID_VAL, part_id);
        return false;
    }

    /* Thực hiện thiết lập lại phần mềm (Reset cấu hình) */
    (void)max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Đọc dọn dẹp các cờ ngắt cũ của cảm biến */
    uint8_t status = 0;
    (void)max30102_read_u8(MAX30102_REG_INTR_STATUS_1, &status);
    (void)max30102_read_u8(MAX30102_REG_INTR_STATUS_2, &status);

    /* Cấu hình các thanh ghi cảm biến:
     * 1. Xóa các con trỏ FIFO về 0.
     * 2. FIFO_CONFIG (0x08): Lấy trung bình 4 mẫu, tự động cuốn và bật ngắt khi gần đầy.
     * 3. SPO2_CONFIG (0x0A): Lấy mẫu dải đo ADC 18-bit đầy đủ, tần số 50 Hz.
     * 4. LED1/2_PA (0x0C/0x0D): Cấp dòng phát sáng thích hợp cho LED hồng ngoại và đỏ.
     * 5. MODE_CONFIG (0x09): Đưa cảm biến hoạt động ở chế độ đo cả RED và IR (SpO2 Mode = 0x03).
     */
    if (max30102_write_u8(MAX30102_REG_FIFO_WR_PTR, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_OVF_COUNTER, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_FIFO_RD_PTR, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_FIFO_CONFIG, 0x5F) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_SPO2_CONFIG, 0x23) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT_PA) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT_PA) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x03) != ESP_OK) {
        ESP_LOGE(TAG, "Ghi tham số cấu hình thanh ghi cảm biến thất bại!");
        return false;
    }

    /* Dọn dẹp bộ đệm dữ liệu phần mềm */
    memset(s_ir_window, 0, sizeof(s_ir_window));
    memset(s_red_window, 0, sizeof(s_red_window));
    s_window_count = 0;
    s_window_pos = 0;
    s_latest_red = 0;
    s_latest_ir = 0;
    s_initialized = true;

    ESP_LOGI(TAG, "MAX30102 đã sẵn sàng hoạt động (ID = 0x%02X)", part_id);
    return true;
}

bool hr_max30102_read(watch_hr_data_t *out) {
    if (!out || !s_initialized) return false;

    /* Đọc dữ liệu từ FIFO phần cứng và tính toán nhịp tim/SpO2 */
    if (!max30102_read_fifo() || !max30102_estimate(out)) {
        out->heart_rate = 0.0f;
        out->spo2 = 0.0f;
        out->red_raw = s_latest_red;
        out->ir_raw = s_latest_ir;
        out->quality = 0;
        out->valid = false;
        return false;
    }

    return true;
}

void hr_max30102_shutdown(void) {
    if (!s_initialized) return;
    
    /* Ghi bit SHDN của thanh ghi MODE_CONFIG lên 1 để chip đi vào chế độ ngủ sâu dòng thấp */
    (void)max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x80);

    /* Xóa sạch bộ đệm dữ liệu phần mềm để chuẩn bị cho lượt đo mới sau này */
    memset(s_ir_window, 0, sizeof(s_ir_window));
    memset(s_red_window, 0, sizeof(s_red_window));
    s_window_count = 0;
    s_window_pos = 0;
    s_latest_red = 0;
    s_latest_ir = 0;

    s_initialized = false;
    ESP_LOGI(TAG, "MAX30102 đã tắt nguồn để tiết kiệm pin.");
}
