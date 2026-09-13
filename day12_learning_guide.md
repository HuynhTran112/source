# 🏆 [NGÀY 12] CẨM NANG TOÀN DIỆN AUTOMOTIVE DIAGNOSTICS: GIÁM SÁT TÍN HIỆU (SIGNAL SUPERVISION, TIMEOUT DETECTION & BUS-OFF RECOVERY STATE MACHINE)
## Lộ trình 4 Bước: Tiêu Chuẩn ISO 26262 ➔ Thực Chiến Giám Sát ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ các cơ chế chẩn đoán an toàn chức năng chuẩn công nghiệp ô tô (**ISO 26262 ASIL-B & AUTOSAR E2E Protection**) trên STM32F746: Thiết kế tầng bảo vệ 3 lớp cho khung tin CAN gồm **Bộ đếm nhịp sống (Alive / Rolling Counter)**, **Mã kiểm tra tính toàn vẹn E2E CRC-8**, và **Bộ giám sát thời gian thực phát hiện mất tín hiệu (Missing Frame / Timeout Detection)** triệt tiêu lỗi hiển thị dữ liệu đóng băng (Frozen Stale Data). Đồng thời xây dựng máy trạng thái tự động phục hồi sự cố ngắt mạch mạng theo chuẩn **ISO 11898-1 Bus-Off Recovery State Machine**.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, thuật toán kiểm tra mã CRC, máy trạng thái FSM toán học, bảng tham số timeout và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 12                                    │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: TIÊU CHUẨN│ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE AN TOÀN    │ BƯỚC 4: PHỎNG VẤN          │
│ • Chuẩn ISO 26262 │ • Bảng thông số   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn An Toàn │
│ • 3 Lớp bảo vệ E2E│   Timeout & Cycle │ • TODO 1-2 [supervision.h] │   Chức Năng & Bus-Off      │
│ • Thuật toán CRC-8│ • Bảng Lookup CRC │ • TODO 3-4 [supervision.c] │ • Stale Frozen Data Trap   │
│ • Máy trạng thái  │ • Máy trạng thái  │ • TODO 5 [bus_off_sm.c]    │ • Timer Wrap-Around Trap   │
│   Bus-Off ISO     │   FSM Phục hồi    │ • TODO 6 [src/main.c]      │ • Kịch bản trả lời 60s     │
│   11898-1         │                   │ • Mổ xẻ 5 Bug an toàn      │   (Elevator Pitch)         │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC AN TOÀN AUTOMOTIVE CỐT LÕI (CHUYÊN CHO NGÀY 12)

| STT | Quy tắc An toàn Automotive | Thể hiện cụ thể trong Ngày 12 (Signal Supervision) |
| :---: | :--- | :--- |
| **1** | **Never Trust Stale Data** | **QUY TẮC SỐNG CÒN:** Không bao giờ giữ nguyên giá trị cũ trên màn hình khi đã quá hạn thời gian nhận gói tin (Timeout). Nếu sau $100\text{ms}$ không có frame mới, phải đổi trạng thái sang `INVALID` và hiển thị gạch ngang `--` kèm đèn báo lỗi! |
| **2** | **Unsigned Subtraction Rule** | Khi so sánh thời gian timeout, BẮT BUỘC dùng phép trừ số nguyên không dấu: `(uint32_t)(now_ms - last_rx_ms) >= timeout_ms`. Tuyệt đối không so sánh `now_ms >= last_rx_ms + timeout_ms` vì sẽ gây lỗi chết đứng khi biến đếm tràn số sau 49.7 ngày! |
| **3** | **Rolling Counter Sequence** | Bộ đếm nhịp sống phải tăng tuần tự $+1$ qua mỗi chu kỳ gửi ($0 \rightarrow 15 \rightarrow 0$). Nếu phát hiện bước nhảy sai hoặc đếm đứng im liên tiếp 3 chu kỳ $\implies$ Hủy bỏ frame và ghi nhận lỗi `E2E_COUNTER_ERROR`. |
| **4** | **Data ID in CRC-8** | Trong chuẩn AUTOSAR E2E Profile 1, hàm CRC-8 (Đa thức $0x1D$ hoặc $0x2F$) bắt buộc phải tính kèm giá trị bí mật **Data ID (16-bit)** của thông điệp để chống lỗi giả mạo gói tin (Masquerading fault). |
| **5** | **Bus-Off Limp-Home Delay** | Khi chuyển sang trạng thái Bus-Off, không được phục hồi ngay lập tức (tránh làm tê liệt bus nếu đang chập mạch vật lý). Bắt buộc phải chờ một khoảng thời gian trễ an toàn ($50\text{ms} - 200\text{ms}$) trước khi thử gia nhập lại mạng. |
| **6** | **Failsafe Default Values** | Khi phát hiện mất tín hiệu cảm biến (Missing Frame), hệ thống điều khiển phải lập tức nạp giá trị mặc định an toàn (Failsafe Default: ví dụ nhiệt độ gán về mức nguy hiểm cực đại để kích quạt làm mát). |
| **7** | **Static Diagnostic Logging** | Mọi sự kiện vi phạm E2E (Mất frame, sai CRC, sai Rolling Counter) phải được ghi vào bộ đếm lỗi DTC (Diagnostic Trouble Code) phục vụ máy quét OBD-II trong xưởng dịch vụ. |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ TIÊU CHUẨN ISO 26262 & CƠ CHẾ BẢO VỆ (SAFETY ARCHITECTURE)

