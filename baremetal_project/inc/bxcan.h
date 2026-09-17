/**
 * ==============================================================================
 * File: baremetal_project/inc/bxcan.h
 * Mục đích: Driver Bare-Metal bxCAN1 (500 kbps, 28 Filter Banks, Loopback/Normal)
 * ==============================================================================
 */

#ifndef BXCAN_H
#define BXCAN_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t id;       /* 11-bit Standard ID */
    uint8_t  dlc;      /* Độ dài dữ liệu (0 - 8 bytes) */
    uint8_t  data[8];  /* Mảng dữ liệu byte */
} CAN_Message_t;

/**
 * @brief Khởi tạo CAN1 tốc độ chuẩn 500 kbps (f_APB1 = 54 MHz, Sample Point 87.5%)
 * @param loopback true: Bật chế độ Loopback tự kiểm tra nội bộ; false: Normal Mode
 */
bool CAN1_Init(bool loopback);

/**
 * @brief Gửi một bản tin CAN qua Mailbox trống
 * @param msg Con trỏ gói tin cần gửi
 * @return true nếu đẩy vào Mailbox thành công
 */
bool CAN1_Transmit(const CAN_Message_t *msg);

/**
 * @brief Kiểm tra và nhận bản tin từ FIFO 0
 * @param msg Con trỏ lưu gói tin nhận được
 * @return true nếu có gói tin mới, false nếu FIFO rỗng
 */
bool CAN1_Receive(CAN_Message_t *msg);

#endif /* BXCAN_H */
