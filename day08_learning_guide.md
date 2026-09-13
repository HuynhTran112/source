# 🏆 [NGÀY 8] CẨM NANG TOÀN DIỆN ZEPHYR CAN SUBSYSTEM: ASYNCHRONOUS MESSAGE QUEUE & TRANSCEIVER STANDBY CONTROL
## Lộ trình 4 Bước: Kiến Trúc Subsystem ➔ Thực Chiến Devicetree/Kconfig ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ hệ thống truyền thông mạng ô tô thời gian thực qua **Zephyr CAN Subsystem** trên STM32F746: Khai báo cấu hình phần cứng CAN Controller và Pinctrl trên Devicetree overlay ($500\text{ kbps}$, Sample Point $87.5\%$), giải quyết triệt để lỗi "bus im lặng" bằng mạch điều khiển chân **Transceiver Standby (STB/EN)**, ứng dụng cơ chế gắn bộ lọc phần cứng thẳng vào hàng đợi tin nhắn nhân hệ điều hành **`can_add_rx_filter_msgq`** giúp giải phóng hoàn toàn ngữ cảnh ngắt ISR, và xử lý giám sát trạng thái mạng tự động (**Bus-Off State Change Callback**).  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phân tầng hệ thống, cấu trúc Devicetree node, Kconfig symbols, giải thuật hàng đợi bất đồng bộ và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 8                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: KIẾN TRÚC │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE ỨNG DỤNG   │ BƯỚC 4: PHỎNG VẤN          │
│ • Zephyr CAN Sub- │ • Soạn prj.conf   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn CAN     │
│   system Pipeline │ • Viết overlay    │ • TODO 1 [prj.conf]        │   Subsystem Zephyr         │
│ • RX Filter MsgQ  │   Node & Pinctrl  │ • TODO 2 [app.overlay]     │ • Bẫy chân Transceiver STB │
│ • Điều khiển STB  │ • Khóa Device     │ • TODO 3 [can_gateway.h]   │ • Filter MsgQ vs Callback  │
│ • Bus-Off State   │   Binding CAN     │ • TODO 4 [can_gateway.c]   │ • Kịch bản trả lời 60s     │
│   Change Handler  │ • Bảng Filter Cấu │ • TODO 5 [src/main.c]      │   (Elevator Pitch)         │
│                   │   hình Chuẩn      │ • Mổ xẻ 5 Bug phần cứng    │                            │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC ZEPHYR CAN CỐT LÕI (CHUYÊN CHO NGÀY 8)

| STT | Quy tắc Zephyr CAN | Thể hiện cụ thể trong Ngày 8 (CAN Subsystem) |
| :---: | :--- | :--- |
| **1** | **Transceiver STB Drive** | **QUY TẮC SỐNG CÒN:** Trên board có chip transceiver ngoài (`TJA1050`/`SN65HVD230`), chân Standby (STB) mặc định thả nổi sẽ khiến transceiver ngủ đông. Bắt buộc phải cấu hình GPIO kéo chân này xuống `LOW` trước khi gọi `can_start()`. |
| **2** | **Filter-to-MsgQ Binding** | Sử dụng API `can_add_rx_filter_msgq()` để gắn bộ lọc phần cứng trực tiếp vào `k_msgq`. Tránh dùng callback `can_add_rx_filter_cb()` nếu tác vụ xử lý frame tốn thời gian, tránh làm trễ các ngắt khác của nhân. |
| **3** | **Explicit Sample Point** | Khai báo rõ ràng `sample-point = <875>;` (tương đương $87.5\%$) trong node Devicetree. Không để Zephyr tự tính toán bừa làm sai lệch vị trí lấy mẫu so với mạng ô tô chuẩn CiA 301. |
| **4** | **Filter Allocation Limit** | Kiểm tra `CONFIG_CAN_MAX_FILTER`. Giá trị mặc định thường là 5; nếu đăng ký nhiều hơn mà không tăng thông số này trong `prj.conf`, hàm `can_add_rx_filter` sẽ trả về mã lỗi `-ENOSPC` (No space left). |
| **5** | **Thread-Safe Queue Size** | Kích thước của `k_msgq` phải được tính toán đủ chứa burst frames: `sizeof(struct can_frame) * N`. Nếu queue đầy, gói tin mới sẽ bị drop. |
| **6** | **Asynchronous Non-blocking TX** | Khi truyền dữ liệu bằng `can_send()`, truyền kèm callback hoặc dùng `K_MSEC(timeout)`. Không truyền vô tận `K_FOREVER` trong luồng điều khiển chính để tránh bị khóa chết (Deadlock) khi bus bị ngắt. |
| **7** | **State Callback Monitoring** | Đăng ký `can_set_state_change_callback()` để đón nhận sự kiện chuyển trạng thái sang `CAN_STATE_BUS_OFF` và thực hiện kích hoạt chu kỳ phục hồi theo tiêu chuẩn an toàn. |

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG & CƠ CHẾ HOẠT ĐỘNG (SYSTEM ARCHITECTURE)

