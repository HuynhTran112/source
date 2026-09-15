# 🏆 [NGÀY 10] CẨM NANG TOÀN DIỆN ZEPHYR MULTI-THREADING: IPC MESSAGE QUEUES, MUTEX PRIORITY INHERITANCE & INTERACTIVE SHELL CLI
## Lộ trình 4 Bước: Kiến Trúc Đa Luồng ➔ Thực Chiến Kconfig ➔ Gõ Code Hệ Thống ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Hoàn thiện đỉnh cao kiến trúc phần mềm nhúng đa luồng (Multi-Threaded Architecture) cho thiết bị CAN Gateway trên STM32F746: Xây dựng mô hình giao tiếp liên luồng an toàn (Thread-Safe Inter-Process Communication - IPC) bằng hàng đợi **`k_msgq`**, triệt tiêu vĩnh viễn biến toàn cục và điều kiện cuộc đua (Race Conditions), chống hiện tượng đảo ngược mức ưu tiên (Priority Inversion) bằng **`k_mutex` có tính năng Priority Inheritance**, giám sát dung lượng ngăn xếp thời gian thực bằng **Thread Analyzer**, và tích hợp giao diện dòng lệnh chẩn đoán từ xa **Zephyr Interactive Shell CLI** qua cổng UART Console.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phân tầng hệ thống, giải thuật điều phối luồng, phân bổ hàng đợi IPC, cú pháp macro Shell và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 10                                    │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: KIẾN TRÚC │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE HỆ THỐNG   │ BƯỚC 4: PHỎNG VẤN          │
│ • Mô hình Producer│ • Soạn prj.conf   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn Multi-  │
│   - Consumer IPC  │ • Bật Zephyr Shell│ • TODO 1 [prj.conf]        │   Threading & IPC          │
│ • k_msgq vs k_fifo│ • Bật Thread      │ • TODO 2 [gateway_model.h] │ • Priority Inversion Bug   │
│ • Priority        │   Analyzer        │ • TODO 3 [gateway_ipc.c]   │ • Priority Inheritance     │
│   Inheritance     │ • Cấu hình Shell  │ • TODO 4 [cli_shell.c]     │ • Kịch bản trả lời 60s     │
│ • Thread Analyzer │   Command Tree    │ • TODO 5 [src/main.c]      │   (Elevator Pitch)         │
│ • Interactive CLI │                   │ • Mổ xẻ 5 Bug đa luồng     │                            │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC ZEPHYR MULTI-THREADING CỐT LÕI (CHUYÊN CHO NGÀY 10)

| STT | Quy tắc Zephyr Multi-Threading | Thể hiện cụ thể trong Ngày 10 (Multi-Threading & CLI) |
| :---: | :--- | :--- |
| **1** | **No Raw Global Variables** | Tuyệt đối không dùng biến toàn cục không được bảo vệ để truyền dữ liệu giữa các luồng. Mọi dữ liệu phải đi qua hàng đợi tin nhắn `k_msgq` hoặc biến trạng thái bọc Mutex. |
| **2** | **Priority Inheritance** | Luôn sử dụng `k_mutex` (mặc định đã bật Priority Inheritance) khi các luồng có mức ưu tiên khác nhau cùng chia sẻ tài nguyên. Không dùng `k_sem` (Binary Semaphore) làm khóa độc quyền vì Semaphore không có tính năng nâng mức ưu tiên! |
| **3** | **Zero-Copy vs Copy by Value** | Trong `k_msgq`, dữ liệu được copy theo giá trị (`memcpy`). Với cấu trúc dữ liệu nhỏ ($< 32$ bytes như CAN Frame hay telemetry data), cơ chế này an toàn tuyệt đối và không lo con trỏ rác (Dangling Pointer). |
| **4** | **Thread Sleep Obligation** | Mọi luồng trong hệ thống bắt buộc phải có ít nhất một điểm dừng giải phóng CPU (như chờ hàng đợi `k_msgq_get()`, hoặc gọi `k_msleep()`), không để bất kỳ luồng nào chạy vòng lặp đói (Busy-loop). |
| **5** | **Thread Analyzer Inspection** | Luôn theo dõi báo cáo của `CONFIG_THREAD_ANALYZER`. Nếu một luồng sử dụng quá $80\%$ ngăn xếp, phải lập tức tăng kích thước stack để tránh kích hoạt MPU Stack Guard. |
| **6** | **Shell Non-Blocking Execution** | Các lệnh trong Shell CLI (`SHELL_CMD_REGISTER`) phải thực thi nhanh và trả về ngay. Tuyệt đối không gọi các vòng lặp chờ đợi lâu trong callback của Shell làm tê liệt Console UART. |
| **7** | **Atomic Operations** | Đối với các biến cờ đếm gói tin (Counters), sử dụng các hàm thao tác nguyên tử `atomic_inc()` / `atomic_get()` thay vì phép toán `++` thông thường để tránh xung đột đa luồng. |

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG & CƠ CHẾ HOẠT ĐỘNG (SYSTEM ARCHITECTURE)

