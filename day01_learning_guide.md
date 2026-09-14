# 🏆 [NGÀY 1] CẨM NANG TOÀN DIỆN BARE-METAL: SYSTEM CLOCK 216MHz OVER-DRIVE, RESET LOGGING & BỘ NHỚ
## Lộ trình 4 Bước: Nguyên Lý Phần Cứng ➔ Thực Chiến RM0385 ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ từ gốc rễ mạch điện tử bán dẫn, tra cứu Reference Manual (RM0385) và Datasheet (DS10610), tính toán thông số PLL, cấu hình chuỗi Over-drive mode 216MHz, quản lý độ trễ Flash Wait States, đọc cờ Reset Reason trong `RCC_CSR` và quản lý bản đồ RAM Stack/Heap chuẩn Bare-metal cho STM32F746 (ARM Cortex-M7).  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, thanh ghi, công thức toán học, bảng tra cứu và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 1                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: NGUYÊN LÝ │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE            │ BƯỚC 4: PHỎNG VẤN          │
│ • Cây xung nhịp   │ • Tra cứu RM0385  │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn bộ nhớ  │
│   Clock Tree PLL  │   & DS10610       │ • TODO 1-4 [Sys_Clock.c]   │ • Flash Latency Trap       │
│ • Flash Latency   │ • Bảng Bus Clocks │ • TODO 5-6 [Sys_Clock.h]   │ • Over-drive Failure       │
│ • Over-drive Mode │ • Bảng Flash WS   │ • TODO 7 [main.c]          │ • Kịch bản trả lời 60s     │
│ • Bus Prescalers  │ • Bảng cờ RCC_CSR │ • Mổ xẻ 5 Bug phần cứng    │   (Elevator Pitch)         │
│ • Reset Reason    │                   │                            │                            │
│ • Bản đồ RAM      │                   │                            │                            │
│ • Bắt tay phần    │                   │                            │                            │
│   cứng (Mermaid)  │                   │                            │                            │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC BARE-METAL CỐT LÕI (CHUYÊN CHO NGÀY 1)

| STT | Quy tắc Bare-metal | Thể hiện cụ thể trong Ngày 1 (Clock & Reset) |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `RCC_CR` (`0x0000 XX83` - HSI ON, PLL OFF), `RCC_CFGR` (`0x0000 0000`), `FLASH_ACR` (`0x0000 0000` - 0 Wait State) trước khi cấu hình. |
| **2** | **Access Type** | **CỰC KỲ QUAN TRỌNG:** Thanh ghi `RCC_CSR` chứa bit xóa cờ `RMVF` (Remove Reset Flag) là dạng `W1C` (Write 1 to Clear). Bắt buộc ghi trực tiếp `RCC->CSR |= RCC_CSR_RMVF` (hoặc ghi thẳng `RCC->CSR = RCC_CSR_RMVF`) để xóa cờ, không để cờ tồn tại sang chu kỳ boot sau. |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường đa bit: `VOS[1:0]` (Bits 15:14 trong `PWR_CR1`), `PLLM[5:0]`, `PLLN[8:0]`, `PLLP[1:0]`, `HPRE[3:0]`, `PPRE1[2:0]`, `PPRE2[2:0]`, `LATENCY[3:0]`. |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi (`RCC_TypeDef`, `PWR_TypeDef`, `FLASH_TypeDef`) bắt buộc dùng `volatile` để trình biên dịch không tối ưu hóa xóa bỏ các vòng lặp Polling cờ trạng thái phần cứng (`HSERDY`, `PLLRDY`, `ODRDY`, `ODSWRDY`, `SWS`). |
| **5** | **Hardware Handshake Pipeline** | Tuân thủ quy trình bắt tay tuần tự phần cứng: Cấp xung $\rightarrow$ Chờ cờ Ready $\rightarrow$ Bật tính năng $\rightarrow$ Chờ xác nhận chuyển đổi. Tuyệt đối không nhảy cóc bước. |
| **6** | **RM / DS Lookup** | Cung cấp chính xác Chapter, Section, từ khóa `Ctrl + F`, công thức `Base Address + Offset` cho `RCC`, `PWR`, `FLASH`. |
| **7** | **Hardware Rationale & Specs** | Bố trí cơ sở kỹ thuật, giới hạn phần cứng ($f_{VCO\_in} = 1\text{MHz}$, $100\text{MHz} \le f_{VCO\_out} \le 432\text{MHz}$, $f_{SYSCLK} \le 216\text{MHz}$, $f_{HCLK} \le 216\text{MHz}$, $f_{PCLK1} \le 54\text{MHz}$, $f_{PCLK2} \le 108\text{MHz}$) liền kề từng bảng thanh ghi. |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ PHẦN CỨNG & CƠ CHẾ VẬT LÝ (HARDWARE ARCHITECTURE)

## 1.1. Kiến trúc Cây Xung Nhịp (Clock Tree): HSE 25MHz & Bộ nhân Main PLL

Lõi ARM Cortex-M7 trên STM32F746NG có thể chạy ở tần số tối đa $216\text{ MHz}$. Trên bo mạch STM32F746G-Discovery, nguồn xung nhịp ổn định đầu vào là thạch anh ngoài **HSE = 25 MHz**. 

Để biến $25\text{ MHz}$ thành $216\text{ MHz}$, chip sử dụng khối mạch tích hợp **Main PLL (Phase-Locked Loop)**:

### So sánh HSI (Internal) vs HSE (External):
| Đặc tính | HSI (High-Speed Internal) | HSE (High-Speed External) |
| :--- | :--- | :--- |
| **Bản chất** | Mạch dao động RC tích hợp sẵn trong ruột silicon (16MHz). | Thạch anh kim loại vật lý hàn ngoài bo mạch (25MHz). |
| **Ưu điểm** | Bật nguồn là chạy ngay, không cần linh kiện ngoài. | Cực kỳ chính xác, sai số chỉ $\approx 20\text{ ppm}$ (0.002%). |
| **Nhược điểm**| Sai số lớn ($\pm 1\% \dots \pm 2\%$) do nhiệt độ môi trường. | Cần vài mili-giây để cơ học rung ổn định (`HSERDY`). |
| **Ứng dụng** | Dùng khi bootup an toàn hoặc ứng dụng tiết kiệm điện. | **Bắt buộc** cho USB, Ethernet, CAN Bus và hệ thống 216MHz. |

