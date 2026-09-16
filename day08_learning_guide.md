# 🏆 [NGÀY 8] CẨM NANG TOÀN DIỆN ZEPHYR CAN SUBSYSTEM: ASYNCHRONOUS MESSAGE QUEUE & TRANSCEIVER STANDBY CONTROL
## Lộ trình 4 Bước: Kiến Trúc Subsystem ➔ Thực Chiến Devicetree/Kconfig ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ hệ thống truyền thông mạng ô tô thời gian thực qua **Zephyr CAN Subsystem** trên STM32F746: Khai báo cấu hình phần cứng CAN Controller và Pinctrl trên Devicetree overlay (`500 kbps`, Sample Point `87.5%`), giải quyết triệt để lỗi "bus im lặng" bằng mạch điều khiển chân **Transceiver Standby (STB/EN)**, ứng dụng cơ chế gắn bộ lọc phần cứng thẳng vào hàng đợi tin nhắn nhân hệ điều hành **`can_add_rx_filter_msgq`** giúp giải phóng hoàn toàn ngữ cảnh ngắt ISR, và xử lý giám sát trạng thái mạng tự động (**Bus-Off State Change Callback**).  
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
| **3** | **Explicit Sample Point** | Khai báo rõ ràng `sample-point = <875>;` (tương đương `87.5%`) trong node Devicetree. Không để Zephyr tự tính toán bừa làm sai lệch vị trí lấy mẫu so với mạng ô tô chuẩn CiA 301. |
| **4** | **Filter Allocation Limit** | Kiểm tra `CONFIG_CAN_MAX_FILTER`. Giá trị mặc định thường là 5; nếu đăng ký nhiều hơn mà không tăng thông số này trong `prj.conf`, hàm `can_add_rx_filter` sẽ trả về mã lỗi `-ENOSPC` (No space left). |
| **5** | **Thread-Safe Queue Size** | Kích thước của `k_msgq` phải được tính toán đủ chứa burst frames: `sizeof(struct can_frame) * N`. Nếu queue đầy, gói tin mới sẽ bị drop. |
| **6** | **Asynchronous Non-blocking TX** | Khi truyền dữ liệu bằng `can_send()`, truyền kèm callback hoặc dùng `K_MSEC(timeout)`. Không truyền vô tận `K_FOREVER` trong luồng điều khiển chính để tránh bị khóa chết (Deadlock) khi bus bị ngắt. |
| **7** | **State Callback Monitoring** | Đăng ký `can_set_state_change_callback()` để đón nhận sự kiện chuyển trạng thái sang `CAN_STATE_BUS_OFF` và thực hiện kích hoạt chu kỳ phục hồi theo tiêu chuẩn an toàn. |

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG (SO SÁNH TRỰC DIỆN VỚI FREERTOS)

### 1.1. So Sánh Cơ Chế Lập Trình CAN: Bare-Metal / FreeRTOS vs Zephyr (Chi Tiết Ưu / Nhược Điểm)

