# 🏆 [NGÀY 6] CẨM NANG TOÀN DIỆN BARE-METAL: BẢO VỆ ĐỘ TIN CẬY HỆ THỐNG (IWDG, WWDG, CSS FAILOVER & PVD)
## Lộ trình 4 Bước: Nguyên Lý Phần Cứng ➔ Thực Chiến RM0385 ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ các khối phần cứng bảo vệ an toàn sống còn chuẩn công nghiệp ô tô trên STM32F746: Chó canh độc lập **IWDG (Independent Watchdog)** chạy xung nội LSI 32kHz chống treo vi điều khiển, chó canh cửa sổ **WWDG (Window Watchdog)** phát hiện lỗi sai lệch tiến trình thực thi phần mềm (Software Flow Violation), mạch bảo vệ mất xung nhịp **CSS (Clock Security System)** tự động chuyển mạch dự phòng sang HSI và phát ngắt bất khả kháng NMI, cùng bộ giám sát điện áp khả trình **PVD (Programmable Voltage Detector)** lưu trữ dữ liệu an toàn trước khi sập nguồn.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, thanh ghi, công thức toán học, bảng tra cứu và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 6                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: NGUYÊN LÝ │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE            │ BƯỚC 4: PHỎNG VẤN          │
│ • Cơ chế IWDG LSI │ • Tra cứu RM0385  │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn Watchdog│
│ • Công thức T_out │ • Bảng Base Addr  │ • TODO 1-2 [watchdog.h]    │   & CSS Failover           │
│ • Cửa sổ WWDG     │ • Bảng Thanh ghi  │ • TODO 3-5 [watchdog.c]    │ • Debugger Breakpoint Trap │
│ • CSS NMI Failover│ • Bảng Khóa Key   │ • TODO 6 [main.c]          │ • Early Refresh Violation  │
│ • Giám sát PVD    │ • Thanh ghi DBGMCU│ • Mổ xẻ 5 Bug phần cứng    │ • Kịch bản trả lời 60s     │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC BARE-METAL CỐT LÕI (CHUYÊN CHO NGÀY 6)

| STT | Quy tắc Bare-metal | Thể hiện cụ thể trong Ngày 6 (Reliability & Watchdog) |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `IWDG_SR` (`0x0000 0000`), `WWDG_CR` (`0x0000 007F`), `RCC_CR` (bit `CSSON = 0`), và `PWR_CR1` trước khi cấu hình. |
| **2** | **Access Type** | **CỰC KỲ QUAN TRỌNG:** Thanh ghi `IWDG_KR` chỉ ghi (`Write-only`), không thể đọc. Cờ `EWIF` (Early Wakeup Interrupt Flag trong `WWDG_SR`) là dạng `rc_w0` (Ghi `0` để xóa cờ). |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường đa bit: `PR[2:0]` trong `IWDG_PR`, `W[6:0]` trong `WWDG_CFR`, và `PLS[2:0]` trong `PWR_CR1`. |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi IWDG, WWDG, DBGMCU và các cờ trạng thái chia sẻ trong ngắt NMI/PVD bắt buộc dùng `volatile`. |
| **5** | **Interrupt Workflow** | Quy trình ngắt bất khả kháng CSS NMI: Mất dao động HSE $\rightarrow$ Phần cứng dựng cờ `CSSF` trong `RCC_CIR` $\rightarrow$ Tự động nhảy vào `NMI_Handler` (Exception số 2, không thể bị vô hiệu hóa bởi `__disable_irq`) $\rightarrow$ Xóa cờ bằng `RCC->CIR |= RCC_CIR_CSSC`. |
| **6** | **RM / DS Lookup** | Cung cấp chính xác Chapter, Section, từ khóa `Ctrl + F`, công thức `Base Address + Offset` cho `IWDG` (RM0385 Chapter 25), `WWDG` (Chapter 26), `DBGMCU` (Chapter 38) và `PWR/PVD` (Chapter 6). |
| **7** | **Hardware Rationale & Specs** | Bố trí cơ sở kỹ thuật: tần số $f_{LSI} \approx 32\text{ kHz}$ (dao động $17\text{kHz} - 47\text{kHz}$ theo nhiệt độ), bảng công thức tính thời gian timeout IWDG và ngưỡng cửa sổ WWDG. |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ PHẦN CỨNG & CƠ CHẾ VẬT LÝ (HARDWARE ARCHITECTURE)

