# 🏆 [NGÀY 7] CẨM NANG TOÀN DIỆN ZEPHYR RTOS: BOARD BRING-UP, DEVICETREE OVERLAY & HỆ THỐNG ĐA LUỒNG (MULTI-THREADING)
## Lộ trình 4 Bước: Kiến Trúc RTOS ➔ Thực Chiến Devicetree/Kconfig ➔ Gõ Code Ứng Dụng ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Chuyển dịch toàn diện từ tư duy Bare-metal sang hệ điều hành thời gian thực **Zephyr RTOS** trên STM32F746: Làm chủ cơ chế tách biệt phần cứng bằng cây thiết bị **Devicetree (`.dts`/`.overlay`)** và bộ cấu hình nhân **Kconfig (`prj.conf`)**, cấu hình ánh xạ chân vật lý qua **Pinctrl**, tạo và điều phối đa luồng **Kernel Multi-Threading (`k_thread`)**, thiết lập lá chắn bảo vệ tràn ngăn xếp bằng phần cứng **`CONFIG_MPU_STACK_GUARD`** và hệ thống nhật ký bất đồng bộ **Zephyr Logging (`LOG_MODULE_REGISTER`)**.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phân tầng hệ thống, cấu trúc Devicetree node, Kconfig symbols, bảng so sánh tài nguyên và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 7                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: KIẾN TRÚC │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE ỨNG DỤNG   │ BƯỚC 4: PHỎNG VẤN          │
│ • Triết lý Zephyr │ • Soạn prj.conf   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn Zephyr  │
│ • Devicetree vs   │ • Viết overlay    │ • TODO 1 [prj.conf]        │   & Devicetree             │
│   Compile Macros  │ • Bảng Kconfig    │ • TODO 2 [app.overlay]     │ • MPU Stack Guard Trap     │
│ • Multi-threading │ • Pinctrl Node    │ • TODO 3 [CMakeLists.txt]  │ • Cooperative vs Preempt   │
│ • MPU Stack Guard │ • Khóa Device     │ • TODO 4-5 [src/main.c]    │ • Kịch bản trả lời 60s     │
│ • Zephyr Logging  │   Binding API     │ • Mổ xẻ 5 Bug hệ thống     │   (Elevator Pitch)         │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC ZEPHYR RTOS CỐT LÕI (CHUYÊN CHO NGÀY 7)

| STT | Quy tắc Zephyr RTOS | Thể hiện cụ thể trong Ngày 7 (Board Bring-Up) |
| :---: | :--- | :--- |
| **1** | **Compile-Time Evaluation** | Devicetree trong Zephyr **hoàn toàn được tính toán lúc biên dịch (Compile-time)**. Mọi hàm `DT_PROP` hay `DEVICE_DT_GET` sinh ra hằng số tĩnh, tiêu thụ 0 byte RAM và 0 chu kỳ khởi động. |
| **2** | **Device Ready Check** | Trước khi dùng bất kỳ ngoại vi nào lấy từ `DEVICE_DT_GET`, **BẮT BUỘC** phải gọi `device_is_ready(dev)`. Tuyệt đối không gọi hàm API khi thiết bị chưa sẵn sàng (tránh lỗi Kernel Oops / HardFault). |
| **3** | **Explicit Thread Stack** | Khai báo ngăn xếp cho Thread bằng macro `K_THREAD_STACK_DEFINE(name, size)`. Tuyệt đối không dùng mảng C thông thường `uint8_t stack[size]` vì sẽ làm sai lệch căn lề (Alignment) của phần cứng MPU. |
| **4** | **MPU Stack Protection** | Luôn bật `CONFIG_MPU_STACK_GUARD=y` trong `prj.conf`. Phần cứng MPU sẽ đặt 1 trang nhớ 32 bytes cấm ghi (Guard Region) ở đáy ngăn xếp để bắt trọn vẹn lỗi Stack Overflow ngay chu kỳ ghi đầu tiên. |
| **5** | **Preemptive vs Cooperative** | Phân biệt Priority: Giá trị âm (từ `-CONFIG_NUM_COOP_PRIO` đến `-1`) là luồng Cooperative (không bị chen ngang), giá trị dương (từ `0` đến `CONFIG_NUM_PREEMPT_PRIO-1`) là luồng Preemptive. |
| **6** | **Deferred Logging** | Dùng `CONFIG_LOG_MODE_DEFERRED=y`. Tuyệt đối không dùng `printf` thô hay log Blocking trong ISR/Real-time threads để không làm trôi thời gian thực (Jitter). |
| **7** | **Pinctrl Dependency** | Trong STM32 Devicetree, chân GPIO không được cấu hình tự do bằng thanh ghi trong driver mà phải được khai báo tập trung trong node `&pinctrl` của file overlay. |

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG & CƠ CHẾ HOẠT ĐỘNG (SYSTEM ARCHITECTURE)