## 1.1. Hiểm Họa Tín Hiệu Đóng Băng (Frozen Stale Data) Trong Mạng Xe Hơi

Hãy tưởng tượng một chiếc ô tô đang chạy trên đường cao tốc với tốc độ $100\text{ km/h}$. Dây CAN Bus nối từ hộp truyền động sang bảng đồng hồ táp-lô bị chuột cắn đứt:
* **Nếu không có cơ chế Supervision:** Biến `speed` trong bộ nhớ vi điều khiển vẫn giữ nguyên giá trị cuối cùng nhận được là `100`. Bảng đồng hồ tiếp tục hiển thị $100\text{ km/h}$ trong khi tài xế đã đạp phanh dừng hẳn xe $\implies$ **Tai nạn chết người thảm khốc!**
* **Cơ chế Giám sát Thời gian thực (Timeout Supervision):**
  * Thông điệp tốc độ xe được phát định kỳ mỗi **$20\text{ms}$** (Chu kỳ danh định - Nominal Period).
  * Vi điều khiển đặt một đồng hồ giám sát với ngưỡng trần **$\text{Timeout} = 100\text{ms}$** (gấp 5 lần chu kỳ gửi).
  * Nếu sau $100\text{ms}$ không có frame mới đến, phần mềm lập tức xóa giá trị `100`, chuyển sang hiển thị gạch ngang `---`, bật đèn báo động cơ check-engine màu vàng và phát còi cảnh báo người lái.

---

## 1.2. Ba Lớp Bảo Vệ Tính Toàn Vẹn Khung Tin CAN (AUTOSAR E2E Protection)