## 1.1. Kiến Trúc Phân Tầng Đa Luồng (The Producer-Consumer Actor Pattern)

Để xử lý luồng dữ liệu thời gian thực từ mạng CAN Bus đẩy lên màn hình táp-lô mà không làm treo hệ thống, dự án triển khai mô hình đa luồng phân cấp rõ ràng:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        KIẾN TRÚC PHÂN TẦNG ĐA LUỒNG ZEPHYR                             │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│   [ CAN BUS HARDWARE ]                                                                 │
│            │                                                                           │
│            ▼ (Ngắt ISR đẩy vào MsgQ)                                                  │
│   ┌──────────────────────────────────┐                                                 │
│   │ LUỒNG 1: CAN RX WORKER (Prio 4)  │ ◄── Luồng Sản Xuất (Producer - Ưu tiên Cao)     │
│   │ • k_msgq_get(&g_can_rx_msgq)     │                                                 │
│   │ • Trích xuất ID & Payload thô    │                                                 │
│   └────────────────┬─────────────────┘                                                 │
│                    │                                                                   │
│                    ▼ k_msgq_put(&g_telemetry_msgq) (Bất đồng bộ O(1), Zero-Lock)      │
│   ┌──────────────────────────────────┐                                                 │
│   │ LUỒNG 2: GUI MODEL UPDATER (Prio 5)◄── Luồng Tiêu Thụ (Consumer - Ưu tiên Vừa)   │
│   │ • Giải mã tín hiệu Tốc độ & RPM  │                                                 │
│   │ • Khóa g_gui_mutex               │                                                 │
│   │ • Cập nhật Model & LVGL Widgets  │                                                 │
│   └────────────────┬─────────────────┘                                                 │
│                    │                                                                   │
│                    ▼ lv_timer_handler() định kỳ 10ms                                   │
│   ┌──────────────────────────────────┐                                                 │
│   │ LUỒNG 3: GUI RENDER ENGINE (Prio 6) ◄── Luồng Hiển Thị (Ưu tiên Trung bình)        │
│   │ • Render pixel ra VDB            │                                                 │
│   │ • DMA2D copy ra SDRAM Framebuffer│                                                 │
│   └──────────────────────────────────┘                                                 │
│                                                                                        │
│   ┌──────────────────────────────────┐                                                 │
│   │ LUỒNG 4: SHELL CLI (Prio 8)      │ ◄── Luồng Chẩn Đoán (Ưu tiên Thấp)              │
│   │ • Nhận lệnh UART từ kỹ sư        │                                                 │
│   │ • In thống kê CPU, Stack, Bus CAN│                                                 │
│   └──────────────────────────────────┘                                                 │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Tài Liệu Zephyr Kernel:**
> 1. **Tra cứu Thiết Kế Đa Luồng:** Mở tài liệu Zephyr tại `https://docs.zephyrproject.org/latest/kernel/services/threads/index.html` (Mục *Threads & Workqueue Services*).
> 2. Đọc mô hình Producer-Consumer: Sử dụng Message Queue làm vùng đệm đồng bộ giữa các luồng có mức ưu tiên khác nhau, đảm bảo ngắt ISR hoặc luồng thời gian thực không bị chặn khi đẩy dữ liệu vào bộ đệm.

---

## 1.2. Mổ Xẻ Bẫy Đảo Ngược Mức Ưu Tiên (Priority Inversion) & Giải Pháp Priority Inheritance

Đây là một trong những câu hỏi phỏng vấn kinh điển nhất trong lập trình hệ điều hành thời gian thực (nổi tiếng với sự cố tàu thám hiểm Sao Hỏa Mars Pathfinder năm 1997):

