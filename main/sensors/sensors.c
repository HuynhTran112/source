/**
 * @file sensors.c
 * @brief Hiện thực hệ thống quản lý, đọc dữ liệu GPS và xử lý hành trình
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Hướng dẫn các thuật toán cốt lõi:
 * 1. Tự động dò Baudrate (Auto-baud rate): Nhiều module GPS thương mại khởi động ở các tốc độ baudrate
 *    khác nhau (9600, 38400, 115200 bps). Khi khởi chạy, hệ thống sẽ mở cổng UART lần lượt ở các baudrate này,
 *    nếu trong 10 giây không phát hiện chuỗi NMEA hợp lệ thì tự động nhảy sang baudrate tiếp theo.
 * 2. Thuật toán khoảng cách Haversine: Tính khoảng cách đường cong mặt cầu giữa hai điểm tọa độ (vĩ độ, kinh độ)
 *    trên Trái Đất:
 *       a = sin^2(dLat/2) + cos(Lat1) * cos(Lat2) * sin^2(dLon/2)
 *       c = 2 * arcsin(sqrt(a))
 *       d = R * c (với bán kính Trái Đất R = 6,371,000 mét)
 * 3. Bộ lọc chống trôi GPS (GPS Drift Filter): Tránh tình trạng quãng đường tự tăng khi người dùng đứng yên
 *    bằng cách bỏ qua các dịch chuyển < 0.5 mét, đồng thời loại bỏ các điểm nhảy tọa độ sai số vật lý bằng cách
 *    giới hạn vận tốc tối đa di chuyển của người đi bộ/xe đạp (< 25 m/s tương đương 90 km/h).
 */

#include "sensors.h"
#include "gps_nmea.h"
#include "board_config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "SENSORS_GPS";

/* Danh sách các tốc độ Baudrate để dò tự động */
static const int s_gps_baud_candidates[] = {
    WATCH_GPS_UART_BAUD,
    WATCH_GPS_UART_BAUD_FALLBACK_1,
    WATCH_GPS_UART_BAUD_FALLBACK_2,
};

#define GPS_MUTEX_TIMEOUT_MS          100  // Thời gian tối đa đợi Mutex
#define GPS_MIN_SEGMENT_DISTANCE_M    0.5  // Ngưỡng lọc thông thấp: Quãng đường di chuyển tối thiểu để tích lũy (mét)
#define GPS_MAX_SEGMENT_SPEED_MPS     25.0 // Tốc độ di chuyển tối đa cho phép để tránh giật tọa độ (25m/s = 90km/h)
#define GPS_UART_RX_BUFFER_SIZE       4096 // Kích thước buffer nhận của bộ đệm UART
#define GPS_HEARTBEAT_INTERVAL_MS     5000 // Chu kỳ in log trạng thái GPS ra console debug (5 giây)

/* Quản lý tranh chấp dữ liệu bằng Mutex và Spinlock */
static portMUX_TYPE s_gps_state_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_gps_task = NULL;
static SemaphoreHandle_t s_gps_mutex = NULL;
static SemaphoreHandle_t s_gps_stopped_sem = NULL; // Semaphore đồng bộ hóa khi tắt Task
static bool s_gps_running = false;
static bool s_gps_uart_installed = false;

/* Các biến dữ liệu định vị dùng chung */
static watch_gps_metrics_t s_metrics = {0};
static uint32_t s_last_rx_seen_ms = 0;

/* Các biến lưu trạng thái phục vụ giải thuật Haversine */
static double s_prev_lat = 0.0;
static double s_prev_lon = 0.0;
static bool s_has_prev_coords = false;
static uint32_t s_last_gps_update_ms = 0;
static watch_gps_track_point_t s_track[WATCH_GPS_TRACK_MAX_POINTS];
static size_t s_track_count = 0;
static float s_last_track_point_km = 0.0f;

/* Các biến phục vụ máy trạng thái Auto-baud */
static uint32_t s_uart_activity_since_baud_ms = 0;
static bool s_uart_idle_logged = false;
static bool s_logged_first_nmea = false;
static int s_gps_baud_index = 0;
static int s_last_good_baud = WATCH_GPS_UART_BAUD;
static bool s_has_last_good_baud = false;