Để đạt chứng chỉ an toàn chức năng ISO 26262 ASIL-B / ASIL-D, gói tin CAN được đóng gói với 3 lớp kiểm tra độc lập:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        CẤU TRÚC FRAME CAN ĐẠT CHUẨN E2E PROTECTION                     │
├──────────────┬──────────────┬───────────────────────────────────────────┬──────────────┤
│ Byte 0       │ Byte 1 (7:4) │ Byte 1 (3:0) đến Byte 6                   │ Byte 7       │
├──────────────┼──────────────┼───────────────────────────────────────────┼──────────────┤
│ E2E CRC-8    │ ROLLING      │ DỮ LIỆU TÍN HIỆU VẬT LÝ (PAYLOAD)         │ DỰ PHÒNG     │
│ CHECKSUM     │ COUNTER (RC) │ • Tốc độ xe (Speed)                       │              │
│ (Đa thức 2F) │ (0 -> 15)    │ • Vòng tua máy (RPM)                      │              │
└──────────────┴──────────────┴───────────────────────────────────────────┴──────────────┘
```

1. **Lớp 1: Rolling Counter (4 bits):** Bắt lỗi **Gói tin bị lặp lại (Repeated Frame)** hoặc **Hộp ECU bị treo đứng (ECU Deadlock)**.
2. **Lớp 2: E2E CRC-8 Checksum:** Bắt lỗi **Nhiễu điện từ làm biến dạng bit (Bit Corruption)** mà mạch CRC phần cứng của chip CAN bỏ sót (xác suất lỗi không phát hiện của CAN CRC là $\approx 10^{-14}$, nhưng với ô tô chạy hàng tỷ dặm thì bắt buộc cần thêm tầng E2E CRC-8).
3. **Lớp 3: Timeout Supervision:** Bắt lỗi **Đứt dây dẫn, mất gói tin trên đường truyền (Missing Frame)**.

---

## 1.3. Máy Trạng Thái Phục Hồi Lỗi Bus-Off Theo Tiêu Chuẩn ISO 11898-1

Khi đường truyền CAN bị ngắn mạch xuống mát (GND) hoặc chập đôi dây, bộ đếm lỗi truyền $TEC$ của chip STM32F746 vượt quá $255$, chip sẽ ngắt toàn bộ mạch phát vật lý để không phá hỏng mạng chung (rơi vào trạng thái **`BUS-OFF`**).

Phần mềm phải quản lý quá trình phục hồi theo máy trạng thái hữu hạn (**FSM**):

```text
       ┌────────────────────────┐
       │   STATE 1: BUS_ACTIVE  │ (Hoạt động truyền nhận bình thường)
       └───────────┬────────────┘
                   │
                   ▼ (TEC > 255 -> Phần cứng dựng cờ BOFF)
       ┌────────────────────────┐
       │   STATE 2: BUS_OFF     │ (Ngắt kết nối vật lý, kích hoạt Failsafe)
       └───────────┬────────────┘
                   │
                   ▼ (Đếm thời gian trễ an toàn 100ms)
       ┌────────────────────────┐
       │  STATE 3: RECOVERING   │ (Phát lệnh can_recover() / bit ABOM)
       └───────────┬────────────┘
                   │
                   ▼ (Phần cứng theo dõi thấy 128 chuỗi 11-bit Recessive)
       ┌────────────────────────┐
       │  STATE 4: RECOVERED    │ (Tái thiết lập bộ đếm lỗi, xóa cờ cảnh báo)
       └────────────────────────┘
```

---

# 📑 BƯỚC 2: THỰC CHIẾN ĐỊNH NGHĨA THAM SỐ GIÁM SÁT (SETUP & LOOKUP)

## 2.1. Bảng Tham Số Giám Sát Tín Hiệu Táp-Lô Ô Tô

| Thông Điệp | CAN ID | Chu Kỳ Gửi (Nominal) | Ngưỡng Timeout | Số Lần Mất Tối Đa | Hành Vi Failsafe Khi Timeout |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`Engine_Telemetry`**| `0x120` | **$20\text{ ms}$** | **$100\text{ ms}$** | 5 chu kỳ | Tốc độ $\rightarrow$ `---`, RPM $\rightarrow$ `0`, Bật đèn báo động cơ. |
| **`Brake_Dynamics`** | `0x240` | **$10\text{ ms}$** | **$50\text{ ms}$** | 5 chu kỳ | Cảnh báo mất phanh ABS, kích hoạt chuông bíp an toàn. |
| **`Battery_Status`** | `0x300` | **$100\text{ ms}$** | **$500\text{ ms}$** | 5 chu kỳ | Báo lỗi nguồn ắc quy, tắt các tải phụ trợ màn hình. |

---

## 2.2. Bảng Tra Cứu Đa Thức CRC-8 AUTOSAR (Profile 1: $0x2F$)

Để tính toán CRC-8 nhanh trong vòng vài nano-giây trên Cortex-M7 mà không dùng vòng lặp dịch bit, ta sử dụng **Bảng tra cứu tĩnh (Lookup Table 256 phần tử)** được nạp sẵn trên Flash. Đa thức chuẩn:
$$P(x) = x^8 + x^5 + x^3 + x^2 + x + 1 \implies \mathbf{\text{Hex: } \texttt{0x2F}}$$

---

# 💻 BƯỚC 3: GÕ CODE AN TOÀN & MỔ XẺ BUG HỆ THỐNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 12

```text
drivers/
├── inc/
│   ├── signal_supervision.h  <-- Khai báo cấu trúc giám sát Timeout & E2E CRC
│   └── bus_off_sm.h          <-- Khai báo máy trạng thái phục hồi Bus-Off
└── src/
    ├── signal_supervision.c  <-- Thuật toán kiểm tra Rolling Counter & Timeout
    └── bus_off_sm.c          <-- Triển khai FSM phục hồi Bus-Off ISO 11898-1
