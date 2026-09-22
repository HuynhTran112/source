# Automotive CAN Telematics Gateway — README HỌC PROJECT

> **Mục đích của file này:** dùng chính project để học Embedded/RTOS/CAN.  
> Không chỉ học “API nào được gọi”, mà phải hiểu **vì sao hệ thống được thiết kế như vậy, dữ liệu đi qua đâu, thread nào chịu trách nhiệm, lỗi xảy ra ở đâu và cách debug**.
>
> **Nguồn làm chuẩn để đọc project:** các file trong `src/` được cung cấp cùng project. Khi README cũ và source khác nhau, **ưu tiên source thực tế** và xem phần `Các điểm cần biết của source hiện tại`.

---

# 1. Trước hết: Project này đang làm gì?

## 1.1. Một câu mô tả

Đây là một **CAN telemetry gateway/diagnostic node** chạy trên STM32F746 với Zephyr RTOS.

Nó nhận CAN frame → kiểm tra dữ liệu → giải mã telemetry xe → kiểm tra các ngưỡng an toàn → lưu lỗi DTC → cho phép xem/điều khiển bằng Zephyr Shell.

Project còn có **CAN simulator/injector** để tự tạo frame phục vụ test.

---

## 1.2. Bài toán mà project giải quyết

Hãy tưởng tượng ECU trên xe đang gửi một frame:

```text
CAN ID = 0x123
DLC    = 8
Data   = [CRC, Counter, Speed, RPM_L, RPM_H, Temp, 0, 0]
```

Gateway phải làm các việc sau:

```text
                CAN BUS
                   │
                   ▼
            ┌─────────────┐
            │ CAN driver  │
            │ + HW filter │
            └──────┬──────┘
                   │ struct can_frame
                   ▼
             raw_can_msgq
                   │
                   ▼
        ┌──────────────────────┐
        │ CAN Worker Thread    │
        │                      │
        │ 1. Check CRC         │
        │ 2. Check Counter     │
        │ 3. Decode telemetry  │
        └─────────┬────────────┘
                  │ valid frame
                  ▼
       ┌──────────────────────┐
       │ g_current_telemetry  │
       │ + mutex              │
       └─────────┬────────────┘
                 │
        ┌────────┴─────────┐
        ▼                  ▼
 safety_monitor        Zephyr Shell
        │               vehicle status
        ▼                  dtc read
      DTC / LED            can sim
                          can auto
                          can inject
```

**Đây là luồng quan trọng nhất của cả project.**

Nếu hiểu được sơ đồ trên, bạn đã hiểu phần lớn kiến trúc.

---

# 2. Tư duy thiết kế hệ thống

## 2.1. Vì sao không xử lý mọi thứ ngay trong CAN interrupt?

Ý tưởng chính là:

```text
Hardware event
    ↓
Nhận frame thật nhanh
    ↓
Đưa frame vào queue
    ↓
Worker thread xử lý nặng hơn
```

Lý do:

- Nhận CAN cần phản hồi nhanh để tránh đầy FIFO.
- CRC/DBC/DTC là logic ứng dụng, không cần chạy trong context nhận phần cứng.
- Worker có thể block trên queue khi không có dữ liệu.
- Tách tầng driver và tầng xử lý giúp code dễ mở rộng.

**Trong source hiện tại không có `CAN_RX0_IRQHandler()` do user tự viết.** Project dùng Zephyr CAN API, và `can_add_rx_filter_msgq()` nối frame nhận được trực tiếp vào `k_msgq`.

Vì vậy phải phân biệt:

```text
Cách 1 — tự viết driver/ISR:
CAN IRQ → ISR → đọc register → queue

Cách 2 — project này:
CAN hardware → Zephyr CAN driver → HW filter → raw_can_msgq → worker
```

Project đang dùng **Cách 2**.

---

## 2.2. Vì sao cần queue?

Có hai tốc độ khác nhau:

```text
Producer: CAN driver
Consumer: CAN worker
```

Producer có thể nhận burst frame.
Consumer cần thời gian để kiểm tra CRC, rolling counter và decode.

Queue đóng vai trò **buffer giữa hai tốc độ này**:

```text
CAN driver
   │
   ├── frame 1 ──┐
   ├── frame 2 ──┤
   ├── frame 3 ──┤──► raw_can_msgq ───► worker
   └── ... ──────┘
```

Trong source:

```c
#define CAN_RX_QUEUE_SIZE 16
K_MSGQ_DEFINE(raw_can_msgq,
              sizeof(struct can_frame),
              CAN_RX_QUEUE_SIZE,
              4);
```

Do đó **queue thực tế là 16 frame**, không phải 32.

---

## 2.3. Vì sao cần mutex?

Có dữ liệu dùng chung:

```c
VehicleTelemetry_t g_current_telemetry;
```

`CAN Worker Thread` ghi dữ liệu.
`diag_shell.c` đọc dữ liệu.

Nếu đọc và ghi đồng thời mà không bảo vệ, có thể xảy ra race condition.

Project dùng:

```c
K_MUTEX_DEFINE(g_telemetry_mutex);
```

Writer:

```c
k_mutex_lock(&g_telemetry_mutex, K_FOREVER);
g_current_telemetry = local_tel;
k_mutex_unlock(&g_telemetry_mutex);
```

Reader cũng lock cùng mutex.

Tư duy cần nhớ:

```text
Queue  → truyền dữ liệu giữa producer/consumer
Mutex  → bảo vệ shared state
```

Đây là hai khái niệm khác nhau.

---

# 3. Đọc project theo thứ tự nào?

Không nên mở tất cả file rồi đọc từ trên xuống.

Đọc theo thứ tự:

```text
1. main.c
      ↓
2. can_gateway.h / can_gateway.c
      ↓
3. dbc_decoder.h / dbc_decoder.c
      ↓
4. safety_monitor.h / safety_monitor.c
      ↓
5. diag_shell.c
```

Lý do:

### Bước 1 — `main.c`
Hiểu thread nào tồn tại và chúng gọi subsystem nào.

### Bước 2 — `can_gateway.c`
Hiểu CAN được start như thế nào, filter nằm ở đâu, frame đi vào queue ra sao.

### Bước 3 — `dbc_decoder.c`
Hiểu frame 8 byte được kiểm tra và biến thành `speed/rpm/temp` như thế nào.

### Bước 4 — `safety_monitor.c`
Hiểu khi nào dữ liệu hợp lệ trở thành DTC.

### Bước 5 — `diag_shell.c`
Hiểu cách test toàn bộ system mà không cần xe thật.

---

# 4. Source map — mỗi file chịu trách nhiệm gì?

| File | Trách nhiệm |
|---|---|
| `main.c` | Tạo thread, giữ telemetry dùng chung, khởi tạo subsystem |
| `can_gateway.c` | CAN device, transceiver STB, CAN mode, start, filter, gửi frame, Bus-Off callback |
| `can_gateway.h` | API của CAN gateway + queue size |
| `dbc_decoder.c` | CRC/E2E + rolling counter + decode telemetry |
| `dbc_decoder.h` | `VehicleTelemetry_t`, `VEHICLE_DATA_ID`, API decoder |
| `safety_monitor.c` | DTC list, timeout CAN, overheat, overspeed |
| `safety_monitor.h` | DTC constants + safety monitor API |
| `diag_shell.c` | Shell command + CAN simulator + fault injection |

