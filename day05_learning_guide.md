# 🏆 [NGÀY 5] CẨM NANG TOÀN DIỆN BARE-METAL: BỘ TĂNG TỐC ĐỒ HỌA DMA2D (CHROM-ART) & THIẾT KẾ PHÂN TẦNG ƯU TIÊN NGẮT NVIC
## Lộ trình 4 Bước: Nguyên Lý Phần Cứng ➔ Thực Chiến RM0385 ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ bộ tăng tốc đồ họa 2D phần cứng độc quyền Chrom-ART (DMA2D) trên STM32F746, thực hiện tô màu vùng nhớ (Register-to-Memory Color Fill), sao chép ảnh có chuyển đổi định dạng màu (Pixel Format Conversion PFC: ARGB8888 sang RGB565) và hòa trộn hai lớp ảnh (Alpha Blending) với tải CPU xấp xỉ 0%. Đồng thời thiết lập kiến trúc phân tầng ưu tiên ngắt NVIC (NVIC Priority Architecture) chuẩn công nghiệp ô tô, triệt tiêu hiện tượng nghẽn ngắt (Interrupt Jitter / Priority Inversion) giữa CAN Bus, UART DMA và Render màn hình.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, thanh ghi, công thức toán học, bảng tra cứu và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 5                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: NGUYÊN LÝ │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE            │ BƯỚC 4: PHỎNG VẤN          │
│ • Cơ chế DMA2D    │ • Tra cứu RM0385  │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn DMA2D   │
│   Chrom-ART Engine│   & PM0253        │ • TODO 1-2 [dma2d.h]       │   & NVIC Grouping          │
│ • 4 Chế độ DMA2D  │ • Bảng Base Addr  │ • TODO 3-5 [dma2d.c]       │ • Line Offset Skew Trap    │
│ • Toán Offset Line│ • Bảng Thanh ghi  │ • TODO 6 [nvic_config.h/c] │ • Priority Inversion Bug   │
│ • Kiến trúc NVIC  │ • Bảng Cờ W1C     │ • TODO 7 [main.c]          │ • Kịch bản trả lời 60s     │
│ • Priority Groups │ • Bảng NVIC Map   │ • Mổ xẻ 5 Bug phần cứng    │   (Elevator Pitch)         │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC BARE-METAL CỐT LÕI (CHUYÊN CHO NGÀY 5)

| STT | Quy tắc Bare-metal | Thể hiện cụ thể trong Ngày 5 (DMA2D & NVIC) |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `DMA2D_CR` (`0x0000 0000`), `DMA2D_OPFCCR` (`0x0000 0000` - ARGB8888), và thanh ghi phân nhóm ngắt `SCB->AIRCR` trước khi ghi đè. |
| **2** | **Access Type** | **CỰC KỲ QUAN TRỌNG:** Thanh ghi `DMA2D_IFCR` (Interrupt Flag Clear Register) là dạng `w` (Write-only to clear). Tuyệt đối không dùng `DMA2D->IFCR |= FLAG`, phải ghi gán trực tiếp `DMA2D->IFCR = DMA2D_IFCR_CTCIF` để không vô tình xóa nhầm cờ báo lỗi truyền cấu hình sai `TEIF` (Transfer Error Interrupt Flag)! |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường đa bit: `MODE[1:0]` (Bits 17:16 trong `DMA2D_CR`), `CM[3:0]` (Color Mode trong `DMA2D_OPFCCR`), và `PRIGROUP[2:0]` (Bits 10:8 trong `SCB->AIRCR`). |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi DMA2D và cờ đồng bộ `volatile uint8_t dma2d_transfer_complete` giữa ISR và Main loop bắt buộc dùng `volatile`. |
| **5** | **Interrupt Workflow** | Quy trình 5 bước ngắt DMA2D: Cờ phần cứng `TCIF` dựng $\rightarrow$ Bật `DMA2D_CR_TCIE` $\rightarrow$ Bật `NVIC_EnableIRQ(DMA2D_IRQn)` $\rightarrow$ Chạy `DMA2D_IRQHandler` $\rightarrow$ Xóa cờ ngắt bằng `DMA2D->IFCR = DMA2D_IFCR_CTCIF`. |
| **6** | **RM / DS Lookup** | Cung cấp chính xác Chapter, Section, từ khóa `Ctrl + F`, công thức `Base Address + Offset` cho `DMA2D` (RM0385 Chapter 10) và `NVIC` (PM0253 Chapter 4). |
| **7** | **Hardware Rationale & Specs** | Bố trí cơ sở kỹ thuật, giới hạn băng thông AXI Bus Matrix, bảng phân bổ 16 mức ưu tiên Preemption Priority phục vụ an toàn hệ thống (CAN > UART > DMA2D > LTDC > SysTick). |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ PHẦN CỨNG & CƠ CHẾ VẬT LÝ (HARDWARE ARCHITECTURE)

