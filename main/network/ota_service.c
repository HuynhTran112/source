/**
 * @file ota_service.c
 * @brief Hiện thực dịch vụ nâng cấp phần mềm qua mạng HTTPS sử dụng API của Espressif
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Chi tiết các bước thực hiện trong Task OTA (watch_ota_task):
 * 1. Thiết lập kết nối HTTPS bảo mật bằng cách đính kèm CA Certificate Bundle của Espressif,
 *    giúp xác thực chứng chỉ SSL của máy chủ tải phần mềm.
 * 2. Gọi hàm `esp_https_ota_begin` để thiết lập bắt tay HTTP và chuẩn bị phân vùng ghi.
 * 3. Đọc mô tả ảnh nhị phân (`esp_https_ota_get_img_desc`): Trích xuất và so sánh phiên bản phần mềm.
 * 4. Vòng lặp tải về (`esp_https_ota_perform`): MCU thực hiện tải từng phân mảnh nhỏ qua mạng, ghi tuần tự vào Flash
 *    cho đến khi nhận đủ toàn bộ file.
 * 5. Xác thực toàn vẹn: Gọi `esp_https_ota_finish` để kiểm tra chữ ký số và mã băm SHA256 của file nhị phân.
 *    Nếu khớp, chip sẽ đổi bit bootloader để lần khởi động tiếp theo nạp phân vùng mới này.
 * 6. Tự động hủy Task và báo cáo tiến trình về giao diện.
 */

#include "ota_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "watch_settings.h"

#ifndef CONFIG_OTA_FIRMWARE_URL
#define CONFIG_OTA_FIRMWARE_URL ""
#endif

static const char *TAG = "OTA_SERVICE";
static SemaphoreHandle_t s_mutex = NULL; // Khóa đồng bộ trạng thái luồng
static portMUX_TYPE s_init_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task = NULL;        // Handle quản lý Task OTA FreeRTOS
static bool s_task_starting = false;
static watch_ota_status_t s_status = {
    .state = WATCH_OTA_IDLE,
    .progress_percent = -1,
};
static char s_ota_url[256];               // Bộ đệm lưu trữ địa chỉ URL tải firmware

esp_err_t watch_ota_init(void) {
    if (s_mutex) return ESP_OK;

    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) return ESP_ERR_NO_MEM;

    taskENTER_CRITICAL(&s_init_lock);
    if (!s_mutex) {
        s_mutex = new_mutex;
        new_mutex = NULL;
    }
    taskEXIT_CRITICAL(&s_init_lock);

    if (new_mutex) {
        vSemaphoreDelete(new_mutex);
    }
    return ESP_OK;
}

static void watch_ota_lock(void) {
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}

static void watch_ota_unlock(void) {
    if (s_mutex) xSemaphoreGive(s_mutex);
}

/**
 * @brief Ghi đè trạng thái tiến độ hiện hành (Yêu cầu phải khóa Mutex trước khi gọi)
 */
static void watch_ota_write_status_locked(watch_ota_state_t state, int progress_percent,
                                          int bytes_read, int image_size, esp_err_t err,
                                          const char *message) {
    s_status.state = state;
    s_status.progress_percent = progress_percent;
    s_status.bytes_read = bytes_read;
    s_status.image_size = image_size;
    s_status.last_error = err;
    if (message) {
        snprintf(s_status.message, sizeof(s_status.message), "%s", message);
    }
}

/**
 * @brief Cập nhật trạng thái tiến độ an toàn đa luồng (Thread-safe)
 */
static void watch_ota_set_status(watch_ota_state_t state, int progress_percent,
                                 int bytes_read, int image_size, esp_err_t err,
                                 const char *message) {
    watch_ota_lock();
    watch_ota_write_status_locked(state, progress_percent, bytes_read, image_size, err, message);
    watch_ota_unlock();
}

bool watch_ota_url_configured(void) {
    char url[sizeof(s_ota_url)] = {0};
    if (watch_settings_get_ota_url(url, sizeof(url)) == ESP_OK && url[0] != '\0') {
        return true;
    }
    return CONFIG_OTA_FIRMWARE_URL[0] != '\0';
}

void watch_ota_get_status(watch_ota_status_t *out) {
    if (!out) return;
    if (watch_ota_init() != ESP_OK) return;
    
    watch_ota_lock();
    *out = s_status;
    watch_ota_unlock();
}

/**
 * @brief Tác vụ FreeRTOS phụ chuyên trách xử lý giao thức HTTPS OTA tải phần mềm
 */