| Khía cạnh Kỹ thuật | 1. Bare-Metal / FreeRTOS (HAL) | 2. Zephyr CAN Subsystem | Đánh Giá Kỹ Thuật, Ưu / Nhược Điểm & Trade-off Chuyên Sâu |
| :--- | :--- | :--- | :--- |
| **1. Tính toán Baudrate & Bit Timing** | Phải tự tính toán thủ công thanh ghi `CAN_BTR` (Prescaler, Tseg1, Tseg2, Sample Point $87.5\%$). Dễ sai lệch 1 tick gây lỗi CRC bus. | Khai báo 2 dòng trong `app.overlay` (`bus-speed = <500000>; sample-point = <875>;`). Zephyr tự tính toán tối ưu $100\%$ lúc compile-time. | • **Bare-Metal:** Hiểu sâu cấu trúc phần cứng từng Time Quanta ($t_q$), nhưng tốn hàng giờ tra RM0385 và dễ tính sai khi đổi tần số $f_{\text{PCLK1}}$.<br>• **Zephyr:** Thuật toán Bit Timing Solver tự động chọn tỉ lệ chia Prescaler và Phase Segments sao cho sai số Baudrate đạt $0\%$ và vị trí Sample Point gần sát nhất với yêu cầu ($87.5\%$). Giảm thiểu $100\%$ lỗi định thời do con người. |
| **2. Cấu hình Bộ lọc ID phần cứng (Filter Banks)** | Phải tự cấu hình 28 Filter Banks (thanh ghi `CAN_FMR`, `CAN_FA1R`, nạp Mask ID, quản lý ngân hàng bộ lọc kép với CAN1 Master). | Định nghĩa struct `can_filter` và gọi hàm API chuẩn hóa `can_add_rx_filter_msgq()`. | • **Bare-Metal:** Tận dụng triệt để cả 28 ngân hàng lọc phần cứng của bxCAN, $0\text{ byte}$ RAM runtime; nhưng code cấu hình mặt nạ (Mask/Filter) vô cùng phức tạp, dễ cấu hình nhầm làm lọt gói tin rác hoặc chặn nhầm gói tin hợp lệ.<br>• **Zephyr:** Trừu tượng hóa hoàn toàn. Lập trình viên chỉ cần truyền ID và Mask nhị phân (`CAN_STD_ID_MASK`), Zephyr tự động tìm và gán vào Filter Bank còn trống trong phần cứng. |
| **3. Cơ chế đón nhận dữ liệu (RX Handshake)** | Trong hàm ngắt ISR (`HAL_CAN_RxFifo0MsgPendingCallback`), phải tự gọi `HAL_CAN_GetRxMessage()` rồi tự gọi `xQueueSendFromISR()`. | **TỰ ĐỘNG HÓA HOÀN TOÀN:** Zephyr tự bốc gói tin từ phần cứng FIFO ném thẳng vào Message Queue `k_msgq` ngay trong ngắt. | • **Bare-Metal / FreeRTOS:** Dễ mắc lỗi gọi nhầm hàm Blocking trong ISR, hoặc xử lý quá lâu trong ISR làm mất ngắt của các ngoại vi khác.<br>• **Zephyr:** Cơ chế Zero-Lock Kernel binding. Driver ngầm thực hiện thao tác copy cực nhanh ($O(1)$) vào `k_msgq` rồi giải phóng ngắt ngay lập tức, chuyển toàn bộ logic xử lý cho Thread Worker mà không cần lập trình viên viết $1$ dòng code ISR nào. |
| **4. Giám sát lỗi & Phục hồi Bus-Off** | Phải tự viết vòng lặp kiểm tra thanh ghi `CAN_ESR`, tự bật cờ `ABOM` (Automatic Bus-Off Management). | Tích hợp sẵn cơ chế Callback tự động `can_set_state_change_callback()` và hàm phục hồi `can_recover()`. | • **Bare-Metal:** Bật `ABOM` tự động có thể gây nguy hiểm nếu bus bị chập điện vật lý liên tục (phần cứng cứ liên tục kết nối lại làm phá hoại bus).<br>• **Zephyr:** Cho phép kiểm soát an toàn theo chuẩn ISO 11898-1: ngắt kết nối an toàn khi Bus-Off, trì hoãn $100\text{ ms}$, kiểm tra điều kiện an toàn rồi mới chủ động gọi `can_recover()`. |

#### 📐 Công Thức Toán Học Bit Timing Chuẩn Ô Tô (CiA 301 / ISO 11898-1):

Định thời 1 bit CAN bao gồm 4 phân đoạn (Segments):
$$\text{Nominal Bit Time} = T_{\text{Sync\_Seg}} + T_{\text{Prop\_Seg}} + T_{\text{Phase\_Seg1}} + T_{\text{Phase\_Seg2}} = (1 + \text{TS1} + \text{TS2}) \times t_q$$

Trong đó độ dài của một Time Quanta ($t_q$) được chia từ bus APB1:
$$t_q = \frac{\text{BRP}}{f_{\text{PCLK1}}}$$

Vị trí điểm lấy mẫu (Sample Point) theo quy chuẩn ô tô ($80\% \sim 87.5\%$):
$$\text{Sample Point} = \frac{1 + \text{TS1}}{1 + \text{TS1} + \text{TS2}} \times 100\%$$