---

# 5. `main.c`: trung tâm của ứng dụng

## 5.1. Shared telemetry

```c
VehicleTelemetry_t g_current_telemetry = { 0 };
K_MUTEX_DEFINE(g_telemetry_mutex);
```

Có hai khái niệm:

```text
g_current_telemetry
    = state mới nhất của xe

raw_can_msgq
    = các frame CAN đang chờ được xử lý
```

Một cái là **state hiện tại**.
Một cái là **dòng message**.

---

## 5.2. CAN Worker Thread

Source tạo thread:

```c
K_THREAD_DEFINE(can_worker_tid,
                2048,
                can_worker_thread_entry,
                NULL, NULL, NULL,
                5, 0, 0);
```

Thông số thực tế:

```text
Stack    = 2048 bytes
Priority = 5
```

Luồng chạy:

```c
while (1) {
    k_msgq_get(&raw_can_msgq, &rx_frame, K_FOREVER);

    if (dbc_decode_vehicle_frame(...)) {
        lock telemetry
        update g_current_telemetry
        unlock

        safety_monitor_update(&local_tel);
    }
}
```

### Tại sao `K_FOREVER`?

Khi queue rỗng:

```text
worker ngủ / blocked
CPU không cần busy-loop
```

Khi có frame:

```text
queue có data
    ↓
worker được đánh thức
    ↓
process frame
```

So với:

```c
while (1) {
    if (queue_not_empty()) {
        ...
    }
}
```

thì `k_msgq_get(..., K_FOREVER)` là thiết kế RTOS tự nhiên hơn.

---

## 5.3. Safety Supervisor Thread

Source:

```c
K_THREAD_DEFINE(safety_tid,
                1024,
                safety_thread_entry,
                NULL, NULL, NULL,
                6, 0, 0);
```

Thông số:

```text
Stack    = 1024 bytes
Priority = 6
Period   = 200 ms
```

Nhiệm vụ của thread này không phải “tính DTC”.

Nó làm việc đơn giản hơn:

```text
mỗi 200 ms
    ↓
safety_monitor_is_fault_active()
    ↓
TRUE  → toggle LED
FALSE → tắt LED
```

Đây là ví dụ tốt để học **separation of responsibilities**:

```text
safety_monitor.c
    = quyết định có fault hay không

main.c / safety thread
    = biến trạng thái fault thành hành vi phần cứng LED
```

---

## 5.4. Main thread làm gì?

`main()` chỉ làm:

```text
log banner
   ↓
safety_monitor_init()
   ↓
can_gateway_init()
   ↓
return
```

Không có vòng `while(1)` trong `main()`.

Các thread được tạo bằng `K_THREAD_DEFINE()` chịu trách nhiệm runtime.

### Kiến thức Zephyr cần học ở đây

- thread
- priority
- stack size
- static thread creation
- blocking
- scheduler

---

# 6. `can_gateway.c`: lớp giao tiếp CAN

Đây là lớp “gần phần cứng” nhất trong project.

## 6.1. Queue

```c
K_MSGQ_DEFINE(raw_can_msgq,
              sizeof(struct can_frame),
              16,
              4);
```

Đọc theo hướng:

```text
K_MSGQ_DEFINE(name, msg_size, max_msgs, align)
```

Trong project:

```text
name     = raw_can_msgq
msg_size = sizeof(struct can_frame)
count    = 16
align    = 4
```

---

## 6.2. Lấy CAN device từ DeviceTree

```c
static const struct device *const can_dev =
    DEVICE_DT_GET(DT_ALIAS(can_primary));
```

Đây là pattern rất quan trọng trong Zephyr:

```text
DeviceTree
   ↓
alias / node
   ↓
DEVICE_DT_GET()
   ↓
struct device *
   ↓
Zephyr driver API
```

Code application không hard-code địa chỉ register CAN trong file C này.

---

## 6.3. STB của CAN transceiver

```c
GPIO_DT_SPEC_GET_OR(DT_NODELABEL(can_stb), gpios, {0});
```

STB là chân standby của transceiver.

Ý tưởng:

```text
MCU CAN controller
        │
        │ TX/RX logic
        ▼
CAN Transceiver
        │
        │ differential CANH/CANL
        ▼
     CAN BUS
```

Controller và transceiver là **hai tầng khác nhau**.

### Controller
Lo CAN frame, bit timing, mailbox, filter.

### Transceiver
Đổi logic TX/RX của MCU ↔ tín hiệu CANH/CANL vật lý.

Nếu transceiver đang ở Standby thì controller có thể “đang chạy” nhưng bus vật lý vẫn không hoạt động như mong đợi.

---

## 6.4. `device_is_ready()`

```c
if (!device_is_ready(can_dev)) {
    return -ENODEV;
}
```

Tư duy:

```text
có device handle
≠
device chắc chắn usable
```

`device_is_ready()` là bước guard trước khi dùng driver.

---

## 6.5. Callback Bus-Off

```c
can_set_state_change_callback(
    can_dev,
    can_state_change_handler,
    NULL);
```

Khi CAN state thay đổi, callback được gọi.

Project quan tâm đặc biệt tới:

```c
if (state == CAN_STATE_BUS_OFF)
```

Sau đó source thực hiện:

```text
stop
  ↓
wait 100 ms
  ↓
start
```

hoặc `can_recover()` khi cấu hình manual recovery tương ứng tồn tại.

### Điểm cần học

Bus-Off không đơn giản là “CAN bị mất kết nối”.
Nó là một trạng thái fault của CAN controller dùng **TEC/REC** để cô lập node khi lỗi quá nhiều.

---

# 7. CAN mode: Normal vs Loopback

Source:

```c
#if defined(USE_CAN_LOOPBACK_MODE)
    can_set_mode(can_dev, CAN_MODE_LOOPBACK);
#else
    can_set_mode(can_dev, CAN_MODE_NORMAL);
#endif
```

## Normal mode

```text
MCU CAN controller
      ↓
transceiver
      ↓
physical bus
      ↓
other CAN nodes
```

Phù hợp để test với transceiver + node khác.

## Loopback mode

Dùng để test logic CAN nội bộ khi không muốn phụ thuộc bus thật.

```text
TX path
  ↘
   internal loopback
  ↗
RX path
```

### Tại sao loopback hữu ích?

Ví dụ bạn chỉ có một board:

```text
board
 ├─ CAN controller
 └─ không có node khác ACK
```

Khi đó test physical CAN bình thường có thể gặp vấn đề ACK.

Loopback giúp kiểm tra:

- frame format
- driver flow
- queue
- decoder
- E2E
- DTC

mà không cần cả mạng CAN thật.

---

# 8. CAN filter: tại sao chỉ nhận ID `0x123`?

Source:

```c
const struct can_filter rx_filter = {
    .id = 0x123,
    .mask = 0x7FF,
    .flags = 0
};
```

Vì standard CAN ID có 11 bit:

```text
0x7FF = 11111111111b
```

Mask này nghĩa là **match toàn bộ 11 bit ID**.

Do đó:

```text
0x123 → ACCEPT
0x122 → REJECT
0x124 → REJECT
```

Sau đó:

```c
can_add_rx_filter_msgq(
    can_dev,
    &raw_can_msgq,
    &rx_filter);
```

Ý tưởng cực kỳ quan trọng:

```text
CAN driver
    ↓
Hardware / driver filter
    ↓
chỉ frame hợp lệ
    ↓
raw_can_msgq
```

Bạn không cần tự viết một worker thread để lọc:

```c
if (frame.id == 0x123)
```

vì filter đã được đăng ký ở tầng CAN driver.

---

# 9. `can_gateway_send_frame()`

```c
return can_send(can_dev,
                frame,
                K_MSEC(100),
                NULL,
                NULL);
```

API này được `diag_shell.c` dùng để làm simulator.

Điểm học được:

```text
application
   ↓
can_gateway_send_frame()
   ↓
can_send()
   ↓
Zephyr CAN driver
   ↓
CAN controller
   ↓
transceiver / bus
```

---

# 10. CAN frame của project

Project sử dụng frame 8-byte cho telemetry:

```text
CAN ID = 0x123
DLC    = 8
```

Payload:

| Byte | Ý nghĩa |
|---:|---|
| 0 | CRC |
| 1 | Rolling Counter (`0..15`) |
| 2 | Vehicle Speed raw |
| 3 | RPM raw low byte |
| 4 | RPM raw high byte |
| 5 | Coolant Temperature raw |
| 6 | Reserved / 0 |
| 7 | Reserved / 0 |

**Đây là format do chính project quy ước trong source.**

Project hiện không kèm một `.dbc` file thật trong source ZIP được cung cấp; decoding được hard-code trong `dbc_decoder.c`.

---

# 11. `dbc_decoder.c`: từ raw bytes thành telemetry

Đây là một module rất đáng học vì nó kết hợp:

```text
Pointer
+ array indexing
+ bit operation
+ integer arithmetic
+ validation
+ state (`last_counter`)
+ struct output
```

---

## 11.1. Guard clause

```c
if (dlc < 8 || out == NULL)
    return false;
```

Đây là **defensive programming**.

Không decode nếu:

```text
dữ liệu ngắn hơn yêu cầu
OR
output pointer NULL
```

Đây chính là pattern “validate input before work”.

---

# 12. E2E CRC: bước kiểm tra thứ nhất

Source lấy:

```c
uint8_t received_crc = data[0];
uint8_t expected_crc =
    compute_e2e_crc8(&data[1], 7, VEHICLE_DATA_ID);
```

Tức là:

```text
Byte 0
 = CRC đã nhận

Byte 1..7
 = payload được dùng để tính lại CRC
```

Nếu:

```text
received != expected
```

thì frame bị loại.

---

## 12.1. Data ID

```c
#define VEHICLE_DATA_ID 0x1A2B
```

Data ID được đưa vào CRC calculation để tăng khả năng phát hiện nhầm message/context.

Trong project, cả simulator và decoder cùng dùng `0x1A2B`.

---

## 12.2. CRC implementation hiện tại

Source dùng:

```c
crc = 0xFF;
...
crc = (crc & 0x80)
     ? ((crc << 1) ^ 0x2F)
     : (crc << 1);
...
return crc ^ 0xFF;
```

Cần học:

- XOR
- shift trái
- kiểm tra MSB
- vòng lặp từng bit
- `uint8_t` overflow / truncation
- init value
- final XOR

### Lưu ý quan trọng

README cũ gọi đây là `CRC-8 SAE J1850` và đồng thời nhắc đa thức `0x1D/0x2F`. **Source thật đang dùng `0x2F` trong thuật toán non-reflected shift-left.**

Đừng học thuộc tên chuẩn chỉ vì comment.
Khi phỏng vấn, nên nói rõ:

> “Implementation hiện tại của project dùng polynomial value `0x2F` trong hàm CRC. Nếu yêu cầu compliance chính thức với một profile/standard cụ thể, tôi sẽ đối chiếu lại algorithm bit-by-bit với specification/reference implementation.”

Đây là cách nói an toàn và kỹ thuật hơn.

---

# 13. Rolling Counter: bước kiểm tra thứ hai

Source:

```c
static uint8_t last_counter = 0xFF;
```

Frame mới:

```c
uint8_t current_counter = data[1] & 0x0F;
```

Nghĩa là chỉ lấy 4 bit thấp:

```text
data[1] = xxxx CCCC
                    ↑↑↑↑
               counter
```

Counter hợp lệ phải tăng:

```text
0 → 1 → 2 → ... → 14 → 15 → 0 → 1 ...
```

Code:

```c
uint8_t next_counter = (last_counter + 1) % 16;
```

---

## 13.1. Rolling counter phát hiện gì?

Ví dụ frame trước:

```text
counter = 5
```

Frame sau expected:

```text
6
```

Nếu nhận:

```text
8
```

thì có thể đã xảy ra:

```text
lost frame(s)
```

Nếu nhận lại:

```text
5
```

thì có thể là:

```text
duplicate / replay-like repetition
```

Trong project, counter là **một lớp phát hiện freshness/continuity**, còn CRC là **một lớp kiểm tra integrity**.

---

# 14. Decode Speed

Source:

```c
out->speed_kmh = (uint16_t)data[2];
```

Do đó:

```text
Raw = 100
→ speed = 100 km/h
```

Factor thực tế của project:

```text
1 km/h per bit
```

Không phải `Raw * 0.01` như một số mô tả trong README cũ.

---

# 15. Decode RPM

Source:

```c
uint16_t raw_rpm =
    (uint16_t)data[3] |
    ((uint16_t)data[4] << 8);

out->engine_rpm = (raw_rpm >> 2);
```

Điều này nghĩa là:

```text
data[3] = low byte
 data[4] = high byte
```

Tức raw RPM đang được ghép theo **little-endian byte order**.

Ví dụ:

```text
Byte 3 = 0xE0
Byte 4 = 0x2E
```

thì:

```text
raw_rpm = 0x2EE0
         = 12000 decimal
```

Sau đó:

```text
engine_rpm = 12000 >> 2
           = 3000 RPM
```

Tương đương:

```text
RPM = Raw × 0.25
```

nhưng code dùng shift để thực hiện integer arithmetic.

---

# 16. Decode coolant temperature

```c
out->coolant_temp = (int16_t)data[5] - 40;
```

Công thức:

```text
Temperature = Raw - 40 °C
```

Ví dụ:

```text
Raw = 128
→ 88 °C
```

---

# 17. Fixed-point / integer arithmetic ở project này

Project tránh dùng `float` trong decode.

Ví dụ RPM:

```c
raw_rpm >> 2
```

thay cho:

```c
raw_rpm * 0.25f
```

Đừng biến điều này thành “float luôn chậm”.

Cách giải thích chính xác hơn:

> Với dữ liệu có scale cố định, integer arithmetic có thể đủ dùng và giúp code đơn giản, dễ kiểm soát và tránh floating-point khi không cần độ chính xác phân số thực sự.

---

# 18. Thứ tự xử lý trong `dbc_decode_vehicle_frame()`

Đây là một câu rất dễ gặp trong phỏng vấn:

```text
Raw frame
  │
  ▼
Check DLC / pointer
  │
  ▼
Check CRC
  │
  ├── FAIL → reject
  │
  ▼
Check rolling counter
  │
  ├── FAIL → reject
  │
  ▼
Decode Speed / RPM / Temperature
  │
  ▼
Set is_e2e_valid = true
  │
  ▼
return true
```

**CRC/counter được kiểm tra trước decode** vì không nên sử dụng telemetry từ một frame chưa được xác thực.

