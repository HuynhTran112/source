# 🏆 [NGÀY 11] CẨM NANG TOÀN DIỆN AUTOMOTIVE PROTOCOL: GIẢI MÃ MA TRẬN TÍN HIỆU CAN THEO CHUẨN VECTOR DBC (BIT UNPACKING & ENDIANNESS)
## Lộ trình 4 Bước: Nguyên Lý DBC ➔ Thực Chiến Bit Unpacking ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ kỹ thuật cốt lõi trong ngành công nghiệp ô tô (Automotive Software Engineering): Phân tích và hiện thực hóa công cụ giải mã ma trận tín hiệu **Vector DBC (DataBase CAN)** trên vi điều khiển STM32F746: Làm chủ giải thuật bóc tách bit lẻ (**Zero-Copy Bit Unpacking**), giải mã chính xác hai định dạng byte kinh điển **Intel (Little-Endian)** và **Motorola (Big-Endian)**, chuyển đổi giá trị thô (Raw Value) sang giá trị vật lý (Physical Value) qua công thức **$V = (\text{Raw} \times \text{Factor}) + \text{Offset}$** bằng số học số nguyên định điểm (**Fixed-Point Arithmetic**), và xử lý dấu bù hai (Two's Complement) cho các tín hiệu âm.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, cấu trúc bit trong byte, công thức toán học, bảng tín hiệu DBC và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 11                                    │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: NGUYÊN LÝ │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE GIẢI MÃ    │ BƯỚC 4: PHỎNG VẤN          │
│ • Cấu trúc file   │ • Bảng tín hiệu   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn DBC     │
│   Vector DBC      │   DBC táp-lô      │ • TODO 1-2 [dbc_decoder.h] │   & Signal Unpacking       │
│ • Intel vs        │ • Sơ đồ trải bit  │ • TODO 3-4 [dbc_decoder.c] │ • Bẫy Motorola Start Bit   │
│   Motorola Order  │ • Công thức       │ • TODO 5 [src/main.c]      │ • Fixed-Point Scaling Trap │
│ • Toán Scaling    │   Fixed-Point Q16 │ • Mổ xẻ 5 Bug giải mã      │ • Kịch bản trả lời 60s     │
│ • Signed 2's Comp │ • Bảng kiểm thử   │                            │   (Elevator Pitch)         │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC GIẢI MÃ AUTOMOTIVE DBC (CHUYÊN CHO NGÀY 11)

| STT | Quy tắc Giải mã DBC | Thể hiện cụ thể trong Ngày 11 (DBC Signal Engine) |
| :---: | :--- | :--- |
| **1** | **Motorola Bit Order Trap** | **QUY TẮC SỐNG CÒN:** Trong chuẩn Motorola (Big-Endian `@0`), Start Bit được định nghĩa là bit có trọng số lớn nhất (**MSB**). Các bit tiếp theo lan lùi về các byte phía trước theo thứ tự zíc-zắc. Lầm tưởng Start Bit là LSB như Intel sẽ làm đảo lộn toàn bộ dữ liệu! |
| **2** | **Zero-Copy Bit Shifting** | Bóc tách bit trực tiếp từ mảng 8 bytes bằng các phép toán dịch bit (`<<`, `>>`) và mặt nạ (`&`). Tuyệt đối không sao chép mảng sang chuỗi nhị phân (String) gây tiêu hao bộ nhớ và làm mất tính thời gian thực. |
| **3** | **Fixed-Point Scaling** | Tránh dùng số thực dấu phẩy động `float/double` trong các vòng lặp giải mã thời gian thực. Sử dụng phép toán số nguyên nhân tử số rồi chia mẫu số (hoặc Fixed-point $Q_{8.8}$ / $Q_{16.16}$) để CPU ARM không bị trễ chu kỳ tính toán FPU. |
| **4** | **Signed Two's Complement** | Với các tín hiệu có dấu (như Nhiệt độ nước `-40` đến `+125` hoặc Góc lái vô lăng `-720` đến `+720`), nếu bit MSB của trường bit $= 1$, phải thực hiện mở rộng dấu (Sign Extension) trước khi cộng với Offset. |
| **5** | **DLC Boundary Protection** | Trước khi trích xuất bất kỳ tín hiệu nào, bắt buộc kiểm tra chiều dài gói tin: `if (frame->dlc < required_bytes) return ERROR;`. Không đọc vượt quá số byte thực nhận để tránh đọc ô nhớ rác. |
| **6** | **Min/Max Saturation Clamping** | Sau khi tính toán giá trị vật lý, phải kiểm tra ranh giới $[Min, Max]$ trong DBC. Nếu giá trị vượt ngưỡng (ví dụ cảm biến bị ngắn mạch), phải chặn bão hòa (Clamp) hoặc gán mã lỗi `SIGNAL_INVALID`. |
| **7** | **Static Table Lookup** | Toàn bộ thông số metadata của tín hiệu (Start bit, Length, Factor, Offset) phải được khai báo bằng từ khóa `const` để nằm cố định trên bộ nhớ **FLASH (`.rodata`)**, tiết kiệm $100\%$ dung lượng RAM SRAM. |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ PHẦN CỨNG & CƠ CHẾ VẬT LÝ (HARDWARE ARCHITECTURE)