```text
 ┌──────────────┐
 │ Thạch anh ngoài │ (HSE = 25 MHz)
 │    OSC IN    │
 └──────┬───────┘
        │
        ▼  Chia PLLM = 25
 ┌──────────────┐
 │  f_VCO_in    │ = 25 MHz / 25 = 1 MHz (Theo RM0385: Bắt buộc nằm trong dải 1 đến 2 MHz, tối ưu 1 MHz)
 └──────┬───────┘
        │
        ▼  Nhân PLLN = 432
 ┌──────────────┐
 │  f_VCO_out   │ = 1 MHz * 432 = 432 MHz (Theo RM0385: Dải VCO cho phép từ 100 đến 432 MHz)
 └──────┬───────┘
        │
        ├────────────────────────────────┬───────────────────────────────┐
        ▼  Chia PLLP = 2                 ▼  Chia PLLQ = 9                ▼  Chia PLLR = 7
 ┌──────────────┐                 ┌──────────────┐                ┌──────────────┐
 │    SYSCLK    │ = 216 MHz       │ USB, SDMMC   │ = 48 MHz       │ DSI (nếu có) │
 │  (Cortex-M7) │                 │  (Chuẩn 48M) │                └──────────────┘
 └──────────────┘                 └──────────────┘
```

### Giới hạn phần cứng bắt buộc (RM0385 Section 5.1.4):
1. **$f_{VCO\_in} = \frac{f_{HSE}}{\text{PLLM}}$**: Phải nằm trong khoảng $[1.0\text{ MHz} \dots 2.0\text{ MHz}]$ để mạch lọc so pha PFD hoạt động chính xác với độ rung pha (Jitter) thấp nhất. Chọn $\text{PLLM} = 25 \implies f_{VCO\_in} = 1.0\text{ MHz}$.
2. **$f_{VCO\_out} = f_{VCO\_in} \times \text{PLLN}$**: Phải nằm trong khoảng $[100\text{ MHz} \dots 432\text{ MHz}]$. Chọn $\text{PLLN} = 432 \implies f_{VCO\_out} = 432.0\text{ MHz}$.
3. **$f_{SYSCLK} = \frac{f_{VCO\_out}}{\text{PLLP}}$**: Tối đa $216\text{ MHz}$. Chọn $\text{PLLP} = 2$ (mã nhị phân `00b`) $\implies f_{SYSCLK} = \frac{432}{2} = 216.0\text{ MHz}$.
4. **$f_{PLL48CLK} = \frac{f_{VCO\_out}}{\text{PLLQ}}$**: Bắt buộc đúng $48\text{ MHz}$ cho USB. Chọn $\text{PLLQ} = 9 \implies f_{USB} = \frac{432}{9} = 48.0\text{ MHz}$.

---

## 1.2. Mối quan hệ Phần cứng giữa Tần số Lõi và Flash Access Latency (Wait States)

Bộ nhớ Flash nhúng trên chip STM32F7 có giới hạn vật lý về thời gian truy cập (Access Time $t_{ACC} \approx 30\text{ ns}$):
* Ở tần số $216\text{ MHz}$, chu kỳ xung nhịp của CPU chỉ kéo dài:
  $$T_{CPU} = \frac{1}{216\text{ MHz}} \approx 4.63\text{ ns}$$
* Vì $4.63\text{ ns} \ll 30\text{ ns}$, nếu CPU truy cập đọc mã lệnh Flash ở tốc độ tối đa, tín hiệu dữ liệu chưa kịp ổn định trên bus $\implies$ CPU đọc trúng dữ liệu rác, gây ra lỗi **`HardFault`** hoặc **`BusFault`** ngay lập tức!
* **Giải pháp phần cứng:** Thanh ghi `FLASH_ACR` cho phép chèn thêm các chu kỳ chờ (**Wait States - WS**):
  $$\text{Số chu kỳ chờ} = \frac{30\text{ ns}}{4.63\text{ ns}} \approx 6.48 \implies \mathbf{7\text{ chu kỳ CPU (tương ứng 6 Wait States - LATENCY = 6)}} $$

```text
Tra cứu RM0385 Table 5: Number of wait states according to CPU clock (HCLK) frequency:
Điện áp hoạt động: 2.7V - 3.6V (V_DD trên Discovery board = 3.3V)
-------------------------------------------------------------------------------------
Wait States (WS)          | Tần số HCLK tối đa cho phép
-------------------------------------------------------------------------------------
0 WS (1 CPU cycle)        | 0 < HCLK <= 30 MHz
1 WS (2 CPU cycles)       | 30 < HCLK <= 60 MHz
2 WS (3 CPU cycles)       | 60 < HCLK <= 90 MHz
3 WS (4 CPU cycles)       | 90 < HCLK <= 120 MHz
4 WS (5 CPU cycles)       | 120 < HCLK <= 150 MHz
5 WS (6 CPU cycles)       | 150 < HCLK <= 180 MHz
6 WS (7 CPU cycles)       | 180 < HCLK <= 210 MHz
7 WS (8 CPU cycles)       | 210 < HCLK <= 216 MHz  <=== CHỌN 7 WS HOẶC 6 WS (VỚI ART ACCELERATOR)
-------------------------------------------------------------------------------------
```

> ⚠️ **QUY TẮC SỐNG CÒN:** **BẮT BUỘC cấu hình tăng Flash Latency LÊN TRƯỚC KHI chuyển đổi nguồn xung SYSCLK sang PLL 216MHz!** Nếu tăng xung trước khi cấu hình Flash Latency, CPU sẽ chết đứng ngay tại lệnh tiếp theo.

---

## 1.3. Cơ chế Năng lượng VOS Scale 1 & Bắt tay Over-Drive Mode

Để transistor bên trong vi mạch Cortex-M7 đóng ngắt tin cậy ở tần số $216\text{ MHz}$, nguồn cấp nội bộ cho khối logic số (V_CORE) phải được kích hoạt chế độ **Over-drive Mode**:

