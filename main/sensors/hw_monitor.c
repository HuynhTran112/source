/**
 * @file hw_monitor.c
 * @brief Hiện thực hệ thống giám sát và quản lý phần cứng thời gian thực
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Chi tiết thiết kế hệ thống:
 * 1. Tiến trình giám sát chạy nền (watch_hw_monitor_task) được gán cố định vào Core 0 dưới dạng
 *    FreeRTOS task có chu kỳ lặp 50ms, giúp thực hiện quét các cảm biến bất đồng bộ.
 * 2. Cơ chế giải vây Bus I2C (I2C Bus Recovery): Nếu một cảm biến bị treo hoặc sụt áp dẫn đến việc kéo vĩnh viễn
 *    đường SDA về mức LOW (lỗi Bus Lockup), MCU sẽ tạm giải phóng Driver I2C, đưa SCL về chế độ Output thường
 *    và tạo 9 nhịp xung clock ảo để ép thiết bị Slave nhả đường SDA. Sau đó khởi tạo lại bộ Driver I2C để khôi phục hoạt động.
 * 3. Đồng bộ hóa bộ nhớ (Thread-safe): Mọi truy xuất đọc/ghi các thông số pin, IMU, nhịp tim đều được bảo vệ
 *    bằng Semaphore Mutex nhằm tránh xung đột tranh chấp tài nguyên (Race Condition) giữa task hiển thị UI (Core 1)
 *    và task quét phần cứng (Core 0).
 * 4. Xử lý nút bấm nguồn:
 *    - Nhấn ngắn (< 1.5 giây): Yêu cầu đồng hồ đi vào chế độ ngủ sâu Deep Sleep để tiết kiệm năng lượng.
 *    - Nhấn dài (>= 1.5 giây): Hiển thị hộp thoại tắt nguồn (Shutdown Dialog) đồ họa của LVGL lên màn hình.
 */

#include "hw_monitor.h"
#include "board_config.h"
#include "sensors.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_lvgl_port.h"
#include "ui_shutdown_dialog.h"
#include "ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

static const char *TAG = "HW_MONITOR";

/* Các chu kỳ quét dữ liệu của Task nền */
#define BAT_POLL_MS     5000  // Quét thông số pin mỗi 5 giây
#define IMU_POLL_MS     50    // Đọc gia tốc/cập nhật bước chân mỗi 50ms (20Hz)
#define HR_POLL_MS      100   // Quét dữ liệu nhịp tim hồng ngoại mỗi 100ms (10Hz)
#define TASK_LOOP_MS    50    // Chu kỳ vòng lặp chính của Task nền

#define HW_MON_MUTEX_TIMEOUT_MS 120 // Thời gian chờ tối đa khi xin khóa Mutex
#define HW_MON_I2C_RETRY_COUNT 2     // Số lần thử lại tối đa khi giao tiếp I2C bị lỗi nhẹ
#define HW_MON_I2C_TIMEOUT_MS 50     // Thời gian timeout giao tiếp I2C
#define HW_MON_TASK_STACK_SIZE 4096  // Stack size cấp phát cho Task giám sát
#define HW_MON_I2C_STUCK_RECOVER_COOLDOWN_MS 30000 // Thời gian giãn cách tối thiểu giữa 2 lần giải vây bus I2C

/* Các biến trạng thái nội bộ được chia sẻ */
static SemaphoreHandle_t s_mutex = NULL; // Khóa Mutex đồng bộ dữ liệu
static TaskHandle_t s_task_handle = NULL; // Handle của FreeRTOS Task giám sát
static bool s_i2c_ready = false;          // Trạng thái sẵn sàng của Driver I2C1