## 1.1. Bản chất Phần cứng Khối Tăng Tốc Đồ Họa DMA2D (Chrom-ART Accelerator)

Trong hệ thống nhúng hiển thị (GUI), việc CPU phải chạy vòng lặp `for` để tô màu hoặc sao chép từng pixel trên màn hình $480 \times 272$ (tổng cộng $130,560\text{ pixels} \times 2\text{ bytes} = 261.12\text{ KB}$ mỗi khung hình) sẽ chiếm dụng tới **$85\% - 95\%$ thời gian xử lý của CPU**, làm gián đoạn việc nhận gói tin CAN Bus và xử lý giao tiếp thời gian thực.

Khối **DMA2D (Chrom-ART Accelerator)** là một mạch phần cứng chuyên dụng độc lập nằm trên Bus Master AXI:
* **Giao tiếp Bus:** Nối trực tiếp vào AXI Bus Matrix 64-bit, có khả năng phát các chuỗi đọc/ghi Burst liên tục với bộ nhớ SDRAM ngoài và SRAM nội.
* **Tải CPU:** Hoàn toàn bằng **$0\%$** trong suốt quá trình copy, fill màu hoặc hòa trộn Alpha (CPU chỉ cần nạp địa chỉ, kích hoạt bit `START`, và quay sang làm việc khác hoặc đi ngủ chờ ngắt).
* **Hiệu năng:** Tốc độ đổ màu và copy đạt tối đa băng thông bộ nhớ FMC SDRAM ($108\text{ MHz} \times 16\text{ bits} = 216\text{ MB/s}$ lý thuyết), nhanh gấp **8 đến 12 lần** so với lệnh `memcpy()` hoặc vòng lặp C thuần của CPU.

```text
                                  ┌──────────────────────────────────────────────────┐
                                  │             ARM CORTEX-M7 CORE                   │
                                  │      (Giải phóng 100% khi vẽ đồ họa)             │
                                  └────────────────────────┬─────────────────────────┘
                                                           │
                                                           ▼ (Phát lệnh Start)
┌──────────────────────────────────────────────────────────┴──────────────────────────────────────────────────────┐
│                                           AXI / AHB BUS MATRIX                                                  │
├────────────────────────────────┬───────────────────────────────────────────────┬────────────────────────────────┤
│                                │                                               │                                │
│   ┌────────────────────────┐   │   ┌───────────────────────────────────────┐   │   ┌────────────────────────┐   │
│   │   SRAM Nội / Flash     │ ◄─┼── │          DMA2D CHROM-ART              │ ──┼─► │       FMC SDRAM        │   │
│   │   (Chứa Icon / Font)   │   │   │  ┌─────────────────────────────────┐  │   │   │  (Chứa Framebuffer     │   │
│   └────────────────────────┘   │   │  │ Khối PFC (Chuyển đổi hệ màu)   │  │   │   │   Hiển thị màn hình)   │   │
│                                │   │  ├─────────────────────────────────┤  │   │   └────────────────────────┘   │
│                                │   │  │ Khối Blending (Alpha Mixer)     │  │   │                                │
│                                │   │  └─────────────────────────────────┘  │   │                                │
│                                │   └───────────────────────────────────────┘   │                                │
└────────────────────────────────┴───────────────────────────────────────────────┴────────────────────────────────┘
```

---

## 1.2. Bốn Chế Độ Hoạt Động Cốt Lõi của DMA2D (`MODE[1:0]` trong `DMA2D_CR`)

| Chế độ (`MODE[1:0]`) | Tên kỹ thuật | Cơ chế phần cứng | Ứng dụng thực tế trong Dự án |
| :---: | :--- | :--- | :--- |
| **`00`b** | **Memory-to-Memory (M2M)** | Copy trực tiếp khối pixel từ bộ nhớ nguồn sang bộ nhớ đích mà **không thay đổi định dạng màu**. | Sao chép nhanh bộ đệm phụ (Back Buffer) sang bộ đệm chính (Front Buffer). |
| **`01`b** | **M2M with Pixel Format Conversion (PFC)** | Đọc pixel từ bộ nhớ nguồn, tự động giải mã và chuyển đổi hệ màu (ví dụ từ RGB565 sang ARGB8888 hoặc ARGB4444) trước khi ghi vào đích. | Nạp các icon định dạng nén từ Flash vào Framebuffer SDRAM. |
| **`10`b** | **M2M with Blending** | Đọc đồng thời 2 lớp ảnh: Lớp tiền cảnh (Foreground) và Lớp hậu cảnh (Background), hòa trộn pixel theo trọng số kênh Alpha ($\alpha$), rồi ghi ra đích. | Vẽ kim đồng hồ trong suốt đè lên mặt đồng hồ tốc độ xe hơi. |
| **`11`b** | **Register-to-Memory (R2M)** | Ghi trực tiếp giá trị màu định sẵn trong thanh ghi `DMA2D_OCOLR` vào toàn bộ khối pixel đích mà **không cần đọc bất kỳ vùng nhớ nguồn nào**. | Xóa trắng màn hình (Clear Screen) hoặc vẽ các thanh đo tốc độ, hộp thoại chữ nhật siêu tốc. |