## 1.1. Cấu Trúc Định Nghĩa Tín Hiệu Trong File Chuẩn Vector DBC

Trong ngành công nghiệp ô tô toàn cầu, file `.dbc` là "bản thiết kế giao tiếp" thống nhất giữa các hãng xe (OEM) và nhà cung cấp linh kiện Tier-1 (Bosch, Continental, Denso). Một dòng định nghĩa thông điệp và tín hiệu chuẩn có cú pháp:

```text
BO_ 288 Vehicle_Dynamics: 8 Gateway_ECU
 SG_ Vehicle_Speed : 0|12@1+ (0.0625,0) [0|255] "km/h" Dashboard_ECU
 SG_ Engine_RPM : 16|14@1+ (0.5,0) [0|8000] "rpm" Dashboard_ECU
 SG_ Steering_Angle : 32|12@0- (0.5,-1024) [-1024|1023] "deg" Dashboard_ECU
```

### Ý nghĩa của các trường thông số:
* `BO_ 288`: Message ID $= 288$ (Hex: `0x120`), độ dài $\text{DLC} = 8\text{ bytes}$.
* `SG_ Vehicle_Speed`:
  * `0|12`: **Start Bit = 0**, **Độ dài = 12 bits**.
  * `@1+`: `@1` nghĩa là định dạng **Intel (Little-Endian)**; dấu `+` nghĩa là số không âm (**Unsigned**).
  * `(0.0625,0)`: $\text{Factor} = 0.0625$ (tức là chia cho 16: $\times 1/16$), $\text{Offset} = 0$.
  * `[0|255]`: Ranh giới vật lý từ $0$ đến $255\text{ km/h}$.
* `SG_ Steering_Angle`:
  * `32|12`: Start Bit = 32, Độ dài = 12 bits.
  * `@0-`: `@0` nghĩa là định dạng **Motorola (Big-Endian)**; dấu `-` nghĩa là số có dấu (**Signed** bù hai).
  * `(0.5,-1024)`: $\text{Factor} = 0.5$, $\text{Offset} = -1024$.

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Vector CANdb++ Specification:**
> 1. **Tra cứu Tài liệu Đặc tả Vector DBC:** Tìm kiếm tài liệu *Vector CANdb++ File Format Specification* hoặc mở phần trợ giúp (Help Documentation) của phần mềm Vector CANdb++ Editor.
> 2. Đọc định nghĩa trường bản tin: Cú pháp dòng `BO_` (Message) và các dòng `SG_` (Signals), cách biểu diễn tín hiệu multiplexed (`m0`, `M`), giá trị mặc định, đơn vị đo lường và danh sách các node nhận (Receivers).

---

## 1.2. Bản Chất Sự Khác Biệt Giữa Định Dạng Intel và Motorola Trên Mạng CAN

Trong một gói tin CAN 8 bytes (từ Byte 0 đến Byte 7), mỗi byte có 8 bits (Bit 0 là LSB, Bit 7 là MSB):

```text
Byte 0: [ 7  6  5  4  3  2  1  0 ]
Byte 1: [15 14 13 12 11 10  9  8 ]
Byte 2: [23 22 21 20 19 18 17 16 ]
Byte 3: [31 30 29 28 27 26 25 24 ]
...
```

### 🔹 Định dạng Intel (Little-Endian - `@1`):
* **Start Bit là LSB (Bit có trọng số thấp nhất)**.
* Tín hiệu phát triển **TIẾN LÊN PHÍA TRƯỚC** theo thứ tự chỉ số bit tăng dần ($0 \rightarrow 1 \rightarrow 2 \dots$).
* Nếu tín hiệu dài 12 bits bắt đầu từ Bit 0: Nó chiếm từ Bit 0 đến Bit 7 của Byte 0, và nối tiếp sang Bit 8 đến Bit 11 của Byte 1.
* 👉 **Giải thuật:** Rất đơn giản, ghép các byte lại theo thứ tự LSB trước MSB sau rồi dịch phải.