*Với $f_{\text{PCLK1}} = 54\text{ MHz}$, Baudrate $= 500\text{ kbps}$, chọn tổng $18\ t_q$:*
$$\text{BRP} = \frac{54 \times 10^6\text{ Hz}}{500 \times 10^3\text{ bps} \times 18} = 6 \implies \text{Prescaler} = 6$$
$$\text{TS1} = 14,\ \text{TS2} = 3 \implies \text{Sample Point} = \frac{1 + 14}{18} \times 100\% = 83.33\%$$

---

### 1.2. Tính Năng "Đắt Giá" Nhất: Gắn Bộ Lọc Thẳng Vào Message Queue (`can_add_rx_filter_msgq`)

Trong FreeRTOS truyền thống, để nhận một gói tin CAN và đưa lên Task xử lý:
1. Bạn phải tự viết hàm ngắt `HAL_CAN_RxFifo0MsgPendingCallback()`.
2. Trong hàm ngắt, bạn phải gọi `xQueueSendFromISR()` để ném dữ liệu sang Task.

**Trong Zephyr RTOS, bạn chỉ cần gọi ĐÚNG 1 DÒNG LỆNH lúc khởi động:**
```c
struct can_filter filter = {
    .id = 0x123,
    .mask = CAN_STD_ID_MASK,
    .flags = 0
};

// Gắn trực tiếp bộ lọc phần cứng vào Message Queue:
can_add_rx_filter_msgq(can_dev, &can_rx_msgq, &filter);
```

* **Luồng chạy tự động:**
  * Mỗi khi có gói tin CAN ID `0x123` bay tới, phần cứng bxCAN lọc khớp ID.
  * Driver ngầm của Zephyr tự động copy nguyên vẹn cấu trúc `struct can_frame` vào hàng đợi `can_rx_msgq`.
  * Luồng `can_rx_thread` ở tầng ứng dụng chỉ việc nằm ngủ `k_msgq_get(&can_rx_msgq, &frame, K_FOREVER)`. Khi có frame tới, Task tự động bật dậy xử lý, **triệt tiêu 100% việc phải tự viết code trong hàm ngắt ISR**!

```text
CAN Bus Frame đến ──► bxCAN Hardware Filter Match ──► Zephyr Driver ISR ──► k_msgq_put() ──► Đánh thức Worker Thread!
```

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Zephyr CAN API:**
> 1. **Mở header:** `zephyr/include/zephyr/drivers/can.h`.
> 2. Tìm kiếm hàm: `can_add_rx_filter_msgq()` và `can_add_rx_filter()`:
>    * Đọc tài liệu Doxygen của hàm: Hàm liên kết trực tiếp `k_msgq` vào bảng filter của driver. Khi ISR nhận ngắt FIFO, driver tự gọi `k_msgq_put(msgq, frame, K_NO_WAIT)` từ ngữ cảnh ISR mà không cần context switch sang thread xử lý.

---

### 1.3. Căn Bệnh "Im Lặng Vĩnh Viễn": Chân Standby (STB) Của CAN Transceiver Ngoài

Bo mạch STM32F746G-Discovery không tích hợp sẵn chip chuyển đổi mức tín hiệu CAN (Transceiver). Khi cắm module ngoài (`SN65HVD230` hoặc `TJA1050`):
* Trên module ngoài luôn có một chân tên là **`STB` (hoặc `Rs`)**:
  * **`STB = 3.3V` (hoặc thả nổi):** Chip transceiver rơi vào chế độ **Standby (Ngủ đông)** để tiết kiệm điện. Lúc này, vi điều khiển STM32 bắn dữ liệu ra chân TX ầm ầm nhưng chip transceiver bị khóa, **ngoài bus vật lý CAN không hề có tín hiệu gì**!
  * **`STB = 0V` (Nối đất GND):** Chip transceiver thức dậy, hoạt động ở chế độ **Normal High-Speed** bình thường. Tín hiệu logic từ PB9 được chuyển đổi thành điện áp vi sai CAN_H - CAN_L với tốc độ lên tới 1 Mbps.