---

## 1.3. Công Thức Toán Học Tính Line Offset Tránh Lỗi Xéo Hình (Skewed Image Bug)

Khi vẽ một hình chữ nhật nhỏ có kích thước $W_{box} \times H_{box}$ vào bên trong một Framebuffer lớn có kích thước $W_{screen} \times H_{screen}$:

```text
   Vùng nhớ Framebuffer Đích trên SDRAM (Chiều rộng W_screen = 480)
┌────────────────────────────────────────────────────────────────────────┐
│                                                                        │
│         (X_pos, Y_pos)                                                 │
│               ┌───────────────────────┐                                │
│               │                       │                                │
│               │   Hình chữ nhật con   │ H_box                          │
│               │   cần vẽ              │                                │
│               │                       │                                │
│               └───────────────────────┘                                │
│                         W_box                                          │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

1. **Địa chỉ Pixel khởi đầu (Output Memory Address - `OMAR`):**
   $$\text{Start Address} = \text{Base Address} + (Y_{pos} \times W_{screen} + X_{pos}) \times \text{BytesPerPixel}$$
   * Với định dạng **RGB565** ($\text{BytesPerPixel} = 2$):
     $$\text{OMAR} = \texttt{0xC0000000} + 2 \times (Y_{pos} \times 480 + X_{pos})$$

2. **Độ lệch dòng đích (Output Line Offset - `OOR`):**
   Sau khi DMA2D vẽ xong $W_{box}$ pixels của một dòng, con trỏ phần cứng phải **nhảy cóc qua phần còn lại của màn hình** để xuống đúng đầu dòng tiếp theo:
   $$\mathbf{\text{Line Offset (OOR)}} = W_{screen} - W_{box} = 480 - W_{box}$$
   * ⚠️ **Lưu ý sống còn:** Giá trị ghi vào `DMA2D_OOR` tính bằng **đơn vị số pixel**, KHÔNG PHẢI số byte! Nếu ghi sai thành byte, hình ảnh sẽ bị xé xéo thành các dải sọc chéo trên màn hình.

---

## 1.4. Kiến Trúc Phân Tầng Ưu Tiên Ngắt NVIC (Nested Vectored Interrupt Controller)

Lõi ARM Cortex-M7 hỗ trợ 16 mức ưu tiên ngắt phần cứng (từ 0 đến 15, số càng nhỏ thì ưu tiên càng cao). Thanh ghi điều khiển phân nhóm ngắt **`SCB->AIRCR` (Application Interrupt and Reset Control Register)** chứa trường `PRIGROUP[2:0]` (Bits 10:8):

### Bảng Phân Bổ Nhóm Ưu Tiên (Priority Grouping):
Trong dự án này, ta sử dụng chuẩn công nghiệp ô tô: **`NVIC_PriorityGroup_4` (`PRIGROUP = 011`b)**:
* Toàn bộ 4 bits phần cứng được dùng cho **Preemption Priority** (Mức ưu tiên ngắt chen ngang: 16 mức từ 0 đến 15).
* $0$ bit dành cho Subpriority.
* **Nguyên lý Chen ngang (Preemption):** Một ngắt có Preemption Priority cao hơn (số bé hơn) ĐƯỢC PHÉP ngắt ngang thân ISR của một ngắt có Preemption Priority thấp hơn đang thực thi.

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│              BẢNG THIẾT KẾ PHÂN TẦNG ƯU TIÊN NGẮT (NVIC PRIORITY MATRIX)                        │
├──────────────┬──────────────┬──────────────┬────────────────────────────────────────────────────┤
│ Tên Ngắt IRQ │ Vector IRQn  │ Ưu Tiên (0-15)│ Cơ Sở Kỹ Thuật & Lý Do Thiết Kế                    │
├──────────────┼──────────────┼──────────────┼────────────────────────────────────────────────────┤
│ CAN1_RX0_IRQn│ IRQn = 19    │ **Priority 1**│ CỰC CAO: Buffer phần cứng bxCAN chỉ có 3 tầng FIFO.│
│              │              │              │ Nếu không đọc kịp, gói tin CAN mạng ô tô bị tràn   │
│              │              │              │ (Overrun) gây mất an toàn hệ thống.                │
├──────────────┼──────────────┼──────────────┼────────────────────────────────────────────────────┤
│ USART1_IRQn  │ IRQn = 37    │ **Priority 2**│ CAO: Xử lý sự kiện IDLE Line và lỗi ORE/FE của DMA │
│              │              │              │ Circular Ring Buffer.                              │
├──────────────┼──────────────┼──────────────┼────────────────────────────────────────────────────┤
│ DMA2D_IRQn   │ IRQn = 90    │ **Priority 3**│ TRUNG BÌNH: Báo hiệu hoàn tất vẽ khung hình đồ họa.│
├──────────────┼──────────────┼──────────────┼────────────────────────────────────────────────────┤
│ LTDC_IRQn    │ IRQn = 88    │ **Priority 4**│ TRUNG BÌNH THẤP: Đồng bộ VSYNC và Line Event.      │
├──────────────┼──────────────┼──────────────┼────────────────────────────────────────────────────┤
│ SysTick_IRQn │ Exception -1 │ **Priority 15**│ THẤP NHẤT: Đếm mili-giây hệ thống. Tuyệt đối không │
│              │              │              │ để SysTick ngắt ngang các ISR truyền thông khẩn cấp│
└──────────────┴──────────────┴──────────────┴────────────────────────────────────────────────────┘
```

