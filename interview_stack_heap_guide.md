# Cẩm Nang Phỏng Vấn: Quản Lý Stack và Heap Trên Vi Điều Khiển

> **Chủ đề:** Quản lý vùng nhớ Stack và Heap trong lập trình nhúng C.  
> **Mục tiêu:** Phân tích cấu trúc bộ nhớ RAM, cơ chế hoạt động của Stack và Heap, nguyên nhân gây lỗi tràn bộ nhớ và kịch bản trả lời phỏng vấn kỹ thuật.

---

## Phần 1: Cấu Trúc Bộ Nhớ RAM Trong Vi Điều Khiển

Bộ nhớ RAM trong vi điều khiển là một dải ô nhớ liên tục, mỗi ô lưu 1 byte dữ liệu và có một địa chỉ vật lý xác định.

Ví dụ trên STM32F746: Bộ nhớ RAM có dung lượng 512KB, trải từ địa chỉ thấp `0x20000000` đến địa chỉ cao nhất `0x20050000`. Trình liên kết (Linker) chia không gian nhớ này thành 4 phân vùng chính:

```text
Địa chỉ CAO   ▲ 0x20050000 ──┬───────────────────────────────────────────┐
              │              │  STACK (Ngăn xếp)                         │
              │              │  Tự động điều khiển bởi CPU (con trỏ SP)  │
              │              │  ▼ Phát triển giảm dần về địa chỉ thấp    │
              │              ├ - - - - - - - - - - - - - - - - - - - - - ┤
              │              │  [VÙNG TRỐNG DỰ PHÒNG]                    │
              │              │  Khu vực va chạm khi xảy ra Stack Overflow │
              │              ├ - - - - - - - - - - - - - - - - - - - - - ┤
              │              │  ▲ Phát triển tăng dần lên địa chỉ cao    │
              │              │  HEAP (Bộ nhớ cấp phát động)              │
              │              │  Quản lý thủ công qua malloc() / free()   │
              │              ├───────────────────────────────────────────┤
              │              │  .bss (Biến toàn cục/tĩnh chưa khởi tạo)  │
              │              │  Xóa về 0 tự động trong startup code      │
              │              ├───────────────────────────────────────────┤
              │              │  .data (Biến toàn cục/tĩnh có giá trị đầu)│
              │              │  Sao chép giá trị từ Flash sang RAM       │
Địa chỉ THẤP  ▼ 0x20000000 ──┴───────────────────────────────────────────┘
```

---

## Phần 2: Cơ Chế Hoạt Động Của Stack

### 2.1. Nguyên lý vận hành của Stack
- **Cơ chế:** Stack hoạt động theo nguyên tắc LIFO (Last In, First Out): dữ liệu nạp vào sau cùng sẽ được lấy ra đầu tiên.
- **Điều khiển:** Phần cứng CPU trực tiếp quản lý Stack thông qua thanh ghi con trỏ ngăn xếp Stack Pointer (`SP`).
- **Hướng phát triển (Full Descending):** Kiến trúc ARM Cortex quy ước con trỏ `SP` bắt đầu từ đỉnh địa chỉ cao nhất của RAM (`0x20050000`). Khi CPU đẩy dữ liệu vào Stack (lệnh `PUSH`), giá trị trong `SP` giảm dần. Khi lấy dữ liệu ra khỏi Stack (lệnh `POP`), giá trị trong `SP` tăng trở lại.

---

### 2.2. Thành phần trong một Stack Frame
Mỗi lần một hàm được gọi, CPU tạo ra một vùng nhớ tạm thời trên Stack gọi là Stack Frame, gồm:
1. **Địa chỉ quay về (Return Address lưu trong thanh ghi `LR`):** Vị trí lệnh tiếp theo của hàm cha cần thực thi sau khi hàm con kết thúc.
2. **Biến cục bộ (Local Variables):** Toàn bộ biến tự động được khai báo bên trong phạm vi hàm.
3. **Đối số truyền vào hàm:** Dùng khi số lượng tham số vượt quá khả năng lưu trữ của các thanh ghi đa dụng `R0 - R3`.
4. **Ngữ cảnh ngắt (Interrupt Context):** Khi xảy ra ngắt ISR, CPU tự động lưu 8 thanh ghi cơ bản (`R0-R3`, `R12`, `LR`, `PC`, `xPSR`) vào Stack trước khi nhảy vào hàm phục vụ ngắt.

---

