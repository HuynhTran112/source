# 🚗 Kiến Trúc 2: Automotive Smart CAN Gateway & Cluster (Zephyr RTOS)

Ứng dụng Cổng truyền thông ô tô và Hộp chẩn đoán thông minh (**Automotive Telematics Gateway & Cluster**) chạy trên nền tảng hệ điều hành thời gian thực **Zephyr RTOS** (STM32F746G-Discovery).

---

## 🛠️ Tính Năng & Kiến Trúc Kỹ Thuật

1. **Zephyr CAN Subsystem:**
   * Cơ chế tiếp nhận bất đồng bộ qua `can_add_rx_filter_msgq()` gắn trực tiếp bộ lọc vào hàng đợi nhân `k_msgq` (Zero-Lock / Zero CPU overhead).
   * Điều khiển chân Standby (STB) của Transceiver TJA1050 qua Devicetree GPIO.
   * Callback giám sát lỗi mạng và tự động phục hồi an toàn ISO 11898-1 Bus-Off FSM (`can_recover`).
2. **Vector DBC Signal Engine & AUTOSAR E2E:**
   * Bóc tách dữ liệu xe hơi (Tốc độ xe, Vòng tua máy RPM, Nhiệt độ nước làm mát) bằng giải thuật **Toán định điểm Fixed-point** không dùng số thực `float`.
   * Xác thực 3 lớp an toàn **AUTOSAR E2E Profile 1**: Data ID bí mật (`0x1A2B`), Rolling Counter 4-bit và mã CRC-8 đa thức SAE J1850 `0x2F`.
3. **An Toàn Đa Luồng & Bộ Lập Lịch Zephyr:**
   * Khóa bảo vệ tài nguyên dùng chung bằng `k_mutex` tự động kích hoạt thuật toán **Priority Inheritance** chống hiểm họa Priority Inversion.
   * Khiên phần cứng **MPU Stack Guard** (`CONFIG_MPU_STACK_GUARD=y`) bẫy lỗi tràn ngăn xếp ngay tại chu kỳ vi phạm đầu tiên.
4. **Cổng Chẩn Đoán Dòng Lệnh Kỹ Thuật (Zephyr Interactive Shell CLI):**
   * Kết nối máy tính qua cáp Micro-USB (ST-LINK VCP UART) để chẩn đoán hệ thống lúc runtime:
     * `vehicle status` : In bảng thông số xe trực quan (Tốc độ, RPM, Nhiệt độ, Trạng thái E2E).
     * `dtc read`        : Đọc danh sách mã lỗi chẩn đoán (DTC_U0100, DTC_P0115, DTC_P0219).
     * `dtc clear`       : Xóa toàn bộ mã lỗi, khôi phục trạng thái an toàn.
     * `can sim <speed>` : Giả lập phát gói tin CAN (dùng để test và demo khi không có xe thật).

---

## 📁 Cấu Trúc Mã Nguồn

```text
zephyr_project/
├── CMakeLists.txt        # File điều phối biên dịch CMake chuẩn Zephyr
├── prj.conf              # Cấu hình Kconfig tĩnh lúc compile-time
├── app.overlay           # Cấu hình phần cứng Devicetree cho STM32F746G-Discovery
├── src/
│   ├── can_gateway.h     # Giao diện CAN Subsystem và hàng đợi k_msgq
│   ├── can_gateway.c     # Khởi tạo CAN1, STB pin và Bus-Off recovery callback
│   ├── dbc_decoder.h     # Định nghĩa cấu trúc thông số xe hơi
│   ├── dbc_decoder.c     # Giải thuật Vector DBC và kiểm tra AUTOSAR E2E CRC-8
│   ├── safety_monitor.h  # Giao diện giám sát an toàn và quản lý mã lỗi DTC
│   ├── safety_monitor.c  # Phát hiện Timeout mất tín hiệu và lưu trữ DTC
│   ├── diag_shell.c      # Giao diện dòng lệnh Zephyr Shell CLI chẩn đoán
│   └── main.c            # Luồng CAN Worker, Luồng Safety và điều phối hệ thống
└── README.md             # Tài liệu hướng dẫn sử dụng và biên dịch
```

---

## 🚀 Hướng Dẫn Biên Dịch & Nạp Chip Bằng West CLI

```bash
# 1. Di chuyển vào thư mục dự án Zephyr:
cd zephyr_project

# 2. Biên dịch ứng dụng cho bo mạch STM32F746G-Discovery:
west build -b stm32f746g_disco

# 3. Nạp file nhị phân zephyr.bin vào vi điều khiển STM32F7 qua ST-LINK:
west flash

# 4. Mở cửa sổ cấu hình Kconfig trực quan trên Terminal:
west build -t menuconfig
```