static void watch_ota_task(void *arg) {
    (void)arg;
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    watch_ota_lock();
    s_task = current_task;
    s_task_starting = false;
    watch_ota_unlock();

    /* Thiết lập cấu hình kết nối HTTP */
    esp_http_client_config_t http_config = {
        .url = s_ota_url,
        .crt_bundle_attach = esp_crt_bundle_attach, // Đính kèm bộ chứng chỉ số root bảo mật
        .timeout_ms = 15000,                        // Timeout kết nối mạng 15 giây
        .keep_alive_enable = true,                  // Giữ kết nối liên tục
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    esp_https_ota_handle_t handle = NULL;

    watch_ota_set_status(WATCH_OTA_RUNNING, 0, 0, 0, ESP_OK, "Đang kết nối máy chủ...");

    /* Bắt đầu quá trình nạp OTA */
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Giao tiếp HTTPS OTA lỗi khởi đầu: %s", esp_err_to_name(err));
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, err, "Không thể kết nối máy chủ");
        goto done;
    }

    /* Đọc thông tin mô tả phiên bản nhị phân của file tải về */
    esp_app_desc_t app_desc = {0};
    if (esp_https_ota_get_img_desc(handle, &app_desc) == ESP_OK) {
        watch_ota_lock();
        snprintf(s_status.new_version, sizeof(s_status.new_version), "%s", app_desc.version);
        watch_ota_unlock();
    }

    int image_size = esp_https_ota_get_image_size(handle);
    watch_ota_set_status(WATCH_OTA_RUNNING, 0, 0, image_size, ESP_OK, "Đang tải firmware mới...");

    /* Vòng lặp tải và ghi dữ liệu nhị phân trực tiếp vào phân vùng Flash */
    do {
        err = esp_https_ota_perform(handle);
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        image_size = esp_https_ota_get_image_size(handle);
        
        int progress = -1;
        if (image_size > 0) {
            progress = (bytes_read * 100) / image_size;
            if (progress > 100) progress = 100;
        }
        watch_ota_set_status(WATCH_OTA_RUNNING, progress, bytes_read, image_size, err,
                             "Đang ghi dữ liệu vào Flash...");
    } while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Tải file phần mềm bị lỗi ngắt quãng: %s", esp_err_to_name(err));
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        esp_https_ota_abort(handle); // Hủy bỏ và dọn dẹp phân vùng lỗi
        watch_ota_set_status(WATCH_OTA_FAILED, -1, bytes_read, image_size, err,
                             "Lỗi tải dữ liệu");
        goto done;
    }

    /* Xác thực gói dữ liệu nhận được có toàn vẹn không */
    if (!esp_https_ota_is_complete_data_received(handle)) {
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        esp_https_ota_abort(handle);
        watch_ota_set_status(WATCH_OTA_FAILED, -1, bytes_read, image_size, ESP_FAIL,
                             "Lỗi: Thiếu gói tin dữ liệu!");
        goto done;
    }

    int final_bytes_read = esp_https_ota_get_image_len_read(handle);
    err = esp_https_ota_finish(handle); // Đóng tiến trình, hoán đổi phân vùng khởi động
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Kiểm tra mã băm SHA256/chữ ký lỗi: %s", esp_err_to_name(err));
        watch_ota_set_status(WATCH_OTA_FAILED, -1, final_bytes_read, image_size, err,
                             "Chữ ký số firmware không hợp lệ");
        goto done;
    }

    watch_ota_set_status(WATCH_OTA_SUCCEEDED, 100, image_size, image_size, ESP_OK,
                         "Nâng cấp thành công! Đang tự động khởi động lại...");

done:
    watch_ota_lock();
    if (s_task == current_task) {
        s_task = NULL;
    }
    s_task_starting = false;
    watch_ota_unlock();
    vTaskDelete(NULL); // Hủy tác vụ khi kết thúc
}

esp_err_t watch_ota_start(void) {
    esp_err_t init_err = watch_ota_init();
    if (init_err != ESP_OK) return init_err;

    s_ota_url[0] = '\0';
    if (watch_settings_get_ota_url(s_ota_url, sizeof(s_ota_url)) != ESP_OK || s_ota_url[0] == '\0') {
        snprintf(s_ota_url, sizeof(s_ota_url), "%s", CONFIG_OTA_FIRMWARE_URL);
    }

    if (s_ota_url[0] == '\0') {
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, ESP_ERR_INVALID_ARG,
                             "Lỗi: Chưa thiết lập địa chỉ máy chủ tải!");
        return ESP_ERR_INVALID_ARG;
    }

    watch_ota_lock();
    if (s_task || s_task_starting) {
        watch_ota_unlock();
        return ESP_ERR_INVALID_STATE; // Lỗi: Tác vụ đang chạy rồi
    }
    watch_ota_write_status_locked(WATCH_OTA_RUNNING, 0, 0, 0, ESP_OK, "Đang khởi chạy tiến trình OTA...");
    s_status.new_version[0] = '\0';
    s_task_starting = true;
    watch_ota_unlock();

    /* Khởi tạo Task OTA mới, gán độ ưu tiên 4 giống LVGL để thực hiện mượt mà */
    BaseType_t ok = xTaskCreate(watch_ota_task, "watch_ota_task", 8192, NULL, 4, NULL);
    if (ok != pdPASS) {
        watch_ota_lock();
        s_task_starting = false;
        watch_ota_write_status_locked(WATCH_OTA_FAILED, -1, 0, 0, ESP_ERR_NO_MEM,
                                      "Không đủ RAM khởi chạy Task OTA");
        watch_ota_unlock();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