/**
 * @brief Đặt lại mốc tính quãng đường (baseline) khi mất tín hiệu hoặc bắt đầu phiên mới
 */
static void gps_reset_distance_baseline_locked(void) {
    s_has_prev_coords = false;
    s_prev_lat = 0.0;
    s_prev_lon = 0.0;
    s_last_gps_update_ms = 0;
}

/**
 * @brief Lấy thời gian chạy của hệ thống tính bằng miligiây
 */
static uint32_t watch_uptime_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/**
 * @brief Lấy quyền khóa Mutex để truy cập dữ liệu GPS
 */
static bool gps_lock(const char *context) {
    if (!s_gps_mutex) return false;
    if (xSemaphoreTake(s_gps_mutex, pdMS_TO_TICKS(GPS_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        return true;
    }
    ESP_LOGW(TAG, "Thời gian chờ Mutex quá lâu tại ngữ cảnh: %s", context ? context : "Không rõ");
    return false;
}

/**
 * @brief Giải phóng khóa Mutex GPS
 */
static void gps_unlock(void) {
    if (s_gps_mutex) xSemaphoreGive(s_gps_mutex);
}

/**
 * @brief Ghi nhận thêm một điểm tọa độ mới vào nhật ký hành trình (Track Log)
 */
static void gps_append_track_point_locked(double lat, double lon, float distance_km) {
    if (fabs(lat) <= 0.0001 || fabs(lon) <= 0.0001) return;
    
    /* Chỉ lưu điểm tọa độ mới nếu quãng đường đi được từ điểm cũ tăng thêm ít nhất 5 mét
     * nhằm tiết kiệm bộ nhớ RAM giới hạn (MAX 2048 điểm) */
    if (s_track_count > 0 && (distance_km - s_last_track_point_km) < 0.005f) return;

    watch_gps_track_point_t p = {
        .latitude_deg = lat,
        .longitude_deg = lon,
    };
    
    if (s_track_count < WATCH_GPS_TRACK_MAX_POINTS) {
        s_track[s_track_count++] = p;
    } else {
        /* Bộ đệm cuộn vòng (Circular Buffer): Đẩy các điểm cũ ra ngoài để chèn điểm mới vào cuối */
        memmove(&s_track[0], &s_track[1], sizeof(s_track[0]) * (WATCH_GPS_TRACK_MAX_POINTS - 1));
        s_track[WATCH_GPS_TRACK_MAX_POINTS - 1] = p;
    }
    s_last_track_point_km = distance_km;
}

static bool gps_is_running(void) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    bool running = s_gps_running;
    taskEXIT_CRITICAL(&s_gps_state_lock);
    return running;
}

static void gps_set_running(bool running) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    s_gps_running = running;
    taskEXIT_CRITICAL(&s_gps_state_lock);
}

static TaskHandle_t gps_get_task(void) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    TaskHandle_t task = s_gps_task;
    taskEXIT_CRITICAL(&s_gps_state_lock);
    return task;
}

static void gps_set_task(TaskHandle_t task) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    s_gps_task = task;
    taskEXIT_CRITICAL(&s_gps_state_lock);
}

static int gps_current_baud(void) {
    return s_gps_baud_candidates[s_gps_baud_index];
}

static void gps_reset_autobaud_state(void) {
    s_gps_baud_index = 0;
    s_logged_first_nmea = false;
    s_uart_activity_since_baud_ms = 0;
    s_uart_idle_logged = false;
    s_last_rx_seen_ms = 0;
}

/**
 * @brief Đưa ra nhãn đánh giá chất lượng tín hiệu định vị phục vụ hiển thị UI
 */
static const char *gps_quality_label(const watch_gps_metrics_t *m, uint32_t age_ms) {
    if (!m || !m->uart_seen) return "MẤT_UART";
    if (age_ms > 3000) return "MẤT_TÍN_HIỆU";
    if (!m->nmea_seen) return "LỖI_NMEA";
    if (!m->fix_valid) return "ĐANG_TÌM_VỆ_TINH";
    if (m->satellites >= 8 && m->hdop > 0.0f && m->hdop <= 1.5f) return "TỐT";
    if (m->satellites >= 5 && m->hdop > 0.0f && m->hdop <= 3.0f) return "TRUNG_BÌNH";
    return "YẾU";
}

