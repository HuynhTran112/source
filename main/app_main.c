/**
 * @file app_main.c
 * @brief Chương trình chính khởi tạo phần cứng và môi trường LVGL cho Smartwatch
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hệ thống sử dụng hệ điều hành FreeRTOS. Thứ tự khởi tạo rất quan trọng:
 * 1. Khởi tạo bộ nhớ không bay hơi NVS Flash (Wi-Fi/BLE cần dùng).
 * 2. Đọc cấu hình tùy chỉnh của người dùng từ NVS Flash (độ sáng, ngôn ngữ, thời gian tắt màn hình).
 * 3. Bật đèn nền LCD ở chế độ PWM để tránh chớp nháy đột ngột.
 * 4. Khởi tạo bộ điều khiển màn hình ST7789 qua kết nối SPI DMA.
 * 5. Khởi tạo chip cảm ứng CST816S qua kết nối I2C và ngắt chân INT.
 * 6. Khởi tạo thư viện đồ họa LVGL (esp_lvgl_port), phân phối bộ nhớ đệm vẽ (Buffer) trên PSRAM.
 * 7. Khởi tạo các Subsystems (WiFi, BLE, cảm biến) nhằm tránh tranh chấp tài nguyên.
 * 8. Khởi chạy giao diện UI người dùng.
 */

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sleep.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "board_config.h"
#include "ble_notify.h"
#include "hw_monitor.h"
#include "net_state.h"
#include "ota_service.h"
#include "ui.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "ui_notif_popup.h"
#include "sensors.h"
#include "vibration_motor.h"
#include "watch_settings.h"
#include "watch_activity_log.h"
#include "battery_max17048.h"
#include "imu_bmi270.h"
#include "hr_max30102.h"

static const char *TAG = "SMARTWATCH_MAIN";

/* ===================================================================
 *  1. KHỞI TẠO ĐÈN NỀN LCD (Điều chế độ rộng xung - PWM)
 *  Sử dụng bộ ngoại vi LEDC của ESP32-S3 điều khiển độ sáng mịn màng
 * =================================================================== */
/**
 * @brief Khởi tạo kênh PWM điều khiển đèn nền LCD
 * @param duty Độ rộng xung ban đầu (0-255)
 */
static void watch_backlight_init(uint32_t duty) {
    /* Cấu hình bộ định thời Timer phát xung PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = WATCH_LCD_BACKLIGHT_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT, // Độ phân giải 8-bit (0-255)
        .freq_hz = 5000,                     // Tần số phát xung 5kHz để mắt người không thấy nhấp nháy
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    /* Cấu hình kênh PWM gắn với chân GPIO ngoại vi */
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = WATCH_LCD_BACKLIGHT_LEDC_CH,
        .timer_sel = WATCH_LCD_BACKLIGHT_LEDC_TIMER,
        .gpio_num = WATCH_PIN_LCD_BACKLIGHT,
        .duty = duty,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    ESP_LOGI(TAG, "Đèn nền LCD đã bật (PWM duty = %lu/255)", (unsigned long)duty);
}

/**
 * @brief Chuyển đổi mã màu từ Settings sang cấu trúc màu của LVGL
 * @param color_id Mã định danh màu
 * @return lv_color_t Màu tương ứng
 */
static lv_color_t watch_color_from_id(watch_color_id_t color_id) {
    switch (color_id) {
        case WATCH_COLOR_GREEN:  return COLOR_GREEN;
        case WATCH_COLOR_RED:    return COLOR_RED;
        case WATCH_COLOR_PURPLE: return COLOR_PURPLE;
        case WATCH_COLOR_ORANGE: return COLOR_ORANGE;
        case WATCH_COLOR_DEFAULT:
        default:                 return COLOR_BG;
    }
}

/* ===================================================================
 *  2. KHỞI TẠO MÀN HÌNH LCD ST7789 (SPI Interface)
 * =================================================================== */