* 👉 **Quy tắc thực chiến:** Bắt buộc phải dùng 1 chân GPIO của STM32 kéo chân STB này xuống mức `0V` (LOW) thì mạng CAN mới phát được dữ liệu!

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Datasheet CAN Transceiver (SN65HVD230 / TJA1050):**
> 1. **Mở file Datasheet của chip Transceiver ngoài:** Tìm mục *Operating Modes* hoặc *Pin Description*.
> 2. Đọc bảng chân chức năng: Chân `Rs` (hoặc `STB`):
>    * Mức logic HIGH (VCC): Kích hoạt chế độ Low-Current Standby Mode (Bộ phát TX bị vô hiệu hóa hoàn toàn).
>    * Mức logic LOW (GND): Kích hoạt chế độ High-Speed Operation Mode (Bộ phát và bộ thu hoạt động đầy đủ).

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

> 🎯 **NGUYÊN TẮC TRA CỨU CAN TRÊN ZEPHYR RTOS:**
> 1. **Tra cứu Kconfig:** Mở `zephyr/drivers/can/Kconfig` hoặc gõ `west build -t menuconfig` tìm kiếm `CONFIG_CAN`.
> 2. **Tra cứu Devicetree Bindings:** Mở `zephyr/dts/bindings/can/st,stm32-can.yaml` để xem cấu trúc khai báo node `&can1`.
> 3. **Tra cứu Pinctrl:** Mở `zephyr/dts/arm/st/f7/stm32f746nghx-pinctrl.dtsi` để lấy tên chuẩn của chân `PB8` và `PB9`.
> 4. **Tra cứu API:** Mở header `zephyr/include/zephyr/drivers/can.h` để xem nguyên mẫu hàm `can_send()` và `can_add_rx_filter()`.

---

## 2.1. Lộ trình Tra cứu Trực tiếp Driver CAN Zephyr (CAN Lookup Methodology)

### 📖 Kênh 1: Cách Tra Cứu Thuộc Tính Node `&can1` (Devicetree Bindings)
1. **Mở file Binding của bộ điều khiển bxCAN:**
   * Đường dẫn: **`zephyr/dts/bindings/can/st,stm32-can.yaml`**.
2. **Đọc mục `properties:` trong file:**
   * `bus-speed`: Đơn vị bps (ví dụ `500000`). Bắt buộc khai báo.
   * `sample-point`: Điểm lấy mẫu tính bằng phần nghìn (ví dụ `875` tương đương `87.5%` chuẩn CiA 301).
   * `pinctrl-0`: Trỏ đến danh sách các pinmux node của chân `CAN_RX` và `CAN_TX`.

### 📖 Kênh 2: Cách Tra Cứu Tên Nhãn Pinmux Chân CAN (`pinctrl.dtsi`)
Để biết Zephyr đặt tên cho chân PB8 (AF9) và PB9 (AF9) là gì:
1. **Mở file pinctrl của dòng STM32F746:**
   * Đường dẫn: **`zephyr/dts/arm/st/f7/stm32f746nghx-pinctrl.dtsi`**
2. **Nhấn `Ctrl + F` ➔ Gõ: `can1_`**:
   * Bạn sẽ thấy ST định nghĩa sẵn:
     * `can1_rx_pb8: can1_rx_pb8 { pinmux = <STM32_PINMUX('B', 8, AF9)>; };`
     * `can1_tx_pb9: can1_tx_pb9 { pinmux = <STM32_PINMUX('B', 9, AF9)>; };`
   * Do đó trong `app.overlay`, bạn chỉ cần điền: `pinctrl-0 = <&can1_rx_pb8 &can1_tx_pb9>;`.

### 📖 Kênh 3: Cách Tra Cứu API Gửi / Nhận CAN Của Zephyr
1. **Mở file header giao tiếp chuẩn:**
   * Đường dẫn: **`zephyr/include/zephyr/drivers/can.h`**.
2. **Đọc định nghĩa hàm và struct:**
   * `struct can_frame`: Chứa `id`, `dlc`, `data[8]`, `flags`.
   * `can_send(const struct device *dev, const struct can_frame *frame, k_timeout_t timeout, can_tx_callback_t cb, void *user_data)`: Hàm truyền frame.
   * `can_add_rx_filter(const struct device *dev, can_rx_callback_t cb, void *user_data, const struct can_filter *filter)`: Hàm cài đặt bộ lọc và đăng ký callback.

---

