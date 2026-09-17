# 🚗 Kiến Trúc 1: STM32F746 Bare-Metal Firmware

Dự án phần mềm nhúng điều khiển thanh ghi trực tiếp (**100% Register-Level / No HAL / No LL**) trên vi điều khiển **ARM Cortex-M7 (STM32F746NGH6)**.

---

## 🛠️ Danh Sách Module Phần Cứng Đã Triển Khai

1. **System Clock (216 MHz Over-Drive) & Reset Reason Logging:**
   * Quản lý thạch anh ngoài HSE 25 MHz, nhân Main PLL lên 432 MHz và chia đôi ra SYSCLK 216 MHz.
   * Kích hoạt Over-Drive mode trong `PWR_CR1`.
   * Cấu hình 6 Flash Wait States (7 CPU cycles) và kích hoạt ART Accelerator.
   * Ghi nhận lý do Reset (POR, BOR, IWDG, WWDG, Soft Reset) qua thanh ghi `RCC_CSR` và xóa cờ `RMVF`.
2. **UART1 RX DMA Circular Ring Buffer:**
   * Cấu hình DMA2 Stream 2 Channel 4 chạy chế độ Circular.
   * Bắt sự kiện rảnh đường truyền IDLE Line để đọc gói tin biến thiên độ dài mà không cần ngắt từng byte (0% CPU overhead).
   * Khắc phục lỗi Overrun Error (ORE) làm tê liệt DMA.
   * Căn lề bộ đệm 32 bytes chống lỗi L1 D-Cache Coherency trên Cortex-M7.
3. **bxCAN Controller (500 kbps CiA 301):**
   * Định thời bit tốc độ 500 kbps (Sample Point 83.3% - 87.5%).
   * Cấu hình 28 Filter Banks (Mask Mode / 32-bit scale).
   * Hỗ trợ **CAN Loopback Mode phần cứng** giúp kiểm thử truyền nhận độc lập trên 1 bo mạch duy nhất mà không cần Transceiver ngoài hay board thứ hai.

---

## 📁 Cấu Trúc Mã Nguồn

```text
baremetal_project/
├── inc/
│   ├── Reg.h          # Ánh xạ địa chỉ thanh ghi Memory-Mapped I/O
│   ├── Sys_Clock.h    # Giao diện xung nhịp 216 MHz và hộp đen Reset CSR
│   ├── uart_dma.h     # Giao diện UART1 RX DMA Circular
│   └── bxcan.h        # Giao diện bộ điều khiển bxCAN1
└── src/
    ├── Sys_Clock.c    # Cài đặt quy trình 7 bước khởi tạo Clock 216 MHz
    ├── uart_dma.c     # Cài đặt DMA2 Stream 2 và xử lý cờ ORE
    ├── bxcan.c        # Cài đặt Bit Timing 500 kbps và Filter Bank 0
    └── main.c         # Vòng lặp chính Super-loop điều phối hệ thống
```