## 1.1. Kiến Trúc Phân Tầng Zephyr CAN Subsystem

Trong Bare-metal ở Ngày 3, chúng ta phải tự quản lý 3 Transmit Mailbox, tự gõ bit nạp `TI0R`, tự đọc `RDLR/RDHR` và kiểm tra cờ `FMP0`.  
**Zephyr CAN Subsystem trừu tượng hóa toàn bộ phần cứng thành kiến trúc hướng dịch vụ:**

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        ỨNG DỤNG NGƯỜI DÙNG (CAN Worker Thread)                         │
│   • can_send(dev, &frame, timeout, callback, user_data)                                │
│   • k_msgq_get(&can_rx_msgq, &rx_frame, K_FOREVER)  <-- Nhận frame dạng Zero-CPU     │
└───────────────────────────────────────────▲────────────────────────────────────────────┘
                                            │ (k_msgq_put an toàn)
┌───────────────────────────────────────────┴────────────────────────────────────────────┐
│                    ZEPHYR CAN CONTROLLER CORE DRIVER (can_stm32.c)                     │
│   • Quản lý ngắt phần cứng CAN1_RX0_IRQHandler / CAN1_TX_IRQHandler                    │
│   • Tự động ánh xạ 28 Filter Banks vào phần cứng bxCAN                                 │
│   • Phục vụ hàng đợi k_msgq ngay trong ISR mà không tốn context switch                 │
└───────────────────────────────────────────▲────────────────────────────────────────────┘
                                            │ (Cấu hình tự động từ Devicetree)
┌───────────────────────────────────────────┴────────────────────────────────────────────┐
│                             PHẦN CỨNG BÁN DẪN (HARDWARE)                               │
│   • bxCAN Controller (APB1 54MHz) ──► Chân PB8 (RX), PB9 (TX)                          │
│   • GPIO Điều Khiển STB (Active LOW) ──► Kéo xuống 0V để đánh thức Transceiver         │
│   • CAN Transceiver SN65HVD230 / TJA1050 ──► Bus Vi Sai CAN_H / CAN_L                  │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 1.2. Giải Pháp Gắn Bộ Lọc Phần Cứng Trực Tiếp Vào Message Queue (`can_add_rx_filter_msgq`)

Đây là tính năng độc đáo và mạnh mẽ nhất của Zephyr RTOS dành cho kỹ sư Automotive:
* Khi nhận được một frame hợp lệ trên bus, khối phần cứng bxCAN lọc ID qua Filter Bank.
* Thay vì đánh thức CPU chạy một hàm callback C phức tạp, nhân Zephyr **copy trực tiếp cấu trúc `struct can_frame` vào hàng đợi `k_msgq` ngay trong ngữ cảnh ngắt cấp thấp**.
* Luồng xử lý dữ liệu (`can_rx_thread`) chỉ cần nằm ngủ chờ ở lệnh `k_msgq_get()`. Khi có frame đến, nó được bộ lập lịch đánh thức dậy xử lý một cách mượt mà, **triệt tiêu hoàn toàn nguy cơ nghẽn ngắt (ISR Starvation)**.

```text
CAN Bus Frame đến ──► bxCAN Hardware Filter Match ──► ISR Driver ──► k_msgq_put() ──► Đánh thức Worker Thread!
```

---

## 1.3. Căn Bệnh "Im Lặng Vĩnh Viễn": Cơ Chế Chân Standby (STB/EN) của Transceiver Ngoài