```text
KỊCH BẢN NGUY HIỂM (Priority Inversion khi dùng Khóa không có Kế thừa ưu tiên):
1. Luồng Thấp (Low Prio - Shell CLI) chiếm giữ Mutex tài nguyên chung.
2. Luồng Cao (High Prio - CAN Worker) cần Mutex đó -> Bị khóa (Blocked) và đi ngủ.
3. Luồng Trung Bình (Medium Prio - Tính toán nền) nhảy vào chiếm CPU vì ưu tiên hơn Luồng Thấp.
4. KẾT QUẢ TAI HẠI: Luồng Thấp không có cơ hội chạy để nhả Mutex -> Luồng Cao bị treo vô hạn
   sau Luồng Trung Bình! (Đảo ngược mức ưu tiên: Thằng Cao nhất bị Thằng Trung bình đè bẹp!)
```

### Cách `k_mutex` trong Zephyr Hóa Giải Bằng Kế Thừa Ưu Tiên (Priority Inheritance):
* Ngay khi **Luồng Cao** cố gắng lấy Mutex đang bị giữ bởi **Luồng Thấp**:
* Nhân Zephyr **tự động nâng mức ưu tiên của Luồng Thấp lên ngang bằng với Luồng Cao**!
* Lúc này, Luồng Trung Bình không thể chen ngang Luồng Thấp được nữa.
* Luồng Thấp nhanh chóng xử lý xong đoạn găng, nhả Mutex ra $\implies$ Mức ưu tiên của nó hạ về như cũ, và Luồng Cao ngay lập tức giành quyền thực thi.

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Zephyr Synchronization Primitives:**
> 1. **Tra cứu Cơ chế Mutex:** Mở `https://docs.zephyrproject.org/latest/kernel/services/synchronization/mutexes.html`.
>    * Đọc phần *Priority Inheritance*: Zephyr mô tả chi tiết cách nhân can thiệp nâng mức ưu tiên động cho luồng giữ khóa khi có một luồng ưu tiên cao hơn đang xếp hàng chờ tài nguyên.
> 2. **So sánh với Semaphore:** Mở trang *Semaphores* và lưu ý: Zephyr Semaphore KHÔNG hỗ trợ Priority Inheritance, do đó không bao giờ được dùng Semaphore làm cơ chế bảo vệ đoạn găng thay cho Mutex.

---

## 1.3. So Sánh Cơ Chế IPC: `k_msgq` vs `k_fifo` vs `k_sem`

| Tiêu chí kỹ thuật | `k_msgq` (Message Queue) | `k_fifo` (First-In First-Out) | `k_sem` (Semaphore) |
| :--- | :--- | :--- | :--- |
| **Cơ chế truyền dữ liệu** | **Copy theo giá trị (`memcpy`)** vào bộ đệm tĩnh có sẵn. | **Truyền con trỏ (`pointer passing`)** tới vùng nhớ động. | Chỉ truyền **tín hiệu cờ (Event Signal)**, không mang dữ liệu. |
| **Cấp phát bộ nhớ** | Tĩnh hoàn toàn lúc khai báo (`CAN_MSGQ_DEFINE`). 0 rủi ro cấp phát. | Cần cấp phát bộ nhớ động (`k_heap` hoặc `k_mem_slab`) cho từng node. | 0 byte dữ liệu. |
| **Rủi ro rò rỉ bộ nhớ** | **KHÔNG CÓ** (Bộ nhớ quay vòng tĩnh). | CÓ NGUY CƠ (Nếu bên nhận quên `free` con trỏ nhận được). | Không có. |
| **Khuyến nghị sử dụng** | **Chuẩn mực cho gói tin vi điều khiển (CAN, UART, Sensor data).** | Truyền các khối dữ liệu khổng lồ (Ảnh camera, gói tin TCP/IP). | Đồng bộ hóa sự kiện đơn lẻ hoặc đếm tài nguyên. |

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Zephyr Data Passing:**
> 1. **Tra cứu Message Queue:** Mở `https://docs.zephyrproject.org/latest/kernel/services/data_passing/message_queues.html`.
>    * Xem cấu trúc hàng đợi Ring Buffer tĩnh: Các thao tác `k_msgq_put()` và `k_msgq_get()` thực hiện copy an toàn theo giá trị với độ phức tạp $O(1)$.
> 2. **Tra cứu Shell Subsystem:** Mở `https://docs.zephyrproject.org/latest/services/shell/index.html` để hiểu kiến trúc dòng lệnh CLI không chặn (Non-blocking UART backend).

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