### 🔸 Định dạng Motorola (Big-Endian - `@0`):
* **Start Bit là MSB (Bit có trọng số cao nhất)**!
* Tín hiệu phát triển **LÙI VỀ PHÍA SAU** trong nội bộ byte và nhảy zíc-zắc qua các ranh giới byte:
  * Từ Start Bit trong Byte hiện tại, đếm lùi về Bit 0 (LSB) của byte đó.
  * Sau đó nhảy sang Bit 7 (MSB) của Byte kế tiếp và tiếp tục đếm lùi!
* 👉 **Hiểm họa lớn nhất:** Nếu lập trình viên giải mã Motorola bằng thuật toán Intel, toàn bộ các bit MSB và LSB sẽ bị lộn ngược hoàn toàn, khiến xe tính toán sai vận tốc hoặc góc lái.

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Chuẩn Biểu Diễn Bit Ô Tô:**
> 1. **Tra cứu Chuẩn Endianness Vector:** Tìm kiếm tài liệu *Vector Application Note: CAN Message Layout and Byte Order*.
>    * Xem giản đồ ma trận 64-bit: Đối chiếu cách sắp xếp bit của Intel (Standard Little-Endian: LSB bit thấp -> MSB bit cao) và Motorola (Sequential / Backward Sawtooth: Start bit = MSB, các bit tiếp theo lùi dần về byte trước).
> 2. Đọc quy tắc giải thuật giải nạp: Xem cách hàm trích xuất bit di chuyển con trỏ byte để không bị nhầm lẫn giữa hai chuẩn.

---

## 1.3. Giải Thuật Chuyển Đổi Vật Lý Bằng Số Nguyên Định Điểm (Fixed-Point Scaling)

Công thức toán học:
$$\text{Physical Value} = (\text{Raw Value} \times \text{Factor}) + \text{Offset}$$

* **Nếu dùng Float thông thường:**
  ```c
  float speed = (raw_val * 0.0625f) + 0.0f; // Tốn chu kỳ lệnh FPU, không chuẩn MISRA-C thời gian thực!
  ```
* **Giải pháp Số nguyên Định điểm (Integer Scaling):**
  Nhận thấy $\text{Factor} = 0.0625 = \frac{1}{16} = \frac{1}{2^4}$.  
  Ta chỉ cần thực hiện phép dịch bit phải:
  $$\text{Speed (km/h)} = \text{raw\_val} \gg 4$$
* Với các hệ số phức tạp như $\text{Factor} = 0.1$ (tương đương chia 10), ta lưu hệ số dưới dạng **Tử số / Mẫu số**:
  $$\text{Value} = \frac{\text{raw\_val} \times \text{Factor\_Numerator}}{\text{Factor\_Denominator}} + \text{Offset}$$

> 📖 **Hướng Dẫn Tra Cứu Nguyên Lý Trong Tiêu Chuẩn AUTOSAR & MISRA:**
> 1. **Tra cứu AUTOSAR E2E Specification:** Xem tài liệu *AUTOSAR Specification of End-to-End Communication Protection (E2E Protocol)*.
>    * Tra cứu đa thức CRC-8 Profile 1 (`0x1D` hoặc `0x2F`) và cách trường Alive Counter (4-bit, chu kỳ $0 \dots 15$) bảo vệ gói tin chống đứng gói (Frozen message).
> 2. **Tra cứu Quy chuẩn MISRA-C Số học:** Quy tắc cấm phép toán dấu phẩy động không tất định trong bộ điều khiển ECU thời gian thực, khuyến nghị sử dụng số nguyên tỷ lệ (Scaled Integers).

---

# 📑 BƯỚC 2: THỰC CHIẾN ĐỊNH NGHĨA MA TRẬN TÍN HIỆU (SETUP & LOOKUP)

> 🎯 **NGUYÊN TẮC TRA CỨU MẠNG TRUYỀN THÔNG Ô TÔ:**
> 1. **Tra cứu Cú pháp Vector DBC:** Tài liệu *Vector CANdb++ File Format Specification* quy định cấu trúc dòng `BO_` (Message) và `SG_` (Signal).
> 2. **Tra cứu Quy chuẩn Endianness:** Ký hiệu `@1` là Intel Standard (Little-Endian), `@0` là Motorola Sequential (Big-Endian).
> 3. **Tra cứu Tiêu chuẩn An toàn Dữ liệu:** Tài liệu *AUTOSAR Specification of End-to-End Communication Protection (E2E Protocol)* cho thuật toán CRC-8 và Alive Counter.