/* Bộ lưu trữ dữ liệu đệm cảm biến thời gian thực */
static watch_battery_data_t s_battery = { .voltage_v = 0.0f, .soc_percent = 0.0f, .valid = false };
static watch_imu_data_t s_imu = {0};
static bool s_imu_found = false;          // Cờ báo IMU BMI270 trực tuyến
static uint32_t s_last_stuck_recover_ms = 0;
static bool s_quick_wake_enabled = false; // Trạng thái tính năng gõ màn hình thức dậy
static int s_screen_timeout = 15;         // Thời gian tắt màn hình mặc định (15 giây)
static watch_hr_data_t s_hr = { .heart_rate = 0.0f, .spo2 = 0.0f, .valid = false };
static bool s_hr_found = false;           // Cờ báo cảm biến nhịp tim MAX30102 trực tuyến
static bool s_hr_measure_enabled = false;  // Trạng thái đo nhịp tim liên tục

static esp_err_t hw_monitor_i2c_init(void);

/**
 * @brief Xin quyền khóa Mutex để truy cập vùng đệm an toàn
 */
static bool hw_monitor_lock(void) {
    return s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(HW_MON_MUTEX_TIMEOUT_MS)) == pdTRUE;
}

/**
 * @brief Trả quyền khóa Mutex
 */
static void hw_monitor_unlock(void) {
    if (s_mutex) xSemaphoreGive(s_mutex);
}

/**
 * @brief Giao tiếp đọc thanh ghi I2C đơn nhịp
 */
static esp_err_t hw_monitor_i2c_read_reg_once(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(WATCH_SENSOR_I2C_PORT, cmd, pdMS_TO_TICKS(HW_MON_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

/**
 * @brief Dọn dẹp hàng đợi FIFO phần cứng của I2C khi xảy ra lỗi truyền nhận
 */
static void hw_monitor_i2c_recover_bus(void) {
    i2c_reset_tx_fifo(WATCH_SENSOR_I2C_PORT);
    i2c_reset_rx_fifo(WATCH_SENSOR_I2C_PORT);
}

/**
 * @brief Giải thuật khôi phục vật lý Bus I2C bị nghẽn (SDA kẹt LOW) bằng xung ảo
 */
static void hw_monitor_i2c_recover_stuck_bus(void) {
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (s_last_stuck_recover_ms != 0 &&
        (uint32_t)(now_ms - s_last_stuck_recover_ms) < HW_MON_I2C_STUCK_RECOVER_COOLDOWN_MS) {
        return;
    }
    s_last_stuck_recover_ms = now_ms;

#if !WATCH_TEMP_DISABLE_I2C_LOGS
    ESP_LOGW(TAG, "Đang thực hiện giải vây Bus I2C cảm biến bị treo...");
#endif

    /* Giải phóng driver I2C phần cứng hiện hành để MCU nắm quyền GPIO */
    esp_err_t err = i2c_driver_delete(WATCH_SENSOR_I2C_PORT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
#if !WATCH_TEMP_DISABLE_I2C_LOGS
        ESP_LOGW(TAG, "Không thể giải phóng driver I2C: %s", esp_err_to_name(err));
#endif
    }
    s_i2c_ready = false;

    /* Cấu hình tạm thời chân SDA và SCL thành Open-Drain GPIO để kéo thả trực tiếp */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WATCH_PIN_IMU_I2C_SDA) | (1ULL << WATCH_PIN_IMU_I2C_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 1);
    gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
    esp_rom_delay_us(5);

    /* Phát tối đa 9 nhịp xung clock trên chân SCL.
     * Thiết bị Slave đang kẹt ở chế độ truyền dữ liệu sẽ dịch chuyển bit trong thanh ghi ra,
     * nhả đường SDA trả về trạng thái HIGH. */
    for (int i = 0; i < 9; i++) {
        gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 0);
        esp_rom_delay_us(5);
        gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
        esp_rom_delay_us(5);
        if (gpio_get_level(WATCH_PIN_IMU_I2C_SDA)) {
            break; // Đã nhả SDA thành công
        }
    }

    /* Tạo nhịp STOP thủ công trên Bus */
    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 0);
    esp_rom_delay_us(5);
    gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
    esp_rom_delay_us(5);
    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 1);
    esp_rom_delay_us(5);

    /* Khởi tạo lại Driver I2C phần cứng sau khi giải vây */
    err = hw_monitor_i2c_init();
    if (err != ESP_OK) {
#if !WATCH_TEMP_DISABLE_I2C_LOGS
        ESP_LOGW(TAG, "Khởi tạo lại driver I2C cảm biến thất bại: %s", esp_err_to_name(err));
#endif
    }
}

