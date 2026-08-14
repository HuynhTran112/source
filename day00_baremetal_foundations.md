# 📘 [NGÀY 0] NỀN TẢNG CỐT LÕI BARE-METAL & CƠ CHẾ THANH GHI VI ĐIỀU KHIỂN

> **Mục tiêu tài liệu:** Cung cấp toàn bộ kiến thức nền tảng "từ số 0" về cách CPU giao tiếp với phần cứng thông qua thanh ghi (Registers), giải mã cú pháp con trỏ struct C, từ khóa `volatile`, các phép toán bitwise và sơ đồ cây xung nhịp (Clock Tree). Đọc xong tài liệu này, bạn sẽ tự tin đọc hiểu 100% mọi driver bare-metal từ Ngày 1 đến Ngày 8 mà không bị bỡ ngỡ.

---

## 1. MEMORY-MAPPED I/O LÀ GÌ? TẠI SAO GHI VÀO ĐỊA CHỈ LẠI ĐIỀU KHIỂN ĐƯỢC PHẦN CỨNG?

### 1.1. Khái niệm cốt lõi: Không gian địa chỉ thống nhất (Unified Address Space)
Trong các vi điều khiển ARM Cortex-M (như STM32F746NG - kiến trúc 32-bit), CPU có khả năng đánh địa chỉ một không gian bộ nhớ rộng $2^{32} = 4\text{ GB}$ (từ `0x0000 0000` đến `0xFFFF FFFF`).

Phần cứng ARM không phân biệt tập lệnh riêng cho bộ nhớ RAM và thiết bị ngoại vi (khác với kiến trúc x86 có lệnh `IN`/`OUT`). Thay vào đó, **mọi thứ đều là địa chỉ bộ nhớ**:

```text
+-----------------------+ 0xFFFF FFFF
| System Control / NVIC | (Bộ điều khiển ngắt, SysTick...)
+-----------------------+ 0xE000 0000
|                       |
+-----------------------+ 0x6000 0000
| External Memory / FMC | (SDRAM, Flash ngoài...)
+-----------------------+ 0x5000 0000
| AHB2 / AHB3 Periph    | (USB OTG, DCMI...)
+-----------------------+ 0x4000 0000  <--- VÙNG NGOẠI VI (PERIPHERALS: RCC, GPIO, UART, CAN...)
| Peripherals (APB/AHB) |      Mỗi thanh ghi phần cứng là 1 ô nhớ 32-bit (4 bytes)
+-----------------------+ 0x2000 0000  <--- VÙNG BỘ NHỚ RAM (SRAM)
| Internal SRAM (RAM)   |      Chứa biến, stack, heap của chương trình
+-----------------------+ 0x0800 0000  <--- VÙNG FLASH CODE
| Internal Flash        |      Chứa mã máy (firmware code) sau khi nạp
+-----------------------+ 0x0000 0000
```

### 1.2. Bên dưới phần cứng thực sự xảy ra điều gì?
Khi bạn viết một biến thông thường trong C:
```c
uint32_t a = 5; // Ghi vào SRAM tại địa chỉ 0x2000 0000
```
CPU phát tín hiệu qua Bus Matrix tới chip nhớ SRAM, các transistor flip-flop trong RAM lưu giá trị `5`.

Nhưng khi bạn ghi vào địa chỉ thuộc vùng Ngoại vi (ví dụ: `0x4002 0014` - thanh ghi `GPIOA->ODR`):
```c
*(uint32_t *)0x40020014 = 0x0001; // Ghi vào thanh ghi ODR của GPIOA
```
1. CPU phát địa chỉ `0x4002 0014` lên bus AHB1.
2. Bus Decoder nhận diện địa chỉ này không dẫn tới RAM, mà dẫn thẳng tới module phần cứng **GPIO Port A**.
3. Đầu ra của ô nhớ này được nối dây điện trực tiếp tới mạch đệm cổng logic (Output Driver).
4. Mạch đệm kích hoạt transistor kéo chân vật lý **PA0** lên mức cao ($3.3\text{V}$) $\rightarrow$ Đèn LED nối chân PA0 sáng lên!