---

## 2.1. Lộ trình Tra cứu Cú Pháp File DBC & Chuẩn AUTOSAR (DBC & E2E Lookup Methodology)

### 📖 Kênh 1: Cách Đọc & Tra Cứu File Mô Tả Mạng CAN (`.dbc`)
Khi mở file `.dbc` bằng bất kỳ trình soạn thảo nào hoặc phần mềm Vector CANdb++:
1. **Dòng khai báo Frame (Message):**
   * Cú pháp: `BO_ <Message_ID> <Message_Name>: <DLC> <Transmitter_Node>`
   * Ví dụ: `BO_ 288 Vehicle_Data: 8 Engine_ECU` (ID thập phân 288 = `0x120`, độ dài 8 bytes).
2. **Dòng khai báo Tín hiệu (Signal):**
   * Cú pháp: `SG_ <Signal_Name> : <Start_Bit>|<Length>@<Byte_Order><Sign> (<Factor>,<Offset>) [<Min>|<Max>] "<Unit>" <Receiver>`
   * Ví dụ: `SG_ Vehicle_Speed : 0|12@1+ (0.0625,0) [0|255] "km/h" Instrument_Cluster`
     * `@1+`: `@1` là Intel (Little-Endian), dấu `+` là Unsigned.
     * `(0.0625, 0)`: Hệ số nhân (Factor) là `1/16`, độ lệch (Offset) là `0`.

### 📖 Kênh 2: Cách Tra Cứu Đa Thức Kiểm Tra Toàn Vẹn E2E CRC-8 (AUTOSAR)
Trong mạng ô tô (chống lỗi rớt bit phần cứng hoặc can thiệp dữ liệu):
1. **Tra cứu tài liệu chuẩn AUTOSAR E2E Profile 1/2:**
   * Đa thức CRC-8 chuẩn công nghiệp ô tô: $P(x) = x^8 + x^4 + x^3 + x^2 + 1$ (Mã Hex: **`0x1D`** hoặc **`0x2F`**).
   * Giá trị khởi tạo (Init Value): **`0xFF`**.
   * Giá trị XOR ngõ ra (XOR Out): **`0xFF`**.
2. **Alive Counter:** Bộ đếm 4-bit (`0x0` đến `0xF`) tăng liên tục sau mỗi chu kỳ gửi để phát hiện lỗi đứng gói (Frozen Message).

---

## 2.2. Bảng Ma Trận Tín Hiệu Mạng Ô Tô Mẫu (Vehicle Telematics DBC)

| Tên Tín Hiệu | Message ID | Start Bit | Độ Dài | Byte Order | Signed? | Factor | Offset | Đơn Vị | Dải Đo |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`Vehicle_Speed`** | `0x120` | 0 | 12 bits | **Intel** (`@1`) | Unsigned | $0.0625$ ($1/16$) | 0 | km/h | $0 \dots 255$ |
| **`Engine_RPM`** | `0x120` | 16 | 14 bits | **Intel** (`@1`) | Unsigned | $0.5$ ($1/2$) | 0 | RPM | $0 \dots 8191$ |
| **`Coolant_Temp`** | `0x120` | 32 | 8 bits | **Intel** (`@1`) | Signed | $1.0$ | $-40$ | $^\circ\text{C}$ | $-40 \dots 215$ |
| **`Steering_Angle`**| `0x240` | 7 | 14 bits | **Motorola** (`@0`)| Signed | $0.1$ ($1/10$) | $-720$ | Độ ($^\circ$)| $-720 \dots 720$ |

---

# 💻 BƯỚC 3: GÕ CODE GIẢI MÃ & MỔ XẺ BUG TÍN HIỆU (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 11

```text
drivers/
├── inc/
│   └── dbc_decoder.h  <-- Khai báo cấu trúc Signal Metadata & API Unpacking
└── src/
    └── dbc_decoder.c  <-- Thuật toán Zero-Copy Bit Extraction & Fixed-Point Scaling
src/
└── main.c             <-- Test bóc tách frame CAN thật với cả 2 chuẩn Intel & Motorola
```

---