esp_err_t hw_monitor_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < HW_MON_I2C_RETRY_COUNT; attempt++) {
        err = hw_monitor_i2c_read_reg_once(addr, reg, data, len);
        if (err == ESP_OK) return ESP_OK;
        hw_monitor_i2c_recover_bus();
        vTaskDelay(pdMS_TO_TICKS(10)); // Tránh xung đột dồn dập
    }
    hw_monitor_i2c_recover_stuck_bus(); // Thực hiện giải vây vật lý nếu thử lại thất bại
    return err;
}

esp_err_t hw_monitor_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(WATCH_SENSOR_I2C_PORT, cmd, pdMS_TO_TICKS(HW_MON_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        hw_monitor_i2c_recover_bus();
        hw_monitor_i2c_recover_stuck_bus();
    }
    return err;
}

/**
 * @brief Khởi tạo driver ngoại vi I2C1 (Sensor Bus)
 */
static esp_err_t hw_monitor_i2c_init(void) {
    if (s_i2c_ready) return ESP_OK;

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WATCH_PIN_IMU_I2C_SDA,
        .scl_io_num = WATCH_PIN_IMU_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = WATCH_IMU_I2C_CLK_HZ,
    };

    esp_err_t err = i2c_param_config(WATCH_SENSOR_I2C_PORT, &i2c_cfg);
    if (err != ESP_OK) {
#if !WATCH_TEMP_DISABLE_I2C_LOGS
        ESP_LOGE(TAG, "Cấu hình tham số I2C cảm biến thất bại: %s", esp_err_to_name(err));
#endif
        return err;
    }

    err = i2c_driver_install(WATCH_SENSOR_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
#if !WATCH_TEMP_DISABLE_I2C_LOGS
        ESP_LOGE(TAG, "Không thể đăng ký driver I2C cảm biến: %s", esp_err_to_name(err));
#endif
        return err;
    }

#if !WATCH_TEMP_DISABLE_I2C_LOGS
    ESP_LOGI(TAG, "Bus I2C cảm biến khởi động thành công trên cổng %d", WATCH_SENSOR_I2C_PORT);
#endif
    s_i2c_ready = true;
    return ESP_OK;
}

static bool watch_hw_monitor_is_imu_needed(void) {
    /* Luôn giữ IMU trực tuyến để đo bước chân nền */
    return true;
}

static bool watch_hw_monitor_is_hr_needed(void) {
    bool enabled = s_hr_measure_enabled;
    if (hw_monitor_lock()) {
        enabled = s_hr_measure_enabled;
        hw_monitor_unlock();
    }
    return enabled || watch_sensors_is_gps_active();
}

bool watch_hw_monitor_is_quick_wake_enabled(void) {
    bool enabled = false;
    if (hw_monitor_lock()) {
        enabled = s_quick_wake_enabled;
        hw_monitor_unlock();
    } else {
        enabled = s_quick_wake_enabled;
    }
    return enabled;
}

void watch_hw_monitor_set_quick_wake_enabled(bool enabled) {
    if (hw_monitor_lock()) {
        s_quick_wake_enabled = enabled;
        hw_monitor_unlock();
    } else {
        s_quick_wake_enabled = enabled;
    }
    ESP_LOGI(TAG, "Đã cập nhật cấu hình Đánh thức nhanh: %s", enabled ? "BẬT" : "TẮT");
}

int watch_hw_monitor_get_screen_timeout(void) {
    int val = 15;
    if (hw_monitor_lock()) {
        val = s_screen_timeout;
        hw_monitor_unlock();
    } else {
        val = s_screen_timeout;
    }
    return val;
}

void watch_hw_monitor_set_screen_timeout(int seconds) {
    if (hw_monitor_lock()) {
        s_screen_timeout = seconds;
        hw_monitor_unlock();
    } else {
        s_screen_timeout = seconds;
    }
    ESP_LOGI(TAG, "Đã cập nhật thời gian chờ tắt màn hình: %d giây", seconds);
}