---

# 📑 BƯỚC 2: THỰC CHIẾN & TRA CỨU RM0385 / PM0253 (SETUP & LOOKUP)

## 2.1. Bản đồ Địa chỉ Base Address & Vector Ngắt Ngày 5

Tra cứu RM0385 *Chapter 2: Memory map* và PM0253 *Chapter 4: Core peripherals*:

| Tên ngoại vi | Bus | Base Address | Offset | Địa chỉ tuyệt đối | Vector IRQn |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`DMA2D`** | AHB1 | `0x4002 0000` | `0xB000` | `0x4002 B000` | `DMA2D_IRQn = 90` |
| **`SCB (AIRCR)`**| System Bus | `0xE000 ED00` | `0x000C` | `0xE000 ED0C` | Quản lý phân nhóm ngắt lõi Core |
| **`NVIC`** | System Bus | `0xE000 E100` | `0x0000` | `0xE000 E100` | Kích hoạt và gán priority IRQ |

---

## 2.2. Bảng Tra cứu Thanh ghi `DMA2D` Chi tiết

Tra cứu RM0385 *Chapter 10: DMA2D controller $\rightarrow$ Section 10.4: DMA2D registers*:

| Thanh ghi | Offset | Reset Value | Bit / Trường | Access | Mô tả & Cấu hình Kỹ thuật |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`DMA2D_CR`** | `0x00` | `0x0000 0000` | `START` (Bit 0) | `RW` | Ghi `1` để bắt đầu truyền. Phần cứng tự xóa về `0` khi hoàn tất. |
| | | | `TCIE` (Bit 9) | `RW` | Ghi `1` để bật ngắt Transfer Complete Interrupt. |
| | | | `TEIE` (Bit 8) | `RW` | Ghi `1` để bật ngắt Transfer Error Interrupt. |
| | | | `MODE[1:0]` (Bit 17:16)| `RW` | Chế độ: `00` M2M, `01` M2M+PFC, `10` Blending, `11` R2M (Color fill). |
| **`DMA2D_ISR`** | `0x04` | `0x0000 0000` | `TCIF` (Bit 1) | `RO` | Cờ báo truyền hoàn tất (Transfer Complete Interrupt Flag). |
| | | | `TEIF` (Bit 0) | `RO` | Cờ báo lỗi cấu hình truyền (Transfer Error Interrupt Flag). |
| **`DMA2D_IFCR`**| `0x08` | `0x0000 0000` | `CTCIF` (Bit 1)| `w` | **W1C:** Ghi `1` để xóa cờ `TCIF`. CẤM DÙNG `\|=`. |
| | | | `CTEIF` (Bit 0)| `w` | **W1C:** Ghi `1` để xóa cờ `TEIF`. CẤM DÙNG `\|=`. |
| **`DMA2D_OMAR`**| `0x3C` | `0x0000 0000` | `MA[31:0]` | `RW` | Địa chỉ vùng nhớ đích (Output Memory Address trong SDRAM). |
| **`DMA2D_OOR`** | `0x40` | `0x0000 0000` | `LO[13:0]` | `RW` | Độ lệch dòng đích (Line Offset tính bằng số pixel: $W_{screen} - W_{box}$). |
| **`DMA2D_NLR`** | `0x44` | `0x0000 0000` | `NL[15:0]` (31:16)| `RW` | Số dòng cần truyền (Number of Lines = $H_{box}$). |
| | | | `PL[13:0]` (13:0) | `RW` | Số pixel trên mỗi dòng (Pixels per Line = $W_{box}$). |
| **`DMA2D_OCOLR`**| `0x48`| `0x0000 0000` | `COLOR[31:0]`| `RW` | Mã màu xuất (trong chế độ R2M: định dạng RGB565 hoặc ARGB8888). |
| **`DMA2D_OPFCCR`**|`0x34`| `0x0000 0000` | `CM[2:0]` (Bit 2:0)| `RW` | Định dạng màu đích: `000` ARGB8888, `001` RGB888, `010` RGB565. |