```text
                    QUY TRÌNH BẮT TAY OVER-DRIVE MODE (RM0385 Section 5.1.4)
                    
 ┌──────────────────────────────────────┐
 │ Cấp xung APB1 cho module PWR         │ RCC->APB1ENR |= RCC_APB1ENR_PWREN
 └──────────────────┬───────────────────┘
                    ▼
 ┌──────────────────────────────────────┐
 │ Đặt VOS = Scale 1 (Điện áp cao nhất) │ PWR->CR1: VOS[1:0] = 11b
 └──────────────────┬───────────────────┘
                    ▼
 ┌──────────────────────────────────────┐
 │ Bật bit ODEN (Over-Drive Enable)     │ PWR->CR1 |= PWR_CR1_ODEN
 └──────────────────┬───────────────────┘
                    ▼
 ┌──────────────────────────────────────┐
 │ Polling cờ ODRDY = 1                 │ while (!(PWR->CSR1 & PWR_CSR1_ODRDY));
 └──────────────────┬───────────────────┘ (Mạch điều áp nội bộ đã ổn định)
                    ▼
 ┌──────────────────────────────────────┐
 │ Bật bit ODSWEN (Over-Drive Switch)   │ PWR->CR1 |= PWR_CR1_ODSWEN
 └──────────────────┬───────────────────┘
                    ▼
 ┌──────────────────────────────────────┐
 │ Polling cờ ODSWRDY = 1               │ while (!(PWR->CSR1 & PWR_CSR1_ODSWRDY));
 └──────────────────────────────────────┘ (Hệ thống sẵn sàng chạy ở 216MHz!)
```

---

## 1.4. Phân tầng Tần số Bus Matrix (AHB, APB1, APB2 Prescalers)

Khi $f_{SYSCLK} = 216\text{ MHz}$, xung nhịp được phân phối tới các bus thông qua các bộ chia (Prescalers) trong thanh ghi `RCC_CFGR`. Phải tuân thủ giới hạn phần cứng tuyệt đối trong Datasheet DS10610:

```text
                                SYSCLK = 216 MHz
                                       │
                                       ▼  HPRE = /1 (HPRE[3:0] = 0000b)
                       ┌───────────────────────────────┐
                       │   AHB Bus (HCLK) = 216 MHz    │ (Max cho phép: 216 MHz)
                       │   (Cortex-M7, SRAM, DMA1/2)   │
                       └───────────────┬───────────────┘
                                       │
                   ┌───────────────────┴───────────────────┐
                   │ PPRE1 = /4                            │ PPRE2 = /2
                   ▼ (PPRE1[2:0] = 101b)                   ▼ (PPRE2[2:0] = 100b)
       ┌───────────────────────────────┐       ┌───────────────────────────────┐
       │   APB1 Bus (PCLK1) = 54 MHz   │       │  APB2 Bus (PCLK2) = 108 MHz   │
       │   (Max cho phép: 54 MHz)      │       │  (Max cho phép: 108 MHz)      │
       │   Ngoại vi: CAN1/2, USART2/3  │       │  Ngoại vi: USART1/6, SPI1     │
       └───────────────────────────────┘       └───────────────────────────────┘
```

> ⚠️ **HẬU QUẢ NẾU CHIA SAI:** Nếu cài đặt `PPRE1 = /2` (thay vì `/4`), bus APB1 sẽ bị đẩy lên $108\text{ MHz}$ (vượt trần $54\text{ MHz}$). Toàn bộ khối CAN Controller và UART trên bus APB1 sẽ bị lệch Baudrate và sai lệch định thời phần cứng!

---

## 1.5. Cơ chế Ghi nhận Lý do Khởi động lại (Reset Reason Logging qua `RCC_CSR`)

Thanh ghi `RCC_CSR` (Control/Status Register, Offset `0x74`) lưu giữ các cờ trạng thái nguyên nhân gây ra lần Reset gần nhất:

```text
 31       30       29       28       27       26       25       24
┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐
│LPWRRSTF│WWDGRSTF│IWDGRSTF│ SFTRSTF│ PORRSTF│ PINRSTF│ BORRSTF│  RMVF  │
└────────┴────────┴────────┴────────┴────────┴────────┴────────┴────────┘
```

* **`PORRSTF` (Bit 27):** Power-on/Power-down reset (Cấp nguồn lần đầu).
* **`PINRSTF` (Bit 26):** Reset từ chân phần cứng bên ngoài (Nút bấm B1 NRST).
* **`SFTRSTF` (Bit 28):** Software reset do CPU phát lệnh (`NVIC_SystemReset()`).
* **`IWDGRSTF` (Bit 29):** Independent Watchdog timeout (Hệ thống bị treo, Watchdog can thiệp).
* **`WWDGRSTF` (Bit 30):** Window Watchdog reset.
* **`BORRSTF` (Bit 25):** Brown-out reset (Điện áp nguồn VDD sụt dưới ngưỡng an toàn).
* **`RMVF` (Bit 24 - Remove reset flag):** **W1C Register.** Ghi bit `1` vào `RMVF` để xóa sạch toàn bộ các cờ reset, chuẩn bị cho lần phát hiện tiếp theo.

---

## 1.6. Tổ chức Bản đồ Bộ nhớ RAM, Stack & Heap

Bộ nhớ nội SRAM trên STM32F746 có tổng dung lượng $512\text{ KB}$ (từ `0x2000 0000` đến `0x2005 0000`), được phân bổ thành 4 phân vùng chuẩn:

```text
Địa chỉ CAO   ▲ 0x2005 0000 ──┬───────────────────────────────────────────┐
              │               │  STACK (LIFO - Full Descending)           │
              │               │  Con trỏ SP tụt dần xuống địa chỉ thấp    │
              │               │  Lưu: Biến cục bộ, Context ISR, Frame hàm │
              │               ├ - - - - - - - - - - - - - - - - - - - - - ┤
              │               │  [VÙNG ĐỆM TỰ DO / VÙNG NGUY HIỂM]        │
              │               │  Nơi xảy ra STACK OVERFLOW!               │
              │               ├ - - - - - - - - - - - - - - - - - - - - - ┤
              │               │  HEAP (Cấp phát động malloc / free)       │
              │               │  Con trỏ phát triển tăng dần lên cao      │
              │               ├───────────────────────────────────────────┤
              │               │  .bss (Biến toàn cục/static chưa init)    │
              │               │  Reset_Handler xóa về 0 khi bootup        │
              │               ├───────────────────────────────────────────┤
              │               │  .data (Biến toàn cục/static có init)     │
              │               │  Copy từ FLASH sang RAM khi bootup        │
Địa chỉ THẤP  ▼ 0x2000 0000 ──┴───────────────────────────────────────────┘
```

