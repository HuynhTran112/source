/**
 * @file ui_shutdown_dialog.h
 * @brief Khai báo các hàm điều khiển hộp thoại xác nhận tắt nguồn (Shutdown Dialog Overlay)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Lớp thiết kế: Application / UI Layer
 * 
 * Mô tả: Cung cấp API hiển thị một hộp thoại xác nhận (Modal Confirmation Dialog) dạng lớp phủ
 * đè lên trên tất cả các màn hình hiện tại. Yêu cầu người dùng xác nhận thao tác tắt nguồn 
 * (đưa chip vào chế độ Deep Sleep) hoặc hủy bỏ quay lại giao diện chính.
 */

#ifndef UI_SHUTDOWN_DIALOG_H
#define UI_SHUTDOWN_DIALOG_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hiển thị hộp thoại xác nhận tắt nguồn trên lớp trên cùng (lv_layer_top)
 * 
 * Hộp thoại đè lên mọi nội dung hiện hành để thu hút sự chú ý của người dùng.
 */
void ui_shutdown_dialog_show(void);

/**
 * @brief Kiểm tra xem hộp thoại tắt nguồn hiện hành có đang hiển thị hay không
 * @return true nếu đang hiển thị, false nếu ngược lại
 */
bool ui_shutdown_dialog_is_visible(void);

/**
 * @brief Đóng và giải phóng tài nguyên của hộp thoại xác nhận tắt nguồn
 */
void ui_shutdown_dialog_close(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SHUTDOWN_DIALOG_H */

