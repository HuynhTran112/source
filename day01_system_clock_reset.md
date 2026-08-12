# 📗 [NGÀY 1] HƯỚNG DẪN BARE-METAL: 216MHz OVER-DRIVE CLOCK & RESET REASON LOGGING

Tài liệu này hướng dẫn chi tiết cách lập trình thanh ghi bare-metal (không sử dụng thư viện HAL/LL) cho **Cấu hình Xung hệ thống 216MHz ở chế độ Over-drive** và **Đọc / Xóa lý do Reset cứng trong RCC->CSR** trên vi điều khiển **STM32F746NG (ARM Cortex-M7)**.

Mọi mã nguồn và hướng dẫn trong tài liệu này đều **tuân thủ tuyệt đối 6 Quy tắc Bare-metal cốt lõi** đã được xác lập trong [`AGENTS.md`](file:///d:/Project/STM32F7/.agents/AGENTS.md) và [`baremetal_register_rules.md`](file:///d:/Project/STM32F7/docs/baremetal_register_rules.md).

---

## 🛠️ RÀ SOÁT 6 QUY TẮC BARE-METAL CỐT LÕI

| STT | Quy tắc | Áp dụng trong Ngày 1 |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `PWR_CR1`, `PWR_CSR1`, `RCC_CR`, `RCC_PLLCFGR`, `RCC_CFGR`, `FLASH_ACR`, `RCC_CSR` trước khi tác động. |
| **2** | **Access Type** | Phân biệt bit `RO` (`HSERDY`, `PLLRDY`, `ODRDY`, `ODSWRDY`, `SWS`, cờ reset), bit `RW` (`ODEN`, `ODSWEN`, `HSEON`, `PLLON`), và bit `RS` (`RMVF` - set 1 để clear). Không bao giờ `|=` trên cờ W1C/RS. |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường `VOS[1:0]`, `LATENCY[3:0]`, `PLLM[5:0]`, `PLLN[8:0]`, `PLLP[1:0]`, `PLLQ[3:0]`, `HPRE[3:0]`, `PPRE1[2:0]`, `PPRE2[2:0]`, `SW[1:0]`. |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi (`PWR_TypeDef`, `RCC_TypeDef`, `FLASH_TypeDef`) đều sử dụng từ khóa `volatile uint32_t` để chống compiler optimization. |
| **5** | **Interrupt Workflow** | Quy trình 5 bước đối với ngắt (được chuẩn bị sẵn nếu kích hoạt ngắt sụt áp PVD / Clock Security System CSS). |
| **6** | **RM / DS Lookup** | Ghi rõ tên chương, số hiệu mục, từ khóa `Ctrl + F`, và công thức tính `Base Address + Offset` cho từng thanh ghi. |

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
