# Cẩm Nang Phỏng Vấn Kỹ Sư Nhúng: Lập Trình C, Kiến Trúc Hệ Thống và Phân Tích Dự Án

> **Đối tượng:** Kỹ sư nhúng định hướng STM32F7, Zephyr RTOS và giao thức truyền thông ô tô.  
> **Mục tiêu:** Hệ thống hóa kiến thức lập trình C nhúng, cấu trúc vi điều khiển ARM Cortex-M, hệ điều hành thời gian thực Zephyr RTOS và kịch bản trả lời phỏng vấn kỹ thuật định lượng.

---

## MỤC LỤC TỔNG QUAN

- [PHẦN 1: LẬP TRÌNH C EMBEDDED (TECH TEST VÀ LIVE-CODING)](#phần-1-lập-trình-c-embedded-tech-test-và-live-coding)
  - [1.1. Từ khóa `volatile` và cơ chế tối ưu của Compiler](#11-từ-khóa-volatile-và-cơ-chế-tối-ưu-của-compiler)
  - [1.2. Thao tác Bitwise, Bitmask & Clear-then-Set Pattern](#12-thao-tác-bitwise-bitmask--clear-then-set-pattern)
  - [1.3. Memory Layout của chương trình C trên MCU](#13-memory-layout-của-chương-trình-c-trên-mcu)
  - [1.4. Con trỏ nâng cao & Bảng hàm Callback (Function Pointer)](#14-con-trỏ-nâng-cao--bảng-hàm-callback-function-pointer)
  - [1.5. Struct Padding, Data Alignment & `__attribute__((packed))`](#15-struct-padding-data-alignment--__attribute__packed)
  - [1.6. Xử lý Endianness (Little-Endian vs Big-Endian)](#16-xử-lý-endianness-little-endian-vs-big-endian)
  - [1.7. Quản lý Bộ nhớ Động không dùng `malloc()` (Static Memory Pool)](#17-quản-lý-bộ-nhớ-động-không-dùng-malloc-static-memory-pool)
  - [1.8. Cài đặt Circular Ring Buffer chuẩn ISR-Safe bằng C](#18-cài-đặt-circular-ring-buffer-chuẩn-isr-safe-bằng-c)
- [PHẦN 2: KIẾN TRÚC VI ĐIỀU KHIỂN VÀ GIAO TIẾP NGOẠI VI](#phần-2-kiến-trúc-vi-điều-khiển-và-giao-tiếp-ngoại-vi)
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
- [PHẦN 3: HỆ ĐIỀU HÀNH THỜI GIAN THỰC VÀ ZEPHYR RTOS](#phần-3-hệ-điều-hành-thời-gian-thực-và-zephyr-rtos)
  - [3.1. Phân biệt Mutex, Binary Semaphore và Message Queue](#31-phân-biệt-mutex-binary-semaphore-và-message-queue)
  - [3.2. Quy tắc vàng: CẤM Block/Sleep bên trong ngắt ISR](#32-quy-tắc-vàng-cấm-blocksleep-bên-trong-ngắt-isr)
  - [3.3. Hiện tượng Nghịch đảo Ưu tiên (Priority Inversion) & Priority Inheritance](#33-hiện-tượng-nghịch-đảo-ưu-tiên-priority-inversion--priority-inheritance)
  - [3.4. Kiến trúc Driver Model Zephyr & Macro `DEVICE_DT_INST_DEFINE`](#34-kiến-trúc-driver-model-zephyr--macro-device_dt_inst_define)
  - [3.5. Devicetree & Kconfig vs `#define` truyền thống](#35-devicetree--kconfig-vs-define-truyền-thống)
  - [3.6. Công cụ dòng lệnh Meta-tool `west`](#36-công-cụ-dòng-lệnh-meta-tool-west)
  - [3.7. Bảo vệ tràn Stack bằng phần cứng: `CONFIG_MPU_STACK_GUARD`](#37-bảo-vệ-tràn-stack-bằng-phần-cứng-config_mpu_stack_guard)
- [PHẦN 4: PHÂN TÍCH CHUYÊN SÂU 4 DỰ ÁN TRONG CV](#phần-4-phân-tích-chuyên-sâu-4-dự-án-trong-cv)
  - [4.1. DỰ ÁN 1: Automotive CAN Gateway (STM32F746 + Zephyr RTOS)](#41-dự-án-1-automotive-can-gateway-stm32f746--zephyr-rtos)
  - [4.2. DỰ ÁN 2: High-Speed Bare-Metal TFT & SDHC Player (STM32F746)](#42-dự-án-2-high-speed-bare-metal-tft--sdhc-player-stm32f746)
  - [4.3. DỰ ÁN 3: ESP32-S3 Wearable Smartwatch](#43-dự-án-3-esp32-s3-wearable-smartwatch)
  - [4.4. DỰ ÁN 4 (Internship): Hệ Thống Giám Sát & Điều Khiển Tép Bạc](#44-dự-án-4-internship-hệ-thống-giám-sát--điều-khiển-tép-bạc)
- [PHẦN 5: TÌNH HUỐNG THỰC TẾ, QUY CHUẨN GIT VÀ KỊCH BẢN PHỎNG VẤN](#phần-5-tình-huống-thực-tế-quy-chuẩn-git-và-kịch-bản-phỏng-vấn)
  - [5.1. Xử lý sự cố Thread RTOS bị treo / không được cấp CPU](#51-xử-lý-sự-cố-thread-rtos-bị-treo--không-được-cấp-cpu)
  - [5.2. Quy trình 3 bước xử lý khi mạng CAN Bus bị nhiễu cao](#52-quy-trình-3-bước-xử-lý-khi-mạng-can-bus-bị-nhiễu-cao)
  - [5.3. Quy chuẩn Git & Giải quyết xung đột Rebase](#53-quy-chuẩn-git--giải-quyết-xung-đột-rebase)
  - [5.4. Kịch bản giới thiệu bản thân 60 giây (Chuẩn phương pháp STAR)](#54-kịch-bản-giới-thiệu-bản-thân-60-giây-chuẩn-phương-pháp-star)

---

# PHẦN 1: LẬP TRÌNH C EMBEDDED (TECH TEST VÀ LIVE-CODING)

### 1.1. Từ khóa `volatile` và cơ chế tối ưu của Compiler

#### Câu hỏi: "Bản chất của `volatile` là gì? Tại sao thiếu nó thì code đọc thanh ghi hoặc nhận ngắt ISR sẽ chạy sai khi bật cờ tối ưu `-O2` / `-O3`?"

#### 1. Bản chất kỹ thuật theo tiêu chuẩn C (ISO C99 / C11 Section 5.1.2.3):
* Từ khóa `volatile` là một Type Qualifier thông báo cho trình biên dịch (Compiler) rằng giá trị của đối tượng bộ nhớ có thể bị biến đổi bất kỳ lúc nào bởi các tác nhân nằm ngoài luồng thực thi hiện tại (như phần cứng ngoại vi, ngắt ISR, hoặc một luồng song song khác).
* **Quy tắc As-If Rule & Sequence Points**: Trong chuẩn C, mọi thao tác truy cập (đọc hoặc ghi) vào một đối tượng `volatile` được phân loại là **Observable Behavior** (hành vi quan sát được). Compiler bị cấm tuyệt đối:
  1. Không được loại bỏ thao tác truy cập ô nhớ (Dead Code Elimination).
  2. Không được hoán đổi thứ tự truy xuất giữa các đối tượng `volatile` qua các điểm tuần tự (Sequence Points).
  3. Không được lưu đệm (cache) giá trị của biến vào thanh ghi CPU (Register Allocation) qua các vòng lặp.

#### 2. Ba trường hợp bắt buộc phải dùng `volatile` trong Embedded:
1. **Con trỏ trỏ tới thanh ghi phần cứng (Memory-Mapped I/O Registers)**:
   ```c
   #define USART1_ISR (*(volatile uint32_t *)0x4001101CUL)
   ```
2. **Biến cờ (Flag) hoặc biến trạng thái chia sẻ giữa ISR và luồng thực thi nền (`main()` / Task)**:
   ```c
   volatile uint8_t g_rx_ready = 0;
   ```
3. **Biến dùng trong các hàm trễ thô (Software Busy-Wait Delays)**:
   ```c
   void delay_cycles(volatile uint32_t count) {
       while (count--);
   }
   ```

---

#### 3. Dẫn chứng thực nghiệm hoàn chỉnh (End-to-End C Code & Mổ xẻ Assembly):

Xét chương trình C thực tế trong hệ thống nhúng: luồng nền `main()` kiểm tra cờ nhận dữ liệu UART `g_rx_ready` để xử lý gói tin, trong khi cờ này được dựng lên bất đồng bộ từ bên trong trình phục vụ ngắt ngoại vi `USART1_IRQHandler()`.

```c
/* File: main.c */
#include <stdint.h>
#include <stdbool.h>

extern void process_uart_payload(void);

/* Bối cảnh: Biến cờ toàn cục chia sẻ giữa ngắt và luồng chính */
#if defined(USE_VOLATILE)
    volatile uint8_t g_rx_ready = 0; /* Đúng chuẩn hệ thống nhúng */
#else
    uint8_t g_rx_ready = 0;          /* THIẾU volatile -> Gây lỗi tối ưu hóa */
#endif

/* Luồng thực thi nền (Background Task) */
void wait_for_uart_packet(void) {
    while (g_rx_ready == 0) {
        /* Chờ cờ ngắt phần cứng dựng lên */
    }
    process_uart_payload();
}

/* Trình phục vụ ngắt phần cứng (Hardware ISR) */
void USART1_IRQHandler(void) {
    /* Đọc dữ liệu từ ngoại vi và bật cờ thông báo */
    g_rx_ready = 1;
}
```

---

#### 4. So sánh Assembly khi biên dịch với GCC ARM Cortex-M7 (`arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -O2 -S`):

##### Trường hợp A: Khi THIẾU `volatile` (Sinh mã lỗi - Treo máy vĩnh viễn)

* **Cơ chế tối ưu của Compiler**:
  Khi bật cờ tối ưu hóa `-O2` hoặc `-O3`, trình biên dịch thực hiện kỹ thuật **Loop-Invariant Code Motion (LICM)** và **Register Hoisting**.
  Quan sát thân hàm `wait_for_uart_packet()`, Compiler phân tích luồng điều khiển (Control Flow Graph) và nhận thấy: bên trong vòng lặp `while (g_rx_ready == 0)` hoàn toàn không có lệnh gán nào cho biến `g_rx_ready`, cũng không có lời gọi hàm ngoài nào có thể can thiệp vào biến này. Do `g_rx_ready` không được khai báo `volatile`, Compiler kết luận giá trị của `g_rx_ready` là hằng số bất biến xuyên suốt vòng lặp. Nó đưa lệnh đọc bộ nhớ (`LDRB`) ra khỏi vòng lặp và chỉ đọc đúng một lần duy nhất trước khi lặp.

```assembly
wait_for_uart_packet:
    ldr   r3, .L_ADDR           ; r3 = Địa chỉ của biến g_rx_ready (&g_rx_ready)
    ldrb  r2, [r3]              ; ĐỌC RAM ĐÚNG 1 LẦN DUY NHẤT: r2 = g_rx_ready (Giá trị ban đầu = 0)
    cbz   r2, .L_LOOP           ; Nếu r2 == 0, nhảy vào nhãn vòng lặp .L_LOOP
    b     process_uart_payload  ; Nếu r2 != 0, thoát lặp gọi hàm xử lý

.L_LOOP:
    b     .L_LOOP               ; BẪY TREO CỨNG: Lệnh nhảy tại chỗ vô tận (Infinite Loop)!

.L_ADDR:
    .word g_rx_ready
```

* **Hậu quả phần cứng thực tế**:
  1. Khi CPU thực thi `wait_for_uart_packet()`, nó nạp `g_rx_ready` (giá trị 0) từ SRAM vào thanh ghi CPU `r2`.
  2. Lệnh `cbz r2, .L_LOOP` kiểm tra thấy `r2 == 0` nên lập tức phân nhánh tới `.L_LOOP`.
  3. Tại `.L_LOOP`, CPU thực thi lệnh `b .L_LOOP` nhảy vòng tròn tại chỗ, tiêu tốn 100% tài nguyên xử lý của lõi Cortex-M7.
  4. Khi có dữ liệu UART đến, phần cứng kích hoạt ngắt, CPU tạm dừng và nhảy vào `USART1_IRQHandler()`. Lệnh ngắt ghi thành công giá trị `1` vào ô nhớ `g_rx_ready` trong SRAM.
  5. Khi ISR kết thúc (`BX LR`), CPU quay trở lại vị trí bị cắt ngang: đó chính là vòng lặp `.L_LOOP` (`b .L_LOOP`). Vì trong `.L_LOOP` **hoàn toàn không có bất kỳ lệnh `LDRB` nào đọc lại SRAM**, CPU tiếp tục nhảy tại chỗ vĩnh viễn trên thanh ghi `r2` vốn vẫn giữ giá trị 0. Toàn bộ thiết bị bị treo cứng (System Hang).

---

##### Trường hợp B: Khi CÓ `volatile` (Sinh mã chính xác - Luôn đồng bộ với RAM)

* **Cơ chế biên dịch**:
  Từ khóa `volatile` cấm trình biên dịch thực hiện kỹ thuật LICM và cấm lưu đệm giá trị vào thanh ghi. Mọi lần đánh giá biểu thức điều kiện `g_rx_ready == 0` đều bắt buộc phải phát ra một chu kỳ truy xuất bus bộ nhớ thực sự.

```assembly
wait_for_uart_packet:
    ldr   r3, .L_ADDR           ; r3 = Địa chỉ của biến g_rx_ready (&g_rx_ready)

.L_POLL_LOOP:
    ldrb  r2, [r3]              ; MỖI VÒNG LẶP ĐỀU BẮT BUỘC PHÁT LỆNH ĐỌC TỪ SRAM VÀO r2!
    cbz   r2, .L_POLL_LOOP      ; Nếu r2 == 0, quay lại .L_POLL_LOOP đọc lại từ SRAM
    b     process_uart_payload  ; Thoát lặp ngay khi r2 != 0 để xử lý gói tin

.L_ADDR:
    .word g_rx_ready
```

* **Cơ chế vận hành thực tế**:
  1. Lệnh `ldrb r2, [r3]` nằm ngay bên trong thân vòng lặp `.L_POLL_LOOP`.
  2. Khi chưa có dữ liệu, CPU liên tục phát các chu kỳ đọc bus SRAM.
  3. Ngay khi `USART1_IRQHandler()` được kích hoạt và nạp `1` vào ô nhớ `g_rx_ready` trong SRAM, chu kỳ đọc `ldrb r2, [r3]` kế tiếp sẽ nạp giá trị `1` vào thanh ghi `r2`.
  4. Lệnh kiểm tra `cbz r2, .L_POLL_LOOP` thấy `r2 != 0` nên không nhảy nữa, CPU thoát vòng lặp và thực thi lệnh `b process_uart_payload` một cách chuẩn xác, đúng với ý đồ thiết kế.

---

#### 5. Mổ xẻ đối với Thanh Ghi Phần Cứng (Memory-Mapped Peripheral Register):

Xét thao tác kiểm tra cờ nhận dữ liệu `RXNE` trong thanh ghi trạng thái `USART_ISR`:

```c
/* Nếu khai báo con trỏ thông thường THIẾU volatile: */
#define USART1_ISR_BUGGY  (*(uint32_t *)0x4001101CUL)

void wait_rxne_buggy(void) {
    while ((USART1_ISR_BUGGY & (1U << 5)) == 0); /* Chờ cờ RXNE (bit 5) = 1 */
}

/* Mã Assembly sinh ra với -O2: */
wait_rxne_buggy:
    ldr   r3, =0x4001101C
    ldr   r3, [r3]              ; Đọc thanh ghi phần cứng ĐÚNG 1 LẦN DUY NHẤT
    tst   r3, #32               ; Kiểm tra bit 5 (RXNE)
    bne   .L_EXIT
.L_STUCK:
    b     .L_STUCK              ; Treo máy tại chỗ, không bao giờ đọc lại phần cứng!
.L_EXIT:
    bx    lr
```

Khi có `volatile`:
```c
#define USART1_ISR_OK     (*(volatile uint32_t *)0x4001101CUL)

void wait_rxne_ok(void) {
    while ((USART1_ISR_OK & (1U << 5)) == 0);
}

/* Mã Assembly sinh ra với -O2: */
wait_rxne_ok:
    ldr   r3, =0x4001101C
.L_READ_HARDWARE:
    ldr   r2, [r3]              ; PHÁT CHU KỲ BUS APB ĐỌC MỚI THANH GHI Ở MỖI VÒNG LẶP!
    tst   r2, #32
    beq   .L_READ_HARDWARE      ; Chưa có byte mới thì tiếp tục đọc thanh ghi phần cứng
    bx    lr
```

* **Kết luận đúc kết khi phỏng vấn**: `volatile` không phải là cơ chế đồng bộ hóa luồng (Thread-safety) hay tạo rào cản bộ nhớ (Memory Barrier / Cache Coherency), mà là **một chỉ thị bắt buộc đối với trình biên dịch** nhằm vô hiệu hóa các phép tối ưu lưu đệm thanh ghi và xóa bỏ thao tác ô nhớ, bảo đảm mã máy phát ra luôn đọc/ghi trực tiếp tới địa chỉ vật lý của phần cứng và SRAM.

---

### 1.2. Thao tác Bitwise, Bitmask & Clear-then-Set Pattern

#### Câu hỏi: "Viết bộ Macro thao tác bit chuẩn mực và giải thích tại sao khi cấu hình trường nhiều bit bắt buộc phải dùng mẫu Clear-then-Set?"

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
  * Giả sử trường cấu hình Mode 2-bit nằm ở bit [5:4] của thanh ghi `GPIOx_MODER`. Giá trị Reset mặc định đang là `0b11` (Analog mode).
  * Nếu muốn chuyển sang `0b01` (General Purpose Output) mà chỉ dùng phép OR:
    `REG = REG | (0b01 << 4) = 0b11 | 0b01 = 0b11` (Vẫn giữ nguyên là Analog!).
  * Phép OR **chỉ có thể biến 0 thành 1, không thể biến 1 thành 0**. Vì vậy, bắt buộc phải xóa sạch trường bit đó về `0b00` trước bằng phép `& ~MASK`, sau đó mới OR với giá trị mới.

#### Bài kiểm tra Live-Coding: "Viết hàm đảo ngược thứ tự các bit của một số 32-bit (Bit Reversal) không dùng hàm thư viện."
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
/* Lưu ý tối ưu: Trên kiến trúc ARM Cortex-M, có thể thay thế bằng lệnh Assembly chuyên dụng: */
/* __asm volatile ("rbit %0, %1" : "=r"(result) : "r"(n)); */
```

---

### 1.3. Memory Layout của chương trình C trên MCU

#### Câu hỏi: "Trình bày các phân vùng bộ nhớ của chương trình C trong vi điều khiển. Chỉ rõ 6 biến sau nằm ở đâu?"

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

* **Vị trí từng biến**:
  1. `const int a = 100;` -> Nằm ở **`.rodata` (Flash ROM)**.
  2. `static int b = 20;` -> Nằm ở **`.data` (RAM)** (Giá trị 20 lưu trong Flash, được startup copy vào RAM).
  3. `static int c;` -> Nằm ở **`.bss` (RAM)** (Startup code xóa về 0).
  4. `int d = 30;` (nằm trong thân hàm) -> Nằm ở **`Stack` (RAM)**.
  5. `char *p = "Hello";` -> Biến con trỏ `p` (4 bytes) nằm ở **`Stack`**, chuỗi `"Hello"` nằm ở **`.rodata` (Flash)**.
  6. `int *ptr = malloc(40);` -> Biến `ptr` nằm ở **`Stack`**, vùng nhớ 40 bytes được cấp phát nằm ở **`Heap` (RAM)**.

---

### 1.4. Con trỏ nâng cao & Bảng hàm Callback (Function Pointer)

#### Câu hỏi: "Phân biệt `const int *p`, `int * const p`, `const int * const p`. Cho ví dụ thực tế dùng Function Pointer để xây dựng Driver Callback."
* **Phân biệt cú pháp**:
  * `const int *p`: Con trỏ trỏ tới dữ liệu hằng. Không thể sửa nội dung `*p = 5`, nhưng con trỏ có thể đổi sang trỏ địa chỉ khác `p = &other`.
  * `int * const p`: Con trỏ hằng trỏ tới dữ liệu biến đổi. Không thể đổi địa chỉ trỏ `p = &other`, nhưng có thể sửa nội dung `*p = 10` (Thanh ghi ngoại vi vi điều khiển chính là con trỏ hằng).
  * `const int * const p`: Cả địa chỉ con trỏ lẫn dữ liệu ô nhớ đều là hằng số cố định, không thể thay đổi.
* **Mẫu thiết kế Driver Callback dùng Function Pointer (Kiến trúc phân tầng hoàn chỉnh)**:

```c
/* ================= TẦNG DRIVER NGOẠI VI (can_driver.h & can_driver.c) ================= */
typedef void (*can_rx_callback_t)(uint32_t can_id, const uint8_t *data, uint8_t dlc);

typedef struct {
    CAN_TypeDef *instance;
    can_rx_callback_t rx_cb; /* Con trỏ hàm callback lưu hàm xử lý của tầng trên */
} can_driver_handle_t;

/* Đối tượng driver quản lý phần cứng */
static can_driver_handle_t s_hcan1 = { .instance = CAN1, .rx_cb = NULL };

/* Hàm cho phép tầng Application đăng ký callback mà không sửa code driver */
void CAN_RegisterRxCallback(can_driver_handle_t *hcan, can_rx_callback_t callback) {
    if (hcan != NULL) {
        hcan->rx_cb = callback;
    }
}

/* Trình phục vụ ngắt phần cứng: Gọi ngược về hàm của tầng Application */
void CAN1_RX0_IRQHandler(void) {
    if (CAN1->RF0R & CAN_RF0R_FMP0) { /* Kiểm tra cờ có bản tin trong FIFO0 */
        uint32_t id = (CAN1->sFIFOMailBox[0].RIR >> 21);
        uint8_t dlc = (CAN1->sFIFOMailBox[0].RDTR & 0x0F);
        uint8_t data[8];
        
        uint32_t rdl = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t rdh = CAN1->sFIFOMailBox[0].RDHR;
        data[0] = (uint8_t)(rdl >> 0);  data[1] = (uint8_t)(rdl >> 8);
        data[2] = (uint8_t)(rdl >> 16); data[3] = (uint8_t)(rdl >> 24);
        data[4] = (uint8_t)(rdh >> 0);  data[5] = (uint8_t)(rdh >> 8);
        data[6] = (uint8_t)(rdh >> 16); data[7] = (uint8_t)(rdh >> 24);

        /* Kích hoạt callback nếu tầng trên đã đăng ký */
        if (s_hcan1.rx_cb != NULL) {
            s_hcan1.rx_cb(id, data, dlc);
        }

        /* Ghi 1 trực tiếp để xóa cờ W1C giải phóng Mailbox (Tuyệt đối không dùng |=) */
        CAN1->RF0R = CAN_RF0R_RFOM0;
    }
}

/* ================= TẦNG ỨNG DỤNG (main.c) ================= */
/* Hàm xử lý của Application: Tách biệt 100% khỏi thanh ghi phần cứng */
static void app_vehicle_telemetry_handler(uint32_t can_id, const uint8_t *data, uint8_t dlc) {
    if (can_id == 0x123 && dlc >= 4) {
        uint16_t speed = (data[0] << 8) | data[1];
        uint16_t rpm   = (data[2] << 8) | data[3];
        /* Cập nhật trạng thái ứng dụng... */
    }
}

int main(void) {
    /* Đăng ký hàm xử lý ứng dụng vào Driver */
    CAN_RegisterRxCallback(&s_hcan1, app_vehicle_telemetry_handler);

    while (1) {
        /* Luồng chính chạy nền tự do, ngắt ngoại vi sẽ tự định tuyến callback */
    }
}
```

---

### 1.5. Struct Padding, Data Alignment & `__attribute__((packed))`

#### Câu hỏi: "Tại sao `sizeof(struct { char a; int b; short c; })` lại ra 12 bytes trên ARM Cortex-M? Hậu quả gì xảy ra nếu không thêm `__attribute__((packed))` khi gửi struct qua CAN/UART?"
* **Cơ chế căn lề tự nhiên (Data Alignment)**:
  * Vi xử lý 32-bit Cortex-M tối ưu hóa truy cập dữ liệu khi biến N-byte nằm ở địa chỉ chia hết cho N.
  * `char a` (1 byte) nằm tại Offset 0.
  * `int b` (4 bytes) bắt buộc nằm ở địa chỉ chia hết cho 4 -> Compiler tự chèn **3 bytes đệm (Padding)** tại Offset 1, 2, 3. Biến `b` chiếm Offset 4, 5, 6, 7.
  * `short c` (2 bytes) nằm tại Offset 8, 9.
  * Toàn bộ struct có thành viên lớn nhất là 4 bytes -> Kích thước tổng phải chia hết cho 4 -> Chèn thêm **2 bytes padding** ở Offset 10, 11.
  * **Tổng cộng: 1 + 3 (pad) + 4 + 2 + 2 (pad) = 12 bytes**.
* **Hậu quả trên bus truyền thông (CAN / UART)**:
  * Khung truyền CAN tiêu chuẩn chỉ có tối đa 8 bytes dữ liệu. Nếu map trực tiếp struct không có `packed`, các byte rác padding sẽ bị gửi lên bus, làm sai lệch toàn bộ vị trí offset dữ liệu phía máy nhận và vượt quá giới hạn 8 bytes.
  * Bắt buộc dùng `__attribute__((packed))` để kích thước struct đúng bằng: `1 + 4 + 2 = 7 bytes`.

---

### 1.6. Xử lý Endianness (Little-Endian vs Big-Endian)

#### Câu hỏi: "Viết hàm C phát hiện kiến trúc vi điều khiển là Little hay Big Endian. Viết macro hoán đổi byte cho số 16-bit và 32-bit."

```c
/* Kiểm tra bằng con trỏ */
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

#### Câu hỏi: "Tại sao trong Firmware tiêu chuẩn ô tô (MISRA-C / ISO 26262) lại cấm `malloc()`? Làm sao cấp phát động an toàn?"
* **3 hiểm họa của `malloc()` / `free()` trong hệ thống nhúng**:
  1. **Phân mảnh bộ nhớ (Heap Fragmentation)**: Các khối nhớ nhỏ xen kẽ khiến hệ thống không tìm được khối nhớ liên tục đủ lớn.
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

#### Câu hỏi: "Cài đặt một Ring Buffer bằng C thuần phục vụ nhận ngắt UART. Làm sao để đảm bảo Thread-Safe giữa ISR và `main()` mà không cần dùng Mutex?"

```c
#define RING_BUFFER_SIZE  128 /* Lũy thừa của 2 để tối ưu phép chia % thành & MASK */
#define RING_BUFFER_MASK  (RING_BUFFER_SIZE - 1)

typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head; /* Chỉ Producer (ISR) ghi */
    volatile uint16_t tail; /* Chỉ Consumer (main) đọc */
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool ring_buffer_push(ring_buffer_t *rb, uint8_t data) {
    uint16_t next_head = (rb->head + 1) & RING_BUFFER_MASK;
    if (next_head == rb->tail) return false; /* Buffer đầy (Giữ trống 1 slot phân biệt rỗng/đầy) */
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

/* ================= VÍ DỤ SỬ DỤNG THỰC TẾ (END-TO-END DEMO) ================= */
static ring_buffer_t s_uart_rx_rb;

/* Trình phục vụ ngắt UART (Single Producer - Chỉ ghi head) */
void USART1_IRQHandler(void) {
    if (USART1->ISR & (1U << 5)) { /* Kiểm tra cờ RXNE (Read Data Register Not Empty) */
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);
        if (!ring_buffer_push(&s_uart_rx_rb, byte)) {
            /* Bộ đệm đầy: Ghi nhận lỗi Buffer Overflow */
        }
    }
}

/* Luồng thực thi nền main (Single Consumer - Chỉ ghi tail) */
int main(void) {
    ring_buffer_init(&s_uart_rx_rb);
    /* Cấu hình UART và kích hoạt ngắt NVIC... */

    while (1) {
        uint8_t rx_data;
        if (ring_buffer_pop(&s_uart_rx_rb, &rx_data)) {
            process_incoming_byte(rx_data); /* Xử lý dữ liệu nhận được */
        }
    }
}
```

* **Cơ sở kỹ thuật Thread-Safe không cần Mutex (Mô hình SPSC Lock-Free)**:
  1. **Tách biệt quyền hạn ghi**: `head` chỉ được cập nhật bởi một Producer duy nhất (ngắt ISR), còn `tail` chỉ được cập nhật bởi một Consumer duy nhất (luồng `main()`).
  2. **Tính nguyên tử (Atomic Access)**: Trên kiến trúc vi xử lý 32-bit ARM Cortex-M, lệnh đọc và ghi biến kiểu `uint16_t` là lệnh đơn lẻ (`LDRH` / `STRH`). CPU không bao giờ bị cắt ngang giữa chừng khi đang đọc/ghi một con trỏ 16-bit.
  3. **Không xung đột điều kiện tương tranh (Race Condition)**: Ngắt ISR có thể nhảy vào bất kỳ lúc nào để nạp dữ liệu và tăng `head`, nhưng nó không bao giờ sửa đổi `tail`. Luồng `main()` chỉ kiểm tra vị trí của `head` để quyết định đọc dữ liệu. Vì vậy, hệ thống hoàn toàn Thread-Safe mà không cần dùng Mutex hay cấm ngắt toàn cục (`__disable_irq()`), giúp loại bỏ 100% thời gian trễ ngắt (Zero Interrupt Latency Overhead).

---

# PHẦN 2: KIẾN TRÚC VI ĐIỀU KHIỂN VÀ GIAO TIẾP NGOẠI VI

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
* **Baudrate**: Tốc độ truyền bit trên đường dây, tính bằng tần số bus chia cho bộ chia USARTDIV.
* **Oversampling 16x vs 8x**: Lấy mẫu tín hiệu 16 lần hoặc 8 lần trong 1 bit để tìm điểm giữa bit ổn định nhất nhằm chống rung pha (Jitter). Chế độ 8x cho phép đạt tốc độ baud cao hơn nhưng khả năng chịu nhiễu kém hơn 16x.
* **Framing Error (FE)**: Xảy ra khi bộ thu kiểm tra tại vị trí Stop bit nhưng không thấy mức điện áp cao ('1') như quy chuẩn (thường do lệch baudrate giữa 2 thiết bị).

---

### 2.7. SPI: Full-Duplex, CPOL và CPHA
* **Full-Duplex**: Khả năng truyền (MOSI) và nhận (MISO) đồng thời trên cùng một chu kỳ xung clock SCK.
* **CPOL (Clock Polarity)**: Mức logic của chân SCK khi bus ở trạng thái nghỉ (`CPOL=0`: Nghỉ mức 0; `CPOL=1`: Nghỉ mức 1).
* **CPHA (Clock Phase)**: Cạnh xung dùng để chốt mẫu dữ liệu (`CPHA=0`: Chốt mẫu ở cạnh đầu tiên; `CPHA=1`: Chốt mẫu ở cạnh thứ hai).

---

### 2.8. I2C: Clock Stretching & Open-Drain Bus
* **Open-Drain Bus**: Cần có điện trở kéo lên nguồn (4.7 kOhm đến 10 kOhm) trên 2 đường SDA và SCL. Các thiết bị chỉ được phép kéo đường dây xuống mức thấp ('0') hoặc thả nổi để điện trở kéo lên mức cao ('1').
* **Clock Stretching**: Cơ chế cho phép thiết bị Slave tạm thời giữ đường SCL ở mức thấp để làm chậm Master khi Slave đang bận xử lý dữ liệu và chưa sẵn sàng nhận tiếp.

---

### 2.9. CAN Bus: Trở đầu cuối 120 Ohm & Phân xử Trọng tài (Arbitration)
* **Điện trở 120 Ohm**: Đường dây vi sai CAN là một đường truyền sóng dài. Hai trở 120 Ohm mắc song song ở 2 đầu tạo nên tổng trở bus tương đương 60 Ohm, giúp triệt tiêu sóng phản xạ gây méo xung tín hiệu.
* **Dominant (0) vs Recessive (1)**: Dominant có chênh lệch điện áp vi sai khoảng 2.0V, Recessive có chênh lệch điện áp bằng 0V. Khi một node phát 0 và một node phát 1, trạng thái trên bus sẽ là 0. Do đó, **CAN ID số càng nhỏ thì độ ưu tiên càng cao**.

---

### 2.10. DMA Controller: Polling vs Interrupt vs DMA
* **Polling**: CPU chạy vòng lặp liên tục chờ cờ phần cứng -> Chiếm dụng 100% CPU.
* **Interrupt**: Mỗi byte nhận/gửi đều kích hoạt ngắt -> Chi phí context switch lớn, dễ nghẽn CPU khi tốc độ truyền cao.
* **DMA**: Khối phần cứng độc lập tự động chuyển cả khối dữ liệu giữa Ngoại vi và Bộ nhớ mà không cần CPU -> CPU hoàn toàn rảnh tay làm việc khác.

---

### 2.11. Thiết bị đo kiểm: JTAG vs SWD, Logic Analyzer vs Oscilloscope
* **JTAG vs SWD**: JTAG dùng 4-5 chân, SWD chỉ dùng 2 chân (`SWDIO`, `SWCLK`) nhưng tốc độ ngang ngửa và tiết kiệm chân IO.
* **Logic Analyzer vs Oscilloscope**: Logic Analyzer chuyên bắt và giải mã dữ liệu số logic (0/1) trên nhiều kênh (UART, SPI, CAN). Oscilloscope đo điện áp tương tự theo thời gian thực để kiểm tra tính toàn vẹn tín hiệu, sụt áp, nhiễu và méo xung.

---

# PHẦN 3: HỆ ĐIỀU HÀNH THỜI GIAN THỰC VÀ ZEPHYR RTOS

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

# PHẦN 4: PHÂN TÍCH CHUYÊN SÂU 4 DỰ ÁN TRONG CV

---

## 4.1. DỰ ÁN 1: Automotive CAN Gateway (STM32F746 + Zephyr RTOS)

### Câu 1: "Với xung nhịp bus APB1 là 54 MHz, tính toán chi tiết Prescaler, BS1, BS2 và SJW để đạt tốc độ chuẩn 500 kbps với điểm lấy mẫu 83.33%?"

* **Kịch bản trả lời:**
  > *"Dạ, trên STM32F746 chạy 216MHz thì bus APB1 cấp cho bộ điều khiển CAN tối đa là 54 MHz.  
  > Để đạt tốc độ 500 kbps, chu kỳ của 1 bit sẽ là 2000 nano-giây. Em chia 1 bit thành 18 khoảng thời gian lượng tử (Time Quanta - tq), tức là tần số của mỗi tq sẽ là 9 MHz. Lấy 54 MHz chia cho 9 MHz thì em ra hệ số chia Prescaler BRP bằng 6.  
  > Về điểm lấy mẫu, tiêu chuẩn CiA 301 khuyến nghị là khoảng 87.5%. Em bố trí 1 bit gồm: 1 tq cho đoạn đồng bộ Sync, 14 tq cho đoạn BS1, và 3 tq còn lại cho đoạn BS2. Lúc này điểm lấy mẫu thực tế rơi vào đúng 15 chia 18, tức là 83.33%, rất sát với chuẩn.  
  > Khi ghi vào thanh ghi CAN_BTR, phần cứng yêu cầu lấy giá trị thực trừ đi 1, nên em nạp BRP=5, TS1=13, TS2=2 và bước nhảy đồng bộ SJW=0."*

* **Thông số kỹ thuật:**
  * Xung nhịp bus: APB1 = 54 MHz.
  * Tốc độ mạng: 500 kbps (1 bit = 2000 ns, chia 18 tq -> 1 tq = 111.1 ns).
  * Bộ chia: `BRP = 54 MHz / (500 kbps x 18) = 6` (Nạp 5 vào thanh ghi).
  * Phân bổ bit: `Sync = 1 tq`, `BS1 = 14 tq`, `BS2 = 3 tq` -> `Sample Point = 15 / 18 = 83.33%`.
  * Bước nhảy bù: `SJW = 1 tq`.

---

### Câu 2: "Show một đoạn DeviceTree Overlay thật cho bxCAN trong Zephyr và giải thích từng thuộc tính."

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
* **Kịch bản trả lời:**
  > *"Dạ, trong file overlay em kích hoạt node `&can1` bằng `status = "okay"`.  
  > Thuộc tính `pinctrl-0` ánh xạ trực tiếp chân PB8 và PB9 sang Alternate Function AF9 của CAN thông qua hệ thống pinctrl của Zephyr trước khi vào `main()`.  
  > Hai thuộc tính `bus-speed = <500000>` và `sample-point = <833>` khai báo tốc độ 500kbps và điểm lấy mẫu 83.3%. Driver CAN gốc của Zephyr sẽ tự động lấy các giá trị này kết hợp với xung nhịp bus trong Devicetree để tự cấu hình thanh ghi BTR tối ưu nhất."*

---

### Câu 3: "Async CAN reception với 6 hardware filter bank vào k_msgq: Filter mode dùng là Mask hay List? Tại sao chọn 6?"

* **Kịch bản trả lời:**
  > *"Dạ, em kết hợp cả 2 chế độ List Mode và Mask Mode:  
  > Em dùng 2 Filter Banks ở chế độ List Mode để bắt chính xác 4 ID khẩn cấp cố định gồm: ID 0x100 phanh khẩn cấp, ID 0x102 bung túi khí, và 2 ID chẩn đoán OBD-II là 0x7DF và 0x7E8.  
  > Còn 4 Filter Banks còn lại em dùng chế độ Mask Mode với mặt nạ 0x7F0 để gom toàn bộ các dải dữ liệu Telemetry của xe, ví dụ dải động cơ từ 0x200 đến 0x20F và dải thân xe từ 0x300 đến 0x30F.  
  > Em chọn đúng 6 Banks vì phần cứng STM32F746 có 28 Banks chia sẻ giữa CAN1 và CAN2. Dùng 6 Banks là vừa đủ cô lập hoàn toàn các gói tin cần thiết của dự án, lọc sạch 100% các bản tin rác khác ngay từ tầng phần cứng, vừa để dành 22 Banks còn lại cho CAN2 mà không gây lãng phí."*

---

### Câu 4: "AUTOSAR E2E Profile 1: Đa thức CRC-8 sử dụng là gì và tại sao chọn Profile 1?"

* **Kịch bản trả lời:**
  > *"Dạ, trong chuẩn AUTOSAR E2E Profile 1, em dùng đa thức CRC-8 chuẩn là 0x1D với giá trị khởi tạo và XOR_OUT đều là 0xFF.  
  > Cấu trúc gói tin 8 bytes gồm: 7 bytes đầu chứa dữ liệu tải trọng và bộ đếm Rolling Counter từ 0 đến 15 kèm Data ID, byte cuối cùng là mã CRC-8.  
  > Em chọn Profile 1 vì nó được thiết kế tối ưu nhất cho mạng CAN truyền thống có độ dài cố định 8 bytes. Về mặt tính toán, thuật toán dùng bảng tra cứu sẵn 256 phần tử, thời gian tính CRC chỉ tốn chưa tới 1.5 microsecond trên Cortex-M7 216MHz, vừa đáp ứng chuẩn an toàn dữ liệu, vừa không làm nghẽn hàng đợi xử lý."*

---

### Câu 5: "Cơ chế ISO 11898-1 Bus-Off Recovery trong 100ms: Phát hiện bằng cách nào và khôi phục cụ thể ra sao?"

* **Kịch bản trả lời:**
  > *"Dạ, khi đường truyền bị lỗi liên tục, bộ đếm lỗi truyền TEC vượt quá 255 thì phần cứng CAN tự động ngắt kết nối để bảo vệ bus, gọi là trạng thái Bus-Off. Driver phát hiện điều này bằng cách đọc bit BOFF trong thanh ghi CAN_ESR hoặc thông qua callback thay đổi trạng thái của Zephyr.  
  > Theo chuẩn ISO 11898-1, một node muốn hòa mạng trở lại thì bắt buộc phải giám sát thấy 128 lần xuất hiện của 11 bit lặn liên tiếp trên đường truyền rảnh.  
  > Trong code, khi bắt được sự kiện Bus-Off, em chuyển máy trạng thái FSM sang RECOVERING, bật timer 100ms và kích hoạt chu trình Reset phần cứng CAN. Sau khi phần cứng tự đếm đủ 128 chuỗi 11 bit lặn thì cờ BOFF tự động xóa, FSM khôi phục lại trạng thái ERROR_ACTIVE và tiếp tục nhận gửi bình thường."*

---

### Câu 6: "Xử lý 12 vehicle signal bằng Fixed-Point: Công thức chuyển đổi raw CAN data sang giá trị vật lý là gì? Cho ví dụ cụ thể."

* **Kịch bản trả lời:**
  > *"Dạ, theo định dạng Vector DBC, giá trị vật lý bằng giá trị thô nhân với hệ số Factor cộng với Offset.  
  > Ví dụ tín hiệu tốc độ xe Vehicle Speed có Factor là 0.01 km/h. Nếu dùng số thực float thì vi điều khiển phải tốn chu kỳ FPU và dễ sinh sai số làm tròn.  
  > Thay vào đó, em dùng kỹ thuật Fixed-Point: em giữ nguyên giá trị thô với đơn vị là centi-km/h, tức là 0.01 km/h. Khi cần in ra màn hình hoặc gửi lên UART, em chỉ việc lấy giá trị đó chia cho 100 để lấy phần nguyên, và chia lấy dư cho 100 để lấy phần thập phân. Cách này chạy 100% bằng số nguyên nên tốc độ xử lý cực nhanh và đảm bảo an toàn tuyệt đối."*

---

### Câu 7: "Dưới 2% CPU load và 1000 frame/s: Đo bằng công cụ gì? Cách chứng minh con số này?"

* **Kịch bản trả lời:**
  > *"Dạ, em chứng minh bằng cả lý thuyết đường truyền và đo đạc thực tế:  
  > Thứ nhất về đường truyền: Một frame CAN chuẩn dài trung bình khoảng 110 bit. Ở tốc độ 500kbps thì 1 frame chiếm bus khoảng 220 microsecond. Khi truyền 1000 frame/giây thì tổng thời gian chiếm bus là 220 mili-giây, tức là Bus Load chỉ khoảng 22%, đường truyền vẫn còn rất thoáng.  
  > Thứ hai về tải CPU: Em dùng bộ đếm chu kỳ phần cứng DWT của Cortex-M7 để đo thời gian thực thi: ngắt ISR nhận frame và nạp vào hàng đợi k_msgq chỉ tốn khoảng 350 chu kỳ (tức là 1.6 microsecond); Worker Thread lấy gói tin, kiểm tra E2E CRC-8 và giải mã tín hiệu tốn khoảng 1200 chu kỳ (khoảng 5.5 microsecond).  
  > Tổng thời gian CPU xử lý 1 frame là hơn 7 microsecond. Một giây nhận 1000 frame thì CPU chỉ tốn khoảng hơn 7 mili-giây, tương đương CPU Load chỉ vào khoảng 0.7%, hoàn toàn nằm dưới ngưỡng 2% mà em nêu trong CV."*

---

## 4.2. DỰ ÁN 2: High-Speed Bare-Metal TFT & SDHC Player (STM32F746)

### Câu 8: "Cấu hình Pixel Clock (PCLK) cho panel 480x272 @ 60Hz: Công thức tính toán chi tiết là gì?"

* **Kịch bản trả lời:**
  > *"Dạ, màn hình 480x272 ngoài vùng hiển thị thực tế thì còn có các khoảng dập xung ngang và dập xung dọc để chùm tia quét quay về đầu dòng và đầu khung.  
  > Cộng cả các khoảng blanking đó thì tổng kích thước quét thực tế là 566 pixel ngang và 286 đường quét dọc.  
  > Để màn hình quét đủ 60 khung hình một giây, em lấy 566 nhân 286 nhân với 60, ra tần số Pixel Clock cần cấp là khoảng 9.71 MHz.  
  > Trong Clock Tree của STM32F7, em lấy nguồn từ khối PLLSAI, cấu hình bộ chia để sinh ra xung nhịp chính xác là 9.6 MHz cấp cho khối LTDC. Với xung 9.6 MHz này, tốc độ làm tươi thực tế đạt 59.3 FPS, khớp hoàn hảo với chuẩn hiển thị mượt mà 60 FPS."*

* **Thông số kỹ thuật:**
  * Kích thước tổng cả Blanking: `H_total = 566 pixels`, `V_total = 286 lines`.
  * Tần số yêu cầu: `566 x 286 x 60 = 9.71 MHz`.
  * Cấu hình thực tế: Nguồn `PLLSAI` chia ra `PCLK = 9.6 MHz` -> Đạt `59.3 FPS` (~60 FPS).

---

### Câu 9: "Kỹ thuật Double Buffering VSYNC Reload (`VBR`) triệt tiêu hiện tượng xé hình (Tearing-Free) hoạt động thế nào?"

* **Kịch bản trả lời:**
  > *"Dạ, hiện tượng xé hình Tearing xảy ra khi CPU ghi đè dữ liệu mới vào đúng vùng nhớ mà bộ điều khiển LTDC đang quét dở ra màn hình.  
  > Để triệt tiêu hoàn toàn, em cấp phát 2 bộ đệm Framebuffer 0 và Framebuffer 1 trên SDRAM ngoài. Trong khi LTDC đang quét hiển thị Framebuffer 0 thì CPU và DMA2D vẽ toàn bộ khung hình mới vào Framebuffer 1.  
  > Khi vẽ xong xuôi, em nạp địa chỉ Framebuffer 1 vào thanh ghi CFBAR và kích hoạt bit nạp dập đứng VBR trong thanh ghi SRCR.  
  > Lúc này phần cứng LTDC sẽ đợi quét hết dòng 272 cuối cùng của khung hình cũ; ngay tại thời điểm chùm tia quay về đỉnh màn hình (khoảng Vertical Blanking), địa chỉ Framebuffer 1 mới được nạp tự động, đảm bảo màn hình chuyển khung hình mượt mà tuyệt đối không có vết xé."*

---

### Câu 10: "DMA2D Chrom-ART: Các chế độ hoạt động và ứng dụng cụ thể trong dự án?"

* **Kịch bản trả lời:**
  > *"Dạ, DMA2D là bộ tăng tốc đồ họa phần cứng chuyên dụng trên STM32F7. Trong dự án em ứng dụng 3 chế độ:  
  > Thứ nhất là chế độ R2M: Điền một màu cố định vào vùng nhớ RAM với tốc độ cực đại để xóa màn hình hoặc vẽ thanh đo.  
  > Thứ hai là chế độ M2M có chuyển đổi định dạng điểm ảnh PFC: Chuyển trực tiếp các icon từ định dạng ARGB8888 sang RGB565 của Framebuffer bằng phần cứng.  
  > Thứ ba là chế độ Blending: Trộn 2 lớp đồ họa với nhau theo kênh độ trong suốt Alpha Channel.  
  > Ngoài ra em cấu hình thanh ghi Line Offset OOR để khi vẽ icon nhỏ vào màn hình lớn, DMA2D tự động nhảy qua phần đệm thừa của dòng để vẽ dòng tiếp theo mà không làm xô lệch hình ảnh."*

---

### Câu 11: "FMC SDRAM: Chuỗi 5 lệnh JEDEC khởi tạo và công thức tính Refresh Rate Counter?"

* **Kịch bản trả lời:**
  > *"Dạ, để khởi động chip SDRAM ngoài, chuẩn JEDEC bắt buộc phải tuân theo chuỗi 5 lệnh: Cấp xung clock -> Phát lệnh Precharge All đưa các bank về trạng thái nghỉ -> Phát ít nhất 8 chu kỳ Auto-Refresh liên tiếp -> Nạp thanh ghi Mode Register để cấu hình CAS Latency bằng 2 -> Đưa SDRAM vào chế độ Normal Mode.  
  > Còn về thanh ghi Refresh Rate Counter: Chip SDRAM MT48LC4M32B2 có 4096 dòng và yêu cầu làm tươi toàn bộ trong 64 mili-giây, nghĩa là cứ 15.625 microsecond phải làm tươi một dòng.  
  > Bus SDRAM của em chạy ở 108 MHz, tức là chu kỳ xung là 9.26 nano-giây. Lấy 15.625 microsecond chia cho chu kỳ 9.26 nano-giây rồi trừ đi 20 chu kỳ dự phòng theo công thức của Reference Manual RM0385, em tính ra giá trị nạp vào thanh ghi FMC_SDRTR chính xác là 1667."*

* **Thông số kỹ thuật:**
  * Xung nhịp SDRAM: `108 MHz` (Chu kỳ `9.26 ns`).
  * Chu kỳ làm tươi 1 dòng: `64 ms / 4096 rows = 15.625 us`.
  * Giá trị nạp: `(15.625 us x 108 MHz) - 20 = 1687.5 - 20 = 1667`.

---

### Câu 12: "Chuẩn hóa thẻ MicroSD SDHC (4GB - 32GB) cho SDMMC: Tại sao bắt buộc dùng Block Addressing LBA?"

* **Kịch bản trả lời:**
  > *"Dạ, các dòng thẻ SDSC cũ dung lượng nhỏ hơn hoặc bằng 2GB thì dùng cơ chế Byte Addressing, tức là địa chỉ truyền vào lệnh đọc ghi là số thứ tự nhân với 512. Nhưng với thẻ SDHC từ 4GB đến 32GB, dung lượng vượt quá giới hạn 4GB của con số 32-bit, nếu nhân 512 sẽ làm tràn biến số nguyên uint32_t ngay lập tức.  
  > Vì vậy chuẩn SDHC bắt buộc chuyển sang cơ chế Block Addressing LBA: tham số truyền vào các lệnh CMD17, CMD18 hay CMD24 chính là số thứ tự của Sector, truyền thẳng mà tuyệt đối không nhân 512.  
  > Trong driver lúc gửi lệnh ACMD41, em bật bit HCS bằng 1; khi thẻ trả về thanh ghi OCR, em kiểm tra cờ CCS bằng 1 để xác nhận đúng thẻ SDHC rồi mới tiến hành đọc ghi."*

---

### Câu 13: "Sự cố D-Cache Coherency khi streaming video từ SDHC vào SDRAM và cách giải quyết triệt để?"

* **Kịch bản trả lời:**
  > *"Dạ, đây là lỗi bất đồng bộ dữ liệu giữa Cache và RAM trên Cortex-M7: Khối phần cứng SDMMC đọc dữ liệu từ thẻ nhớ nạp thẳng vào ô nhớ SDRAM ngoài mà không qua CPU. Nếu trước đó CPU đã từng đọc vùng nhớ này, CPU sẽ tiếp tục đọc dữ liệu cũ lưu trong L1 D-Cache thay vì đọc dữ liệu mới dưới RAM, làm màn hình bị vỡ hình và sọc rác.  
  > Để xử lý triệt để, em thực hiện 3 bước:  
  > Một là căn lề bộ đệm đúng 32 bytes khớp với độ dài một Cache Line.  
  > Hai là trước khi CPU đọc dữ liệu, gọi hàm `SCB_InvalidateDCache_by_Addr` để xóa hiệu lực dòng Cache cũ, ép CPU đọc trực tiếp từ SDRAM.  
  > Ba là gọi chỉ thị rào cản phần cứng DSB để đảm bảo toàn bộ thao tác bộ nhớ hoàn tất trước khi lệnh tiếp theo chạy."*

---

## 4.3. DỰ ÁN 3: ESP32-S3 Wearable Smartwatch

### Câu 14: "Tại sao phải tách Dual I2C Port? Nếu dùng chung thì hiện tượng gì xảy ra?"
* **Kịch bản trả lời:**
  > *"Dạ, cảm biến nhịp tim MAX30102 và cảm biến chuyển động BMI270 lấy mẫu liên tục với tần số cao. Trong khi đó, màn hình cảm ứng phát sinh ngắt không theo chu kỳ khi người dùng thao tác vuốt chạm.  
  > Nếu dùng chung 1 bus I2C, khi CPU đang bận truyền gói dữ liệu dài của cảm biến thì lệnh đọc tọa độ cảm ứng sẽ bị nghẽn lại, gây ra hiện tượng giật trễ cảm ứng rất khó chịu (độ trễ trên 50ms).  
  > Em tách riêng Port 0 cho Touch Controller và Port 1 cho mảng cảm biến, giúp phản hồi cảm ứng luôn mượt mà tức thì dưới 10ms."*

---

### Câu 15: "Cấu hình LVGL Buffer trong PSRAM: Trade-off giữa SRAM và PSRAM là gì?"
* **Kịch bản trả lời:**
  > *"Dạ, màn hình AMOLED 368x448 RGB565 chiếm khoảng 330KB RAM cho một khung hình, trong khi SRAM nội của ESP32-S3 chỉ còn khoảng 380KB cho ứng dụng. Nếu đặt Framebuffer trong SRAM nội thì hệ thống sẽ cạn kiệt RAM và không đủ chạy BLE hay WiFi stack.  
  > Vì vậy em đưa Framebuffer sang bộ nhớ ngoài Octal-SPI PSRAM 8MB.  
  > Về mặt trade-off, tốc độ truy cập PSRAM chậm hơn SRAM nội khoảng 3 lần. Em tối ưu bằng cách dùng cơ chế Double Partial Buffer kích thước bằng 1/10 màn hình đặt tại SRAM nội để LVGL render nhanh, sau đó dùng DMA đẩy dữ liệu song song ra màn hình. Nhờ đó tốc độ khung hình vẫn duy trì mượt mà 35 đến 45 FPS mà tiết kiệm được hơn 300KB SRAM nội."*

---

### Câu 16: "Dòng tiêu thụ 25 µA Standby: Giải trình tính khả thi thực tế?"
* **Kịch bản trả lời:**
  > *"Dạ, để đạt được mức 25 micro-ampe thì không thể chỉ gọi lệnh sleep thông thường mà vẫn cấp nguồn cho cảm biến được.  
  > Về phần cứng, em dùng mạch transistor PMOS ngắt hoàn toàn nguồn VCC của module GPS, chip nhịp tim và màn hình hiển thị.  
  > Vi điều khiển ESP32-S3 được đưa vào chế độ Deep Sleep, tắt toàn bộ lõi CPU chính và khối Radio Bluetooth/WiFi.  
  > Hệ thống chỉ duy trì nguồn cho khối ULP Coprocessor chạy ở tần số thấp và khối RTC Timer để nhận ngắt đếm bước chân từ cảm biến BMI270 và chờ ngắt thức giấc từ nút bấm vật lý. Dòng tiêu thụ đo thực tế dao động ổn định trong khoảng 22 đến 26 micro-ampe."*

---

### Câu 17: "BLE Sync Latency 20ms & GPS 1PPS Sync: Hiện thực thế nào?"
* **Kịch bản trả lời:**
  > *"Dạ về BLE: Em dùng hàm `esp_ble_gap_set_prefer_conn_params` thương lượng khoảng thời gian kết nối Connection Interval từ 15ms đến 20ms và đặt Slave Latency bằng 0, đảm bảo đồng hồ gửi dữ liệu lên điện thoại phản hồi ngay trong vòng 20ms.  
  > Còn về GPS: Chân xung 1PPS của module GPS có độ chính xác cấp nano-giây được em đưa vào chân ngắt GPIO của ESP32. Ngay khi có cạnh lên của xung 1PPS, ngắt ISR sẽ chốt giá trị bộ đếm microsecond timer nội bộ để tự động hiệu chỉnh sai số trôi dạt của thạch anh RTC trong đồng hồ, giữ độ chính xác thời gian chuẩn tuyệt đối."*

---

### Câu 18: "UI Watchdog Guard: Thuật toán phục hồi cụ thể khi GUI bị treo?"
* **Kịch bản trả lời:**
  > *"Dạ, em tạo một tác vụ Watchdog riêng biệt có mức ưu tiên cao nhất để giám sát một biến đếm nhịp tim `g_ui_heartbeat`.  
  > Trong vòng lặp chính của LVGL, cứ mỗi chu kỳ ứng dụng sẽ nạp lại biến đếm bằng 10. Cứ mỗi giây, watchdog task giảm biến đếm đi 1 đơn vị.  
  > Nếu biến đếm giảm về 0 (nghĩa là tác vụ đồ họa bị treo hoặc deadlock quá 10 giây): watchdog sẽ ghi lại nguyên nhân lỗi vào phân vùng RTC Memory, sau đó chủ động xóa và tạo lại task LVGL mới. Nếu sau 30 giây mà hệ thống vẫn không phản hồi thì mới kích hoạt ngắt Watchdog phần cứng để reboot toàn bộ chip."*

---

### Câu 19: "Khi chạy thực tế, Smartwatch hay gặp lỗi chạy tầm 5, 10 đến 30 phút thì bị đơ máy rồi tự reset. Bạn phân tích nguyên nhân gốc rễ và xử lý thế nào?"
* **Kịch bản trả lời:**
  > *"Trong hệ thống đồng hồ thông minh chạy FreeRTOS và thư viện đồ họa LVGL trên ESP32-S3, hiện tượng thiết bị bị đơ sau một thời gian vận hành bắt nguồn từ 3 cơ chế lỗi chính:  
  > - **Hiện tượng:** Thiết bị vận hành bình thường lúc ban đầu; sau 5 đến 30 phút, màn hình bị đóng băng, cảm ứng mất phản hồi, và sau 5 giây hệ thống tự động khởi động lại.  
  > - **3 Nguyên nhân kỹ thuật:**  
  >   1. **Rò rỉ và phân mảnh bộ nhớ Heap:** Mỗi giây khi nhận dữ liệu nhịp tim MAX30102 hoặc thông báo BLE, mã nguồn gọi các hàm định dạng chuỗi động (`lv_label_set_text_fmt`, cJSON) mà bỏ sót lệnh `free()` trong nhánh xử lý lỗi. Sau 15 - 30 phút, Heap cạn kiệt hoặc bị xé vụn, hàm cấp phát trả về `NULL`. Thao tác truy xuất vào con trỏ NULL kích hoạt ngoại lệ `LoadStoreProhibited` và gây reset chip.  
  >   2. **Xung đột an toàn luồng LVGL và Deadlock:** Thư viện LVGL không hỗ trợ thread-safe mặc định. Nếu tác vụ BLE hoặc cảm biến gọi trực tiếp hàm cập nhật giao diện mà không có Mutex bảo vệ (`lvgl_port_lock()`), hai lõi CPU cùng ghi vào cây Widget Tree gây vòng lặp vô hạn. Khi tác vụ đồ họa bị nghẽn quá 5 giây, Task Watchdog Timer (TWDT) không được nạp lại sẽ kích hoạt ngắt phần cứng khởi động lại hệ thống.  
  >   3. **Khóa bus I2C (SDA Stuck Low):** Khi cảm biến bị sụt áp hoặc nhiễu đường truyền, chân SDA có thể bị giữ ở mức thấp. Nếu driver I2C không cấu hình timeout, CPU sẽ chờ cờ phần cứng vô hạn, làm treo tác vụ cảm biến và kích hoạt Watchdog.  
  > - **Giải pháp 4 bước xử lý:**  
  >   1. **Kiến trúc truyền thông qua FreeRTOS Queue:** Tác vụ BLE và cảm biến không gọi trực tiếp hàm LVGL mà chỉ đóng gói dữ liệu vào Struct gửi vào Queue; duy nhất tác vụ giao diện đọc Queue và render, loại bỏ nguy cơ tranh chấp và Deadlock.  
  >   2. **Cơ chế phục hồi bus I2C qua 9 xung Clock:** Thiết lập timeout phần cứng 25ms cho mọi thao tác I2C. Nếu phát hiện chân SDA bị kẹt ở mức 0, CPU phát 9 xung clock trên đường SCL để giải phóng đường truyền.  
  >   3. **Kiểm soát bộ nhớ tĩnh:** Không dùng `malloc()` trong chu kỳ runtime; sử dụng bộ đệm tĩnh `static char` cho các chuỗi hiển thị. Giám sát `uxTaskGetStackHighWaterMark()` để bảo đảm ngăn xếp mỗi tác vụ luôn dư tối thiểu 1KB.  
  >   4. **Cấu hình Watchdog TWDT 5 giây kết hợp lưu vết RTC:** Thiết lập TWDT 5000ms. Khi phát sinh sự cố, hệ thống ghi mã lỗi và địa chỉ Program Counter vào vùng nhớ RTC trước khi reset để khôi phục trạng thái đếm bước chân mà không làm mất dữ liệu người dùng."*

---

## 4.4. DỰ ÁN 4 (Internship): Hệ Thống Giám Sát & Điều Khiển Tép Bạc

### Câu 20: "Bộ lọc Moving Average giảm nhiễu 40%: Window size bao nhiêu? Tại sao chọn kích thước đó?"
* **Kịch bản trả lời:**
  > *"Dạ, em chọn kích thước cửa sổ lọc N = 16 mẫu.  
  > Lý do thứ nhất là về tối ưu: vì 16 là lũy thừa của 2, nên phép chia lấy trung bình được trình biên dịch tối ưu thành phép dịch bit phải 4 vị trí (`>> 4`), không tốn chu kỳ chia số nguyên của CPU.  
  > Lý do thứ hai là về cân bằng giữa độ mịn và độ trễ: Trong môi trường ao tôm, máy bơm công suất lớn sinh ra rất nhiều xung nhiễu gai điện. Nếu chọn cửa sổ quá nhỏ như N=4 thì không lọc được nhiễu; còn nếu chọn N=64 thì tín hiệu quá trễ, không phát hiện kịp sự cố môi trường. Cửa sổ N=16 ở tần số lấy mẫu 10Hz mang lại độ trễ chỉ 1.6 giây, giảm 40% phương sai nhiễu mà vẫn đảm bảo phản hồi tức thì cho thuật toán điều khiển relay."*

---

### Câu 21: "Lỗi Timing trên đường truyền RS485: Bắt lỗi bằng Oscilloscope thế nào?"
* **Kịch bản trả lời:**
  > *"Dạ, đường truyền RS485 hoạt động ở chế độ bán song công, vi điều khiển phải điều khiển chân DE để chuyển đổi giữa chế độ truyền và nhận.  
  > Khi đo bằng máy hiện sóng, em kẹp kênh 1 vào chân DE và kênh 2 vào đường TX của vi điều khiển. Em phát hiện ra lỗi: code cũ khi thấy cờ TXE bật lên thì lập tức hạ chân DE về 0 để chuyển sang nhận. Nhưng lúc đó byte cuối cùng mới chỉ vừa chuyển từ thanh ghi DR sang Shift Register và chưa truyền xong ra đường dây, dẫn tới Stop bit bị cắt cụt giữa chừng và cảm biến báo lỗi Framing Error.  
  > Em sửa lại bằng cách chờ cờ TC (Transmission Complete) bật lên rồi mới hạ chân DE, đảm bảo toàn bộ byte kể cả Stop bit đã rời khỏi chân truyền vật lý an toàn."*

---

### Câu 22: "MQTT 99.8% Uptime & Tối ưu hóa Latency FreeRTOS: Con số 0.2% từ đâu ra?"
* **Kịch bản trả lời:**
  > *"Dạ, con số 0.2% Downtime tương đương khoảng 2.8 phút mất kết nối mỗi ngày. Phân tích log thực tế cho thấy nguyên nhân chủ yếu do sóng WiFi ngoài trang trại bị suy hao khi trời mưa giông hoặc router cấp phát lại IP. Em áp dụng thuật toán Exponential Backoff with Jitter để tự động kết nối lại an toàn, đồng thời lưu tạm dữ liệu vào bộ nhớ Flash để đẩy bù khi có mạng.  
  > Còn về độ trễ FreeRTOS giảm từ 120ms xuống dưới 50ms: em tách rời tác vụ đọc cảm biến chậm khỏi tác vụ truyền tin mạng, đồng thời chuyển sang dùng Direct-to-Task Notifications thay vì Semaphore để đánh thức tác vụ ngay lập tức, giảm thời gian chuyển đổi ngữ cảnh từ 15 microsecond xuống dưới 3 microsecond."*


---

# PHẦN 5: TÌNH HUỐNG THỰC TẾ, QUY CHUẨN GIT VÀ KỊCH BẢN PHỎNG VẤN

### 5.1. Xử lý sự cố Thread RTOS bị treo / không được cấp CPU
1. **Kiểm tra Deadlock do Mutex**: Xem Thread có đang chờ một Mutex bị giữ bởi tác vụ khác không nhả ra.
2. **Kiểm tra Starvation do Priority**: Kiểm tra xem có Thread ưu tiên cao hơn đang chạy vòng lặp liên tục (`while(1)` không có `k_sleep` hoặc `yield`) làm bộ lập lịch không bao giờ nhường quyền.
3. **Kiểm tra Stack Overflow**: Kiểm tra cờ lỗi MPU Stack Guard xem Thread có bị vỡ ngữ cảnh do tràn ngăn xếp hay không.

---

### 5.2. Quy trình 3 bước xử lý khi mạng CAN Bus bị nhiễu cao
1. **Đo trở đầu cuối khi tắt nguồn**: Đo điện trở giữa chân CAN_H và CAN_L. Giá trị chuẩn bắt buộc phải là 60 Ohm (sai số 5%). Nếu đo được 120 Ohm nghĩa là đứt 1 trở; nếu đo ra 0 Ohm là chập dây.
2. **Đồng bộ hóa Sample Point**: Sử dụng Logic Analyzer kiểm tra cấu hình Bit Timing giữa các node trên bus có lệch nhau không (chuẩn phải đồng nhất tại 83.3% đến 87.5%).
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

> *"Em chào anh/chị. Em là [Tên], tốt nghiệp chuyên ngành Kỹ thuật Máy tính/Điện Tử. Định hướng của em là trở thành một Kỹ sư Lập trình Nhúng Firmware chuyên sâu về kiến trúc hệ điều hành và giao thức ô tô.  
> 
> - **Situation & Task:** Trong quá trình học tập, nhận thấy các dự án sinh viên thường chỉ dừng lại ở mức gọi thư viện HAL cơ bản, em đặt mục tiêu làm chủ vi điều khiển từ tầng thanh ghi Bare-metal cho đến hệ điều hành chuyên dụng chuẩn công nghiệp Zephyr RTOS trên chip ARM Cortex-M7.  
> - **Action:** Em đã xây dựng thành công 2 dự án kỹ thuật độc lập: Một cụm Telematics & CAN Gateway chạy Zephyr RTOS đạt chuẩn an toàn AUTOSAR E2E, lọc phần cứng 6 filter banks và xử lý tín hiệu DBC bằng Fixed-Point; và một hệ thống Bare-Metal điều khiển trực tiếp thanh ghi FMC SDRAM và bộ tăng tốc đồ họa DMA2D đạt tốc độ hiển thị 60 FPS không giật xé hình.  
> - **Result:** Toàn bộ mã nguồn em đều tổ chức theo chuẩn kiến trúc module, đo lường định lượng bằng DWT Cycle Counter và viết Unit Test tự động. Với nền tảng hiểu sâu bản chất phần cứng và tư duy làm phần mềm chuẩn hóa, em tin mình sẽ nhanh chóng đóng góp hiệu quả vào các dự án nhúng tại quý công ty."*