> **Tóm lại:** **Thanh ghi (Register)** bản chất là các mạch chốt dữ liệu (Flip-Flops/Latches) vật lý của khối phần cứng (UART, CAN, Timer, PLL...), nhưng được gán (map) vào một địa chỉ cụ thể trên bus hệ thống. Đọc/ghi thanh ghi chính là đo/điều khiển mạch điện thông qua thao tác bộ nhớ!

---

## 2. STRUCT C ÁNH XẠ THANH GHI HOẠT ĐỘNG NHƯ THẾ NÀO?

Trong code driver Ngày 1, bạn thấy cú pháp:
```c
RCC->CR |= RCC_CR_HSEON;
```
Tại sao một cấu trúc C (`struct`) lại có thể trỏ chính xác từng milimet vào các thanh ghi của vi điều khiển?

### 2.1. Bản chất bộ nhớ của C `struct`
Trong ngôn ngữ C chuẩn, các trường trong `struct` được sắp xếp **tuần tự, liên tục** trong bộ nhớ từ địa chỉ thấp đến cao. Mỗi biến kiểu `uint32_t` chiếm đúng 4 bytes ($32\text{ bits}$).

Hãy quan sát struct của khối RCC:
```c
typedef struct {
    volatile uint32_t CR;         // Offset: 0x00 (Chiếm 4 bytes: 0x00 - 0x03)
    volatile uint32_t PLLCFGR;    // Offset: 0x04 (Chiếm 4 bytes: 0x04 - 0x07)
    volatile uint32_t CFGR;       // Offset: 0x08 (Chiếm 4 bytes: 0x08 - 0x0B)
    volatile uint32_t CIR;        // Offset: 0x0C (Chiếm 4 bytes: 0x0C - 0x0F)
    volatile uint32_t AHB1RSTR;   // Offset: 0x10 (Chiếm 4 bytes: 0x10 - 0x13)
    volatile uint32_t AHB2RSTR;   // Offset: 0x14 (Chiếm 4 bytes: 0x14 - 0x17)
    volatile uint32_t AHB3RSTR;   // Offset: 0x18 (Chiếm 4 bytes: 0x18 - 0x1B)
    uint32_t RESERVED0;           // Offset: 0x1C (Chiếm 4 bytes đệm để không lệch offset!)
    volatile uint32_t APB1RSTR;   // Offset: 0x20 (Chiếm 4 bytes: 0x20 - 0x23)
    ...
} RCC_TypeDef;
```

### 2.2. Ma thuật ép kiểu con trỏ: `((RCC_TypeDef *)RCC_BASE)`
Giả sử Reference Manual quy định khối RCC bắt đầu từ địa chỉ cơ sở `RCC_BASE = 0x40023800UL`.
Ta định nghĩa macro:
```c
#define RCC ((RCC_TypeDef *)0x40023800UL)
```

Khi bạn viết `RCC->CFGR = 0x1234;`, Compiler sẽ tính toán địa chỉ như sau:
$$\text{Địa chỉ mục tiêu} = \text{Địa chỉ cơ sở (RCC)} + \text{Độ lệch (Offset của trường CFGR)}$$
$$\text{Địa chỉ mục tiêu} = \text{0x4002 3800} + \text{0x08} = \mathbf{\text{0x4002 3808}}$$

```text
  Địa chỉ RAM/Bus        Struct Field           Tên Thanh ghi Phần cứng
  +------------------+  ------------------     -------------------------
  | 0x4002 3800      |  RCC->CR                RCC Clock Control Register
  +------------------+  ------------------     -------------------------
  | 0x4002 3804      |  RCC->PLLCFGR           RCC PLL Configuration Register
  +------------------+  ------------------     -------------------------
  | 0x4002 3808      |  RCC->CFGR              RCC Clock Configuration Register
  +------------------+  ------------------     -------------------------
  | 0x4002 380C      |  RCC->CIR               RCC Clock Interrupt Register
  +------------------+  ------------------     -------------------------
  | 0x4002 381C      |  RCC->RESERVED0         (Vùng trống phần cứng không dùng)
  +------------------+  ------------------     -------------------------
  | 0x4002 3820      |  RCC->APB1RSTR          RCC APB1 Peripheral Reset Register
```