* **Nguyên tắc an toàn Bare-metal:** Tránh dùng `malloc/free` để triệt tiêu nguy cơ phân mảnh bộ nhớ (Heap Fragmentation) và mất tính tiền định thời gian thực (Non-deterministic Latency).
* **Phòng ngừa Stack Overflow:** Không khai báo mảng lớn cục bộ bên trong hàm (chuyển sang cấp phát toàn cục hoặc `static`).


---

## 1.7. Sơ Đồ Tuần Tự: Quy Trình Cấu Hình & Vận Hành Hệ Thống Xung Nhịp (Configuration & Execution Pipeline)

Mô hình hóa toàn bộ chuỗi các bước cấu hình tuần tự các thanh ghi (`RCC`, `PWR`, `FLASH`) để đưa hệ thống từ trạng thái Reset ban đầu lên xung nhịp đỉnh cao $216\text{ MHz}$ và chu trình xử lý cờ Reset lúc boot:

---

### 📋 Sơ Đồ 1: Quy Trình Cấu Hình Tuần Tự Đưa Hệ Thống Lên 216MHz Over-Drive (Clock System Configuration Pipeline)

```mermaid
sequenceDiagram
    autonumber
    actor App as Application Code (Sys_Clock.c)
    participant RCC_CR as RCC->CR (Clock Control)
    participant PLL as Main PLL Silicon Circuit
    participant FLASH as FLASH->ACR (Access Control)
    participant PWR as PWR->CR1 & CSR1 (Power Controller)
    participant RCC_CFGR as RCC->CFGR (Clock Configuration)
    participant Core as Cortex-M7 Core (SYSCLK)

    Note over App,Core: GIAI ĐOẠN 1: BẮT TAY KÍCH HOẠT THẠCH ANH NGOÀI HSE (25MHz)
    App->>RCC_CR: 1. Ghi HSEON = 1 (Bật thạch anh ngoài)
    RCC_CR->>RCC_CR: 2. Mạch dao động cơ học rung ổn định
    RCC_CR-->>App: 3. Phần cứng bật cờ HSERDY = 1 (Hardware Ready Handshake)
    App->>RCC_CR: 4. Polling lặp: while (!(RCC->CR & RCC_CR_HSERDY));

    Note over App,Core: GIAI ĐOẠN 2: BẢO VỆ ĐỘ TRỄ FLASH & NẠP ĐIỆN ÁP LÕI SCALE 1
    App->>PWR: 5. Cấp xung PWREN, nạp VOS[1:0] = 11b (Scale 1 mode cho f > 180MHz)
    App->>FLASH: 6. Nâng trước Flash Latency = 7 Wait States (LATENCY = 7) & Bật ART Accelerator
    FLASH-->>Core: 7. Mạch trễ truy cập Flash sẵn sàng đón xung nhịp siêu cao (216MHz)

    Note over App,Core: GIAI ĐOẠN 3: BẮT TAY KHÓA PHA BỘ NHÂN MAIN PLL (432MHz VCO)
    App->>RCC_CR: 8. Cấu hình PLLM=25, PLLN=432, PLLP=2, PLLSRC=HSE; Ghi PLLON = 1
    PLL->>PLL: 9. Mạch lọc so pha PFD & VCO điều chỉnh tần số đồng pha
    PLL-->>RCC_CR: 10. Phần cứng bật cờ PLLRDY = 1 (PLL Lock Handshake)
    App->>RCC_CR: 11. Polling lặp: while (!(RCC->CR & RCC_CR_PLLRDY));

    Note over App,Core: GIAI ĐOẠN 4: BẮT TAY 2 BƯỚC OVER-DRIVE MODE (V_CORE BOOST CHO 216MHz)
    App->>PWR: 12. Bước A: Ghi ODEN = 1 (Over-Drive Enable)
    PWR->>PWR: 13. Mạch điều áp nội bộ tăng cường dòng cấp cho logic số
    PWR-->>App: 14. Phần cứng bật cờ ODRDY = 1 trong PWR_CSR1
    App->>PWR: 15. Polling lặp: while (!(PWR->CSR1 & PWR_CSR1_ODRDY));
    App->>PWR: 16. Bước B: Ghi ODSWEN = 1 (Over-Drive Switch Enable)
    PWR->>PWR: 17. Chuyển mạch nguồn cấp logic sang kênh Over-drive
    PWR-->>App: 18. Phần cứng bật cờ ODSWRDY = 1 trong PWR_CSR1
    App->>PWR: 19. Polling lặp: while (!(PWR->CSR1 & PWR_CSR1_ODSWRDY));

    Note over App,Core: GIAI ĐOẠN 5: BẮT TAY CHUYỂN NGUỒN XUNG CHÍNH (CLOCK SWITCH)
    App->>RCC_CFGR: 20. Cài đặt bộ chia Bus: HPRE=/1 (AHB=216M), PPRE1=/4 (APB1=54M), PPRE2=/2 (APB2=108M)
    App->>RCC_CFGR: 21. Yêu cầu chuyển nguồn: Ghi SW[1:0] = 10b (Chọn Main PLL làm SYSCLK)
    RCC_CFGR->>Core: 22. Mạch ghép xung (Glitch-Free Multiplexer) chuyển sang PLL
    RCC_CFGR-->>App: 23. Phần cứng phản hồi trạng thái: SWS[1:0] = 10b (Switch Status Handshake)
    App->>RCC_CFGR: 24. Polling lặp: while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    Note over App,Core: ➔ BẮT TAY TOÀN DIỆN THÀNH CÔNG: Hệ thống vận hành ở 216MHz đỉnh cao!
```

---

### 📋 Sơ Đồ 2: Quy Trình Đọc & Xử Lý Cờ Nguyên Nhân Reset Lúc Khởi Động (Reset Reason Detection & Clean Pipeline)