src/
└── main.c                   <-- Tích hợp vòng lặp kiểm tra định kỳ 10ms
```

---

### 📂 KHỐI 1: FILE HEADER GIÁM SÁT AN TOÀN [ `drivers/inc/signal_supervision.h` ]

#### TODO 1 [File: `drivers/inc/signal_supervision.h`]: Cấu Trúc Đối Tượng Giám Sát
```c
#ifndef SIGNAL_SUPERVISION_H
#define SIGNAL_SUPERVISION_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SIGNAL_STATUS_OK = 0,
    SIGNAL_STATUS_TIMEOUT,       /* Mất gói tin quá thời gian cho phép */
    SIGNAL_STATUS_CRC_ERROR,     /* Sai mã kiểm tra E2E Checksum */
    SIGNAL_STATUS_COUNTER_ERROR  /* Sai nhịp đếm Rolling Counter */
} SignalSafetyStatus_t;

/**
 * @brief Đối tượng giám sát một thông điệp CAN thời gian thực
 */
typedef struct {
    uint32_t can_id;              /* Định danh CAN ID */
    uint32_t timeout_threshold_ms;/* Ngưỡng thời gian báo lỗi mất gói */
    uint32_t last_rx_time_ms;     /* Dấu thời gian nhận gói tin gần nhất */
    uint8_t  last_rolling_counter;/* Giá trị Rolling Counter lần trước */
    uint16_t data_id;             /* Mã bí mật Data ID trong chuẩn AUTOSAR */
    SignalSafetyStatus_t status;  /* Trạng thái an toàn hiện tại */
} CanMsgSupervisor_t;

/* Khởi tạo đối tượng giám sát */
void Supervision_Init(CanMsgSupervisor_t *sup, uint32_t id, uint32_t timeout_ms, uint16_t data_id);

/* Nạp gói tin mới và kiểm tra E2E (Gọi ngay khi vừa nhận frame) */
SignalSafetyStatus_t Supervision_FeedFrame(CanMsgSupervisor_t *sup, const uint8_t *payload, 
                                           uint8_t dlc, uint32_t current_time_ms);

/* Kiểm tra quá hạn thời gian (Gọi định kỳ trong Timer hoặc luồng Worker) */
void Supervision_PeriodicCheck(CanMsgSupervisor_t *sup, uint32_t current_time_ms);

#endif /* SIGNAL_SUPERVISION_H */
```

---

### 📂 KHỐI 2: FILE SOURCE GIẢI THUẬT AN TOÀN [ `drivers/src/signal_supervision.c` ]

#### TODO 2 [File: `drivers/src/signal_supervision.c`]: Thuật Toán Bảng Tra Cứu CRC-8 AUTOSAR
```c
#include "signal_supervision.h"