> **Cảnh báo cực kỳ quan trọng:** Nếu nhà sản xuất để trống một ô nhớ giữa các thanh ghi (ví dụ offset `0x1C` không có thanh ghi nào), ta **bắt buộc** phải khai báo một biến `uint32_t RESERVED` vào struct. Nếu quên, tất cả các thanh ghi phía sau sẽ bị dịch lên 4 bytes và ghi đè sai thanh ghi trong thực tế!

---

## 3. TẠI SAO BẮT BUỘC PHẢI DÙNG TỪ KHÓA `volatile`?

### 3.1. Compiler Optimization (Trình biên dịch tối ưu hóa mã như thế nào?)
Trình biên dịch C (GCC/Clang) luôn cố gắng tối ưu mã nguồn để chạy nhanh nhất. Khi đọc một biến nhiều lần liên tiếp, compiler giả định: *"Biến này ở trong RAM, trong vòng lặp ta không sửa nó, vậy giá trị của nó không thể tự đổi. Hãy copy nó vào 1 thanh ghi CPU (R0, R1) để đọc cho nhanh thay vì phải phát tín hiệu ra bus RAM liên tục!"*

### 3.2. Hiểm họa chết người với Bare-metal Register
Hãy xem xét vòng lặp chờ thạch anh ngoài HSE khởi động ổn định:
```c
// Giả sử KHÔNG có từ khóa volatile:
uint32_t *cr_ptr = (uint32_t *)0x40023800;

RCC->CR |= (1 << 16); // Bật HSEON

// Chờ bit HSERDY (bit 17) bật lên 1 bởi phần cứng
while (!(*cr_ptr & (1 << 17))) {
    // Vòng lặp chờ
}
```

**Compiler nhìn thấy đoạn code trên và suy luận:**
1. CPU đọc `*cr_ptr` lần đầu tiên $\rightarrow$ bit 17 đang là `0` (vì thạch anh cần vài mili-giây mới dao động ổn định).
2. Bên trong thân vòng lặp `{ }` không có lệnh nào sửa đổi giá trị của `*cr_ptr`.
3. Compiler kết luận: `*cr_ptr` sẽ mãi mãi bằng `0`, điều kiện vòng lặp luôn đúng (`while(1)`).
4. Compiler tối ưu hóa mã máy thành: **Vòng lặp vô tận treo vĩnh viễn (Infinite Hang)!**

### 3.3. `volatile` giải quyết vấn đề như thế nào?
Từ khóa `volatile` là một lời cảnh báo dứt khoát gửi tới Compiler:
> *"Giá trị tại địa chỉ ô nhớ này có thể bị thay đổi bất ngờ bởi phần cứng bên ngoài (hoặc ngắt ISR) mà trình biên dịch không thể nhìn thấy trong luồng code C thông thường. BẮT BUỘC mỗi lần kiểm tra phải tạo lệnh đọc trực tiếp từ địa chỉ phần cứng thật trên bus, TUYỆT ĐỐI KHÔNG ĐƯỢC TỐI ƯU HÓA!"*

Do đó, mọi thanh ghi vi điều khiển luôn phải được khai báo dạng:
```c
volatile uint32_t CR;
```

---

## 4. BẬC THẦY BITWISE: CÁC PHÉP TOÁN BẢN LỀ TRÊN THANH GHI

Một thanh ghi 32-bit gồm 32 công tắc riêng lẻ (từ bit 0 đến bit 31). Để điều khiển từng công tắc mà không làm ảnh hưởng đến các công tắc bên cạnh, bạn phải thành thạo 4 thao tác Bitwise sau:

### 4.1. Bật bit lên 1 (Set Bit) $\rightarrow$ Dùng phép `OR` (`|`)
- **Nguyên lý:** `X | 1 = 1`, `X | 0 = X` (Giữ nguyên các bit khác).
- **Cú pháp:**
  ```c
  REG |= (1U << POS);
  // Ví dụ: Bật bit HSEON (Bit 16) trong RCC->CR
  RCC->CR |= (1U << 16);
  ```

### 4.2. Tắt bit về 0 (Clear Bit) $\rightarrow$ Dùng phép `AND` với đảo bit (`& ~`)
- **Nguyên lý:** `X & 0 = 0`, `X & 1 = X` (Giữ nguyên các bit khác).
- **Cú pháp:**
  ```c
  REG &= ~(1U << POS);
  // Ví dụ: Tắt bit PLLON (Bit 24) trong RCC->CR
  RCC->CR &= ~(1U << 24);
  ```