---

# 19. `safety_monitor.c`: sau khi frame hợp lệ thì sao?

`CAN Worker` gọi:

```c
safety_monitor_update(&local_tel);
```

Module này chịu trách nhiệm biến telemetry thành trạng thái an toàn.

---

## 19.1. Danh sách DTC

Project có tối đa:

```c
static uint16_t s_active_dtcs[8];
```

Nghĩa là tối đa 8 DTC đang active.

`add_dtc_internal()` còn kiểm tra duplicate trước khi thêm.

---

## 19.2. Overheat

```c
if (tel->coolant_temp > 105) {
    add_dtc_internal(DTC_P0115);
}
```

Ngưỡng project:

```text
Temp > 105 °C
→ DTC_P0115
```

---

## 19.3. Overspeed

```c
if (tel->engine_rpm > 6500) {
    add_dtc_internal(DTC_P0219);
}
```

Ngưỡng project:

```text
RPM > 6500
→ DTC_P0219
```

---

## 19.4. CAN communication timeout

Timestamp hợp lệ được lưu trong:

```c
s_last_msg_time
```

Khi `safety_monitor_get_active_dtc()` được gọi, source kiểm tra:

```c
if ((uint32_t)(k_uptime_get_32() - s_last_msg_time) > 1000)
```

Nếu quá 1000 ms:

```text
→ DTC_U0100
```

Điểm quan trọng:

**timeout hiện tại được đánh giá khi gọi `safety_monitor_get_active_dtc()`**, không phải một timer callback độc lập.

Điều này dẫn tới một behavior cần nhớ:

```text
CAN auto OFF
   ↓
không có frame hợp lệ
   ↓
1 second trôi qua
   ↓
DTC_U0100 sẽ chỉ được add khi có code gọi get_active_dtc()
```

Ví dụ rõ nhất là lệnh:

```text
dtc read
```

---

# 20. DTC có tự xóa không?

Không.

`add_dtc_internal()` chỉ thêm.

DTC được clear khi gọi:

```c
safety_monitor_clear_dtc();
```

và hàm này reset:

```text
s_dtc_count = 0
s_last_msg_time = current time
```

Vì vậy behavior là:

```text
fault xuất hiện
   ↓
DTC active
   ↓
frame tốt vẫn tiếp tục
   ↓
DTC vẫn active
   ↓
chỉ xóa bằng `dtc clear`
```

Đây là kiểu **latched diagnostic fault** trong phạm vi project này.

---

# 21. `diag_shell.c`: công cụ test của project

Đây là file rất quan trọng để học cách test embedded system.

Nó không chỉ là “CLI”.
Nó là **test/control plane** cho toàn bộ project.

Các nhóm lệnh:

```text
vehicle
 └── status

dtc
 ├── read
 └── clear

can
 ├── sim <speed>
 ├── auto <on|off>
 └── inject <overheat|overspeed|corrupt>
```

---

# 22. `vehicle status`

Lệnh đọc:

```text
vehicle status
```

Nó lock `g_telemetry_mutex`, sau đó in:

- speed
- RPM
- coolant temperature
- rolling counter
- E2E valid flag

### Điểm cần hiểu

`vehicle status` hiển thị **telemetry hợp lệ gần nhất đã được commit vào `g_current_telemetry`**.

Một frame CRC sai không ghi `local_tel` vào global state.

Do đó, sau `can inject corrupt`, không nên kỳ vọng `vehicle status` tự chuyển thành “CORRUPT” vì source hiện tại không commit trạng thái fail đó vào `g_current_telemetry`.

---

# 23. `dtc read`

```text
dtc read
```

Luồng:

```text
Shell
  ↓
safety_monitor_get_active_dtc()
  ↓
check timeout
  ↓
copy DTC array ra local buffer
  ↓
print
```

Đây là nơi timeout `U0100` thực sự được đánh giá trong source hiện tại.

---

# 24. `dtc clear`

```text
dtc clear
```

Reset toàn bộ active DTC.

Đây là ví dụ đơn giản để học:

```text
command
  ↓
application API
  ↓
protected shared state
```

---

# 25. CAN simulator

Project có thể tự tạo frame:

```c
send_sim_frame(speed, rpm, temp, corrupt_crc);
```

Frame simulator luôn:

```text
ID  = 0x123
DLC = 8
```

và tự xây payload theo đúng format decoder mong đợi.

Điểm rất hay để học:

```text
decoder format
      ↑
      │ must match
      │
simulator format
```

Nếu hai phía dùng format khác nhau thì test sẽ fail dù CAN driver vẫn chạy bình thường.

---

# 26. `can auto on/off`

Simulator tự động chạy mỗi:

```text
200 ms
```

tức:

```text
5 Hz
```

Mỗi lần tốc độ thay đổi:

```text
speed += 2 hoặc -= 2
```

RPM:

```text
RPM = 1200 + speed × 25
```

Temperature:

```text
88 °C
```

CRC:

```text
valid
```

### Luồng test

```text
can auto on
      ↓
sim thread phát frame mỗi 200 ms
      ↓
CAN send
      ↓
CAN receive path
      ↓
queue
      ↓
worker
      ↓
CRC + counter
      ↓
decode
      ↓
telemetry + safety
```

---

# 27. Fault injection

## 27.1. Overheat

```text
can inject overheat
```

Source gửi:

```text
Speed = 90 km/h
RPM   = 3000
Temp  = 115 °C
CRC   = valid
```

Expected:

```text
CRC        → PASS
Counter    → PASS
Decode     → PASS
Safety     → Temp > 105
             ↓
          DTC_P0115
```

---

## 27.2. Overspeed

```text
can inject overspeed
```

Source gửi:

```text
Speed = 140 km/h
RPM   = 6800
Temp  = 88 °C
CRC   = valid
```

Expected:

```text
CRC        → PASS
Counter    → PASS
Decode     → PASS
Safety     → RPM > 6500
             ↓
          DTC_P0219
```

---

## 27.3. Corrupt CRC

```text
can inject corrupt
```

Source cố ý sửa CRC:

```c
crc ^ 0xAA
```

Expected:

```text
received CRC != calculated CRC
          ↓
        reject
```

Quan trọng:

```text
CRC fail
→ không gọi safety_monitor_update()
→ không cập nhật telemetry global
```

Đây là ví dụ trực tiếp của nguyên tắc:

> **Không sử dụng dữ liệu trước khi dữ liệu qua validation.**

---

# 28. Bức tranh toàn bộ runtime

Đây là sơ đồ nên tự vẽ lại được khi phỏng vấn:

```text
                    ┌────────────────────┐
                    │     CAN BUS        │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │ Transceiver        │
                    └─────────┬──────────┘
                              │ RX
                              ▼
                    ┌────────────────────┐
                    │ STM32 CAN controller│
                    │ + HW filter        │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │ Zephyr CAN driver  │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │ raw_can_msgq       │
                    │ 16 frames         │
                    └─────────┬──────────┘
                              │
                        K_FOREVER wait
                              │
                              ▼
                    ┌────────────────────┐
                    │ CAN Worker Thread  │
                    │ priority 5         │
                    └─────────┬──────────┘
                              │
                              ▼
                    ┌────────────────────┐
                    │ DBC Decoder        │
                    │ CRC + Counter      │
                    └─────────┬──────────┘
                              │ valid
                 ┌────────────┴────────────┐
                 ▼                         ▼
        ┌──────────────────┐      ┌──────────────────┐
        │ Telemetry state  │      │ Safety monitor   │
        │ + mutex          │      │ + DTC list       │
        └────────┬─────────┘      └────────┬─────────┘
                 │                         │
                 ▼                         ▼
        ┌──────────────────┐      ┌──────────────────┐
        │ vehicle status   │      │ LED supervisor   │
        └──────────────────┘      └──────────────────┘
```