/* Bảng Lookup CRC-8 với đa thức 0x2F (AUTOSAR 8H2F Profile) */
static const uint8_t CRC8_TABLE[256] = {
    0x00, 0x2F, 0x5E, 0x71, 0xBC, 0x93, 0xE2, 0xCD, 0x57, 0x78, 0x09, 0x26, 0xEB, 0xC4, 0xB5, 0x9A,
    0xAE, 0x81, 0xF0, 0xDF, 0x12, 0x3D, 0x4C, 0x63, 0xF9, 0xD6, 0xA7, 0x88, 0x45, 0x6A, 0x1B, 0x34,
    0x73, 0x5C, 0x2D, 0x02, 0xCF, 0xE0, 0x91, 0xBE, 0x24, 0x0B, 0x7A, 0x55, 0x98, 0xB7, 0xC6, 0xE9,
    0xDD, 0xF2, 0x83, 0xAC, 0x61, 0x4E, 0x3F, 0x10, 0x8A, 0xA5, 0xD4, 0xFB, 0x36, 0x19, 0x68, 0x47,
    0xE6, 0xC9, 0xB8, 0x97, 0x5A, 0x75, 0x04, 0x2B, 0xB1, 0x9E, 0xEF, 0xC0, 0x0D, 0x22, 0x53, 0x7C,
    0x48, 0x67, 0x16, 0x39, 0xF4, 0xDB, 0xAA, 0x85, 0x1F, 0x30, 0x41, 0x6E, 0xA3, 0x8C, 0xFD, 0xD2,
    0x95, 0xBA, 0xCB, 0xE4, 0x29, 0x06, 0x77, 0x58, 0xC2, 0xED, 0x9C, 0xB3, 0x7E, 0x51, 0x20, 0x0F,
    0x3B, 0x14, 0x65, 0x4A, 0x87, 0xA8, 0xD9, 0xF6, 0x6C, 0x43, 0x32, 0x1D, 0xD0, 0xFF, 0x8E, 0xA1,
    0xE3, 0xCC, 0xBD, 0x92, 0x5F, 0x70, 0x01, 0x2E, 0xB4, 0x9B, 0xEA, 0xC5, 0x08, 0x27, 0x56, 0x79,
    0x4D, 0x62, 0x13, 0x3C, 0xF1, 0xDE, 0xAF, 0x80, 0x1A, 0x35, 0x44, 0x6B, 0xA6, 0x89, 0xF8, 0xD7,
    0x90, 0xBF, 0xCE, 0xE1, 0x2C, 0x03, 0x72, 0x5D, 0xC7, 0xE8, 0x99, 0xB6, 0x7B, 0x54, 0x25, 0x0A,
    0x3E, 0x11, 0x60, 0x4F, 0x82, 0xAD, 0xDC, 0xF3, 0x69, 0x46, 0x37, 0x18, 0xD5, 0xFA, 0x8B, 0xA4,
    0x05, 0x2A, 0x5B, 0x74, 0xB9, 0x96, 0xE7, 0xC8, 0x52, 0x7D, 0x0C, 0x23, 0xEE, 0xC1, 0xB0, 0x9F,
    0xAB, 0x84, 0xF5, 0xDA, 0x17, 0x38, 0x49, 0x66, 0xFC, 0xD3, 0xA2, 0x8D, 0x40, 0x6F, 0x1E, 0x31,
    0x76, 0x59, 0x28, 0x07, 0xCA, 0xE5, 0x94, 0xBB, 0x21, 0x0E, 0x7F, 0x50, 0x9D, 0xB2, 0xC3, 0xEC,
    0xD8, 0xF7, 0x86, 0xA9, 0x64, 0x4B, 0x3A, 0x15, 0x8F, 0xA0, 0xD1, 0xFE, 0x33, 0x1C, 0x6D, 0x42
};

static uint8_t Calculate_E2E_CRC8(const uint8_t *data, uint8_t len, uint16_t data_id)
{
    uint8_t crc = 0xFF; /* Giá trị khởi tạo chuẩn E2E */

    /* Tính toán kèm Data ID (Byte thấp rồi Byte cao) */
    crc = CRC8_TABLE[crc ^ (uint8_t)(data_id & 0xFF)];
    crc = CRC8_TABLE[crc ^ (uint8_t)((data_id >> 8) & 0xFF)];

    /* Tính toán toàn bộ payload (trừ Byte 0 chứa chính mã CRC) */
    for (uint8_t i = 1; i < len; i++) {
        crc = CRC8_TABLE[crc ^ data[i]];
    }

    return (crc ^ 0xFF); /* XOR kết quả cuối */
}
```

#### TODO 3 [File: `drivers/src/signal_supervision.c`]: Kiểm Tra Rolling Counter & Timeout Chuẩn Xác
```c
void Supervision_Init(CanMsgSupervisor_t *sup, uint32_t id, uint32_t timeout_ms, uint16_t data_id)
{
    sup->can_id = id;
    sup->timeout_threshold_ms = timeout_ms;
    sup->last_rx_time_ms = 0;
    sup->last_rolling_counter = 0xFF; /* Chưa khởi tạo */
    sup->data_id = data_id;
    sup->status = SIGNAL_STATUS_TIMEOUT; /* Ban đầu coi như chưa có dữ liệu */
}