> 🎯 **NGUYÊN TẮC TRA CỨU SHELL & IPC ĐA LUỒNG TRÊN ZEPHYR RTOS:**
> 1. **Tra cứu Kconfig Shell & Debug:** Mở `zephyr/subsys/shell/Kconfig` hoặc gõ `west build -t menuconfig` tìm kiếm `CONFIG_SHELL`, `CONFIG_THREAD_ANALYZER`.
> 2. **Tra cứu Shell API:** Mở header `zephyr/include/zephyr/shell/shell.h` để xem macro đăng ký lệnh `SHELL_CMD_REGISTER`.
> 3. **Tra cứu Kernel IPC API:** Mở header `zephyr/include/zephyr/kernel.h` để tra cứu cơ chế hàng đợi `k_msgq` và điều phối luồng `k_thread`.

---

## 2.1. Lộ trình Tra cứu Trực tiếp Giao diện Shell & Đa luồng (Shell & Multi-threading Lookup Methodology)

### 📖 Kênh 1: Cách Tra Cứu Cấu Hình Shell CLI Kconfig (`prj.conf`)
1. **Tìm kiếm các tùy chọn Backend của Shell:**
   * Trong `menuconfig` hoặc web: gõ `CONFIG_SHELL_BACKEND_SERIAL` để chuyển hướng dòng lệnh qua cổng ST-Link VCP (Virtual COM Port).
2. **Công cụ giám sát tràn ngăn xếp tự động (Thread Analyzer):**
   * Tra cứu `CONFIG_THREAD_ANALYZER_AUTO`: Kích hoạt một daemon ngầm định kỳ quét vùng MPU Stack Guard và tính toán High Watermark của từng luồng.
   * `CONFIG_THREAD_ANALYZER_AUTO_INTERVAL`: Chu kỳ quét tính bằng giây.

### 📖 Kênh 2: Cách Tra Cứu Cú Pháp Đăng Ký Lệnh CLI (`shell.h`)
1. **Mở file header:**
   * Đường dẫn: **`zephyr/include/zephyr/shell/shell.h`**.
2. **Đọc cú pháp Macro đăng ký lệnh:**
   * `SHELL_CMD_REGISTER(syntax, subcmds, help, handler)`: Đăng ký lệnh gốc.
   * `shell_print(const struct shell *sh, const char *fmt, ...)`: In văn bản có định dạng ra terminal.
   * Hàm Handler có mẫu chuẩn: `static int cmd_handler(const struct shell *sh, size_t argc, char **argv)`.

### 📖 Kênh 3: Cách Tra Cứu Kernel IPC Hàng Đợi (`kernel.h`)
1. **Khai báo hàng đợi tĩnh:**
   * Macro `K_MSGQ_DEFINE(q_name, q_msg_size, q_max_msgs, q_align)`: Cấp phát bộ nhớ tĩnh quay vòng (Ring Buffer) an toàn 100% không sợ phân mảnh RAM.
2. **Thao tác gửi/nhận bất đồng bộ:**
   * `k_msgq_put(struct k_msgq *msgq, const void *data, k_timeout_t timeout)`: Đẩy bản tin vào hàng đợi (trả về `-EAGAIN` nếu hàng đợi đầy).
   * `k_msgq_get(struct k_msgq *msgq, void *data, k_timeout_t timeout)`: Lấy bản tin ra (luồng nhận tự động Block tiết kiệm CPU nếu hàng đợi rỗng).

---

## 2.2. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS |
| :--- | :---: | :--- |
| **`CONFIG_SHELL`** | `y` | Kích hoạt hệ thống giao diện dòng lệnh Zephyr Shell. |
| **`CONFIG_SHELL_BACKENDS`** | `y` | Kích hoạt các backend hỗ trợ xuất nhập Shell. |
| **`CONFIG_SHELL_BACKEND_SERIAL`**| `y` | Sử dụng cổng UART nối tiếp Console làm cổng nhập lệnh Shell. |
| **`CONFIG_THREAD_ANALYZER`** | `y` | Bật công cụ đo lường mức tiêu thụ ngăn xếp của từng luồng. |
| **`CONFIG_THREAD_ANALYZER_USE_LOG`**| `y` | In kết quả phân tích Stack qua Zephyr Logging. |
| **`CONFIG_THREAD_ANALYZER_AUTO`**| `y` | Tự động quét và in báo cáo Stack định kỳ. |
| **`CONFIG_THREAD_ANALYZER_AUTO_INTERVAL`**| `10` | Chu kỳ quét phân tích ngăn xếp: 10 giây/lần. |

---

# 💻 BƯỚC 3: GÕ CODE HỆ THỐNG & MỔ XẺ BUG ĐA LUỒNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 10

