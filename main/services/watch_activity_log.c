/**
 * @file watch_activity_log.c
 * @brief Hiện thực hệ thống quản lý cơ sở dữ liệu lịch sử hoạt động thể thao
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Chi tiết thiết kế:
 * 1. Vì mỗi bản ghi lưu trữ hành trình (GPS route) chiếm tới 49 KB RAM, cấu trúc lưu trữ
 *    `watch_activity_store_t` có tổng kích thước lên đến gần 150 KB. Ta bắt buộc phải phân bổ
 *    vùng nhớ này trên PSRAM (SPIRAM) bằng hàm `heap_caps_calloc` để tránh gây tràn bộ nhớ Internal RAM.
 * 2. Lưu trữ không bay hơi (NVS): Dữ liệu được tuần tự hóa (Serialization) và lưu vào Flash NVS
 *    dưới dạng một vùng nhớ thô (Blob) với khóa `"activities"` trong không gian `"watch"`.
 * 3. Quản lý bản ghi: Khi ghi nhận một phiên tập luyện mới, hệ thống dịch chuyển các bản ghi cũ xuống dưới,
 *    đè đè bản ghi mới nhất vào phần tử 0 (cơ chế dịch mảng LIFO). Nếu số lượng bản ghi vượt quá 3,
 *    bản ghi cũ nhất sẽ tự động bị xóa bỏ.
 */

#include "watch_activity_log.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"

#define WATCH_ACTIVITY_NS       "watch"       // Tên không gian tên (Namespace) lưu trữ trong NVS
#define WATCH_ACTIVITY_KEY      "activities"  // Khóa (Key) định danh bản ghi lưu trong NVS
#define WATCH_ACTIVITY_MAGIC    0x57414354U   // Mã định dạng Magic để kiểm tra tính toàn vẹn ("WATC")
#define WATCH_ACTIVITY_VERSION  2U            // Phiên bản dữ liệu cấu trúc lưu trữ

static const char *TAG = "ACT_LOG";

/* Cấu trúc đóng gói toàn bộ bản tin lưu trữ để ghi Flash */
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    watch_activity_record_t records[WATCH_ACTIVITY_LOG_MAX];
} watch_activity_store_t;

static watch_activity_store_t *s_store = NULL; // Con trỏ quản lý cơ sở dữ liệu trong bộ nhớ RAM

/**
 * @brief Kiểm tra và cấp phát bộ nhớ đệm cơ sở dữ liệu trên PSRAM nếu chưa có
 * @return true Cấp phát thành công
 */
static bool activity_log_ensure_store(void) {
    if (s_store) return true;
    /* Thực hiện cấp phát trên PSRAM để tối ưu bộ nhớ nội */
    s_store = heap_caps_calloc(1, sizeof(*s_store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_store) {
        s_store = calloc(1, sizeof(*s_store)); // Dự phòng dùng Internal RAM nếu lỗi PSRAM
    }
    if (!s_store) return false;
    
    s_store->magic = WATCH_ACTIVITY_MAGIC;
    s_store->version = WATCH_ACTIVITY_VERSION;
    return true;
}

/**
 * @brief Ghi đồng bộ dữ liệu s_store từ RAM vào phân vùng Flash NVS
 */
static esp_err_t activity_log_save(void) {
    if (!s_store) return ESP_ERR_INVALID_STATE;
    nvs_handle_t nvs;
    
    esp_err_t err = nvs_open(WATCH_ACTIVITY_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    
    /* Ghi đè toàn bộ cấu trúc dữ liệu thô (Blob) */
    err = nvs_set_blob(nvs, WATCH_ACTIVITY_KEY, s_store, sizeof(*s_store));
    if (err == ESP_OK) {
        err = nvs_commit(nvs); // Xác nhận lưu thay đổi
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_activity_log_init(void) {
    if (!activity_log_ensure_store()) return ESP_ERR_NO_MEM;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_ACTIVITY_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    /* Cấp phát vùng nhớ đệm tạm thời để đọc dữ liệu từ NVS */
    watch_activity_store_t *stored = heap_caps_malloc(sizeof(*stored), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!stored) {
        stored = malloc(sizeof(*stored));
    }
    if (!stored) {
        nvs_close(nvs);
        return ESP_ERR_NO_MEM;
    }

    size_t len = sizeof(*stored);
    err = nvs_get_blob(nvs, WATCH_ACTIVITY_KEY, stored, &len);
    nvs_close(nvs);

    /* Xác thực tính toàn vẹn của dữ liệu đọc được từ Flash NVS */
    if (err == ESP_OK && len == sizeof(*stored) &&
        stored->magic == WATCH_ACTIVITY_MAGIC &&
        stored->version == WATCH_ACTIVITY_VERSION &&
        stored->count <= WATCH_ACTIVITY_LOG_MAX) {
        *s_store = *stored; // Copy dữ liệu vào vùng đệm hoạt động chính
        free(stored);
        ESP_LOGI(TAG, "Đã nạp thành công lịch sử tập luyện (%d bản ghi) từ Flash NVS", s_store->count);
        return ESP_OK;
    }

    free(stored);

    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Lỗi đọc dữ liệu lịch sử từ NVS: %s, tiến hành tạo mới cơ sở dữ liệu...", esp_err_to_name(err));
    }
    
    /* Thiết lập cơ sở dữ liệu trống ban đầu */
    memset(s_store, 0, sizeof(*s_store));
    s_store->magic = WATCH_ACTIVITY_MAGIC;
    s_store->version = WATCH_ACTIVITY_VERSION;
    return activity_log_save();
}

esp_err_t watch_activity_log_add(const watch_activity_record_t *record) {
    if (!record) return ESP_ERR_INVALID_ARG;
    if (!activity_log_ensure_store()) return ESP_ERR_NO_MEM;

    /* Dịch chuyển mảng bản ghi: records[1] -> records[2], records[0] -> records[1] */
    for (int i = WATCH_ACTIVITY_LOG_MAX - 1; i > 0; i--) {
        s_store->records[i] = s_store->records[i - 1];
    }
    
    /* Điền bản ghi mới nhất vào phần tử đầu tiên (index 0) */
    s_store->records[0] = *record;
    s_store->records[0].route[WATCH_ACTIVITY_ROUTE_MAX - 1] = '\0'; // Đảm bảo kết thúc chuỗi an toàn
    
    if (s_store->count < WATCH_ACTIVITY_LOG_MAX) {
        s_store->count++;
    }
    
    ESP_LOGI(TAG, "Đã lưu bản ghi hoạt động thể thao mới vào Flash NVS!");
    return activity_log_save();
}

size_t watch_activity_log_count(void) {
    return s_store ? s_store->count : 0;
}

size_t watch_activity_log_get(watch_activity_record_t *out, size_t max_records) {
    if (!out || max_records == 0 || !s_store) return 0;
    size_t n = s_store->count < max_records ? s_store->count : max_records;
    memcpy(out, s_store->records, n * sizeof(s_store->records[0]));
    return n;
}