SignalSafetyStatus_t Supervision_FeedFrame(CanMsgSupervisor_t *sup, const uint8_t *payload, 
                                           uint8_t dlc, uint32_t current_time_ms)
{
    if (dlc < 2) {
        return SIGNAL_STATUS_TIMEOUT;
    }

    /* 1. KIỂM TRA MÃ E2E CRC-8 (Byte 0) */
    uint8_t expected_crc = Calculate_E2E_CRC8(payload, dlc, sup->data_id);
    if (payload[0] != expected_crc) {
        sup->status = SIGNAL_STATUS_CRC_ERROR;
        return SIGNAL_STATUS_CRC_ERROR;
    }

    /* 2. KIỂM TRA ROLLING COUNTER (4 bits cao của Byte 1) */
    uint8_t current_rc = (payload[1] >> 4) & 0x0F;
    if (sup->last_rolling_counter != 0xFF) {
        uint8_t expected_rc = (sup->last_rolling_counter + 1U) & 0x0F;
        if (current_rc != expected_rc) {
            sup->status = SIGNAL_STATUS_COUNTER_ERROR;
            return SIGNAL_STATUS_COUNTER_ERROR;
        }
    }
    sup->last_rolling_counter = current_rc;

    /* 3. CẬP NHẬT DẤU THỜI GIAN VÀ TRẠNG THÁI HỢP LỆ */
    sup->last_rx_time_ms = current_time_ms;
    sup->status = SIGNAL_STATUS_OK;
    return SIGNAL_STATUS_OK;
}

void Supervision_PeriodicCheck(CanMsgSupervisor_t *sup, uint32_t current_time_ms)
{
    /* BẮT BUỘC: DÙNG PHÉP TRỪ KHÔNG DẤU CHỐNG LỖI TRÀN BIẾN (TIMER WRAP-AROUND) */
    uint32_t elapsed_ms = (uint32_t)(current_time_ms - sup->last_rx_time_ms);

    if (elapsed_ms >= sup->timeout_threshold_ms) {
        sup->status = SIGNAL_STATUS_TIMEOUT;
    }
}
```

---

### 📂 KHỐI 3: MÁY TRẠNG THÁI PHỤC HỒI BUS-OFF [ `drivers/src/bus_off_sm.c` ]

#### TODO 4 [File: `drivers/src/bus_off_sm.c`]: FSM Phục Hồi Lỗi Chuẩn ISO 11898-1
```c
#include "Reg.h"
#include <stdint.h>

typedef enum {
    BUS_STATE_ACTIVE = 0,
    BUS_STATE_OFF_DETECTED,
    BUS_STATE_WAIT_RECOVERY,
    BUS_STATE_RECOVERED
} BusOffFsmState_t;

static BusOffFsmState_t s_bus_fsm = BUS_STATE_ACTIVE;
static uint32_t s_bus_off_entry_time = 0;

void BusOff_FSM_Poll(uint32_t current_time_ms)
{
    /* Kiểm tra cờ Bus-Off phần cứng trong thanh ghi CAN_ESR (Bit 2 BOFF) */
    bool is_hardware_bus_off = (CAN1->ESR & (1U << 2)) != 0;

    switch (s_bus_fsm) {
    case BUS_STATE_ACTIVE:
        if (is_hardware_bus_off) {
            /* Phát hiện sự cố sập bus: Chuyển trạng thái và lưu dấu thời gian */
            s_bus_fsm = BUS_STATE_OFF_DETECTED;
            s_bus_off_entry_time = current_time_ms;
        }
        break;

    case BUS_STATE_OFF_DETECTED:
        /* Chờ 100ms thời gian trễ an toàn trước khi thử kích hoạt phục hồi */
        if ((uint32_t)(current_time_ms - s_bus_off_entry_time) >= 100U) {
            /* Bật bit ABOM (Automatic Bus-Off Management) để phần cứng theo dõi 128 chuỗi 11-bit */
            CAN1->MCR |= CAN_MCR_ABOM;
            s_bus_fsm = BUS_STATE_WAIT_RECOVERY;
        }
        break;

    case BUS_STATE_WAIT_RECOVERY:
        if (!is_hardware_bus_off) {
            /* Cờ BOFF đã tự động xóa về 0: Mạng đã phục hồi thành công! */
            s_bus_fsm = BUS_STATE_ACTIVE;
        }
        break;

    default:
        s_bus_fsm = BUS_STATE_ACTIVE;
        break;
    }
}
```

---

### 📂 KHỐI 4: VÒNG LẶP KIỂM CHỨNG HỆ THỐNG [ `src/main.c` ]

#### TODO 5 [File: `src/main.c`]: Tích Hợp Giám Sát Và Kích Hoạt Đèn Báo Lỗi Táp-Lô
```c
#include <stdio.h>
#include "signal_supervision.h"
#include "dbc_decoder.h"

