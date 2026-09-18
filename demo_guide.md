# 🎬 Cẩm Nang Hướng Dẫn Live Demo & Thuyết Trình Dự Án (Demo Guide)

> **Mục tiêu:** Hướng dẫn từng bước cách chuẩn bị, nạp firmware và biểu diễn thực tế (Live Demo) 2 dự án trên kit **STM32F746G-Discovery** cùng dự án **Wearable Smartwatch** trước nhà tuyển dụng hoặc hội đồng kỹ thuật.  
> **Thời lượng demo lý tưởng:** $5 - 10\text{ phút}$ (gây ấn tượng mạnh ngay từ những thao tác đầu tiên).

---

## 📋 MỤC LỤC TỔNG QUAN

- [1. CHUẨN BỊ PHẦN CỨNG & KẾT NỐI](#1-chuẩn-bị-phần-cứng--kết-nối)
- [2. KỊCH BẢN DEMO DỰ ÁN 1: AUTOMOTIVE CAN GATEWAY (ZEPHYR RTOS)](#2-kịch-bản-demo-dự-án-1-automotive-can-gateway-zephyr-rtos)
  - [2.1. Lệnh nạp 1-Click duy nhất](#21-lệnh-nạp-1-click-duy-nhất)
  - [2.2. Kết nối Terminal tương tác](#22-kết-nối-terminal-tương-tác)
  - [2.3. Quy trình 6 bước demo "gây ấn tượng" trên terminal](#23-quy-trình-6-bước-demo-gây-ấn-tượng-trên-terminal)
  - [2.4. Kịch bản nói mẫu (Talking Points) khi demo CAN](#24-kịch-bản-nói-mẫu-talking-points-khi-demo-can)
- [3. KỊCH BẢN DEMO DỰ ÁN 2: TRÌNH PHÁT VIDEO TFT 60 FPS (BARE-METAL SDHC)](#3-kịch-bản-demo-dự-án-2-trình-phát-video-tft-60-fps-bare-metal-sdhc)
  - [3.1. Lệnh nạp 1-Click duy nhất](#31-lệnh-nạp-1-click-duy-nhất)
  - [3.2. Quy trình demo trực tiếp trên màn hình LCD & Nút bấm](#32-quy-trình-demo-trực-tiếp-trên-màn-hình-lcd--nút-bấm)
  - [3.3. Kịch bản nói mẫu (Talking Points) khi demo Video](#33-kịch-bản-nói-mẫu-talking-points-khi-demo-video)
- [4. KỊCH BẢN THUYẾT TRÌNH DỰ ÁN 3: WEARABLE IOT SMARTWATCH (ESP32-S3)](#4-kịch-bản-thuyết-trình-dự-án-3-wearable-iot-smartwatch-esp32-s3)
- [5. BẢNG TỔNG HỢP LỆNH CHUYỂN ĐỔI NHANH (CHEAT SHEET)](#5-bảng-tổng-hợp-lệnh-chuyển-đổi-nhanh-cheat-sheet)

---

# 1. CHUẨN BỊ PHẦN CỨNG & KẾT NỐI

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   KIT PHÁT TRIỂN STM32F746G-DISCOVERY                  │
│                                                                        │
│   [ Khe cắm Thẻ nhớ MicroSD ]              [ Màn hình LCD 4.3" ]       │
│   (Ghim sẵn thẻ FAT32 có file .BIN)        (480x272 RGB565)            │
│                 │                                      ▲               │
│                 ▼                                      │               │
│        STM32F746NGH6 MCU ◄─────────────────────────────┘               │
│        (ARM Cortex-M7 @ 216MHz)                                        │
│                 ▲                                                      │
│                 │                                                      │
│   [ Cổng USB ST-LINK (CN14) ] ◄── Cáp Micro-USB ──► Máy tính (PC/Laptop)│
│   (Nạp code & Debug VCP COM4)                       (Terminal 115200)  │
│                                                                        │
│   [ Nút bấm Xanh (PI11) ] : Nút User Button chuyển đổi video           │
│   [ Đèn LED Xanh (PI1) ]  : Đèn LED cảnh báo an toàn ô tô              │
└────────────────────────────────────────────────────────────────────────┘
```

1. **Cắm cáp Micro-USB:** Cắm vào cổng **USB ST-LINK (CN14)** ở góc trên bo mạch kết nối vào cổng USB máy tính.
2. **Xác định cổng COM:** Mở *Device Manager* $\to$ *Ports (COM & LPT)*, kiểm tra tên **`STMicroelectronics STLink Virtual COM Port`** (thông thường là `COM4`).
3. **Thẻ nhớ MicroSD:** Cắm thẻ nhớ MicroSD chứa sẵn 3 file video đã định dạng: `CAR1.BIN` (88MB), `CAR2.BIN` (37MB), `NEON.BIN` (94MB).

---

# 2. KỊCH BẢN DEMO DỰ ÁN 1: AUTOMOTIVE CAN GATEWAY (ZEPHYR RTOS)

### 2.1. Lệnh nạp 1-Click duy nhất

Mở cửa sổ PowerShell tại thư mục `D:\Project\STM32F7` và gõ:

```powershell
.\build_zephyr.ps1
```

> **Hệ thống tự động:** Gọi `west build` biên dịch sạch sẽ ứng dụng Zephyr và dùng `STM32CubeProgrammer CLI` nạp thẳng vào Flash STM32F7 trong **1.5 giây**.

---

### 2.2. Kết nối Terminal tương tác

Mở bất kỳ phần mềm Serial Terminal (như **PuTTY**, **Tera Term**, **Hercules** hoặc VSCode Serial Monitor):
* **Port:** `COM4` (hoặc cổng COM của ST-LINK trên máy anh)
* **Baud rate:** `115200`
* **Data bits:** `8`, **Stop bits:** `1`, **Parity:** `None`

*(Hoặc nếu không muốn cài thêm phần mềm, anh có thể chạy lệnh PowerShell có sẵn bên dưới).*

---

### 2.3. Quy trình 6 bước demo "gây ấn tượng" trên terminal

#### 🔹 Bước 1: Khởi động & kiểm tra kiến trúc đa luồng Zephyr
Nhấn phím `Enter` trên Terminal, dấu nhắc dòng lệnh sẽ xuất hiện:
```text
ecu:~$ 
```
Gõ lệnh xem danh sách các luồng đang chạy song song trong RTOS:
```text
kernel threads
```
* **Ý nghĩa trình diễn:** Cho nhà tuyển dụng thấy hệ thống đang chạy preemptive đa nhiệm gồm: `can_worker` (mức ưu tiên 5), `safety_tid` (mức ưu tiên 6), `sim_tid` (mức ưu tiên 7) và `shell` (mức ưu tiên 14) cùng mức tiêu thụ ngăn xếp Stack an toàn.

---

#### 🔹 Bước 2: Xem số liệu xe hơi đang vận hành trực tiếp
Gõ lệnh:
```text
vehicle status
```
* **Màn hình hiển thị thực tế:**
```text
========================================
   THONG SO VAN HANH XE HOI (TELEMETRY) 
========================================
• Toc do hien tai : 84 km/h
• Vong tua may    : 3300 RPM
• Nhiet do nuoc   : 88 degC
• Rolling Counter : 6
• E2E Integrity   : VALID (OK)
========================================
```
* **Thao tác thêm:** Gõ lại `vehicle status` lần nữa sau 2 giây, chỉ số tốc độ và vòng tua máy sẽ tự động thay đổi (tăng giảm từ $45\text{ km/h}$ đến $116\text{ km/h}$, tua máy $2300 - 4100\text{ RPM}$).
* **Điểm nhấn kỹ thuật:** Dữ liệu được giải mã qua **Vector DBC** bằng toán định điểm (Fixed-point không dùng `float`), vượt qua kiểm tra **AUTOSAR E2E Profile 1 (CRC-8 SAE J1850 đa thức 0x2F)**.

---

#### 🔹 Bước 3: Đọc mã lỗi chẩn đoán tiêu chuẩn ô tô (Diagnostic Trouble Codes - DTC)
Gõ lệnh:
```text
dtc read
```
* **Màn hình hiển thị:**
```text
Danh sach ma loi chan doan (DTC) hien huu (0 loi):
➔ Khong co ma loi nao. He thong AN TOAN tuyet doi.
```
* **Quan sát phần cứng:** Đèn LED User (chân PI1 trên bo mạch) **tắt hoàn toàn** vì hệ thống đang ở trạng thái an toàn.

---

#### 🔹 Bước 4: Demo bơm lỗi quá nhiệt động cơ (Fault Injection: Overheat)
Gõ lệnh:
```text
can inject overheat
```
Hệ thống báo: `Da bom loi QUA NHIET (115 degC)!`  
Ngay lập tức kiểm tra lại:
```text
dtc read
```
* **Màn hình hiển thị:**
```text
Danh sach ma loi chan doan (DTC) hien huu (1 loi):
 [1] DTC: 0x0115
```
* **Quan sát phần cứng:** Đèn LED cảnh báo PI1 trên bo mạch **bắt đầu nhấp nháy liên tục ở tần số 2.5 Hz (mỗi 200ms)** để báo động tài xế!
* **Ý nghĩa trình diễn:** Chứng minh máy trạng thái giám sát an toàn `safety_monitor` phát hiện nước làm mát vượt quá ngưỡng an toàn $105^\circ\text{C}$ và sinh mã chuẩn OBD-II: **`P0115` (Engine Coolant Overheating)**.

---

#### 🔹 Bước 5: Demo bơm lỗi quá vòng tua máy (Fault Injection: Overspeed)
Gõ lệnh:
```text
can inject overspeed
dtc read
```
* **Màn hình hiển thị:**
```text
Danh sach ma loi chan doan (DTC) hien huu (2 loi):
 [1] DTC: 0x0115
 [2] DTC: 0x0219
```
* **Ý nghĩa:** Bắt mã chuẩn OBD-II: **`P0219` (Engine Overspeed Condition)** khi tua máy vượt quá $6500\text{ RPM}$.

---

#### 🔹 Bước 6: Demo sự cố mất kết nối mạng CAN (Loss of Communication) & Tự phục hồi
1. **Tắt nguồn phát CAN để giả lập đứt cáp giữa đường:**
   ```text
   can auto off
   ```
   Chờ 1 giây (quá ngưỡng Timeout $1000\text{ ms}$), gõ:
   ```text
   dtc read
   ```
   * **Kết quả:** Hệ thống lập tức kích hoạt thêm mã lỗi **`0x0100` (`DTC_U0100`: Lost Communication with ECM)**.
2. **Khôi phục lại mạng và xóa mã lỗi:**
   ```text
   can auto on
   dtc clear
   dtc read
   ```
   * **Kết quả:** `Danh sach ma loi: 0 loi. He thong AN TOAN tuyet doi.` Đèn LED xanh trên kit **ngừng chớp nháy và tắt hẳn**!

---

### 2.4. Kịch bản nói mẫu (Talking Points) khi demo CAN

> *"Em xin trình diễn ứng dụng Automotive CAN Telematics Gateway chạy trên nền tảng Zephyr RTOS đa nhiệm:  
> Em thiết kế hệ thống theo chuẩn kiến trúc ô tô, tách biệt luồng nhận gói tin CAN bằng hàng đợi k_msgq Zero-Lock với mức ưu tiên cao, và luồng Safety Supervisor giám sát lỗi độc lập.  
> Khi xe vận hành, gói tin CAN ID 0x123 được gửi liên tục ở tần số 5 Hz. Bộ giải mã Vector DBC bóc tách tín hiệu tốc độ xe, vòng tua máy và nhiệt độ bằng toán Fixed-Point để tối ưu CPU. Mỗi bản tin đều được bảo vệ nghiêm ngặt bằng chuẩn AUTOSAR E2E Profile 1 với mã CRC-8 đa thức SAE J1850.  
> Để phục vụ việc kiểm thử và chẩn đoán trong nhà máy, em xây dựng cổng Zephyr Shell CLI tương tác trực tiếp qua UART. Em có thể trực tiếp giám sát Telemetry, đọc mã lỗi DTC, hoặc chủ động bơm các lỗi phần cứng như quá nhiệt động cơ P0115 hay đứt cáp U0100 để kiểm tra phản xạ nhấp nháy đèn cảnh báo và cơ chế tự phục hồi của vi điều khiển."*

---

# 3. KỊCH BẢN DEMO DỰ ÁN 2: TRÌNH PHÁT VIDEO TFT 60 FPS (BARE-METAL SDHC)

### 3.1. Lệnh nạp 1-Click duy nhất

Để chuyển từ dự án CAN sang dự án Video Player, mở PowerShell và gõ:

```powershell
powershell -ExecutionPolicy Bypass -File "D:\Project\TFT_video_STM32F7\build.ps1" -Action flash
```

*(Hoặc nếu đang đứng trong thư mục `D:\Project\TFT_video_STM32F7`: `.\build.ps1 -Action flash`)*

> Firmware biên dịch $100\%$ C thanh ghi trần không dùng HAL/LL, liên kết với file linker `STM32F746NGHX_FLASH.ld` và nạp vào chip trong **2 giây**.

---

### 3.2. Quy trình demo trực tiếp trên màn hình LCD & Nút bấm

#### 🔹 Bước 1: Khởi động Menu đồ họa
* Ngay sau khi nạp, màn hình LCD $4.3\text{ inch}$ ($480 \times 272$) sẽ sáng lên và hiển thị Menu chọn video đồ họa:
  * Thẻ nhớ SDHC được mount tự động qua hệ thống tệp tin FatFs (FAT32).
  * Danh sách 3 video hiển thị trên màn hình: `CAR1.BIN`, `CAR2.BIN`, `NEON.BIN`.
  * Có thanh đếm ngược tự động phát video sau 4 giây nếu người dùng không bấm nút.

#### 🔹 Bước 2: Chọn video & Quan sát tốc độ 60 - 62 FPS
* Nhấn **Nút bấm màu xanh (User Button PI11)** trên bo mạch để di chuyển chọn video mong muốn (hoặc để tự động phát).
* Video bắt đầu phát toàn màn hình với chất lượng màu sắc rực rỡ (RGB565 16-bit):
  * **Quan sát góc trên bên trái màn hình:** Có bộ đếm FPS hiển thị trực tiếp con số **`FPS: 60`** hoặc **`FPS: 61 - 62`** màu xanh lá nổi bật.
  * **Quan sát chuyển động:** Hình ảnh các pha đua xe hoặc ánh đèn neon chuyển động cực kỳ mượt mà, $100\%$ **không hề có hiện tượng xé hình (Tearing)** hay giật khung hình.

#### 🔹 Bước 3: Thoát về Menu chọn video khác
* Khi đang xem video, **nhấn nút User Button (PI11)**:
  * Trình phát video lập tức dừng luồng đọc thẻ nhớ, giải phóng bộ đệm và quay trở lại Menu đồ họa ban đầu.
  * Nhấn tiếp nút để chọn file video dung lượng lớn hơn ($88\text{ MB}$ hoặc $94\text{ MB}$) để chứng minh khả năng streaming bền bỉ liên tục không bị nghẽn bộ đệm.

---

### 3.3. Kịch bản nói mẫu (Talking Points) khi demo Video

> *"Em xin trình bày dự án thứ hai: Trình phát video 60 FPS từ thẻ nhớ MicroSD SDHC được viết hoàn toàn 100% bằng thanh ghi trần Bare-Metal, không sử dụng bất kỳ thư viện HAL hay LL nào:  
> Để đạt được con số 60 đến 62 FPS mượt mà trên độ phân giải 480x272 mà CPU Cortex-M7 vẫn giữ tải dưới 15%, em đã thiết kế đường ống Zero-Copy tối ưu hóa băng thông Bus Matrix:  
> 1. Xung nhịp vi điều khiển được cấu hình lên mức tối đa 216 MHz ở chế độ Over-Drive, Flash 7 Wait States kết hợp kích hoạt ART Accelerator và L1 Cache (I-Cache & D-Cache 16KB).  
> 2. Bộ nhớ ngoài FMC SDRAM 8 MB chạy ở xung nhịp 108 MHz với bus dữ liệu 16-bit, khởi tạo chuẩn JEDEC 5 bước để làm Double Framebuffer.  
> 3. Bộ điều khiển thẻ nhớ SDMMC chạy ở chế độ Bus 4-bit tần số cao 48 MHz kết hợp giao thức LBA Block Addressing của thẻ SDHC để kéo dữ liệu từ hệ thống tệp ChaN FatFs với tốc độ đọc tuần tự đạt 15 đến 18 MB/s.  
> 4. Toàn bộ quá trình chuyển dữ liệu từ bộ nhớ đệm sang màn hình do bộ tăng tốc đồ họa phần cứng DMA2D (Chrom-ART) và LTDC đảm nhiệm. Em áp dụng cơ chế Double Buffering kích hoạt đồng bộ tại ngắt VSYNC Reload để triệt tiêu hoàn toàn 100% hiện tượng xé hình (Screen Tearing)."*

---

# 4. KỊCH BẢN THUYẾT TRÌNH DỰ ÁN 3: WEARABLE IOT SMARTWATCH (ESP32-S3)

Khi nhà tuyển dụng hỏi về dự án thứ 3 trong CV (**Wearable IoT Smartwatch & Sensor Hub**), anh sử dụng tài liệu chuyên sâu [`project3_smartwatch_interview.md`](file:///d:/Project/STM32F7/project3_smartwatch_interview.md) để trình bày:

1. **Kiến trúc Dual-Core FreeRTOS SMP:**
   * Core 0 (PRO_CPU): Đảm nhiệm các tác vụ thời gian thực cứng gồm NimBLE Bluetooth Stack, Sensor Hub (MAX30102 nhịp tim + BMI270 đếm bước) và GPS UART 1PPS.
   * Core 1 (APP_CPU): Dành riêng cho bộ máy đồ họa LVGL v8 và xử lý cử chỉ cảm ứng CST816D.
2. **Kiến trúc bộ nhớ Hybrid (SRAM vs PSRAM):**
   * Đặt 2 Partial Buffer kích thước bằng 1/10 màn hình ($2 \times 33\text{ KB} = 66\text{ KB}$) trong Internal SRAM để đạt tốc độ render tối đa $1\text{ chu kỳ clock}$.
   * PSRAM ngoài 8MB chỉ dùng chứa asset ảnh tĩnh, font chữ tiếng Việt và bộ đệm OTA.
3. **Điểm nhấn gỡ lỗi kinh điển (Case Study 5 - 30 phút bị đơ máy rồi Reset):**
   * Phân tích 3 cơ chế: Rò rỉ Heap do hàm định dạng chuỗi động, phân mảnh bộ nhớ khi nhận tin nhắn dài, và Task Watchdog Timer (TWDT 5000ms) cắn chết khi xảy ra deadlock.
   * Giải pháp: Cấm tuyệt đối `malloc` tuần hoàn (dùng mảng tĩnh `static char`), tách luồng an toàn qua `FreeRTOS Queue` theo nguyên tắc Zero-Direct-Call, và gán I2C 9-Clock Recovery chống kẹt bus `SDA Stuck Low`.

---

# 5. BẢNG TỔNG HỢP LỆNH CHUYỂN ĐỔI NHANH (CHEAT SHEET)

| Mục Đích Thao Tác | Lệnh PowerShell Cần Gõ | Thời Gian Thực Thi |
| :--- | :--- | :--- |
| **Nạp dự án CAN Gateway (Zephyr RTOS)** | `.\build_zephyr.ps1` | $\approx 2\text{ giây}$ |
| **Nạp dự án Video 60 FPS (Bare-Metal)** | `powershell -ExecutionPolicy Bypass -File "D:\Project\TFT_video_STM32F7\build.ps1" -Action flash` | $\approx 2\text{ giây}$ |
| **Xem bảng Telemetry xe hơi (COM4)** | `vehicle status` | Tức thời |
| **Đọc mã lỗi chẩn đoán DTC (COM4)** | `dtc read` | Tức thời |
| **Xóa sạch toàn bộ mã lỗi DTC (COM4)** | `dtc clear` | Tức thời |
| **Mô phỏng đứt cáp CAN / Timeout (COM4)** | `can auto off` (sau 1s báo lỗi `0x0100` & chớp LED) | $1\text{ giây}$ |
| **Bật lại mạng CAN xe chạy bình thường** | `can auto on` | Tức thời |
| **Bơm lỗi quá nhiệt động cơ 115°C (COM4)** | `can inject overheat` (báo lỗi `0x0115` & chớp LED) | Tức thời |
| **Bơm lỗi đạp ga quá tua 6800 RPM (COM4)**| `can inject overspeed` (báo lỗi `0x0219` & chớp LED) | Tức thời |
| **Xem danh sách luồng đa nhiệm Zephyr** | `kernel threads` | Tức thời |