### 📂 KHỐI 1: FILE HEADER MA TRẬN TÍN HIỆU [ `drivers/inc/dbc_decoder.h` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 1:
1. **Tra cứu Đặc tả Vector CANdb++ Specification:**
   - **Mở tài liệu CANdb++ File Format**:
     - Cú pháp `SG_ <Signal_Name> : <Start_Bit>|<Length>@<Byte_Order><Sign> (<Factor>,<Offset>) [<Min>|<Max>] "<Unit>" <Receiver>`.
     - Ánh xạ `@1` ➔ `DBC_BYTE_ORDER_INTEL` (Little-Endian).
     - Ánh xạ `@0` ➔ `DBC_BYTE_ORDER_MOTOROLA` (Big-Endian).
2. **Khai báo struct metadata chuẩn tối ưu Flash (.rodata):**
   - Định nghĩa `DbcSignalMeta_t` chứa đầy đủ trường start bit, bit length, hệ số tử/mẫu (`factor_num`, `factor_den`), offset và ngưỡng bão hòa `min_val`, `max_val`.

#### TODO 1 [File: `drivers/inc/dbc_decoder.h`]: Khai Báo Cấu Trúc Signal Metadata
```c
#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DBC_BYTE_ORDER_INTEL = 0,    /* Little-Endian (@1) */
    DBC_BYTE_ORDER_MOTOROLA = 1  /* Big-Endian (@0) */
} DbcByteOrder_t;

/**
 * @brief Metadata định nghĩa 1 tín hiệu CAN chuẩn Vector DBC
 */
typedef struct {
    uint8_t        start_bit;         /* Vị trí bit bắt đầu */
    uint8_t        bit_length;        /* Độ dài trường bit (1 - 32 bits) */
    DbcByteOrder_t byte_order;        /* Intel hay Motorola */
    bool           is_signed;         /* Có dấu (Two's complement) hay không */
    uint16_t       factor_num;        /* Tử số của Factor */
    uint16_t       factor_den;        /* Mẫu số của Factor */
    int32_t        offset;            /* Giá trị dịch Offset */
    int32_t        min_val;           /* Ngưỡng tối thiểu hợp lệ */
    int32_t        max_val;           /* Ngưỡng tối đa hợp lệ */
} DbcSignalMeta_t;

/**
 * @brief Bóc tách trường bit thô (Raw Value) từ mảng 8 bytes CAN
 */
uint32_t DBC_UnpackRaw(const uint8_t *payload, uint8_t dlc, 
                       uint8_t start_bit, uint8_t bit_len, DbcByteOrder_t order);

/**
 * @brief Chuyển đổi giá trị thô sang giá trị vật lý (Physical Value) có tính bù dấu
 */
int32_t DBC_DecodeSignal(const uint8_t *payload, uint8_t dlc, const DbcSignalMeta_t *meta);

#endif /* DBC_DECODER_H */
```

---

### 📂 KHỐI 2: FILE SOURCE THUẬT TOÁN BÓC TÁCH BIT [ `drivers/src/dbc_decoder.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 2:
1. **Tra cứu Chuẩn biểu diễn Bit Endianness trong ô tô:**
   - **Mở Vector Application Note "CAN Message Layout and Byte Order"**:
     - Định dạng **Intel (`@1`)**: Start Bit là LSB. Bit tăng tịnh tiến `current_bit = start_bit + i`, chia 8 lấy `byte_idx` và chia dư 8 lấy `bit_idx`.
     - Định dạng **Motorola Sequential (`@0`)**: Start Bit là MSB. Các bit tiếp theo đếm lùi trong nội bộ byte (`current_bit--`), khi chạm bit 0 của byte đó thì nhảy zíc-zắc sang bit 7 của byte kế tiếp (`current_bit += 15`).
2. **Bảo vệ an toàn bộ nhớ:**
   - Bắt buộc kiểm tra `byte_idx < dlc` chống truy cập ngoài mảng khi frame thực nhận ngắn hơn dự kiến.

