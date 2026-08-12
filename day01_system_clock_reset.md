# 📗 [NGÀY 1] HƯỚNG DẪN BARE-METAL: 216MHz OVER-DRIVE CLOCK & RESET REASON LOGGING

Tài liệu này hướng dẫn chi tiết cách lập trình thanh ghi bare-metal (không sử dụng thư viện HAL/LL) cho **Cấu hình Xung hệ thống 216MHz ở chế độ Over-drive** và **Đọc / Xóa lý do Reset cứng trong RCC->CSR** trên vi điều khiển **STM32F746NG (ARM Cortex-M7)**.

Mọi mã nguồn và hướng dẫn đều **tuân thủ tuyệt đối 7 Quy tắc Bare-metal cốt lõi** trong [`AGENTS.md`](file:///d:/Project/STM32F7/.agents/AGENTS.md).

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
| **7** | **Hardware Rationale & Specs** | Đặt **Cơ sở Kỹ thuật & Giới hạn Phần cứng** ngay sát từng thanh ghi (kèm bảng DS10610, RM0385 và công thức toán học) giúp người đọc không bị loãng thông tin. |

---

## 🔍 CHI TIẾT CẤU HÌNH THANH GHI & CƠ SỞ KỸ THUẬT

### 1. Bản đồ bộ nhớ & Địa chỉ Base Address
* **Tài liệu tra cứu:** Reference Manual **RM0385** -> *Chapter 2: Memory map -> Section 2.2* (Trang 86).
* **Từ khóa (`Ctrl + F`):** `Register boundary addresses` hoặc `PWR boundary address`.
* **Công thức địa chỉ:** `Absolute Address = Peripheral Base Address + Register Offset`

```text
PERIPH_BASE       = 0x4000 0000UL (Bắt đầu vùng APB1)
AHB1PERIPH_BASE   = 0x4000 0000UL + 0x0002 0000UL = 0x4002 0000UL

PWR_BASE          = APB1PERIPH_BASE + 0x7000UL     = 0x4000 7000UL
RCC_BASE          = AHB1PERIPH_BASE + 0x3800UL     = 0x4002 3800UL
FLASH_R_BASE      = AHB1PERIPH_BASE + 0x3C00UL     = 0x4002 3C00UL
```

---

### 2. Power Controller (`PWR`) & Kích hoạt Over-Drive Mode
* **RM Tra cứu:** RM0385 -> *Chapter 6: Power controller (PWR) -> Section 6.3: PWR registers*.
* **Từ khóa (`Ctrl + F`):** `PWR_CR1` hoặc `PWR power control register 1`.

#### Bảng Thanh ghi `PWR`
| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`PWR_CR1`** | `0x00` | `0x4000 7000` | `0x0000 C000` | 15:14 | `VOS[1:0]` | `RW` | Regulator voltage scaling. Ghi `11` (Scale 1 mode). |
| | | | | 16 | `ODEN` | `RW` | Over-drive enable. Ghi `1` để bật Over-drive. |
| | | | | 17 | `ODSWEN` | `RW` | Over-drive switching enable. Ghi `1` sau khi `ODRDY` = 1. |
| **`PWR_CSR1`**| `0x04` | `0x4000 7004` | `0x0000 0000` | 16 | `ODRDY` | `RO` | Over-drive mode ready flag. Chờ bit = `1`. |
| | | | | 17 | `ODSWRDY` | `RO` | Over-drive mode switching ready flag. Chờ bit = `1`. |

#### 📐 Cơ sở Kỹ thuật & Giới hạn Phần cứng (PWR & Over-drive)
1. **Tại sao cần Scale 1 (`VOS = 11`)?** Để chạy tần số tối đa 216MHz, điện áp bộ điều áp $V_{CORE}$ phải đạt mức Scale 1 (1.2V).
2. **Tại sao BẮT BỘC phải bật Over-drive Mode?** (*RM0385 Section 6.1.4*)
   - Khi $SYSCLK > 180\text{ MHz}$, điện áp Scale 1 tiêu chuẩn không đủ làm các transistor trong lõi Cortex-M7 đóng mở kịp thời gian lan truyền (propagation delay). Nếu không bật Over-drive, CPU sẽ bị HardFault hoặc chạy chập chờn dù nạp code thành công.
3. **Chuỗi Handshake 2 bước phần cứng:**
   - Cấp clock `RCC_APB1ENR` bit `PWREN`.
   - Set `ODEN = 1` $\rightarrow$ Vòng lặp chờ cờ phần cứng `PWR_CSR1` bit `ODRDY = 1`.
   - Set `ODSWEN = 1` $\rightarrow$ Vòng lặp chờ cờ phần cứng `PWR_CSR1` bit `ODSWRDY = 1`.

---

### 3. Nguồn Clock & Bộ nhân Main PLL (`RCC_CR` & `RCC_PLLCFGR`)
* **RM Tra cứu:** RM0385 -> *Chapter 5: Reset and clock control (RCC) -> Section 5.3.1 & 5.3.2*.
* **Từ khóa (`Ctrl + F`):** `RCC_PLLCFGR` hoặc `Main PLL configuration register`.

#### Bảng Thanh ghi `RCC_CR` & `RCC_PLLCFGR`
| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`RCC_CR`** | `0x00` | `0x4002 3800` | `0x0000 0083` | 16 | `HSEON` | `RW` | Bật thạch anh ngoài 25MHz (`1`). |
| | | | | 17 | `HSERDY` | `RO` | Cờ báo HSE ổn định (vòng lặp chờ `1`). |
| | | | | 24 | `PLLON` | `RW` | Bật bộ nhân Main PLL (`1`). |
| | | | | 25 | `PLLRDY` | `RO` | Cờ báo PLL khóa pha (vòng lặp chờ `1`). |
| **`RCC_PLLCFGR`**|`0x04`| `0x4002 3804` | `0x2400 3010` | 5:0 | `PLLM[5:0]`| `RW` | Bộ chia ngõ vào PLLM = 25. |
| | | | | 14:6 | `PLLN[8:0]`| `RW` | Bộ nhân VCO PLLN = 432. |
| | | | | 17:16 | `PLLP[1:0]`| `RW` | Bộ chia hệ thống PLLP = 00 (/2). |
| | | | | 22 | `PLLSRC` | `RW` | Nguồn PLL: `1` = HSE (thạch anh ngoài). |
| | | | | 27:24 | `PLLQ[3:0]`| `RW` | Bộ chia 48MHz USB/SDMMC = 9. |

#### 📐 Cơ sở Kỹ thuật & Công thức Toán học (PLL Parameters)
1. **Nguồn vào HSE:** Bo mạch `STM32F746G-Discovery` hàn sẵn thạch anh ngoài $25\text{ MHz}$.
2. **Công thức & Giới hạn phần cứng (*RM0385 Section 5.3.2*):**
   - **Tần số vào VCO ($f_{VCO\_in}$):** RM0385 quy định $1\text{ MHz} \le f_{VCO\_in} \le 2\text{ MHz}$ (khuyến nghị $1\text{ MHz}$).
     $$f_{VCO\_in} = \frac{f_{HSE}}{PLLM} = \frac{25\text{ MHz}}{25} = \mathbf{1\text{ MHz}} \quad (\text{Chọn } PLLM = 25)$$
   - **Tần số ra VCO ($f_{VCO\_out}$):** RM0385 quy định $100\text{ MHz} \le f_{VCO\_out} \le 432\text{ MHz}$.
     $$f_{VCO\_out} = f_{VCO\_in} \times PLLN = 1\text{ MHz} \times 432 = \mathbf{432\text{ MHz}} \quad (\text{Chọn } PLLN = 432)$$
   - **Tần số System Clock ($f_{SYSCLK}$):** Max speed dòng STM32F746 là $216\text{ MHz}$.
     $$f_{SYSCLK} = \frac{f_{VCO\_out}}{PLLP} = \frac{432\text{ MHz}}{2} = \mathbf{216\text{ MHz}} \quad (\text{Chọn } PLLP = 2, \text{bitfield } \texttt{00}b)$$
   - **Tần số ngoại vi 48MHz ($f_{USB}$):** Chuẩn USB FS / SDMMC bắt buộc đúng $48\text{ MHz}$.
     $$f_{USB} = \frac{f_{VCO\_out}}{PLLQ} = \frac{432\text{ MHz}}{9} = \mathbf{48\text{ MHz}} \quad (\text{Chọn } PLLQ = 9)$$

---

### 4. Cấu hình Bộ Chia Bus & Chuyển Nguồn Clock (`RCC_CFGR`)
* **RM Tra cứu:** RM0385 -> *Chapter 5: Reset and clock control (RCC) -> Section 5.3.3*.
* **Từ khóa (`Ctrl + F`):** `PPRE1[2:0]` hoặc `APB Low speed prescaler`.

#### Bảng Thanh ghi `RCC_CFGR`
| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`RCC_CFGR`** | `0x08` | `0x4002 3808` | `0x0000 0000` | 1:0 | `SW[1:0]` | `RW` | Ghi `10` để chọn PLLCLK làm SYSCLK. |
| | | | | 3:2 | `SWS[1:0]` | `RO` | Trạng thái nguồn clock hiện tại (`10` = PLL). |
| | | | | 7:4 | `HPRE[3:0]` | `RW` | AHB Prescaler: `0xxx` (/1 -> AHB = 216MHz). |
| | | | | 12:10| `PPRE1[2:0]`| `RW` | APB1 Prescaler: `101` (/4 -> APB1 = 54MHz). |
| | | | | 15:13| `PPRE2[2:0]`| `RW` | APB2 Prescaler: `100` (/2 -> APB2 = 108MHz). |

#### 📐 Cơ sở Kỹ thuật & Giới hạn Bus (*Datasheet DS10610 Table 17*)
1. **AHB Bus (HCLK):** Giới hạn tối đa **$216\text{ MHz}$** $\rightarrow$ Chọn `HPRE` = `/1` $\implies 216\text{ MHz} / 1 = 216\text{ MHz}$.
2. **APB1 Bus (PCLK1 - Ngoại vi tốc độ thấp UART/CAN/SPI2/3):** Giới hạn cứng tối đa **$54\text{ MHz}$**.
   - Nếu chọn `/1` hoặc `/2` $\implies 216 / 2 = 108\text{ MHz} > 54\text{ MHz}$ (**Vượt giới hạn gây hỏng ngoại vi!**).
   - Bắt buộc chọn bộ chia tối thiểu **`/4`** ($\text{bitfield } \texttt{101}b = \text{0x5}$):
     $$PCLK1 = \frac{216\text{ MHz}}{4} = \mathbf{54\text{ MHz}} \quad (\text{Vừa khớp giới hạn max})$$
3. **APB2 Bus (PCLK2 - Ngoại vi tốc độ cao SDMMC/LTDC/SPI1):** Giới hạn cứng tối đa **$108\text{ MHz}$**.
   - Bắt buộc chọn bộ chia tối thiểu **`/2`** ($\text{bitfield } \texttt{100}b = \text{0x4}$):
     $$PCLK2 = \frac{216\text{ MHz}}{2} = \mathbf{108\text{ MHz}} \quad (\text{Vừa khớp giới hạn max})$$

---

### 5. Flash Interface (`FLASH_ACR`) & Wait States
* **RM Tra cứu:** RM0385 -> *Chapter 3: Embedded Flash memory -> Section 3.8.1*.
* **Từ khóa (`Ctrl + F`):** `FLASH_ACR` hoặc `Flash access control register`.

#### Bảng Thanh ghi `FLASH_ACR`
| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`FLASH_ACR`**| `0x00` | `0x4002 3C00` | `0x0000 0000` | 3:0 | `LATENCY[3:0]`| `RW` | Độ trễ Flash. Ghi `0110` (6 Wait States). |
| | | | | 8 | `PRFTEN` | `RW` | Prefetch enable (`1`). |
| | | | | 9 | `ARTEN` | `RW` | ART Accelerator enable (`1`). |

#### 📐 Cơ sở Kỹ thuật (Flash Wait States & ART)
1. **Quy định Wait States (*RM0385 Table 7*) ở 3.3V:**
   - $0 < HCLK \le 30\text{ MHz} \implies 0\text{ WS}$
   - $180 < HCLK \le 216\text{ MHz} \implies \mathbf{6\text{ Wait States}}\ (\text{bitfield } \texttt{LATENCY[3:0]} = \texttt{0110}b = \text{0x6})$.
2. **Lý do bật `ARTEN` & `PRFTEN`:** Bộ nhớ Flash nhúng không thể đáp ứng tốc độ đọc $216\text{ MHz}$. Đệm lệnh ART Accelerator (`ARTEN = 1`) và Prefetch (`PRFTEN = 1`) qua I-Cache giúp CPU đạt hiệu năng tương đương 0 Wait State.

---

### 6. Đọc & Xóa Lý do Reset Cứng (`RCC_CSR`)
* **RM Tra cứu:** RM0385 -> *Chapter 5: Reset and clock control (RCC) -> Section 5.3.22*.
* **Từ khóa (`Ctrl + F`):** `RCC_CSR` hoặc `Clock control & status register`.

#### Bảng Thanh ghi `RCC_CSR`
| Thanh ghi | Offset | Địa chỉ Tuyệt đối | Reset Value | Bit | Tên Bit | Access | Mô tả & Cấu hình |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **`RCC_CSR`** | `0x74` | `0x4002 3874` | `0x0E00 0000` | 24 | `RMVF` | `RS` | Remove Reset Flag. Ghi `1` để xóa mọi cờ reset. |
| | | | | 25 | `BORRSTF` | `RO` | Cờ Brown-out reset. |
| | | | | 26 | `PINRSTF` | `RO` | Cờ Reset nút bấm cứng NRST. |
| | | | | 27 | `PORRSTF` | `RO` | Cờ Power-on / Power-down reset. |
| | | | | 28 | `SFTRSTF` | `RO` | Cờ Reset phần mềm (`NVIC_SystemReset`). |
| | | | | 29 | `IWDGRSTF`| `RO` | Cờ Independent Watchdog reset. |
| | | | | 30 | `WWDGRSTF`| `RO` | Cờ Window Watchdog reset. |
| | | | | 31 | `LPWRRSTF`| `RO` | Cờ Low-power reset. |

#### 📐 Cơ sở Kỹ thuật (Reset Flags Clearing Mechanism)
- Bit `RMVF` (Remove Reset Flag) là bit kiểu **`RS` (Set by software to clear flags)**.
- Các cờ reset (Bit 25..31) đều là kiểu **`RO` (Read-Only)**, không thể ghi `|= 0` thủ công. Ghi `1` vào `RMVF` (`RCC->CSR |= RCC_CSR_RMVF;`) bắt buộc phần cứng xả sạch các cờ về `0` để sẵn sàng cho đợt chẩn đoán sau.

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
3. **Cơ sở phần cứng liền kề:** 100% thanh ghi đều được đính kèm phần giải thích cơ sở kỹ thuật, giới hạn bus và công thức ngay phía dưới.