/**
 * @brief Cấu hình ngoại vi SPI và khởi động màn hình TFT ST7789
 * @param[out] panel_handle Trả về con trỏ quản lý panel LCD
 * @param[out] io_handle Trả về con trỏ quản lý cổng IO LCD
 */
static void watch_lcd_init(esp_lcd_panel_handle_t *panel_handle,
                           esp_lcd_panel_io_handle_t *io_handle) {
    /* --- Bước 2a: Thiết lập cấu hình SPI Bus --- */
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = WATCH_PIN_LCD_SCLK,
        .mosi_io_num = WATCH_PIN_LCD_MOSI,
        .miso_io_num = -1, // Không dùng chân nhận dữ liệu (LCD chỉ nhận lệnh/dữ liệu ghi)
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = WATCH_LCD_H_RES * WATCH_LCD_V_RES * sizeof(lv_color_t) / 2, // Kích thước truyền tải SPI lớn cho vẽ mượt mà
    };
    ESP_ERROR_CHECK(spi_bus_initialize(WATCH_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* --- Bước 2b: Tạo kết nối logic SPI Panel IO --- */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = WATCH_PIN_LCD_DC,
        .cs_gpio_num = WATCH_PIN_LCD_CS,
        .pclk_hz = WATCH_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0, // Chạy ở SPI Mode 0 để tương thích cao nhất với LCD ST7789 thương mại
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)WATCH_LCD_SPI_HOST, &io_cfg, io_handle));

    /* --- Thực hiện Reset cứng bằng phần cứng để khởi động màn hình --- */
    vTaskDelay(pdMS_TO_TICKS(100)); // Chờ nguồn ổn định
    gpio_set_direction(WATCH_PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WATCH_PIN_LCD_RST, 0); // Kéo xuống mức thấp (Reset)
    vTaskDelay(pdMS_TO_TICKS(100)); // Giữ 100ms
    gpio_set_level(WATCH_PIN_LCD_RST, 1); // Trả lại mức cao
    vTaskDelay(pdMS_TO_TICKS(150)); // Chờ màn hình khởi động lại bộ nhớ trong

    /* --- Bước 2c: Khởi tạo Driver Panel ST7789 từ Espressif SDK --- */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1, // Không dùng reset tự động của SDK vì đã reset bằng tay ở trên
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16, // Định dạng màu 16-bit RGB565
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(*io_handle, &panel_cfg, panel_handle));

    /* Khởi động tấm nền LCD */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Đưa khoảng lệch Gap về 0 */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(*panel_handle, 0, 0));

    /* Cấu hình hướng hiển thị và màu đảo ngược (ST7789 IPS thường bị ngược màu mặc định) */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));

    ESP_LOGI(TAG, "Màn hình ST7789 đã khởi tạo thành công (%dx%d)", WATCH_LCD_H_RES, WATCH_LCD_V_RES);
}

/* ===================================================================
 *  3. KHỞI TẠO CẢM ỨNG CST816S (I2C Interface)
 * =================================================================== */
/**
 * @brief Cấu hình cổng I2C0 và cài đặt chip điều khiển cảm ứng CST816S
 * @param[out] touch_handle Trả về con trỏ quản lý driver cảm ứng
 */