## 1.1. Bản chất Phần cứng của IWDG (Independent Watchdog)

Khối **IWDG** là một mạch đếm lùi phần cứng 12-bit hoàn toàn độc lập với phần còn lại của chip:
* **Nguồn xung độc lập:** Được cấp xung riêng biệt bởi bộ dao động nội **LSI (Low-Speed Internal RC) $\approx 32\text{ kHz}$**. Xung LSI tiếp tục dao động kể cả khi thạch anh ngoài HSE bị hỏng, bộ nhân PLL mất khóa pha, hoặc CPU rơi vào các chế độ tiết kiệm điện sâu (Stop/Standby).
* **Cơ chế hoạt động:** Bộ đếm 12-bit đếm lùi từ giá trị nạp sẵn $\text{RLR}$ về $0$. Nếu trước khi chạm mức $0$, phần mềm không ghi mã **`0xAAAA`** vào thanh ghi khóa `IWDG_KR` (hành động "Feed Dog / Kick Watchdog"), mạch phần cứng sẽ **kéo chân Reset nội bộ xuống mức thấp $\implies$ Vi điều khiển bị Reset cưỡng bức ngay lập tức**!

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                 MẠCH ĐỘC LẬP IWDG                                      │
│                                                                                        │
│   ┌────────────────────────┐      f_LSI (32 kHz)     ┌─────────────────────────────┐   │
│   │ Dao động nội RC LSI    │ ──────────────────────► │ Bộ chia Prescaler (/4../256)│   │
│   └────────────────────────┘                         └──────────────┬──────────────┘   │
│                                                                     │                  │
│                                           Xung đếm lùi              ▼                  │
│   ┌────────────────────────┐      Nạp lại RLR        ┌─────────────────────────────┐   │
│   │ CPU ghi Key: 0xAAAA    │ ──────────────────────► │ Bộ đếm lùi 12-bit Counter   │   │
│   └────────────────────────┘      (Đá chó định kỳ)   └──────────────┬──────────────┘   │
│                                                                     │                  │
│                                                                     ▼ (Chạm mốc 0)     │
│                                                      ┌─────────────────────────────┐   │
│                                                      │ PHÁT LỆNH RESET CỨNG HỆ THỐNG│   │
│                                                      │ (Dựng cờ IWDGRSTF ở RCC_CSR)│   │
│                                                      └─────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### Công thức Toán học Tính Thời Gian Timeout của IWDG:
Thời gian đếm lùi tối đa trước khi Reset hệ thống được tính theo công thức:
$$T_{timeout} = \frac{4 \times 2^{\text{PR[2:0]}} \times (\text{RLR} + 1)}{f_{LSI}}$$

Với tần số chuẩn $f_{LSI} = 32,000\text{ Hz}$, ta chọn bộ chia **$\text{Prescaler} = /64$** (mã `PR[2:0] = 010`b $\implies 4 \times 2^2 = 64$):
* Một chu kỳ đếm: $t_{tick} = \frac{64}{32,000} = 0.002\text{ s} = \mathbf{2\text{ ms}}$.
* Muốn thời gian Timeout an toàn là **$2.0\text{ giây}$**:
  $$\text{RLR} = \frac{T_{timeout}}{t_{tick}} - 1 = \frac{2000\text{ ms}}{2\text{ ms}} - 1 = \mathbf{999} \quad (\text{Mã Hex: } \texttt{0x3E7})$$

---

## 1.2. Bản chất Phần cứng của WWDG (Window Watchdog) & Lỗi "Refresh Quá Sớm"

