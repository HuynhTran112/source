/**
 * @file ui.c
 * @brief Định nghĩa điểm vào chính (Main Entry Point) và khởi tạo luồng giao diện người dùng (GUI)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Lớp thiết kế: Application / UI Layer
 * 
 * Mô tả: File này quản lý vòng đời khởi tạo giao diện của đồng hồ thông minh sử dụng thư viện
 * đồ họa LVGL (Light and Versatile Graphics Library). Giao diện được bắt đầu bằng cách khởi tạo
 * hệ thống quản lý ngăn xếp màn hình (Screen Stack Manager), thiết lập thanh trạng thái cố định
 * (Status Bar) ở trên cùng của giao diện và kích hoạt màn hình mặt đồng hồ chính (Watchface).
 */

#include "ui.h"
#include "ui_nav.h"
#include "ui_screens.h"

/**
 * @brief Khởi tạo hệ thống giao diện đồ họa người dùng của Smartwatch
 * 
 * Hàm này thực hiện các bước:
 * 1. Khởi tạo cấu trúc quản lý định tuyến màn hình (ui_nav_init) để theo dõi các màn hình đang mở.
 * 2. Tạo thanh Status Bar chung hiển thị trên cùng để theo dõi Pin, kết nối BLE, Wi-Fi và giờ hệ thống.
 * 3. Tạo đối tượng màn hình Watchface và đẩy vào ngăn xếp làm màn hình gốc (Root Screen).
 */
void ui_init(void)
{
    /* Bước 1: Khởi tạo cơ chế điều hướng và quản lý ngăn xếp màn hình (Navigation Stack) */
    ui_nav_init();

    /* Bước 2: Tạo thanh trạng thái (Status Bar) hiển thị toàn cục trên lớp trên cùng (Top Layer) */
    ui_statusbar_create();

    /* Bước 3: Khởi tạo và tải màn hình mặt đồng hồ chính (Watchface Screen) làm màn hình gốc */
    lv_obj_t *wf = ui_watchface_screen_create();
    ui_nav_push(wf);
}

/**
 * @brief Gửi yêu cầu chuyển tiếp sang chế độ ngủ sâu (Deep Sleep) tắt màn hình
 * 
 * Hàm này gọi API từ phân lớp điều hướng để lưu lại trạng thái màn hình cuối cùng
 * trước khi tắt nguồn màn hình và cấu hình các nguồn đánh thức (nút vật lý / ngắt chạm).
 */
void ui_request_deep_sleep(void)
{
    ui_nav_enter_deep_sleep();
}