---

# 💻 BƯỚC 3: GÕ CODE & MỔ XẺ BUG PHẦN CỨNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 5

```text
drivers/
├── inc/
│   ├── dma2d.h        <-- Khai báo API đồ họa DMA2D (Fill, Copy, Blend)
│   └── nvic_config.h  <-- Khai báo API quản lý phân tầng ưu tiên ngắt NVIC
└── src/
    ├── dma2d.c        <-- Triển khai driver DMA2D Bare-metal & ISR
    └── nvic_config.c  <-- Cấu hình Priority Grouping 4 và phân bổ Priority
src/
└── main.c             <-- Demo vẽ giao diện táp-lô xe hơi siêu tốc và test ngắt
```

---

### 📂 KHỐI 1: FILE HEADER GIAO DIỆN DMA2D [ `drivers/inc/dma2d.h` ]

#### TODO 1 [File: `drivers/inc/dma2d.h`]: Khai báo API Đồ Họa Tăng Tốc Phần Cứng
```c
#ifndef DMA2D_H
#define DMA2D_H

#include <stdint.h>

/* Kích thước màn hình Rocktech Discovery LCD */
#define LCD_SCREEN_WIDTH   480U
#define LCD_SCREEN_HEIGHT  272U

/* Các mã màu RGB565 thông dụng */
#define COLOR_BLACK        0x0000U
#define COLOR_WHITE        0xFFFFU
#define COLOR_RED          0xF800U
#define COLOR_GREEN        0x07E0U
#define COLOR_BLUE         0x001FU
#define COLOR_GRAY         0x7BEFU
#define COLOR_YELLOW       0xFFE0U

/**
 * @brief Khởi tạo khối đồ họa phần cứng DMA2D & bật ngắt NVIC
 */
void DMA2D_Init(void);

/**
 * @brief Tô màu đơn sắc siêu tốc vào vùng chữ nhật (Register-to-Memory)
 * @param dst_addr Địa chỉ pixel bắt đầu trên Framebuffer (SDRAM)
 * @param x Tọa độ X góc trên bên trái
 * @param y Tọa độ Y góc trên bên trái
 * @param width Chiều rộng hình chữ nhật
 * @param height Chiều cao hình chữ nhật
 * @param rgb565_color Mã màu 16-bit RGB565
 */
void DMA2D_FillRect(uint32_t dst_base, uint16_t x, uint16_t y, 
                    uint16_t width, uint16_t height, uint16_t rgb565_color);

/**
 * @brief Chờ quá trình truyền DMA2D hoàn tất (Polling an toàn kèm timeout)
 */
uint8_t DMA2D_WaitTransferComplete(uint32_t timeout_cycles);

#endif /* DMA2D_H */
```

---

### 📂 KHỐI 2: FILE SOURCE DRIVER DMA2D [ `drivers/src/dma2d.c` ]