## 1.1. Triết Lý Thiết Kế của Zephyr RTOS: Devicetree + Kconfig

Trong Bare-metal (từ Ngày 0 đến Ngày 6), toàn bộ địa chỉ thanh ghi (`0x4002 0000`), số chân GPIO (`PA9/PB7`) và cấu hình bộ nhớ được định nghĩa cứng trong file code C. Khi chuyển chip hoặc đổi chân cắm, kỹ sư phải sửa trực tiếp trong source code driver.

**Zephyr RTOS giải quyết bài toán này bằng mô hình tách lớp 3 thành phần độc lập:**

```text
                                  ┌──────────────────────────────┐
                                  │      MÃ NGUỒN C ỨNG DỤNG     │ (src/main.c)
                                  │  (Thuần logic, gọi Driver API)│
                                  └──────────────┬───────────────┘
                                                 │
                        ┌────────────────────────┴────────────────────────┐
                        ▼                                                 ▼
     ┌────────────────────────────────────┐             ┌───────────────────────────────────┐
     │      DEVICETREE (.dts / .overlay)  │             │          KCONFIG (prj.conf)       │
     ├────────────────────────────────────┤             ├───────────────────────────────────┤
     │ • Định nghĩa PHẦN CỨNG:            │             │ • Định nghĩa TÍNH NĂNG PHẦN MỀM:  │
     │   - Chân cắm ngoại vi (Pin Muxing) │             │   - Bật/tắt Driver (CONFIG_GPIO=y)│
     │   - Địa chỉ Base Address & Offset  │             │   - Bật MPU Guard, Heap, Log      │
     │   - Tần số xung nhịp Clock         │             │   - Số lượng luồng, Kích thước RAM│
     │   - Kênh DMA, Ngắt IRQ vector      │             │                                   │
     └────────────────────────────────────┘             └───────────────────────────────────┘
```

### Devicetree trong Zephyr khác gì Linux?
* **Trên Linux nhúng:** File Devicetree được biên dịch thành file nhị phân `.dtb`. Khi Linux khởi động, nhân Kernel nạp file `.dtb` vào RAM và duyệt cây (Parsing) lúc Run-time $\implies$ Tốn nhiều RAM và thời gian boot.
* **Trên Zephyr RTOS:** Bộ tiền xử lý Python đọc file Devicetree và tạo ra file header **`devicetree_generated.h`** chứa các macro `#define` tĩnh. Khi biên dịch code C, Trình biên dịch GCC thay thế trực tiếp các macro này $\implies$ **Tốn đúng 0 byte RAM, thời gian nạp bằng 0 chu kỳ lệnh!**

---

## 1.2. Cơ Chế Đa Luồng (Multi-Threading) & Phân Bổ Mức Ưu Tiên

Nhân Zephyr quản lý các tác vụ thực thi bằng bộ lập lịch ưu tiên dựa trên thời gian (Preemptive Priority-based Scheduler):