Khác với IWDG chỉ quan tâm xem phần mềm có bị "chết đứng" hay không, **WWDG** được sinh ra để phát hiện lỗi **"chạy loạn chu trình" (Program Flow Error)**:
* **Nguồn xung:** Chạy bằng xung bus **APB1 ($f_{PCLK1} = 54\text{ MHz}$)** thông qua bộ chia nội.
* **Bộ đếm 7-bit (`T[6:0]`):** Đếm lùi từ một giá trị (ví dụ `0x7F` = 127) xuống `0x3F` (63).
* **Quy tắc Cửa Sổ 2 Đầu (Window Operation):**
  1. **Quá muộn (Underflow):** Nếu để bộ đếm tụt xuống dưới `0x40` (tức là bit `T6` chuyển từ $1 \rightarrow 0$) $\implies$ Reset CPU!
  2. **Quá sớm (Window Violation):** Nếu phần mềm nạp lại bộ đếm khi giá trị đếm **vẫn còn lớn hơn ngưỡng cửa sổ `W[6:0]`** $\implies$ Reset CPU ngay lập tức!

```text
 Giá trị Counter
      ▲
 0x7F │ ─── Khởi đầu
      │      ❌ VÙNG NGUY HIỂM 1: CẤM REFRESH! (Nếu refresh tại đây -> Reset CPU ngay!)
  W   │ ─── Ngưỡng Cửa Sổ (Window Value)
      │      ✅ VÙNG CHO PHÉP REFRESH AN TOÀN (Window Open)
 0x40 │ ─── Ngưỡng Cảnh Báo Cuối (Early Wakeup Interrupt - EWI)
      │      ❌ VÙNG NGUY HIỂM 2: TRỄ HẠN! (Counter tụt xuống 0x3F -> Reset CPU!)
 0x3F ┴────────────────────────────────────────────────────────────────────────► Thời gian
```

> 🎯 **Giá trị kỹ thuật ô tô:** Nếu code bị con trỏ hoang đâm loạn xạ hoặc vòng lặp bị nhảy cóc bước khiến lệnh refresh chó bị gọi liên tục không đúng chu kỳ $\implies$ WWDG sẽ phát hiện và khởi động lại vi điều khiển ngay.

---

## 1.3. Cơ Chế Bảo Vệ Mất Xung Nhịp CSS (Clock Security System) & Ngắt NMI

Nếu xe đang chạy ở tốc độ 100 km/h mà thạch anh ngoài HSE 25MHz bị đứt chân do rung lắc cơ học:
1. Bộ nhân PLL sẽ mất nguồn xung đầu vào $\rightarrow$ Tần số dao động rơi tự do.
2. Nếu không có mạch bảo vệ, CPU sẽ chết đứng trong vòng vài micro-giây.
3. **Mạch phần cứng CSS (bật bằng bit `CSSON` trong `RCC_CR`):**
   * Liên tục so sánh chu kỳ của thạch anh ngoài HSE với dao động nội HSI 16MHz.
   * Ngay khi phát hiện HSE ngừng rung:
     * **Bước 1:** Phần cứng **tự động cắt kết nối HSE**, chuyển tức thì nguồn cấp SYSCLK sang dao động nội **HSI 16MHz**.
     * **Bước 2:** Phần cứng tự động tắt bit `PLLON` và phát tín hiệu ngắt **NMI (Non-Maskable Interrupt - Ngắt Bất Khả Kháng)**.
     * **Bước 3:** CPU nhảy ngay vào `NMI_Handler`, ghi nhận lỗi hỏng phần cứng vào Flash, chuyển trạng thái xe sang chế độ an toàn (Limp-Home Mode) và cảnh báo người lái!

---

# 📑 BƯỚC 2: THỰC CHIẾN & TRA CỨU RM0385 (SETUP & LOOKUP)

## 2.1. Bản đồ Địa chỉ Base Address Ngoại vi Ngày 6

Tra cứu RM0385 *Chapter 2: Memory map $\rightarrow$ Table 1*:

| Tên ngoại vi | Bus | Base Address | Offset | Địa chỉ tuyệt đối | Chức năng chính |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`IWDG`** | APB1 | `0x4000 3000` | `0x0000` | `0x4000 3000` | Independent Watchdog (Độc lập LSI). |
| **`WWDG`** | APB1 | `0x4000 2C00` | `0x0000` | `0x4000 2C00` | Window Watchdog (Cửa sổ APB1). |
| **`DBGMCU`** | System | `0xE004 2000` | `0x0000` | `0xE004 2000` | Đóng băng Watchdog khi Debug Breakpoint. |

---

## 2.2. Bảng Tra cứu Thanh ghi `IWDG` & Mã Khóa An Toàn (`IWDG_KR`)

Tra cứu RM0385 *Chapter 25: Independent watchdog $\rightarrow$ Section 25.4: IWDG registers*:

| Thanh ghi | Offset | Reset Value | Bit / Trường | Access | Ý nghĩa Kỹ thuật Phần cứng |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`IWDG_KR`** | `0x00` | `0x0000 0000` | `KEY[15:0]` | `w` | **Thanh ghi Khóa An toàn:**<br>• Ghi `0x5555`: Cho phép sửa `PR` và `RLR`.<br>• Ghi `0xAAAA`: Nạp lại giá trị `RLR` vào bộ đếm (Kick Dog).<br>• Ghi `0xCCCC`: Kích hoạt khởi động IWDG (Không thể tắt). |
| **`IWDG_PR`** | `0x04` | `0x0000 0000` | `PR[2:0]` | `RW` | Bộ chia Prescaler: `000`b=/4, `001`b=/8, `010`b=/16, `011`b=/32, `100`b=/64, `101`b=/128, `110`b=/256. |
| **`IWDG_RLR`**| `0x08` | `0x0000 0FFF` | `RL[11:0]` | `RW` | Giá trị nạp lại ban đầu (12-bit, tối đa 4095). |
| **`IWDG_SR`** | `0x0C` | `0x0000 0000` | `PVU` (Bit 0) | `RO` | Prescaler Value Update: Đợi bit $= 0$ trước khi đổi PR. |
| | | | `RVU` (Bit 1) | `RO` | Reload Value Update: Đợi bit $= 0$ trước khi đổi RLR. |

---

## 2.3. Bảng Tra cứu Thanh ghi Đóng Băng Debugger (`DBGMCU_APB1_FZ`)

Khi dùng ST-Link đặt Breakpoint dừng CPU, xung nhịp LSI vẫn tiếp tục chạy khiến IWDG đếm lùi và dập Reset chip. Để gỡ lỗi bình thường, bắt buộc phải cấu hình thanh ghi **`DBGMCU_APB1_FZ`**:

| Thanh ghi | Địa chỉ | Bit | Tên Bit | Access | Mô tả cấu hình |
| :--- | :---: | :---: | :--- | :---: | :--- |
| **`DBGMCU_APB1_FZ`** | `0xE004 2008` | 12 | `DBG_IWDG_STOP` | `RW` | Ghi `1`: Đóng băng bộ đếm IWDG khi lõi CPU bị dừng bởi Debugger! |
| | | 11 | `DBG_WWDG_STOP` | `RW` | Ghi `1`: Đóng băng bộ đếm WWDG khi lõi CPU bị dừng bởi Debugger! |

---

# 💻 BƯỚC 3: GÕ CODE & MỔ XẺ BUG PHẦN CỨNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 6

```text
drivers/
├── inc/
│   └── watchdog.h     <-- Khai báo API IWDG, WWDG và CSS Failover
└── src/
    └── watchdog.c     <-- Triển khai driver an toàn, mở khóa Key & NMI Handler
src/
└── main.c             <-- Vòng lặp giám sát, Feed Dog định kỳ và Test Failover
```

---

### 📂 KHỐI 1: FILE HEADER AN TOÀN HỆ THỐNG [ `drivers/inc/watchdog.h` ]