### 4.3. Cấu hình trường nhiều bit (Multi-Bit Bitfield RMW Pattern)
Khi một thông số chiếm nhiều bit (ví dụ bộ chia `PLLM[5:0]` chiếm 6 bits từ bit 0 đến 5):
- **Tuyệt đối không được ghi đè trực tiếp:** `REG |= (25 << 0);` $\rightarrow$ Nếu giá trị cũ đang là `0x3F` (`111111`b), phép OR sẽ cho ra giá trị rác sai hoàn toàn!
- **Mẫu chuẩn RMW (Read - Modify - Write): Xóa sạch trước $\rightarrow$ Gán giá trị sau:**
  ```c
  #define PLLM_MASK   (0x3FU << 0) // Mặt nạ bao phủ 6 bit (111111b)
  
  // Bước 1: Xóa sạch 6 bit đó về 000000b
  RCC->PLLCFGR &= ~PLLM_MASK;
  
  // Bước 2: Gán giá trị mong muốn (ví dụ: 25 = 0x19)
  RCC->PLLCFGR |= (25U << 0);
  ```

### 4.4. Kiểm tra trạng thái cờ (Polling Flag)
- **Cú pháp:**
  ```c
  // Chờ cho đến khi bit HSERDY (bit 17) nhảy lên 1
  while (!(RCC->CR & (1U << 17))) {
      // Chờ đợi phần cứng khóa xung
  }
  ```

### 4.5. QUY TẮC SỐNG CÒN: Không bao giờ dùng `|=` trên thanh ghi cờ W1C / ICR
Nhiều ngoại vi (DMA, UART, CAN, Timer) chứa các thanh ghi cờ ngắt có cơ chế **W1C (Write 1 to Clear)** hoặc thanh ghi xóa cờ **ICR (Interrupt Flag Clear Register)**:

- **Bản chất của W1C:** Khi sự kiện ngắt xảy ra, phần cứng tự bật bit cờ lên `1`. Để xóa cờ đó, phần mềm phải **ghi giá trị `1`** vào đúng bit đó.
- **Hiểm họa khi dùng phép `|=`:**
  ```c
  // ❌ SAI LẦM KINH ĐIỂN:
  DMA2->LIFCR |= DMA_LIFCR_CTCIF0; // Muốn xóa cờ truyền xong kênh 0
  ```
  Khi bạn viết `DMA2->LIFCR |= FLAG;`:
  1. CPU sẽ đọc toàn bộ giá trị hiện tại của thanh ghi (có thể lúc này cờ lỗi truyền TCIF1, TEIF0 của các kênh khác cũng đang bằng `1`).
  2. Phép OR giữ nguyên các số `1` đó và CPU ghi toàn bộ ngược lại vào thanh ghi.
  3. **Hậu quả:** Bạn đã vô tình **xóa sạch toàn bộ cờ ngắt và cờ báo lỗi của các kênh khác** trước khi trình xử lý ngắt (ISR) kịp đọc!
- **Cách xử lý chuẩn (Ghi gán trực tiếp `=` thay vì `|=`):**
  ```c
  // ✅ ĐÚNG: Ghi trực tiếp giá trị bit cần xóa (Direct Assignment)
  DMA2->LIFCR = DMA_LIFCR_CTCIF0;         // Chỉ xóa đúng cờ Channel 0
  USART1->ICR = USART_ICR_ORECF;          // Chỉ xóa đúng cờ Overrun Error (Ngày 2)
  ```

> 📌 **Lưu ý đặc biệt về `RCC->CSR` ở Ngày 1:**
> Trong hàm xóa cờ Reset `System_ClearResetFlags()`, ta viết `RCC->CSR |= RCC_CSR_RMVF;` vẫn an toàn tuyệt đối là vì: các cờ reset xung quanh (Bit 25..31) đều là kiểu **`RO` (Read-Only)**, phần cứng bỏ qua mọi lệnh ghi vào bit RO. Tuy nhiên, đây là trường hợp ngoại lệ. Với các thanh ghi ngắt thực thụ như **UART (cờ ORE/FE)** hay **DMA (cờ TCIF/TEIF)** ở **Ngày 2**, bạn **BẮT BUỘC** phải dùng phép gán trực tiếp `=`!