```text
zephyr_gateway/
├── prj.conf           <-- Bật CONFIG_SHELL, CONFIG_THREAD_ANALYZER
└── src/
    ├── gateway_model.h<-- Khai báo cấu trúc Telemetry & Message Queue liên luồng
    ├── gateway_ipc.c  <-- Triển khai IPC hàng đợi giữa CAN và GUI Model
    ├── cli_shell.c    <-- Đăng ký hệ thống lệnh Shell tương tác (can_stats, set_speed)
    └── main.c         <-- Khởi tạo điều phối 4 luồng chạy song song
```

---

### 📂 KHỐI 1: ĐỊNH NGHĨA DỮ LIỆU LIÊN LUỒNG [ `src/gateway_model.h` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 1:
1. **Tra cứu Cấu trúc dữ liệu IPC và Atomic Types:**
   - **Mở Zephyr Docs** ➔ `Kernel Services -> Atomic Services`: Header `<zephyr/sys/atomic.h>` cung cấp kiểu `atomic_t` đảm bảo phép tăng giảm bộ đếm gói tin không bị chia cắt giữa các luồng.
   - Định nghĩa struct `VehicleTelemetry_t` chứa toàn bộ trường dữ liệu táp-lô xe hơi (Speed, RPM, Temp, Battery, Gear).

#### TODO 1 [File: `src/gateway_model.h`]: Cấu Trúc Dữ Liệu Táp-Lô Xe Hơi
```c
#ifndef GATEWAY_MODEL_H
#define GATEWAY_MODEL_H

#include <stdint.h>
#include <zephyr/kernel.h>

/**
 * @brief Cấu trúc dữ liệu trạng thái xe (Telemetry State)
 *        Được truyền an toàn qua k_msgq giữa các luồng
 */
typedef struct {
    uint16_t speed_kmh;      /* Tốc độ xe (0 - 240 km/h) */
    uint16_t engine_rpm;     /* Vòng tua máy (0 - 8000 RPM) */
    int8_t   coolant_temp_c; /* Nhiệt độ nước làm mát (-40 đến 125 C) */
    uint16_t battery_mv;     /* Điện áp ắc quy (mV, ví dụ 12400 cho 12.4V) */
    uint8_t  gear_position;  /* Số: 0=P, 1=R, 2=N, 3=D */
    uint32_t timestamp_ms;   /* Dấu thời gian nhận gói tin */
} VehicleTelemetry_t;

/* Hàng đợi tin nhắn IPC trung chuyển giữa luồng CAN và luồng GUI Model */
extern struct k_msgq g_telemetry_msgq;

/* Thống kê hiệu năng mạng CAN (Dùng biến atomic để an toàn đa luồng) */
extern atomic_t g_can_rx_count;
extern atomic_t g_can_tx_count;
extern atomic_t g_can_err_count;

#endif /* GATEWAY_MODEL_H */
```

---

### 📂 KHỐI 2: ĐIỀU PHỐI HÀNG ĐỢI IPC [ `src/gateway_ipc.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 2:
1. **Cấp phát hàng đợi tĩnh K_MSGQ_DEFINE:**
   - **Mở Zephyr Docs** ➔ `Kernel Services -> Message Queues`:
     - Cú pháp: `K_MSGQ_DEFINE(q_name, q_msg_size, q_max_msgs, q_align)`.
     - Phân bổ 8 slots, mỗi slot `sizeof(VehicleTelemetry_t)` với căn lề 4-byte.
2. **Khởi tạo biến Atomic:**
   - Sử dụng macro `ATOMIC_INIT(0)` để khởi tạo các biến đếm thống kê an toàn đa luồng.
3. **Luồng GUI Model Consumer:**
   - Chờ gói tin qua `k_msgq_get(&g_telemetry_msgq, &telemetry, K_FOREVER)`.
   - Cập nhật sang cụm đồng hồ táp-lô qua các API đã bảo vệ Mutex (`GUI_Cluster_UpdateSpeed`, `GUI_Cluster_UpdateRPM`).