```text
Độ ưu tiên CAO  ▲
                │  Cooperative Threads (Mức ưu tiên âm: -16 đến -1)
                │  • Không bao giờ bị luồng khác chen ngang (Preempt).
                │  • Chỉ nhường CPU khi tự nguyện gọi k_yield() hoặc k_sleep().
                │  • Ứng dụng: Tác vụ truyền thông khẩn cấp CAN Bus, giải mã túi khí.
  ──────────────┼────────────────────────────────────────────────────────────────────────
                │  Preemptive Threads (Mức ưu tiên không âm: 0 đến 14)
                │  • Luồng có mức ưu tiên cao hơn (số bé hơn) ĐƯỢC CHEN NGANG luồng thấp.
                │  • Các luồng cùng mức ưu tiên được chia sẻ thời gian (Time-slicing).
                │  • Ứng dụng: GUI Render, Tác vụ nền (Background Worker), CLI Shell.
                │
Độ ưu tiên THẤP ▼  Idle Thread (Mức ưu tiên thấp nhất: CONFIG_NUM_PREEMPT_PRIO)
```

---

## 1.3. Cơ Chế Bảo Vệ Ngăn Xếp Bằng Phần Cứng (`CONFIG_MPU_STACK_GUARD`)

Trong hệ điều hành thời gian thực, lỗi nguy hiểm nhất là **Tràn ngăn xếp luồng (Thread Stack Overflow)**. Khi một luồng dùng hết ngăn xếp được cấp, dữ liệu sẽ ghi đè lên ngăn xếp của luồng kế bên trong RAM, gây sập hệ thống ngẫu nhiên rất khó gỡ lỗi.

Zephyr sử dụng khối **MPU (Memory Protection Unit)** của ARM Cortex-M7 để tạo ra cơ chế phòng thủ vật lý:

```text
Địa chỉ CAO   ▲ ┌───────────────────────────────────────────────┐
              │ │   Vùng Ngăn Xếp Cho Phép (Thread Stack Area)  │ (Đọc/Ghi bình thường)
              │ │   Tụt dần xuống địa chỉ thấp (Con trỏ SP)     │ ▼
              │ ├───────────────────────────────────────────────┤
              │ │   MPU GUARD REGION (Cấm tuyệt đối truy cập!)  │ ◄── 32 Bytes do MPU khóa chặt!
Địa chỉ THẤP  ▼ └───────────────────────────────────────────────┘
```

* Khi con trỏ `SP` của luồng tụt quá giới hạn và ghi vào vùng **MPU Guard Region**:
* Phần cứng Cortex-M7 lập tức kích hoạt ngoại lệ **`MemManage Fault`**.
* Nhân Zephyr bắt ngay lập tức luồng phạm quy, in chính xác tên luồng và địa chỉ gây lỗi ra Terminal qua hàm `k_panic()`, ngăn chặn hoàn toàn việc phá hỏng dữ liệu của các luồng khác!

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

## 2.1. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

Tập tin `prj.conf` kích hoạt các subsystem cần thiết cho dự án:

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS |
| :--- | :---: | :--- |
| **`CONFIG_GPIO`** | `y` | Bật hệ thống driver điều khiển GPIO chuẩn Zephyr. |
| **`CONFIG_SERIAL`** | `y` | Bật giao tiếp nối tiếp UART/USART. |
| **`CONFIG_CONSOLE`** | `y` | Điều hướng đầu ra Console sang cổng nối tiếp ST-Link. |
| **`CONFIG_UART_CONSOLE`** | `y` | Sử dụng UART làm kênh Console chuẩn. |
| **`CONFIG_LOG`** | `y` | Bật hệ thống ghi nhật ký Zephyr Logging Subsystem. |
| **`CONFIG_LOG_MODE_DEFERRED`**| `y` | Nhật ký ghi vào RAM đệm, chỉ in ra UART lúc CPU rảnh rỗi (Chống giật khung hình). |
| **`CONFIG_MPU_STACK_GUARD`** | `y` | Bật mạch phần cứng MPU giám sát tràn ngăn xếp của từng luồng. |
| **`CONFIG_THREAD_NAME`** | `y` | Cho phép gán tên chuỗi cho từng luồng phục vụ gỡ lỗi. |

---

## 2.2. Ánh Xạ Chân Phần Cứng trong File Overlay (`app.overlay`)