---

# 29. Boot flow: từ reset tới application

README này tập trung vào source application, nên cần tách hai tầng:

## Tầng firmware/CPU startup

Khái niệm cần biết:

```text
Reset
 ↓
vector table
 ↓
startup code
 ↓
.data / .bss / stack
 ↓
Zephyr kernel boot
 ↓
main + static threads
```

## Tầng project application

```text
main()
 ↓
safety_monitor_init()
 ↓
can_gateway_init()
 ↓
CAN runtime
```

`main.c` **không phải nơi duy nhất chứa “khởi động system”**.
Zephyr đã khởi động kernel và static threads trước khi application logic thực thi.

---

# 30. Zephyr concepts cần học trực tiếp từ project

## 30.1. `K_THREAD_DEFINE`

Học:

- static thread creation
- stack allocation
- priority
- entry function
- scheduler

Source:

```c
K_THREAD_DEFINE(...);
```

---

## 30.2. `k_msgq`

Học:

- producer/consumer
- blocking
- bounded queue
- data copy semantics
- queue full behavior

Source:

```c
K_MSGQ_DEFINE(...);
k_msgq_get(..., K_FOREVER);
```

---

## 30.3. `k_mutex`

Học:

- mutual exclusion
- critical section
- shared state
- priority inheritance concept

Source:

```c
K_MUTEX_DEFINE(...);
k_mutex_lock(...);
k_mutex_unlock(...);
```

---

## 30.4. DeviceTree

Học chuỗi:

```text
DT node / alias
      ↓
DEVICE_DT_GET()
GPIO_DT_SPEC_GET_OR()
      ↓
struct device / gpio_dt_spec
      ↓
Zephyr driver API
```

Trong source ZIP hiện tại chỉ thấy phần C dùng DeviceTree macro; file `app.overlay`, `prj.conf`, board DTS và build files không nằm trong ZIP được cung cấp. Vì vậy không nên coi các giá trị overlay trong README cũ là source đã được xác minh.

---

## 30.5. Zephyr Shell

Học:

```text
SHELL_CMD_REGISTER()
SHELL_STATIC_SUBCMD_SET_CREATE()
```

và callback:

```c
static int cmd_xxx(const struct shell *sh,
                   size_t argc,
                   char **argv)
```

Đây là pattern CLI rất phổ biến trong embedded.

---

# 31. CAN fundamentals cần học để hiểu source

Không cần học toàn bộ CAN specification ngay lập tức.

Học theo thứ tự:

```text
1. CAN node
2. CAN controller
3. CAN transceiver
4. CANH/CANL
5. Standard ID 11-bit
6. Arbitration
7. DLC + Data
8. CRC / ACK / EOF
9. RX FIFO
10. Hardware filter
11. Error counters
12. Error Active / Passive / Bus-Off
```

---

# 32. CAN controller vs CAN transceiver

Đây là câu phỏng vấn rất quan trọng.

```text
Application
   ↓
CAN driver
   ↓
CAN controller  ← logic frame / timing / error state
   ↓
CAN transceiver ← electrical conversion
   ↓
CANH/CANL
```

Nếu thiếu transceiver:

```text
CAN controller có thể vẫn được cấu hình
nhưng physical CAN bus không hoạt động đúng.
```

Đây là lý do các lỗi “driver OK nhưng bus không chạy” thường phải kiểm tra phần cứng.

---

# 33. CAN arbitration — tại sao ID nhỏ có priority cao?

CAN dùng dominant/recessive logic.

Nếu một node gửi:

```text
0 = dominant
1 = recessive
```

và phát hiện bus khác với bit mình mong đợi, node có thể mất arbitration.

Ở vùng arbitration:

```text
ID nhỏ hơn
→ có bit dominant ở vị trí sớm hơn
→ giành bus trước
```

Ví dụ ý tưởng:

```text
ID A: 001...
ID B: 010...
     ^
```

Node thấy `0` dominant sẽ thắng node đang gửi `1` tại bit đó.

Không có “priority field” riêng.
**ID chính là cơ sở arbitration.**

---

# 34. ACK: tại sao một CAN transmitter cần node khác?

ACK slot là cơ chế xác nhận rằng frame đã được ít nhất một receiver nhận đúng.

Tư duy:

```text
Transmitter phát frame
      ↓
Receiver nhận + kiểm tra
      ↓
Receiver gửi ACK dominant
      ↓
Transmitter thấy ACK
```

Nếu không có node nhận/ACK trên physical bus:

```text
Transmitter
   ↓
không thấy ACK
   ↓
ACK error
```

Đây là một nguyên nhân quan trọng khi test một board đơn lẻ ở Normal mode.

Loopback là một cách test logic nội bộ phù hợp hơn cho single-board test, tùy driver/hardware mode.

---

# 35. CAN fault confinement: TEC / REC

Hai khái niệm cần biết:

```text
TEC = Transmit Error Counter
REC = Receive Error Counter
```

Các state:

```text
Error Active
    ↓ lỗi tăng
Error Passive
    ↓ lỗi tiếp tục
Bus-Off
```

Mục tiêu của cơ chế là:

> Một node bị lỗi không được tiếp tục gây nhiễu mạng vô thời hạn.

Project theo dõi Bus-Off qua callback:

```c
can_state_change_handler(...)
```

---

# 36. DBC là gì trong project này?

DBC có thể hiểu đơn giản là **quy tắc giải mã CAN signal**.

Một signal thường có:

```text
CAN ID
Start bit
Length
Byte order
Signed/Unsigned
Factor
Offset
Min/Max
Unit
```

Trong project này, các quy tắc đó đang được biểu diễn trực tiếp bằng code.

Ví dụ:

```text
Byte 2
Speed
Factor = 1
Offset = 0
```

và:

```text
Byte 3-4
RPM raw
Factor = 0.25
Offset = 0
```

Do không có `.dbc` file trong ZIP, nên khi học hãy coi `dbc_decoder.c` là **executable representation** của message layout hiện tại.

---

# 37. Byte order: bài học từ RPM

Source:

```c
raw_rpm = data[3] | (data[4] << 8);
```

Mô hình:

```text
data[3] = low byte
 data[4] = high byte
```

Đây là little-endian byte composition.

Không được suy đoán endianness chỉ từ “ô tô dùng DBC”.
Phải nhìn đúng signal definition / code.

---

# 38. Bitwise operations cần học từ project

Project dùng nhiều thao tác bit rất điển hình.

## Mask

```c
data[1] & 0x0F
```

Giữ lại 4 bit thấp.

## Shift

```c
data[4] << 8
```

Đưa byte cao lên vị trí đúng.

## OR

```c
data[3] | (data[4] << 8)
```

Ghép hai byte thành `uint16_t`.

## XOR

CRC sử dụng:

```c
crc ^= byte;
```

Đây là một trong những nhóm kỹ năng Embedded C cốt lõi.

---

# 39. Pointer trong project