---

## 5. BỨC TRANH TOÀN CẢNH: HỆ THỐNG CÂY XUNG NHỊP (CLOCK TREE)

Vi điều khiển giống như một cơ thể sống, và **Clock** chính là nhịp tim. Nếu không có xung nhịp nhấp nháy, các flip-flop bên trong chip không thể chuyển trạng thái.

### 5.1. Sơ đồ phân phối xung tổng quát
```text
[Thạch anh ngoài HSE 25MHz] hoặc [Nội HSI 16MHz]
                 │
                 ▼
      ┌─────────────────────┐
      │   Bộ nhân Main PLL  │  (Chia M -> Nhân N -> Chia P)
      └─────────────────────┘
                 │
                 ▼
      [ SYSCLK: Tối đa 216 MHz ]  <--- Xung nhịp trung tâm hệ thống
                 │
                 ▼ (AHB Prescaler: HPRE)
      [ HCLK / AHB Bus: 216 MHz ] ───► Cấp cho CPU Cortex-M7 Core, SRAM, DMA
                 │
        ┌────────┴────────────────────┐
        │ (APB1 Prescaler: PPRE1)     │ (APB2 Prescaler: PPRE2)
        ▼                             ▼
 [ APB1 Bus: Max 54 MHz ]     [ APB2 Bus: Max 108 MHz ]
  (UART2-8, CAN1-2, I2C, SPI2-3)   (UART1/6, SPI1/4-6, SDMMC, Timers cao tốc)
        │                             │
        ▼ (Nhân đôi nếu PPRE1 ≠ 1)    ▼ (Nhân đôi nếu PPRE2 ≠ 1)
 [ Timer Clocks: 108 MHz ]     [ Timer Clocks: 216 MHz ]
  (TIM2, TIM3, TIM4, TIM5...)   (TIM1, TIM8, TIM9, TIM10...)
```

### 5.2. Các thuật ngữ quan trọng cần ghi nhớ:
1. **HSI (High-Speed Internal):** Xung RC nội bên trong chip (~16MHz). Sai số cao theo nhiệt độ, dùng để khởi động ban đầu.
2. **HSE (High-Speed External):** Thạch anh gắn ngoài bo mạch (Board Discovery dùng 25MHz). Độ chính xác cực cao, bắt buộc dùng cho CAN, USB, UART baudrate chuẩn.
3. **PLL (Phase-Locked Loop):** Mạch nhân tần số phần cứng. Biến xung 25MHz thành xung siêu cao tốc 216MHz.
4. **SYSCLK (System Clock):** Xung chính cấp vào lõi vi điều khiển.
5. **HCLK (AHB Bus Clock):** Cấp cho đường truyền dữ liệu cao tốc giữa CPU, bộ nhớ Flash, SRAM, DMA.
6. **PCLK1 (APB1 Peripheral Clock):** Xung cấp cho các ngoại vi tốc độ trung bình (Max 54MHz trên F7).
7. **PCLK2 (APB2 Peripheral Clock):** Xung cấp cho các ngoại vi tốc độ cao (Max 108MHz trên F7).

---

## 6. BÀI TẬP DẪN DẮT: TỰ TAY TÍNH TOÁN BỘ SỐ PLL CHO STM32F746

Hãy cùng giải bài toán thực tế của **Ngày 1**:
> **Đề bài:** Thiết kế hệ thống xung nhịp STM32F746 chạy hết công suất $f_{SYSCLK} = 216\text{ MHz}$ và cấp đúng chuẩn $48\text{ MHz}$ cho USB từ thạch anh ngoài $f_{HSE} = 25\text{ MHz}$.

### Bước 1: Tính bộ chia đầu vào `PLLM`
- RM0385 quy định tần số ngõ vào khối nhân tần VCO phải nằm trong khoảng $1\text{ MHz} \le f_{VCO\_in} \le 2\text{ MHz}$ (nhà sản xuất khuyến nghị chọn đúng $1\text{ MHz}$ để PLL ít nhiễu pha jitter nhất).
$$f_{VCO\_in} = \frac{f_{HSE}}{PLLM} = \frac{25\text{ MHz}}{PLLM} = 1\text{ MHz} \implies \mathbf{PLLM = 25}$$