```mermaid
sequenceDiagram
    autonumber
    actor ResetSource as Nguồn Kích Hoạt Reset (Nút bấm NRST / IWDG / Power-On)
    participant Silicon as Mạch Reset Controller (Hardware)
    participant RCC_CSR as RCC->CSR (Reset Status)
    actor App as Application Bootloader (main.c)
    participant UART as Virtual COM Port (ST-LINK)

    ResetSource->>Silicon: 1. Sự kiện Reset phần cứng xảy ra (Ví dụ: Chó canh IWDG sủa hoặc cắm nguồn POR)
    Silicon->>Silicon: 2. Kéo chân Reset nội bộ, giữ chip trong trạng thái Reset an toàn
    Silicon->>RCC_CSR: 3. Chốt cờ nguyên nhân phần cứng: Bật IWDGRSTF=1 (hoặc PORRSTF, PINRSTF, SFTRSTF)
    Silicon->>App: 4. Thả lỏng Reset ➔ Lõi CPU bắt đầu nạp Vector Table và nhảy vào main()

    Note over App,UART: GIAI ĐOẠN ĐỌC & GIẢI MÃ LÝ DO RESET
    App->>RCC_CSR: 5. Đọc giá trị thanh ghi: uint32_t reset_flags = RCC->CSR;
    App->>App: 6. Kiểm tra bitmask phân loại (IWDGRSTF? PINRSTF? PORRSTF?)
    App->>UART: 7. Xuất log chẩn đoán ra Terminal: "[SYSTEM] Boot reason: IWDG Watchdog Reset!"

    Note over App,RCC_CSR: GIAI ĐOẠN GIẢI PHÓNG CỜ (W1C HANDSHAKE)
    App->>RCC_CSR: 8. Ghi bit xóa cờ: RCC->CSR = RCC_CSR_RMVF; (Write 1 to Clear - CẤM DÙNG |=)
    RCC_CSR->>RCC_CSR: 9. Phần cứng xóa sạch toàn bộ các cờ *RSTF về 0
    Note over App,RCC_CSR: ➔ HOÀN TẤT: Thanh ghi sạch 100%, sẵn sàng phát hiện chính xác lần reset kế tiếp!
```

---

### 📋 Sơ Đồ 3: Quy Trình Khởi Động Chip Cortex-M7 & Khởi Tạo Phân Vùng Bộ Nhớ (Bootup & Memory Layout Pipeline)

Mô hình hóa chuỗi hoạt động phần cứng từ khi cấp nguồn vật lý, nạp vector bảng ngắt, khởi tạo dữ liệu RAM đến khi bước vào hàm `main()`:

```mermaid
sequenceDiagram
    autonumber
    participant Power as Nguồn Cấp (VDD 3.3V)
    participant Core as ARM Cortex-M7 Core
    participant Flash as Flash Memory (0x0800 0000)
    participant SRAM as Main SRAM (0x2000 0000)
    actor Startup as Reset_Handler (startup_stm32f746xx.s)
    actor Main as Application main()

    Power->>Core: 1. Điện áp VDD vượt ngưỡng POR ➔ Nhả tín hiệu Internal Reset
    Core->>Flash: 2. Đọc 4 bytes đầu tiên tại địa chỉ 0x08000000 ➔ Nạp Initial MSP (Đỉnh RAM: 0x20050000)
    Core->>Flash: 3. Đọc 4 bytes tiếp theo tại địa chỉ 0x08000004 ➔ Nạp địa chỉ hàm Reset_Handler
    Core->>Startup: 4. Thiết lập PC = Reset_Handler, bắt đầu thực thi mã lệnh khởi động

    Note over Startup,SRAM: GIAI ĐOẠN KHỞI TẠO BỘ NHỚ RAM (.DATA & .BSS)
    Startup->>Flash: 5. Nạp con trỏ dữ liệu khởi tạo: _sidata (Nằm trên Flash)
    Startup->>SRAM: 6. Sao chép phân vùng .data (Biến toàn cục có khởi tạo) từ Flash vào RAM (_sdata ➔ _edata)
    Startup->>SRAM: 7. Xóa sạch phân vùng .bss (Biến toàn cục chưa khởi tạo) về 0 (_sbss ➔ _ebss)

    Note over Startup,Main: GIAI ĐOẠN CẤU HÌNH XUNG NHỊP & BƯỚC VÀO MAIN
    Startup->>Startup: 8. Gọi hàm SystemInit() & Sys_Clock_Init() (Thiết lập 216MHz Over-Drive theo Sơ đồ 1)
    Startup->>Main: 9. Nhảy vào thực thi hàm main()
    Main->>SRAM: 10. Stack tụt dần từ đỉnh 0x20050000 xuống dưới khi gọi hàm; Heap phát triển từ đáy lên cao!
```

---

# 📑 BƯỚC 2: THỰC CHIẾN & TRA CỨU RM0385 / DATASHEET (SETUP & LOOKUP)

## 2.1. Bản đồ Địa chỉ Base Address & Ngoại vi Ngày 1

Tra cứu Reference Manual RM0385 *Chapter 2: Memory map $\rightarrow$ Table 1*:

| Tên ngoại vi | Bus | Base Address | Offset | Địa chỉ tuyệt đối | Chức năng chính |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`RCC`** | AHB1 | `0x4002 3800` | `0x0000` | `0x4002 3800` | Reset & Clock Control (Bật PLL, Prescalers, Reset flags). |
| **`PWR`** | APB1 | `0x4000 7000` | `0x0000` | `0x4000 7000` | Power Controller (Cấu hình VOS Scale 1, Over-drive mode). |
| **`FLASH`** | AHB1 | `0x4002 3C00` | `0x0000` | `0x4002 3C00` | Flash Access Control (Cài đặt Latency Wait States, ART, Prefetch). |

---

## 2.2. Bảng Tra cứu Thanh ghi & Bitmask Chi tiết

Tra cứu RM0385 *Chapter 5 (RCC)*, *Chapter 4 (PWR)*, và *Chapter 3 (Flash)*:

| Module | Thanh ghi | Bit / Trường | Giá trị gán | Ý nghĩa kỹ thuật phần cứng |
| :--- | :--- | :--- | :---: | :--- |
| **`RCC`** | `RCC_APB1ENR` | `PWREN` (Bit 28) | `1`b | Cấp xung APB1 cho module điều khiển nguồn PWR. |
| **`PWR`** | `PWR_CR1` | `VOS[1:0]` (Bit 15:14) | `11`b | Cài đặt điện áp lõi Scale 1 mode (cần thiết cho $f > 180\text{MHz}$). |
| **`RCC`** | `RCC_CR` | `HSEON` (Bit 16) | `1`b | Bật dao động thạch anh ngoài HSE 25MHz. |
| | | `HSERDY` (Bit 17) | RO Polling | Chờ thạch anh dao động cơ học ổn định (`HSERDY = 1`). |
| **`FLASH`**| `FLASH_ACR` | `LATENCY[3:0]` (Bit 3:0) | `0111`b (7WS) | Cài đặt 7 Wait States (hoặc 6WS + ART) cho HCLK = 216MHz. |
| | | `ARTEN` (Bit 9) | `1`b | Bật bộ tăng tốc dòng lệnh ART Accelerator. |
| | | `PRFTEN` (Bit 8) | `1`b | Bật bộ đệm nạp lệnh đón đầu Prefetch. |
| **`RCC`** | `RCC_CFGR` | `HPRE[3:0]` (Bit 7:4) | `0000`b (`/1`) | Bộ chia AHB Prescaler: HCLK = SYSCLK / 1 = 216MHz. |
| | | `PPRE1[2:0]` (Bit 12:10)| `101`b (`/4`) | Bộ chia APB1 Prescaler: PCLK1 = 216 / 4 = 54MHz. |
| | | `PPRE2[2:0]` (Bit 15:13)| `100`b (`/2`) | Bộ chia APB2 Prescaler: PCLK2 = 216 / 2 = 108MHz. |
| **`RCC`** | `RCC_PLLCFGR`| `PLLM[5:0]` (Bit 5:0) | `25`d (`0x19`) | Chia tần số đầu vào HSE: $25\text{MHz} / 25 = 1\text{MHz}$. |
| | | `PLLN[8:0]` (Bit 14:6) | `432`d (`0x1B0`)| Nhân tần số VCO: $1\text{MHz} \times 432 = 432\text{MHz}$. |
| | | `PLLP[1:0]` (Bit 17:16)| `00`b (`/2`) | Chia tần số hệ thống: $432\text{MHz} / 2 = 216\text{MHz}$. |
| | | `PLLSRC` (Bit 22) | `1`b | Chọn nguồn cấp cho PLL là HSE. |
| | | `PLLQ[3:0]` (Bit 27:24)| `9`d (`1001`b) | Chia tần số USB/SDMMC: $432\text{MHz} / 9 = 48\text{MHz}$. |
| **`RCC`** | `RCC_CR` | `PLLON` (Bit 24) | `1`b | Kích hoạt bộ nhân Main PLL. |
| | | `PLLRDY` (Bit 25) | RO Polling | Chờ vòng khóa pha PLL khóa thành công (`PLLRDY = 1`). |
| **`PWR`** | `PWR_CR1` | `ODEN` (Bit 16) | `1`b | Kích hoạt chế độ Over-drive mode. |
| | `PWR_CSR1` | `ODRDY` (Bit 16) | RO Polling | Chờ nguồn điện áp Over-drive sẵn sàng (`ODRDY = 1`). |
| | `PWR_CR1` | `ODSWEN` (Bit 17) | `1`b | Chuyển mạch cấp áp Over-drive sang lõi. |
| | `PWR_CSR1` | `ODSWRDY` (Bit 17)| RO Polling | Chờ mạch chuyển áp Over-drive hoàn tất (`ODSWRDY = 1`). |
| **`RCC`** | `RCC_CFGR` | `SW[1:0]` (Bit 1:0) | `10`b | Chuyển nguồn xung SYSCLK sang ngõ ra của Main PLL. |
| | | `SWS[1:0]` (Bit 3:2) | RO Polling | Chờ phần cứng xác nhận SYSCLK đang dùng PLL (`SWS = 10b`). |

---

# 💻 BƯỚC 3: GÕ CODE & MỔ XẺ BUG PHẦN CỨNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 1

```text
drivers/
├── inc/
│   ├── Reg.h          <-- Khai báo Base Address, Struct Mapping & Bit Definitions
│   └── Sys_Clock.h    <-- Khai báo API công khai: System_Clock_Init(), GetResetReason()
└── src/
    └── Sys_Clock.c    <-- Triển khai 7 bước Bare-metal cấu hình Clock & Reset Logging
src/
└── main.c             <-- Gọi System_Clock_Init() và in Reset Reason
```

---

### 📂 KHỐI 1: FILE HEADER GIAO DIỆN [ `drivers/inc/Sys_Clock.h` ]

#### TODO 1 [File: `drivers/inc/Sys_Clock.h`]: Khai báo Enum Reset Reason & Hàm Prototypes
```c
#ifndef SYS_CLOCK_H
#define SYS_CLOCK_H

#include <stdint.h>

/**
 * @brief Định danh nguyên nhân gây ra lần Reset gần nhất
 */
typedef enum {
    RESET_REASON_UNKNOWN = 0,
    RESET_REASON_POR,       /* Power-on / Power-down Reset */
    RESET_REASON_PIN,       /* Chân NRST bên ngoài (Nút bấm B1) */
    RESET_REASON_SOFTWARE,  /* Lệnh phần mềm NVIC_SystemReset() */
    RESET_REASON_IWDG,      /* Independent Watchdog Timeout */
    RESET_REASON_WWDG,      /* Window Watchdog Timeout */
    RESET_REASON_BOR        /* Brown-out Reset (Sụt áp nguồn) */
} SystemResetReason_t;

/* Khởi tạo xung nhịp hệ thống đạt 216MHz với Over-drive mode */
void System_Clock_Init(void);

/* Đọc nguyên nhân reset và xóa cờ RMVF */
SystemResetReason_t System_GetResetReason(void);
const char* System_GetResetReasonString(SystemResetReason_t reason);

#endif /* SYS_CLOCK_H */
```

---

### 📂 KHỐI 2: FILE SOURCE DRIVER [ `drivers/src/Sys_Clock.c` ]