#### TODO 2 [File: `drivers/src/dma2d.c`]: Khởi Tạo Cấp Xung & Xử Lý Ngắt DMA2D
```c
#include "dma2d.h"
#include "Reg.h"

/* Biến cờ volatile báo hiệu hoàn tất truyền từ ngắt */
static volatile uint8_t g_dma2d_busy = 0;

void DMA2D_Init(void)
{
    /* BƯỚC 1: Cấp clock cho ngoại vi DMA2D trên AHB1 */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2DEN;

    /* Đảm bảo trạng thái dừng ban đầu */
    DMA2D->CR = 0;

    /* BƯỚC 2: Xóa cờ ngắt tồn đọng từ lần chạy trước */
    DMA2D->IFCR = (DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTEIF);

    /* BƯỚC 3: Bật cờ ngắt hoàn thành truyền (TCIE) và ngắt lỗi (TEIE) */
    DMA2D->CR |= (DMA2D_CR_TCIE | DMA2D_CR_TEIE);

    /* BƯỚC 4: Kích hoạt NVIC cho DMA2D (Priority được gán ở nvic_config.c) */
    NVIC_EnableIRQ(DMA2D_IRQn);
}

void DMA2D_IRQHandler(void)
{
    /* Kiểm tra cờ ngắt Transfer Complete */
    if (DMA2D->ISR & DMA2D_ISR_TCIF) {
        /* BẮT BUỘC: Ghi trực tiếp vào IFCR để xóa cờ (W1C) - CẤM DÙNG |= */
        DMA2D->IFCR = DMA2D_IFCR_CTCIF;
        g_dma2d_busy = 0;
    }

    /* Kiểm tra cờ ngắt Transfer Error */
    if (DMA2D->ISR & DMA2D_ISR_TEIF) {
        DMA2D->IFCR = DMA2D_IFCR_CTEIF;
        g_dma2d_busy = 0;
        /* Xử lý lỗi cấu hình vùng nhớ vượt biên */
    }
}
```

#### TODO 3 [File: `drivers/src/dma2d.c`]: Hàm Vẽ Khối Chữ Nhật R2M Siêu Tốc
```c
void DMA2D_FillRect(uint32_t dst_base, uint16_t x, uint16_t y, 
                    uint16_t width, uint16_t height, uint16_t rgb565_color)
{
    /* Bảo vệ an toàn: Chống tràn biên màn hình */
    if ((x + width > LCD_SCREEN_WIDTH) || (y + height > LCD_SCREEN_HEIGHT)) {
        return;
    }

    /* Chờ lượt truyền trước hoàn tất */
    DMA2D_WaitTransferComplete(500000);

    g_dma2d_busy = 1;

    /* 1. Tính toán địa chỉ byte khởi đầu trên SDRAM (Mỗi pixel RGB565 chiếm 2 bytes) */
    uint32_t start_addr = dst_base + (2U * (y * LCD_SCREEN_WIDTH + x));
    DMA2D->OMAR = start_addr;

    /* 2. Cấu hình Line Offset (OOR): Số pixel còn lại của dòng trên màn hình */
    DMA2D->OOR = (LCD_SCREEN_WIDTH - width);

    /* 3. Cấu hình Số dòng (NL) và Số pixel mỗi dòng (PL) */
    DMA2D->NLR = ((uint32_t)height << 16) | (uint32_t)width;

    /* 4. Cấu hình định dạng màu đích: RGB565 (Mã 010b = 0x2) */
    DMA2D->OPFCCR = (0x2U << 0);

    /* 5. Nạp mã màu vào thanh ghi Output Color Register */
    DMA2D->OCOLR = rgb565_color;

    /* 6. Thiết lập Chế độ: Register-to-Memory (MODE = 11b = 0x3) và KÍCH HOẠT START */
    DMA2D->CR = (0x3U << 16) | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_START;
}

uint8_t DMA2D_WaitTransferComplete(uint32_t timeout_cycles)
{
    while (g_dma2d_busy) {
        if (timeout_cycles-- == 0) {
            /* Dừng cưỡng bức nếu lỗi phần cứng */
            DMA2D->CR &= ~DMA2D_CR_START;
            g_dma2d_busy = 0;
            return 0; /* Lỗi timeout */
        }
    }
    return 1; /* Thành công */
}
```

---

### 📂 KHỐI 3: THIẾT KẾ PHÂN TẦNG ƯU TIÊN NGẮT [ `drivers/src/nvic_config.c` ]

#### TODO 4 [File: `drivers/inc/nvic_config.h`]: Khai Báo API Cấu Hình NVIC
```c
#ifndef NVIC_CONFIG_H
#define NVIC_CONFIG_H

#include <stdint.h>

void System_NVIC_Priority_Init(void);

#endif /* NVIC_CONFIG_H */
```