### 2.3. Hiện tượng tràn Stack (Stack Overflow)
- **Cơ chế:** Khi các hàm lồng nhau quá nhiều cấp hoặc khai báo mảng cục bộ dung lượng lớn, con trỏ `SP` giảm vượt qua giới hạn quy định của vùng Stack, ghi đè dữ liệu lên các vùng nhớ lân cận như `.bss` và `.data`.
- **Hậu quả kỹ thuật:**
  * Giá trị của các biến toàn cục bị thay đổi bất thường.
  * Địa chỉ quay về trong thanh ghi `LR` bị sai lệch. Khi hàm thực thi lệnh trả về (`BX LR`), CPU nhảy vào vùng nhớ không hợp lệ và kích hoạt ngoại lệ `HardFault`.

#### Quy tắc phòng tránh tràn Stack:
1. **Không sử dụng hàm đệ quy:** Đệ quy tạo thêm Stack Frame liên tục sau mỗi lần gọi hàm và nhanh chóng làm cạn kiệt dung lượng Stack.
2. **Không khai báo mảng hoặc cấu trúc dữ liệu lớn bên trong hàm:** Tránh khai báo mảng cục bộ như `char buffer[1024];`. Cần chuyển sang dạng mảng tĩnh (`static char buffer[1024];`) hoặc biến toàn cục.
3. **Cấu hình kích thước Stack có biên độ an toàn:** Trong Linker Script, dự phòng dung lượng Stack dư khoảng $20\% - 30\%$ so với mức tiêu thụ đo đạc thực tế.

---

## Phần 3: Cơ Chế Hoạt Động Của Heap

### 3.1. Nguyên lý vận hành của Heap
- **Đặc điểm:** Heap là vùng nhớ tự do nằm giữa phân vùng `.bss` và Stack, phát triển theo chiều tăng dần từ địa chỉ thấp lên địa chỉ cao.
- **Quản lý:** Lập trình viên trực tiếp điều khiển việc cấp phát và giải phóng thông qua thư viện chuẩn C:
  * `malloc(size)`: Cấp phát một khối nhớ liên tục có dung lượng `size` byte.
  * `free(ptr)`: Trả lại khối nhớ đã cấp phát về cho hệ thống.

---

### 3.2. Rủi ro khi sử dụng cấp phát động (malloc / free) trong hệ thống nhúng

Trong lập trình vi điều khiển thời gian thực, việc sử dụng `malloc` và `free` tiềm ẩn nhiều rủi ro do 3 nguyên nhân kỹ thuật:

#### Lý do 1: Phân mảnh bộ nhớ (Heap Fragmentation)
Sau nhiều chu kỳ cấp phát và giải phóng các khối nhớ với kích thước khác nhau, bộ nhớ bị chia nhỏ thành nhiều khoảng trống rời rạc. Khi hệ thống yêu cầu một khối nhớ lớn liên tục, dù tổng dung lượng nhớ còn trống đủ lớn, hàm `malloc()` vẫn trả về con trỏ `NULL` do không tìm được đoạn nhớ liên tục nào đáp ứng yêu cầu. Hậu quả là chương trình không thể tiếp tục thực thi tác vụ.

#### Lý do 2: Rò rỉ bộ nhớ (Memory Leak)
Các thiết bị nhúng thường hoạt động liên tục 24/7. Nếu một nhánh xử lý lỗi bỏ sót lệnh `free()`, bộ nhớ RAM sẽ hao hụt dần theo thời gian. Sau một khoảng thời gian vận hành dài (vài giờ hoặc vài ngày), vùng Heap cạn kiệt hoàn toàn, khiến hệ thống dừng hoạt động hoặc khởi động lại đột ngột.

#### Lý do 3: Độ trễ không xác định (Non-deterministic Latency)
Hàm `malloc()` phải duyệt qua danh sách liên kết để tìm khối nhớ trống phù hợp. Thời gian thực thi phụ thuộc vào mức độ phân mảnh hiện tại: khi Heap gọn gàng, việc tìm kiếm chỉ mất khoảng $1\mu s$; khi Heap phân mảnh phức tạp, thời gian có thể kéo dài lên hàng trăm micro giây. Sự biến thiên này vi phạm yêu cầu thời gian thực (Real-Time) của các hệ thống an toàn như phanh ABS hoặc mạng CAN trên ô tô.

---

### Phân tích lỗi thực tế: Thiết bị Smartwatch bị đóng băng và tự khởi động lại sau 5 đến 30 phút