#### TODO 1 [File: `drivers/inc/watchdog.h`]: Khai báo API Watchdog & Giám Sát Xung Nhịp
```c
#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>

/**
 * @brief Khởi tạo Independent Watchdog (IWDG) với thời gian Timeout chỉ định
 * @param timeout_ms Thời gian hết hạn tính bằng mili-giây (Ví dụ: 2000 cho 2s)
 */
void IWDG_Config(uint32_t timeout_ms);

/**
 * @brief Nạp lại bộ đếm IWDG (Đá chó định kỳ - Feed Watchdog)
 */
void IWDG_Refresh(void);

/**
 * @brief Kích hoạt mạch giám sát an toàn xung nhịp Clock Security System (CSS)
 */
void System_CSS_Enable(void);

#endif /* WATCHDOG_H */
```

---

### 📂 KHỐI 2: FILE SOURCE DRIVER AN TOÀN [ `drivers/src/watchdog.c` ]

#### TODO 2 [File: `drivers/src/watchdog.c`]: Khởi Tạo IWDG Chuẩn Xác Từng Bước
```c
#include "watchdog.h"
#include "Reg.h"

#define IWDG_KEY_RELOAD     0xAAAAU
#define IWDG_KEY_ENABLE     0xCCCCU
#define IWDG_KEY_WRITE_ACCESS 0x5555U

void IWDG_Config(uint32_t timeout_ms)
{
    /* BƯỚC 1: ĐÓNG BĂNG WATCHDOG KHI DEBUGGER BREAKPOINT (RẤT QUAN TRỌNG) */
    /* Tránh việc MCU bị reset liên tục khi lập trình viên đang dừng debug code */
    DBGMCU->APB1FZ |= (DBGMCU_APB1_FZ_DBG_IWDG_STOP | DBGMCU_APB1_FZ_DBG_WWDG_STOP);

    /* BƯỚC 2: Kích hoạt bộ đếm IWDG bằng Key 0xCCCC */
    IWDG->KR = IWDG_KEY_ENABLE;

    /* BƯỚC 3: Mở khóa quyền ghi vào thanh ghi PR và RLR bằng Key 0x5555 */
    IWDG->KR = IWDG_KEY_WRITE_ACCESS;

    /* BƯỚC 4: Chờ cờ PVU và RVU về 0 (Xác nhận mạch sẵn sàng nhận giá trị mới) */
    while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU));

    /* BƯỚC 5: Thiết lập bộ chia Prescaler = /64 (Mã 010b = 0x4) */
    /* Với LSI ~ 32kHz: f_tick = 32000 / 64 = 500Hz -> Mỗi tick = 2ms */
    IWDG->PR = (0x4U << 0);

    /* BƯỚC 6: Tính toán và nạp giá trị RLR (Tối đa 4095) */
    uint32_t reload_val = (timeout_ms / 2U);
    if (reload_val > 4095U) {
        reload_val = 4095U;
    }
    IWDG->RLR = reload_val;

    /* BƯỚC 7: Nạp lại bộ đếm lần đầu và khóa quyền ghi */
    while (IWDG->SR & IWDG_SR_RVU);
    IWDG->KR = IWDG_KEY_RELOAD;
}

void IWDG_Refresh(void)
{
    /* Ghi trực tiếp mã 0xAAAA vào thanh ghi KR để nạp lại RLR */
    IWDG->KR = IWDG_KEY_RELOAD;
}
```

#### TODO 3 [File: `drivers/src/watchdog.c`]: Kích Hoạt CSS & Xử Lý Ngắt Bất Khả Kháng NMI
```c
void System_CSS_Enable(void)
{
    /* Bật tính năng Clock Security System trên thạch anh ngoài HSE */
    RCC->CR |= RCC_CR_CSSON;
}

/**
 * @brief Trình phục vụ ngắt bất khả kháng NMI (Non-Maskable Interrupt)
 *        Tự động nhảy vào đây khi mất thạch anh ngoài HSE!
 */
void NMI_Handler(void)
{
    /* Kiểm tra xem ngắt NMI có phải do Clock Security System kích hoạt không */
    if (RCC->CIR & RCC_CIR_CSSF) {
        /* 1. Xóa cờ ngắt CSS bằng cách ghi 1 vào bit CSSC (W1C) */
        RCC->CIR |= RCC_CIR_CSSC;

        /* 2. Lúc này phần cứng ĐÃ TỰ ĐỘNG CHUYỂN SYSCLK sang HSI 16MHz */
        
        /* 3. Thực hiện hành vi an toàn: Cắt tải, cảnh báo lỗi nguồn xung */
        /* Đèn LED nhấp nháy báo động sự cố hỏng thạch anh ngoài */
        while (1) {
            /* Hệ thống chạy ở chế độ dự phòng an toàn (Fail-Safe Mode) */
        }
    }
}
```

