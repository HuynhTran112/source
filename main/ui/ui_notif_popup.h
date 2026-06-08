/*
 * ui_notif_popup.h - Popup hiển thị thông báo mới
 */
#ifndef UI_NOTIF_POPUP_H
#define UI_NOTIF_POPUP_H

#include "ble_notify.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo popup notification (gọi 1 lần sau ui_init).
 */
void ui_notif_popup_init(void);

/**
 * @brief Hiển thị popup thông báo mới.
 *        Tự động ẩn sau 5 giây.
 *        Thread-safe: có thể gọi từ BLE task, sẽ dùng lv_async để chuyển sang
 * LVGL task.
 */
void ui_notif_popup_show(const watch_ble_notification_t *notif);

#ifdef __cplusplus
}
#endif

#endif /* UI_NOTIF_POPUP_H */