Bo mạch STM32F746G-Discovery có đèn LED màu xanh lá tại chân **`PI1`** và nút bấm User B1 tại chân **`PI11`**. Ta khai báo cấu trúc phần cứng chuẩn trong Devicetree:

```dts
/ {
    aliases {
        led0 = &green_led;
    };

    leds {
        compatible = "gpio-leds";
        green_led: led_0 {
            gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>;
            label = "User Green LED PI1";
        };
    };
};
```

---

# 💻 BƯỚC 3: GÕ CODE ỨNG DỤNG & MỔ XẺ BUG HỆ THỐNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 7

```text
zephyr_gateway/
├── CMakeLists.txt     <-- Khai báo dự án Zephyr CMake
├── prj.conf           <-- Cấu hình hệ điều hành Kconfig
├── app.overlay        <-- Cây thiết bị phần cứng Devicetree cho STM32F746-Disco
└── src/
    └── main.c         <-- Tạo luồng Worker, điều khiển GPIO và chạy Logging
```

---

### 📂 KHỐI 1: CẤU HÌNH DỰ ÁN [ `CMakeLists.txt` & `prj.conf` ]

#### TODO 1 [File: `CMakeLists.txt`]: Tích Hợp Dự Án Zephyr RTOS
```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(can_gateway_zephyr)

target_sources(app PRIVATE src/main.c)
```

#### TODO 2 [File: `prj.conf`]: Cấu Hình Subsystem Nhân Zephyr
```properties
# Bật tính năng điều khiển GPIO và Console UART
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y

# Bật hệ thống Zephyr Logging đa tầng
CONFIG_LOG=y
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Bảo vệ an toàn bộ nhớ và ngăn xếp luồng
CONFIG_MPU_STACK_GUARD=y
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y

# Cấp phát bộ nhớ Heap chung cho hệ thống (4KB)
CONFIG_HEAP_MEM_POOL_SIZE=4096
```

---

### 📂 KHỐI 2: ĐỊNH NGHĨA PHẦN CỨNG [ `app.overlay` ]

#### TODO 3 [File: `app.overlay`]: Khai Báo Node LED & Thiết Bị Ngoại Vi
```dts
/ {
    aliases {
        led-status = &status_led;
    };

    leds {
        compatible = "gpio-leds";
        status_led: led_pi1 {
            gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>;
            label = "Status LED PI1";
        };
    };
};

/* Bật bộ điều khiển GPIOI trên chip STM32F7 */
&gpioi {
    status = "okay";
};
```

---

### 📂 KHỐI 3: MÃ NGUỒN ỨNG DỤNG ĐA LUỒNG [ `src/main.c` ]

#### TODO 4 [File: `src/main.c`]: Khởi Tạo Luồng Độc Lập & Nhận Diện Thiết Bị Devicetree
```c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* Đăng ký Module Nhật ký với Zephyr Logging */
LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

/* Kích thước ngăn xếp và Mức ưu tiên của các luồng */
#define BLINK_STACK_SIZE   1024
#define BLINK_PRIORITY     7

#define MONITOR_STACK_SIZE 2048
#define MONITOR_PRIORITY   5

/* Lấy thông số cấu hình LED từ Devicetree thông qua alias 'led-status' */
static const struct gpio_dt_spec s_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_status), gpios);

/* Prototype cho các hàm thực thi của luồng */
void blink_thread_entry(void *p1, void *p2, void *p3);
void monitor_thread_entry(void *p1, void *p2, void *p3);

/* BƯỚC 1: ĐỊNH NGHĨA VÀ KHỞI TẠO TĨNH CÁC LUỒNG BẰNG K_THREAD_DEFINE */
K_THREAD_DEFINE(blink_tid, BLINK_STACK_SIZE,
                blink_thread_entry, NULL, NULL, NULL,
                BLINK_PRIORITY, 0, 0);

K_THREAD_DEFINE(monitor_tid, MONITOR_STACK_SIZE,
                monitor_thread_entry, NULL, NULL, NULL,
                MONITOR_PRIORITY, 0, 0);
```