---

### 📂 KHỐI 3: TÍCH HỢP VÒNG LẶP HỆ THỐNG [ `src/main.c` ]

#### TODO 4 [File: `src/main.c`]: Vòng Lặp Chính Kick Dog Định Kỳ & Kiểm Tra Khởi Động
```c
#include "Sys_Clock.h"
#include "watchdog.h"
#include "can.h"
#include "uart_dma.h"

int main(void)
{
    /* 1. Khởi tạo xung nhịp hệ thống 216MHz */
    System_Clock_Init();

    /* 2. Bật mạch giám sát thạch anh ngoài CSS */
    System_CSS_Enable();

    /* 3. Kiểm tra Hộp đen Reset Reason (Từ Ngày 1) xem có phải do Watchdog cắn không */
    SystemResetReason_t reset_reason = System_GetResetReason();
    if (reset_reason == RESET_REASON_IWDG) {
        /* Lần boot trước bị treo code khiến Watchdog reset -> Ghi log cảnh báo! */
    }

    /* 4. Khởi tạo mạng CAN, UART và các ngoại vi */
    CAN_Init();
    UART_DMA_Init();

    /* 5. KÍCH HOẠT IWDG VỚI TIMEOUT 2.0 GIÂY (Sau khi các ngoại vi đã init xong) */
    IWDG_Config(2000);

    while (1) {
        /* Xử lý các tác vụ truyền thông thời gian thực */
        
        /* ĐÁ CHÓ ĐỊNH KỲ: Phải đảm bảo chu kỳ lặp < 2000ms */
        IWDG_Refresh();
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug Phần Cứng "Kinh Điển" trong Ngày 6

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY PHẦN CỨNG WATCHDOG & RELIABILITY                          │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN CỨNG             │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Không đổi được │ Quên ghi Key `0x5555` vào `IWDG_KR` trước │ Phải nạp Key `0x5555` và chờ    │
│    Prescaler/RLR  │ hoặc không chờ cờ `PVU`/`RVU` về 0.       │ cờ `PVU=0`, `RVU=0` trong SR.   │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Cứ dừng debug  │ Khi Debugger đặt breakpoint, CPU dừng     │ Ghi `1` vào các bit tương ứng   │
│    là MCU bị reset│ nhưng LSI/IWDG vẫn đếm lùi đến hết giờ.   │ trong `DBGMCU->APB1FZ`.         │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. WWDG reset ngay│ Lập trình viên gọi hàm refresh WWDG quá   │ Chỉ refresh khi Counter tụt     │
│    lập tức lúc nạp│ sớm, khi counter vẫn cao hơn ngưỡng cửa sổ│ xuống DƯỚI giá trị Window `W`.  │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. MCU treo cứng  │ Bật IWDG TRƯỚC các hàm init ngoại vi nặng │ Chỉ bật IWDG ở CUỐI hàm main()  │
│    ngay khi boot  │ (như SDRAM init, xóa Flash) tốn > 2 giây. │ sau khi mọi thứ đã sẵn sàng.    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Không bắt được │ Lầm tưởng CSS phát ngắt thông thường qua  │ CSS nối thẳng vào `NMI_Handler`.│
│    sự cố mất HSE  │ NVIC. Không thể chặn NMI bằng lệnh tắt ngắt│ Phải viết code cứu hộ trong NMI.│
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao vi điều khiển STM32 lại có tới 2 bộ Watchdog (IWDG và WWDG)? Khi nào dùng cái nào?
* **Trả lời chuẩn Bare-metal:** 
  * **IWDG (Independent Watchdog):** Chạy bằng xung RC nội LSI 32kHz độc lập hoàn toàn. Dùng để làm **lá chắn an toàn cuối cùng** bắt lỗi CPU bị treo cứng (Deadlock, HardFault lặp vô tận, mất nguồn clock chính). Ưu tiên tính độc lập tuyệt đối, không cần độ chính xác micro-giây.
  * **WWDG (Window Watchdog):** Chạy bằng xung APB1 chính xác. Nó bắt buộc phần mềm phải refresh trong một **khoảng thời gian cửa sổ nghiêm ngặt** (không được quá muộn và KHÔNG ĐƯỢC QUÁ SỚM). Dùng để bắt lỗi **loạn luồng thực thi phần mềm** (Software Runaway / Memory Corruption nhảy cóc lệnh).

### ❓ Câu 2: Chuyện gì xảy ra nếu thạch anh ngoài HSE bị hỏng khi đang bật tính năng CSS (Clock Security System)?
* **Trả lời chuẩn Bare-metal:** 
  * Ngay khi mạch phần cứng phát hiện mất xung HSE, CSS sẽ thực hiện 3 hành động song song:
    1. Tự động chuyển đổi nguồn xung nhịp của hệ thống (SYSCLK) sang nguồn dao động nội **HSI 16MHz** để CPU không bị ngắt quãng lệnh.
    2. Vô hiệu hóa bộ nhân Main PLL (vì nguồn cấp cho PLL là HSE đã mất).
    3. Kích hoạt ngắt bất khả kháng **NMI (Non-Maskable Interrupt)**. Lõi CPU lập tức nhảy vào `NMI_Handler` để thực hiện quy trình hạ cánh an toàn (chuyển xe sang chế độ khẩn cấp, lưu trạng thái lỗi).

### ❓ Câu 3: Làm thế nào để phân biệt lần khởi động hiện tại là do Cắm nguồn (Power-on) hay do Watchdog dập Reset?
* **Trả lời chuẩn Bare-metal:**
  * Đọc thanh ghi kiểm soát trạng thái reset **`RCC->CSR`** ngay khi vừa boot:
    * Bit 27 `PORRSTF = 1`: Reset do cắm nguồn (Power-on Reset).
    * Bit 29 `IWDGRSTF = 1`: Reset do Independent Watchdog hết hạn.
    * Bit 30 `WWDGRSTF = 1`: Reset do Window Watchdog phát hiện vi phạm cửa sổ.
  * Sau khi đọc xong, bắt buộc phải ghi `1` vào bit **`RMVF` (Remove Reset Flag)** để xóa sạch cờ, sẵn sàng cho chu kỳ giám sát tiếp theo.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Trong các thiết bị Gateway ô tô đòi hỏi chuẩn tin cậy cao, em thiết lập cơ chế phòng thủ 3 tầng phần cứng trên STM32F746:  
> Tầng 1 là **Clock Security System (CSS)** giám sát thạch anh ngoài HSE 25MHz; nếu mất dao động cơ học, phần cứng tự động ép xung sang HSI 16MHz dự phòng và phát ngắt bất khả kháng NMI để cứu dữ liệu.  
> Tầng 2 là **IWDG chạy xung nội LSI 32kHz** với chu kỳ timeout 2.0 giây, đóng vai trò chốt chặn cuối cùng nếu firmware bị kẹt vòng lặp hoặc sập lõi. Em cấu hình thanh ghi `DBGMCU_APB1_FZ` để đóng băng bộ đếm IWDG khi debugger đặt breakpoint, tránh lỗi tự reset khi gỡ lỗi.  
> Tầng 3 là việc đọc và giải mã thanh ghi **`RCC->CSR`** lúc bootup để lưu vết nguyên nhân Reset vào bộ nhớ lỗi, giúp đội ngũ kỹ thuật phân biệt chính xác giữa sự cố sụt áp nguồn hay lỗi treo Watchdog."*