Bo mạch STM32F746G-Discovery không có chip CAN Transceiver onboard. Khi gắn module ngoài (`SN65HVD230` hoặc `TJA1050`):
* Chân **`STB` (Standby)** là công tắc tiết kiệm năng lượng:
  * `STB = HIGH (3.3V / 5V)`: Transceiver rơi vào chế độ **Standby / Sleep**. Mạch phát (Driver) bị ngắt điện, mạch thu (Receiver) chuyển sang chế độ phản hồi chậm. **Vi điều khiển gửi dữ liệu ra chân TX nhưng ngoài bus vật lý không có tín hiệu gì!**
  * `STB = LOW (0V - Nối GND)`: Transceiver hoạt động ở chế độ **Normal High-Speed Mode**. Tín hiệu logic từ PB9 được chuyển đổi thành điện áp vi sai $CAN\_H - CAN\_L$ với tốc độ lên tới $1\text{ Mbps}$.
* 👉 **Nguyên tắc kỹ thuật:** Bắt buộc phải dùng 1 chân GPIO của STM32 để kéo chân STB xuống mức `0V` ngay khi khởi động.

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

## 2.1. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS |
| :--- | :---: | :--- |
| **`CONFIG_CAN`** | `y` | Kích hoạt toàn bộ Subsystem điều khiển mạng CAN của Zephyr. |
| **`CONFIG_CAN_INIT_PRIORITY`** | `80` | Mức ưu tiên khởi tạo Driver lúc boot (sau GPIO và Clock). |
| **`CONFIG_CAN_MAX_FILTER`** | `14` | Cấp phát vùng nhớ quản lý tối đa 14 bộ lọc CAN cho ứng dụng. |
| **`CONFIG_CAN_STATS`** | `y` | Bật bộ đếm thống kê gói tin gửi/nhận và cờ lỗi phần cứng. |
| **`CONFIG_LOG`** | `y` | Bật Zephyr Logging để theo dõi gói tin CAN. |

---

## 2.2. Khai Báo Node CAN1 & Pinctrl trong File Overlay (`app.overlay`)

Tra cứu sơ đồ chân của bo STM32F746G-Discovery:
* Chân `PB8`: `CAN1_RX` (AF9)
* Chân `PB9`: `CAN1_TX` (AF9)
* Chân `PJ5` (hoặc chân bất kỳ trên header): Dùng làm chân điều khiển **`CAN_STB`** kéo xuống LOW.

```dts
/ {
    aliases {
        can-primary = &can1;
    };

    transceiver {
        compatible = "gpio-keys";
        can_stb: stb_pin {
            gpios = <&gpioj 5 GPIO_ACTIVE_LOW>;
            label = "CAN Transceiver Standby Control";
        };
    };
};

/* Kích hoạt khối CAN1 trên STM32F746 */
&can1 {
    status = "okay";
    bus-speed = <500000>;      /* Tốc độ 500 kbps */
    sample-point = <875>;      /* Điểm lấy mẫu 87.5% chuẩn CiA 301 */
    pinctrl-0 = <&can1_rx_pb8 &can1_tx_pb9>;
    pinctrl-names = "default";
};

/* Kích hoạt GPIOJ điều khiển chân STB */
&gpioj {
    status = "okay";
};
```

---

# 💻 BƯỚC 3: GÕ CODE ỨNG DỤNG & MỔ XẺ BUG HỆ THỐNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 8

```text
zephyr_gateway/
├── app.overlay        <-- Cấu hình node CAN1, Pinctrl PB8/PB9 và chân STB PJ5
├── prj.conf           <-- Bật CONFIG_CAN=y, tăng kích thước CAN_MAX_FILTER
└── src/
    ├── can_gateway.h  <-- Khai báo API CAN Gateway và cấu trúc Message Queue
    ├── can_gateway.c  <-- Đánh thức Transceiver, đăng ký MsgQ Filter, phát/thu luồng
    └── main.c         <-- Khởi chạy Gateway và điều phối luồng xử lý
```

---

### 📂 KHỐI 1: FILE HEADER GIAO TIẾP CAN [ `src/can_gateway.h` ]

#### TODO 1 [File: `src/can_gateway.h`]: Định Nghĩa Cấu Trúc Gói Tin & Queue
```c
#ifndef CAN_GATEWAY_H
#define CAN_GATEWAY_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

/* Độ dài hàng đợi tin nhắn CAN (Chứa tối đa 16 frames dự phòng burst) */
#define CAN_RX_QUEUE_SIZE 16

/* Khởi tạo hệ thống CAN Gateway */
int CAN_Gateway_Init(void);

/* Gửi một frame CAN chuẩn 11-bit ID */
int CAN_Gateway_Send(uint32_t std_id, const uint8_t *data, uint8_t dlc);

/* Hàng đợi tin nhắn ngoại vi dùng chung để luồng Worker đọc dữ liệu */
extern struct k_msgq g_can_rx_msgq;

#endif /* CAN_GATEWAY_H */
```

