# Project Roadmap — Dual-Architecture CAN Gateway

**Chú thích ưu tiên:** `[MUST]` = rẻ công, giá trị cao, nên làm. `[NICE]` = tốt nếu có thời gian, không bắt buộc để project đủ mạnh apply fresher.

---

## PHASE 1 — Bare-Metal Drivers

### NGÀY 0 — Nền tảng Cốt lõi Bare-Metal (Dành cho người mới bắt đầu)
- [x] `[MUST]` **Tài liệu Nền tảng**: Đã hoàn thành [`day00_baremetal_foundations.md`](file:///d:/Project/STM32F7/day00_baremetal_foundations.md) giải thích Memory-Mapped I/O, Struct-pointer offset mapping, từ khóa `volatile`, Bitwise RMW pattern, Clock Tree và bài tập tính toán PLL từng bước.

### NGÀY 1 — System Clock (216MHz Over-drive) & Reset Reason
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 1 (4 Bước Chuẩn Kỹ Thuật)**: Đã chuẩn hóa [`day01_learning_guide.md`](file:///d:/Project/STM32F7/day01_learning_guide.md) gom trọn vẹn theo mạch: **Nguyên lý phần cứng (Clock Tree PLL, Over-drive, Flash Wait States, Bản đồ RAM Stack/Heap) $\rightarrow$ Thực chiến (Tra cứu RM0385, Bảng Bus/Prescalers) $\rightarrow$ Gõ code (Khung 7 bước TODO có gắn nhãn file, 5 Bug phần cứng) $\rightarrow$ Phỏng vấn (Bộ 5 câu hỏi vặn, Kịch bản 60s)**.
- [x] `[MUST]` **Over-drive Mode**: chạy 216MHz bắt buộc phải bật Over-drive trong `PWR` (không chỉ cấu hình PLL) — kiểm tra lại `PWR->CR1` (bit `ODEN`) và `PWR->CSR1` (chờ `ODRDY`), rồi mới bật `ODSWEN`.
- [x] `[MUST]` **Reset Reason Logging**: đọc `RCC->CSR` lúc boot để log ra UART lý do reset gần nhất (Power-on, Software reset, IWDG timeout, Brown-out...), sau đó clear flag (`RMVF`).
- [x] `[MUST]` **Interview Deep-Dive**: Bổ sung phân tích thứ tự cấu hình Flash Latency và xử lý mất xung Polling Timeout / Clock Security System (CSS).

### NGÀY 2 — UART RX DMA Ring Buffer
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 2 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day02_learning_guide.md`](file:///d:/Project/STM32F7/day02_learning_guide.md) phân tích DMA Circular, IDLE Line, D-Cache 32B alignment, gắn nhãn file TODOs và xử lý lỗi ORE/FE.
- [ ] `[MUST]` Thêm xử lý lỗi phần cứng: **Overrun Error (ORE)**, **Framing Error (FE)** — nếu không clear ORE, DMA sẽ đứng im mà không báo lỗi rõ ràng, đây là bug kinh điển hay bị hỏi khi phỏng vấn.
- [ ] `[MUST]` **Cache coherency & 32-byte Alignment với D-Cache**: buffer được cả CPU và DMA truy cập — bắt buộc khai báo `__attribute__((aligned(32)))` và gọi `SCB_InvalidateDCache_by_Addr()` trước khi CPU đọc. Nếu không căn lề 32 bytes (độ dài Cache Line Cortex-M7), invalidate cache sẽ nguy cơ làm hỏng (corrupt) các biến RAM nằm liền kề.
- [ ] `[NICE]` Đo lường thực tế claim "0% CPU overload" — dùng **DWT Cycle Counter** hoặc toggle GPIO đo bằng oscilloscope/logic analyzer khi flood UART tốc độ cao. Có số liệu cụ thể trong README mạnh hơn mô tả định tính.

### NGÀY 3 — bxCAN Driver
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 3 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day03_learning_guide.md`](file:///d:/Project/STM32F7/day03_learning_guide.md) phân tích Bit Timing CiA 301 (500kbps, Sample Point 87.5%), 28 Filter Banks (Mask/List Mode), CAN1 Master sharing, gắn nhãn file TODOs, sơ đồ Self-Test Loopback Mode cho board Discovery và Automotive Bus-Off Recovery.
- [ ] `[MUST]` **Cấu hình CAN Filter (Mask/List Mode)**: "Accept All Filter" chỉ dùng để test ban đầu — bổ sung filter theo ID để chứng minh hiểu bộ lọc CAN. Lưu ý: CAN1 là Master quản lý phân chia 28 Filter Banks (thanh ghi `CAN1->FMR` bit `CAN2SB`), bắt buộc phải cấp clock CAN1 và cấu hình `CAN1->FMR` kể cả khi dùng CAN2.
- [ ] `[MUST]` Xác nhận lại: **STM32F746G-Discovery không có CAN transceiver onboard** — cần module ngoài, test bằng bus thật chứ không chỉ loopback nếu muốn claim "tested on real CAN bus".
- [ ] `[NICE]` Thêm xử lý **Bus-Off recovery** và theo dõi **TEC/REC (error counters)** — chi tiết rất "automotive", hay được hỏi.

### NGÀY 4 — FMC SDRAM + LTDC
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 4 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day04_learning_guide.md`](file:///d:/Project/STM32F7/day04_learning_guide.md) phân tích FMC SDRAM 108MHz (chuỗi 5 lệnh JEDEC, Refresh Rate Counter 1667), LTDC Display Timings 480x272, Double Buffering VSYNC Reload (`VBR`) triệt tiêu Tearing, gắn nhãn file TODOs và MPU Non-cacheable policy.

### NGÀY 5 — DMA2D (Chrom-ART) & NVIC Priority Matrix
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 5 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day05_learning_guide.md`](file:///d:/Project/STM32F7/day05_learning_guide.md) phân tích bộ tăng tốc đồ họa DMA2D (R2M color fill, M2M PFC, Line Offset OOR), kiến trúc phân tầng ưu tiên ngắt `NVIC_PriorityGroup_4` (CAN > UART > DMA2D > LTDC > SysTick) và 5 bug phần cứng kinh điển.

### NGÀY 6 — Hardware Reliability: Watchdogs (IWDG/WWDG) & CSS Failover
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 6 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day06_learning_guide.md`](file:///d:/Project/STM32F7/day06_learning_guide.md) phân tích Independent Watchdog (LSI 32kHz, công thức Timeout, đóng băng DBGMCU), Window Watchdog (Cửa sổ APB1), Clock Security System (CSS tự động chuyển xung HSI và NMI Handler) và giám sát điện áp PVD.

---

## PHASE 2 — Zephyr OS & LVGL

### NGÀY 7 — Zephyr Board Bring-Up & Devicetree Overlay
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 7 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day07_learning_guide.md`](file:///d:/Project/STM32F7/day07_learning_guide.md) phân tích triết lý Devicetree compile-time macro, Kconfig, Pinctrl, Multi-threading, bảo vệ ngăn xếp phần cứng `CONFIG_MPU_STACK_GUARD` và Deferred Logging.

### NGÀY 8 — Zephyr CAN Subsystem & Transceiver Standby Control
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 8 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day08_learning_guide.md`](file:///d:/Project/STM32F7/day08_learning_guide.md) phân tích Zephyr CAN driver, điều khiển chân Standby (STB) của transceiver ngoài, cơ chế gắn filter trực tiếp vào hàng đợi `can_add_rx_filter_msgq` và State Change Callback theo dõi Bus-Off.

### NGÀY 9 — Zephyr Display Driver & LVGL Graphics Pipeline
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 9 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day09_learning_guide.md`](file:///d:/Project/STM32F7/day09_learning_guide.md) ràng buộc FMC SDRAM và LTDC trên Devicetree (`chosen zephyr,display = &ltdc`), tích hợp thư viện đồ họa LVGL (kim đồng hồ Arc, thanh RPM), Dirty area invalidation và Mutex bảo vệ an toàn luồng đồ họa.

### NGÀY 10 — Multi-Threaded IPC Architecture & Shell CLI
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 10 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day10_learning_guide.md`](file:///d:/Project/STM32F7/day10_learning_guide.md) phân tầng Producer-Consumer Actor Pattern giữa CAN Worker ↔ `k_msgq` ↔ GUI Model Updater, hóa giải Priority Inversion bằng `k_mutex` Priority Inheritance, giám sát Stack bằng Thread Analyzer và giao diện dòng lệnh Zephyr Shell CLI qua UART.

---

## PHASE 3 — Automotive Protocols, Diagnostics & Portfolio Packaging

### NGÀY 11 — Vector DBC Signal Decoding Engine
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 11 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day11_learning_guide.md`](file:///d:/Project/STM32F7/day11_learning_guide.md) phân tích cấu trúc Vector DBC, giải thuật Zero-Copy Bit Unpacking cho cả hai định dạng Intel Little-Endian (`@1`) và Motorola Big-Endian (`@0`), chuyển đổi giá trị vật lý bằng số nguyên định điểm Fixed-Point Scaling và mở rộng dấu bù hai Sign Extension.

### NGÀY 12 — Signal Supervision, Missing Frame & Bus-Off Recovery FSM
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 12 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day12_learning_guide.md`](file:///d:/Project/STM32F7/day12_learning_guide.md) xây dựng 3 lớp bảo vệ chuẩn AUTOSAR E2E (Rolling Counter, E2E CRC-8 đa thức $0x2F$ kèm Data ID, Timeout Supervision chống dữ liệu đông đá Stale Data) và máy trạng thái phục hồi lỗi ngắt mạng ISO 11898-1 Bus-Off Recovery FSM.

### NGÀY 13 — Dual-Architecture Benchmarking & Host Unit Testing
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 13 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day13_learning_guide.md`](file:///d:/Project/STM32F7/day13_learning_guide.md) đo đạc định lượng trực diện Bare-Metal vs Zephyr RTOS bằng ARM Cortex-M7 DWT Cycle Counter (độ phân giải $4.63\text{ ns}$), bảng so sánh thời gian boot và dung lượng Flash/RAM, bộ kiểm thử đơn vị tự động Host Unit Testing với Unity Framework chạy trên PC (`make test`), và rà soát tiêu chuẩn MISRA-C.

### NGÀY 14 — Portfolio Packaging, Technical CV & Master Interview Cheat Sheet
- [x] `[MUST]` **Cẩm nang Toàn diện Ngày 14 (4 Bước Chuẩn Kỹ Thuật)**: Đã hoàn thành [`day14_learning_guide.md`](file:///d:/Project/STM32F7/day14_learning_guide.md) đóng gói Portfolio GitHub chuyên nghiệp (kèm mục Known Issues & Lessons Learned), chiến lược 2 cột viết CV (Automotive vs Firmware thuần), Bộ 20 Câu Hỏi Phỏng Vấn Sát Hạch Chuyên Sâu mổ xẻ từ thanh ghi Cortex-M7 đến Zephyr RTOS, và kịch bản thuyết trình 2 phút chuẩn phương pháp STAR.

---

## Cách "kể chuyện" project theo 2 hướng job khác nhau

| Yếu tố | Apply job Automotive | Apply job Firmware thuần (không automotive) |
|---|---|---|
| **Tên project trong CV** | "Automotive HMI & CAN Gateway" / "Digital Instrument Cluster" | "Dual-Architecture Embedded Display & Communication System" |
| **Mở đầu mô tả** | Nhấn CAN protocol, real-time signal display, automotive reliability | Nhấn register-level driver, RTOS architecture, display pipeline |
| **CAN được mô tả là** | Automotive network protocol, signal-based communication (DBC-style) | Serial communication protocol driver viết từ thanh ghi (ngang hàng SPI/I2C/UART) |
| **DBC / AUTOSAR / UDS mini-demo** | Nhấn mạnh — chứng minh hiểu domain automotive | Nhắc sơ hoặc bỏ qua — không liên quan JD |
| **Watchdog / PVD / Reset reason** | Nhấn mạnh (an toàn = core value ngành automotive) | Vẫn nói nhưng đóng khung là "system reliability engineering" nói chung |
| **Cache coherency / MPU / NVIC priority** | Nói nhưng ở mức vừa phải | Nhấn mạnh — đây chính là core skill JD firmware thuần hay hỏi |
| **RTOS (Zephyr) + LVGL** | Nói như 1 phần của HMI ô tô | Nhấn mạnh như RTOS/GUI framework kỹ năng tổng quát |
| **Thiếu sót cần bù thêm** | Không nhiều (project đã đúng hướng) | Nên bổ sung thêm 1 driver I2C/SPI thật (VD cảm biến rời) để không chỉ có UART/CAN — JD firmware thuần hay hỏi kinh nghiệm I2C/SPI |
| **Project nào nên đặt lead đầu CV** | STM32 CAN project | Tùy — có thể ESP32 smartwatch (BLE, sensor, mobile) ăn khớp hơn nếu JD nghiêng IoT/wearable |
| **Câu hỏi phỏng vấn mở** | "Kể về hệ thống CAN bạn làm" | "Kể về driver phức tạp nhất bạn từng viết từ thanh ghi" |

**Ý chính**: phần cứng và code không đổi giữa 2 cột — chỉ đổi cái gì được nhấn mạnh trước, cái gì để phía sau. Nên chuẩn bị sẵn 2 bản mô tả ngắn (elevator pitch) khác nhau cho project này, chọn bản phù hợp tuỳ từng JD cụ thể.