- **Bối cảnh hệ thống:** Đồng hồ thông minh sử dụng vi điều khiển ESP32-S3 Dual-Core 240 MHz, FreeRTOS, giao diện đồ họa LVGL v8, kết nối BLE và cảm biến nhịp tim MAX30102.
- **Hiện tượng:** Thiết bị vận hành bình thường lúc ban đầu. Sau 5 đến 30 phút chạy liên tục, màn hình bị đóng băng, cảm ứng không phản hồi, và sau đó 5 giây hệ thống tự khởi động lại về màn hình logo.
- **Cơ chế gây lỗi (Root Cause):**
  1. Tích tụ rò rỉ bộ nhớ: Mỗi chu kỳ nhận bản tin định kỳ từ cảm biến nhịp tim hoặc BLE, mã nguồn gọi các hàm xử lý chuỗi động (`lv_label_set_text_fmt`, cJSON) có sử dụng `malloc()`. Do một nhánh rẽ xử lý lỗi bỏ sót lệnh `free()`, mỗi phút hệ thống mất khoảng $2\text{ KB}$ RAM.
  2. Phân mảnh bộ nhớ: Các gói tin BLE đến bất định với kích thước khác nhau làm chia nhỏ vùng nhớ Heap.
  3. Chuỗi phản ứng lỗi: Khi Heap cạn kiệt, hàm cấp phát trả về `NULL`. Mã nguồn không kiểm tra con trỏ `NULL` mà truy xuất thẳng vào vùng nhớ `NULL->...`, kích hoạt ngoại lệ `LoadStoreProhibited`. Đồng thời, tác vụ giao diện bị nghẽn khiến Task Watchdog Timer (TWDT) vượt ngưỡng 5000ms không được nạp lại, kích hoạt cơ chế phần cứng Reset vi điều khiển.
- **Giải pháp xử lý:** Loại bỏ hoàn toàn việc cấp phát động trong các tác vụ chu kỳ. Sử dụng bộ đệm tĩnh `static char s_buf[64]` và chuyển giao dữ liệu qua FreeRTOS Queue để kiểm soát toàn bộ tài nguyên nhớ ngay từ khâu biên dịch.

---

### 3.3. Giải pháp cấp phát tĩnh (Static Allocation)
Trong các tiêu chuẩn an toàn công nghiệp (MISRA-C, ISO 26262), việc sử dụng `malloc/free` trong quá trình vận hành (runtime) bị nghiêm cấm.
- Toàn bộ bộ đệm truyền nhận (UART Ring Buffer, CAN Frame, biến trạng thái) được khai báo dưới dạng mảng tĩnh (`static` hoặc toàn cục).
- Toàn bộ dung lượng RAM được xác định chính xác tại thời điểm biên dịch thông qua file `.map`, loại bỏ rủi ro tràn bộ nhớ động trong suốt vòng đời thiết bị.

---

## Phần 4: Cấu Hình Kích Thước Stack và Heap Trong Mã Nguồn

Trong file Linker Script của vi điều khiển (ví dụ `STM32F746NGHx_FLASH.ld`), kích thước tối thiểu của Stack và Heap được định nghĩa tường minh:

```ld
/* Kích thước Heap tối thiểu */
_Min_Heap_Size = 0x0;      /* Trong dự án an toàn, Heap được tắt hoàn toàn */

/* Kích thước Stack tối thiểu */
_Min_Stack_Size = 0x1000;  /* Dành riêng 4096 bytes (4KB) cho Stack */
```

Khi phát hiện nguy cơ tràn Stack do số cấp gọi hàm sâu, kỹ sư điều chỉnh tham số `_Min_Stack_Size` (ví dụ từ 2KB lên 4KB hoặc 8KB) để đảm bảo biên độ an toàn cho hệ thống.

---

## Phần 5: Bộ Câu Hỏi Phỏng Vấn Thường Gặp Về Quản Lý Bộ Nhớ

### Câu hỏi 1: Khai báo mảng `int a[1000];` bên trong một hàm có rủi ro gì?
- **Trả lời:**  
  Biến `a` là biến cục bộ nên được cấp phát trên Stack. Với kiểu `int` 4 bytes, mảng này chiếm ngay $4000\text{ bytes} \approx 4\text{ KB}$ dung lượng Stack. Trên vi điều khiển, nếu file Linker Script chỉ cấu hình 1KB hoặc 2KB cho Stack, lệnh này sẽ gây tràn Stack (Stack Overflow) ngay khi vào hàm, dẫn tới lỗi `HardFault`. Để đảm bảo an toàn, cần chuyển mảng ra ngoài phạm vi hàm hoặc thêm từ khóa `static` để chuyển dữ liệu sang phân vùng `.bss`.