#### TODO 2 [File: `src/gateway_ipc.c`]: Cấp Phát MsgQ & Luồng GUI Model Consumer
```c
#include "gateway_model.h"
#include "gui_cluster.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gateway_ipc, LOG_LEVEL_INF);

/* 1. Định nghĩa hàng đợi tin nhắn Telemetry (Chứa tối đa 8 phần tử) */
K_MSGQ_DEFINE(g_telemetry_msgq, sizeof(VehicleTelemetry_t), 8, 4);

/* Biến đếm thống kê kiểu Atomic */
atomic_t g_can_rx_count  = ATOMIC_INIT(0);
atomic_t g_can_tx_count  = ATOMIC_INIT(0);
atomic_t g_can_err_count = ATOMIC_INIT(0);

#define MODEL_THREAD_STACK_SIZE 2048
#define MODEL_THREAD_PRIORITY   5

void gui_model_consumer_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    VehicleTelemetry_t telemetry;
    LOG_INF("Luồng GUI Model Consumer bắt đầu lắng nghe hàng đợi IPC...");

    while (1) {
        /* Chờ nhận dữ liệu từ hàng đợi tin nhắn (Zero CPU load khi không có tin) */
        int ret = k_msgq_get(&g_telemetry_msgq, &telemetry, K_FOREVER);
        if (ret == 0) {
            /* Cập nhật an toàn sang thư viện đồ họa LVGL (Bảo vệ bằng Mutex) */
            GUI_Cluster_UpdateSpeed(telemetry.speed_kmh);
            GUI_Cluster_UpdateRPM(telemetry.engine_rpm);
        }
    }
}

K_THREAD_DEFINE(gui_model_tid, MODEL_THREAD_STACK_SIZE,
                gui_model_consumer_thread, NULL, NULL, NULL,
                MODEL_THREAD_PRIORITY, 0, 0);
```

---

### 📂 KHỐI 3: GIAO DIỆN DÒNG LỆNH CHẨN ĐOÁN [ `src/cli_shell.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 3:
1. **Hệ thống Shell trong Zephyr:**
   - **Mở Zephyr Docs** ➔ `Subsystems -> Shell`:
     - Header `<zephyr/shell/shell.h>`.
     - Macro `SHELL_STATIC_SUBCMD_SET_CREATE` định nghĩa danh sách lệnh con.
     - Macro `SHELL_CMD_REGISTER` đăng ký root command name (ví dụ `gateway`).
     - Hàm `shell_print(sh, fmt, ...)` in chuỗi ra Console không chặn.
2. **Trích xuất số liệu chẩn đoán:**
   - Sử dụng `atomic_get()` đọc biến đếm RX/TX/Error.
   - Sử dụng `k_msgq_num_free_get()` kiểm tra số lượng slot trống trong hàng đợi IPC.

#### TODO 3 [File: `src/cli_shell.c`]: Đăng Ký Hệ Thống Lệnh Zephyr Shell
```c
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "gateway_model.h"
#include "gui_cluster.h"

/* Lệnh in thống kê mạng CAN: "gateway stats" */
static int cmd_gateway_stats(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc); ARG_UNUSED(argv);

    shell_print(sh, "========================================");
    shell_print(sh, "       CAN GATEWAY DIAGNOSTICS          ");
    shell_print(sh, "========================================");
    shell_print(sh, "Gói tin nhận thành công (RX): %ld", atomic_get(&g_can_rx_count));
    shell_print(sh, "Gói tin gửi thành công (TX):  %ld", atomic_get(&g_can_tx_count));
    shell_print(sh, "Lỗi truyền thông (Errors):    %ld", atomic_get(&g_can_err_count));
    shell_print(sh, "Dung lượng hàng đợi IPC trống: %u/%u",
                k_msgq_num_free_get(&g_telemetry_msgq), 8);
    shell_print(sh, "========================================");
    return 0;
}

/* Lệnh giả lập tốc độ xe: "gateway set_speed <val>" */
static int cmd_gateway_set_speed(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Cú pháp sai! Ví dụ: gateway set_speed 120");
        return -EINVAL;
    }

    uint16_t speed = (uint16_t)atoi(argv[1]);
    if (speed > 240) {
        shell_warn(sh, "Tốc độ vượt ngưỡng an toàn (>240), tự động gán về 240 km/h");
        speed = 240;
    }

    /* Đóng gói vào struct và đẩy vào hàng đợi IPC */
    VehicleTelemetry_t test_data = {
        .speed_kmh = speed,
        .engine_rpm = speed * 35,
        .timestamp_ms = k_uptime_get_32()
    };

    k_msgq_put(&g_telemetry_msgq, &test_data, K_NO_WAIT);
    shell_print(sh, "Đã bơm dữ liệu mô phỏng: Tốc độ = %u km/h, RPM = %u", 
                test_data.speed_kmh, test_data.engine_rpm);
    return 0;
}

/* Cây phân cấp lệnh Shell */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gateway,
    SHELL_CMD(stats, NULL, "Hiển thị thống kê hoạt động CAN Gateway", cmd_gateway_stats),
    SHELL_CMD_ARG(set_speed, NULL, "Giả lập tốc độ xe (km/h)", cmd_gateway_set_speed, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gateway, &sub_gateway, "Lệnh chẩn đoán hệ thống CAN Gateway", NULL);
```