#### TODO 2 [File: `drivers/src/Sys_Clock.c`]: Chuỗi khởi tạo 7 Bước Clock 216MHz Over-Drive
```c
#include "Sys_Clock.h"
#include "Reg.h"

void System_Clock_Init(void)
{
    /* BƯỚC 1: Bật Clock cho Power Controller (PWR) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    /* BƯỚC 2: Cài đặt điện áp lõi VOS = Scale 1 (11b) */
    PWR->CR1 &= ~(3U << 14);
    PWR->CR1 |=  (3U << 14);

    /* BƯỚC 3: Bật thạch anh ngoài HSE 25MHz & Chờ ổn định */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* BƯỚC 4: TĂNG FLASH WAIT STATES LÊN 7 WS (HOẶC 6 WS + ART) */
    FLASH->ACR &= ~(0xFU << 0);
    FLASH->ACR |=  (0x7U << 0);                     /* 7 Wait States */
    FLASH->ACR |=  (FLASH_ACR_PRFTEN | FLASH_ACR_ARTEN); /* Bật Prefetch & ART */

    /* BƯỚC 5: Thiết lập bộ chia Bus Prescalers & Tham số PLL */
    /* HCLK = SYSCLK / 1 (216MHz), PCLK1 = HCLK / 4 (54MHz), PCLK2 = HCLK / 2 (108MHz) */
    RCC->CFGR &= ~((0xFU << 4) | (0x7U << 10) | (0x7U << 13));
    RCC->CFGR |=  ((0x0U << 4) | (0x5U << 10) | (0x4U << 13));

    /* Cấu hình PLL: M=25, N=432, P=2 (00b), Q=9, Nguồn = HSE */
    RCC->PLLCFGR = (25U  << 0)  |   /* PLLM = 25 */
                   (432U << 6)  |   /* PLLN = 432 */
                   (0U   << 16) |   /* PLLP = 2 (/2) */
                   (1U   << 22) |   /* PLLSRC = HSE */
                   (9U   << 24);    /* PLLQ = 9 */

    /* Bật PLL và chờ khóa pha */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* BƯỚC 6: Bật Over-drive Mode (Handshake 4 bước) */
    PWR->CR1 |= (1U << 16);                 /* Bật bit ODEN */
    while (!(PWR->CSR1 & (1U << 16)));      /* Chờ cờ ODRDY = 1 */

    PWR->CR1 |= (1U << 17);                 /* Bật bit ODSWEN */
    while (!(PWR->CSR1 & (1U << 17)));      /* Chờ cờ ODSWRDY = 1 */

    /* BƯỚC 7: Chuyển SYSCLK sang nguồn Main PLL */
    RCC->CFGR &= ~(0x3U << 0);
    RCC->CFGR |=  (0x2U << 0);              /* SW = 10b (PLL) */
    while ((RCC->CFGR & (0x3U << 2)) != (0x2U << 2)); /* Chờ SWS = 10b */
}
```

#### TODO 3 [File: `drivers/src/Sys_Clock.c`]: Đọc Lý do Reset & Xóa Cờ W1C
```c
SystemResetReason_t System_GetResetReason(void)
{
    SystemResetReason_t reason = RESET_REASON_UNKNOWN;
    uint32_t csr = RCC->CSR;

    if (csr & (1U << 27)) {
        reason = RESET_REASON_POR;
    } else if (csr & (1U << 26)) {
        reason = RESET_REASON_PIN;
    } else if (csr & (1U << 28)) {
        reason = RESET_REASON_SOFTWARE;
    } else if (csr & (1U << 29)) {
        reason = RESET_REASON_IWDG;
    } else if (csr & (1U << 30)) {
        reason = RESET_REASON_WWDG;
    } else if (csr & (1U << 25)) {
        reason = RESET_REASON_BOR;
    }

    /* Xóa cờ reset sau khi đọc xong (W1C register qua bit RMVF) */
    RCC->CSR |= (1U << 24);

    return reason;
}

const char* System_GetResetReasonString(SystemResetReason_t reason)
{
    switch (reason) {
        case RESET_REASON_POR:      return "Power-on Reset (POR)";
        case RESET_REASON_PIN:      return "External Pin Reset (NRST button)";
        case RESET_REASON_SOFTWARE: return "Software Reset (NVIC_SystemReset)";
        case RESET_REASON_IWDG:     return "Independent Watchdog Reset (IWDG)";
        case RESET_REASON_WWDG:     return "Window Watchdog Reset (WWDG)";
        case RESET_REASON_BOR:      return "Brown-out Reset (BOR)";
        default:                    return "Unknown Reset Reason";
    }
}
```

---

### 📂 KHỐI 3: FILE MAIN CHÍNH [ `src/main.c` ]

#### TODO 4 [File: `src/main.c`]: Khởi chạy System Clock & Log Reset Reason
```c
#include "Sys_Clock.h"

int main(void)
{
    /* 1. Đọc nguyên nhân reset từ chu kỳ chạy trước */
    SystemResetReason_t reset_reason = System_GetResetReason();

    /* 2. Khởi tạo xung nhịp hệ thống đạt 216MHz Over-drive */
    System_Clock_Init();

    /* 3. Vòng lặp chính */
    while (1) {
        /* Ứng dụng thực thi tại tần số 216MHz ổn định */
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug Phần Cứng "Kinh Điển" trong Ngày 1

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 LỖI KINH ĐIỂN VÀ CÁCH KHẮC PHỤC                                │
├───────────────────────────┬──────────────────────────────────────┬───────────────────────────────┤
│ 1. Tăng Clock trước       │ CPU chạy 216MHz nhưng Flash vẫn ở 0  │ BẮT BUỘC tăng Flash Latency   │
│    Flash Latency          │ Wait State làm CPU đọc sai mã lệnh,  │ lên 7 WS TRƯỚC KHI chuyển đổi │
│                           │ kích hoạt HardFault ngay tức khắc.   │ nguồn xung sang PLL.          │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 2. Quên Over-drive Mode   │ PLL phát 216MHz nhưng bộ điều áp VOS │ Bắt buộc thực hiện đầy đủ 4   │
│                           │ ở mức điện áp thấp làm sập nguồn     │ bước bắt tay ODEN -> ODRDY -> │
│                           │ lõi và CPU tự reset ngẫu nhiên.      │ ODSWEN -> ODSWRDY.            │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 3. Sai Bus Prescaler APB1 │ Cài đặt PPRE1 = /2 làm APB1 chạy ở   │ Tra cứu DS10610 Table 15:     │
│                           │ 108MHz (vượt giới hạn max 54MHz) làm │ Cài đặt PPRE1 = /4 (101b) để  │
│                           │ tê liệt các ngoại vi CAN1/2, USART2/3│ APB1 đạt chuẩn 54MHz.         │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 4. Vòng lặp Polling vô hạn│ Thạch anh ngoài HSE bị hỏng/đứt chân │ Luôn thêm Timeout counter     │
│    treo cứng MCU          │ làm cờ HSERDY không bao giờ bật 1,   │ trong vòng lặp while(HSERDY); │
│                           │ CPU bị kẹt vĩnh viễn trong while.    │ để fallback về HSI nếu lỗi.   │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 5. Quên xóa cờ RMVF       │ Cờ PORRSTF trong RCC_CSR không tự    │ Ghi bit 1 vào RCC_CSR bit RMVF│
│    trong RCC_CSR          │ biến mất, các lần reset sau bị đọc   │ sau khi đọc để chuẩn bị cờ    │
│                           │ chồng chéo nhiều cờ cùng lúc.        │ sạch cho lần boot kế tiếp.    │
└───────────────────────────┴──────────────────────────────────────┴───────────────────────────────┘
```