Ví dụ:

```c
bool dbc_decode_vehicle_frame(
    const uint8_t *data,
    uint8_t dlc,
    VehicleTelemetry_t *out);
```

Ở đây:

```text
const uint8_t *data
    = con trỏ tới byte input, không được sửa data

VehicleTelemetry_t *out
    = con trỏ tới vùng nhớ output mà hàm được phép ghi
```

Đây là pattern:

```text
const input pointer
        +
mutable output pointer
```

---

# 40. `static` trong decoder

```c
static uint8_t last_counter = 0xFF;
```

`static` local có:

```text
storage lifetime = toàn bộ runtime
scope            = chỉ bên trong function
```

Vì vậy function nhớ được counter của frame trước.

Đây chính là một ví dụ đẹp để học sự khác nhau giữa:

```text
local automatic variable
vs
static local variable
```

---

# 41. `extern` trong project

`diag_shell.c` có:

```c
extern VehicleTelemetry_t g_current_telemetry;
extern struct k_mutex g_telemetry_mutex;
```

Trong `main.c` object được định nghĩa thật:

```c
VehicleTelemetry_t g_current_telemetry = { 0 };
K_MUTEX_DEFINE(g_telemetry_mutex);
```

Tức:

```text
main.c
  = definition

diag_shell.c
  = declaration/reference bằng extern
```

Đây là bài học linker/global symbol cơ bản.

---

# 42. Testing strategy của project

Thay vì chỉ “flash rồi xem có chạy không”, hãy test theo từng lớp.

## Test 1 — Boot

Expected:

```text
Zephyr boot
CAN init OK
Shell available
```

## Test 2 — Normal telemetry

```text
can auto on
vehicle status
```

Mục tiêu:

```text
frame → queue → decoder → telemetry
```

## Test 3 — Overheat

```text
can inject overheat
dtc read
```

Mục tiêu:

```text
valid frame → safety threshold → DTC
```

## Test 4 — Overspeed

```text
can inject overspeed
dtc read
```

## Test 5 — CRC error

```text
can inject corrupt
```

Mục tiêu:

```text
CRC fail → frame reject
```

## Test 6 — Communication timeout

```text
can auto off
... chờ > 1 s
dtc read
```

Mục tiêu:

```text
không có valid update → timeout → DTC_U0100
```

Nhớ rằng source hiện tại đánh giá timeout khi `dtc read` gọi API đọc DTC.

---

# 43. Debug theo tầng — cách tư duy khi project không chạy

Đừng ngay lập tức sửa decoder.

Đi từ dưới lên:

```text
1. Power
2. Transceiver STB
3. CAN controller ready
4. CAN mode
5. Physical bus
6. ACK
7. CAN RX filter
8. Queue
9. Decoder
10. Safety monitor
11. Shell
```

---

## 43.1. CAN không nhận frame

Checklist:

```text
device_is_ready() ?
STB LOW ?
transceiver có nguồn ?
CANH/CANL đúng ?
termination đúng ?
bitrate giống nhau ?
ID có đúng 0x123 ?
mask có đúng 0x7FF ?
queue có nhận được frame ?
```

---

## 43.2. Frame vào queue nhưng decoder fail

Kiểm tra:

```text
DLC >= 8 ?
CRC algorithm ?
Data ID = 0x1A2B ?
counter có liên tục ?
RPM byte order ?
temperature offset ?
```

---

## 43.3. Decoder PASS nhưng DTC không xuất hiện

Kiểm tra:

```text
safety_monitor_update() có được gọi ?
threshold có vượt chưa ?
DTC đã tồn tại chưa ?
```

Timeout thì kiểm tra thêm:

```text
dtc read đã được gọi chưa ?
```

---

# 44. Một lỗi kiến trúc quan trọng cần tự nhận ra

Project hiện dùng:

```c
safety_monitor_is_fault_active()
```

trong safety thread để điều khiển LED.

Nhưng timeout `DTC_U0100` lại được tạo trong:

```c
safety_monitor_get_active_dtc()
```

Vì vậy nếu:

```text
can auto off
```

mà không có ai gọi `dtc read`, thì chỉ riêng safety thread có thể **chưa thấy U0100 được tạo**.

Đây là ví dụ cực tốt để học architectural review:

> “Code có thể compile và chạy, nhưng behavior giữa các subsystem vẫn có thể không nhất quán.”

---

# 45. Một điểm cần nhận ra ở CRC error path

Khi decoder phát hiện CRC sai:

```c
out->is_e2e_valid = false;
return false;
```

nhưng `main.c` chỉ update global telemetry khi decoder trả về `true`.

Do đó:

```text
bad frame
  ↓
decode returns false
  ↓
g_current_telemetry KHÔNG được overwrite
```

Kết quả:

```text
vehicle status
```

sẽ tiếp tục hiển thị **last valid telemetry**.

Đây không nhất thiết là “bug”; nó là một behavior cần hiểu.

Nếu muốn hiển thị trạng thái lỗi mới nhất, kiến trúc cần thêm một state riêng cho communication/E2E health.

---

# 46. Một điểm cần nhận ra ở simulator

`diag_shell.c` tạo:

```c
K_THREAD_DEFINE(sim_tid, 1024, ..., 7, ...);
```

Thông số:

```text
Stack    = 1024
Priority = 7
Period   = 200 ms
```

Nó gọi:

```c
can_gateway_send_frame()
```

Do đó simulator **không bypass CAN stack**.
Nó vẫn đi qua `can_send()`.

Đây là điều tốt để học test integration.

---

# 47. Tư duy phân tầng của project

Có thể chia project thành 4 tầng:

```text
┌──────────────────────────────┐
│ 4. User / Diagnostic         │
│    Shell                     │
├──────────────────────────────┤
│ 3. Application Logic         │
│    DBC / E2E / Safety / DTC  │
├──────────────────────────────┤
│ 2. RTOS / Driver Abstraction │
│    Thread / MsgQ / Mutex     │
│    Zephyr CAN / GPIO         │
├──────────────────────────────┤
│ 1. Hardware                  │
│    STM32 CAN + transceiver   │
│    CANH/CANL                 │
└──────────────────────────────┘
```

Khi debug, hãy xác định lỗi nằm ở tầng nào trước.

---

# 48. Tại sao thiết kế này dễ mở rộng?

Ví dụ muốn thêm signal `battery_voltage`.

Không cần sửa CAN driver.

Chỉ cần mở rộng:

```text
frame format
   ↓
dbc_decoder
   ↓
VehicleTelemetry_t
   ↓
safety / shell nếu cần
```

Nếu thêm DTC mới:

```text
safety_monitor.h
    ↓
safety_monitor.c
```

Nếu thêm command:

```text
diag_shell.c
```

Tầng CAN vẫn giữ nguyên.

Đây là lợi ích của **separation of concerns**.

---

# 49. Nhưng project vẫn còn những chỗ có thể cải tiến

Đây là phần nên học để đi từ “biết code” sang “biết review firmware”.

## 49.1. Timeout nên được đánh giá độc lập

Hiện tại timeout gắn với `get_active_dtc()`.

Thiết kế mạnh hơn có thể là:

```text
periodic safety thread
       ↓
check communication timeout
       ↓
update DTC state
       ↓
LED / logger / shell
```

---

## 49.2. E2E status nên là state riêng

Hiện tại CRC fail làm decoder return false.