---

### 📂 KHỐI 4: ĐIỀU PHỐI TOÀN BỘ HỆ THỐNG [ `src/main.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 4:
1. **Kiến trúc Dispatcher Worker đa luồng:**
   - Luồng `can_dispatcher_thread` đọc frame từ `g_can_rx_msgq`, tăng biến đếm nguyên tử `g_can_rx_count`.
   - Phân giải ID 0x100 thành các trường `speed_kmh` và `engine_rpm`.
   - Chuyển tiếp tức thì vào `g_telemetry_msgq` bằng hàm `k_msgq_put(..., K_NO_WAIT)` (thao tác O(1) không khóa).

#### TODO 4 [File: `src/main.c`]: Cầu Nối Luồng CAN RX Vào Hàng Đợi Telemetry
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "can_gateway.h"
#include "gateway_model.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

#define CAN_WORKER_STACK_SIZE 2048
#define CAN_WORKER_PRIORITY   4

void can_dispatcher_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    struct can_frame rx_frame;
    LOG_INF("Khởi động luồng CAN Dispatcher Worker...");

    while (1) {
        /* Chờ gói tin từ hàng đợi CAN phần cứng */
        if (k_msgq_get(&g_can_rx_msgq, &rx_frame, K_FOREVER) == 0) {
            atomic_inc(&g_can_rx_count);

            /* Giả sử ID 0x100 mang dữ liệu Tốc độ xe (Bytes 0-1) và Vòng tua (Bytes 2-3) */
            if (rx_frame.id == 0x100 && rx_frame.dlc >= 4) {
                VehicleTelemetry_t telemetry;
                telemetry.speed_kmh  = (uint16_t)(rx_frame.data[0] | (rx_frame.data[1] << 8));
                telemetry.engine_rpm = (uint16_t)(rx_frame.data[2] | (rx_frame.data[3] << 8));
                telemetry.timestamp_ms = k_uptime_get_32();

                /* Đẩy dữ liệu đã phân tích vào hàng đợi Telemetry để luồng GUI tiêu thụ */
                k_msgq_put(&g_telemetry_msgq, &telemetry, K_NO_WAIT);
            }
        }
    }
}