---

### 📂 KHỐI 2: FILE SOURCE DRIVER CAN ZEPHYR [ `src/can_gateway.c` ]

#### TODO 2 [File: `src/can_gateway.c`]: Đánh Thức Transceiver & Đăng Ký Filter MsgQ
```c
#include "can_gateway.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_gateway, LOG_LEVEL_INF);

/* 1. Lấy thiết bị CAN1 từ Devicetree */
static const struct device *const s_can_dev = DEVICE_DT_GET(DT_ALIAS(can_primary));

/* 2. Lấy chân GPIO điều khiển STB */
static const struct gpio_dt_spec s_can_stb = GPIO_DT_SPEC_GET(DT_NODELABEL(can_stb), gpios);

/* 3. Cấp phát bộ nhớ cho hàng đợi k_msgq */
CAN_MSGQ_DEFINE(g_can_rx_msgq, CAN_RX_QUEUE_SIZE);

/* Callback theo dõi sự thay đổi trạng thái mạng (Bus-Off / Error Warning) */
static void can_state_change_handler(const struct device *dev, enum can_state state,
                                     struct can_bus_err_cnt err_cnt, void *user_data)
{
    ARG_UNUSED(dev); ARG_UNUSED(user_data);

    if (state == CAN_STATE_BUS_OFF) {
        LOG_ERR("CẢNH BÁO NGUY HIỂM: CAN CONTROLLER ĐÃ RƠI VÀO BUS-OFF!");
        LOG_ERR("Chỉ số lỗi: TEC = %u, REC = %u", err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);
        /* Kích hoạt cờ an toàn cho toàn hệ thống */
    } else if (state == CAN_STATE_ERROR_PASSIVE) {
        LOG_WRN("Cảnh báo: CAN Controller rơi vào trạng thái Error Passive!");
    }
}

int CAN_Gateway_Init(void)
{
    /* BƯỚC 1: KIỂM TRA THIẾT BỊ PHẦN CỨNG CAN */
    if (!device_is_ready(s_can_dev)) {
        LOG_ERR("Lỗi: Thiết bị CAN Controller chưa sẵn sàng!");
        return -ENODEV;
    }

    /* BƯỚC 2: KÉO CHÂN TRANSCEIVER STB XUỐNG MỨC LOW ĐỂ ĐÁNH THỨC CHIP */
    if (device_is_ready(s_can_stb.port)) {
        gpio_pin_configure_dt(&s_can_stb, GPIO_OUTPUT_INACTIVE); /* INACTIVE = 0V vì ACTIVE_LOW */
        LOG_INF("Đã đánh thức CAN Transceiver thành công (STB = LOW)!");
    } else {
        LOG_WRN("Cảnh báo: Không tìm thấy chân điều khiển STB trong Devicetree!");
    }

    /* BƯỚC 3: ĐĂNG KÝ CALLBACK THEO DÕI TRẠNG THÁI LỖI BUS-OFF */
    can_set_state_change_callback(s_can_dev, can_state_change_handler, NULL);

    /* BƯỚC 4: THIẾT LẬP BỘ LỌC PHẦN CỨNG GẮN VÀO HÀNG ĐỢI K_MSGQ */
    /* Chấp nhận các ID trong dải táp-lô ô tô: 0x100 đến 0x107 */
    struct can_filter filter = {
        .id = 0x100,
        .mask = 0x7F8,               /* So khớp chính xác 11-bit ID */
        .flags = CAN_FILTER_DATA     /* Chỉ nhận Data Frame, loại bỏ Remote Frame */
    };

    int filter_id = can_add_rx_filter_msgq(s_can_dev, &g_can_rx_msgq, &filter);
    if (filter_id < 0) {
        LOG_ERR("Lỗi: Không thể đăng ký Filter MsgQ (Mã lỗi: %d)", filter_id);
        return filter_id;
    }
    LOG_INF("Đã kích hoạt Filter Bank #%d gắn trực tiếp vào k_msgq!", filter_id);

    /* BƯỚC 5: KHỞI ĐỘNG BỘ ĐIỀU KHIỂN CAN */
    int ret = can_start(s_can_dev);
    if (ret != 0) {
        LOG_ERR("Lỗi: Không thể start CAN controller (Mã lỗi: %d)", ret);
        return ret;
    }

    LOG_INF("Khởi tạo Zephyr CAN Subsystem 500kbps hoàn tất!");
    return 0;
}
```

