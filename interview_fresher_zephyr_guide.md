# Cẩm Nang Phỏng Vấn Toàn Diện: C Embedded, System Architecture & Project Deep-Dive

> **Bách khoa toàn thư phỏng vấn dành riêng cho Kỹ sư Nhúng Fresher / Junior (STM32F7 / Zephyr RTOS / Automotive).**  
> Tài liệu tích hợp trọn vẹn **5 trụ cột kiến thức then chốt**:  
> 1. **Lập trình C Embedded Thực Chiến:** Bitwise, memory layout, con trỏ hàm, struct padding, ring buffer, volatile, static pool.  
> 2. **Kiến trúc Vi điều khiển & Ngoại vi:** ARM Cortex-M, NVIC, Bootloader sequence, HardFault debug, UART, SPI, I2C, CAN, DMA.  
> 3. **Hệ điều hành RTOS & Zephyr Chuyên sâu:** Mutex, Semaphore, Queue, Priority Inversion, Devicetree, Kconfig, Driver Model, MPU Stack Guard.  
> 4. **Xoáy Sâu 4 Dự Án Trong CV:** Khảo sát từng công thức, thanh ghi, giải thuật và cơ sở phần cứng bảo vệ toàn bộ claim trong CV.  
> 5. **Tình Huống Thực Địa, Quy Chuẩn Git & Kịch Bản STAR:** Debug sự cố bus, xử lý conflict rebase, bài thuyết trình 60 giây.

---

## MỤC LỤC TỔNG QUAN

