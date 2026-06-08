/**
 * @file vibration_motor.c
 * @brief Hiện thực các hàm điều khiển động cơ rung sử dụng bộ phát xung PWM LEDC
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Driver sử dụng tính năng phát xung PWM của bộ ngoại vi LEDC (LED Controller)
 * kết hợp với cơ chế đa nhiệm FreeRTOS Task để tạo ra các kiểu rung không đồng bộ (Asynchronous),
 * tránh block luồng xử lý đồ họa UI chính.
 */

#include "vibration_motor.h"
#include "board_config.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "VIB_MOTOR";

static TaskHandle_t s_vibration_task = NULL; // Biến lưu luồng điều khiển rung hiện hành
static bool s_initialized = false;           // Trạng thái khởi tạo driver

/* Định nghĩa các kiểu rung của đồng hồ */
typedef enum {
    VIBRATION_PATTERN_PULSE,   // Kiểu rung đơn ngắn (ví dụ: phản hồi nút nhấn)
    VIBRATION_PATTERN_NOTIFY,  // Kiểu rung kép ngắn (ví dụ: thông báo có tin nhắn)
    VIBRATION_PATTERN_ALARM,   // Kiểu rung lặp lại chu kỳ lâu (dùng cho báo thức)
} vibration_pattern_t;

/* Cấu trúc truyền tải lệnh điều khiển cho tác vụ FreeRTOS */
typedef struct {
    vibration_pattern_t pattern; // Kiểu mẫu rung
    uint8_t strength;            // Cường độ rung (0: yếu, 1: trung bình, 2: mạnh)
    uint32_t duration_ms;        // Thời lượng rung (chỉ áp dụng cho kiểu PULSE)
} vibration_cmd_t;

/**
 * @brief Quy đổi mức cường độ (0, 1, 2) sang giá trị thanh ghi Duty của bộ Timer LEDC 10-bit (0-1023)
 * @param strength Cường độ rung của người dùng cài đặt
 * @return uint32_t Giá trị độ rộng xung tương ứng
 */
static uint32_t vibration_duty_for_strength(uint8_t strength) {
    switch (strength) {
        case 0:  return 360;  // Mức rung yếu (~35% điện áp định mức)
        case 2:  return 900;  // Mức rung mạnh nhất (~88% điện áp định mức)
        case 1:
        default: return 640;  // Mức rung trung bình (~62% điện áp định mức)
    }
}

/**
 * @brief Bật motor rung bằng cách xuất xung PWM
 */
static void vibration_set_on(uint8_t strength) {
    if (!s_initialized) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH,
                  vibration_duty_for_strength(strength));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH);
}

/**
 * @brief Tắt động cơ rung (đặt Duty về 0)
 */
static void vibration_set_off(void) {
    if (!s_initialized) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH);
}

/**
 * @brief Tác vụ FreeRTOS thực thi luồng rung tương tác phần cứng
 */
static void vibration_task(void *arg) {
    vibration_cmd_t cmd = *(vibration_cmd_t *)arg;
    free(arg); // Giải phóng vùng nhớ heap cấp phát cho tham số lệnh

    switch (cmd.pattern) {
        case VIBRATION_PATTERN_NOTIFY:
            /* Rung kép ngắn: Rung 90ms -> Nghỉ 60ms -> Rung 90ms */
            vibration_set_on(cmd.strength);
            vTaskDelay(pdMS_TO_TICKS(90));
            vibration_set_off();
            vTaskDelay(pdMS_TO_TICKS(60));
            vibration_set_on(cmd.strength);
            vTaskDelay(pdMS_TO_TICKS(90));
            vibration_set_off();
            break;
        case VIBRATION_PATTERN_ALARM:
            /* Rung báo thức lặp tuần hoàn đến khi có lệnh dừng: Rung 650ms -> Nghỉ 350ms */
            while (true) {
                vibration_set_on(cmd.strength);
                vTaskDelay(pdMS_TO_TICKS(650));
                vibration_set_off();
                vTaskDelay(pdMS_TO_TICKS(350));
            }
            break;
        case VIBRATION_PATTERN_PULSE:
        default:
            /* Rung nhịp đơn: Rung theo thời gian chỉ định -> Tắt */
            vibration_set_on(cmd.strength);
            vTaskDelay(pdMS_TO_TICKS(cmd.duration_ms ? cmd.duration_ms : 180));
            vibration_set_off();
            break;
    }

    s_vibration_task = NULL;
    vTaskDelete(NULL); // Tự hủy tác vụ khi thực hiện xong chuỗi rung
}

esp_err_t watch_vibration_init(void) {
    /* Cấu hình bộ Timer phát xung PWM tần số 200 Hz, phân giải 10-bit */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = WATCH_VIBRATOR_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = WATCH_VIBRATOR_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) return err;

    /* Cấu hình kênh đầu ra cho chân cắm GPIO động cơ rung */
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = WATCH_VIBRATOR_LEDC_CH,
        .timer_sel = WATCH_VIBRATOR_LEDC_TIMER,
        .gpio_num = WATCH_PIN_VIBRATOR,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) return err;

    s_initialized = true;
    ESP_LOGI(TAG, "Động cơ rung đã cấu hình thành công trên GPIO %d", WATCH_PIN_VIBRATOR);
    return ESP_OK;
}

/**
 * @brief Khởi động một chu kỳ rung mới trong hệ thống
 */
static void vibration_start(vibration_pattern_t pattern, uint8_t strength, uint32_t duration_ms) {
    if (!s_initialized) return;
    watch_vibration_stop(); // Hủy bỏ bất kỳ tiến trình rung nào đang chạy để đồng bộ

    /* Cấp phát vùng nhớ heap cho gói tin lệnh */
    vibration_cmd_t *cmd = malloc(sizeof(*cmd));
    if (!cmd) return;
    cmd->pattern = pattern;
    cmd->strength = strength;
    cmd->duration_ms = duration_ms;

    /* Tạo một Task có độ ưu tiên thấp (priority 3) để thực hiện rung bất tuần tự */
    if (xTaskCreate(vibration_task, "vibration_task", 2048, cmd, 3, &s_vibration_task) != pdPASS) {
        free(cmd);
        s_vibration_task = NULL;
        vibration_set_off();
    }
}

void watch_vibration_pulse(uint8_t strength, uint32_t duration_ms) {
    vibration_start(VIBRATION_PATTERN_PULSE, strength, duration_ms);
}

void watch_vibration_notify(uint8_t strength) {
    vibration_start(VIBRATION_PATTERN_NOTIFY, strength, 0);
}

void watch_vibration_alarm_start(uint8_t strength) {
    vibration_start(VIBRATION_PATTERN_ALARM, strength, 0);
}

void watch_vibration_stop(void) {
    if (s_vibration_task) {
        TaskHandle_t task = s_vibration_task;
        s_vibration_task = NULL;
        vTaskDelete(task); // Dừng ngang tác vụ FreeRTOS điều khiển rung
    }
    vibration_set_off(); // Tắt hoàn toàn dòng cấp
}