Có thể thiết kế thêm:

```text
last_valid_telemetry
last_rx_time
crc_error_count
counter_error_count
communication_state
```

để diagnosis tốt hơn.

---

## 49.3. Error recovery nên được thiết kế không blocking callback

Source hiện có:

```c
can_stop(can_dev);
k_msleep(100);
can_start(can_dev);
```

ngay trong state-change handler nhánh tương ứng.

Khi port sang driver/hardware khác, cần kiểm tra callback context có cho phép blocking/sleep hay không.

Một kiến trúc khác là:

```text
CAN state callback
      ↓
set event/flag
      ↓
safety/recovery thread
      ↓
perform blocking recovery
```

Đây là cách tách interrupt/event handling khỏi công việc có thể block.

---

# 50. Các kiến thức Embedded C nên học từ project này

## C language

- pointer
- `const` pointer
- `static`
- `extern`
- struct
- enum
- fixed-width integer types
- boolean
- bitwise `& | ^ << >>`
- array
- function pointer / callback
- conditional compilation `#if defined(...)`

## Memory / linker

- global variable
- static storage
- stack của thread
- address vs pointer
- definition vs declaration
- `extern`

## RTOS

- task/thread
- scheduler
- priority
- blocking
- queue
- mutex
- shared state
- race condition

## Embedded communication

- CAN frame
- arbitration
- ACK
- bitrate
- filtering
- RX/TX
- transceiver
- error confinement

## Embedded software architecture

- HAL/driver vs application
- producer/consumer
- separation of concerns
- state management
- fault handling
- diagnostics
- test injection

---

# 51. Những câu hỏi phải tự trả lời sau khi học project

Không cần học thuộc đáp án. Hãy thử nói bằng sơ đồ.

### Q1. Một CAN frame từ bus đi tới `dbc_decode_vehicle_frame()` bằng đường nào?

Phải trả lời được:

```text
CAN bus
→ transceiver
→ controller
→ Zephyr driver
→ filter
→ raw_can_msgq
→ CAN worker
→ decoder
```

### Q2. Vì sao dùng `k_msgq_get(..., K_FOREVER)`?

Vì worker là consumer và không cần busy-wait khi queue rỗng.

### Q3. Vì sao cần mutex?

Vì `g_current_telemetry` là shared state giữa worker và shell.

### Q4. Vì sao CRC kiểm tra trước decode?

Không sử dụng dữ liệu chưa được xác thực.

### Q5. Rolling counter dùng để làm gì?

Kiểm tra tính liên tục/freshness của sequence frame.

### Q6. `data[1] & 0x0F` làm gì?

Lấy 4 bit thấp của byte 1.

### Q7. Vì sao `data[4] << 8`?

Đưa high byte lên vị trí 8..15 để ghép thành `uint16_t`.

### Q8. Vì sao queue size là 16?

Đây là kích thước được source cấu hình bằng `CAN_RX_QUEUE_SIZE`.

### Q9. Hardware filter giải quyết gì?

Giảm frame không cần thiết đi vào application processing.

### Q10. Tại sao một board đơn có thể gặp ACK error ở Normal mode?

Vì transmitter cần một receiver trên physical bus acknowledge frame.

### Q11. Khi CRC sai, global telemetry có bị overwrite không?

Không, vì main chỉ commit telemetry khi decoder trả `true`.

### Q12. `DTC_U0100` có được thêm ngay khi mất frame không?

Không theo flow source hiện tại; timeout được kiểm tra khi `safety_monitor_get_active_dtc()` được gọi.

### Q13. DTC có tự clear khi dữ liệu tốt trở lại không?

Không, source hiện tại chỉ clear qua `safety_monitor_clear_dtc()`.

### Q14. `static last_counter` khác biến local bình thường thế nào?

Nó giữ giá trị giữa các lần gọi function nhưng vẫn giới hạn scope trong function.

### Q15. `extern` trong `diag_shell.c` dùng để làm gì?

Tham chiếu tới symbol được định nghĩa ở `main.c`.

### Q16. `VehicleTelemetry_t` là data model hay CAN frame?

Nó là **application-level telemetry model**, khác với raw `struct can_frame`.

### Q17. Vì sao simulator nằm trong shell file?

Để cung cấp control/test interface nhanh cho developer; đây là test-oriented application code.

### Q18. Nếu queue đầy thì sao?

Cần kiểm tra semantics của API/driver path đang sử dụng; source hiện tại không có logic riêng để báo queue overflow ở application layer.

### Q19. Nếu frame tới quá nhanh thì bottleneck nằm ở đâu?

Có thể ở filter/driver/FIFO/queue/decoder/consumer throughput. Phải đo từng tầng.

### Q20. Nếu chuyển từ STM32 sang MCU khác, phần nào dễ giữ nguyên?

Application logic như decoder/safety có khả năng tái sử dụng nhiều hơn; CAN/board integration phải đi qua driver + DeviceTree/SoC-specific configuration.

---

# 52. Một câu trả lời phỏng vấn hoàn chỉnh về project

Khi interviewer hỏi:

> “Em trình bày project này.”

Có thể trình bày theo flow:

> “Project của em là một CAN telematics gateway chạy trên STM32F746 với Zephyr RTOS. Hệ thống nhận CAN frame ID 0x123, đưa frame qua CAN filter rồi vào `k_msgq`. CAN worker thread block trên queue, khi nhận frame thì kiểm tra CRC và rolling counter trước khi decode speed, RPM và coolant temperature. Nếu frame hợp lệ, telemetry được cập nhật vào shared state có mutex bảo vệ, sau đó truyền sang safety monitor để kiểm tra các threshold và tạo DTC. Một safety thread kiểm tra trạng thái fault để điều khiển LED. Ngoài ra em có Zephyr Shell để đọc telemetry, đọc/xóa DTC và mô phỏng hoặc inject lỗi CAN. Kiến trúc này tách CAN ingestion khỏi application processing, dùng queue cho producer-consumer và mutex cho shared state.”

Điểm quan trọng là **nói theo data flow**, không đọc tên API như một danh sách.

---

# 53. Roadmap học project trong 7 buổi

## Buổi 1 — Đọc kiến trúc

Mục tiêu:

```text
main.c
→ thread
→ queue
→ telemetry
→ safety
→ shell
```

Tự vẽ sơ đồ hệ thống từ trí nhớ.

---

## Buổi 2 — Zephyr RTOS

Học trực tiếp:

```text
K_THREAD_DEFINE
k_msgq
k_mutex
K_FOREVER
K_MSEC
scheduler / priority
```

Phải giải thích được producer-consumer và shared state.

---

## Buổi 3 — CAN fundamentals

Học:

```text
controller
transceiver
frame
ID
arbitration
ACK
filter
Bus-Off
```

---

## Buổi 4 — Decode dữ liệu

Học:

```text
mask
shift
OR
XOR
little-endian
factor
offset
static counter
```

Tự decode một frame bằng tay.

---

## Buổi 5 — E2E / CRC

Tự trace:

```text
Data ID
→ init CRC
→ XOR byte
→ 8 bit iterations
→ final XOR
```

Không học thuộc “CRC là gì”; phải trace được một byte.

---

## Buổi 6 — Safety / Diagnostics

Tự trace:

```text
overheat
overspeed
corrupt
timeout
DTC clear
```

---

## Buổi 7 — Interview / Debug

