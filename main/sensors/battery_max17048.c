/**
 * @file battery_max17048.c
 * @brief Hiện thực hàm đọc dữ liệu từ IC đo pin MAX17048G qua bus I2C
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Driver thực hiện việc đọc các thanh ghi điện áp tế bào pin (VCELL) và dung lượng pin (SOC)
 * từ chip đo pin MAX17048G sử dụng các hàm giao tiếp I2C gián tiếp qua phần cứng được định nghĩa trong hw_monitor.c.
 */

#include "battery_max17048.h"
#include "hw_monitor.h"

#define MAX17048_I2C_ADDR     0x36   // Địa chỉ I2C mặc định của IC MAX17048G
#define MAX17048_REG_VCELL    0x02   // Địa chỉ thanh ghi đọc điện áp pin (VCELL)
#define MAX17048_REG_SOC      0x04   // Địa chỉ thanh ghi đọc phần trăm pin (State of Charge)

/* Độ phân giải LSB của thanh ghi điện áp theo datasheet là 78.125 uV */
#define MAX17048_VCELL_LSB_V  0.000078125f

bool battery_max17048_read(watch_battery_data_t *out) {
    if (!out) return false;

    uint8_t vcell_buf[2] = {0};
    uint8_t soc_buf[2] = {0};

    /* Đọc dữ liệu 16-bit (2 bytes) từ thanh ghi VCELL và SOC của IC */
    if (hw_monitor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_VCELL, vcell_buf, sizeof(vcell_buf)) != ESP_OK ||
        hw_monitor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_SOC, soc_buf, sizeof(soc_buf)) != ESP_OK) {
        return false;
    }

    /* Ghép 2 bytes dữ liệu thô đọc được (MSB đầu tiên) */
    uint16_t vcell_raw = ((uint16_t)vcell_buf[0] << 8) | vcell_buf[1];
    uint16_t soc_raw = ((uint16_t)soc_buf[0] << 8) | soc_buf[1];

    /* Quy đổi điện áp thực tế: V_cell = VCELL_raw * 78.125 uV */
    out->voltage_v = (float)vcell_raw * MAX17048_VCELL_LSB_V;

    /* Quy đổi dung lượng: SOC_% = SOC_raw / 256.0 (datasheet mục thanh ghi SOC) */
    out->soc_percent = (float)soc_raw / 256.0f;

    /* Ràng buộc giá trị phần trăm pin nằm trong khoảng từ 0% đến 100% */
    if (out->soc_percent < 0.0f) out->soc_percent = 0.0f;
    if (out->soc_percent > 100.0f) out->soc_percent = 100.0f;

    out->charge_rate = 0.0f; // Để trống hoặc mở rộng thuật toán tính tốc độ sạc sau này
    out->valid = true;
    
    return true;
}