- [PHẦN 1 — LẬP TRÌNH C EMBEDDED THỰC CHIẾN (TECH TEST & LIVE-CODING)](#phần-1--lập-trình-c-embedded-thực-chiến-tech-test--live-coding)
  - [1.1. Từ khóa `volatile` và cơ chế tối ưu của Compiler](#11-từ-khóa-volatile-và-cơ-chế-tối-ưu-của-compiler)
  - [1.2. Thao tác Bitwise, Bitmask & Clear-then-Set Pattern](#12-thao-tác-bitwise-bitmask--clear-then-set-pattern)
  - [1.3. Memory Layout của chương trình C trên MCU](#13-memory-layout-của-chương-trình-c-trên-mcu)
  - [1.4. Con trỏ nâng cao & Bảng hàm Callback (Function Pointer)](#14-con-trỏ-nâng-cao--bảng-hàm-callback-function-pointer)
  - [1.5. Struct Padding, Data Alignment & `__attribute__((packed))`](#15-struct-padding-data-alignment--__attribute__packed)
  - [1.6. Xử lý Endianness (Little-Endian vs Big-Endian)](#16-xử-lý-endianness-little-endian-vs-big-endian)
  - [1.7. Quản lý Bộ nhớ Động không dùng `malloc()` (Static Memory Pool)](#17-quản-lý-bộ-nhớ-động-không-dùng-malloc-static-memory-pool)
  - [1.8. Cài đặt Circular Ring Buffer chuẩn ISR-Safe bằng C](#18-cài-đặt-circular-ring-buffer-chuẩn-isr-safe-bằng-c)
- [PHẦN 2 — KIẾN TRÚC VI ĐIỀU KHIỂN & GIAO TIẾP NGOẠI VI (CORTEX-M & PERIPHERALS)](#phần-2--kiến-trúc-vi-điều-khiển--giao-tiếp-ngoại-vi-cortex-m--peripherals)
  - [2.1. NVIC & Cơ chế phân nhóm mức ưu tiên (Priority Grouping)](#21-nvic--cơ-chế-phân-nhóm-mức-ưu-tiên-priority-grouping)
  - [2.2. SysTick Timer vs General-Purpose Timer](#22-systick-timer-vs-general-purpose-timer)
  - [2.3. Quy trình Boot từ chân Reset đến hàm `main()`](#23-quy-trình-boot-từ-chân-reset-đến-hàm-main)
  - [2.4. Quy trình gỡ lỗi bắt lỗi khi MCU rơi vào `HardFault_Handler`](#24-quy-trình-gỡ-lỗi-bắt-lỗi-khi-mcu-rơi-vào-hardfault_handler)
  - [2.5. Watchdog Timers: IWDG vs WWDG](#25-watchdog-timers-iwdg-vs-wwdg)
  - [2.6. UART: Baudrate, Oversampling 8x/16x, Framing & Parity](#26-uart-baudrate-oversampling-8x16x-framing--parity)
  - [2.7. SPI: Full-Duplex, CPOL và CPHA](#27-spi-full-duplex-cpol-và-cpha)
  - [2.8. I2C: Clock Stretching & Open-Drain Bus](#28-i2c-clock-stretching--open-drain-bus)
  - [2.9. CAN Bus: Trở đầu cuối 120 Ohm & Phân xử Trọng tài (Arbitration)](#29-can-bus-trở-đầu-cuối-120-ohm--phân-xử-trọng-tài-arbitration)
  - [2.10. DMA Controller: Polling vs Interrupt vs DMA](#210-dma-controller-polling-vs-interrupt-vs-dma)
  - [2.11. Thiết bị đo kiểm: JTAG vs SWD, Logic Analyzer vs Oscilloscope](#211-thiết-bị-đo-kiểm-jtag-vs-swd-logic-analyzer-vs-oscilloscope)
- [PHẦN 3 — HỆ ĐIỀU HÀNH THỜI GIAN THỰC & ZEPHYR RTOS CHUYÊN SÂU](#phần-3--hệ-điều-hành-thời-gian-thực--zephyr-rtos-chuyên-sâu)
  - [3.1. Phân biệt Mutex, Binary Semaphore và Message Queue](#31-phân-biệt-mutex-binary-semaphore-và-message-queue)
  - [3.2. Quy tắc vàng: CẤM Block/Sleep bên trong ngắt ISR](#32-quy-tắc-vàng-cấm-blocksleep-bên-trong-ngắt-isr)
  - [3.3. Hiện tượng Nghịch đảo Ưu tiên (Priority Inversion) & Priority Inheritance](#33-hiện-tượng-nghịch-đảo-ưu-tiên-priority-inversion--priority-inheritance)
  - [3.4. Kiến trúc Driver Model Zephyr & Macro `DEVICE_DT_INST_DEFINE`](#34-kiến-trúc-driver-model-zephyr--macro-device_dt_inst_define)
  - [3.5. Devicetree & Kconfig vs `#define` truyền thống](#35-devicetree--kconfig-vs-define-truyền-thống)
  - [3.6. Công cụ dòng lệnh Meta-tool `west`](#36-công-cụ-dòng-lệnh-meta-tool-west)
  - [3.7. Bảo vệ tràn Stack bằng phần cứng: `CONFIG_MPU_STACK_GUARD`](#37-bảo-vệ-tràn-stack-bằng-phần-cứng-config_mpu_stack_guard)
- [PHẦN 4 — XOÁY SÂU TOÀN DIỆN 4 DỰ ÁN TRONG CV (PROJECT DEEP-DIVE)](#phần-4--xoáy-sâu-toàn-diện-4-dự-án-trong-cv-project-deep-dive)
  - [4.1. DỰ ÁN 1: Automotive CAN Gateway (STM32F746 + Zephyr RTOS)](#41-dự-án-1-automotive-can-gateway-stm32f746--zephyr-rtos)
  - [4.2. DỰ ÁN 2: High-Speed Bare-Metal TFT & SDHC Player (STM32F746)](#42-dự-án-2-high-speed-bare-metal-tft--sdhc-player-stm32f746)
  - [4.3. DỰ ÁN 3: ESP32-S3 Wearable Smartwatch](#43-dự-án-3-esp32-s3-wearable-smartwatch)
  - [4.4. DỰ ÁN 4 (Internship): Hệ Thống Giám Sát & Điều Khiển Tép Bạc](#44-dự-án-4-internship-hệ-thống-giám-sát--điều-khiển-tép-bạc)
- [PHẦN 5 — TÌNH HUỐNG THỰC ĐỊA, QUY CHUẨN GIT & KỊCH BẢN PHỎNG VẤN (STAR)](#phần-5--tình-huống-thực-địa-quy-chuẩn-git--kịch-bản-phỏng-vấn-star)
  - [5.1. Xử lý sự cố Thread RTOS bị treo / không được cấp CPU](#51-xử-lý-sự-cố-thread-rtos-bị-treo--không-được-cấp-cpu)
  - [5.2. Quy trình 3 bước xử lý khi mạng CAN Bus bị nhiễu cao](#52-quy-trình-3-bước-xử-lý-khi-mạng-can-bus-bị-nhiễu-cao)
  - [5.3. Quy chuẩn Git & Giải quyết xung đột Rebase](#53-quy-chuẩn-git--giải-quyết-xung-đột-rebase)
  - [5.4. Kịch bản giới thiệu bản thân 60 giây (Chuẩn phương pháp STAR)](#54-kịch-bản-giới-thiệu-bản-thân-60-giây-chuẩn-phương-pháp-star)

---

# PHẦN 1 — LẬP TRÌNH C EMBEDDED THỰC CHIẾN (TECH TEST & LIVE-CODING)

### 1.1. Từ khóa `volatile` và cơ chế tối ưu của Compiler

#### ❓ Câu hỏi: "Bản chất của `volatile` là gì? Tại sao thiếu nó thì code đọc thanh ghi hoặc nhận ngắt ISR sẽ chạy sai khi bật cờ tối ưu `-O2` / `-O3`?"
* **Bản chất**: `volatile` thông báo cho trình biên dịch (Compiler) rằng giá trị của ô nhớ có thể bị thay đổi bất kỳ lúc nào bởi phần cứng (Hardware register), một ngắt (ISR), hoặc một luồng thực thi song song khác. Compiler **tuyệt đối không được tối ưu hóa** biến này (cấm lưu tạm giá trị vào thanh ghi CPU R0-R12 và cấm xóa bỏ các lệnh đọc/ghi ô nhớ tưởng chừng thừa thãi).
* **3 trường hợp bắt buộc trong Embedded**:
  1. Con trỏ trỏ tới thanh ghi phần cứng ngoại vi (Memory-Mapped I/O):
     ```c
     #define USART1_ISR (*(volatile uint32_t *)0x4001101CUL)
     ```
  2. Biến cờ (Flag) hoặc biến dữ liệu toàn cục được thay đổi trong ISR và đọc trong `main()`:
     ```c
     volatile uint8_t g_rx_flag = 0;
     ```
  3. Biến dùng để tạo delay thô bằng vòng lặp (nếu không có `volatile`, compiler sẽ tối ưu xóa sổ cả vòng `for`):
     ```c
     for (volatile uint32_t i = 0; i < 100000; i++);
     ```
* **Mổ xẻ Assembly khi thiếu `volatile`**:
  ```c
  /* Code C */
  while (!g_rx_flag);
  ```
  * **Khi có `volatile`**: Mỗi chu kỳ lặp, CPU đều phát lệnh `LDR R0, [g_rx_flag]` để đọc trực tiếp từ RAM.
  * **Khi thiếu `volatile` (với `-O2`)**: Compiler đọc `g_rx_flag` vào thanh ghi `R0` một lần duy nhất trước vòng lặp. Vì bên trong thân vòng lặp không có lệnh nào sửa `g_rx_flag`, Compiler sinh ra mã:
    ```assembly
    LDR  R0, =g_rx_flag
    LDR  R1, [R0]        ; Đọc RAM 1 lần
    CMP  R1, #0
    BNE  .exit_loop
    .loop:
    B    .loop           ; Nhảy tại chỗ vĩnh viễn (Treo máy!)
    ```
    Dù ISR có đổi `g_rx_flag = 1` trong RAM thì CPU vẫn loop vô tận trên thanh ghi `R1`.

---

### 1.2. Thao tác Bitwise, Bitmask & Clear-then-Set Pattern

#### ❓ Câu hỏi: "Viết bộ Macro thao tác bit chuẩn mực và giải thích tại sao khi cấu hình trường nhiều bit bắt buộc phải dùng mẫu Clear-then-Set?"

```c
#define SET_BIT(REG, BIT)          ((REG) |= (1UL << (BIT)))
#define CLEAR_BIT(REG, BIT)        ((REG) &= ~(1UL << (BIT)))
#define TOGGLE_BIT(REG, BIT)       ((REG) ^= (1UL << (BIT)))
#define READ_BIT(REG, BIT)         (((REG) >> (BIT)) & 1UL)

/* Mẫu Clear-then-Set an toàn cho trường nhiều bit (Multi-bit field) */
#define MODIFY_REG(REG, CLEAR_MASK, SET_VAL, BIT_POS) \
    ((REG) = ((REG) & ~((CLEAR_MASK) << (BIT_POS))) | (((SET_VAL) & (CLEAR_MASK)) << (BIT_POS)))
```

* **Tại sao bắt buộc phải Clear trước khi Set?**
  * Giả sử trường cấu hình Mode 2-bit nằm ở vị trí bit [5:4] của thanh ghi `GPIOx_MODER`. Giá trị Reset mặc định đang là `0b11` (Analog mode).
  * Nếu bạn muốn chuyển sang `0b01` (General Purpose Output) mà chỉ dùng phép toán OR:
    $$\text{REG} = \text{REG} \mid (0b01 \ll 4) = 0b11 \mid 0b01 = 0b11 \implies \text{Vẫn là Analog!}$$
  * Phép OR **chỉ có thể biến 0 thành 1, không thể biến 1 thành 0**. Vì vậy, bắt buộc phải xóa sạch trường bit đó về `0b00` trước bằng phép AND với NOT mask, sau đó mới OR với giá trị mới.

#### ❓ Live-Coding Test: "Viết hàm đảo ngược thứ tự các bit của một số 32-bit (Bit Reversal) không dùng hàm thư viện."
```c
uint32_t reverse_bits(uint32_t n) {
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result <<= 1;          // Dịch trái kết quả để lấy chỗ cho bit mới
        result |= (n & 1UL);   // Lấy bit thấp nhất của n đưa vào kết quả
        n >>= 1;               // Dịch phải n để xét bit kế tiếp
    }
    return result;
}
/* Trên lõi ARM Cortex-M, có thể thay bằng 1 lệnh Assembly: */
/* __asm volatile ("rbit %0, %1" : "=r"(result) : "r"(n)); */
```

---

### 1.3. Memory Layout của chương trình C trên MCU

#### ❓ Câu hỏi: "Trình bày các phân vùng bộ nhớ của chương trình C trong vi điều khiển. Chỉ rõ 6 biến sau nằm ở đâu?"

```text
Địa chỉ cao (RAM)
┌───────────────────────────────┐
│             STACK             │ <--- Biến cục bộ, địa chỉ hàm, ISR frame (phát triển xuống)
├───────────────────────────────┤
│               ↓               │
│               ↑               │
├───────────────────────────────┤
│             HEAP              │ <--- malloc / free (phát triển lên)
├───────────────────────────────┤
│             .bss              │ <--- Biến toàn cục / static chưa khởi tạo (= 0)
├───────────────────────────────┤
│             .data             │ <--- Biến toàn cục / static đã khởi tạo khác 0
└───────────────────────────────┘
Địa chỉ thấp (RAM)

Bộ nhớ FLASH ROM (Non-volatile):
┌───────────────────────────────┐
│     .data initialization      │ <--- Giá trị khởi tạo nạp xuống RAM lúc boot
├───────────────────────────────┤
│            .rodata            │ <--- Hằng số const, chuỗi ký tự
├───────────────────────────────┤
│       .text (Code/Vector)     │ <--- Mã máy thực thi, Vector Table
└───────────────────────────────┘
```

* **Xác định vị trí từng biến**:
  1. `const int a = 100;` $\to$ Nằm ở **`.rodata` (Flash ROM)**.
  2. `static int b = 20;` $\to$ Nằm ở **`.data` (RAM)** (Giá trị 20 lưu trong Flash, được startup copy vào RAM).
  3. `static int c;` $\to$ Nằm ở **`.bss` (RAM)** (Startup code xóa về 0).
  4. `int d = 30;` (nằm trong thân hàm) $\to$ Nằm ở **`Stack` (RAM)** (Được cấp phát khi hàm được gọi và giải phóng khi thoát hàm).
  5. `char *p = "Hello";` $\to$ Bản thân biến con trỏ `p` (4 bytes) nằm ở **`Stack`** (hoặc `.data` nếu khai báo toàn cục), nhưng chuỗi ký tự `"Hello"` nằm ở **`.rodata` (Flash)**.
  6. `int *ptr = malloc(10 * sizeof(int));` $\to$ Biến `ptr` nằm ở **`Stack`**, vùng nhớ 40 bytes được cấp phát nằm ở **`Heap` (RAM)**.

---

### 1.4. Con trỏ nâng cao & Bảng hàm Callback (Function Pointer)

#### ❓ Câu hỏi: "Phân biệt `const int *p`, `int * const p`, `const int * const p`. Cho ví dụ thực tế dùng Function Pointer để xây dựng Driver Callback."
* **Phân biệt cú pháp**:
  * `const int *p`: Con trỏ trỏ tới dữ liệu hằng. Không thể sửa nội dung `*p = 5` (lỗi biên dịch), nhưng con trỏ có thể đổi sang trỏ địa chỉ khác `p = &other`.
  * `int * const p`: Con trỏ hằng trỏ tới dữ liệu biến đổi. Không thể đổi địa chỉ trỏ `p = &other`, nhưng có thể sửa nội dung `*p = 10`. *(Ví dụ: Thanh ghi ngoại vi vi điều khiển là con trỏ hằng)*.
  * `const int * const p`: Cả địa chỉ con trỏ lẫn dữ liệu ô nhớ đều là hằng số cố định, không thể thay đổi.
* **Mẫu thiết kế Driver Callback dùng Function Pointer**:
  ```c
  /* Định nghĩa kiểu con trỏ hàm xử lý khung tin CAN nhận được */
  typedef void (*can_rx_callback_t)(uint32_t can_id, const uint8_t *data, uint8_t dlc);

  typedef struct {
      CAN_TypeDef *instance;
      can_rx_callback_t rx_cb; /* Con trỏ hàm callback */
  } can_driver_handle_t;

  /* Đăng ký Callback */
  void CAN_RegisterRxCallback(can_driver_handle_t *hcan, can_rx_callback_t callback) {
      if (hcan != NULL) {
          hcan->rx_cb = callback;
      }
  }

  /* Trong hàm phục vụ ngắt phần cứng (ISR) */
  void CAN1_RX0_IRQHandler(void) {
      if (CAN1->RF0R & CAN_RF0R_FMP0) {
          uint32_t id = (CAN1->sFIFOMailBox[0].RIR >> 21);
          uint8_t data[8];
          if (g_hcan1.rx_cb != NULL) {
              g_hcan1.rx_cb(id, data, 8);
          }
          CAN1->RF0R |= CAN_RF0R_RFOM0; /* Release FIFO0 */
      }
  }
  ```

---

### 1.5. Struct Padding, Data Alignment & `__attribute__((packed))`

#### ❓ Câu hỏi: "Tại sao `sizeof(struct { char a; int b; short c; })` lại ra 12 bytes trên ARM Cortex-M? Hậu quả gì xảy ra nếu không thêm `__attribute__((packed))` khi gửi struct qua CAN/UART?"
* **Cơ chế căn lề tự nhiên (Data Alignment)**:
  * Vi xử lý 32-bit Cortex-M tối ưu hóa truy cập dữ liệu khi biến $N$-byte nằm ở địa chỉ bộ nhớ chia hết cho $N$.
  * Trường `char a` (1 byte) nằm tại Offset 0.
  * Trường `int b` (4 bytes) bắt buộc phải nằm ở địa chỉ chia hết cho 4 $\implies$ Compiler tự chèn **3 bytes đệm (Padding Bytes)** tại Offset 1, 2, 3. Biến `b` chiếm Offset 4, 5, 6, 7.
  * Trường `short c` (2 bytes) nằm tại Offset 8, 9.
  * Toàn bộ struct có thành viên lớn nhất là 4 bytes $\implies$ Kích thước tổng phải chia hết cho 4 $\implies$ Chèn thêm **2 bytes padding** ở Offset 10, 11.
  * **Tổng cộng: $1 + 3\text{ (pad)} + 4 + 2 + 2\text{ (pad)} = 12\text{ bytes}$**.
* **Hậu quả trên bus truyền thông (CAN / UART)**:
  * Khung truyền CAN tiêu chuẩn chỉ có tối đa 8 bytes dữ liệu. Nếu bạn map trực tiếp một struct không có `packed`, các byte rác padding sẽ bị gửi lên bus, làm sai lệch toàn bộ vị trí offset dữ liệu phía máy nhận và vượt quá dung lượng 8 bytes.
  * Bắt buộc dùng `__attribute__((packed))` để kích thước struct đúng bằng $1 + 4 + 2 = 7\text{ bytes}$.

---

### 1.6. Xử lý Endianness (Little-Endian vs Big-Endian)

#### ❓ Câu hỏi: "Viết hàm C phát hiện kiến trúc vi điều khiển là Little hay Big Endian. Viết macro hoán đổi byte cho số 16-bit và 32-bit."

```c
/* Cách kiểm tra bằng con trỏ */
int is_little_endian(void) {
    uint16_t test = 0x0001;
    uint8_t *byte_ptr = (uint8_t *)&test;
    return (*byte_ptr == 0x01); /* Nếu byte đầu là 0x01 -> Little Endian */
}

/* Macro hoán đổi byte (Byte Swapping) */
#define SWAP_UINT16(x)  ((((x) & 0x00FFU) << 8) | (((x) & 0xFF00U) >> 8))

#define SWAP_UINT32(x)  ((((x) & 0x000000FFUL) << 24) | \
                         (((x) & 0x0000FF00UL) << 8)  | \
                         (((x) & 0x00FF0000UL) >> 8)  | \
                         (((x) & 0xFF000000UL) >> 24))
```

---

### 1.7. Quản lý Bộ nhớ Động không dùng `malloc()` (Static Memory Pool)

#### ❓ Câu hỏi: "Tại sao trong Firmware tiêu chuẩn ô tô (MISRA-C / ISO 26262) lại cấm `malloc()`? Làm sao cấp phát động an toàn?"
* **3 hiểm họa của `malloc()` / `free()` trong hệ thống nhúng**:
  1. **Phân mảnh bộ nhớ (Heap Fragmentation)**: Sau thời gian dài chạy liên tục, các khối nhớ nhỏ xen kẽ khiến hệ thống không tìm được khối nhớ liên tục đủ lớn.
  2. **Thời gian thực thi không tiền định (Non-deterministic timing)**: Thuật toán tìm khối trống tốn chu kỳ CPU khác nhau tùy trạng thái heap.
  3. **Rò rỉ bộ nhớ (Memory Leak)**: Quên gọi `free()` hoặc thất bại cấp phát (`NULL pointer`) gây HardFault.
* **Giải pháp: Fixed-Block Static Memory Pool**:
  ```c
  #define POOL_BLOCK_SIZE  64
  #define POOL_NUM_BLOCKS  16

  typedef struct {
      uint8_t memory[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
      uint16_t allocated_mask; /* Bitmask quản lý 16 block (1 = đã dùng, 0 = rảnh) */
  } memory_pool_t;

  static memory_pool_t s_pool = { .allocated_mask = 0 };

  void* pool_alloc(void) {
      for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
          if (!(s_pool.allocated_mask & (1U << i))) {
              s_pool.allocated_mask |= (1U << i);
              return (void *)s_pool.memory[i];
          }
      }
      return NULL; /* Hết bộ đệm */
  }

  void pool_free(void *ptr) {
      if (ptr == NULL) return;
      uint32_t offset = (uint8_t *)ptr - (uint8_t *)s_pool.memory;
      int index = offset / POOL_BLOCK_SIZE;
      if (index >= 0 && index < POOL_NUM_BLOCKS) {
          s_pool.allocated_mask &= ~(1U << index);
      }
  }
  ```

---

### 1.8. Cài đặt Circular Ring Buffer chuẩn ISR-Safe bằng C

#### ❓ Câu hỏi: "Cài đặt một Ring Buffer bằng C thuần phục vụ nhận ngắt UART. Làm sao để đảm bảo Thread-Safe giữa ISR và `main()` mà không cần dùng Mutex?"

```c
#define RING_BUFFER_SIZE  128 /* Lũy thừa của 2 để tối ưu phép chia % thành & MASK */
#define RING_BUFFER_MASK  (RING_BUFFER_SIZE - 1)

typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head; /* Chỉ ISR ghi */
    volatile uint16_t tail; /* Chỉ main() đọc */
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool ring_buffer_push(ring_buffer_t *rb, uint8_t data) {
    uint16_t next_head = (rb->head + 1) & RING_BUFFER_MASK;
    if (next_head == rb->tail) return false; /* Buffer đầy */
    rb->buffer[rb->head] = data;
    rb->head = next_head;
    return true;
}

bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data) {
    if (rb->head == rb->tail) return false; /* Buffer rỗng */
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) & RING_BUFFER_MASK;
    return true;
}
```

---

# PHẦN 2 — KIẾN TRÚC VI ĐIỀU KHIỂN & GIAO TIẾP NGOẠI VI (CORTEX-M & PERIPHERALS)

### 2.1. NVIC & Cơ chế phân nhóm mức ưu tiên (Priority Grouping)
* **NVIC (Nested Vectored Interrupt Controller)**: Khối điều khiển ngắt độc quyền trên ARM Cortex-M hỗ trợ ngắt lồng nhau và thời gian trễ ngắt cực thấp.
* **Preemption Priority vs Subpriority**:
  * **Preemption Priority**: Ưu tiên cắt ngang. Nếu ngắt mới có Preemption Priority cao hơn (số nhỏ hơn), nó sẽ đình chỉ ngắt hiện tại để nhảy vào xử lý.
  * **Subpriority**: Ưu tiên xếp hàng. Nếu 2 ngắt có cùng Preemption Priority xảy ra đồng thời, ngắt có Subpriority cao hơn sẽ được phục vụ trước (không thể cắt ngang nhau).

---

### 2.2. SysTick Timer vs General-Purpose Timer
* **SysTick Timer**: Bộ đếm lùi 24-bit nằm ngay bên trong nhân vi xử lý ARM Cortex, dùng chung cho mọi dòng chip Cortex-M. Mục đích cốt lõi là tạo nhịp đếm định thời cố định (thường là 1ms) cho hệ điều hành RTOS (OS Tick).
* **General-Purpose Timer (TIMx)**: Bộ định thời ngoại vi do hãng bán dẫn (STMicroelectronics) tích hợp trên bus APB, có độ rộng 16-bit hoặc 32-bit, hỗ trợ các tính năng phần cứng nâng cao như PWM, Input Capture, Encoder Interface.

---

### 2.3. Quy trình Boot từ chân Reset đến hàm `main()`
1. Phần cứng đọc địa chỉ `0x0000_0000` (ánh xạ từ Flash `0x0800_0000`): Nạp giá trị vào con trỏ ngăn xếp chính **MSP (Main Stack Pointer)**.
2. Phần cứng đọc địa chỉ `0x0000_0004`: Nạp địa chỉ hàm **Reset_Handler** vào thanh ghi **PC (Program Counter)**.
3. CPU bắt đầu thực thi hàm `Reset_Handler` trong file startup:
   * Copy toàn bộ dữ liệu phân vùng `.data` từ Flash sang RAM.
   * Xóa sạch (Zero-fill) toàn bộ phân vùng `.bss` trong RAM.
   * Gọi hàm `SystemInit()` cấu hình xung nhịp ban đầu và bật khối tính toán FPU.
4. Gọi lệnh nhảy sang hàm `main()`.

---

### 2.4. Quy trình gỡ lỗi bắt lỗi khi MCU rơi vào `HardFault_Handler`
1. **Đọc thanh ghi lỗi SCB**:
   * Kiểm tra `SCB->HFSR` (HardFault Status Register) xem lỗi do cưỡng bức hay bảng vector.
   * Kiểm tra `SCB->CFSR` (Configurable Fault Status Register): Phân tích chi tiết lỗi `MemManage Fault` (truy cập vùng nhớ cấm), `BusFault` (truy cập bus lỗi), hoặc `UsageFault` (`DIVBYZERO` chia cho 0, `UNALIGN` truy cập lệch byte).
2. **Đọc Stacked Registers trên Stack**:
   * Khi nhảy vào ngắt, phần cứng tự động lưu 8 thanh ghi: `R0, R1, R2, R3, R12, LR, PC, xPSR`.
   * Trích xuất giá trị **`PC`** trên Stack để xác định chính xác địa chỉ dòng lệnh Assembly gây sập hệ thống.

---

### 2.5. Watchdog Timers: IWDG vs WWDG
* **IWDG (Independent Watchdog)**: Sử dụng xung nhịp nội độc lập riêng biệt **LSI 32 kHz**, hoạt động hoàn toàn tách rời xung nhịp hệ thống chính. Dùng làm phòng tuyến an toàn cuối cùng chống treo máy (kể cả khi thạch anh chính HSE bị chết).
* **WWDG (Window Watchdog)**: Chạy theo xung nhịp bus APB1. Yêu cầu phần mềm phải làm tươi trong một "cửa sổ thời gian" chính xác (Refresh quá sớm hoặc quá muộn đều kích hoạt Reset MCU). Dùng để phát hiện các luồng code bị chạy sai lệch nhịp thời gian.

---

### 2.6. UART: Baudrate, Oversampling 8x/16x, Framing & Parity
* **Baudrate**: Tốc độ truyền bit trên đường dây ($Baud = \frac{f_{CK}}{8 \times (2 - OVER8) \times USARTDIV}$).
* **Oversampling 16x vs 8x**: Lấy mẫu tín hiệu 16 lần hoặc 8 lần trong 1 bit để tìm điểm giữa bit ổn định nhất nhằm chống rung pha (Jitter). Chế độ 8x cho phép đạt tốc độ baud cao hơn nhưng khả năng chịu nhiễu kém hơn 16x.
* **Framing Error (FE)**: Xảy ra khi bộ thu kiểm tra tại vị trí Stop bit nhưng không thấy mức điện áp cao ('1') như quy chuẩn (thường do lệch baudrate giữa 2 thiết bị).

---

### 2.7. SPI: Full-Duplex, CPOL và CPHA
* **Full-Duplex**: Khả năng truyền (MOSI) và nhận (MISO) đồng thời trên cùng một chu kỳ xung clock SCK.
* **CPOL (Clock Polarity)**: Mức logic của chân SCK khi bus ở trạng thái nghỉ (`CPOL=0`: Nghỉ mức 0; `CPOL=1`: Nghỉ mức 1).
* **CPHA (Clock Phase)**: Cạnh xung dùng để chốt mẫu dữ liệu (`CPHA=0`: Chốt mẫu ở cạnh đầu tiên; `CPHA=1`: Chốt mẫu ở cạnh thứ hai).

---

### 2.8. I2C: Clock Stretching & Open-Drain Bus
* **Open-Drain Bus**: Cần có điện trở kéo lên nguồn ($4.7\text{ k}\Omega \sim 10\text{ k}\Omega$) trên 2 đường SDA và SCL. Các thiết bị chỉ được phép kéo đường dây xuống mức thấp ('0') hoặc thả nổi để điện trở kéo lên mức cao ('1').
* **Clock Stretching**: Cơ chế cho phép thiết bị Slave tạm thời giữ đường SCL ở mức thấp để làm chậm Master khi Slave đang bận xử lý dữ liệu và chưa sẵn sàng nhận tiếp.

---

### 2.9. CAN Bus: Trở đầu cuối 120 Ohm & Phân xử Trọng tài (Arbitration)
* **Điện trở 120 Ohm**: Đường dây vi sai CAN là một Transmission Line. Hai trở $120\ \Omega$ mắc song song ở 2 đầu tạo nên tổng trở bus tương đương $60\ \Omega$, giúp triệt tiêu sóng phản xạ (Signal Reflection) gây biến dạng dạng sóng xung.
* **Dominant (0) vs Recessive (1)**: Dominant có chênh lệch điện áp $V_{diff} \approx 2\text{V}$, Recessive có $V_{diff} \approx 0\text{V}$. Khi một node phát 0 và một node phát 1, trạng thái trên bus sẽ là 0. Do đó, **CAN ID số càng nhỏ thì độ ưu tiên càng cao**.

---

### 2.10. DMA Controller: Polling vs Interrupt vs DMA
* **Polling**: CPU chạy vòng lặp liên tục chờ cờ phần cứng $\implies$ Chiếm dụng 100% CPU.
* **Interrupt**: Mỗi byte nhận/gửi đều kích hoạt ngắt $\implies$ Chi phí context switch lớn, dễ nghẽn CPU khi tốc độ truyền cao.
* **DMA**: Khối phần cứng độc lập tự động chuyển cả khối dữ liệu giữa Ngoại vi và Bộ nhớ mà không cần CPU $\implies$ CPU hoàn toàn rảnh tay làm việc khác.

---

### 2.11. Thiết bị đo kiểm: JTAG vs SWD, Logic Analyzer vs Oscilloscope
* **JTAG vs SWD**: JTAG dùng 4-5 chân, SWD chỉ dùng 2 chân (`SWDIO`, `SWCLK`) nhưng tốc độ ngang ngửa và tiết kiệm chân IO.
* **Logic Analyzer vs Oscilloscope**: Logic Analyzer chuyên bắt và giải mã dữ liệu số logic (0/1) trên nhiều kênh (UART, SPI, CAN). Oscilloscope đo điện áp tương tự theo thời gian thực để kiểm tra tính toàn vẹn tín hiệu, sụt áp, nhiễu và méo xung.

---

# PHẦN 3 — HỆ ĐIỀU HÀNH THỜI GIAN THỰC & ZEPHYR RTOS CHUYÊN SÂU

### 3.1. Phân biệt Mutex, Binary Semaphore và Message Queue
| Cơ chế | Mục đích chính | Khái niệm Ownership | Dùng trong ISR |
|---|---|---|---|
| **Mutex** | Bảo vệ tài nguyên chia sẻ | **CÓ**: Thread nào Lock thì chính Thread đó phải Unlock | **CẤM HOÀN TOÀN** |
| **Binary Semaphore** | Báo hiệu sự kiện (Signaling) | **KHÔNG**: ISR có thể Give, Task khác Take | **ĐƯỢC PHÉP** |
| **Message Queue** | Truyền dữ liệu an toàn kèm đồng bộ | Quản lý bộ đệm FIFO | Được phép Put với cờ No Wait |

---

### 3.2. Quy tắc vàng: CẤM Block/Sleep bên trong ngắt ISR
* **Nguyên nhân**: ISR chạy trong Interrupt Context của CPU, hoàn toàn **không có Thread Control Block (TCB) riêng để lưu ngữ cảnh**. Nếu gọi hàm gây Block (`k_msleep()`, `k_mutex_lock()`), hệ điều hành không thể chuyển giao ngữ cảnh, dẫn đến Kernel Panic hoặc treo cứng vi điều khiển.

---

### 3.3. Hiện tượng Nghịch đảo Ưu tiên (Priority Inversion) & Priority Inheritance
* **Hiện tượng**: Task ưu tiên cao (Task H) bị chặn bởi Task ưu tiên thấp (Task L) do Task L đang giữ Mutex. Một Task trung bình (Task M) không cần Mutex nhưng ưu tiên cao hơn Task L sẽ chiếm quyền CPU của Task L, gián tiếp làm Task H bị trễ vô thời hạn.
* **Giải pháp (Priority Inheritance)**: Khi Task H chờ Mutex, RTOS tạm thời nâng mức ưu tiên của Task L lên bằng Task H để Task M không thể xen ngang. Khi Task L nhả Mutex, ưu tiên của nó hạ về ban đầu và Task H lập tức chiếm quyền xử lý.

---

### 3.4. Kiến trúc Driver Model Zephyr & Macro `DEVICE_DT_INST_DEFINE`
* Zephyr tách biệt phần cứng ra file Devicetree (`.dts` / `.overlay`).
* Macro `DEVICE_DT_INST_DEFINE` khởi tạo cấu trúc `struct device` tại thời điểm biên dịch, đăng ký hàm Init và tham số cấu hình phần cứng theo thứ tự ưu tiên (`POST_KERNEL`), giúp mã ứng dụng gọi các API chuẩn hóa (`can_send`, `uart_poll_in`) mà không phụ thuộc vào dòng vi điều khiển cụ thể.

---

### 3.5. Devicetree & Kconfig vs `#define` truyền thống
* **Devicetree**: Mô tả cấu hình phần cứng (chân cẳng, địa chỉ cơ sở, IRQ) bằng cấu trúc cây, độc lập hoàn toàn với mã C.
* **Kconfig**: Hệ thống quản lý cấu hình biên dịch kế thừa từ Linux Kernel. Có khả năng tự động kiểm tra tính phụ thuộc (Dependency Checking), chỉ biên dịch những file thực sự được kích hoạt, tối ưu dung lượng Flash/RAM.

---

### 3.6. Công cụ dòng lệnh Meta-tool `west`
* Đảm nhận 3 vai trò:
  1. Quản lý đa kho lưu trữ git qua file `west.yml` (`west init`, `west update`).
  2. Kích hoạt chuỗi build CMake + Ninja (`west build -b stm32f746g_disco app`).
  3. Giao tiếp với mạch nạp ST-Link / OpenOCD để nạp code (`west flash`).

---

### 3.7. Bảo vệ tràn Stack bằng phần cứng: `CONFIG_MPU_STACK_GUARD`
* Zephyr sử dụng bộ bảo vệ bộ nhớ MPU của ARM Cortex cấu hình một vùng đệm nhỏ (32 bytes) ở đáy mỗi Stack thành vùng cấm ghi (Read-Only). Ngay khi Thread bị tràn stack, lệnh ghi vào vùng này sẽ lập tức kích hoạt `MemManage Fault`, hệ thống bắt gọn lỗi tràn stack trước khi nó kịp làm hỏng dữ liệu của Thread khác.

---

# PHẦN 4 — XOÁY SÂU TOÀN DIỆN 4 DỰ ÁN TRONG CV (PROJECT DEEP-DIVE)

---

## 4.1. DỰ ÁN 1: Automotive CAN Gateway (STM32F746 + Zephyr RTOS)

### ❓ Câu 1: "Với xung nhịp bus APB1 là 54 MHz, tính toán chi tiết Prescaler, BS1, BS2 và SJW để đạt tốc độ chuẩn 500 kbps với điểm lấy mẫu 83.33%?"
* **Cơ sở phần cứng**: Trên STM32F746 chạy tối đa 216MHz, bus APB1 cấp xung cho khối ngoại vi CAN1 tối đa là $f_{APB1} = 54\text{ MHz}$.
* **Chuẩn hóa tốc độ 500 kbps**:
  * Chu kỳ một bit: $T_{bit} = \frac{1}{500,000} = 2000\text{ ns}$.
  * Chọn tổng số Time Quanta ($t_q$) cho 1 bit: $N = 18\ t_q$.
  * Tần số chia baud: $f_{tq} = 500\text{ kbps} \times 18 = 9\text{ MHz}$.
  * Hệ số chia Prescaler:
    $$\text{BRP} = \frac{f_{APB1}}{f_{tq}} = \frac{54\text{ MHz}}{9\text{ MHz}} = 6$$
* **Phân bổ các đoạn trong 1 bit theo khuyến nghị CiA 301**:
  * $\text{Sync\_Seg} = 1\ t_q$ (Bắt buộc theo chuẩn CAN).
  * Điểm lấy mẫu (Sample Point) lý thuyết: $87.5\%$. Với $N=18$:
    $$\text{Sync\_Seg} + \text{BS1} = 18 \times 0.875 = 15.75 \approx 15\ t_q \implies \text{BS1} = 14\ t_q$$
  * Đoạn đệm 2: $\text{BS2} = 18 - (\text{Sync\_Seg} + \text{BS1}) = 18 - 15 = 3\ t_q$.
  * Điểm lấy mẫu thực tế: $\frac{1 + 14}{18} = \frac{15}{18} = 83.33\%$.
  * Bước nhảy bù đồng bộ: $\text{SJW} = 1\ t_q$.
* **Cấu hình thanh ghi `CAN_BTR`**:
  * `BRP = 6 - 1 = 5` (bit 0..9).
  * `TS1 = 14 - 1 = 13` (bit 16..19).
  * `TS2 = 3 - 1 = 2` (bit 20..22).
  * `SJW = 1 - 1 = 0` (bit 24..25).

---

### ❓ Câu 2: "Show một đoạn DeviceTree Overlay thật cho bxCAN trong Zephyr và giải thích từng thuộc tính."

```dts
/* File: app.overlay */
&can1 {
    status = "okay";                        /* Kích hoạt node can1 */
    pinctrl-0 = <&can1_rx_pb8 &can1_tx_pb9>;/* Ghép kênh chân phần cứng: PB8 RX, PB9 TX */
    pinctrl-names = "default";
    bus-speed = <500000>;                   /* Tốc độ bitrate 500 kbps */
    sample-point = <833>;                   /* Điểm lấy mẫu 83.3% */
    
    transceiver {
        status = "okay";
    };
};
```
* **`pinctrl-0`**: Tự động cấu hình thanh ghi GPIO Alternate Function `AF9` cho PB8/PB9 thông qua hệ thống Pinctrl của Zephyr tại thời điểm khởi động trước `main()`.
* **`bus-speed` & `sample-point`**: Zephyr CAN driver tự động tính toán thanh ghi `CAN_BTR` tối ưu từ các giá trị này dựa trên xung nhịp bus clock được khai báo trong cây Devicetree gốc của STM32F7.

---

### ❓ Câu 3: "Async CAN reception với 6 hardware filter bank vào k_msgq: Filter mode dùng là Mask hay List? Tại sao chọn 6?"
* **Cơ chế phối hợp**: Sử dụng kết hợp **Identifier List Mode** và **Identifier Mask Mode**:
  * **2 Filter Banks ở chế độ List Mode (32-bit)**: Dùng để bắt chính xác 4 ID khẩn cấp và chẩn đoán cố định: `0x100` (Phanh khẩn cấp Emergency Brake), `0x102` (Túi khí Airbag), `0x7DF` (OBD-II Broadcast Request) và `0x7E8` (ECU Response).
  * **4 Filter Banks ở chế độ Mask Mode (32-bit)**: Dùng để bắt các dải dữ liệu Telemetry của xe:
    * Bank 2: ID `0x200` với Mask `0x7F0` (Bắt toàn bộ dải động cơ `0x200 - 0x20F`: RPM, Throttle, Torque).
    * Bank 3: ID `0x300` với Mask `0x7F0` (Bắt dải thân xe: Đèn, Cửa, Cần gạt nước).
    * Bank 4 & 5: Dự phòng cho mạng pin EV / Battery BMS (`0x400 - 0x41F`).
* **Tại sao chọn đúng 6 Banks?**
  * Trên STM32F746, phần cứng CAN1 là Master quản lý chung 28 Filter Banks cho cả CAN1 và CAN2 (`CAN1->FMR` bit `CAN2SB`).
  * Cấp phát đúng 6 Banks cho CAN1 vừa đủ cô lập các gói tin cần giám sát, vừa tiết kiệm 22 Banks còn lại cho kênh CAN2, đồng thời loại bỏ 100% các bản tin CAN rác ngay từ tầng phần cứng (Hardware Filtering), không cho chúng sinh ngắt làm phiền CPU.

---

### ❓ Câu 4: "AUTOSAR E2E Profile 1 — Polynomial CRC-8 em dùng là gì? Tại sao chọn Profile 1?"
* **Đa thức CRC-8 chuẩn AUTOSAR E2E Profile 1**:
  $$\text{Polynomial} = 0x1D \quad (x^8 + x^4 + x^3 + x^2 + 1), \quad \text{Init} = 0xFF, \quad \text{XOR\_OUT} = 0xFF$$
* **Cấu trúc gói tin E2E Profile 1 (8 bytes)**:
  * Byte 0..6: Dữ liệu tải trọng (Payload).
  * Byte 7: 4-bit thấp là **Rolling Counter (0 đến 15)**, 4-bit cao là **Data ID nibble**.
  * Byte cuối: Chứa mã kiểm tra **CRC-8**.
* **Tại sao chọn Profile 1?**:
  * Tối ưu hóa riêng cho các mạng CAN truyền thống có chiều dài tải trọng cố định 8 bytes.
  * Chi phí tính toán cực thấp (chỉ dùng bảng tra cứu 256 phần tử `CRC_Table`), thực thi chỉ tốn dưới 1.5 microsecond trên lõi Cortex-M7 216MHz, đủ đáp ứng an toàn chức năng mà không làm nghẽn hàng đợi.

---

### ❓ Câu 5: "Cơ chế ISO 11898-1 Bus-Off Recovery trong 100ms: Phát hiện bằng cách nào và khôi phục cụ thể ra sao?"
* **Phát hiện**:
  * Khi vi điều khiển phát sinh lỗi truyền dẫn liên tục, bộ đếm lỗi truyền `TEC` (Transmit Error Counter) vượt quá 255. Phần cứng CAN tự động ngắt kết nối khỏi mạng để bảo vệ bus.
  * Driver phát hiện bằng cách đọc thanh ghi `CAN_ESR` bit `BOFF` (`CAN_ESR_BOFF = 1`) hoặc thông qua callback `can_set_state_change_callback()` của Zephyr chuyển sang trạng thái `CAN_STATE_BUS_OFF`.
* **Cơ chế phục hồi theo ISO 11898-1**:
  * Tiêu chuẩn bắt buộc node phải giám sát bus và ghi nhận đủ **128 lần xuất hiện của 11 bit lặn (Recessive bits) liên tiếp** mới được phép kích hoạt lại.
  * **Giải pháp trong Driver**:
    1. Khi phát hiện Bus-Off, chuyển máy trạng thái FSM sang `RECOVERING`.
    2. Kích hoạt timer đơn chu kỳ 100ms.
    3. Thiết lập bit `INRQ` trong `CAN_MCR` để vào chế độ Init, sau đó xóa `INRQ` để phần cứng tự động đếm 128 chuỗi 11 bit lặn.
    4. Khi hoàn tất, cờ `BOFF` tự động xóa, FSM phục hồi về trạng thái `ERROR_ACTIVE` và tiếp tục truyền nhận bình thường.

---

### ❓ Câu 6: "Xử lý 12 vehicle signal bằng Fixed-Point: Công thức chuyển đổi raw CAN data $\to$ giá trị vật lý là gì? Cho ví dụ cụ thể."
* **Công thức chuẩn Vector DBC**:
  $$\text{Physical\_Value} = (\text{Raw\_Value} \times \text{Factor}) + \text{Offset}$$
* **Ví dụ Signal Tốc độ xe (Vehicle Speed)**:
  * Chiều dài: 16 bits (Byte 0 và Byte 1).
  * Factor = $0.01\text{ km/h}$, Offset = $0$.
  * **Giải pháp Fixed-Point (Tránh dùng `float`)**:
    ```c
    /* Dùng số nguyên định điểm tỉ lệ 100 (Centi-km/h): */
    uint16_t speed_centi = raw_val; /* Đơn vị là 0.01 km/h */

    /* Khi cần in ra Shell UART hoặc gửi hiển thị: */
    printf("Speed: %d.%02d km/h\n", speed_centi / 100, speed_centi % 100);
    ```

---

### ❓ Câu 7: "Dưới 2% CPU load và 1000 frame/s: Đo bằng công cụ gì? Cách chứng minh con số này?"
* **Chứng minh lý thuyết băng thông (Bus Load)**:
  * Mạng CAN 500 kbps: 1 bit = $2\ \mu\text{s}$.
  * Một khung CAN chuẩn kèm bit stuffing có độ dài trung bình khoảng $110\text{ bits} \implies \text{Thời gian chiếm bus} \approx 110 \times 2\ \mu\text{s} = 220\ \mu\text{s}$.
  * Với tần suất $1000\text{ frames/s}$, tổng thời gian đường truyền bận:
    $$1000 \times 220\ \mu\text{s} = 0.22\text{s} \implies \text{Bus Load} \approx 22\%$$
* **Đo đạc thực tế CPU Load**:
  * Sử dụng **Zephyr Thread Analyzer** (`CONFIG_THREAD_ANALYZER=y` và `CONFIG_THREAD_ANALYZER_USE_STATISTICS=y`).
  * Hoặc dùng bộ đếm chu kỳ phần cứng **ARM Cortex-M7 DWT Cycle Counter** (`DWT->CYCCNT`):
    * Lõi Cortex-M7 chạy ở 216 MHz $\implies 1$ giây có $216,000,000$ cycles.
    * Đo thời gian xử lý 1 gói tin CAN trong ISR + đẩy vào `k_msgq`: Tốn khoảng 350 cycles ($\approx 1.62\ \mu\text{s}$).
    * Đo thời gian Worker Thread lấy gói tin, kiểm tra E2E CRC-8 và Unpack tín hiệu: Tốn khoảng 1200 cycles ($\approx 5.55\ \mu\text{s}$).
    * Tổng chi phí CPU cho 1 frame: $1.62 + 5.55 = 7.17\ \mu\text{s}$.
    * Với 1000 frames/s, tổng thời gian CPU tiêu tốn trong 1 giây là:
      $$1000 \times 7.17\ \mu\text{s} = 7.17\text{ ms} \implies \text{CPU Load} = \frac{7.17\text{ ms}}{1000\text{ ms}} = 0.717\% < 2\%$$

---

## 4.2. DỰ ÁN 2: High-Speed Bare-Metal TFT & SDHC Player (STM32F746)

### ❓ Câu 8: "Cấu hình Pixel Clock (PCLK) cho panel 480x272 @ 60Hz: Công thức tính toán chi tiết là gì?"
* **Thông số Timing của màn hình LCD 4.3 inch (Rocktech RK043FN48H)**:
  * Chiều ngang (Horizontal): $\text{Active} = 480$, $\text{HSYNC} = 41$, $\text{HBP} = 13$, $\text{HFP} = 32 \implies H_{total} = 480 + 41 + 13 + 32 = 566\text{ pixels}$.
  * Chiều dọc (Vertical): $\text{Active} = 272$, $\text{VSYNC} = 10$, $\text{VBP} = 2$, $\text{VFP} = 2 \implies V_{total} = 272 + 10 + 2 + 2 = 286\text{ lines}$.
* **Công thức tính Pixel Clock lý thuyết**:
  $$f_{PCLK} = H_{total} \times V_{total} \times \text{Frame Rate} = 566 \times 286 \times 60 = 9,712,560\text{ Hz} \approx 9.71\text{ MHz}$$
* **Cấu hình trên Clock Tree STM32F7**:
  * Xung nhịp nguồn cấp từ `PLLSAI`: $f_{IN} = \text{HSE} / M = 25\text{ MHz} / 25 = 1\text{ MHz}$.
  * Đặt $N = 192$, $R = 4 \implies f_{PLLSAI\_R} = \frac{1\text{ MHz} \times 192}{4} = 48\text{ MHz}$.
  * Bộ chia `PLLSAIDIVR` đặt chia $5$:
    $$f_{PCLK} = \frac{48\text{ MHz}}{5} = 9.6\text{ MHz}$$
  * Tần số quét thực tế: $\frac{9,600,000}{566 \times 286} = 59.3\text{ FPS}$ (Khớp chuẩn $60\text{ FPS}$).

---

### ❓ Câu 9: "Kỹ thuật Double Buffering VSYNC Reload (`VBR`) triệt tiêu hiện tượng xé hình (Tearing-Free) hoạt động thế nào?"
* **Nguyên nhân Tearing**: Xảy ra khi tốc độ ghi của ứng dụng lệch pha với tốc độ quét của chùm tia LTDC, làm cho nửa trên màn hình là khung hình cũ, nửa dưới là khung hình mới.
* **Giải thuật VBR (Vertical Blanking Reload)**:
  1. Cấp phát 2 bộ đệm khung (Framebuffer 0 và Framebuffer 1) nằm trên bộ nhớ ngoài FMC SDRAM (`0xC0000000` và `0xC007FA00`).
  2. Trong khi LTDC đang quét hiển thị Framebuffer 0, CPU/DMA2D vẽ toàn bộ hình ảnh mới vào Framebuffer 1.
  3. Khi vẽ xong, cập nhật địa chỉ Framebuffer 1 vào thanh ghi địa chỉ lớp: `LTDC_Layer1->CFBAR = (uint32_t)Framebuffer_1;`.
  4. Bật cờ nạp khi dập đứng: `LTDC->SRCR = LTDC_SRCR_VBR;`.
  5. Phần cứng LTDC chờ quét hết dòng 272 của khung hình hiện tại. Trong khoảng thời gian Vertical Blanking (khi chùm tia quay về đỉnh màn hình), địa chỉ mới được nạp tự động vào shadow register, triệt tiêu $100\%$ hiện tượng xé hình.

---

### ❓ Câu 10: "DMA2D Chrom-ART: Các chế độ hoạt động và ứng dụng cụ thể trong dự án?"
* **Chế độ R2M (Register-to-Memory)**: Điền một màu cố định vào một vùng hình chữ nhật trên RAM với tốc độ cực đại (dùng để xóa màn hình hoặc vẽ thanh tiến trình).
* **Chế độ M2M with PFC (Pixel Format Conversion)**: Chuyển đổi định dạng điểm ảnh phần cứng theo thời gian thực (ví dụ: Chuyển icon ảnh từ ARGB8888 sang RGB565 của Framebuffer).
* **Chế độ M2M with Blending**: Trộn 2 lớp đồ họa (Foreground và Background) với độ trong suốt Alpha Channel (0-255).
* **Line Offset Register (`DMA2D_OOR`)**:
  * Khi vẽ một icon kích thước $40 \times 40$ vào màn hình $480 \times 272$, sau khi vẽ hết 40 pixels của 1 dòng, DMA2D phải nhảy qua phần còn lại của màn hình để xuống dòng kế tiếp:
    $$\text{Output\_Offset} = 480 - 40 = 440\text{ pixels}$$

---

### ❓ Câu 11: "FMC SDRAM: Chuỗi 5 lệnh JEDEC khởi tạo và công thức tính Refresh Rate Counter?"
* **5 lệnh JEDEC bắt buộc trong hàm `SDRAM_InitSequence()`**:
  1. `CLK_ENABLE`: Cấp xung nhịp cho SDRAM.
  2. `PALL` (Precharge All): Đưa toàn bộ các bank của SDRAM về trạng thái nghỉ.
  3. `AUTOREFRESH`: Gửi ít nhất 8 chu kỳ tự làm tươi liên tiếp.
  4. `LOAD_MODE_REGISTER`: Nạp thanh ghi Mode Register của chip SDRAM (CAS Latency = 2, Burst Length = 1, Burst Type = Sequential).
  5. `NORMAL_MODE`: Đưa SDRAM vào chế độ vận hành bình thường.
* **Công thức tính Refresh Rate Counter (`FMC_SDRTR`)**:
  * Chip SDRAM MT48LC4M32B2 có 4096 dòng (rows), chu kỳ làm tươi toàn bộ là $64\text{ ms}$.
  * Thời gian làm tươi cho 1 dòng:
    $$T_{refresh} = \frac{64\text{ ms}}{4096} = 15.625\ \mu\text{s}$$
  * Xung nhịp SDRAM: $f_{SDCLK} = \frac{HCLK}{2} = \frac{216\text{ MHz}}{2} = 108\text{ MHz}$.
  * Giá trị nạp vào thanh ghi `FMC_SDRTR`:
    $$\text{COUNT} = (15.625\ \mu\text{s} \times 108\text{ MHz}) - 20 = 1687.5 - 20 = 1667$$

---

### ❓ Câu 12: "Chuẩn hóa thẻ MicroSD SDHC (4GB - 32GB) cho SDMMC: Tại sao bắt buộc dùng Block Addressing LBA?"
* **Bản chất**: Thẻ SDSC cũ ($\le 2\text{GB}$) dùng Byte Addressing (địa chỉ là số thứ tự nhân với 512). Thẻ SDHC ($4\text{GB} - 32\text{GB}$) có dung lượng vượt quá ngưỡng 4GB ($2^{32} - 1$), nếu dùng Byte Addressing sẽ làm **tràn số nguyên 32-bit (`uint32_t`)**.
* Do đó, chuẩn SDHC bắt buộc dùng **Block Addressing (LBA)**: Tham số trong các lệnh `CMD17`, `CMD18`, `CMD24` chính là **số thứ tự của Sector** (`final_addr = sector_addr`), tuyệt đối không nhân 512.
* **Quy trình xác thực SDHC trong Driver**:
  * Gửi `ACMD41` kèm cờ `HCS (Bit 30) = 1`.
  * Đọc thanh ghi `OCR`: Khi `Bit 31 (Ready) = 1`, kiểm tra `Bit 30 (CCS - Card Capacity Status)`: Nếu `CCS = 1`, xác nhận đúng thẻ SDHC. Nếu `CCS = 0`, Driver từ chối hoạt động để đảm bảo tính chuẩn hóa.

---

### ❓ Câu 13: "Sự cố D-Cache Coherency khi streaming video từ SDHC vào SDRAM và cách giải quyết triệt để?"
* **Hiện tượng**: SDMMC đọc dữ liệu từ thẻ nhớ nạp vào SDRAM thông qua FIFO/DMA. CPU đọc ra để hiển thị thì gặp dữ liệu rác, màn hình bị vỡ hình sọc nhiễu do CPU đọc dữ liệu cũ lưu trong L1 D-Cache Cortex-M7.
* **Quy trình giải quyết chuẩn 3 bước**:
  1. Căn lề bộ đệm đúng 32 bytes (độ dài Cache Line Cortex-M7): `__attribute__((aligned(32)))`.
  2. Trước khi đọc dữ liệu, xóa hiệu lực dòng Cache tương ứng:
     ```c
     SCB_InvalidateDCache_by_Addr((uint32_t *)pBuffer, buffer_size);
     ```
  3. Gọi chỉ thị rào cản đồng bộ bộ nhớ: `__asm volatile ("dsb 0xF" ::: "memory");`.

---

## 4.3. DỰ ÁN 3: ESP32-S3 Wearable Smartwatch

### ❓ Câu 14: "Tại sao phải tách Dual I2C Port? Nếu dùng chung thì hiện tượng gì xảy ra?"
* **Phân chia**:
  * `I2C0`: Chuyên dụng cho Touch Controller (CST816).
  * `I2C1`: Dành cho MAX30102 (Nhịp tim quang học) và BMI270 (IMU).
* **Nguyên nhân**: Cảm biến nhịp tim lấy mẫu liên tục với tần số cao (100 Hz). Khi người dùng vuốt màn hình cảm ứng, Touch Controller phát sinh ngắt liên tục. Nếu dùng chung 1 bus I2C, thao tác đọc Touch sẽ bị nghẽn bởi các giao dịch I2C dài của cảm biến, gây ra hiện tượng giật trễ phản hồi cảm ứng (Touch Latency $> 50\text{ ms}$). Việc tách riêng bus đảm bảo độ trễ cảm ứng luôn dưới $10\text{ ms}$.

---

### ❓ Câu 15: "Cấu hình LVGL Buffer trong PSRAM: Trade-off giữa SRAM và PSRAM là gì?"
* **Vấn đề**: Màn hình AMOLED $368 \times 448$ RGB565 tốn $368 \times 448 \times 2 = 329,728\text{ bytes} \approx 322\text{ KB RAM}$. SRAM nội của ESP32-S3 chỉ có ~380 KB khả dụng cho ứng dụng. Nếu đặt Framebuffer trong SRAM nội, hệ thống sẽ cạn kiệt RAM và không đủ chạy BLE/WiFi stack.
* **Giải pháp**: Đặt Framebuffer trong **Octal-SPI PSRAM** ngoài (8MB).
* **Trade-off & Tối ưu**:
  * Tốc độ truy cập PSRAM qua SPI chậm hơn SRAM nội khoảng 3 đến 4 lần.
  * Khắc phục: Sử dụng chế độ **Double Partial Buffer (kích thước 1/10 màn hình)** đặt tại SRAM nội để LVGL render các widget nhỏ, sau đó dùng EDMA chuyển dữ liệu song song ra màn hình, giữ vững tốc độ khung hình 35 - 45 FPS mà vẫn tiết kiệm hơn 300KB RAM nội.

---

### ❓ Câu 16: "Dòng tiêu thụ 25 µA Standby: Giải trình tính khả thi thực tế?"
* **Kiến trúc phần cứng Power-Gating**:
  * Sử dụng mạch công tắc tải (Load Switch / PMOS transistor) ngắt hoàn toàn $V_{CC}$ của GPS module (GT-U8), chip đo nhịp tim (MAX30102) và màn hình hiển thị.
  * Đưa ESP32-S3 vào chế độ **Deep Sleep**, tắt toàn bộ lõi CPU chính và khối Radio RF (Bluetooth/WiFi).
  * Chỉ duy trì nguồn cho khối **ULP Coprocessor** (chạy ở xung nhịp thấp 8 MHz) và RTC Controller để đếm bước chân từ ngắt INT của BMI270 và chờ ngắt thức giấc từ nút bấm cơ. Dòng tiêu thụ đo thực tế dao động từ $22\ \mu\text{A}$ đến $26\ \mu\text{A}$.

---

### ❓ Câu 17: "BLE Sync Latency 20ms & GPS 1PPS Sync: Hiện thực thế nào?"
* **BLE Low Latency**:
  * Sử dụng hàm `esp_ble_gap_set_prefer_conn_params()` thương lượng với smartphone:
    * `min_interval` = 16 (tương đương $16 \times 1.25\text{ ms} = 20\text{ ms}$).
    * `max_interval` = 24 ($30\text{ ms}$).
    * `latency` = 0.
* **GPS 1PPS Hardware Synchronization**:
  * Chân xung 1PPS của GPS có độ chính xác cấp nano giây được nối vào chân ngắt GPIO của ESP32.
  * ISR ngắt cạnh lên của 1PPS lập tức chốt giá trị của bộ đếm Microsecond Timer nội bộ, hiệu chỉnh sai số trôi dạt của thạch anh RTC trong đồng hồ, giữ độ chính xác thời gian tuyệt đối.

---

### ❓ Câu 18: "UI Watchdog Guard: Thuật toán phục hồi cụ thể khi GUI bị treo?"
* **Cơ chế**:
  * Tác vụ `vWatchdogTask` có mức ưu tiên cao nhất định kỳ kiểm tra một cờ đếm nhịp tim `g_ui_heartbeat`.
  * Trong vòng lặp chính của LVGL, ứng dụng liên tục nạp lại `g_ui_heartbeat = 10`.
  * Cứ mỗi giây, watchdog task giảm biến đếm đi 1 đơn vị. Nếu biến đếm giảm về 0 (nghĩa là tác vụ đồ họa bị deadlock hoặc kẹt vòng lặp quá 10 giây):
    1. Ghi log nguyên nhân lỗi vào phân vùng RTC Memory.
    2. Hủy tác vụ LVGL cũ và tái khởi tạo (`vTaskDelete` $\to$ `xTaskCreate`).
    3. Nếu sau 30 giây hệ thống vẫn không thể phản hồi, kích hoạt ngắt Watchdog phần cứng khởi động lại toàn bộ ESP32-S3.

---

## 4.4. DỰ ÁN 4 (Internship): Hệ Thống Giám Sát & Điều Khiển Tép Bạc

### ❓ Câu 19: "Bộ lọc Moving Average giảm nhiễu 40%: Window size bao nhiêu? Tại sao chọn kích thước đó?"
* **Kích thước cửa sổ lựa chọn**: $N = 16$ mẫu.
* **Lý do lựa chọn kỹ thuật**:
  1. Do $16 = 2^4$, phép chia tính trung bình $\frac{\sum X}{16}$ được compiler tối ưu thành **phép dịch bit phải 4 vị trí (`>> 4`)**, không tốn chu kỳ chia số nguyên của CPU.
  2. Cửa sổ $N=16$ với tần số lấy mẫu 10 Hz mang lại độ trễ chỉ $1.6\text{ giây}$, giảm $40\%$ phương sai nhiễu gai điện từ máy bơm công suất lớn mà vẫn đảm bảo độ phản hồi kịp thời cho thuật toán điều khiển relay.

---

### ❓ Câu 20: "Lỗi Timing trên đường truyền RS485: Bắt lỗi bằng Oscilloscope thế nào?"
* **Hiện tượng**: Master gửi lệnh Modbus RTU nhưng thỉnh thoảng không nhận được phản hồi từ các cảm biến đo pH/DO ở xa, máy thu báo lỗi **Framing Error**.
* **Phát hiện bug trên Oscilloscope**:
  * Kẹp kênh 1 vào chân điều khiển thu/phát `DE` (Driver Enable) của MAX485, kẹp kênh 2 vào đường `TX` của MCU.
  * Ngay khi byte cuối cùng vừa được nạp vào `USART_DR`, cờ `TXE` bật lên. Code lập tức hạ chân `DE = 0` về chế độ nhận. Tuy nhiên, lúc này byte dữ liệu thực tế **vẫn đang nằm trong Shift Register phần cứng và chưa truyền xong ra đường dây**. Stop bit bị cắt cụt giữa chừng.
* **Cách khắc phục**: Chuyển điều kiện hạ chân `DE` từ cờ `TXE` sang cờ **`TC` (Transmission Complete)**, đảm bảo toàn bộ các bit đã rời khỏi chân truyền vật lý rồi mới hạ chân `DE`.

---

### ❓ Câu 21: "MQTT 99.8% Uptime & Tối ưu hóa Latency FreeRTOS: Con số 0.2% từ đâu ra?"
* **Nguyên nhân 0.2% Downtime**: Tương đương khoảng $2.8\text{ phút}$ mất kết nối trong 1 ngày do sóng WiFi suy hao khi mưa giông hoặc router DHCP renew.
* **Cơ chế phục hồi**: Triển khai máy trạng thái kết nối lại với thuật toán **Exponential Backoff with Jitter** (thử kết nối lại sau 1s, 2s, 4s, 8s... tối đa 60s). Dữ liệu trong lúc mất mạng được lưu tạm vào Flash SPIFFS, khi có mạng trở lại sẽ tự động đẩy bù.
* **Tối ưu Latency FreeRTOS từ 120ms xuống < 50ms**: Tách rời tác vụ đọc cảm biến khỏi tác vụ truyền tin mạng, sử dụng **Direct-to-Task Notifications** thay vì Semaphore để giảm chi phí chuyển đổi ngữ cảnh từ 15 microsecond xuống dưới 3 microsecond.

---

# PHẦN 5 — TÌNH HUỐNG THỰC ĐỊA, QUY CHUẨN GIT & KỊCH BẢN PHỎNG VẤN (STAR)

### 5.1. Xử lý sự cố Thread RTOS bị treo / không được cấp CPU
1. **Kiểm tra Deadlock do Mutex**: Xem Thread có đang chờ một Mutex bị giữ bởi tác vụ khác không nhả ra.
2. **Kiểm tra Starvation do Priority**: Kiểm tra xem có Thread ưu tiên cao hơn đang chạy vòng lặp liên tục (`while(1)` không có `k_sleep` hoặc `yield`) làm bộ lập lịch không bao giờ nhường quyền.
3. **Kiểm tra Stack Overflow**: Kiểm tra cờ lỗi MPU Stack Guard xem Thread có bị vỡ ngữ cảnh do tràn ngăn xếp hay không.

---

### 5.2. Quy trình 3 bước xử lý khi mạng CAN Bus bị nhiễu cao
1. **Đo trở đầu cuối khi tắt nguồn**: Đo điện trở giữa chân CAN_H và CAN_L. Giá trị chuẩn bắt buộc phải là $60\ \Omega$ (sai số $\pm 5\%$). Nếu đo được $120\ \Omega$ nghĩa là đứt 1 trở; nếu đo ra $0\ \Omega$ là chập dây.
2. **Đồng bộ hóa Sample Point**: Sử dụng Logic Analyzer kiểm tra cấu hình Bit Timing giữa các node trên bus có lệch nhau không (chuẩn phải đồng nhất tại $83.3\% \sim 87.5\%$).
3. **Kiểm tra chênh lệch điện thế đất (Common Ground)**: Đảm bảo mass của các node trên mạng được nối chung hoặc có biến áp cách ly vi sai.

---

### 5.3. Quy chuẩn Git & Giải quyết xung đột Rebase
* **Quy ước Conventional Commits**:
  * `feat(can): implement iso 11898 bus-off recovery fsm`
  * `fix(uart): clear overrun error flag to prevent dma freeze`
  * `docs: update bit timing derivation formulas`
* **Quy trình giải quyết xung đột khi Rebase**:
  1. Git tạm dừng rebase tại commit gây xung đột.
  2. Mở file xung đột, phân tích sự khác nhau giữa nhánh hiện tại (`HEAD`) và nhánh rebase (`Incoming`).
  3. Sửa code giữ lại logic đúng nhất, lưu file.
  4. Gõ `git add <file>` để đánh dấu đã giải quyết (Tuyệt đối **không** dùng `git commit`).
  5. Gõ `git rebase --continue` để Git tiếp tục áp dụng các commit còn lại.

---

### 5.4. Kịch bản giới thiệu bản thân 60 giây (Chuẩn phương pháp STAR)

> *"Em chào anh/chị. Em là [Tên], tốt nghiệp chuyên ngành Kỹ thuật Máy tính/Điện Tử. Định hướng của em là trở thành một Kỹ sư Lập trình Nhúng Firmware chuyên sâu về kiến trúc hệ điều hành và giao thức ô tô.*
> 
> * **Situation & Task:** Trong quá trình học tập, nhận thấy các dự án sinh viên thường chỉ dừng lại ở mức gọi thư viện HAL cơ bản, em đặt mục tiêu làm chủ vi điều khiển từ tầng thanh ghi Bare-metal cho đến hệ điều hành chuyên dụng chuẩn công nghiệp Zephyr RTOS trên chip ARM Cortex-M7.*
> * **Action:** Em đã xây dựng thành công 2 dự án kỹ thuật độc lập: Một cụm Telematics & CAN Gateway chạy Zephyr RTOS đạt chuẩn an toàn AUTOSAR E2E, lọc phần cứng 6 filter banks và xử lý tín hiệu DBC bằng Fixed-Point; và một hệ thống Bare-Metal điều khiển trực tiếp thanh ghi FMC SDRAM và bộ tăng tốc đồ họa DMA2D đạt tốc độ hiển thị 60 FPS không giật xé hình.*
> * **Result:** Toàn bộ mã nguồn em đều tổ chức theo chuẩn kiến trúc module, đo lường định lượng bằng DWT Cycle Counter và viết Unit Test tự động. Với nền tảng hiểu sâu bản chất phần cứng và tư duy làm phần mềm chuẩn hóa, em tin mình sẽ nhanh chóng đóng góp hiệu quả vào các dự án nhúng tại quý công ty."*