Tự trả lời mà không nhìn README:

```text
Tại sao queue?
Tại sao mutex?
Normal vs Loopback?
CAN controller vs transceiver?
ACK error?
Bus-Off?
CRC vs counter?
DeviceTree?
```

---

# 54. Cách học một function trong project

Dùng 5 câu hỏi này với **mọi function**:

```text
1. Input là gì?
2. Output là gì?
3. Shared state nào bị đọc/ghi?
4. Function chạy trong thread/context nào?
5. Vì sao thiết kế như vậy?
```

Ví dụ `dbc_decode_vehicle_frame()`:

```text
1. Input
   data + dlc

2. Output
   VehicleTelemetry_t *out

3. State
   static last_counter

4. Context
   CAN worker thread

5. Why
   validate → decode → return trusted telemetry
```

---

# 55. Cách đọc code để không bị “học API rời rạc”

Đừng học:

```text
k_msgq_get() = hàm lấy queue
k_mutex_lock() = hàm lock mutex
can_send() = hàm send CAN
```

Hãy học:

```text
Vấn đề
  ↓
Thiết kế
  ↓
Primitive RTOS / API
  ↓
Code
  ↓
Behavior
```

Ví dụ:

```text
Vấn đề:
CAN arrival có thể burst

Thiết kế:
producer / consumer

Primitive:
k_msgq

Code:
K_MSGQ_DEFINE + k_msgq_get

Behavior:
worker không busy-wait, có buffer trung gian
```

Đó mới là kiến thức có thể chuyển sang project khác.

---

# 56. Những gì README cũ mô tả nhưng source hiện tại không xác nhận

Phần này cố tình để tránh học sai.

## 56.1. Queue 32 frame

README cũ có chỗ mô tả `k_msgq` 32 frame.

Source thật:

```c
#define CAN_RX_QUEUE_SIZE 16
```

→ **16 frame**.

## 56.2. CAN ISR custom

README cũ mô tả `CAN_RX0_IRQHandler` tự đọc:

```text
CAN_RI0R
CAN_RDT0R
CAN_RDL0R
CAN_RDH0R
```

Source được cung cấp không có ISR custom như vậy.

→ Đây là flow của một implementation bare-metal/custom-driver khác, **không phải code application hiện tại**.

## 56.3. Thread architecture

README cũ mô tả các thread priority khác.

Source thật:

```text
CAN worker   priority 5
Safety       priority 6
Simulator    priority 7
```

Shell command callbacks không phải thread do project tự tạo theo kiểu README cũ.

## 56.4. DBC decode

README cũ có mô tả:

```text
Speed = Raw × 0.01
RPM   = Raw × 0.25
```

Source thật:

```text
Speed = Raw × 1
RPM   = Raw >> 2  ≈ Raw × 0.25
```

## 56.5. RPM endianness

README cũ từng mô tả Big-Endian.

Source thật ghép:

```c
raw_rpm = data[3] | (data[4] << 8);
```

→ low byte ở `data[3]`, high byte ở `data[4]`.

## 56.6. Data ID

README cũ có chỗ dùng `0x1001`.

Source thật:

```c
#define VEHICLE_DATA_ID 0x1A2B
```

## 56.7. Timeout DTC

README cũ mô tả mất CAN >1s là tự phát DTC ngay.

Source hiện tại chỉ kiểm tra timeout trong:

```c
safety_monitor_get_active_dtc()
```

## 56.8. Corrupt frame và `vehicle status`

README cũ gợi ý `vehicle status` có thể phản ánh ngay CRC failure.

Source hiện tại không commit frame fail vào `g_current_telemetry`.

---

# 57. Phạm vi của README này

README này được viết để học **source hiện được cung cấp**.

Các file sau không có trong source ZIP hiện tại:

```text
app.overlay
prj.conf
CMakeLists.txt
board DTS
DBC file thật
RM0385 PDF
datasheet
```

Vì vậy những phần liên quan trực tiếp tới:

- pinctrl exact configuration
- Kconfig exact setting
- build target
- clock tree cụ thể
- register value cụ thể
- DBC file syntax thực tế

nên được học tiếp khi có các file đó.

Không nên coi một giá trị viết trong README cũ là bằng chứng rằng build hiện tại chắc chắn dùng giá trị đó.

---

# 58. Checklist: học xong project khi nào?

Bạn được xem là **đã hiểu project**, không phải chỉ nhớ code, khi tự trả lời được tất cả các ý sau:

```text
[ ] Vẽ được architecture từ CAN bus → shell
[ ] Giải thích producer/consumer
[ ] Giải thích queue size 16
[ ] Giải thích mutex bảo vệ telemetry
[ ] Giải thích 3 thread trong source
[ ] Giải thích normal vs loopback
[ ] Giải thích CAN controller vs transceiver
[ ] Giải thích ACK
[ ] Giải thích hardware filter 0x123 / 0x7FF
[ ] Tự decode 8-byte frame bằng tay
[ ] Ghép RPM bằng shift/or
[ ] Giải thích rolling counter
[ ] Trace được CRC function
[ ] Biết Data ID = 0x1A2B
[ ] Biết khi nào DTC_P0115 xuất hiện
[ ] Biết khi nào DTC_P0219 xuất hiện
[ ] Biết khi nào DTC_U0100 được đánh giá
[ ] Biết tại sao CRC bad không overwrite telemetry
[ ] Biết cách clear DTC
[ ] Biết cách test bằng shell
[ ] Biết những chỗ source hiện tại còn hạn chế
```

---

# 59. Tóm tắt cuối cùng — thứ cần “đọng lại”

Đừng nhớ project bằng danh sách file.
Hãy nhớ bằng **luồng**:

```text
CAN frame
   ↓
Filter
   ↓
Queue
   ↓
Worker
   ↓
Validate
   ├── CRC
   └── Rolling Counter
   ↓
Decode
   ↓
Trusted Telemetry
   ├── Shared State
   └── Safety Monitor
           ↓
          DTC
           ↓
          LED

Shell
   ├── observe telemetry
   ├── read/clear DTC
   ├── generate CAN frame
   └── inject fault
```

Và nhớ 4 ý tưởng kiến trúc:

```text
1. CAN driver chịu trách nhiệm nhận/gửi frame.
2. Queue tách CAN ingestion khỏi application processing.
3. Decoder biến raw bytes thành trusted telemetry.
4. Safety monitor biến telemetry thành diagnostic state.
```

Đó là “xương sống” của project.

---

# 60. Bài tập tự luyện ngay trên source

## Bài 1
Tự viết trên giấy frame:

```text
ID = 0x123
Counter = 7
Speed = 80
RPM = 3000
Temp = 88
```

và tính ra Byte 0..7.

## Bài 2
Đổi RPM threshold từ `6500` thành `6000`.

Xác định chính xác file/function phải sửa.

## Bài 3
Thêm telemetry mới:

```text
fuel_percent
```

xác định các tầng cần sửa.

## Bài 4
Thiết kế lại timeout để không phụ thuộc vào `dtc read`.

Vẽ architecture mới trước rồi mới code.

## Bài 5
Thiết kế một `communication health state` có:

```text
NO_DATA
VALID
CRC_ERROR
COUNTER_ERROR
TIMEOUT
```

và giải thích state transition.

Đây là bài nâng cấp từ “làm project” sang “thiết kế firmware”.