### Bước 2: Tính bộ nhân `PLLN` và bộ chia hệ thống `PLLP`
- Tần số ngõ ra của VCO: $f_{VCO\_out} = f_{VCO\_in} \times PLLN = 1\text{ MHz} \times PLLN$.
- Tần số hệ thống: $f_{SYSCLK} = \frac{f_{VCO\_out}}{PLLP} = \frac{1\text{ MHz} \times PLLN}{PLLP} = 216\text{ MHz}$.
- Theo RM0385, $PLLP \in \{2, 4, 6, 8\}$. Chọn $PLLP = 2$ (mã bit `00`b) để đạt tần số cao nhất:
$$1\text{ MHz} \times PLLN = 216\text{ MHz} \times 2 = 432\text{ MHz} \implies \mathbf{PLLN = 432}$$
- *Kiểm tra giới hạn phần cứng:* RM0385 quy định $100\text{ MHz} \le f_{VCO\_out} \le 432\text{ MHz}$. Giá trị $432\text{ MHz}$ hoàn toàn hợp lệ!

### Bước 3: Tính bộ chia ngõ ra USB `PLLQ`
- Ngoại vi USB yêu cầu xung clock cố định chính xác $48\text{ MHz}$:
$$f_{USB} = \frac{f_{VCO\_out}}{PLLQ} = \frac{432\text{ MHz}}{PLLQ} = 48\text{ MHz} \implies \mathbf{PLLQ = \frac{432}{48} = 9}$$

### Bước 4: Tính bộ chia các Bus (`AHB`, `APB1`, `APB2`)
- **AHB Bus:** Hỗ trợ tối đa 216MHz $\rightarrow$ Chọn chia 1 (`HPRE = /1`) $\implies HCLK = 216\text{ MHz}$.
- **APB1 Bus:** Hỗ trợ tối đa **54MHz** (Datasheet DS10610).
  $$\text{Nếu chia 2} \implies 216 / 2 = 108\text{ MHz} > 54\text{ MHz} \text{ (HỎNG PHẦN CỨNG!)}$$
  $$\implies \text{Bắt buộc chọn chia 4 } (\texttt{PPRE1 = /4}) \implies PCLK1 = \frac{216}{4} = \mathbf{54\text{ MHz}}.$$
- **APB2 Bus:** Hỗ trợ tối đa **108MHz** (Datasheet DS10610).
  $$\implies \text{Bắt buộc chọn chia 2 } (\texttt{PPRE2 = /2}) \implies PCLK2 = \frac{216}{2} = \mathbf{108\text{ MHz}}.$$

🎉 **Kết luận:** Bộ số hoàn hảo được cấu hình vào thanh ghi `RCC_PLLCFGR` và `RCC_CFGR`:
$$\mathbf{PLLM=25, PLLN=432, PLLP=2, PLLQ=9, HPRE=/1, PPRE1=/4, PPRE2=/2}$$

---

## 7. TỔNG KẾT VÀ BƯỚC TIẾP THEO

Bạn đã nắm vững toàn bộ các khái niệm:
1. **Memory-Mapped I/O:** Thanh ghi là ô nhớ vật lý gắn với dây điều khiển.
2. **Struct Mapping:** Ép kiểu địa chỉ thành con trỏ struct để truy xuất thanh ghi theo offset tự động.
3. **`volatile`:** Bắt buộc compiler đọc/ghi giá trị thực từ phần cứng, không tối ưu hóa mất vòng lặp.
4. **Bitwise Operations:** Kỹ thuật Clear-then-Set (RMW) an toàn cho thanh ghi đa bit.
5. **Clock Tree & PLL:** Công thức phân phối xung nhịp tới từng bus ngoại vi.

👉 **Bây giờ bạn đã sẵn sàng:** Hãy mở file [`day01_system_clock_reset.md`](file:///d:/Project/STM32F7/docs/day01_system_clock_reset.md) để đọc chi tiết bảng thanh ghi và mã nguồn driver Ngày 1!