#### TODO 3 [File: `src/can_gateway.c`]: Hàm Truyền Gói Tin An Toàn Kèm Timeout
```c
int CAN_Gateway_Send(uint32_t std_id, const uint8_t *data, uint8_t dlc)
{
    struct can_frame frame = {
        .id = std_id,
        .dlc = dlc,
        .flags = 0 /* Standard ID 11-bit */
    };

    if (dlc > 8) {
        frame.dlc = 8;
    }
    memcpy(frame.data, data, frame.dlc);

    /* Gửi frame với timeout 100ms. Tuyệt đối không dùng K_FOREVER */
    int ret = can_send(s_can_dev, &frame, K_MSEC(100), NULL, NULL);
    if (ret != 0) {
        LOG_ERR("Lỗi truyền gói tin CAN ID 0x%03X (Mã lỗi: %d)", std_id, ret);
        return ret;
    }

    return 0;
}
```

---

### 📂 KHỐI 3: LUỒNG THỰC THI CHÍNH [ `src/main.c` ]

#### TODO 4 [File: `src/main.c`]: Khởi Chạy Luồng Nhận CAN Bất Đồng Bộ
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "can_gateway.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

#define CAN_RX_THREAD_STACK_SIZE 2048
#define CAN_RX_THREAD_PRIORITY   4 /* Ưu tiên cao hơn các tác vụ thông thường */

void can_rx_worker_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    struct can_frame rx_frame;
    LOG_INF("Luồng CAN RX Worker đã sẵn sàng đón nhận dữ liệu từ MsgQ...");

    while (1) {
        /* Chờ vô tận ở k_msgq mà không tốn 1% CPU nào (Luồng tự Sleep khi hàng đợi rỗng) */
        int ret = k_msgq_get(&g_can_rx_msgq, &rx_frame, K_FOREVER);
        if (ret == 0) {
            LOG_INF("ĐÃ NHẬN GÓI TIN CAN: ID = 0x%03X, DLC = %u, Data = [%02X %02X %02X %02X]",
                    rx_frame.id, rx_frame.dlc,
                    rx_frame.data[0], rx_frame.data[1], rx_frame.data[2], rx_frame.data[3]);
            
            /* Dữ liệu sẽ được đẩy sang Ngày 9 (LVGL GUI) và Ngày 11 (DBC Signal Decode) */
        }
    }
}