static void watch_touch_init(esp_lcd_touch_handle_t *touch_handle) {
    /* --- Bước 3a: Cấu hình cổng ngoại vi I2C Master --- */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WATCH_PIN_TOUCH_SDA,
        .scl_io_num = WATCH_PIN_TOUCH_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = WATCH_TOUCH_I2C_CLK_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(WATCH_TOUCH_I2C_PORT, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(WATCH_TOUCH_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    /* --- Bước 3b: Kéo Reset cứng cảm ứng để khởi chạy ổn định --- */
    ESP_LOGI(TAG, "Đang nháy chân Reset cảm ứng điện dung CST816S trên GPIO %d...", WATCH_PIN_TOUCH_RST);
    gpio_reset_pin(WATCH_PIN_TOUCH_RST);
    gpio_set_direction(WATCH_PIN_TOUCH_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WATCH_PIN_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // Giữ mức thấp 20ms
    gpio_set_level(WATCH_PIN_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // Chờ chip khởi chạy bộ xử lý cảm ứng bên trong

    /* --- Bước 3c: Quét thiết bị I2C để xác thực kết nối vật lý --- */
    ESP_LOGI(TAG, "Đang quét tìm địa chỉ I2C của CST816S...");
    bool found_device = false;
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(WATCH_TOUCH_I2C_PORT, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Phát hiện thiết bị I2C tại địa chỉ: 0x%02X", addr);
            found_device = true;
        }
    }
    if (!found_device) {
        ESP_LOGE(TAG, "Không tìm thấy bất kỳ thiết bị I2C nào trên bus cảm ứng!");
    }

    /* --- Bước 3d: Gắn cổng IO I2C cảm ứng với SDK --- */
    esp_lcd_panel_io_handle_t tp_io_handle;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = 0; // Kế thừa cấu hình tốc độ từ driver I2C đã tạo ở trên

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(
        (esp_lcd_i2c_bus_handle_t)WATCH_TOUCH_I2C_PORT, &tp_io_cfg, &tp_io_handle));

    /* --- Bước 3e: Cấu hình phần mềm cho cảm ứng --- */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = WATCH_LCD_H_RES,
        .y_max = WATCH_LCD_V_RES,
        .rst_gpio_num = WATCH_PIN_TOUCH_RST,
        .int_gpio_num = WATCH_PIN_TOUCH_INT,
        .levels = {
            .reset = 0,     // Chân reset tích cực mức thấp
            .interrupt = 0, // Chân ngắt kéo xuống LOW khi có sự kiện chạm tay
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 1, // Đảo ngược trục Y để đồng bộ với hướng màn hình hiển thị
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, touch_handle));

    /* Bật trở kháng kéo cao (Pull-up) cho chân ngắt nhằm chống nhiễu */
    gpio_set_pull_mode((gpio_num_t)WATCH_PIN_TOUCH_INT, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Driver cảm ứng CST816S đã sẵn sàng");
}

/* ===================================================================
 *  4. KHỞI TẠO CỔNG ESP_LVGL_PORT (Liên kết phần cứng và LVGL)
 * =================================================================== */
/**
 * @brief Đăng ký LCD và Cảm ứng với thư viện đồ họa LVGL
 * 
 * Lưu ý bộ nhớ: Bộ nhớ đệm đồ họa (LVGL Buffers) được phân bổ trên PSRAM (SPIRAM)
 * để tránh hết RAM nội (Internal RAM) và hỗ trợ cơ chế Double Buffer giúp cuộn màn hình
 * mượt mà không bị xé hình (Tearing).
 */
static void watch_lvgl_port_init(esp_lcd_panel_handle_t panel_handle,
                                 esp_lcd_panel_io_handle_t io_handle,
                                 esp_lcd_touch_handle_t touch_handle) {
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    
    /* Kiểm tra bộ nhớ SPIRAM hiện có */
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t pixel_bytes = sizeof(lv_color_t);
    const size_t full_screen_bytes = (size_t)WATCH_LCD_H_RES * (size_t)WATCH_LCD_V_RES * pixel_bytes;
    uint32_t chosen_buffer_pixels = WATCH_LCD_H_RES * WATCH_LCD_V_RES / 10; // Mặc định 1/10 màn hình
    bool chosen_double_buffer = false;
    bool chosen_buff_spiram = false;

    /* Tối ưu hóa các tham số phân luồng LVGL */
    lvgl_cfg.task_affinity = 1;     // Gắn task vẽ đồ họa vào Core 1 (Core 0 chạy mạng/BLE)
    lvgl_cfg.task_priority = 4;     // Ưu tiên chạy rất cao cho giao diện hiển thị mượt
    lvgl_cfg.task_stack = 8192;     // Dung lượng stack của task
    lvgl_cfg.timer_period_ms = 16;
    lvgl_cfg.task_max_sleep_ms = 100;

    if (psram_free >= (full_screen_bytes + 32768)) {
        chosen_buffer_pixels = WATCH_LCD_H_RES * WATCH_LCD_V_RES; // Cấu hình buffer toàn màn hình
        chosen_buff_spiram = true;
        chosen_double_buffer = (psram_free >= (full_screen_bytes * 2 + 65536)); // Bật double buffer nếu dư PSRAM
        lvgl_cfg.timer_period_ms = chosen_double_buffer ? 8 : 12;
        if (chosen_double_buffer) {
            lvgl_cfg.task_stack = 12288; // Tăng stack để vẽ đồ họa nhanh hơn
        }
        ESP_LOGI(TAG, "PSRAM trống: %u KB - Sử dụng %s bộ đệm toàn màn hình, nhịp vẽ=%ums",
                 (unsigned int)(psram_free / 1024),
                 chosen_double_buffer ? "Double" : "Single",
                 lvgl_cfg.timer_period_ms);
    } else {
        /* Chế độ dự phòng khi thiếu PSRAM: Vẽ theo từng phần nhỏ trên Internal RAM */
        chosen_buffer_pixels = WATCH_LCD_H_RES * WATCH_LCD_V_RES / 10;
        chosen_double_buffer = false;
        chosen_buff_spiram = false;
        ESP_LOGW(TAG, "PSRAM quá thấp: %u KB - Dùng bộ đệm nhỏ %u px để tránh tràn bộ nhớ (OOM)",
                 (unsigned int)(psram_free / 1024), (unsigned int)chosen_buffer_pixels);
    }

    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    /* --- Đăng ký Panel hiển thị lên cổng đồ họa của LVGL --- */
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = chosen_buffer_pixels,
        .double_buffer = chosen_double_buffer,
        .trans_size = WATCH_LCD_H_RES * WATCH_LCD_V_RES / 2, // Bộ đệm DMA SPI nội dung (~68KB)
        .hres = WATCH_LCD_H_RES,
        .vres = WATCH_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false, // Phải để false để LVGL dùng bộ đệm PSRAM ổn định
            .buff_spiram = chosen_buff_spiram,
        },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    /* Chế độ chống xé hình: Chỉ swap buffer khi đã vẽ xong hoàn chỉnh */
    if (chosen_double_buffer) {
        disp->driver->full_refresh = 1;
        ESP_LOGI(TAG, "Bật chế độ full_refresh để chống xé hình khi vuốt danh sách!");
    }

    /* --- Đăng ký driver đầu vào cảm ứng CST816S cho màn hình --- */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch_handle,
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "Hệ thống vẽ đồ họa LVGL đã sẵn sàng");
}