---

# 🎙️ BƯỚC 4: PHỎNG VẤN & THIẾT KẾ NÂNG CAO (INTERVIEW PREP)

## 4.1. Bộ 5 Câu Hỏi Phỏng Vấn Chuyên Sâu (Top 5 Deep-Dive Questions)

### ❓ Câu 1: Tại sao STM32F746 chạy ở 216MHz bắt buộc phải cấu hình Flash Wait States? Nếu không cấu hình, lỗi phần cứng nào sẽ phát sinh?
* **Trả lời:** Flash nhúng có thời gian truy cập vật lý khoảng $30\text{ ns}$. Ở $216\text{ MHz}$, 1 chu kỳ CPU chỉ kéo dài $4.63\text{ ns}$. Nếu không chèn Wait States, CPU nạp lệnh trước khi Flash kịp đưa dữ liệu ra bus, dẫn đến dữ liệu lệnh bị sai lệch $\rightarrow$ Mạch giải mã lệnh kích hoạt ngoại lệ phần cứng **`HardFault`** hoặc **`BusFault`** (Precise Data Bus Error).

---

### ❓ Câu 2: Trình bày quy trình bắt tay 4 bước để kích hoạt Over-drive Mode trên STM32F7?
* **Trả lời:**
  1. Cấp xung cho module PWR thông qua `RCC_APB1ENR_PWREN`.
  2. Bật bit `ODEN` trong `PWR_CR1` để kích hoạt bơm áp Over-drive.
  3. Polling cờ `ODRDY` trong `PWR_CSR1` chờ mạch ổn áp nội bộ báo sẵn sàng.
  4. Bật bit `ODSWEN` trong `PWR_CR1` để chuyển nguồn áp sang lõi, sau đó chờ cờ `ODSWRDY` trong `PWR_CSR1` báo hoàn tất.

---

### ❓ Câu 3: Làm thế nào để bảo vệ hệ thống không bị treo đứng nếu thạch anh ngoài HSE bị đứt chân hoặc dao động không lên?
* **Trả lời:** 
  1. Trong vòng lặp Polling `while (!(RCC->CR & RCC_CR_HSERDY))`, lập trình viên phải chèn một biến đếm Timeout (ví dụ `timeout = 10000; while(!HSERDY && --timeout);`). Nếu hết thời gian mà chưa có cờ, hệ thống chuyển hướng tiếp tục chạy bằng dao động nội HSI 16MHz và phát cảnh báo lỗi.
  2. Bật tính năng phần cứng **Clock Security System (CSS)** qua bit `CSSON` trong `RCC_CR`. Khi HSE mất xung giữa chừng, phần cứng tự động chuyển SYSCLK về HSI và phát ngắt NMI khẩn cấp để lưu trữ dữ liệu an toàn.

---

### ❓ Câu 4: Sự khác biệt bản chất giữa thanh ghi `RCC_CSR` và các thanh ghi điều khiển thông thường khi xóa cờ?
* **Trả lời:** Các cờ reset trong `RCC_CSR` (`PORRSTF`, `PINRSTF`, `SFTRSTF`...) là dạng cờ trạng thái tích lũy phần cứng. Chúng không tự động biến mất khi đọc, cũng không xóa bằng cách ghi đè trực tiếp lên từng bit đó. Để xóa toàn bộ các cờ này, lập trình viên phải ghi bit `1` vào bit điều khiển riêng biệt **`RMVF` (Remove Reset Flag - Bit 24)**.

---

### ❓ Câu 5: Tại sao trong hệ thống nhúng quan trọng (như Automotive / Medical), việc dùng `malloc/free` trên vùng nhớ Heap bị nghiêm cấm?
* **Trả lời:**
  1. **Heap Fragmentation (Phân mảnh bộ nhớ):** Sau nhiều chu kỳ cấp phát và giải phóng các block có kích thước khác nhau, RAM bị xé vụn. Đến khi cần một block liên tục đủ lớn, `malloc` thất bại và trả về con trỏ `NULL`, dẫn đến sập hệ thống.
  2. **Non-deterministic Latency (Bất định về thời gian):** Thuật toán duyệt danh sách ô nhớ trống của `malloc` tốn thời gian thay đổi tùy theo mức độ phân mảnh, vi phạm nghiêm trọng giới hạn thời gian thực (Hard Real-Time Deadlines).
  3. **Memory Leak (Rò rỉ bộ nhớ):** Quên giải phóng trong một nhánh rẽ lỗi nhỏ sẽ tích tụ làm cạn kiệt RAM sau thời gian dài chạy 24/7. Giải pháp bắt buộc là cấp phát tĩnh (Static Memory Allocation) $100\%$.

---

## 4.2. Kịch bản Trả lời Phỏng vấn 60 Giây (Elevator Pitch)

> *"Trong thiết kế Bare-metal trên STM32F746, em trực tiếp cấu hình hệ thống xung nhịp đạt mức cực đại **216MHz** từ thạch anh ngoài **HSE 25MHz**. Em thiết lập bộ chia Main PLL với $PLLM=25, PLLN=432, PLLP=2$ để đạt $SYSCLK=216\text{MHz}$, đồng thời chia bus $PCLK1=54\text{MHz}$ và $PCLK2=108\text{MHz}$ tuân thủ đúng giới hạn phần cứng trong Datasheet DS10610. Trước khi kích hoạt PLL, em bảo vệ CPU khỏi lỗi HardFault bằng cách nâng **Flash Latency lên 7 Wait States** và bật bộ tăng tốc ART Accelerator. Em triển khai quy trình bắt tay 4 bước để kích hoạt chế độ **Over-drive Mode** trong `PWR_CR1`, và tích hợp cơ chế **Reset Reason Logging** thông qua thanh ghi `RCC_CSR` để ghi nhận chính xác nguyên nhân lỗi hệ thống (như sụt áp BOR hoặc Watchdog timeout) trước khi xóa cờ bằng bit `RMVF`."*
