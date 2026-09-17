/**
 * ==============================================================================
 * File: baremetal_project/inc/uart_dma.h
 * Mục đích: Driver Bare-Metal UART1 RX DMA Ring Buffer & IDLE Line Detection
 * ==============================================================================
 */

#ifndef UART_DMA_H
#define UART_DMA_H

#include <stdint.h>
#include <stdbool.h>

#define UART_RX_BUFFER_SIZE 128

/**
 * @brief Khởi tạo UART1 (115200 baud, 8N1) kèm DMA2 Stream 2 Channel 4 Circular Mode
 * @param baudrate Tốc độ baud (ví dụ: 115200)
 */
void UART1_DMA_Init(uint32_t baudrate);

/**
 * @brief Gửi một chuỗi ký tự qua UART1 (Blocking Polling)
 * @param str Chuỗi ký tự kết thúc bằng '\0'
 */
void UART1_SendString(const char *str);

/**
 * @brief Kiểm tra xem có dữ liệu mới do DMA nhận về qua sự kiện IDLE Line không
 * @param out_buf Con trỏ bộ đệm nhận dữ liệu
 * @param out_len Số byte nhận được
 * @return true nếu có dữ liệu mới, false nếu không
 */
bool UART1_GetReceivedData(uint8_t *out_buf, uint16_t *out_len);

#endif /* UART_DMA_H */