/* ===================================================================
 *  5. TÁC VỤ KẾT NỐI VÀ TRUYỀN DỮ LIỆU TELEMETRY QUA BLE
 * =================================================================== */
/**
 * @brief Hàm callback xử lý khi nhận gói dữ liệu/thông báo BLE từ điện thoại
 */
static void watch_ble_notification_cb(const watch_ble_notification_t *notif) {
    const watch_settings_t *settings = watch_settings_get();
    if (settings->vibration_enabled) {
        watch_vibration_notify(settings->vibration_strength);
    }
    ui_notif_popup_show(notif); // Hiển thị popup cuộc gọi/tin nhắn lên màn hình đồng hồ
}

/**
 * @brief Đa nhiệm (FreeRTOS Task) định kỳ gửi dữ liệu cảm biến lên ứng dụng điện thoại
 */
static void watch_ble_telemetry_task(void *arg) {
    (void)arg;
    while (1) {
        uint32_t tel_interval_ms = 30000; // Mặc định gửi định kỳ mỗi 30 giây để tiết kiệm pin
        
        if (watch_bt_is_connected()) {
            watch_battery_data_t bat = {0};
            watch_imu_data_t imu = {0};
            watch_hr_data_t hr = {0};
            watch_gps_metrics_t gps = {0};
            
            bool bat_ok = watch_hw_monitor_get_battery(&bat);
            bool imu_ok = watch_hw_monitor_get_imu(&imu);
            bool hr_ok = watch_hw_monitor_get_hr(&hr) && hr.valid;
            bool gps_ok = watch_sensors_get_gps_metrics(&gps);
            bool sport_active = watch_sensors_is_gps_active();
            
            /* Tăng tần suất cập nhật lên 2 giây một lần nếu đang ở chế độ luyện tập có bật GPS */
            tel_interval_ms = sport_active ? 2000 : 30000;

            uint32_t session_id = ui_activity_get_current_session_id();
            const char *sport_mode = sport_active ? ui_activity_get_current_sport_mode() : "none";
            char tel[208];
            
            /* Định dạng chuỗi truyền dữ liệu Telemetry chuẩn: TEL|bước_chân|khoảng_cách|tốc_độ|phần_trăm_pin|nhịp_tim|SpO2|chế_độ_thể_thao|gps_hợp_lệ|vĩ_độ|kinh_độ|mã_phiên */
            snprintf(tel, sizeof(tel),
                     "TEL|%lu|%.3f|%.1f|%d|%.0f|%.0f|%s|%d|%.6f|%.6f|%lu",
                     (unsigned long)(imu_ok ? imu.step_count : 0),
                     gps_ok ? gps.total_distance_km : 0.0f,
                     gps_ok ? gps.speed_kmh : 0.0f,
                     bat_ok ? (int)bat.soc_percent : -1,
                     hr_ok ? hr.heart_rate : 0.0f,
                     hr_ok ? hr.spo2 : 0.0f,
                     sport_mode,
                     gps_ok && gps.fix_valid ? 1 : 0,
                     gps_ok ? gps.latitude_deg : 0.0,
                     gps_ok ? gps.longitude_deg : 0.0,
                     (unsigned long)session_id);
            watch_ble_notify_send_cmd(tel);
        }
        vTaskDelay(pdMS_TO_TICKS(tel_interval_ms));
    }
}