#### TODO 5 [File: `drivers/src/nvic_config.c`]: Cấu Hình Nhóm Phân Tầng Ưu Tiên Chuẩn Automotive
```c
#include "nvic_config.h"
#include "Reg.h"

void System_NVIC_Priority_Init(void)
{
    /* BƯỚC 1: Cấu hình Phân nhóm ưu tiên (Priority Grouping 4) */
    /* Ghi khóa an toàn VECTKEY 0x5FA vào SCB->AIRCR kèm PRIGROUP = 011b */
    /* Toàn bộ 4 bits = 16 Preemption Priorities, 0 Subpriority */
    uint32_t aircr = SCB->AIRCR;
    aircr &= ~((0xFFFFU << 16) | (0x7U << 8));
    aircr |=  ((0x05FAU << 16) | (0x3U << 8));
    SCB->AIRCR = aircr;

    /* BƯỚC 2: Phân bổ Priority theo ma trận an toàn hệ thống (Số nhỏ = Ưu tiên cao) */
    
    /* 1. CAN1 RX0 (Ưu tiên 1 - CỰC CAO: Đảm bảo không mất gói tin an toàn xe) */
    NVIC_SetPriority(CAN1_RX0_IRQn, 1);

    /* 2. USART1 DMA RX (Ưu tiên 2 - CAO: Quản lý IDLE line và Ring buffer) */
    NVIC_SetPriority(USART1_IRQn, 2);
    NVIC_SetPriority(DMA2_Stream2_IRQn, 2);

    /* 3. DMA2D (Ưu tiên 3 - TRUNG BÌNH: Đồ họa giao diện) */
    NVIC_SetPriority(DMA2D_IRQn, 3);

    /* 4. LTDC VSYNC / Line Event (Ưu tiên 4 - TRUNG BÌNH THẤP: Quét hiển thị) */
    NVIC_SetPriority(LTDC_IRQn, 4);

    /* 5. SysTick (Ưu tiên 15 - THẤP NHẤT: Tránh làm trễ các ISR truyền thông) */
    NVIC_SetPriority(SysTick_IRQn, 15);
}
```

---

### 📂 KHỐI 4: TÍCH HỢP HỆ THỐNG [ `src/main.c` ]