K_THREAD_DEFINE(can_worker_tid, CAN_WORKER_STACK_SIZE,
                can_dispatcher_thread, NULL, NULL, NULL,
                CAN_WORKER_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Hệ thống Zephyr Multi-Threading CAN Gateway sẵn sàng!");
    CAN_Gateway_Init();
    return 0;
}
```

---

## 3.2. Mổ xẻ 5 Bug Đa Luồng "Kinh Điển" trong Ngày 10

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG ĐA LUỒNG & IPC                                   │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN MỀM               │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Dữ liệu hiển   │ Đọc/ghi biến toàn cục `VehicleState` đồng │ Loại bỏ biến toàn cục thô, dùng │
│    thị bị rác     │ thời từ 2 luồng khác nhau (Race Condition)│ `k_msgq` hoặc bọc Mutex.        │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Luồng CAN bị   │ Dùng `k_sem` làm khóa bảo vệ thay cho     │ Luôn dùng `k_mutex` để có cơ chế│
│    treo cứng      │ `k_mutex` gây ra lỗi Priority Inversion.  │ Priority Inheritance tự động.   │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Tràn hàng đợi  │ Kích thước item trong `K_MSGQ_DEFINE` sai │ Luôn dùng toán tử `sizeof(Type)`│
│    (Queue Mismatch)khác với kiểu struct truyền vào `put/get`. │ trong định nghĩa macro `k_msgq`.│
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. MPU Stack Guard│ Luồng Shell CLI bị tràn Stack khi in các  │ Tăng `CONFIG_SHELL_STACK_SIZE`  │
│    kích hoạt Panic│ bảng thông kê dung lượng lớn.             │ lên tối thiểu 2048 bytes.       │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Gói tin bị mất │ Hàng đợi `k_msgq_put` dùng `K_NO_WAIT` khi│ Tăng độ sâu hàng đợi hoặc tăng  │
│    khi bus dồn dập│ hàng đợi đã đầy mà không xử lý lỗi -EAGAIN│ ưu tiên cho Consumer Thread.    │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao trong hệ thống nhúng thời gian thực, kỹ sư chuyên nghiệp tuyệt đối không dùng biến toàn cục để truyền dữ liệu giữa các luồng?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Khi hai luồng có mức ưu tiên khác nhau cùng truy cập một biến toàn cục đa byte (ví dụ `uint32_t` hoặc một `struct`), phép toán đọc/ghi trên kiến trúc 32-bit Cortex-M không phải lúc nào cũng nguyên tử (Non-atomic). Nếu một luồng đang ghi dở nửa chừng 2 bytes đầu thì bị ngắt hoặc luồng ưu tiên cao hơn chen ngang vào đọc, luồng đọc sẽ nhận được dữ liệu bị xé vụn (Torn Read / Data Inconsistency) dẫn đến xử lý sai hoàn toàn.
  * Ngoài ra, biến toàn cục không cung cấp cơ chế thông báo sự kiện (Notification/Signaling). Luồng nhận buộc phải liên tục đọc biến trong vòng lặp vô tận (Polling) làm lãng phí chu kỳ CPU và năng lượng. Sử dụng hàng đợi `k_msgq` giải quyết triệt để: Dữ liệu được copy an toàn, và luồng nhận tự động chuyển sang trạng thái Sleep giải phóng CPU cho đến khi có dữ liệu mới.

### ❓ Câu 2: Cơ chế Priority Inheritance trong `k_mutex` của Zephyr giải quyết sự cố tàu Mars Pathfinder ra sao?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Sự cố Mars Pathfinder xảy ra do: Luồng thu thập dữ liệu (Ưu tiên thấp) nắm giữ Mutex bus thông tin. Luồng thông tin vô tuyến (Ưu tiên cao) cần Mutex nên phải chờ. Một luồng truyền thông thông thường (Ưu tiên trung bình) không cần Mutex nhảy vào chiếm CPU trong thời gian dài, gián tiếp làm luồng Thấp không thể nhả Mutex, khiến luồng Cao bị trễ hạn chót (Deadline Miss) và kích hoạt Watchdog Reset liên tục.
  * Trong Zephyr, `k_mutex` mặc định bật **Priority Inheritance**: Khi luồng Cao đợi Mutex, nhân hệ điều hành tạm thời nâng mức ưu tiên của luồng Thấp lên bằng luồng Cao. Điều này ngăn chặn mọi luồng Trung bình chen ngang, giúp luồng Thấp hoàn thành đoạn găng và nhả Mutex nhanh nhất có thể.

### ❓ Câu 3: Thread Analyzer trong Zephyr đo lường dung lượng ngăn xếp đã sử dụng bằng kỹ thuật gì?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * Khi một luồng được tạo ra với macro `K_THREAD_STACK_DEFINE`, Zephyr khởi tạo toàn bộ vùng nhớ ngăn xếp đó bằng một mẫu byte đặc biệt gọi là **Stack Watermark (Mã số ảo `0xAA` liên tiếp)**.
  * Trong quá trình luồng thực thi, khi con trỏ `SP` tụt xuống, các byte `0xAA` này sẽ bị ghi đè bởi biến cục bộ và con trỏ trả về hàm.
  * Bộ công cụ **Thread Analyzer** định kỳ quét từ đáy ngăn xếp ngược lên trên để tìm byte `0xAA` nguyên vẹn đầu tiên chưa bị ghi đè. Từ đó, nó tính toán chính xác số byte tối đa mà luồng đã từng sử dụng (High Watermark) và in ra tỷ lệ phần trăm tiêu thụ, giúp kỹ sư tối ưu hóa kích thước RAM ngăn xếp chuẩn xác.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 10, em hoàn thiện kiến trúc đa luồng toàn diện cho thiết bị CAN Gateway theo mô hình **Producer-Consumer Actor Pattern**.  
> Em triệt tiêu hoàn toàn biến toàn cục bằng cách thiết lập đường truyền dữ liệu liên luồng thông qua hàng đợi **`k_msgq`**, kết nối bất đồng bộ từ **CAN Dispatcher Worker (Priority 4)** sang **GUI Model Updater (Priority 5)** với độ trễ cực thấp và an toàn bộ nhớ tuyệt đối. Để loại bỏ nguy cơ Deadlock và đảo ngược mức ưu tiên, em áp dụng **`k_mutex` có tính năng Priority Inheritance** bảo vệ vùng dữ liệu táp-lô dùng chung.  
> Cuối cùng, em tích hợp hệ thống **Zephyr Shell CLI** và công cụ **Thread Analyzer** qua UART Console, cho phép kỹ sư theo dõi trực tiếp mức tiêu thụ ngăn xếp của từng luồng và chẩn đoán số lượng gói tin CAN gửi nhận theo thời gian thực ngay trên bo mạch mà không cần gắn mạch gỡ lỗi ST-Link."*
