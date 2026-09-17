# 🏎️ Hệ Thống Dự Án STM32F746G-DISCO: CAN Gateway (Zephyr RTOS) & TFT SDHC Player

Hệ thống nhúng chuyên sâu trên vi điều khiển **ARM Cortex-M7 (STM32F746NGH6)** được tổ chức thành **2 dự án độc lập tại 2 thư mục riêng biệt** nhằm chứng minh trọn vẹn năng lực kỹ thuật:
1. **Dự án 1 (Thư mục này `D:\Project\STM32F7\`):** Automotive CAN Gateway & Telematics Cluster chạy trên hệ điều hành thời gian thực **Zephyr RTOS**.
2. **Dự án 2 (Thư mục chị em `D:\Project\TFT_video_STM32F7\`):** Trình phát Video & Đồ họa 60 FPS mượt mà không giật xé hình viết **100% thanh ghi trần Bare-Metal (RM0385)** kết hợp thẻ nhớ **MicroSD SDHC (4GB - 32GB)** và hệ thống tệp tin **ChaN FatFs (FAT32)**.

---

## 📂 Tổ Chức 2 Dự Án Độc Lập

```text
D:\Project\
│
├── STM32F7/                               # 📁 DỰ ÁN 1: AUTOMOTIVE CAN GATEWAY (ZEPHYR RTOS)
│   ├── zephyr_project/                    # Mã nguồn ứng dụng Zephyr RTOS độc lập
│   │   ├── CMakeLists.txt                 # File điều phối biên dịch CMake chuẩn Zephyr
│   │   ├── prj.conf                       # Cấu hình Kconfig (CAN, Shell CLI, MPU Stack Guard)
│   │   ├── app.overlay                    # Ràng buộc phần cứng Devicetree cho STM32F746G-Discovery
│   │   ├── src/                           # Multi-threading, Vector DBC, AUTOSAR E2E, Diagnostic Shell
│   │   └── README.md                      # Hướng dẫn build và flash bằng West CLI
│   ├── baremetal_project/                 # Driver thanh ghi tham chiếu (Clock 216MHz, UART DMA, bxCAN)
│   └── docs/ & guides/                    # Cẩm nang kỹ thuật 14 ngày & tài liệu phỏng vấn chuyên sâu
│       ├── project_roadmap.md             # Lộ trình tổng thể dự án
│       ├── day00 -> day14                 # Cẩm nang từ phần cứng Cortex-M7 đến Zephyr RTOS
│       └── sdmmc_fatfs_architecture.md   # Cẩm nang chuyên đề thẻ nhớ SDHC & ChaN FatFs
│
└── TFT_video_STM32F7/                     # 📁 DỰ ÁN 2: 60 FPS VIDEO & TFT PLAYER (BARE-METAL SDHC)
    ├── Inc/ & Src/                        # 100% thanh ghi trần C (FMC SDRAM, LTDC, DMA2D, SDMMC SDHC)
    │   ├── sys_clock.c / .h               # Xung nhịp 216 MHz Over-Drive, Flash 7WS, L1 Cache
    │   ├── sdram.c / .h                   # FMC SDRAM 8 MB (108 MHz, 16-bit bus, chuỗi 5 lệnh JEDEC)
    │   ├── ltdc.c / .h                    # LTDC 480x272 @ 60 FPS, Pixel Clock 9.71 MHz, VSYNC Reload
    │   ├── dma2d.c / .h                   # Chrom-ART tăng tốc tô màu & copy buffer 0% CPU
    │   ├── sdmmc.c / .h                   # [CHUẨN SDHC] Bus 4-bit 24-48MHz, Block Addressing LBA
    │   ├── diskio.c & ff.c                # Cầu nối phần cứng FatFs & Bộ máy tệp tin FAT32
    │   └── media_player.c / .h            # Streaming video trực tiếp từ thẻ SDHC & Demo 60 FPS
    ├── build.ps1                          # Script tự động Compile & Flash bằng GNU ARM Toolchain
    └── README.md                          # Tài liệu hướng dẫn sử dụng và công cụ chuyển đổi video
```

---

## 🚀 So Sánh Bản Chất Kỹ Thuật Giữa 2 Dự Án

| Tiêu Chí | 📁 Dự Án 1 (`STM32F7/zephyr_project`) | 📁 Dự Án 2 (`TFT_video_STM32F7`) |
| :--- | :--- | :--- |
| **Mục tiêu năng lực** | Làm chủ hệ điều hành thời gian thực (RTOS) và giao thức ô tô. | Làm chủ phần cứng silicon, thanh ghi trần và tối ưu băng thông vi xử lý. |
| **Tầng trừu tượng** | Zephyr Driver Model (`DEVICE_DT_DEFINE`, Devicetree, Kconfig). | 100% Thanh ghi trần (No HAL, No LL, RM0385). |
| **Xung nhịp CPU** | Do Zephyr Clock Control Driver cấu hình lúc boot hệ thống. | 216 MHz Over-Drive (Tự tính toán PLL, Flash 7 Wait States, bật L1 Cache). |
| **Xử lý Ngoại vi** | Zephyr CAN Subsystem (`can_add_rx_filter_msgq`, chân STB GPIO). | FMC SDRAM 108MHz, LTDC 480x272 RGB565, DMA2D Chrom-ART, SDMMC 4-bit. |
| **Lưu trữ Thẻ nhớ** | Không sử dụng (tập trung giao tiếp bus mạng ô tô). | MicroSD **SDHC (4GB - 32GB)**, Block Addressing LBA 512B, FatFs FAT32. |
| **Bảo vệ An toàn** | Khiên phần cứng `CONFIG_MPU_STACK_GUARD`, AUTOSAR E2E Profile 1. | Double Buffering VSYNC Reload (`VBR`) chống rách hình, D-Cache Invalidate. |
| **Đa luồng & IPC** | Đa luồng Preemptive (`K_THREAD_DEFINE`), `k_msgq`, `k_mutex`. | Super-loop hướng sự kiện + DMA2D phần cứng độc lập. |
| **Giao diện Người dùng** | Cổng dòng lệnh tương tác **Zephyr Shell CLI** qua cáp Micro-USB VCP. | Màn hình màu LCD 4.3 inch 480x272 @ 60 FPS + Nút bấm User Button (PI11). |