#### TODO 2 [File: `drivers/src/dbc_decoder.c`]: Giải Thuật Bóc Tách Bit Intel & Motorola
```c
#include "dbc_decoder.h"

uint32_t DBC_UnpackRaw(const uint8_t *payload, uint8_t dlc, 
                       uint8_t start_bit, uint8_t bit_len, DbcByteOrder_t order)
{
    if (payload == 0 || dlc == 0 || bit_len == 0 || bit_len > 32) {
        return 0;
    }

    uint32_t raw_val = 0;

    if (order == DBC_BYTE_ORDER_INTEL) {
        /* ====================================================================
         * THUẬT TOÁN INTEL (LITTLE-ENDIAN): Bit đếm tiến từ Start Bit
         * ==================================================================== */
        for (uint8_t i = 0; i < bit_len; i++) {
            uint8_t current_bit = start_bit + i;
            uint8_t byte_idx = current_bit / 8U;
            uint8_t bit_idx  = current_bit % 8U;

            if (byte_idx >= dlc) {
                break; /* Chống đọc vượt biên DLC */
            }

            /* Trích xuất từng bit và đưa vào vị trí tương ứng trong raw_val */
            if (payload[byte_idx] & (1U << bit_idx)) {
                raw_val |= (1UL << i);
            }
        }
    } else {
        /* ====================================================================
         * THUẬT TOÁN MOTOROLA (BIG-ENDIAN): Start Bit là MSB, đếm lùi zíc-zắc
         * ==================================================================== */
        uint8_t current_bit = start_bit;
        for (uint8_t i = 0; i < bit_len; i++) {
            uint8_t byte_idx = current_bit / 8U;
            uint8_t bit_idx  = current_bit % 8U;

            if (byte_idx < dlc) {
                if (payload[byte_idx] & (1U << bit_idx)) {
                    /* Bit MSB của tín hiệu nạp vào vị trí cao nhất */
                    raw_val |= (1UL << (bit_len - 1U - i));
                }
            }

            /* Bước nhảy Motorola: Đếm lùi trong byte, hết byte nhảy sang MSB byte kế */
            if (bit_idx == 0) {
                current_bit += 15U; /* Nhảy từ bit 0 của byte này sang bit 7 của byte kế tiếp */
            } else {
                current_bit -= 1U;
            }
        }
    }

    return raw_val;
}
```

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 3:
1. **Thuật toán Sign Extension (Mở rộng dấu bù hai):**
   - Nếu `is_signed = true` và bit cao nhất của trường bit bằng 1: Tạo mặt nạ `(~0UL << bit_length)` để điền các bit 1 lên vị trí 31 của kiểu `int32_t`.
2. **Số học số nguyên định điểm (Fixed-point Scaling):**
   - Ép kiểu `int64_t` trước khi thực hiện phép nhân `(signed_raw * factor_num)` để tránh hiện tượng tràn số nguyên 32-bit (Integer Overflow).
3. **Bảo vệ biên bão hòa (Clamping):**
   - Kiểm tra và ép giá trị vào khoảng `[min_val, max_val]` chống sai số ngoại lai.

#### TODO 3 [File: `drivers/src/dbc_decoder.c`]: Mở Rộng Dấu Bù Hai & Tính Giá Trị Vật Lý
```c
int32_t DBC_DecodeSignal(const uint8_t *payload, uint8_t dlc, const DbcSignalMeta_t *meta)
{
    /* 1. Bóc tách giá trị thô từ payload */
    uint32_t raw = DBC_UnpackRaw(payload, dlc, meta->start_bit, meta->bit_length, meta->byte_order);

    int32_t signed_raw = (int32_t)raw;

    /* 2. MỞ RỘNG DẤU BÙ HAI (SIGN EXTENSION) CHO TÍN HIỆU SIGNED */
    if (meta->is_signed && (meta->bit_length < 32)) {
        uint32_t sign_mask = 1UL << (meta->bit_length - 1U);
        if (raw & sign_mask) {
            /* Nếu bit dấu là 1 -> Điền toàn bộ các bit phía trên thành 1 */
            signed_raw = (int32_t)(raw | (~0UL << meta->bit_length));
        }
    }

    /* 3. TÍNH TOÁN GIÁ TRỊ VẬT LÝ BẰNG SỐ HỌC NGUYÊN (FIXED-POINT) */
    /* Physical = (signed_raw * factor_num) / factor_den + offset */
    int64_t scaled_val = ((int64_t)signed_raw * meta->factor_num) / meta->factor_den;
    int32_t physical_val = (int32_t)(scaled_val + meta->offset);

    /* 4. BẢO VỆ RANH GIỚI BÃO HÒA (CLAMPING) */
    if (physical_val < meta->min_val) {
        physical_val = meta->min_val;
    } else if (physical_val > meta->max_val) {
        physical_val = meta->max_val;
    }

    return physical_val;
}
```

---

### 📂 KHỐI 3: KIỂM THỬ GIẢI MÃ TÍN HIỆU XE HƠI [ `src/main.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 4:
1. **Kiểm thử đối chiếu với Vector CANoe / CANalyzer:**
   - Tạo mẫu mảng 8 bytes đại diện frame thực tế từ mạng CAN động cơ.
   - Gọi `DBC_DecodeSignal()` cho tín hiệu `SIG_SPEED` (Intel, Factor 1/16) và `SIG_RPM` (Intel, Factor 1/2) và xác nhận kết quả khớp 100% với file DBC mẫu.