#### TODO 6 [File: `src/main.c`]: Vẽ Giao Diện Tốc Độ Bằng DMA2D Dưới Tải CAN Bus
```c
#include "Sys_Clock.h"
#include "uart_dma.h"
#include "can.h"
#include "sdram.h"
#include "ltdc.h"
#include "dma2d.h"
#include "nvic_config.h"

int main(void)
{
    /* 1. Khởi tạo Clock 216MHz Over-Drive */
    System_Clock_Init();

    /* 2. Khởi tạo phân tầng ưu tiên ngắt NVIC */
    System_NVIC_Priority_Init();

    /* 3. Khởi tạo bộ nhớ ngoài SDRAM & Bộ quét LCD LTDC */
    SDRAM_Init();
    LTDC_Init();

    /* 4. Khởi tạo bộ tăng tốc đồ họa DMA2D */
    DMA2D_Init();

    /* 5. Khởi tạo mạng CAN Bus 500kbps & UART DMA */
    CAN_Init();
    UART_DMA_Init();

    /* Xóa sạch nền màn hình sang màu đen (480x272) bằng DMA2D (tốn < 2ms, CPU 0%) */
    DMA2D_FillRect(SDRAM_FRAMEBUFFER_ADDR, 0, 0, 480, 272, COLOR_BLACK);

    /* Vẽ hộp táp-lô tốc độ nền xám (200x80) */
    DMA2D_FillRect(SDRAM_FRAMEBUFFER_ADDR, 140, 96, 200, 80, COLOR_GRAY);

    /* Vẽ thanh đo tốc độ màu xanh (Progress Bar 180x20) */
    DMA2D_FillRect(SDRAM_FRAMEBUFFER_ADDR, 150, 140, 180, 20, COLOR_GREEN);

    while (1) {
        /* CPU hoàn toàn rảnh rỗi để giải mã gói tin CAN Bus và truyền thông */
        __WFI(); /* Chờ ngắt tiết kiệm năng lượng */
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug Phần Cứng "Kinh Điển" trong Ngày 5

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY PHẦN CỨNG DMA2D & NVIC                                    │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN CỨNG             │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Hình vẽ bị xé  │ Quên tính `DMA2D_OOR` hoặc tính nhầm bằng │ `OOR = W_screen - W_box`.       │
│    xéo góc (Skew) │ byte thay vì số pixel.                    │ Tuyệt đối không nhân 2 byte.    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Treo cứng CPU  │ Ghi vào `DMA2D->CR` khi chưa bật clock    │ Bật `RCC_AHB1ENR_DMA2DEN`       │
│    (HardFault)    │ trong `RCC_AHB1ENR`.                      │ trước khi đụng vào thanh ghi.   │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Mất cờ báo lỗi │ Dùng `DMA2D->IFCR |= FLAG`. Thanh ghi IFCR│ Ghi trực tiếp `=` vào cờ cần    │
│    truyền TEIF    │ là Write-only, đọc ra toàn 0 làm sai lệch.│ xóa: `DMA2D->IFCR = CTCIF`.     │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Mất gói tin CAN│ Cài Priority của SysTick hoặc DMA2D cao   │ Luôn đặt CAN RX ở Priority 0    │
│    khi vẽ màn hình│ hơn CAN RX làm CAN FIFO bị tràn (Overrun).│ hoặc 1, SysTick ở Priority 15.  │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Màn hình chớp  │ DMA2D ghi vào Framebuffer khi LTDC đang   │ Đồng bộ qua ngắt LTDC Line      │
│    sọc (Tearing)  │ quét dở dang giữa khung hình.             │ hoặc cờ VSYNC Reload.           │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Bộ tăng tốc DMA2D (Chrom-ART) khác gì so với bộ điều khiển DMA thông thường (DMA1/DMA2)?
* **Trả lời chuẩn Bare-metal:** 
  * DMA thông thường là bộ vận chuyển dữ liệu tuyến tính (1 chiều, 1 chiều dài `NDTR`). Nó không hiểu khái niệm hình học 2 chiều và không có mạch số học bên trong.
  * DMA2D là bộ xử lý đồ họa chuyên biệt (2D Engine): Nó quản lý dữ liệu theo dạng **ma trận dòng x cột** (thanh ghi `NLR`), tự động cộng bước nhảy dòng (**Line Offset `OOR`**). Đặc biệt, DMA2D tích hợp 2 khối tính toán phần cứng: **PFC (Pixel Format Converter)** để đổi định dạng màu on-the-fly và **Alpha Blender** để hòa trộn độ trong suốt giữa 2 lớp ảnh mà không tốn chu kỳ lệnh nào của CPU.

### ❓ Câu 2: Tại sao phải cấu hình `NVIC_PriorityGroup_4` trong hệ thống nhúng thời gian thực?
* **Trả lời chuẩn Bare-metal:** 
  * Vi điều khiển ARM Cortex-M7 dùng trường `PRIGROUP` trong thanh ghi `SCB->AIRCR` để chia 4 bits priority thành 2 phần: Preemption Priority (Ưu tiên chen ngang) và Subpriority (Ưu tiên cùng mức).
  * Trong các hệ thống an toàn như Ô tô, ta chọn `NVIC_PriorityGroup_4` để dành **toàn bộ 4 bits cho Preemption Priority (16 mức)**. Điều này đảm bảo tính tiền định tuyệt đối (Strict Determinism): Bất kỳ khi nào ngắt an toàn mạng CAN Bus kích hoạt, nó ĐƯỢC PHÉP lập tức ngắt ngang các tác vụ render đồ họa hoặc SysTick mà không bao giờ bị xếp hàng chờ đợi.

### ❓ Câu 3: Làm thế nào để vẽ một hình chữ nhật $100 \times 100$ vào chính giữa màn hình $480 \times 272$ bằng DMA2D?
* **Trả lời chuẩn Bare-metal:**
  * Tọa độ góc trên bên trái: $X = (480 - 100) / 2 = 190$, $Y = (272 - 100) / 2 = 86$.
  * Địa chỉ bắt đầu `OMAR` $= \text{Base} + 2 \times (86 \times 480 + 190)$ (cho RGB565).
  * Độ lệch dòng `DMA2D->OOR` $= 480 - 100 = \mathbf{380}$ (đơn vị pixel).
  * Kích thước `DMA2D->NLR` $= (100 \ll 16) \mid 100$.
  * Chọn `MODE = 11`b (R2M), nạp mã màu vào `OCOLR`, bật `START` và chờ cờ `TCIF`.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Trong kiến trúc hệ thống hiển thị táp-lô ô tô trên STM32F746, em giải quyết bài toán nghẽn CPU khi render đồ họa bằng cách ứng dụng bộ tăng tốc phần cứng **DMA2D Chrom-ART**. Thay vì dùng CPU lặp mảng tốn hàng chục mili-giây, em tận dụng chế độ **Register-to-Memory (R2M)** để xóa màn hình và đổ màu giao diện với tải CPU xấp xỉ **0%**, đồng thời dùng chế độ **PFC** để nạp icon ARGB từ Flash ra SDRAM cực nhanh.  
> Để bảo vệ mạng truyền thông thời gian thực, em thiết kế kiến trúc ngắt phân tầng **`NVIC_PriorityGroup_4`**, đặt ngắt nhận **CAN1 RX0 ở Priority 1** và **UART DMA ở Priority 2**, trong khi đưa **DMA2D xuống Priority 3** và **SysTick xuống Priority 15**. Cấu trúc này đảm bảo ngắt đồ họa không bao giờ gây ra hiện tượng trễ nhịp hay tràn hàng đợi (Overrun) của gói tin CAN an toàn."*
