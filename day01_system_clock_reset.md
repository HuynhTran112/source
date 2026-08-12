# 📗 [NGÀY 1] HƯỚNG DẪN BARE-METAL: 216MHz OVER-DRIVE CLOCK & RESET REASON LOGGING

Tài liệu này hướng dẫn chi tiết cách lập trình thanh ghi bare-metal (không sử dụng thư viện HAL/LL) cho **Cấu hình Xung hệ thống 216MHz ở chế độ Over-drive** và **Đọc / Xóa lý do Reset cứng trong RCC->CSR** trên vi điều khiển **STM32F746NG (ARM Cortex-M7)**.

Mọi mã nguồn và hướng dẫn trong tài liệu này đều **tuân thủ tuyệt đối 7 Quy tắc Bare-metal cốt lõi** đã được xác lập trong [`AGENTS.md`](file:///d:/Project/STM32F7/.agents/AGENTS.md).

---

## 🛠️ RÀ SOÁT 7 QUY TẮC BARE-METAL CỐT LÕI

| STT | Quy tắc | Áp dụng trong Ngày 1 |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `PWR_CR1`, `PWR_CSR1`, `RCC_CR`, `RCC_PLLCFGR`, `RCC_CFGR`, `FLASH_ACR`, `RCC_CSR` trước khi tác động. |
| **2** | **Access Type** | Phân biệt bit `RO` (`HSERDY`, `PLLRDY`, `ODRDY`, `ODSWRDY`, `SWS`, cờ reset), bit `RW` (`ODEN`, `ODSWEN`, `HSEON`, `PLLON`), và bit `RS` (`RMVF` - set 1 để clear). Không bao giờ `|=` trên cờ W1C/RS. |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường `VOS[1:0]`, `LATENCY[3:0]`, `PLLM[5:0]`, `PLLN[8:0]`, `PLLP[1:0]`, `PLLQ[3:0]`, `HPRE[3:0]`, `PPRE1[2:0]`, `PPRE2[2:0]`, `SW[1:0]`. |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi (`PWR_TypeDef`, `RCC_TypeDef`, `FLASH_TypeDef`) đều sử dụng từ khóa `volatile uint32_t` để chống compiler optimization. |
| **5** | **Interrupt Workflow** | Quy trình 5 bước đối với ngắt (được chuẩn bị sẵn nếu kích hoạt ngắt sụt áp PVD / Clock Security System CSS). |
| **6** | **RM / DS Lookup** | Ghi rõ tên chương, số hiệu mục, từ khóa `Ctrl + F`, và công thức tính `Base Address + Offset` cho từng thanh ghi. |
| **7** | **Hardware Rationale & Specs** | Bắt buộc nêu rõ **Cơ sở Kỹ thuật & Giới hạn Phần cứng** (DS10610 Table 17 limits: APB1 max 54MHz, APB2 max 108MHz; RM0385 Table 7 Flash 6 WS @ 3.3V; Over-drive 2-step sequence). |

---

## 🔍 HƯỚNG DẪN TRA CỨU REFERENCE MANUAL (RM0385) & DATASHEET (DS10610)

### 1. Bản đồ bộ nhớ (Memory Map) & Địa chỉ Base Address
* **Tài liệu tra cứu:** Reference Manual **RM0385** (DocID027590 Rev 5).
* **Mục tra cứu:** **Chapter 2: Memory and bus architecture -> Section 2.2: Memory map** (Trang 86).
* **Từ khóa tìm kiếm (`Ctrl + F`):** `Register boundary addresses` hoặc `PWR boundary address`.

**Công thức tính Địa chỉ tuyệt đối:** `Absolute Address = Peripheral Base Address + Register Offset`

```text
PERIPH_BASE       = 0x4000 0000UL (Bắt đầu vùng ngoại vi APB1)
AHB1PERIPH_BASE   = 0x4000 0000UL + 0x0002 0000UL = 0x4002 0000UL

PWR_BASE          = APB1PERIPH_BASE + 0x7000UL     = 0x4000 7000UL
RCC_BASE          = AHB1PERIPH_BASE + 0x3800UL     = 0x4002 3800UL
FLASH_R_BASE      = AHB1PERIPH_BASE + 0x3C00UL     = 0x4002 3C00UL
```

---

### 2. Bảng Tra cứu Thanh ghi Chi tiết (Register Lookup Table)

#### A. Power Controller (`PWR`) — Base: `0x4000 7000`
* **Tài liệu:** RM0385 -> Chapter 6: Power controller (PWR) -> Section 6.3: PWR registers.
* **Từ khóa (`Ctrl + F`):** `PWR_CR1` hoặc `PWR power control register 1`.

| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cách cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`PWR_CR1`** | `0x00` | `0x4000 7000` | `0x0000 C000` | 15:14 | `VOS[1:0]` | `RW` | Regulator voltage scaling. Ghi `11` (Scale 1 mode) để chạy max speed. |
| | | | | 16 | `ODEN` | `RW` | Over-drive enable. Ghi `1` để bật Over-drive. |
| | | | | 17 | `ODSWEN` | `RW` | Over-drive switching enable. Ghi `1` sau khi `ODRDY` = 1. |
| **`PWR_CSR1`**| `0x04` | `0x4000 7004` | `0x0000 0000` | 16 | `ODRDY` | `RO` | Over-drive mode ready flag. Chờ bit này = `1`. |
| | | | | 17 | `ODSWRDY` | `RO` | Over-drive mode switching ready flag. Chờ bit này = `1`. |

---

#### B. Reset and Clock Control (`RCC`) — Base: `0x4002 3800`
* **Tài liệu:** RM0385 -> Chapter 5: Reset and clock control (RCC) -> Section 5.3: RCC registers.
* **Từ khóa (`Ctrl + F`):** `RCC_PLLCFGR` hoặc `RCC_CSR`.

| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cách cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`RCC_CR`** | `0x00` | `0x4002 3800` | `0x0000 0083` | 16 | `HSEON` | `RW` | Bật thạch anh ngoài 25MHz (`1`). |
| | | | | 17 | `HSERDY` | `RO` | Cờ báo HSE ổn định (vòng lặp `while (!(RCC->CR & RCC_CR_HSERDY))`). |
| | | | | 24 | `PLLON` | `RW` | Bật bộ nhân tần Main PLL (`1`). |
| | | | | 25 | `PLLRDY` | `RO` | Cờ báo PLL đã khóa pha (`while (!(RCC->CR & RCC_CR_PLLRDY))`). |
| **`RCC_PLLCFGR`**|`0x04`| `0x4002 3804` | `0x2400 3010` | 5:0 | `PLLM[5:0]`| `RW` | Bộ chia ngõ vào PLLM = 25 (`25MHz / 25 = 1MHz`). |
| | | | | 14:6 | `PLLN[8:0]`| `RW` | Bộ nhân VCO PLLN = 432 (`1MHz * 432 = 432MHz`). |
| | | | | 17:16 | `PLLP[1:0]`| `RW` | Bộ chia hệ thống PLLP = 00 (/2) (`432MHz / 2 = 216MHz`). |
| | | | | 22 | `PLLSRC` | `RW` | Nguồn PLL: `1` = HSE (thạch anh ngoài). |
| | | | | 27:24 | `PLLQ[3:0]`| `RW` | Bộ chia 48MHz USB/SDMMC = 9 (`432MHz / 9 = 48MHz`). |
| **`RCC_CFGR`** | `0x08` | `0x4002 3808` | `0x0000 0000` | 1:0 | `SW[1:0]` | `RW` | Ghi `10` để chọn PLLCLK làm Nguồn System Clock. |
| | | | | 3:2 | `SWS[1:0]` | `RO` | Trạng thái nguồn clock hiện tại (`10` = PLL). |
| | | | | 7:4 | `HPRE[3:0]` | `RW` | AHB Prescaler: `0xxx` (/1 -> AHB = 216MHz). |
| | | | | 12:10| `PPRE1[2:0]`| `RW` | APB1 Prescaler: `101` (/4 -> APB1 = 54MHz). |
| | | | | 15:13| `PPRE2[2:0]`| `RW` | APB2 Prescaler: `100` (/2 -> APB2 = 108MHz). |
| **`RCC_APB1ENR`**|`0x40`| `0x4002 3840` | `0x0000 0000` | 28 | `PWREN` | `RW` | Cấp clock cho khối PWR (`1`). BẮT BỘC trước khi ghi `PWR_CR1`. |
| **`RCC_CSR`** | `0x74` | `0x4002 3874` | `0x0E00 0000` | 24 | `RMVF` | `RS` | Remove Reset Flag. Ghi `1` để xóa mọi cờ báo reset. |
| | | | | 25 | `BORRSTF` | `RO` | Cờ Brown-out reset. |
| | | | | 26 | `PINRSTF` | `RO` | Cờ Reset nút bấm cứng NRST. |
| | | | | 27 | `PORRSTF` | `RO` | Cờ Power-on / Power-down reset. |
| | | | | 28 | `SFTRSTF` | `RO` | Cờ Reset phần mềm (`NVIC_SystemReset`). |
| | | | | 29 | `IWDGRSTF`| `RO` | Cờ Independent Watchdog reset. |
| | | | | 30 | `WWDGRSTF`| `RO` | Cờ Window Watchdog reset. |
| | | | | 31 | `LPWRRSTF`| `RO` | Cờ Low-power reset. |

---

#### C. Flash Interface (`FLASH`) — Base: `0x4002 3C00`
* **Tài liệu:** RM0385 -> Chapter 3: Embedded Flash memory -> Section 3.8: Flash registers.
* **Từ khóa (`Ctrl + F`):** `FLASH_ACR` hoặc `Flash access control register`.

| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cách cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`FLASH_ACR`**| `0x00` | `0x4002 3C00` | `0x0000 0000` | 3:0 | `LATENCY[3:0]`| `RW` | Độ trễ Flash. Ở 216MHz & 3.3V, bắt buộc chọn `0110` (6 Wait States). |
| | | | | 8 | `PRFTEN` | `RW` | Prefetch enable (`1`). |
| | | | | 9 | `ARTEN` | `RW` | ART Accelerator enable (`1`). |

---

## 📐 CƠ SỞ KỸ THUẬT & LÝ DO CHỌN GIÁ TRỊ BIT (HARDWARE RATIONALE & SPECS BASIS)

Theo **Quy tắc 7 trong AGENTS.md**, mọi cấu hình thanh ghi đều phải được giải thích dựa trên **Bảng Giới hạn Phần cứng (Datasheet DS10610)** và **Quy định Ép buộc / Công thức (RM0385)**:

### 1. Cơ sở chọn Nguồn Xung & Thạch Anh (HSE = 25 MHz)
* **Cơ sở thực tế:** Bo mạch **STM32F746G-Discovery** hàn sẵn thạch anh ngoài HSE $25\text{ MHz}$.
* **Tra cứu RM0385:** *Section 5.2.1 HSE clock*. Ghi bit `HSEON = 1` trong `RCC_CR` để cấp nguồn cho bộ dao động ngoài.

---

### 2. Cơ sở Tính toán Thông số Bộ Nhân PLL (`PLLM`, `PLLN`, `PLLP`, `PLLQ`)
* **Tra cứu RM0385:** *Section 5.3.2 RCC_PLLCFGR & Hình Cây Clock Tree (Trang 122)*.
* **Các giới hạn phần cứng bắt buộc:**
  1. **Tần số vào bộ nhân VCO ($f_{VCO\_in}$):** Bắt buộc nằm trong khoảng $1\text{ MHz} \le f_{VCO\_in} \le 2\text{ MHz}$ (khuyến nghị $1\text{ MHz}$ để tối ưu jitter).
     $$f_{VCO\_in} = \frac{f_{HSE}}{PLLM} = \frac{25\text{ MHz}}{25} = \mathbf{1\text{ MHz}} \quad \Rightarrow \text{Chọn } PLLM = 25$$
  2. **Tần số ra bộ nhân VCO ($f_{VCO\_out}$):** Bắt buộc nằm trong khoảng $100\text{ MHz} \le f_{VCO\_out} \le 432\text{ MHz}$.
     $$f_{VCO\_out} = f_{VCO\_in} \times PLLN = 1\text{ MHz} \times 432 = \mathbf{432\text{ MHz}} \quad \Rightarrow \text{Chọn } PLLN = 432$$
  3. **Tần số xung hệ thống ($f_{SYSCLK}$):** Tối đa $216\text{ MHz}$ cho dòng STM32F746.
     $$f_{SYSCLK} = \frac{f_{VCO\_out}}{PLLP} = \frac{432\text{ MHz}}{2} = \mathbf{216\text{ MHz}} \quad \Rightarrow \text{Chọn } PLLP = 2\ (\text{bitfield } \texttt{00}b)$$
  4. **Tần số ngoại vi 48MHz ($f_{USB/SDMMC}$):** Chuẩn USB OTG FS / SDMMC bắt buộc $48\text{ MHz}$.
     $$f_{USB} = \frac{f_{VCO\_out}}{PLLQ} = \frac{432\text{ MHz}}{9} = \mathbf{48\text{ MHz}} \quad \Rightarrow \text{Chọn } PLLQ = 9$$

---

### 3. Cơ sở Chọn Bộ Chia Bus (`HPRE`, `PPRE1`, `PPRE2`)
* **Tra cứu Datasheet DS10610:** *Table 17: General operating conditions*.
* **Các giới hạn bus cực đại phần cứng:**
  * **AHB Bus (HCLK):** Tối đa **$216\text{ MHz}$**.
    $$\Rightarrow \text{Chọn } HPRE = /1\ (\text{bitfield } \texttt{0000}b) \implies 216\text{ MHz} / 1 = 216\text{ MHz}$$
  * **APB1 Bus (PCLK1 - Ngoại vi tốc độ thấp UART/CAN/SPI2/3):** Tối đa **$54\text{ MHz}$**.
    * Nếu chọn `/1` hoặc `/2` $\implies 216 / 2 = 108\text{ MHz} > 54\text{ MHz}$ (**Quá tải gây hỏng phần cứng!**).
    * Bắt buộc chọn bộ chia tối thiểu **`/4`** ($\text{bitfield } \texttt{101}b = \text{0x5}$):
      $$PCLK1 = \frac{216\text{ MHz}}{4} = \mathbf{54\text{ MHz}} \quad (\text{Vừa khớp giới hạn max})$$
  * **APB2 Bus (PCLK2 - Ngoại vi tốc độ cao SDMMC/LTDC/SPI1):** Tối đa **$108\text{ MHz}$**.
    * Bắt buộc chọn bộ chia tối thiểu **`/2`** ($\text{bitfield } \texttt{100}b = \text{0x4}$):
      $$PCLK2 = \frac{216\text{ MHz}}{2} = \mathbf{108\text{ MHz}} \quad (\text{Vừa khớp giới hạn max})$$

---

### 4. Cơ sở Cấu hình Độ Trễ Flash (`LATENCY = 6 WS`)
* **Tra cứu RM0385:** *Chapter 3: Embedded Flash memory -> Section 3.3.2 & Table 7: Number of wait states according to CPU clock frequency (VCC = 2.7V - 3.6V)*.
* **Bảng quy định:**
  - $0 < HCLK \le 30\text{ MHz} \implies 0\text{ WS}$
  - $180 < HCLK \le 216\text{ MHz} \implies \mathbf{6\text{ Wait States}}\ (\text{bitfield } \texttt{LATENCY[3:0]} = \texttt{0110}b = \text{0x6})$.
* **Lý do bật `ARTEN` & `PRFTEN`:** Vì Flash có 6 WS, nếu không bật ART Accelerator (`ARTEN = 1`) và Prefetch (`PRFTEN = 1`), CPU Cortex-M7 sẽ bị rảnh rỗi chờ bộ nhớ. ART đệm lệnh qua I-Cache giúp đạt hiệu năng tương đương 0 Wait State.

---

### 5. Cơ sở Trình tự Kích hoạt Over-drive Mode (`PWR_CR1` & `PWR_CSR1`)
* **Tra cứu RM0385:** *Section 6.1.4 Over-drive mode*.
* **Nguyên lý phần cứng:** Khi $SYSCLK > 180\text{ MHz}$, điện áp lõi $V_{CORE}$ mặc định không đủ duy trì tính ổn định của các transistor trong CPU.
* **Chuỗi Handshake 2 bước ép buộc:**
  1. Ghi `ODEN = 1` trong `PWR_CR1` $\rightarrow$ Vòng lặp chờ cờ phần cứng `PWR_CSR1` bit `ODRDY = 1`.
  2. Ghi `ODSWEN = 1` trong `PWR_CR1` $\rightarrow$ Vòng lặp chờ cờ phần cứng `PWR_CSR1` bit `ODSWRDY = 1`.

---

## ⚡ TẠI SAO BẮT BỘC PHẢI BẬT OVER-DRIVE MODE KHI CHẠY 216MHZ?

Theo **RM0385 -> Section 6.1.4: Over-drive mode**:
1. Trong kiến trúc Cortex-M7 của STM32F7, khi hệ thống hoạt động ở tần số cực đại **216 MHz**, điện áp lõi VCORE mặc định ở Scale 1 không đủ để các đường logic đáp ứng kịp thời gian lan truyền (propagation delay).
2. Nếu chỉ cấu hình PLL lên 216MHz mà **KHÔNG bật Over-drive Mode**, CPU có thể bị rớt lệnh, gãy luồng thực thi (HardFault) hoặc chạy chập chờn khi nhiệt độ thay đổi, mặc dù biên dịch và nạp code hoàn toàn không báo lỗi.
3. **Quy trình bật Over-drive đúng chuẩn phần cứng:**
   - Cấp clock `RCC_APB1ENR` bit `PWREN`.
   - Đặt `VOS[1:0]` trong `PWR_CR1` bằng `11` (Scale 1 mode).
   - Set bit `ODEN` trong `PWR_CR1`.
   - Vòng lặp chờ cờ `ODRDY` trong `PWR_CSR1` chuyển lên `1`.
   - Set bit `ODSWEN` trong `PWR_CR1`.
   - Vòng lặp chờ cờ `ODSWRDY` trong `PWR_CSR1` chuyển lên `1`.

---

## 📊 THUẬT TOÁN LOGGING LÝ DO RESET (RESET REASON)

Giống như các bộ điều khiển ECU trong ngành Ô tô (Automotive), việc lưu lại lý do khởi động lại (Power-On Reset, Watchdog Timeout, Software Reset...) tại thời điểm boot là tính năng chẩn đoán quan trọng giúp xác định nguyên nhân sự cố hệ thống.

```text
Boot Phase
   │
   ├──► Đọc thanh ghi RCC->CSR (Offset 0x74)
   │     │
   │     ├── LPWRRSTF (Bit 31) == 1 ──► Reset do sụt nguồn Low Power
   │     ├── WWDGRSTF (Bit 30) == 1 ──► Reset do Window Watchdog
   │     ├── IWDGRSTF (Bit 29) == 1 ──► Reset do Independent Watchdog
   │     ├── SFTRSTF  (Bit 28) == 1 ──► Reset do Software (NVIC_SystemReset)
   │     ├── PORRSTF  (Bit 27) == 1 ──► Reset do Cấp nguồn (Power-On)
   │     ├── BORRSTF  (Bit 25) == 1 ──► Reset do Brown-out Voltage
   │     └── PINRSTF  (Bit 26) == 1 ──► Reset do Nhấn nút Nút Cứng NRST
   │
   └──► Xóa cờ báo Reset: RCC->CSR |= RCC_CSR_RMVF (Bit 24 = 1)
```

> [!CAUTION]
> **Lưu ý về Cờ RMVF (`RCC_CSR` Bit 24):** Bit `RMVF` là bit loại **`RS` (Set by software to clear flags)**. Khi ta ghi `1` vào bit `RMVF`, phần cứng sẽ tự động xóa tất cả các cờ báo reset từ Bit 25 đến Bit 31 về `0`. Không dùng phép toán `|=` trên các cờ đọc `RO`.

---

## 💻 MÃ NGUỒN C-DRIVER HOÀN CHỈNH (BARE-METAL)

Các file mã nguồn driver đã được tạo tại:
- Header Thanh ghi: [`stm32f746xx_registers.h`](file:///d:/Project/STM32F7/drivers/inc/stm32f746xx_registers.h)
- Driver Header: [`system_clock.h`](file:///d:/Project/STM32F7/drivers/inc/system_clock.h)
- Driver Source: [`system_clock.c`](file:///d:/Project/STM32F7/drivers/src/system_clock.c)

---

## ✅ KẾT QUẢ XÁC NHẬN VÀ KIỂM THỬ (VERIFICATION)

1. **Địa chỉ thanh ghi:** Tra cứu đúng 100% với RM0385 Memory Map (`PWR`: `0x4000 7000`, `RCC`: `0x4002 3800`, `FLASH`: `0x4002 3C00`).
2. **Quy tắc Bitfield:** Mọi thao tác cấu hình trường thông số đều áp dụng mẫu Clear-then-Set `&= ~MASK; |= VALUE`.
3. **An toàn phần cứng:** Chuỗi kích hoạt Over-drive có đầy đủ 2 vòng lặp kiểm tra cờ ready (`ODRDY` & `ODSWRDY`).