/**
 * @brief Vòng lặp giám sát của Task nền FreeRTOS
 */
static void watch_hw_monitor_task(void *arg) {
    (void)arg;

    uint32_t last_bat_poll_ms = 0;
    uint32_t last_imu_poll_ms = 0;
    uint32_t last_hr_poll_ms = 0;
    uint32_t btn_press_duration_ms = 0;

    while (1) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 1. Đo mức pin định kỳ (5000ms) */
        if (last_bat_poll_ms == 0 || (now_ms - last_bat_poll_ms) >= BAT_POLL_MS) {
            watch_battery_data_t bat;
            if (battery_max17048_read(&bat) && hw_monitor_lock()) {
                s_battery = bat;
                hw_monitor_unlock();
            }
            last_bat_poll_ms = now_ms;
        }

        /* 2. Quét IMU đếm bước chân định kỳ (50ms) */
        if (watch_hw_monitor_is_imu_needed() &&
            (last_imu_poll_ms == 0 || (now_ms - last_imu_poll_ms) >= IMU_POLL_MS)) {
            bool imu_found = false;
            if (hw_monitor_lock()) {
                imu_found = s_imu_found;
                hw_monitor_unlock();
            }

            if (!imu_found) {
                /* Thử khởi tạo lại nếu thiết bị mất kết nối đột ngột */
                if (imu_bmi270_init()) {
                    if (hw_monitor_lock()) {
                        s_imu_found = true;
                        s_imu.valid = true;
                        hw_monitor_unlock();
                    }
                }
            } else {
                watch_imu_data_t imu;
                if (imu_bmi270_read(&imu) && hw_monitor_lock()) {
                    s_imu = imu;
                    hw_monitor_unlock();
                }
            }
            last_imu_poll_ms = now_ms;
        }

        /* 3. Đo nhịp tim và SpO2 định kỳ (100ms) */
        bool hr_needed = watch_hw_monitor_is_hr_needed();
        if (hr_needed && (last_hr_poll_ms == 0 || (now_ms - last_hr_poll_ms) >= HR_POLL_MS)) {
            bool hr_found = false;
            if (hw_monitor_lock()) {
                hr_found = s_hr_found;
                hw_monitor_unlock();
            }

            if (!hr_found) {
                if (hr_max30102_init()) {
                    if (hw_monitor_lock()) {
                        s_hr_found = true;
                        s_hr.valid = false;
                        hw_monitor_unlock();
                    }
                }
            } else {
                watch_hr_data_t hr;
                bool ok = hr_max30102_read(&hr);
                if (hw_monitor_lock()) {
                    s_hr = hr;
                    if (!ok) s_hr.valid = false;
                    hw_monitor_unlock();
                }
            }
            last_hr_poll_ms = now_ms;
        } else if (!hr_needed && s_hr_found) {
            /* Tắt nguồn cảm biến đo mạch để tiết kiệm năng lượng khi không cần thiết */
            hr_max30102_shutdown();
            if (hw_monitor_lock()) {
                s_hr_found = false;
                hw_monitor_unlock();
            }
        }

        /* 4. Giám sát phím nguồn vật lý (Tích cực mức thấp - Active LOW) */
        static bool s_btn_handled = false;
        static bool s_last_btn_state = false;
        bool btn_pressed = (gpio_get_level(WATCH_PIN_BUTTON_POWER) == 0);

        if (btn_pressed) {
            if (!s_last_btn_state) {
                ESP_LOGI(TAG, "Phím nguồn được nhấn! (GPIO%d=0)", WATCH_PIN_BUTTON_POWER);
            }
            btn_press_duration_ms += TASK_LOOP_MS;
            
            /* Xử lý nhấn giữ (Long Press >= 1500ms): Hiện cửa sổ lựa chọn tắt nguồn */
            if (btn_press_duration_ms >= 1500) {
                ESP_LOGI(TAG, "Phát hiện nhấn giữ nút nguồn -> Đang hiển thị Shutdown Dialog...");
                btn_press_duration_ms = 0; // Reset bộ đếm tránh hiển thị lặp
                s_btn_handled = true;
                if (lvgl_port_lock(1000)) {
                    if (!ui_shutdown_dialog_is_visible()) {
                        ui_shutdown_dialog_show();
                    }
                    lvgl_port_unlock();
                } else {
                    ESP_LOGE(TAG, "Lỗi: Không thể khóa luồng đồ họa LVGL để vẽ Shutdown Dialog!");
                }
            }
        } else {
            /* Xử lý sự kiện nhả phím nguồn */
            if (s_last_btn_state) {
                ESP_LOGI(TAG, "Phím nguồn đã nhả! Thời gian nhấn: %lu ms", (unsigned long)btn_press_duration_ms);
                
                /* Nhấn ngắn (Short Press 50ms - 1500ms): Ra lệnh ngủ sâu tắt màn hình */
                if (btn_press_duration_ms >= 50 && btn_press_duration_ms < 1500 && !s_btn_handled) {
                    ESP_LOGI(TAG, "Phát hiện nhấn ngắn -> Gửi yêu cầu tắt màn hình (Sleep)...");
                    if (lvgl_port_lock(1000)) {
                        ui_request_deep_sleep();
                        lvgl_port_unlock();
                    } else {
                        ESP_LOGE(TAG, "Lỗi: Không thể khóa luồng đồ họa LVGL để tắt màn hình!");
                    }
                }
            }
            btn_press_duration_ms = 0;
            s_btn_handled = false;
        }
        s_last_btn_state = btn_pressed;

        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_MS));
    }
}

