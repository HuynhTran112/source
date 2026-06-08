/**
 * @file ui_icons.h
 * @brief Định nghĩa hàm tiện ích tạo biểu tượng ứng dụng (App Icons UI Helper)
 * 
 * Dự án Đồ án tốt nghiệp: Thiết kế chế tạo Smartwatch chạy ESP32-S3
 * Lớp thiết kế: Application / UI Layer
 * 
 * Mô tả: Cung cấp API tạo biểu tượng dạng hình tròn thống nhất với ký hiệu (symbol)
 * hoặc chữ cái viết tắt căn giữa. Hỗ trợ thay đổi màu nền động hoặc chuyển đổi
 * sang chế độ đơn sắc (monochrome) tùy cấu hình màu sắc hệ thống.
 */

#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

/**
 * @brief Tạo đối tượng biểu tượng hình tròn với ký tự trung tâm
 * 
 * @param parent Đối tượng LVGL cha chứa biểu tượng này
 * @param symbol Ký tự hoặc mã glyph của icon (ví dụ: LV_SYMBOL_SETTINGS, "W",...)
 * @param bg_color Màu nền gốc của biểu tượng ứng dụng
 * @param monochrome Cờ xác định có bật chế độ đơn sắc cho giao diện hay không
 * @param mono_color Màu hiển thị đơn sắc khi cờ monochrome được thiết lập
 * @return lv_obj_t* Con trỏ trỏ tới đối tượng nền của biểu tượng được tạo (lv_obj_create)
 */
lv_obj_t *ui_icon_create(lv_obj_t *parent, const char *symbol, lv_color_t bg_color, bool monochrome, lv_color_t mono_color);

#endif // UI_ICONS_H