K_THREAD_DEFINE(can_rx_tid, CAN_RX_THREAD_STACK_SIZE,
                can_rx_worker_thread, NULL, NULL, NULL,
                CAN_RX_THREAD_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Khởi động hệ thống Zephyr CAN Gateway...");

    if (CAN_Gateway_Init() < 0) {
        LOG_ERR("Dừng hệ thống do lỗi khởi tạo CAN!");
        return -1;
    }

    /* Bắn thử một gói tin chào mừng lên mạng xe hơi */
    uint8_t boot_msg[4] = {0x01, 0x02, 0x03, 0x04};
    CAN_Gateway_Send(0x100, boot_msg, sizeof(boot_msg));

    return 0;
}
```

---

## 3.2. Mổ xẻ 5 Bug Phần Cứng "Kinh Điển" trong Ngày 8

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG ZEPHYR CAN SUBSYSTEM                             │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN CỨNG             │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Bus im lặng    │ Chân STB của Transceiver ngoài thả nổi    │ Khai báo chân STB trong overlay │
│    hoàn toàn      │ làm chip chuyển sang chế độ Standby/Sleep.│ và kéo xuống LOW trước khi gửi. │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Đăng ký Filter │ Số filter vượt quá giới hạn mặc định      │ Tăng `CONFIG_CAN_MAX_FILTER=14` │
│    báo lỗi -ENOSPC│ `CONFIG_CAN_MAX_FILTER` trong nhân Zephyr.│ trong file `prj.conf`.          │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Mất gói tin CAN│ Kích thước `CAN_RX_QUEUE_SIZE` quá nhỏ    │ Tăng queue lên 16 hoặc 32 frames│
│    khi bus tải cao│ khiến `k_msgq` bị đầy tràn (Queue Full).  │ và tăng ưu tiên cho RX Thread.  │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Deadlock đóng  │ Dùng `can_send(..., K_FOREVER)` khi đường │ Luôn truyền kèm timeout xác định│
│    băng luồng TX  │ dây bus bị đứt, bộ đệm TX bị nghẽn cứng.  │ (ví dụ `K_MSEC(100)`).          │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Lệch Baudrate  │ Không định nghĩa `sample-point = <875>;`  │ Luôn ghi tường minh sample-point│
│    gây lỗi Bit CRC│ làm Zephyr chọn tỷ lệ chia nhịp không khớp│ trong node Devicetree overlay.  │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao trong Zephyr RTOS, việc sử dụng `can_add_rx_filter_msgq()` lại được ưu tiên tuyệt đối hơn `can_add_rx_filter_cb()`?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * `can_add_rx_filter_cb()` gọi hàm thực thi ngay trong **ngữ cảnh ngắt ISR phần cứng**. Nếu bên trong callback lập trình viên thực hiện các thao tác tốn thời gian (như copy chuỗi, giải mã tín hiệu phức tạp, tính CRC) hoặc vô tình gọi các hàm kernel có khả năng gây ngủ (blocking functions), toàn bộ hệ thống ngắt của MCU sẽ bị tê liệt.
  * `can_add_rx_filter_msgq()` phân tách ranh giới rõ ràng: Trình phục vụ ngắt chỉ làm nhiệm vụ duy nhất là đẩy gói tin vào hàng đợi `k_msgq` (thao tác O(1) cực nhanh). Toàn bộ logic giải mã được giao cho luồng Worker chạy ở ngữ cảnh Thread (Thread context), có thể bị ngắt chen ngang an toàn và không gây ảnh hưởng đến tính thời gian thực của hệ điều hành.

### ❓ Câu 2: Chân Standby (STB) của chip CAN Transceiver ngoài có tác dụng gì? Nếu quên cấu hình thì hiện tượng gì xảy ra?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Chân STB dùng để điều khiển chế độ tiêu thụ năng lượng của chip thu phát vật lý: Mức CAO ($3.3\text{V}/5\text{V}$) là chế độ Standby (tắt khối phát công suất), mức THẤP ($0\text{V}$) là chế độ Normal High-Speed.
  * Nếu kỹ sư quên cấu hình hoặc thả nổi chân STB (nhiều module có điện trở nội kéo lên VCC), Transceiver sẽ bị khóa cứng ở trạng thái Standby. Lúc này code trên STM32 nạp vào thanh ghi `TDR` vẫn báo gửi thành công nhưng trên hai dây vật lý `CAN_H` và `CAN_L` không hề xuất hiện chênh lệch điện áp vi sai $\implies$ **Bus bị câm hoàn toàn!**

### ❓ Câu 3: Làm thế nào để phát hiện và xử lý sự cố Bus-Off trong Zephyr CAN Subsystem?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * Đăng ký hàm giám sát bằng `can_set_state_change_callback(dev, callback, user_data)`.
  * Khi bộ đếm lỗi truyền vượt quá $TEC > 255$, phần cứng tự động chuyển sang `CAN_STATE_BUS_OFF` và gọi callback.
  * Tại đây, hệ thống có thể lựa chọn 2 phương án:
    1. Để phần cứng tự động phục hồi nếu trong Devicetree có cấu hình phục hồi tự động.
    2. Hoặc gọi hàm `can_recover(dev, timeout)` để chủ động yêu cầu nhân Zephyr khởi động lại khối CAN controller sau khi kiểm tra bus đã an toàn.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 8 của dự án, em nâng cấp hạ tầng truyền thông CAN Bus lên hệ thống **Zephyr CAN Subsystem**.  
> Em xử lý triệt để bài toán phần cứng bằng cách cấu hình node **`pinctrl`** và sử dụng một chân GPIO phụ để kéo chân **Standby (STB)** của transceiver ngoài `SN65HVD230` xuống mức LOW, đưa mạch vào chế độ truyền tốc độ cao $500\text{ kbps}$ chuẩn xác.  
> Để tối ưu hóa hiệu năng đa luồng, em ứng dụng cơ chế **`can_add_rx_filter_msgq`**, liên kết trực tiếp bộ lọc định danh phần cứng vào hàng đợi tin nhắn nhân **`k_msgq`**. Giải pháp này giải phóng hoàn toàn thời gian xử lý trong hàm ngắt ISR, cho phép luồng **CAN RX Worker** nhận diện và phân phối các frame táp-lô ô tô một cách an toàn mà không làm gián đoạn các luồng đồ họa hay giao tiếp khác."*
