/**
 * @file imu_bmi270.c
 * @brief Hiện thực driver và thuật toán đếm bước chân cho cảm biến IMU BMI270
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hướng dẫn thuật toán chi tiết:
 * 1. Nạp file cấu hình nhị phân (ASIC firmware 8KB) của hãng Bosch vào RAM nội của cảm biến BMI270.
 * 2. Cấu hình dải đo gia tốc (Range = 4G) và con quay hồi chuyển (Range = 2000 dps).
 * 3. Triển khai bộ đếm bước chân (Pedometer) phần mềm bằng cách tính độ lớn vector gia tốc 3 trục (Magnitude),
 *    qua bộ lọc thông thấp (Low-pass filter) khử nhiễu tư thế và bắt nhịp gót chân chạm đất (đỉnh xung gia tốc).
 */

#include "imu_bmi270.h"
#include "board_config.h"
#include "hw_monitor.h"
#include "bmi270_config.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

#define BMI270_I2C_ADDR          WATCH_IMU_I2C_ADDR // Địa chỉ I2C của BMI270 (0x68)
#define BMI270_REG_CHIP_ID       0x00               // Thanh ghi chứa mã định danh chip
#define BMI270_REG_DATA_8        0x0C               // Địa chỉ bắt đầu dữ liệu đo lường 6-trục (X, Y, Z Accel/Gyro)
#define BMI270_REG_ACC_CONF      0x40               // Cấu hình tần số lấy mẫu (ODR) và bộ lọc Accel
#define BMI270_REG_ACC_RANGE     0x41               // Cấu hình dải đo gia tốc (Range)
#define BMI270_REG_GYR_CONF      0x42               // Cấu hình tần số lấy mẫu (ODR) và bộ lọc Gyro
#define BMI270_REG_GYR_RANGE     0x43               // Cấu hình dải đo tốc độ góc (Range)
#define BMI270_REG_PWR_CONF      0x7C               // Thanh ghi cấu hình quản lý nguồn điện
#define BMI270_REG_PWR_CTRL      0x7D               // Thanh ghi kích hoạt nguồn các cảm biến bên trong
#define BMI270_REG_CMD           0x7E               // Thanh ghi nhận lệnh điều khiển trực tiếp (Soft-reset,...)

#define BMI270_CHIP_ID_VAL       0x24               // Giá trị Chip ID mặc định của BMI270
#define BMI270_CMD_SOFT_RESET    0xB6               // Mã lệnh thực hiện Reset mềm hệ thống

/* Các hằng số tính toán vật lý */
#define BMI270_ACCEL_LSB_PER_G   4096.0f            // Độ nhạy ở dải đo +-4G (4096 LSB/G)
#define BMI270_GYRO_LSB_PER_DPS  16.4f              // Độ nhạy ở dải đo +-2000dps (16.4 LSB/dps)

/* Các ngưỡng lọc cho thuật toán đếm bước chân phần mềm */
#define BMI270_STEP_THRESHOLD_G  1.28f              // Ngưỡng gia tốc đỉnh để nhận diện bước chân (1.28G)
#define BMI270_STEP_RELEASE_G    0.15f              // Ngưỡng phục hồi trạng thái để chuẩn bị đếm bước tiếp theo
#define BMI270_STEP_MIN_MS       300U               // Thời gian cách biệt tối thiểu giữa 2 bước chân liên tiếp (300ms)
#define BMI270_STEP_MAX_MS       2000U              // Thời gian tối đa để reset trạng thái chờ nếu đứng yên (2 giây)

static const char *TAG = "IMU_BMI270";

static bool s_initialized;           // Cờ báo trạng thái khởi tạo thành công
static uint32_t s_step_count;        // Biến lưu trữ tổng số bước chân đo được
static uint32_t s_last_step_ms;      // Mốc thời gian (TickCount) phát hiện bước chân gần nhất
static bool s_step_armed = true;     // Cờ trạng thái sẵn sàng ghi nhận nhịp gõ tiếp theo
static float s_mag_lp = 1.0f;        // Giá trị độ lớn gia tốc sau bộ lọc thông thấp (Low-pass)

/**
 * @brief Ghi dữ liệu 8-bit vào một thanh ghi của BMI270
 */
static esp_err_t bmi270_write_u8(uint8_t reg, uint8_t value) {
    return hw_monitor_i2c_write_reg(BMI270_I2C_ADDR, reg, &value, 1);
}

/**
 * @brief Đọc dữ liệu 8-bit từ một thanh ghi của BMI270
 */
static esp_err_t bmi270_read_u8(uint8_t reg, uint8_t *value) {
    return hw_monitor_i2c_read_reg(BMI270_I2C_ADDR, reg, value, 1);
}

/**
 * @brief Chuyển đổi dữ liệu thô 2 byte Little Endian sang số nguyên 16-bit có dấu
 */