## 2.2. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS | Đánh Giá Kỹ Thuật, Ưu / Nhược Điểm & Chi Phí Tài Nguyên |
| :--- | :---: | :--- | :--- |
| **`CONFIG_CAN`** | `y` | Kích hoạt toàn bộ Subsystem điều khiển mạng CAN của Zephyr. | • **Ưu điểm:** Cung cấp API CAN chuẩn hóa, trừu tượng hóa toàn bộ thanh ghi bxCAN của STM32.<br>• **Chi phí:** Tăng khoảng $\approx 8.4\text{ KB}$ Flash ROM. |
| **`CONFIG_CAN_INIT_PRIORITY`** | `80` | Mức ưu tiên khởi tạo Driver lúc boot (sau GPIO và Clock). | • **Đánh giá Kỹ thuật:** Bắt buộc phải khởi tạo sau GPIO (Priority 40) và Clock RCC để các chân Pinmux PB8/PB9 đã sẵn sàng trước khi nạp lệnh bxCAN.<br>• **Lưu ý:** Nếu đặt mức ưu tiên nhỏ hơn GPIO, driver CAN sẽ bị lỗi lúc boot. |
| **`CONFIG_CAN_MAX_FILTER`** | `14` | Cấp phát vùng nhớ quản lý tối đa 14 bộ lọc CAN cho ứng dụng. | • **Ưu điểm:** Khớp chính xác với 14 bộ lọc của khối CAN1 độc lập (hoặc 28 bộ lọc chia sẻ với CAN2).<br>• **Trade-off:** Mỗi bộ lọc tiêu tốn $\approx 32\text{ bytes}$ RAM quản lý nội bộ trong driver. Nếu khai báo thiếu, hàm `can_add_rx_filter` trả về mã lỗi `-ENOSPC`. |
| **`CONFIG_CAN_STATS`** | `y` | Bật bộ đếm thống kê gói tin gửi/nhận và cờ lỗi phần cứng. | • **Ưu điểm:** Theo dõi số gói TX/RX thành công, số lần tràn FIFO (Overrun), số lần chuyển trạng thái Error Warning / Error Passive / Bus-Off.<br>• **Chi phí:** Tốn thêm $\approx 64\text{ bytes}$ RAM cho struct thống kê. Cực kỳ hữu ích trong giai đoạn chẩn đoán mạng ô tô. |
| **`CONFIG_LOG`** | `y` | Bật Zephyr Logging để theo dõi gói tin CAN. | • **Ưu điểm:** Cho phép in chi tiết từng frame ID, DLC, Payload khi debug.<br>• **Lưu ý:** Trong môi trường xe tải bus cao ($> 2000\text{ frames/s}$), bắt buộc phải tắt hoặc chuyển log sang chế độ Deferred để tránh làm nghẽn bus. |

---

## 2.3. Khai Báo Node CAN1 & Pinctrl trong File Overlay (`app.overlay`)

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

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 1:
1. **Tra cứu Header Subsystem CAN chuẩn của Zephyr:**
   - **Mở Zephyr SDK** ➔ `zephyr/include/zephyr/drivers/can.h`:
     - Chứa toàn bộ định nghĩa `struct can_frame`, `struct can_filter`, và các mã lỗi trả về (`-ENODEV`, `-ENOSPC`, `-EBUSY`).
   - `zephyr/include/zephyr/kernel.h`: Cung cấp cấu trúc hàng đợi tin nhắn `struct k_msgq`.
2. **Khai báo dung lượng hàng đợi an toàn:**
   - Đặt `CAN_RX_QUEUE_SIZE = 16` đảm bảo chứa đủ các burst frame khi tải bus tăng vọt lên 80-90%.

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

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 2:
1. **Lấy con trỏ thiết bị CAN qua Devicetree:**
   - Sử dụng `DEVICE_DT_GET(DT_ALIAS(can_primary))` để lấy con trỏ `const struct device *`.
2. **Đánh thức Transceiver vật lý (SN65HVD230 / TJA1050):**
   - Tra cứu Datasheet chip Transceiver: Chân STB phải ở mức LOW (0V) để đưa mạch vào High-Speed Mode.
   - Cấu hình qua Zephyr GPIO API: `gpio_pin_configure_dt(&s_can_stb, GPIO_OUTPUT_INACTIVE)`.