static CanMsgSupervisor_t s_sup_engine;

int main(void)
{
    /* Khởi tạo đối tượng giám sát: ID 0x120, Timeout 100ms, Data ID 0x1234 */
    Supervision_Init(&s_sup_engine, 0x120, 100, 0x1234);

    uint32_t simulated_clock_ms = 0;

    while (1) {
        simulated_clock_ms += 10; /* Giả lập bước thời gian mỗi 10ms */

        /* Kiểm tra định kỳ xem tín hiệu có bị chết đứng hay đứt dây không */
        Supervision_PeriodicCheck(&s_sup_engine, simulated_clock_ms);

        if (s_sup_engine.status == SIGNAL_STATUS_TIMEOUT) {
            /* KÍCH HOẠT HÀNH VI AN TOÀN FAILSAFE:
             * Hiển thị dấu gạch ngang '---' trên táp-lô và bật đèn cảnh báo */
        } else if (s_sup_engine.status == SIGNAL_STATUS_OK) {
            /* Dữ liệu tươi mới: Cho phép hiển thị số km/h bình thường */
        }
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug An Toàn "Kinh Điển" trong Ngày 12

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG AN TOÀN CHỨC NĂNG AUTOMOTIVE                     │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN MỀM / PHẦN CỨNG   │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Táp-lô hiển thị│ Không có cơ chế Timeout Supervision, biến │ Luôn có đồng hồ đếm lùi Timeout,│
│    vận tốc đông đá│ lưu giá trị cũ vô hạn khi đứt cáp CAN.    │ gán về `INVALID` sau 100ms.     │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Lỗi Timer sau  │ So sánh `now >= last + timeout` bị tràn   │ Luôn dùng phép trừ không dấu    │
│    49.7 ngày chạy │ số nguyên 32-bit làm đơ toàn bộ giám sát. │ `(uint32_t)(now - last) >= T`.  │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Sai mã E2E CRC │ Quên đưa mã bí mật `Data ID` vào hàm tính │ Tính toán đủ Data ID theo chuẩn │
│    dù payload đúng│ CRC-8 theo tiêu chuẩn AUTOSAR Profile 1.  │ AUTOSAR E2E Profile 1.          │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Báo lỗi Counter│ ECU đối phương vừa khởi động lại Reset    │ Cho phép chấp nhận mọi giá trị  │
│    khi xe vừa đề  │ bộ đếm về 0, code bắt lỗi sai nhịp đếm.   │ khi `last_counter == 0xFF`.     │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Bus-Off lặp vô │ Thử phục hồi ngay lập tức lúc đường dây   │ Thêm độ trễ an toàn 100ms và    │
│    tận liên tục   │ vẫn đang chập điện, làm nghẽn bus thêm.   │ giới hạn số lần thử lại tối đa. │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao phép so sánh `(uint32_t)(now_ms - last_rx_ms) >= timeout_ms` luôn hoạt động đúng kể cả khi biến đếm thời gian bị tràn số (Wrap-Around)?
* **Trả lời chuẩn Kỹ sư Automotive:** 
  * Biến `uint32_t` trên vi điều khiển 32-bit có giá trị cực đại là $2^{32} - 1 = 4,294,967,295\text{ ms}$ (tương đương khoảng $49.71\text{ ngày}$). Khi chạy qua mốc này, giá trị sẽ tự động quay vòng về $0$.
  * Giả sử: `last_rx_ms` ghi nhận ở thời điểm sát mép: `0xFFFFFFF0` ($4,294,967,280$). Sau đó $20\text{ms}$, biến `now_ms` đã tràn số và có giá trị mới là `0x00000004`.
  * Nếu dùng phép cộng: `last_rx_ms + 100` sẽ bị tràn số và dẫn đến so sánh sai logic.
  * Khi dùng **phép trừ số nguyên không dấu 32-bit**:
    $$0\text{x}00000004 - 0\text{xFFFFFFF0} = \mathbf{20\text{ (Hợp số bù hai tự động triệt tiêu phần tràn!)}}$$
  * Vì vậy, phép trừ không dấu luôn phản ánh khoảng cách thời gian trôi qua một cách chính xác tuyệt đối mà không cần bất kỳ lệnh `if` kiểm tra tràn số nào.

### ❓ Câu 2: Tiêu chuẩn AUTOSAR E2E (End-to-End) Protection giải quyết những mối nguy hại (Communication Faults) nào?
* **Trả lời chuẩn Kỹ sư Automotive:** 
  * AUTOSAR E2E giải quyết 6 lỗi kinh điển trong mạng truyền thông xe hơi:
    1. **Mất gói tin (Loss of Communication):** Phát hiện qua Timeout Supervision.
    2. **Gói tin lặp lại (Repetition of Information):** Bắt qua Rolling Counter đứng im.
    3. **Sai thứ tự gói tin (Incorrect Sequence):** Bắt qua Rolling Counter nhảy cóc.
    4. **Dữ liệu bị biến dạng (Data Corruption):** Bắt qua mã E2E CRC-8.
    5. **Giả mạo nguồn phát (Masquerading):** Bắt qua việc chèn `Data ID` bí mật vào CRC.
    6. **Trễ hạn chót thời gian thực (Timing Jitter):** Bắt qua bộ đếm chu kỳ danh định.

### ❓ Câu 3: Khi phát hiện tín hiệu tốc độ xe bị Missing Timeout, táp-lô ô tô nên hiển thị số 0 hay gạch ngang `--`?
* **Trả lời chuẩn Kỹ sư Automotive:**
  * **Tuyệt đối không bao giờ hiển thị số `0`!**
  * Trong kỹ thuật an toàn ô tô (Functional Safety), số `0` là một **giá trị vật lý hợp lệ** (xe đang dừng bánh). Nếu xe đang lao dốc với tốc độ $80\text{ km/h}$ mà dây CAN đứt, việc táp-lô hiển thị số `0` sẽ khiến tài xế lầm tưởng xe đã dừng hoặc đồng hồ bị lag, dẫn đến thao tác nhả phanh gây tử nạn.
  * **Nguyên tắc Failsafe:** Phải chuyển hiển thị sang trạng thái bất định (Dấu gạch ngang `---`), đổi màu chữ sang màu đỏ hoặc vàng cam, đồng thời kích hoạt đèn cảnh báo nguy hiểm trên bảng điều khiển để người lái lập tức chủ động tấp xe vào lề.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 12, em nâng cấp thiết bị CAN Gateway đạt các tiêu chí an toàn chức năng nghiêm ngặt theo chuẩn **ISO 26262 ASIL-B** và **AUTOSAR E2E Protection**.  
> Em thiết kế kiến trúc phòng thủ 3 tầng gồm: Thuật toán tra cứu nhanh **E2E CRC-8** với đa thức chuẩn $0x2F$ có tích hợp Data ID chống làm giả gói tin, bộ giám sát nhịp sống **Rolling Counter (4-bit)** chống lặp thông điệp, và hệ thống **Timeout Supervision** phát hiện đứt cáp theo chu kỳ thời gian thực.  
> Để phần mềm hoạt động bền bỉ trong môi trường công nghiệp ô tô nhiều năm không bị treo, em áp dụng quy tắc trừ số nguyên không dấu **Unsigned Subtraction** loại bỏ hoàn toàn bẫy tràn số Timer 49.7 ngày. Cuối cùng, em xây dựng máy trạng thái **Bus-Off Recovery FSM** theo tiêu chuẩn **ISO 11898-1**, giúp Gateway tự động cô lập sự cố khi chập mạch vật lý và tái hòa nhập mạng an toàn khi đường truyền phục hồi."*