#### TODO 5 [File: `src/main.c`]: Thân Luồng Thực Thi & Kiểm Tra Sẵn Sàng Thiết Bị
```c
void blink_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    /* BƯỚC 2: KIỂM TRA TÍNH SẴN SÀNG CỦA NGOẠI VI (DEVICE READY CHECK) */
    if (!gpio_is_ready_dt(&s_led)) {
        LOG_ERR("Lỗi: Ngoại vi GPIO điều khiển LED chưa sẵn sàng!");
        return;
    }

    /* BƯỚC 3: CẤU HÌNH HƯỚNG XUẤT CHO CHÂN GPIO */
    int ret = gpio_pin_configure_dt(&s_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Lỗi: Không thể cấu hình chân LED PI1 (Mã lỗi: %d)", ret);
        return;
    }

    LOG_INF("Khởi tạo luồng Blink LED thành công! Bắt đầu chớp tắt...");

    while (1) {
        gpio_pin_toggle_dt(&s_led);
        /* Tự động chuyển CPU sang trạng thái Sleep và nhường tài nguyên cho luồng khác */
        k_msleep(500);
    }
}

void monitor_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Khởi tạo luồng Hệ Thống Giám Sát (Monitor Worker) thành công!");

    uint32_t heartbeat_count = 0;
    while (1) {
        heartbeat_count++;
        LOG_INF("[HEARTBEAT #%u] Hệ điều hành Zephyr RTOS đang chạy ổn định!", heartbeat_count);
        
        /* Định kỳ 2 giây in thông điệp trạng thái một lần */
        k_sleep(K_SECONDS(2));
    }
}

int main(void)
{
    LOG_INF("=================================================");
    LOG_INF("     STM32F746 DUAL-GATEWAY ZEPHYR BRING-UP      ");
    LOG_INF("=================================================");
    
    /* Hàm main kết thúc, nhường hoàn toàn quyền điều khiển cho bộ lập lịch Zephyr */
    return 0;
}
```

---

## 3.2. Mổ xẻ 5 Bug Hệ Thống "Kinh Điển" trong Ngày 7

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG ZEPHYR RTOS                                      │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA HỆ ĐIỀU HÀNH           │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. `DEVICE_DT_GET`│ Quên bật Kconfig tương ứng trong `prj.conf`│ Kiểm tra `prj.conf`: phải có    │
│    trả về con trỏ │ (ví dụ thiếu `CONFIG_GPIO=y`). Driver     │ `CONFIG_GPIO=y`, kiểm tra node  │
│    NULL hoặc treo │ không được biên dịch vào mã máy.          │ `status = "okay";` ở overlay.   │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Kernel Panic   │ Cấp phát Stack luồng quá nhỏ (ví dụ 256B) │ Tăng kích thước Stack lên 1KB   │
│    (MemManage)    │ khiến biến cục bộ đè vào MPU Guard Region.│ hoặc 2KB; tránh đệ quy/mảng to. │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Các luồng khác │ Luồng Cooperative (Priority âm) chạy vòng │ Thêm `k_yield()` hoặc           │
│    bị chết đói    │ lặp vô tận tính toán mà không nhường CPU. │ `k_msleep()` vào trong thân lặp.│
│    (Starvation)   │                                           │                                 │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Mất log hoặc in│ Dùng `LOG_MODE_IMMEDIATE=y` trong luồng   │ Đổi sang `LOG_MODE_DEFERRED=y`  │
│    chậm trễ ngắt  │ tần số cao làm nghẽn UART Console.        │ để log được đệm vào RAM.        │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Lỗi biên dịch  │ Tên alias trong file overlay gõ sai khác  │ Kiểm tra khớp từng ký tự giữa   │
│    macro DT_ALIAS │ với chuỗi truyền vào macro trong C code.  │ `DT_ALIAS(led_status)` & overlay│
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Devicetree trong Zephyr RTOS được nạp và xử lý vào thời điểm nào? Nó tối ưu hơn Linux nhúng ra sao?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Trong Zephyr RTOS, Devicetree được xử lý **hoàn toàn ở giai đoạn Biên dịch (Compile-time)** bằng bộ công cụ Python. Nó trích xuất dữ liệu từ các file `.dts`/`.overlay` và sinh ra trực tiếp mã nguồn C macro tĩnh (`devicetree_generated.h`).
  * Khác với Linux nhúng phải nạp file nhị phân `.dtb` vào RAM và duyệt cây lúc boot (tốn hàng chục KB RAM và chu kỳ CPU), Zephyr không tốn một byte RAM nào cho việc lưu cấu trúc cây lúc chạy. Trình biên dịch GCC có thể thực hiện tối ưu hóa hằng số (Constant Folding) và loại bỏ mã chết (Dead Code Elimination) ngay từ lúc build.

