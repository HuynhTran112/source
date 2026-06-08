/**
 * @file board_config.h
 * @brief Cấu hình tài nguyên phần cứng đồng hồ thông minh (Smartwatch)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Tài liệu mô tả chi tiết sơ đồ chân GPIO kết nối giữa ESP32-S3 và các ngoại vi:
 * Màn hình TFT LCD ST7789, Cảm ứng CST816S, IMU BMI270, Cảm biến nhịp tim MAX30102,
 * IC quản lý pin MAX17048, Động cơ rung PWM và Định vị GPS GT-U8.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ===================================================================
 *  1. CẤU HÌNH MÀN HÌNH TFT LCD (Giao tiếp SPI)
 *  Sử dụng driver điều khiển ST7789, độ phân giải 240x284 pixels
 * =================================================================== */
#define WATCH_PIN_LCD_SCLK      40  // Chân xung Clock của bus SPI (TFT_SCL)
#define WATCH_PIN_LCD_MOSI      39  // Chân truyền dữ liệu Master Out Slave In (TFT_SDA)
#define WATCH_PIN_LCD_RST       38  // Chân Reset cứng màn hình (TFT_RST)
#define WATCH_PIN_LCD_DC        42  // Chân phân biệt Dữ liệu/Lệnh (TFT_DC - Data/Command)
#define WATCH_PIN_LCD_CS        41  // Chân chọn chip ngoại vi SPI (TFT_CS - Chip Select)
#define WATCH_PIN_LCD_BACKLIGHT 2   // Chân điều khiển đèn nền LCD (LEDK - PWM qua MOSFET SI2302)

#define WATCH_LCD_H_RES         240 // Độ phân giải chiều ngang màn hình (pixels)
#define WATCH_LCD_V_RES         284 // Độ phân giải chiều dọc màn hình (pixels)
#define WATCH_LCD_SPI_HOST      SPI2_HOST // Khối SPI phần cứng sử dụng (SPI2)
#define WATCH_LCD_PIXEL_CLK_HZ  (40 * 1000 * 1000) // Tần số xung nhịp SPI (40 MHz)

/* ===================================================================
 *  2. CẤU HÌNH ĐÈN NỀN LCD (Sử dụng bộ phát xung PWM - LEDC)
 * =================================================================== */
#define WATCH_LCD_BACKLIGHT_LEDC_CH    LEDC_CHANNEL_0  // Kênh PWM LEDC điều khiển đèn nền
#define WATCH_LCD_BACKLIGHT_LEDC_TIMER LEDC_TIMER_0    // Bộ đếm thời gian (Timer) điều khiển PWM
#define WATCH_LCD_BACKLIGHT_DUTY_DEFAULT 200          // Độ rộng xung mặc định (0-255, tương đương ~78% độ sáng)

/* ===================================================================
 *  3. CẤU HÌNH CẢM ỨNG CẤP PHÙ HỢP (Giao tiếp I2C Port 0)
 *  Sử dụng driver cảm ứng điện dung CST816S
 * =================================================================== */
#define WATCH_PIN_TOUCH_SDA     48  // Chân dữ liệu I2C (TP_SDA)
#define WATCH_PIN_TOUCH_SCL     47  // Chân nhịp clock I2C (TP_SCL)
#define WATCH_PIN_TOUCH_INT     14  // Chân ngắt: CST816S kéo mức thấp (LOW) khi phát hiện chạm (TP_INT)
#define WATCH_PIN_TOUCH_RST     21  // Chân Reset cứng chip cảm ứng (TP_RST)

#define WATCH_TOUCH_I2C_PORT    I2C_NUM_0           // Cổng I2C phần cứng 0
#define WATCH_TOUCH_I2C_CLK_HZ  (400 * 1000)        // Tần số giao tiếp I2C Fast Mode (400 kHz)
#define WATCH_TEMP_DISABLE_I2C_LOGS 1               // Khử log I2C rác khi debug

/* ===================================================================
 *  4. CẤU HÌNH BUS I2C DÀNH CHO CẢM BIẾN (I2C Port 1)
 *  Bus dùng chung cho cảm biến IMU BMI270, nhịp tim MAX30102, pin MAX17048
 * =================================================================== */
#define WATCH_SENSOR_I2C_PORT   I2C_NUM_1           // Cổng I2C phần cứng 1 dành cho cảm biến
#define WATCH_PIN_IMU_I2C_SDA   19                  // Chân truyền dữ liệu I2C cảm biến
#define WATCH_PIN_IMU_I2C_SCL   8                   // Chân xung nhịp clock I2C cảm biến

#define WATCH_PIN_IMU_INT       20                  // Chân nhận tín hiệu ngắt từ IMU BMI270 (đếm bước chân)
#define WATCH_PIN_HR_INT        13                  // Chân nhận tín hiệu ngắt từ cảm biến nhịp tim MAX30102
#define WATCH_PIN_BATTERY_ALRT  11                  // Chân cảnh báo mức pin từ chip MAX17048

#define WATCH_IMU_I2C_ADDR      0x68                // Địa chỉ I2C của cảm biến IMU BMI270
#define WATCH_IMU_I2C_CLK_HZ    400000              // Tần số I2C bus cảm biến (400 kHz)

/* ===================================================================
 *  5. CẤU HÌNH ĐỘNG CƠ RUNG (PWM qua bộ phát xung LEDC)
 * =================================================================== */
#define WATCH_PIN_VIBRATOR           12              // Chân GPIO điều khiển motor rung (vibration motor)
#define WATCH_VIBRATOR_LEDC_CH       LEDC_CHANNEL_1  // Kênh điều khiển PWM cho motor
#define WATCH_VIBRATOR_LEDC_TIMER    LEDC_TIMER_1    // Bộ đếm thời gian cho xung motor
#define WATCH_VIBRATOR_LEDC_FREQ_HZ  200             // Tần số phát xung rung (200 Hz giúp rung êm ái)

/* ===================================================================
 *  6. CẤU HÌNH PHÍM BẤM VẬT LÝ
 * =================================================================== */
#define WATCH_PIN_BUTTON_POWER       1               // Chân GPIO kết nối nút nguồn/BOOT vật lý trên thiết bị

/* ===================================================================
 *  7. CẤU HÌNH MODULE GPS (GT-U8 giao tiếp UART)
 * =================================================================== */
#define WATCH_GPS_MODULE_NAME        "GT-U8"         // Tên định danh module định vị GPS
#define WATCH_GPS_UART_PORT          UART_NUM_1      // Cổng UART sử dụng (UART1)
#define WATCH_GPS_UART_BAUD          9600            // Tốc độ Baudrate mặc định của module (9600 bps)
#define WATCH_GPS_UART_BAUD_FALLBACK_1 38400         // Baudrate dự phòng mức 1
#define WATCH_GPS_UART_BAUD_FALLBACK_2 115200        // Baudrate dự phòng mức 2

#define WATCH_PIN_GPS_RST            -1              // Chân Reset GPS (-1 nghĩa là không sử dụng reset cứng)
#define WATCH_PIN_GPS_PPS            4               // Chân tín hiệu nhịp thời gian PPS (Pulse Per Second)
#define WATCH_PIN_GPS_UART_TX        5               // Chân truyền dữ liệu UART từ ESP32 sang GPS RX
#define WATCH_PIN_GPS_UART_RX        6               // Chân nhận dữ liệu UART vào ESP32 từ GPS TX

#endif // BOARD_CONFIG_H