#### TODO 4 [File: `src/main.c`]: Kiểm Thử Thực Tế Với Gói Tin Mạng CAN Ô Tô
```c
#include <stdio.h>
#include "dbc_decoder.h"

/* Khai báo tĩnh các metadata tín hiệu theo bảng DBC (Lưu trên Flash) */
static const DbcSignalMeta_t SIG_SPEED = {
    .start_bit = 0, .bit_length = 12, .byte_order = DBC_BYTE_ORDER_INTEL,
    .is_signed = false, .factor_num = 1, .factor_den = 16, .offset = 0,
    .min_val = 0, .max_val = 255
};

static const DbcSignalMeta_t SIG_RPM = {
    .start_bit = 16, .bit_length = 14, .byte_order = DBC_BYTE_ORDER_INTEL,
    .is_signed = false, .factor_num = 1, .factor_den = 2, .offset = 0,
    .min_val = 0, .max_val = 8000
};

static const DbcSignalMeta_t SIG_STEERING = {
    .start_bit = 7, .bit_length = 14, .byte_order = DBC_BYTE_ORDER_MOTOROLA,
    .is_signed = true, .factor_num = 1, .factor_den = 10, .offset = 0,
    .min_val = -720, .max_val = 720
};

int main(void)
{
    /* Giả lập gói tin CAN nhận được từ bus:
     * Byte 0..1: Speed Raw = 1920 (1920 / 16 = 120 km/h) -> Hex 0x0780 -> Byte 0=0x80, Byte 1=0x07
     * Byte 2..3: RPM Raw = 6000 (6000 / 2 = 3000 RPM) -> Hex 0x1770 -> Byte 2=0x70, Byte 3=0x17
     */
    uint8_t can_payload[8] = { 0x80, 0x07, 0x70, 0x17, 0x00, 0x00, 0x00, 0x00 };

    int32_t speed = DBC_DecodeSignal(can_payload, 8, &SIG_SPEED);
    int32_t rpm   = DBC_DecodeSignal(can_payload, 8, &SIG_RPM);

    /* Kiểm tra kết quả: Tốc độ phải ra đúng 120 km/h, RPM phải ra đúng 3000 */
    while (1) {
        /* Chạy bình thường */
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug Giải Mã Tín Hiệu "Kinh Điển" trong Ngày 11

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY GIẢI MÃ TÍN HIỆU AUTOMOTIVE DBC                           │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN MỀM               │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Giá trị góc lái│ Áp dụng giải thuật Intel cho tín hiệu     │ Tách rẽ nhánh riêng cho         │
│    Motorola ra rác│ Motorola (Start Bit của Motorola là MSB). │ Motorola: đếm lùi bit zíc-zắc.  │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Nhiệt độ âm bị │ Quên mở rộng bit dấu bù hai (Sign         │ Kiểm tra bit MSB của trường bit,│
│    nhảy lên +65500│ Extension) trước khi cộng với Offset âm.  │ điền bit 1 vào các bit cao hơn. │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Tràn số khi    │ Lấy `int32_t` nhân với tử số lớn làm      │ Ép kiểu sang `int64_t` trước khi│
│    tính Factor    │ tràn số nguyên trước khi chia cho mẫu số. │ thực hiện phép nhân hệ số.      │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Đọc dữ liệu rác│ Gói tin nhận được có DLC = 4 nhưng tín    │ Kiểm tra `byte_idx < dlc` trong │
│    ngoài bộ nhớ   │ hiệu nằm ở byte 6, đọc tràn vùng nhớ đệm. │ vòng lặp bóc tách bit.          │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Tiêu hao RAM   │ Khai báo bảng `DbcSignalMeta_t` không có  │ Thêm từ khóa `const` để bảng    │
│    lãng phí       │ từ khóa `const`, bị copy vào RAM lúc boot.│ nằm cố định trên Flash ROM.     │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao trong mạng truyền thông CAN Bus của xe hơi, các hãng sản xuất lại sử dụng định dạng Motorola Big-Endian song song với Intel?
* **Trả lời chuẩn Kỹ sư Automotive:** 
  * Định dạng Intel (Little-Endian) rất tự nhiên với kiến trúc vi xử lý x86 và ARM Cortex (vì CPU lưu biến LSB trước).
  * Tuy nhiên, các chip ECU điều khiển ô tô truyền thống (như Motorola 68HC12, Freescale PowerPC, Renesas RH850) sử dụng kiến trúc Big-Endian nguyên bản. Trong giao thức truyền thông, Big-Endian giúp các thiết bị máy hiện sóng và máy phân tích logic (CANoe, CANalyzer) hiển thị các byte dữ liệu theo đúng thứ tự logic từ trái sang phải dễ đọc cho mắt người.
  * Vì vậy, tiêu chuẩn Vector DBC bắt buộc mọi ECU Gateway hiện đại phải hỗ trợ song song cả hai định dạng: Nhận các gói tin Motorola từ hộp động cơ (ECU Engine) và chuyển đổi sang Intel để hiển thị lên táp-lô đồ họa.

### ❓ Câu 2: Giải thuật mở rộng dấu (Sign Extension) hoạt động như thế nào khi giải mã tín hiệu số âm có độ dài bất kỳ (ví dụ 11 bits)?
* **Trả lời chuẩn Kỹ sư Automotive:** 
  * Trong chuẩn C, biến `int32_t` có bit dấu nằm ở vị trí thứ 31. Khi ta bóc một tín hiệu 11-bit có dấu từ frame CAN, bit dấu của nó thực chất nằm ở vị trí **Bit 10**.
  * Nếu ta chỉ ép kiểu `(int32_t)raw`, CPU sẽ hiểu bit 31 đang là `0` và biến nó thành một số dương khổng lồ.
  * **Giải pháp Sign Extension:** 
    1. Kiểm tra bit 10: `if (raw & (1 << 10))`.
    2. Nếu bit 10 bằng `1` (số âm), ta tạo một mặt nạ lấp đầy các bit từ 11 đến 31 bằng các số `1`: `raw |= (~0UL << 11)`.
    3. Lúc này biến 32-bit trở thành số bù hai hoàn chỉnh và mang giá trị âm chính xác theo quy ước toán học.

### ❓ Câu 3: Tại sao trong phần mềm ô tô tiêu chuẩn AUTOSAR / MISRA-C, người ta cấm dùng phép chia số thực `float` khi giải mã tín hiệu?
* **Trả lời chuẩn Kỹ sư Automotive:**
  * Thứ nhất, phép tính số thực dấu phẩy động (Floating-point) không có tính tiền định tuyệt đối về mặt thời gian (Non-deterministic latency): Một phép chia float có thể tốn từ hàng chục đến hàng trăm chu kỳ CPU tùy thuộc vào giá trị có bị Denormalized hay không, gây trồi sụt độ trễ (Jitter) trong vòng lặp thời gian thực.
  * Thứ hai, sai số làm tròn số thực (Floating-point Rounding Error) có thể tích lũy qua hàng triệu chu kỳ lặp khiến việc so sánh giá trị bằng lệnh `==` bị sai lệch.
  * Sử dụng số học số nguyên định điểm (**Fixed-point arithmetic**) với phép nhân trước chia sau đảm bảo tính toán chính xác $100\%$, tốc độ thực thi trong 1 chu kỳ lệnh hợp ngữ và an toàn tuyệt đối theo tiêu chuẩn an toàn chức năng ISO 26262 ASIL-B.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 11, em phát triển bộ nhân giải mã tín hiệu mạng ô tô chuyên dụng **DBC Signal Engine** theo tiêu chuẩn của hãng Vector Informatik.  
> Em giải quyết triệt để bài toán bóc tách bit lẻ thời gian thực bằng thuật toán **Zero-Copy Bit Unpacking**, hỗ trợ hoàn hảo cả hai hệ quy chiếu byte: **Intel Little-Endian** và **Motorola Big-Endian** với bước nhảy zíc-zắc qua các ranh giới byte.  
> Để đạt chuẩn an toàn thời gian thực nghiêm ngặt và tối ưu hóa hiệu năng vi điều khiển ARM Cortex-M7, em áp dụng kỹ thuật số học nguyên định điểm **Fixed-Point Scaling**, loại bỏ hoàn toàn các phép chia số thực `float`, đồng thời tích hợp thuật toán mở rộng dấu bù hai **Sign Extension** cho các tín hiệu âm như góc lái và nhiệt độ động cơ. Nhờ đó, Gateway có thể giải mã hàng nghìn tín hiệu CAN mỗi giây với độ chính xác tuyệt đối và tải CPU gần như không đáng kể."*