3. **Đăng ký giám sát lỗi Bus-Off:**
   - Tra cứu `can_set_state_change_callback()` trong `can.h`: Đón nhận sự kiện `CAN_STATE_BUS_OFF` và log lại chỉ số đếm lỗi `tx_err_cnt` (TEC), `rx_err_cnt` (REC).
4. **Cài đặt Filter Bank liên kết trực tiếp vào k_msgq:**
   - Tra cứu `can_add_rx_filter_msgq()`: Nhận vào struct `can_filter`, kết nối trực tiếp hardware filter vào `g_can_rx_msgq`.
   - Gọi `can_start(s_can_dev)` để bắt đầu hoạt động trên bus.

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

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 3:
1. **Cấu trúc can_frame và truyền tin:**
   - **Mở `can.h`**: `struct can_frame` chứa `id`, `dlc` (tối đa 8 bytes với CAN 2.0A/B), và mảng `data[8]`.
   - Hàm `can_send()`: Truyền timeout hữu hạn `K_MSEC(100)` để ngăn ngừa luồng bị treo vĩnh viễn khi dây bus bị đứt hoặc mất ACK từ các node khác.

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

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 4:
1. **Đón nhận tin nhắn từ k_msgq trong luồng Worker:**
   - **Mở Zephyr Docs** ➔ `Kernel Services -> Message Queues`:
     - Hàm `k_msgq_get(&g_can_rx_msgq, &rx_frame, K_FOREVER)` đưa luồng vào trạng thái ngủ khi không có gói tin, hoàn toàn không tiêu tốn chu kỳ CPU nào (Zero-CPU idle).
     - Khi phần cứng bxCAN nhận frame, ISR đẩy frame vào queue và kernel đánh thức luồng dậy tức thì.

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
  * Chân STB dùng để điều khiển chế độ tiêu thụ năng lượng của chip thu phát vật lý: Mức CAO (`3.3V` hoặc `5V`) là chế độ Standby (tắt khối phát công suất), mức THẤP (`0V`) là chế độ Normal High-Speed.
  * Nếu kỹ sư quên cấu hình hoặc thả nổi chân STB (nhiều module có điện trở nội kéo lên VCC), Transceiver sẽ bị khóa cứng ở trạng thái Standby. Lúc này code trên STM32 nạp vào thanh ghi `TDR` vẫn báo gửi thành công nhưng trên hai dây vật lý `CAN_H` và `CAN_L` không hề xuất hiện chênh lệch điện áp vi sai -> **Bus bị câm hoàn toàn!**

### ❓ Câu 3: Làm thế nào để phát hiện và xử lý sự cố Bus-Off trong Zephyr CAN Subsystem?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * Đăng ký hàm giám sát bằng `can_set_state_change_callback(dev, callback, user_data)`.
  * Khi bộ đếm lỗi truyền vượt quá `TEC > 255`, phần cứng tự động chuyển sang `CAN_STATE_BUS_OFF` và gọi callback.
  * Tại đây, hệ thống có thể lựa chọn 2 phương án:
    1. Để phần cứng tự động phục hồi nếu trong Devicetree có cấu hình phục hồi tự động.
    2. Hoặc gọi hàm `can_recover(dev, timeout)` để chủ động yêu cầu nhân Zephyr khởi động lại khối CAN controller sau khi kiểm tra bus đã an toàn.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 8 của dự án, em nâng cấp hạ tầng truyền thông CAN Bus lên hệ thống **Zephyr CAN Subsystem**.  
> Em xử lý triệt để bài toán phần cứng bằng cách cấu hình node **`pinctrl`** và sử dụng một chân GPIO phụ để kéo chân **Standby (STB)** của transceiver ngoài `SN65HVD230` xuống mức LOW, đưa mạch vào chế độ truyền tốc độ cao `500 kbps` chuẩn xác.  
> Để tối ưu hóa hiệu năng đa luồng, em ứng dụng cơ chế **`can_add_rx_filter_msgq`**, liên kết trực tiếp bộ lọc định danh phần cứng vào hàng đợi tin nhắn nhân **`k_msgq`**. Giải pháp này giải phóng hoàn toàn thời gian xử lý trong hàm ngắt ISR, cho phép luồng **CAN RX Worker** nhận diện và phân phối các frame táp-lô ô tô một cách an toàn mà không làm gián đoạn các luồng đồ họa hay giao tiếp khác."*