esp_err_t watch_hw_monitor_start(void) {
    if (s_task_handle) return ESP_OK;

    esp_err_t err = hw_monitor_i2c_init();
    if (err != ESP_OK) return err;

    /* Cấu hình chân GPIO của phím nguồn vật lý làm đầu vào có trở kéo cao (Pull-up) */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            ESP_LOGE(TAG, "Không thể khởi tạo Semaphore Mutex!");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Tạo Task giám sát phần cứng chạy cố định trên Core 0, độ ưu tiên 2 */
    BaseType_t task_ok = xTaskCreatePinnedToCore(watch_hw_monitor_task, "hw_monitor_task",
                                                 HW_MON_TASK_STACK_SIZE, NULL, 2,
                                                 &s_task_handle, 0);
    if (task_ok != pdPASS) {
        s_task_handle = NULL;
        ESP_LOGE(TAG, "Không thể khởi tạo FreeRTOS Task giám sát phần cứng!");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool watch_hw_monitor_get_battery(watch_battery_data_t *out) {
    if (!out) return false;
    if (!hw_monitor_lock()) return false;
    *out = s_battery;
    bool valid = s_battery.valid;
    hw_monitor_unlock();
    return valid;
}

bool watch_hw_monitor_get_imu(watch_imu_data_t *out) {
    if (!out) return false;
    if (!hw_monitor_lock()) return false;
    *out = s_imu;
    bool found = s_imu_found;
    hw_monitor_unlock();
    return found;
}

bool watch_hw_monitor_get_hr(watch_hr_data_t *out) {
    if (!out) return false;
    if (!hw_monitor_lock()) return false;
    *out = s_hr;
    bool valid = s_hr.valid;
    hw_monitor_unlock();
    return valid;
}

void watch_hw_monitor_set_hr_measure_enabled(bool enabled) {
    if (hw_monitor_lock()) {
        s_hr_measure_enabled = enabled;
        if (enabled) {
            /* Reset dữ liệu về trạng thái trống khi bắt đầu lượt đo mới */
            s_hr.valid = false;
            s_hr.heart_rate = 0.0f;
            s_hr.spo2 = 0.0f;
            s_hr.quality = 0;
        }
        hw_monitor_unlock();
    } else {
        s_hr_measure_enabled = enabled;
    }
    ESP_LOGI(TAG, "Cảm biến nhịp tim đo thủ công: %s", enabled ? "BẬT" : "TẮT");
}

void watch_hw_monitor_reset_steps(void) {
    imu_bmi270_reset_steps();
    if (hw_monitor_lock()) {
        s_imu.step_count = 0;
        hw_monitor_unlock();
    }
}