### ❓ Câu 2: Cơ chế `CONFIG_MPU_STACK_GUARD` phát hiện lỗi tràn ngăn xếp (Stack Overflow) như thế nào?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Thay vì dùng phương pháp kiểm tra phần mềm thụ động (như ghi mẫu số ảo Canary `0xA5A5A5A5` ở đáy stack rồi chờ bộ lập lịch kiểm tra định kỳ), Zephyr kích hoạt khối phần cứng **MPU (Memory Protection Unit)** của ARM Cortex-M7.
  * MPU sẽ cấu hình một vùng đệm kích thước 32 bytes nằm ngay dưới đáy ngăn xếp của luồng thành vùng cấm truy cập (`No Access`). Cứ mỗi lần CPU chuyển đổi ngữ cảnh (Context Switch), MPU tự động tái lập trình vùng cấm này tương ứng với luồng chuẩn bị chạy. Nếu con trỏ ngăn xếp `SP` vượt ranh giới và ghi đè vào đây, ngoại lệ phần cứng **`MemManage Fault`** lập tức nổ ra ngay tại chu kỳ lệnh vi phạm, bắt quả tang chính xác dòng lệnh gây lỗi.

### ❓ Câu 3: Sự khác biệt cơ bản giữa Luồng Cooperative và Luồng Preemptive trong Zephyr là gì?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * **Cooperative Thread (Priority âm: `-CONFIG_NUM_COOP_PRIO` đến `-1`):** Một khi đã chiếm CPU, nó sẽ nắm quyền thực thi độc quyền. Không có bất kỳ luồng nào khác (kể cả luồng có priority cao hơn) có thể chen ngang, trừ khi chính nó chủ động nhường quyền bằng lệnh `k_yield()` hoặc `k_sleep()`.
  * **Preemptive Thread (Priority không âm: `0` đến `CONFIG_NUM_PREEMPT_PRIO - 1`):** Bộ lập lịch có quyền tước quyền thực thi bất kỳ lúc nào nếu có một luồng khác có priority cao hơn sẵn sàng chạy hoặc khi hết khoảng thời gian chia sẻ (Time slice).

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Sau khi làm chủ toàn bộ nền tảng Bare-metal ở tầng thanh ghi, em mở rộng dự án CAN Gateway lên hệ điều hành **Zephyr RTOS** để khai thác khả năng đa nhiệm và quản trị phần cứng hiện đại.  
> Em thiết kế file **`app.overlay`** để phân tách hoàn toàn tầng mô tả chân vật lý (Pinctrl) khỏi mã nguồn C, giúp ứng dụng có thể dễ dàng chuyển đổi sang các dòng chip STM32 khác chỉ bằng việc thay file cấu hình.  
> Để bảo vệ hệ thống trước các lỗi bộ nhớ thời gian thực, em cấu hình tính năng **`CONFIG_MPU_STACK_GUARD`** tận dụng khối MPU phần cứng của Cortex-M7 bắt lỗi tràn ngăn xếp tức thời, đồng thời áp dụng mô hình **Deferred Logging** để toàn bộ hoạt động in vết hệ thống được đẩy vào RAM đệm và xử lý lúc CPU rảnh rỗi, triệt tiêu hoàn toàn hiện tượng jitter hay trễ nhịp thời gian thực của các luồng truyền thông."*