/**
 * @brief Nhảy sang Baudrate tiếp theo trong danh sách cấu hình
 */
static void gps_switch_to_next_baud(void) {
    s_gps_baud_index = (s_gps_baud_index + 1) % (sizeof(s_gps_baud_candidates) / sizeof(s_gps_baud_candidates[0]));
    uart_set_baudrate(WATCH_GPS_UART_PORT, gps_current_baud());
    uart_flush_input(WATCH_GPS_UART_PORT);

    if (gps_lock("chuyển đổi baudrate")) {
        s_metrics.current_baud = gps_current_baud();
        gps_unlock();
    }

    s_logged_first_nmea = false;
    s_uart_activity_since_baud_ms = 0;
    ESP_LOGW(TAG, "Dò Baudrate GPS: Chuyển sang tốc độ %d bps", gps_current_baud());
}

/**
 * @brief Gửi gói cấu hình dạng chuỗi lệnh PMTK độc quyền của hãng MediaTek sang module GPS
 */
static bool gps_send_pmtk(const char *command) {
    if (!command || command[0] != '$') return false;

    /* Tính checksum XOR của lệnh */
    uint8_t checksum = 0;
    for (const char *p = command + 1; *p && *p != '*'; p++) {
        checksum ^= (uint8_t)*p;
    }

    char message[96];
    int length = snprintf(message, sizeof(message), "%s*%02X\r\n", command, checksum);
    if (length <= 0 || length >= (int)sizeof(message)) {
        ESP_LOGE(TAG, "Lệnh PMTK quá dài!");
        return false;
    }

    int written = uart_write_bytes(WATCH_GPS_UART_PORT, message, length);
    if (written != length) {
        ESP_LOGW(TAG, "Lỗi ghi UART gửi lệnh PMTK: ghi %d/%d bytes", written, length);
        return false;
    }
    if (uart_wait_tx_done(WATCH_GPS_UART_PORT, pdMS_TO_TICKS(200)) != ESP_OK) {
        ESP_LOGW(TAG, "Timeout truyền lệnh PMTK qua UART");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}

/**
 * @brief Thiết lập tần số lấy mẫu và loại bản tin xuất của module GPS
 */
static void gps_configure_module(void) {
    /* Cấu hình tần số cập nhật dữ liệu định vị: 5 Hz (5 lần quét mỗi giây) */
    bool rate_ok = gps_send_pmtk("$PMTK220,200");
    /* Ép module GPS chỉ xuất các bản tin RMC và GGA (bỏ qua các bản tin rác GSA, GSV để giảm tải xử lý cho ESP32) */
    bool output_ok = gps_send_pmtk("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    ESP_LOGI(TAG, "Cấu hình PMTK: Tần số quét 5Hz = %s, Lọc xuất bản tin = %s",
             rate_ok ? "Thành công" : "Thất bại",
             output_ok ? "Thành công" : "Thất bại");
}

/**
 * @brief Xử lý phân tích dòng NMEA thu được và tính toán khoảng cách di chuyển
 */
static void gps_parse_nmea_line(char *line) {
    if (!line || line[0] != '$') return;
    if (!watch_gps_nmea_has_valid_checksum(line)) {
        ESP_LOGW(TAG, "Lỗi Checksum dòng NMEA nhận được!");
        return;
    }

    char first_nmea[128] = {0};
    if (!s_logged_first_nmea) {
        strncpy(first_nmea, line, sizeof(first_nmea) - 1);
    }

    int gsv_sats_in_view = 0;
    if (strstr(line, "GSV")) {
        char *p = strchr(line, ','); // Bỏ qua nhãn $--GSV
        if (p) p = strchr(p + 1, ','); // Bỏ qua số bản tin
        if (p) p = strchr(p + 1, ','); // Bỏ qua số hiệu tin
        if (p && *(p + 1) != '\0') {
            gsv_sats_in_view = atoi(p + 1); // Đọc số lượng vệ tinh thấy được
        }
    }

    watch_gps_nmea_data_t parsed;
    bool has_parsed = watch_gps_nmea_parse_line(line, &parsed);

    if (gps_lock("phân tích bản tin")) {
        s_metrics.nmea_seen = true;
        s_metrics.nmea_lines++;
        if (gsv_sats_in_view > 0 && (gsv_sats_in_view > (int)s_metrics.satellites || !s_metrics.fix_valid)) {
            s_metrics.satellites = (uint32_t)gsv_sats_in_view;
        }
        if (has_parsed) {
            if (parsed.has_gga) s_metrics.satellites = parsed.satellites;
            if (parsed.has_gga) {
                s_metrics.fix_quality = parsed.fix_quality;
                s_metrics.hdop = parsed.hdop;
                s_metrics.fix_valid = parsed.fix_valid;
            } else if (parsed.pdop > 0.0f || parsed.hdop > 0.0f || parsed.vdop > 0.0f) {
                s_metrics.pdop = parsed.pdop;
                s_metrics.hdop = parsed.hdop;
                s_metrics.vdop = parsed.vdop;
                if (parsed.fix_valid) s_metrics.fix_valid = true;
            }
            if (parsed.has_rmc) {
                s_metrics.fix_valid = parsed.fix_valid;
                if (parsed.fix_valid) {
                    double curr_lat = parsed.latitude_deg;
                    double curr_lon = parsed.longitude_deg;

                    s_metrics.latitude_deg = curr_lat;
                    s_metrics.longitude_deg = curr_lon;
                    s_metrics.speed_kmh = parsed.speed_kmh;

                    /* --- Thuật toán tính quãng đường và lưu vết vết hành trình (Haversine) --- */
                    if (fabs(curr_lat) > 0.0001 && fabs(curr_lon) > 0.0001) {
                        uint32_t ms = watch_uptime_ms();
                        if (!s_has_prev_coords) {
                            s_prev_lat = curr_lat;
                            s_prev_lon = curr_lon;
                            s_has_prev_coords = true;
                            s_last_gps_update_ms = ms;
                            gps_append_track_point_locked(curr_lat, curr_lon, s_metrics.total_distance_km);
                        } else {
                            #ifndef M_PI
                            #define M_PI 3.14159265358979323846
                            #endif
                            
                            /* Quy đổi tọa độ từ Độ sang Radian */
                            double phi1 = s_prev_lat * M_PI / 180.0;
                            double phi2 = curr_lat * M_PI / 180.0;
                            double delta_phi = (curr_lat - s_prev_lat) * M_PI / 180.0;
                            double delta_lambda = (curr_lon - s_prev_lon) * M_PI / 180.0;

                            /* Áp dụng công thức lượng giác Haversine */
                            double a = sin(delta_phi / 2.0) * sin(delta_phi / 2.0) +
                                       cos(phi1) * cos(phi2) *
                                       sin(delta_lambda / 2.0) * sin(delta_lambda / 2.0);
                            if (a > 1.0) a = 1.0;
                            double c = 2.0 * asin(sqrt(a));
                            double d_i = 6371000.0 * c; // Khoảng cách thực tế tính bằng mét

                            /* Vận tốc hiệu dụng (mét trên giây) phục vụ lọc sai lệch */
                            double dt = (double)(ms - s_last_gps_update_ms) / 1000.0;
                            if (dt <= 0.0) dt = 1.0;
                            double v_eff = d_i / dt;

                            /* Thực hiện lọc:
                             * - Khoảng cách dịch chuyển tối thiểu phải lớn hơn ngưỡng lọc tĩnh (0.5m).
                             * - Vận tốc tính toán phải nằm trong giới hạn thực tế di chuyển (dưới 90km/h). */
                            if (d_i >= GPS_MIN_SEGMENT_DISTANCE_M && v_eff <= GPS_MAX_SEGMENT_SPEED_MPS) {
                                s_metrics.total_distance_km += (float)(d_i / 1000.0); // Tích lũy quãng đường di chuyển (km)
                                gps_append_track_point_locked(curr_lat, curr_lon, s_metrics.total_distance_km);
                                s_prev_lat = curr_lat;
                                s_prev_lon = curr_lon;
                                s_last_gps_update_ms = ms;
                            } else if (v_eff > GPS_MAX_SEGMENT_SPEED_MPS) {
                                /* Cập nhật mốc tọa độ cơ sở mới nếu xảy ra sai số nhảy vọt lớn
                                 * nhằm tránh việc so sánh dồn tích lũy sai với vị trí cũ */
                                s_prev_lat = curr_lat;
                                s_prev_lon = curr_lon;
                                s_last_gps_update_ms = ms;
                            }
                        }
                    }
                }
            }
        }
        gps_unlock();
    }

    if (!s_logged_first_nmea) {
        ESP_LOGI(TAG, "Đã bắt được bản tin NMEA đầu tiên: %s", first_nmea);
        s_logged_first_nmea = true;
        s_last_good_baud = gps_current_baud();
        s_has_last_good_baud = true;
    }
}

/**
 * @brief Đa nhiệm (FreeRTOS Task) đọc luồng bytes từ cổng UART và ghép thành dòng bản tin
 */
static void gps_uart_reader_task(void *arg) {
    (void)arg;
    uint8_t buf[128];
    char line[128];
    int line_len = 0;
    bool discard_line = false;
    uint32_t last_log = 0;

    while (gps_is_running()) {
        /* Đọc dữ liệu từ cổng UART1 phần cứng với timeout 200ms */
        const int n = uart_read_bytes(WATCH_GPS_UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(200));
        uint32_t ms = watch_uptime_ms();

        /* In định kỳ thông tin chẩn đoán chất lượng tín hiệu định vị */
        if (ms - last_log >= GPS_HEARTBEAT_INTERVAL_MS) {
            if (gps_lock("ghi log trạng thái")) {
                const uint32_t age_ms = s_metrics.last_rx_ms ? (uint32_t)(ms - s_metrics.last_rx_ms) : UINT32_MAX;
                ESP_LOGI(TAG,
                         "Trạng thái GPS: %s | Khóa: %s | Vệ tinh: %u | HDOP: %.1f | Quãng đường: %.3f km | Nhịp truyền: %lu ms | Baudrate: %d",
                         gps_quality_label(&s_metrics, age_ms),
                         s_metrics.fix_valid ? "ĐÃ KHÓA" : "ĐANG DÒ VỆ TINH",
                         s_metrics.satellites,
                         (double)s_metrics.hdop,
                         (double)s_metrics.total_distance_km,
                         (unsigned long)age_ms,
                         gps_current_baud());
                gps_unlock();
            }
            last_log = ms;
        }

        if (n <= 0) {
            /* Máy trạng thái Auto-baud hoạt động khi không nhận được byte nào trong 5 giây */
            if (s_last_rx_seen_ms != 0 && (ms - s_last_rx_seen_ms) >= 5000 && !s_uart_idle_logged) {
                ESP_LOGW(TAG, "Đường truyền UART im lặng quá 5 giây...");
                s_uart_idle_logged = true;
            }
            /* Nếu chưa nhận bản tin NMEA hợp lệ nào trong 10 giây: Đổi Baudrate khác */
            if (!s_logged_first_nmea && s_uart_activity_since_baud_ms != 0 && (ms - s_uart_activity_since_baud_ms) >= 10000) {
                gps_switch_to_next_baud();
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        s_last_rx_seen_ms = ms;
        s_uart_idle_logged = false;
        if (s_uart_activity_since_baud_ms == 0) {
            s_uart_activity_since_baud_ms = ms;
        }

        if (gps_lock("cập nhật số lượng bytes thô")) {
            s_metrics.raw_bytes += n;
            s_metrics.uart_seen = true;
            s_metrics.last_rx_ms = ms;
            gps_unlock();
        }

        /* Duyệt qua từng byte thô nhận được và ghép thành chuỗi dòng bản tin hoàn chỉnh */
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue; // Bỏ qua ký tự về đầu dòng
            if (c == '\n') {
                if (!discard_line) {
                    line[line_len] = '\0';
                    if (line_len > 5) {
                        gps_parse_nmea_line(line); // Đọc thành công dòng bản tin
                    }
                }
                line_len = 0;
                discard_line = false;
            } else if (discard_line) {
                continue;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            } else {
                ESP_LOGW(TAG, "Bản tin NMEA quá dài vượt giới hạn bộ đệm; đã hủy bỏ dòng!");
                line_len = 0;
                discard_line = true;
            }
        }
    }
    
    gps_set_task(NULL);
    if (s_gps_stopped_sem) {
        xSemaphoreGive(s_gps_stopped_sem); // Báo hiệu tiến trình kết thúc an toàn
    }
    vTaskDelete(NULL);
}

/**
 * @brief Gỡ bỏ Driver UART1 phần cứng
 */
static void gps_delete_uart_driver(void) {
    if (!s_gps_uart_installed) return;
    esp_err_t err = uart_driver_delete(WATCH_GPS_UART_PORT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Lỗi gỡ bỏ Driver UART GPS: %s", esp_err_to_name(err));
        return;
    }
    s_gps_uart_installed = false;
}

/**
 * @brief Nháy Reset cứng phần cứng để khởi động lại chip GPS (nếu chân Reset được nối)
 */
static void gps_reset_release(void) {
#if WATCH_PIN_GPS_RST >= 0
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WATCH_PIN_GPS_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(WATCH_PIN_GPS_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(WATCH_PIN_GPS_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
#else
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
}

void watch_sensors_start_gps_imu(void) {
    if (gps_is_running()) return;
    if (gps_get_task()) {
        /* Chờ luồng cũ giải phóng trong tối đa 1.5 giây để tránh tranh chấp Driver UART */
        if (!s_gps_stopped_sem || xSemaphoreTake(s_gps_stopped_sem, pdMS_TO_TICKS(1500)) != pdTRUE) {
            ESP_LOGW(TAG, "Luồng GPS cũ đang kết thúc; lệnh bật bị bỏ qua để đảm bảo an toàn!");
            return;
        }
    }
    
    if (!s_gps_mutex) s_gps_mutex = xSemaphoreCreateMutex();
    if (!s_gps_mutex) {
        ESP_LOGE(TAG, "Không thể khởi tạo Semaphore Mutex GPS!");
        return;
    }
    if (!s_gps_stopped_sem) s_gps_stopped_sem = xSemaphoreCreateBinary();
    if (!s_gps_stopped_sem) {
        ESP_LOGE(TAG, "Không thể tạo Semaphore đồng bộ tắt GPS!");
        return;
    }
    while (xSemaphoreTake(s_gps_stopped_sem, 0) == pdTRUE) {}

    gps_reset_autobaud_state();
    
    if (gps_lock("khởi động lại thông số")) {
        s_metrics.uart_seen = false;
        s_metrics.nmea_seen = false;
        s_metrics.fix_valid = false;
        s_metrics.last_rx_ms = 0;
        s_metrics.raw_bytes = 0;
        s_metrics.nmea_lines = 0;
        s_metrics.current_baud = gps_current_baud();
        gps_reset_distance_baseline_locked();
        gps_unlock();
    }

    gps_reset_release();

    ESP_LOGI(TAG, "Khởi động giao tiếp UART GPS: RX=GPIO%d, TX=GPIO%d, PPS=GPIO%d (Reset Pin=%d)",
             WATCH_PIN_GPS_UART_RX, WATCH_PIN_GPS_UART_TX, WATCH_PIN_GPS_PPS, WATCH_PIN_GPS_RST);

#if defined(WATCH_PIN_GPS_PPS) && WATCH_PIN_GPS_PPS >= 0
    gpio_config_t pps_cfg = {
        .pin_bit_mask = 1ULL << WATCH_PIN_GPS_PPS,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pps_cfg);
#endif
  
    /* Cấu hình UART dạng tiêu chuẩn 8N1 */
    uart_config_t cfg = {
        .baud_rate = gps_current_baud(),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(WATCH_GPS_UART_PORT, GPS_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Cổng UART GPS đã cài đặt sẵn driver, tái sử dụng cổng");
        s_gps_uart_installed = true;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cài đặt UART driver thất bại: %s", esp_err_to_name(err));
        return;
    } else {
        s_gps_uart_installed = true;
    }

    err = uart_param_config(WATCH_GPS_UART_PORT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cấu hình tham số UART thất bại: %s", esp_err_to_name(err));
        gps_delete_uart_driver();
        return;
    }

    err = uart_set_pin(WATCH_GPS_UART_PORT, WATCH_PIN_GPS_UART_TX, WATCH_PIN_GPS_UART_RX, -1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ánh xạ chân UART GPIO thất bại: %s", esp_err_to_name(err));
        gps_delete_uart_driver();
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    gps_configure_module(); // Thiết lập lệnh PMTK cấu hình module GPS
  
    gps_set_running(true);
    TaskHandle_t task_handle = NULL;
    
    /* Khởi tạo luồng quét UART, độ ưu tiên rất cao (Priority 3) trên Core 0 */
    BaseType_t task_ok = xTaskCreatePinnedToCore(gps_uart_reader_task, "gps_uart_task", 8192, NULL, 3, &task_handle, 0);
    if (task_ok != pdPASS) {
        gps_set_running(false);
        gps_set_task(NULL);
        gps_delete_uart_driver();
        ESP_LOGE(TAG, "Không thể tạo Task đọc luồng UART GPS!");
        return;
    }
    gps_set_task(task_handle);
    ESP_LOGI(TAG, "Khởi tạo thành công Task đọc dữ liệu vệ tinh GPS tốc độ %d bps", gps_current_baud());
}

void watch_sensors_stop_gps_imu(void) {
    gps_set_running(false);

    if (gps_get_task() &&
        (!s_gps_stopped_sem || xSemaphoreTake(s_gps_stopped_sem, pdMS_TO_TICKS(1500)) != pdTRUE)) {
        ESP_LOGW(TAG, "Hết thời gian chờ dừng Task GPS; giữ nguyên driver tránh treo!");
        return;
    }

    gps_delete_uart_driver();
}

bool watch_sensors_get_gps_metrics(watch_gps_metrics_t *out) {
    if (!out || !s_gps_mutex) return false;
    if (!gps_lock("đọc thông số")) return false;
    *out = s_metrics;
    gps_unlock();
    return true;
}

void watch_sensors_reset_gps_track(void) {
    if (!s_gps_mutex) return;
    if (!gps_lock("reset track")) return;
    s_metrics.total_distance_km = 0.0f;
    s_track_count = 0;
    s_last_track_point_km = 0.0f;
    gps_reset_distance_baseline_locked();
    gps_unlock();
}

bool watch_sensors_is_gps_active(void) {
    return gps_is_running();
}

size_t watch_sensors_get_gps_track(watch_gps_track_point_t *out, size_t max_points) {
    if (!out || max_points == 0 || !s_gps_mutex) return 0;
    if (!gps_lock("đọc track log")) return 0;
    size_t n = s_track_count < max_points ? s_track_count : max_points;
    memcpy(out, s_track, n * sizeof(s_track[0]));
    gps_unlock();
    return n;
}

int watch_sensors_format_gps_track(char *out, size_t out_size) {
    if (!out || out_size == 0 || !s_gps_mutex) return 0;
    out[0] = '\0';
    if (!gps_lock("định dạng chuỗi track log")) return 0;

    size_t used = 0;
    for (size_t i = 0; i < s_track_count; i++) {
        int written = snprintf(out + used, out_size - used, "%s%.6f,%.6f",
                               i == 0 ? "" : ";",
                               s_track[i].latitude_deg,
                               s_track[i].longitude_deg);
        if (written < 0) break;
        if ((size_t)written >= out_size - used) {
            used = out_size - 1;
            out[used] = '\0';
            ESP_LOGW(TAG, "Đã đầy bộ đệm xuất chuỗi track log: size=%u, điểm=%u",
                     (unsigned int)out_size, (unsigned int)s_track_count);
            break;
        }
        used += (size_t)written;
    }
    gps_unlock();
    return (int)used;
}