#if CONFIG_WATCH_HEAP_MONITOR
/**
 * @brief Đa nhiệm (FreeRTOS Task) kiểm tra rò rỉ bộ nhớ (chỉ chạy khi debug)
 */
static void watch_heap_monitor_task(void *arg) {
    (void)arg;
    uint32_t sample = 0;
    size_t min_internal_free = SIZE_MAX;
    size_t min_internal_largest = SIZE_MAX;
    size_t min_psram_free = SIZE_MAX;
    size_t min_psram_largest = SIZE_MAX;

    for (;;) {
        size_t total_free = esp_get_free_heap_size();
        size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        if (internal_free < min_internal_free) min_internal_free = internal_free;
        if (internal_largest < min_internal_largest) min_internal_largest = internal_largest;
        if (psram_free < min_psram_free) min_psram_free = psram_free;
        if (psram_largest < min_psram_largest) min_psram_largest = psram_largest;

        ESP_LOGI(TAG,
                 "Heap free: %lu | Internal: %lu (min %lu) | Internal largest: %lu (min %lu) | PSRAM: %lu (min %lu) | PSRAM largest: %lu (min %lu)",
                 (unsigned long)total_free,
                 (unsigned long)internal_free,
                 (unsigned long)min_internal_free,
                 (unsigned long)internal_largest,
                 (unsigned long)min_internal_largest,
                 (unsigned long)psram_free,
                 (unsigned long)min_psram_free,
                 (unsigned long)psram_largest,
                 (unsigned long)min_psram_largest);

        if ((sample++ % 4U) == 0U) {
            bool heap_ok = heap_caps_check_integrity_all(true);
            if (!heap_ok) {
                ESP_LOGE(TAG, "Cảnh báo: Phát hiện lỗi toàn vẹn vùng nhớ Heap!");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
#endif

/* ===================================================================
 *  6. ĐÁNH THỨC BẰNG CHẠM CẢM ỨNG (Triple Tap Detection)
 * =================================================================== */
/**
 * @brief Giải thuật chống nhiễu ngắt, phát hiện chạm liên tục 3 lần để bật màn hình
 * @return true Đánh thức thành công
 * @return false Không đủ 3 lần chạm (quay lại ngủ sâu)
 */
static bool watch_detect_triple_tap(void) {
    /* Cấu hình chân ngắt của cảm ứng làm chân đầu vào */
    gpio_config_t touch_pin_cfg = {
        .pin_bit_mask = 1ULL << WATCH_PIN_TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&touch_pin_cfg);

    int tap_count = 1; // Nhát gõ đầu tiên đã tạo ngắt kích hoạt thức dậy
    uint32_t start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    bool last_state = gpio_get_level(WATCH_PIN_TOUCH_INT);

    ESP_LOGI("TRIPLE_TAP", "Phát hiện sự kiện gõ màn hình. Chờ thêm 2 nhịp chạm trong 800ms...");

    while ((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - start_ms < 800) {
        bool current_state = gpio_get_level(WATCH_PIN_TOUCH_INT);
        if (last_state == 1 && current_state == 0) {
            /* Phát hiện sườn xuống (Chạm ngắt kéo chân INT về mức LOW) */
            tap_count++;
            ESP_LOGI("TRIPLE_TAP", "Phát hiện gõ lần %d", tap_count);
            if (tap_count >= 3) {
                ESP_LOGI("TRIPLE_TAP", "Xác nhận chạm 3 lần (Triple Tap)! Bật nguồn giao diện...");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Chống rung phím cảm ứng (debounce)
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGW("TRIPLE_TAP", "Không đủ 3 lần chạm trong 800ms (Số lần: %d). Trở lại Deep Sleep...", tap_count);
    return false;
}

/* ===================================================================
 *  7. ĐIỂM BẮT ĐẦU CHƯƠNG TRÌNH (Application Entry Point)
 * =================================================================== */
void app_main(void) {
    /* Đặt mức log chi tiết cho I2C để thuận tiện chẩn đoán thiết bị */
    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_INFO);
    esp_log_level_set("CST816S", ESP_LOG_INFO);

    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  BẮT ĐẦU CHƯƠNG TRÌNH CHÍNH SMARTWATCH  ");
    ESP_LOGI(TAG, "=========================================");

    /* Khởi tạo phân vùng bộ nhớ không bay hơi Flash NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Đọc cấu hình tùy chọn và nhật ký hoạt động */
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_activity_log_init());
    const watch_settings_t *settings = watch_settings_get();

    /* --- Kiểm tra nguyên nhân đánh thức từ trạng thái Deep Sleep --- */
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
        /* Nếu đánh thức bằng cảm ứng: Sử dụng thuật toán Triple Tap chống đánh thức giả trong túi quần */
        if (!settings->quick_wake_enabled || !watch_detect_triple_tap()) {
            gpio_config_t touch_pin_cfg = {
                .pin_bit_mask = 1ULL << WATCH_PIN_TOUCH_INT,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&touch_pin_cfg);

            gpio_config_t btn_pin_cfg = {
                .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&btn_pin_cfg);

            /* Đợi thả tay khỏi màn hình cảm ứng hoặc nút nguồn */
            for (int i = 0; i < 25 && gpio_get_level(WATCH_PIN_TOUCH_INT) == 0; i++) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            for (int i = 0; i < 25 && gpio_get_level(WATCH_PIN_BUTTON_POWER) == 0; i++) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            /* Vô hiệu hóa toàn bộ ngắt cũ */
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

            /* Thiết lập ngắt đánh thức bằng nút nguồn vật lý */
            esp_sleep_enable_ext1_wakeup((1ULL << WATCH_PIN_BUTTON_POWER), ESP_EXT1_WAKEUP_ANY_LOW);

            /* Thiết lập ngắt đánh thức bằng cảm ứng nếu bật tính năng Đánh thức nhanh */
            if (settings->quick_wake_enabled) {
                esp_sleep_enable_ext0_wakeup((gpio_num_t)WATCH_PIN_TOUCH_INT, 0);
            }

            /* Dừng cảm biến đo đạc để tắt nguồn hoàn toàn */
            watch_sensors_stop_gps_imu();
            watch_hw_monitor_set_hr_measure_enabled(false);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_deep_sleep_start(); // Trở lại giấc ngủ sâu để tiết kiệm năng lượng
        }
    }

    /* Thiết lập độ sáng từ bộ nhớ */
    uint32_t backlight_duty = (uint32_t)settings->brightness_pct * 255U / 100U;
    ui_backlight_set_active_duty(backlight_duty);
    watch_backlight_init(backlight_duty);
    
    /* Khởi tạo phần cứng cơ bản */
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_vibration_init());

    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
    watch_lcd_init(&panel_handle, &io_handle);

    esp_lcd_touch_handle_t touch_handle;
    watch_touch_init(&touch_handle);

    /* Thiết lập giao diện đồ họa LVGL */
    watch_lvgl_port_init(panel_handle, io_handle, touch_handle);

    /* --- Khởi tạo các dịch vụ nền (Subsystems) ---
     * Lưu ý: Không đặt các hàm này bên trong khóa lvgl_port_lock()
     * để tránh chặn luồng hiển thị gây lag / treo.
     */
    watch_net_init(); // WiFi phục vụ nâng cấp OTA
    ESP_LOGI(TAG, "Mạng Wi-Fi đã khởi tạo");

    ESP_ERROR_CHECK(watch_ota_init());
    ESP_LOGI(TAG, "Dịch vụ OTA sẵn sàng");

    /* Gắn bộ thu tin nhắn báo BLE */
    watch_ble_notify_set_new_cb(watch_ble_notification_cb);

    /* Cài đặt thời gian timeout tắt màn hình và cảm biến đo nhịp tim */
    watch_hw_monitor_set_screen_timeout(settings->screen_timeout_sec);
    watch_hw_monitor_set_quick_wake_enabled(settings->quick_wake_enabled);
    ESP_ERROR_CHECK(watch_hw_monitor_start());

    /* Khóa luồng đồ họa để tạo các đối tượng vẽ trong LVGL an toàn */
    if (lvgl_port_lock(-1)) {
        ui_language_set((ui_language_t)settings->language);
        ui_watchface_set_style(settings->watchface_style);
        ui_menu_set_system_color(settings->icons_monochrome,
                                 watch_color_from_id(settings->icon_color));
        ui_init();
        ui_notif_popup_init();
        lvgl_port_unlock(); // Giải phóng khóa đồ họa
    }

#if CONFIG_WATCH_HEAP_MONITOR
    xTaskCreatePinnedToCore(watch_heap_monitor_task, "heap_mon", 4096, NULL, 1, NULL, 0);
#endif
    /* Khởi chạy đa nhiệm gửi dữ liệu Telemetry lên điện thoại */
    xTaskCreatePinnedToCore(watch_ble_telemetry_task, "ble_tel", 8192, NULL, 1, NULL, 0);

    ESP_LOGI(TAG, "Đồng hồ thông minh khởi chạy thành công!");
}