### Câu hỏi 2: Biến `const int x = 10;` và `static int y = 20;` nằm ở phân vùng nhớ nào?
- **Trả lời:**  
  - `const int x = 10;`: Nằm ở vùng `.rodata` (Read-Only Data) lưu trữ trên bộ nhớ Flash, không tiêu tốn dung lượng RAM.
  - `static int y = 20;`: Nằm ở vùng `.data` trên bộ nhớ RAM (vì có giá trị khởi tạo khác 0). Giá trị khởi tạo ban đầu được lưu trong Flash và sao chép sang RAM khi khởi động.

### Câu hỏi 3: Biến toàn cục chưa khởi tạo (`int g_val;`) và khởi tạo bằng 0 (`int g_val2 = 0;`) nằm ở đâu?
- **Trả lời:**  
  Cả hai biến đều nằm ở phân vùng `.bss` trên RAM. Trong quy trình khởi động vi điều khiển, hàm `Reset_Handler` thực thi một vòng lặp xóa toàn bộ vùng `.bss` về giá trị 0 trước khi chuyển quyền điều khiển sang hàm `main()`.

### Câu hỏi 4: Cách xác định dung lượng Flash và RAM sau khi biên dịch chương trình?
- **Trả lời:**  
  - Quan sát bảng tổng kết kích thước bộ nhớ từ trình biên dịch:
    $$\text{Dung lượng Flash} = \text{.text} + \text{.rodata} + \text{.data}$$
    $$\text{Dung lượng RAM} = \text{.data} + \text{.bss} + \text{Stack} + \text{Heap}$$
  - Hoặc kiểm tra chi tiết trong file `.map` do Linker sinh ra để xem dung lượng chính xác của từng hàm và từng biến.

### Câu hỏi 5: Phân biệt sự khác nhau giữa Stack Overflow và Buffer Overflow?
- **Trả lời:**  
  - **Stack Overflow:** Xảy ra khi con trỏ ngăn xếp `SP` vượt qua ranh giới vùng nhớ dành cho Stack (do hàm lồng nhau quá sâu hoặc kích thước biến cục bộ vượt dung lượng Stack).
  - **Buffer Overflow:** Xảy ra khi thao tác ghi dữ liệu vượt quá độ dài được cấp phát của một mảng cụ thể (ví dụ ghi 20 bytes vào mảng kích thước 10 bytes), làm hỏng dữ liệu của các biến lân cận trong bộ nhớ.

---

## Phần 6: Kịch Bản Trả Lời Phỏng Vấn (Tóm Tắt 60 Giây)

Khi người phỏng vấn đặt câu hỏi: *"Bạn quản lý Stack và Heap như thế nào trong dự án nhúng?"*, câu trả lời có thể trình bày theo 3 luận điểm kỹ thuật chính:

> "Trong lập trình vi điều khiển, tôi quản lý Stack và Heap theo định hướng an toàn bộ nhớ và tính tất định (Deterministic):
>
> 1. **Về phân bổ không gian nhớ:** Kích thước Stack và Heap được cấu hình tường minh trong file Linker Script (`.ld`). Bộ nhớ RAM được phân chia rõ ràng giữa vùng dữ liệu tĩnh (`.data`, `.bss`), vùng Heap phát triển từ dưới lên, và vùng Stack phát triển giảm dần từ đỉnh cao nhất của RAM.
>
> 2. **Về quản lý Heap:** Trong các hệ thống nhúng yêu cầu độ tin cậy cao, tôi hạn chế tối đa hoặc không sử dụng `malloc/free` trong quá trình runtime. Việc cấp phát động trên dung lượng RAM hạn chế dễ dẫn đến phân mảnh bộ nhớ và nguy cơ rò rỉ RAM, làm hệ thống dừng hoạt động sau thời gian dài vận hành. Thay vào đó, tôi ưu tiên cơ chế cấp phát tĩnh (Static Allocation) để kiểm soát chính xác 100% dung lượng RAM thông qua file `.map` ngay từ thời điểm biên dịch.
>
> 3. **Về kiểm soát Stack:** Rủi ro chính của Stack là hiện tượng Stack Overflow gây ghi đè dữ liệu lên vùng `.bss`. Tôi phòng ngừa bằng ba nguyên tắc: không dùng hàm đệ quy, không khai báo cấu trúc dữ liệu lớn bên trong hàm (sử dụng biến `static` hoặc bộ đệm toàn cục), và luôn cấu hình kích thước Stack trong Linker Script có biên độ an toàn từ 20% đến 30% so với mức tiêu thụ đo đạc cao nhất."