static int16_t bmi270_i16_le(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief Thuật toán đếm bước chân thời gian thực (Pedometer) sử dụng lọc thông thấp
 * @param ax, ay, az Gia tốc thực tế thu được từ cảm biến trên 3 trục X, Y, Z (đơn vị: G)
 */
static void bmi270_update_steps(float ax, float ay, float az) {
    /* 1. Tính toán độ lớn vector gia tốc tổng (Magnitude): mag = sqrt(ax^2 + ay^2 + az^2) */
    float mag = sqrtf((ax * ax) + (ay * ay) + (az * az));
    
    /* 2. Áp dụng bộ lọc thông thấp IIR để loại bỏ các dao động nhiễu tần số cao:
     *    y(n) = y(n-1) + alpha * (x(n) - y(n-1)) với alpha = 0.08 */
    s_mag_lp += 0.08f * (mag - s_mag_lp);
    
    /* 3. Lấy hiệu gia tốc động (Dynamic Acceleration): Loại bỏ ảnh hưởng của gia trọng trường Trái Đất (1G) */
    float dyn = mag - s_mag_lp;
    
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    uint32_t dt_ms = (uint32_t)(now_ms - s_last_step_ms);

    /* 4. Nhận diện bước đi:
     *    Nếu gia tốc động vượt ngưỡng (> 0.28G) và thời gian giãn cách đủ lâu (> 300ms tương ứng tốc độ đi tối đa) */
    if (s_step_armed && dyn > (BMI270_STEP_THRESHOLD_G - 1.0f) && dt_ms >= BMI270_STEP_MIN_MS) {
        s_step_count++;
        s_last_step_ms = now_ms;
        s_step_armed = false; // Khóa nhận diện để tránh đếm trùng một bước chân kéo dài
        return;
    }

    /* 5. Phục hồi trạng thái chuẩn bị đếm bước kế tiếp khi gia tốc động rơi xuống dưới ngưỡng giải phóng (< 0.15G) */
    if (!s_step_armed && (dyn < BMI270_STEP_RELEASE_G || dt_ms > BMI270_STEP_MAX_MS)) {
        s_step_armed = true;
    }
}

bool imu_bmi270_init(void) {
    uint8_t chip_id = 0;
    esp_err_t err = bmi270_read_u8(BMI270_REG_CHIP_ID, &chip_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi đọc thanh ghi CHIP_ID của BMI270: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Đọc thành công CHIP_ID BMI270: 0x%02X", chip_id);

    if (chip_id != BMI270_CHIP_ID_VAL) {
        ESP_LOGE(TAG, "Sai lệch mã định danh thiết bị: Mong muốn 0x%02X, Đọc được 0x%02X",
                 BMI270_CHIP_ID_VAL, chip_id);
        return false;
    }

    /* --- Bước 1: Phát lệnh Reset mềm (Soft Reset) chip cảm biến --- */
    err = bmi270_write_u8(BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gửi lệnh Soft Reset thất bại!");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20)); // Delay chờ thanh ghi khởi động lại (> 2ms theo datasheet)

    /* --- Bước 2: Vô hiệu hóa chế độ tiết kiệm năng lượng để nạp cấu hình --- */
    err = bmi270_write_u8(BMI270_REG_PWR_CONF, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể tắt chế độ tiết kiệm năng lượng!");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Chờ bộ đếm thời gian ổn định (> 450 us)

    /* --- Bước 3: Đưa ASIC vào chế độ chuẩn bị nạp (INIT_CTRL = 0x00) --- */
    err = bmi270_write_u8(0x59, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bắt đầu chế độ nạp cấu hình thất bại!");
        return false;
    }

    /* --- Bước 4: Nạp file cấu hình ASIC 8KB của Bosch qua cổng I2C (Chia nhỏ 64 bytes) --- */
    ESP_LOGI(TAG, "Đang nạp file cấu hình ASIC 8KB vào bộ nhớ RAM BMI270...");
    const size_t config_size = sizeof(bmi270_config_file);
    const size_t chunk_size = 64;
    for (size_t offset = 0; offset < config_size; offset += chunk_size) {
        uint16_t word_offset = (uint16_t)(offset / 2);
        uint8_t addr_buf[2];
        addr_buf[0] = (uint8_t)(word_offset & 0x0F);
        addr_buf[1] = (uint8_t)((word_offset >> 4) & 0xFF);

        /* Ghi địa chỉ bắt đầu của gói cấu hình vào thanh ghi INIT_ADDR (0x5B) */
        err = hw_monitor_i2c_write_reg(BMI270_I2C_ADDR, 0x5B, addr_buf, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi ghi địa chỉ ghi tại offset %d: %s", offset, esp_err_to_name(err));
            return false;
        }

        /* Ghi gói dữ liệu nhị phân 64 byte vào thanh ghi INIT_DATA (0x5E) */
        err = hw_monitor_i2c_write_reg(BMI270_I2C_ADDR, 0x5E, &bmi270_config_file[offset], chunk_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi ghi khối dữ liệu tại offset %d: %s", offset, esp_err_to_name(err));
            return false;
        }
    }
    ESP_LOGI(TAG, "Đã nạp xong file nhị phân cấu hình!");

    /* --- Bước 5: Hoàn tất quá trình tải cấu hình (INIT_CTRL = 0x01) --- */
    err = bmi270_write_u8(0x59, 0x01);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Xác nhận kết thúc nạp cấu hình thất bại!");
        return false;
    }

    /* --- Bước 6: Đọc phản hồi trạng thái từ cảm biến kiểm tra ASIC có chạy đúng --- */
    uint8_t status = 0;
    for (int retry = 0; retry < 12; retry++) {
        vTaskDelay(pdMS_TO_TICKS(25));
        status = 0;
        err = bmi270_read_u8(0x21, &status); // Đọc thanh ghi trạng thái khởi tạo (0x21)
        if (err == ESP_OK) {
            if ((status & 0x0F) == 0x01) {
                break; // Thành công: ASIC đã hoạt động tốt
            }
            if ((status & 0x0F) == 0x02) {
                ESP_LOGE(TAG, "ASIC báo lỗi khởi tạo nội bộ (status = 0x02)");
                return false;
            }
        }
    }

    if ((status & 0x0F) != 0x01) {
        ESP_LOGE(TAG, "Đợi ASIC khởi động quá thời gian: status = 0x%02X (Kỳ vọng 0x01)", status);
        return false;
    }
    ESP_LOGI(TAG, "Bộ xử lý ASIC BMI270 đã chạy ổn định!");

    /* --- Bước 7: Thiết lập các cấu hình thanh ghi cảm biến hoạt động --- */
    if (bmi270_write_u8(BMI270_REG_ACC_CONF, 0xA6) != ESP_OK ||  // Lấy mẫu Accelerometer ODR = 25Hz
        bmi270_write_u8(BMI270_REG_ACC_RANGE, 0x02) != ESP_OK || // Dải đo gia tốc +-4G
        bmi270_write_u8(BMI270_REG_GYR_CONF, 0xA6) != ESP_OK ||  // Lấy mẫu Gyroscope ODR = 25Hz
        bmi270_write_u8(BMI270_REG_GYR_RANGE, 0x00) != ESP_OK || // Dải đo vận tốc góc +-2000 dps
        bmi270_write_u8(BMI270_REG_PWR_CTRL, 0x0E) != ESP_OK) {  // Bật hoạt động Accel, Gyro và Cảm biến Nhiệt độ
        ESP_LOGE(TAG, "Lỗi cấu hình tham số dải đo của cảm biến!");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    s_initialized = true;
    ESP_LOGI(TAG, "Cảm biến IMU BMI270 đã cấu hình hoàn tất!");
    return true;
}

bool imu_bmi270_read(watch_imu_data_t *out) {
    if (!out || !s_initialized) return false;

    uint8_t raw[12] = {0};
    
    /* Đọc liên tục 12 bytes từ thanh ghi DATA_8 (0x0C) để lấy dữ liệu 3 trục Accel + 3 trục Gyro cùng lúc */
    esp_err_t err = hw_monitor_i2c_read_reg(BMI270_I2C_ADDR, BMI270_REG_DATA_8, raw, sizeof(raw));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Đọc thanh ghi dữ liệu đo lường thất bại: %s", esp_err_to_name(err));
        return false;
    }

    /* Ghép các bytes dữ liệu thô (Little Endian) */
    int16_t ax_raw = bmi270_i16_le(&raw[0]);
    int16_t ay_raw = bmi270_i16_le(&raw[2]);
    int16_t az_raw = bmi270_i16_le(&raw[4]);
    int16_t gx_raw = bmi270_i16_le(&raw[6]);
    int16_t gy_raw = bmi270_i16_le(&raw[8]);
    int16_t gz_raw = bmi270_i16_le(&raw[10]);

    memset(out, 0, sizeof(*out));
    
    /* Quy đổi dữ liệu thô sang đơn vị vật lý thực tế */
    out->accel_x = (float)ax_raw / BMI270_ACCEL_LSB_PER_G;
    out->accel_y = (float)ay_raw / BMI270_ACCEL_LSB_PER_G;
    out->accel_z = (float)az_raw / BMI270_ACCEL_LSB_PER_G;
    out->gyro_x  = (float)gx_raw / BMI270_GYRO_LSB_PER_DPS;
    out->gyro_y  = (float)gy_raw / BMI270_GYRO_LSB_PER_DPS;
    out->gyro_z  = (float)gz_raw / BMI270_GYRO_LSB_PER_DPS;

    /* Cập nhật thuật toán đếm bước chân */
    bmi270_update_steps(out->accel_x, out->accel_y, out->accel_z);
    
    out->step_count = s_step_count;
    out->valid = true;
    return true;
}

void imu_bmi270_reset_steps(void) {
    s_step_count = 0;
    s_last_step_ms = 0;
    s_step_armed = true;
    s_mag_lp = 1.0f;
}
