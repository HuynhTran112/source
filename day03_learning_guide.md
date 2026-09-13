# 🏆 [NGÀY 3] CẨM NANG TOÀN DIỆN BARE-METAL: bxCAN CONTROLLER, 28 FILTER BANKS & AUTOMOTIVE BUS-OFF RECOVERY
## Lộ trình 4 Bước: Nguyên Lý Phần Cứng ➔ Thực Chiến RM0385 ➔ Gõ Code Driver ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ từ gốc rễ mạch vi sai CAN Bus, tra cứu Reference Manual (RM0385) và Datasheet (DS10610), tính toán thông số Bit Timing chuẩn CiA 301 ($500\text{ kbps}$, Sample Point $87.5\%$), cấu hình 28 Filter Banks (Mask/List Mode), quản lý 3 Transmit Mailboxes / 2 Receive FIFOs, xử lý ngắt nhận dữ liệu và thuật toán phục hồi lỗi chuẩn công nghiệp ô tô (**Automotive Bus-Off Recovery**) trên STM32F746 (ARM Cortex-M7).  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phần cứng, thanh ghi, công thức toán học, bảng tra cứu và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 3                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: NGUYÊN LÝ │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE            │ BƯỚC 4: PHỎNG VẤN          │
│ • Bus Vi sai CAN  │ • Tra cứu RM0385  │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn bxCAN   │
│ • Mailboxes/FIFOs │   & DS10610       │ • TODO 1-3 [can.h]         │ • Bit Stuffing & CRC Error │
│ • Bit Timing Math │ • Bảng Base Addr  │ • TODO 4-8 [can.c]         │ • CAN1 Master Filter Trap  │
│ • 28 Filter Banks │ • Bảng Thanh ghi  │ • TODO 9 [main.c]          │ • Kịch bản trả lời 60s     │
│ • Bus-Off Recovery│ • Bảng W1C        │ • Mổ xẻ 5 Bug phần cứng    │   (Elevator Pitch)         │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC BARE-METAL CỐT LÕI (CHUYÊN CHO NGÀY 3)

| STT | Quy tắc Bare-metal | Thể hiện cụ thể trong Ngày 3 (bxCAN) |
| :---: | :--- | :--- |
| **1** | **Reset Value** | Tra cứu giá trị mặc định của `CAN_MCR` (`0x0001 0002` - chế độ Sleep bật sẵn), `CAN_BTR`, `CAN_FMR` (`0x2A1C 0E01` - cờ `FINIT = 1`) trước khi cấu hình. |
| **2** | **Access Type** | **CỰC KỲ QUAN TRỌNG:** Thanh ghi `CAN_TSR` (các cờ `RQCPx`, `TXOKx`) và `CAN_RFR` (cờ `FULLx`, bit giải phóng `RFOMx`) là dạng `rc_w1` (Write 1 to Clear) hoặc `rs` (Write 1 to Set). **TUYỆT ĐỐI KHÔNG DÙNG `|=`**, phải ghi gán trực tiếp `=` để không xóa nhầm trạng thái của Mailbox khác! |
| **3** | **Multi-Bit Clear-Set** | Áp dụng quy tắc xóa trước - gán sau (`REG &= ~MASK; REG |= VALUE;`) cho các trường đa bit: `TS1[3:0]`, `TS2[2:0]`, `SJW[1:0]`, `BRP[9:0]`, `CAN2SB[5:0]`, `DLC[3:0]`, `STID[10:0]`. |
| **4** | **`volatile` Qualification** | Mọi struct ánh xạ thanh ghi CAN và các biến chia sẻ giữa ngắt ISR và main (`can_rx_count`, `can_bus_off_flag`) bắt buộc khai báo `volatile`. |
| **5** | **Interrupt Workflow** | Quy trình 5 bước ngắt nhận `CAN1_RX0_IRQHandler`: Cờ phần cứng `FMP0 > 0` $\rightarrow$ Bật `CAN_IER_FMPIE0` $\rightarrow$ Bật `NVIC_EnableIRQ(CAN1_RX0_IRQn)` $\rightarrow$ Đọc dữ liệu từ `CAN_RDLR`/`CAN_RDHR` trong ISR $\rightarrow$ Giải phóng mailbox bằng lệnh `CAN1->RF0R = CAN_RF0R_RFOM0`. |
| **6** | **RM / DS Lookup** | Cung cấp chính xác Chapter, Section, từ khóa `Ctrl + F`, công thức `Base Address + Offset` cho `CAN1`, `CAN2`, `CAN Filters`, `GPIOB`. |
| **7** | **Hardware Rationale & Specs** | Bố trí cơ sở kỹ thuật, giới hạn phần cứng ($f_{PCLK1} \le 54\text{MHz}$, Bit Timing chuẩn CiA 301 Sample Point $87.5\%$, 3 Transmit Mailboxes, 2 FIFO nhận 3 tầng) liền kề từng bảng thanh ghi. |

---

# 🧠 BƯỚC 1: NGUYÊN LÝ PHẦN CỨNG & CƠ CHẾ VẬT LÝ (HARDWARE ARCHITECTURE)

## 1.1. Kiến trúc Mạng CAN & Sự Cần Thiết của CAN Transceiver Rời

Chip STM32F746NG chỉ tích hợp khối **CAN Controller (bxCAN)**, chịu trách nhiệm ở tầng liên kết dữ liệu (Data Link Layer: mã hóa bit, nhồi bit - bit stuffing, tính toán mã kiểm tra CRC, phân định ưu tiên ID). 

Để giao tiếp trên bus vật lý ngoài, tín hiệu số từ STM32 phải đi qua chip chuyển đổi **CAN Transceiver** (ví dụ: `SN65HVD230` nguồn 3.3V hoặc `TJA1050` nguồn 5V):

```text
┌─────────────────────────── STM32F746NG Microcontroller ──────────────────────────┐
│                                                                                  │
│   ┌────────────────────────┐      TX Line (PB9)          ┌──────────────────┐    │
│   │  bxCAN Controller      │ ──────────────────────────► │  CAN Transceiver │    │
│   │  - 3 TX Mailboxes      │                             │  (SN65HVD230)    │ ───► CAN_H (3.5V)
│   │  - 2 RX FIFOs (3-deep) │ ◄────────────────────────── │  (Chuyển đổi     │ ───► CAN_L (1.5V)
│   │  - 28 Filter Banks     │      RX Line (PB8)          │   Logic <-> Bus) │     (Bus Vi Sai)
│   └────────────────────────┘                             └──────────────────┘
└──────────────────────────────────────────────────────────────────────────────────┘
```

> [!NOTE]
> **Thực hành trên Kit STM32F746G-DISCO (Không có CAN Transceiver):**  
> Bo mạch Discovery STM32F746G-DISCO **không hàn sẵn chip CAN Transceiver ngoài**. Nếu chưa cắm thêm module SN65HVD230 ngoài, bạn hãy cấu hình bit **`CAN_BTR_LBKM = 1` (Loopback Mode)** trong thanh ghi `CAN1->BTR`.  
> Ở chế độ Loopback, khối phần cứng `bxCAN` tự động nối tín hiệu `TX` chui thẳng ngược về `RX` ngay bên trong vi điều khiển, giúp bạn thực hành tự gửi/nhận ngắt CAN hoàn hảo $100\%$ trên duy nhất 1 bo mạch mà không cần cắm thêm bất kỳ linh kiện nào ngoài.

```text
┌───────────────────────────────── STM32F746NG Microcontroller ──────────────────────────────────┐
│                                                                                                │
│   ┌────────────────────────────────────────────────────────────────────────────────────────┐   │
│   │  bxCAN Controller (Chế độ LOOPBACK MODE: CAN_BTR_LBKM = 1)                             │   │
│   │                                                                                        │   │
│   │  ┌───────────────────────┐   Đường nối lặp nội bộ    ┌──────────────────────────────┐  │   │
│   │  │  3 TX Mailboxes       ├──────────────────────────►│ 28 Filter Banks ➔ 2 RX FIFOs│  │   │
│   │  └───────────┬───────────┘    (Internal Loopback)    └──────────────▲───────────────┘  │   │
│   └──────────────┼──────────────────────────────────────────────────────┼──────────────────┘   │
│                  │                                                      │                      │
│            Cổng phát TX                                           Cổng nhận RX                 │
│             (Chân PB9)                                             (Chân PB8)                  │
│                  │                                                      │                      │
│                  └────── Ngắt kết nối khỏi chân vật lý bên ngoài ───────┘                      │
│                           (Không cần Chip Transceiver ngoài)                                   │
└────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Mức logic trên Bus Vi sai CAN (Differential Bus):
* **Trạng thái Dominant (Mức logic 0 - Mức Thống trị):**
  * Chân `CAN_H` được kéo lên $\approx 3.5\text{ V}$. Chân `CAN_L` được kéo xuống $\approx 1.5\text{ V}$.
  * Điện áp vi sai: $V_{DIFF} = V_{CAN\_H} - V_{CAN\_L} \approx 2.0\text{ V} > 0.9\text{ V}$.
  * Nếu một node phát mức 0 và một node phát mức 1 cùng lúc, mức 0 sẽ đè bẹp mức 1 trên bus (Cơ chế phân định quyền ưu tiên Bus Arbitration).
* **Trạng thái Recessive (Mức logic 1 - Mức Ẩn / Trạng thái Nghỉ):**
  * Cả hai dây `CAN_H` và `CAN_L` đều được thả về mức điện áp trung gian $\approx 2.5\text{ V}$.
  * Điện áp vi sai: $V_{DIFF} = V_{CAN\_H} - V_{CAN\_L} \approx 0\text{ V} < 0.5\text{ V}$.

---

## 1.2. Cấu trúc Nội bộ của bxCAN: Mailboxes, FIFOs & Filter Banks

```text
                               ┌──────────────────────────────────────────────────┐
                               │                 bxCAN CORE ENGINE                │
                               │ - Protocol Controller (Bit stuffing, CRC, ACK)   │
                               │ - Baud Rate Prescaler (Bit Timing Logic)         │
                               └──────────────────────────────────────────────────┘
                                        ▲                               ▲
                         Gửi dữ liệu    │                               │  Nhận dữ liệu
              ┌─────────────────────────┴────────┐            ┌─────────┴────────────────────────┐
              │     3 TRANSMIT MAILBOXES         │            │       28 FILTER BANKS            │
              │  ┌────────────────────────────┐  │            │  (CAN1 Master quản lý chia sẻ)   │
              │  │ Mailbox 0 (TIR, TDTR, TDR) │  │            └─────────────────┬────────────────┘
              │  ├────────────────────────────┤  │                              │
              │  │ Mailbox 1 (TIR, TDTR, TDR) │  │               Đạt điều kiện  │
              │  ├────────────────────────────┤  │               lọc ID         ▼
              │  │ Mailbox 2 (TIR, TDTR, TDR) │  │            ┌──────────────────────────────────┐
              │  └────────────────────────────┘  │            │       2 RECEIVE FIFOs (3-deep)   │
              │   Tự động chọn Mailbox có ID     │            │  ┌────────────────────────────┐  │
              │   ưu tiên cao nhất để gửi trước! │            │  │ FIFO 0: Mailbox 0, 1, 2    │  │
              └──────────────────────────────────┘            │  ├────────────────────────────┤  │
                                                              │  │ FIFO 1: Mailbox 0, 1, 2    │  │
                                                              │  └────────────────────────────┘  │
                                                              └──────────────────────────────────┘
```

* **3 Hộp thư Phát (Transmit Mailboxes 0, 1, 2):** Cho phép nạp sẵn tối đa 3 bản tin. bxCAN tự động so sánh ID giữa các mailbox đang chờ để phát bản tin có Identifier nhỏ nhất (ưu tiên cao nhất) ra bus trước.
* **2 Hộp thư Nhận FIFO (Receive FIFOs 0 và 1):** Mỗi FIFO có cấu trúc hàng đợi 3 tầng sâu (3-deep). Nếu ứng dụng chưa kịp đọc bản tin thứ 1, phần cứng vẫn có thể nhận tiếp bản tin thứ 2 và thứ 3 mà không bị mất dữ liệu (Overrun).

---

## 1.3. Tính Toán CAN Bit Timing Chuẩn CiA 301 & Cơ Chế Đồng Bộ Hóa Toàn Cảnh

---

### ⏱️ 1.3.1. Các Khái Niệm & Định Nghĩa Nền Tảng Của CAN Bit Timing

Trước khi đi vào các bước tính toán số học, các khái niệm thời gian trong mạng CAN được chuẩn hóa quốc tế theo ISO 11898-1 và Reference Manual RM0385 như sau:

#### 1. Chu kỳ 1 Bit CAN (T_bit) và Tốc độ Baudrate
* Thời lượng của 1 bit là khoảng thời gian để truyền trọn vẹn 1 bit dữ liệu trên bus:
  ```text
  T_bit = 1 / Baudrate
  ```
  *Ví dụ:* Ở tốc độ 500 kbps (500,000 bps), chu kỳ 1 bit là:
  ```text
  T_bit = 1 / 500,000 = 0.000002 s = 2000 ns (2.0 µs)
  ```

#### 2. Bản chất của tq (Time Quantum) & Bộ chia BRP (Baud Rate Prescaler)
* **tq (Time Quantum - số nhiều: Time Quanta):** Là **đơn vị thời gian nguyên tử nhỏ nhất (Atomic Clock Tick)** của khối điều khiển Bit Timing Logic (BTL) trong silicon CAN controller.
* **Cách tạo ra từ phần cứng:** Bộ điều khiển CAN không đếm bit bằng micro-giây tùy ý, mà dùng một bộ chia tần số số học **Baud Rate Prescaler (BRP)** từ xung nhịp bus f_PCLK1 (trên STM32F746 là 54 MHz):
  ```text
  tq = BRP / f_PCLK1
  ```
* **Lượng tử hóa chu kỳ bit:** Chu kỳ T_bit luôn được cấu thành từ một **số nguyên lần các đơn vị tq** (N_q nằm trong khoảng từ 8 đến 25 tq theo chuẩn ISO 11898-1):
  ```text
  T_bit = N_q * tq
  ```

#### 3. Bốn Phân Đoạn (Segments) Cấu Thành 1 Bit CAN (ISO 11898-1)
Chu kỳ 1 bit CAN được chia làm 4 phân đoạn liên tiếp với nhiệm vụ vật lý riêng biệt:

1. **`Sync_Seg` (Synchronization Segment - Đoạn Đồng bộ):**
   * **Độ dài:** Cố định đúng **1 tq** (phần cứng quy định, không thể thay đổi).
   * **Nhiệm vụ:** Dùng để đồng bộ hóa cạnh xung giữa các nút trên mạng. Cạnh xuống từ Recessive sang Dominant của bit kỳ vọng phải rơi vào đoạn này.
2. **`Prop_Seg` (Propagation Segment - Đoạn Bù trễ Dây dẫn & Transceiver):**
   * **Nhiệm vụ:** Bù trừ độ trễ vật lý khi tín hiệu điện chạy dọc trên đường dây cáp và đi qua các cổng bán dẫn của chip CAN Transceiver.
   * **Cơ sở vật lý:** Tín hiệu từ Node A phải truyền tới Node xa nhất B, rồi từ Node B phản hồi ngược lại Node A trong cùng 1 chu kỳ bit:
     ```text
     T_Prop_Seg >= 2 * (t_wire_delay + t_transceiver_delay)
     ```
   * Đoạn này giúp điện áp trên toàn bộ chiều dài sợi cáp đạt trạng thái ổn định phẳng lặng trước khi đo đạc.
3. **`Phase_Seg1` (Phase Buffer Segment 1 - Đoạn Đệm Pha 1):**
   * **Vị trí:** Nằm ngay trước **Điểm lấy mẫu (Sample Point)**.
   * **Nhiệm vụ:** Kéo dài bit khi xung nhịp bị trễ pha (Late Edge) trong quá trình tái đồng bộ (Resynchronization).
4. **`Phase_Seg2` (Phase Buffer Segment 2 - Đoạn Đệm Pha 2):**
   * **Vị trí:** Nằm ngay sau **Điểm lấy mẫu (Sample Point)** kéo dài đến hết bit.
   * **Nhiệm vụ:** Cắt ngắn bit khi xung nhịp bị sớm pha (Early Edge) trong quá trình tái đồng bộ.

#### 4. Quy ước Gộp Của STM32 bxCAN: Khối BS1 và BS2
Để tinh gọn các trường thanh ghi điều khiển `CAN_BTR`, hãng STMicroelectronics gộp 4 phân đoạn trên thành 2 khối:
* **Khối `BS1` (Bit Segment 1):** Gộp chung `Prop_Seg` và `Phase_Seg1`:
  ```text
  BS1 = Prop_Seg + Phase_Seg1   (Cấu hình qua trường TS1[3:0], nhận giá trị từ 1 đến 16 tq)
  ```
* **Khối `BS2` (Bit Segment 2):** Chính là `Phase_Seg2`:
  ```text
  BS2 = Phase_Seg2              (Cấu hình qua trường TS2[2:0], nhận giá trị từ 1 đến 8 tq)
  ```
* Như vậy, tổng số Time Quanta trong 1 bit trên STM32 là:
  ```text
  N_q = Sync_Seg (1 tq) + BS1 + BS2
  ```

#### 5. Khái Niệm Điểm Lấy Mẫu (Sample Point) & Rationale Tại Sao Chọn 87.5%?
* **Điểm lấy mẫu (Sample Point):** Là thời điểm chính xác mà bộ điều khiển CAN đọc điện áp trên bus để chốt giá trị bit đó là mức 0 hay mức 1:
  ```text
  Sample Point (%) = ((Sync_Seg + BS1) / N_q) * 100% = ((1 + BS1) / N_q) * 100%
  ```
* **Tại sao chuẩn CiA 301 / ISO 11898-1 quy định tối ưu là 87.5%?**
  * **Bản chất toán học:** `87.5% = 7/8 = 0.111 (nhị phân)`, là phân số nhị phân tối ưu cho mạch đếm trong vi mạch số.
  * **Cơ sở vật lý:** Điểm lấy mẫu giải quyết mâu thuẫn giữa 2 yêu cầu kỹ thuật:
    * *Muốn đẩy lùi càng về cuối bit càng tốt (> 80%):* Để đoạn `Prop_Seg` đủ dài, cho phép kết nối chiều dài dây cáp đạt tối đa (100 mét ở tốc độ 500 kbps).
    * *Không được đẩy quá sát đuôi bit (< 90%):* Để đoạn `Phase_Seg2` còn đủ không gian cho phần cứng co ngắn bit bù trừ độ trôi tần số (Clock Drift) do thạch anh biến thiên theo nhiệt độ.
    * $\implies$ Điểm cân bằng cực trị hội tụ chính xác tại **87.5%**.

#### 6. Biên Độ Nhảy Bù Pha SJW (Synchronization Jump Width)
* Là số lượng `tq` tối đa mà phần cứng được phép co hoặc dãn trên `Phase_Seg1` và `Phase_Seg2` trong mỗi chu kỳ tái đồng bộ (cấu hình qua trường `SJW[1:0]`, thường chọn `1 tq` đến `4 tq`).

---

### 📐 1.3.2. Quy Trình & Thuật Toán Tính Toán Bit Timing Cho STM32F746

#### 📌 Bước 1: Xác Định Các Thông Số Đầu Vào (Nguồn Gốc Từ Đâu?)
* **Tần số ngoại vi (f_PCLK1):** `54 MHz` (`54,000,000 Hz`). Tra trong Datasheet (DS10610 - Bus APB1 tối đa 54 MHz) và code cấu hình Clock hệ thống Ngày 1.
* **Tốc độ Baudrate mục tiêu (500 kbps):**
  * *Tra ở đâu để biết?* Không nằm trong Reference Manual hay Datasheet. Đây là **yêu cầu hệ thống (System Specification)** được quy định trong **file cơ sở dữ liệu CAN (.DBC)** hoặc các tiêu chuẩn quốc tế như **ISO 15765-4 (OBD-II chẩn đoán ô tô)** và **CiA 301**.
  * Chu kỳ 1 bit: `T_bit = 1 / 500,000 = 2000 ns`.
* **Điểm lấy mẫu mục tiêu:** `87.5%` (Chuẩn CiA 301 cho tốc độ <= 500 kbps).

#### 🔍 Bước 2: Thuật Toán Giải Mã Tìm Nghiệm Số Nguyên N_q và BRP
Để tốc độ truyền đạt độ chính xác tuyệt đối (Baudrate Error = 0.00%), bộ chia BRP bắt buộc phải là một **SỐ NGUYÊN DƯƠNG**.

1. **Thiết lập phương trình:**
   ```text
   BRP = f_PCLK1 / (Baudrate * N_q) = 54,000,000 / (500,000 * N_q) = 108 / N_q
   ```
2. **Ràng buộc:**
   * Phần cứng quy định: `8 <= N_q <= 25`.
   * BRP nguyên $\implies$ **N_q bắt buộc phải là ƯỚC SỐ của 108**.
3. **Tìm các ước số của 108 trong đoạn [8, 25]:**
   Các ước số của 108 gồm: `1, 2, 3, 4, 6, 9, 12, 18, 27, 36, 54, 108`.  
   Trong khoảng `[8, 25]`, ta có đúng **3 ứng viên**:
   * Ứng viên 1: `N_q = 9`   $\implies BRP = 108 / 9 = 12`
   * Ứng viên 2: `N_q = 12`  $\implies BRP = 108 / 12 = 9`
   * Ứng viên 3: `N_q = 18`  $\implies BRP = 108 / 18 = 6`
4. **So khớp Sample Point 87.5% để chọn ứng viên tối ưu:**
   * **Nếu chọn N_q = 9:**
     * `Sample Point 87.5% = 9 * 0.875 = 7.875` --> Chọn `1 + BS1 = 8 tq`.
     * Phân đoạn còn lại: `BS2 = 9 - 8 = 1 tq`.
     * *Rủi ro:* `BS2` chỉ có `1 tq`, biên độ co dãn bù pha quá hẹp (`SJW` tối đa chỉ là 1), rất dễ mất đồng bộ khi nhiệt độ môi trường làm trôi tần số thạch anh. *(Loại)*
   * **Nếu chọn N_q = 12:**
     * `Sample Point 87.5% = 12 * 0.875 = 10.5 tq`.
     * Nếu chọn `1 + BS1 = 10` --> `Sample Point = 10 / 12 = 83.33%` (Lệch nhiều so với chuẩn 87.5%).
     * Nếu chọn `1 + BS1 = 11` --> `Sample Point = 11 / 12 = 91.67%` (Quá sát đuôi bit, `BS2` chỉ còn 1 tq). *(Loại)*
   * **Nếu chọn N_q = 18:**
     * `Sample Point 87.5% = 18 * 0.875 = 15.75` --> Chọn `1 + BS1 = 16 tq` (tức `BS1 = 15 tq`).
     * Phân đoạn còn lại: `BS2 = 18 - 16 = 2 tq`.
     * `Điểm lấy mẫu thực tế = (1 + 15) / 18 = 16 / 18 = 88.89%` (Cực kỳ sát chuẩn quốc tế 87.5%).
     * Đồng thời `BS2 = 2 tq` tạo khoảng đệm an toàn tuyệt đối cho phép cấu hình `SJW = 1 tq` hoặc `2 tq`.
   * 👉 **KẾT LUẬN:** **`N_q = 18` là nghiệm số nguyên duy nhất thỏa mãn trọn vẹn cả 2 điều kiện!**

#### ⏱️ Bước 3: Phân Bổ Chi Tiết Các Phân Đoạn
* `tq = T_bit / 18 = 2000 ns / 18 = 111.11 ns`.
* `BRP = 6`.
* `Sync_Seg = 1 tq`.
* `BS1 = 15 tq` (Bao gồm `Prop_Seg = 7 tq` và `Phase_Seg1 = 8 tq`).
* `BS2 = Phase_Seg2 = 2 tq`.
* `SJW = 1 tq` (hoặc `2 tq`).
* Điểm lấy mẫu: `(1 + 15) / 18 = 16 / 18 = 88.89%`.

---

### 📋 1.3.3. Bảng Tổng Kết Cấu Hình Thanh Ghi CAN_BTR (Tuân Thủ Quy Tắc Phần Cứng N - 1)

Trong Reference Manual RM0385 (Mục 30.9.2 - thanh ghi `CAN_BTR`), phần cứng vi điều khiển **tự động cộng thêm 1** vào giá trị nạp:

```text
Giá trị ghi vào thanh ghi = Giá trị thực tế mong muốn - 1
```

| Tên Trường Bit | Vị Trí Bit | Ý Nghĩa Kỹ Thuật | Giá Trị Thực Tế | Công Thức Tính | Giá Trị Nạp (Dec) | Giá Trị Hex / Nhị Phân |
| :--- | :---: | :--- | :---: | :--- | :---: | :---: |
| **`BRP[9:0]`** | `[9:0]` | Baud Rate Prescaler | `6` | `6 - 1` | **`5`** | `0x005` (`0000000101b`) |
| **`TS1[3:0]`** | `[19:16]` | Time Segment 1 (`BS1`) | `15 tq` | `15 - 1` | **`14`** | `0xE` (`1110b`) |
| **`TS2[2:0]`** | `[22:20]` | Time Segment 2 (`BS2`) | `2 tq` | `2 - 1` | **`1`** | `0x1` (`001b`) |
| **`SJW[1:0]`** | `[25:24]` | Resync Jump Width | `1 tq` | `1 - 1` | **`0`** | `0x0` (`00b`) |

Mã code cấu hình Bare-metal theo chuẩn Clear-then-Set:
```c
/* Bước 1: Xóa trắng 4 trường bit trong CAN_BTR */
CAN1->BTR &= ~((0x03UL << 24) |   /* SJW[1:0] */
               (0x07UL << 20) |   /* TS2[2:0] */
               (0x0FUL << 16) |   /* TS1[3:0] */
               (0x3FFUL << 0));   /* BRP[9:0] */

/* Bước 2: Nạp các giá trị đã tính toán */
CAN1->BTR |= ((0UL << 24)  |      /* SJW = 1 tq (nạp 0)   */
              (1UL << 20)  |      /* TS2 = 2 tq (nạp 1)   */
              (14UL << 16) |      /* TS1 = 15 tq (nạp 14) */
              (5UL << 0));        /* BRP = 6 (nạp 5)      */
```

---

### ⚡ 1.3.4. Bản Chất Vật Lý Của Bit CAN: Phân Tách 2 Trạng Thái Dominant vs. Recessive

Bus CAN truyền tín hiệu vi sai qua 2 dây xoắn đôi (`CAN_H`, `CAN_L`) với 2 điện trở đầu cuối 120 Ohm (`R_bus = 120 || 120 = 60 Ohm`).

```text
========================================================================================================================
TRƯỜNG HỢP 1: TRẠNG THÁI RECESSIVE (MỨC LOGIC 1 - TRẠNG THÁI NGHỈ / ẨN)
========================================================================================================================
Sơ đồ Transceiver:
                 VCC (3.3V / 5V)
                      │
                     [ ] Cầu phân áp nội trở kháng cao
                      ├───► CAN_H = 2.5V ────────────┐
                      │                               │  RL = 60 Ohm
                      │                               │  (Không có dòng điện chạy qua: I_bus ≈ 0 mA)
                      │                               │
                      ├───► CAN_L = 2.5V ────────────┘
                     [ ] Cầu phân áp nội trở kháng cao
                      │
                     GND
• Trạng thái Transistor: Tầng công suất TẮT HOÀN TOÀN (Trở kháng cao High-Z).
• Điện áp Dây CAN_H:     2.5 V (Treo điện áp tự nhiên qua cầu phân áp nội)
• Điện áp Dây CAN_L:     2.5 V (Treo điện áp tự nhiên qua cầu phân áp nội)
• Hiệu điện thế vi sai:  V_DIFF = CAN_H - CAN_L = 2.5V - 2.5V = 0.0 V (Chuẩn ISO: -0.5V <= V_DIFF <= +0.5V)
• Dòng điện Bus:         I_bus ≈ 0 mA
• Chân RX (Vào MCU):     3.3 V (Mức Logic 1)
• Đặc tính phân định:    Bị mức Dominant đè bẹp hoàn toàn nếu có một Node khác phát Dominant cùng lúc.

========================================================================================================================
TRƯỜNG HỢP 2: TRẠNG THÁI DOMINANT (MỨC LOGIC 0 - TRẠNG THÁI THỐNG TRỊ)
========================================================================================================================
Sơ đồ Transceiver:
                 VCC (3.3V / 5V)
                      │
                     ┌┴┐ Transistor High-side DẪN BÃO HÒA (Kéo nguồn chủ động)
                     └┬┘
                      ├───► CAN_H = 3.5V ────────────┐
                      │                               │  Bơm dòng qua trở kết thúc bus:
                      │                               │  I_bus = 2.0V / 60 Ohm ≈ 33.3 mA
                      │                               │
                      ├───► CAN_L = 1.5V ────────────┘
                     ┌┴┐ Transistor Low-side DẪN BÃO HÒA (Kéo mass chủ động)
                     └┬┘
                      │
                     GND
• Trạng thái Transistor: Tầng công suất KÍCH HOẠT DẪN CỰC MẠNH.
• Điện áp Dây CAN_H:     3.5 V (Kéo chủ động lên mức cao)
• Điện áp Dây CAN_L:     1.5 V (Kéo chủ động xuống mức thấp)
• Hiệu điện thế vi sai:  V_DIFF = CAN_H - CAN_L = 3.5V - 1.5V = +2.0 V (Chuẩn ISO: +1.5V <= V_DIFF <= +3.0V)
• Dòng điện Bus:         I_bus = 2.0V / 60 Ohm ≈ 33.3 mA
• Chân RX (Vào MCU):     0.0 V (Mức Logic 0)
• Đặc tính phân định:    THỐNG TRỊ TOÀN BUS. Nếu Node A phát Recessive mà Node B phát Dominant, 
                         dây bus bị ép thành 3.5V/1.5V (V_DIFF = 2.0V). Cả hai đều đọc về mức 0.
```

| Đại lượng Đo Đạc | TRƯỜNG HỢP 1: RECESSIVE (Mức Logic 1) | TRƯỜNG HỢP 2: DOMINANT (Mức Logic 0) |
| :--- | :---: | :---: |
| **Trạng thái Transceiver TXD** | `3.3 V` (Không kích hoạt tầng công suất) | `0.0 V` (Kích hoạt tầng công suất dẫn bão hòa) |
| **Điện áp Dây CAN_H** | **`2.5 V`** (Treo điện áp phân cực trung gian) | **`3.5 V`** (Kéo nguồn chủ động) |
| **Điện áp Dây CAN_L** | **`2.5 V`** (Treo điện áp phân cực trung gian) | **`1.5 V`** (Kéo mass chủ động) |
| **Hiệu điện thế vi sai (V_DIFF)** | `V_DIFF = 2.5V - 2.5V =` **`0.0 V`** (`-0.5V <= V_DIFF <= +0.5V`) | `V_DIFF = 3.5V - 1.5V =` **`+2.0 V`** (`+1.5V <= V_DIFF <= +3.0V`) |
| **Dòng điện tải qua Bus (R_L = 60 Ohm)** | `I_bus ≈` **`0 mA`** | `I_bus ≈` **`33.3 mA`** |
| **Điện áp Chân RX vào Vi điều khiển** | **`3.3 V`** (Mức Logic 1) | **`0.0 V`** (Mức Logic 0) |
| **Quyền ưu tiên Phân định (Arbitration)** | Bị đè bẹp (Yielding) | **Thống trị tuyệt đối (Dominant)** |

---

### 🔄 1.3.5. Cơ Chế Đồng Bộ Hóa (Synchronization) & Phân Tích Dạng Sóng Toàn Cảnh

Bus CAN là giao thức **truyền thông không đồng bộ (Asynchronous)** — không có dây xung Clock đi kèm dữ liệu. Mỗi nút mạng chạy thạch anh riêng, chắc chắn sẽ có sai số trôi tần số (Clock Drift do nhiệt độ và dung sai chế tạo). Nếu không liên tục đồng bộ lại pha, điểm lấy mẫu sẽ trôi lệch và gây ra lỗi dữ liệu.

#### ⚠️ Quy Tắc Vàng Kích Hoạt Đồng Bộ (ISO 11898-1):
1. **CHỈ CÓ CẠNH CHUYỂN MỨC TỪ RECESSIVE SANG DOMINANT (Tương đương cạnh xuống 3.3V -> 0.0V trên chân RX của MCU / Cạnh lên của V_DIFF)** mới được phần cứng bxCAN sử dụng để đồng bộ!
2. **CẠNH CHUYỂN TỪ DOMINANT VỀ RECESSIVE TUYỆT ĐỐI KHÔNG ĐỒNG BỘ.**  
   *Cơ sở vật lý:* Khi chuyển về Recessive, transistor ngắt dẫn, điện áp hai dây xả về 2.5V phụ thuộc vào hằng số thời gian RC của điện dung ký sinh trên đường cáp dài qua điện trở 60 Ohm. Sườn dốc xả này bị méo theo độ dài cáp nên không đủ độ sắc nét và ổn định để làm mốc chuẩn thời gian.

#### 💡 Bảng Đối Chiếu: Cạnh Xuống 1 -> 0 Nằm ở Đường Nào?

| Đường Tín Hiệu | Mức 1 (Recessive) | Mức 0 (Dominant) | Chiều Biến Thiên Điện Áp | Trạng Thái Sườn Xung |
| :--- | :---: | :---: | :---: | :---: |
| **Chân RX vi điều khiển (PB8)** | **`3.3 V`** | **`0.0 V`** | **`3.3V -> 0.0V`** | **CẠNH XUỐNG (Falling Edge)** *(bxCAN bắt cạnh này)* |
| **Dây vi sai CAN_L ngoài cáp** | **`2.5 V`** | **`1.5 V`** | **`2.5V -> 1.5V`** | **CẠNH XUỐNG (Falling Edge)** |
| **Dây vi sai CAN_H ngoài cáp** | **`2.5 V`** | **`3.5 V`** | **`2.5V -> 3.5V`** | **CẠNH LÊN (Rising Edge)** |
| **Hiệu điện thế vi sai V_DIFF** | **`0.0 V`** | **`+2.0 V`** | **`0.0V -> +2.0V`** | **CẠNH LÊN (Rising Edge)** |

#### Hai Chế Độ Đồng Bộ Hóa Trong Khối bxCAN:
1. **Hard Synchronization (Đồng bộ Cứng):**
   * Xảy ra **duy nhất 1 lần khi bắt đầu Frame** — tại cạnh xuống của bit **SOF (Start of Frame)** sau trạng thái Bus Idle.
   * Bộ đếm Time Quanta nội bộ của Node nhận bị **RESET TỨC THÌ VỀ 0**, ép cạnh này bắt đầu chính xác tại phân đoạn `Sync_Seg`.
2. **Resynchronization (Tái đồng bộ Mềm):**
   * Xảy ra ở tất cả các cạnh Recessive $
ightarrow$ Dominant tiếp theo bên trong bản tin.
   * Đo sai số pha: `e = vị trí cạnh thực tế - vị trí Sync_Seg kỳ vọng`. Phần cứng tự động co hoặc dãn bằng giá trị `SJW`.

---

#### 📊 Dạng Sóng Phân Tích: So Sánh 3 Kịch Bản Tái Đồng Bộ (Resync Modes)

```text
========================================================================================================================
KỊCH BẢN 1: ĐỒNG BỘ HOÀN HẢO (IN-SYNC: Phase Error e = 0)
========================================================================================================================
Cạnh Recessive -> Dominant rơi ĐÚNG VÀO phân đoạn Sync_Seg (t_q 01). Bộ đếm không cần điều chỉnh.

                  |<-------------------------- 1 BIT HOÀN HẢO (18 t_q = 2000 ns) -------------------------->|
Phân đoạn         | Sync_Seg |          Prop_Seg         |          Phase_Seg1           |    Phase_Seg2   |
Số lượng t_q      | (1 t_q)  |          (7 t_q)          |            (8 t_q)            |      (2 t_q)    |
──────────────────┼──────────┼───────────────────────────┼───────────────────────────────┼─────────────────┼────
CAN_H (Bus ngoài) | 2.5V ────┐3.5V (CẠNH LÊN vi sai)                                     │                 │
                  |          └───────────────────────────────────────────────────────────┴─────────────────┴────
CAN_L (Bus ngoài) | 2.5V ────┐                                                           │                 │
                  |          │ 1.5V (CẠNH XUỐNG vi sai)                                  │                 │
                  |          └───────────────────────────────────────────────────────────┴─────────────────┴────
V_DIFF (H - L)    | 0.0V ────┐2.0V (CẠNH LÊN vi sai)                                     │                 │
                  |          └───────────────────────────────────────────────────────────┴─────────────────┴────
──────────────────┼──────────┼───────────────────────────┼───────────────────────────────┼─────────────────┼────
CHÂN RX (VÀO MCU) | 3.3V ────┐                                                           │                 │
(PB8 - CAN1_RX)   | (Logic 1)│ 0.0V (Logic 0 - Dominant)  <=== ĐÂY LÀ CẠNH XUỐNG (FALLING)│                 │
                  |          └───────────────────────────────────────────────────────────┴─────────────────┴────
Trục t_q          |  t_q 01  | t_q 02  . . . . .  t_q 08 | t_q 09   . . . . . .   t_q 16 | t_q 17   t_q 18 │
                  |▲         |                           |                               ▲                 │
                  |CẠNH RƠI  |                           |                               SAMPLE POINT      │
                  |CHUẨN ĐÂY!|                           |                               (Chốt mẫu t_q 16) │

========================================================================================================================
KỊCH BẢN 2: CẠNH ĐẾN TRỄ (LATE EDGE: Phase Error e > 0) ➔ PHẦN CỨNG KÉO DÀI Phase_Seg1 THÊM SJW
========================================================================================================================
Nguyên nhân: Bên phát chạy chậm hơn bên nhận hoặc trễ truyền dẫn cáp làm cạnh tới muộn hơn Sync_Seg (rơi vào Prop_Seg).
Hành động:   bxCAN tự động nới rộng Phase_Seg1 thêm một khoảng dãn Δt = min(e, SJW) = +1 t_q.
Hệ quả:      Đẩy lùi Sample Point về sau thêm 1 t_q, tránh lấy mẫu trúng lúc sườn điện áp đang chuyển pha!

                  | Sync_Seg |          Prop_Seg         |      Phase_Seg1 GỐC   | KÉO DÀI |   Phase_Seg2    |
Số lượng t_q      | (1 t_q)  |          (7 t_q)          |         (8 t_q)       |+SJW(1tq)|     (2 t_q)     |
──────────────────┼──────────┼───────────────────────────┼───────────────────────┼─────────┼─────────────────┼────
CHÂN RX (VÀO MCU) | 3.3V ────────────┐                   │                       │         │                 │
(Đến trễ!)        |                  │ 0.0V (Dominant)   │                       │         │                 │
                  |                  └───────────────────┴───────────────────────┴─────────┴─────────────────┴────
Trục t_q          |  t_q 01  | t_q 02│. . . . . . t_q 08 | t_q 09 . . . . t_q 16 │ t_q 17  │ t_q 18   t_q 19 │
                  |          |▲      |                   |                       |         ▲                 │
                  |          |CẠNH BỊ| TRỄ PHA (e > 0)   |                       |         SAMPLE POINT MỚI  │
                  |          |DỜI VÀO| Prop_Seg          |                       |         (Dời ra t_q 17!)  │

========================================================================================================================
KỊCH BẢN 3: CẠNH ĐẾN SỚM (EARLY EDGE: Phase Error e < 0) ➔ PHẦN CỨNG CẮT NGẮN Phase_Seg2 ĐI SJW
========================================================================================================================
Nguyên nhân: Bên phát chạy nhanh hơn bên nhận, cạnh của bit mới ập đến khi bit cũ chưa kịp kết thúc (rơi vào Phase_Seg2).
Hành động:   bxCAN tự động gọt bớt Phase_Seg2 đi một khoảng co Δt = min(|e|, SJW) = -1 t_q.
Hệ quả:      Kết thúc bit hiện tại sớm hơn, ép phân đoạn Sync_Seg của bit tiếp theo bắt nhịp ngay lập tức với cạnh sớm!

                  | Sync_Seg |          Prop_Seg         |          Phase_Seg1           |Phase_Seg2| BỊ CẮT BỚT! │
Số lượng t_q      | (1 t_q)  |          (7 t_q)          |            (8 t_q)            | (1 t_q)  | [-SJW: 1tq] │
──────────────────┼──────────┼───────────────────────────┼───────────────────────────────┼──────────┼─────────────┼────
CHÂN RX (VÀO MCU) | 3.3V ────────────────────────────────────────────────────────────────┐          │ 0.0V (Cạnh  │
(Đến sớm!)        |                                                                      │          │ của bit mới │
                  |                                                                      └──────────┴─────────────┴────
Trục t_q          |  t_q 01  | t_q 02  . . . . .  t_q 08 | t_q 09   . . . . . .   t_q 16 │  t_q 17  │(Cắt t_q 18, │
                  |          |                           |                               ▲          │ép Sync_Seg  │
                  |          |                           |                               SAMPLE PT  │bit mới ngay)│
========================================================================================================================
```

---

## 1.4. Cơ chế 28 Filter Banks & Phân Quyền CAN1 Master

Hệ thống có **28 Filter Banks (từ Bank 0 đến Bank 27)** dùng để lọc phần cứng các Identifier không mong muốn, giải phóng $100\%$ tải CPU:

```text
 ┌────────────────────────────────────────────────────────────────────────────────────────┐
 │                      28 FILTER BANKS (Quản lý tập trung bởi CAN1)                      │
 ├────────────────────────────────────────┬───────────────────────────────────────────────┤
 │ Filter Banks dành cho CAN1             │ Filter Banks dành cho CAN2                    │
 │ Bank 0 ──► Bank (CAN2SB - 1)           │ Bank CAN2SB ──► Bank 27                       │
 └────────────────────────────────────────┴───────────────────────────────────────────────┘
```

> ⚠️ **BẪY PHẦN CỨNG CAN1 MASTER:** Khối **CAN1 đóng vai trò là Master** quản lý toàn bộ 28 Filter Banks. Thanh ghi phân định ranh giới `CAN1->FMR` (trường `CAN2SB[5:0]`) quyết định Bank nào thuộc về CAN1 và Bank nào thuộc về CAN2. **KỂ CẢ KHI DỰ ÁN CHỈ DÙNG CAN2, BẮT BUỘC PHẢI CẤP CLOCK CHO CAN1 VÀ CẤU HÌNH FILTER TRÊN CAN1!**

### Hai chế độ lọc chính của Filter Bank:
1. **Identifier Mask Mode (Chế độ Mặt nạ):**
   * Sử dụng 2 thanh ghi: **Filter Register (ID mong muốn)** và **Mask Register (Mặt nạ kiểm tra)**.
   * Tại các vị trí bit trong Mask bằng `1`: Bit trên ID của gói tin nhận được **bắt buộc phải khớp 100%** với Filter ID.
   * Tại các vị trí bit trong Mask bằng `0`: Phần cứng **bỏ qua không kiểm tra** (Don't care - chấp nhận cả 0 lẫn 1).
   * *Ứng dụng:* Dùng để lọc cả một dải ID (ví dụ nhận tất cả các ID từ `0x700` đến `0x70F`).
2. **Identifier List Mode (Chế độ Danh sách):**
   * Cả 2 thanh ghi đều dùng làm ID mong muốn.
   * Gói tin nhận được phải có ID khớp chính xác với 1 trong 2 ID trong danh sách.

```mermaid
graph TD
    A["Frame CAN trên Bus tới (ID: 0x123)"] --> B{"Chế độ Filter Scale?"}
    B -->|"32-bit Scale (FS1R=1)"| C{"Chế độ Filter Mode?"}
    B -->|"16-bit Scale (FS1R=0)"| D["Lọc được 4 Standard IDs"]
    C -->|"Mask Mode (FM1R=0)"| E["FR1: ID Mong muốn<br>FR2: MASK (1=Check, 0=Don't Care)"]
    C -->|"Identifier List Mode (FM1R=1)"| F["FR1: ID Khớp 100% (Ví dụ 0x100)<br>FR2: ID Khớp 100% (Ví dụ 0x200)"]
    E --> G{"Kết quả Khớp?"}
    F --> G
    G -->|Đúng| H["Đẩy vào Receive FIFO 0 hoặc FIFO 1 (theo FFA1R)"]
    G -->|Sai| I["Phần cứng tự động HỦY FRAME (Zero CPU Overhead!)"]
```

---

## 1.5. Cơ chế Quản lý Lỗi & Automotive Bus-Off Recovery State Machine

Giao thức CAN tích hợp mạch giám sát lỗi phần cứng qua 2 bộ đếm: **TEC (Transmit Error Counter)** và **REC (Receive Error Counter)**:

```text
                     TEC <= 127 && REC <= 127
                   ┌──────────────────────────┐
                   │       ERROR ACTIVE       │  <=== Trạng thái bình thường,
                   │ (Tham gia gửi/nhận cờ lỗi│       phát Active Error Flag (6 bit 0)
                   └────────────┬─────────────┘
                                │
                    TEC > 127 hoặc REC > 127
                                ▼
                   ┌──────────────────────────┐
                   │       ERROR PASSIVE      │  <=== Bị nghi ngờ lỗi đường truyền,
                   │ (Chỉ phát Passive Flag)  │       chỉ được phát Passive Error (6 bit 1)
                   └────────────┬─────────────┘
                                │
                            TEC > 255
                                ▼
                   ┌──────────────────────────┐
                   │         BUS-OFF          │  <=== BỊ CÁCH LY KHỎI BUS HOÀN TOÀN!
                   │ (Ngắt kết nối ngõ ra TX) │       Không thể gửi hay nhận dữ liệu.
                   └────────────┬─────────────┘
                                │
                    Phục hồi: Đếm đủ 128 lần xuất hiện của 11 bit Recessive (1)
                                ▼
                   Trở lại trạng thái ERROR ACTIVE
```

* **Chế độ Tự động Phục hồi (Automatic Bus-Off Management - ABOM):**
  * Nếu bit `ABOM = 1` trong `CAN_MCR`: Khi rơi vào Bus-Off, phần cứng tự động theo dõi bus. Ngay khi đếm đủ 128 lần chuỗi 11-bit recessive, phần cứng tự động thoát Bus-Off và kéo TEC/REC về 0.
  * Nếu bit `ABOM = 0` (Thủ công bằng phần mềm): CPU phải tự xóa chế độ Init để khởi tạo lại CAN Controller.

### Sơ đồ Tuần tự Truyền Frame CAN (Transmitting Sequence):
```mermaid
sequenceDiagram
    autonumber
    actor App as Application Layer
    participant CAN as bxCAN Hardware
    participant Bus as CAN Bus Physical Line

    App->>CAN: Kiểm tra Mailbox rảnh: (CAN1->TSR & (TME0 | TME1 | TME2))
    Note over CAN: Chọn ví dụ Mailbox 0 đang rảnh (TME0 == 1)
    App->>CAN: Ghi ID và IDE/RTR vào CAN1->sTxMailBox[0].TIR
    App->>CAN: Ghi độ dài DLC (ví dụ: 8 bytes) vào CAN1->sTxMailBox[0].TDTR
    App->>CAN: Ghi 4 bytes đầu vào TDLR, 4 bytes sau vào TDHR
    App->>CAN: Kích hoạt truyền: CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ
    CAN->>Bus: Trọng tài bus (Arbitration) và phát frame
    Bus-->>CAN: Nhận ACK từ node khác trên mạng
    CAN->>CAN: Bật cờ CAN_TSR_TXOK0 = 1 và CAN_TSR_RQCP0 = 1
```

---

# 📑 BƯỚC 2: THỰC CHIẾN & TRA CỨU RM0385 / DATASHEET (SETUP & LOOKUP)

## 2.1. Bản đồ Địa chỉ Base Address & Vector Ngắt bxCAN

Tra cứu RM0385 *Chapter 2: Memory map $\rightarrow$ Table 1* & *Chapter 10: Interrupt vector table*:

| Ngoại vi | Bus | Base Address | Offset | Địa chỉ tuyệt đối | IRQ Number | Hàm xử lý ngắt (ISR) |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **`CAN1`** | APB1 | `0x4000 6400` | `0x0000` | `0x4000 6400` | - | Quản lý khối bxCAN 1 & Filter Banks |
| **`CAN1_TX`** | - | - | - | - | `IRQn = 19` | `CAN1_TX_IRQHandler()` |
| **`CAN1_RX0`**| - | - | - | - | `IRQn = 20` | `CAN1_RX0_IRQHandler()` (Nhận dữ liệu FIFO 0) |
| **`CAN1_RX1`**| - | - | - | - | `IRQn = 21` | `CAN1_RX1_IRQHandler()` (Nhận dữ liệu FIFO 1) |
| **`CAN1_SCE`**| - | - | - | - | `IRQn = 22` | `CAN1_SCE_IRQHandler()` (Báo lỗi & Bus-Off) |
| **`CAN2`** | APB1 | `0x4000 6800` | `0x0000` | `0x4000 6800` | - | Khối bxCAN 2 |

---

## 2.2. Ghép kênh chân GPIO (Pin Multiplexing cho CAN1 trên Discovery)

Tra cứu Datasheet DS10610 *Table 11: Alternate function mapping*:
* **`CAN1_RX`:** Chân **`PB8`** $\rightarrow$ Mã **`AF9`** (Alternate Function 9).
* **`CAN1_TX`:** Chân **`PB9`** $\rightarrow$ Mã **`AF9`** (Alternate Function 9).

```text
PB8  --> AFRH8[3:0] = 1001b (AF9) -> CAN1_RX
PB9  --> AFRH9[3:0] = 1001b (AF9) -> CAN1_TX
```

---

## 2.3. Bảng Tra cứu Thanh ghi & Bitmask Chi tiết của bxCAN

Tra cứu RM0385 *Chapter 31: Controller area network (bxCAN)*:

| Thanh ghi | Bit / Trường | Giá trị gán | Ý nghĩa kỹ thuật phần cứng |
| :--- | :--- | :---: | :--- |
| **`CAN_MCR`** | `INRQ` (Bit 0) | `1`b | Yêu cầu vào chế độ Khởi tạo (Initialization Mode). |
| | `SLEEP` (Bit 1) | `0`b | Thoát khỏi chế độ Tiết kiệm điện Sleep Mode. |
| | `ABOM` (Bit 6) | `1`b | Bật tự động phục hồi khỏi trạng thái lỗi Bus-Off. |
| | `TXFP` (Bit 2) | `1`b | Ưu tiên gửi theo thứ tự thời gian nạp (FIFO Priority). |
| **`CAN_MSR`** | `INAK` (Bit 0) | RO Polling | Chờ phần cứng xác nhận đã vào chế độ Khởi tạo (`INAK = 1`). |
| **`CAN_BTR`** | `BRP[9:0]` | `5`d | Prescaler chia 6 ($54\text{MHz} / 6 = 9\text{MHz} \implies t_q = 111.11\text{ns}$). |
| | `TS1[3:0]` | `14`d (`0xE`) | Time Segment 1 $= 15\,t_q$. |
| | `TS2[2:0]` | `1`d (`0x1`) | Time Segment 2 $= 2\,t_q$. |
| | `SJW[1:0]` | `0`d (`0x0`) | Resynchronization Jump Width $= 1\,t_q$. |
| **`CAN_FMR`** | `FINIT` (Bit 0) | `1`b | Mở khóa cấu hình bộ lọc Filter Initialization. |
| | `CAN2SB[5:0]` | `14`d | Ranh giới chia sẻ: Bank 0-13 cho CAN1, Bank 14-27 cho CAN2. |
| **`CAN_FA1R`** | `FACT0` (Bit 0) | `1`b | Kích hoạt (Activate) Filter Bank 0. |
| **`CAN_TSR`** | `TME0` (Bit 26) | RO Flag | Cờ báo Transmit Mailbox 0 đang trống (`TME0 = 1`). |
| | `RQCP0` (Bit 0) | `rc_w1` | Cờ báo yêu cầu truyền Mailbox 0 đã hoàn tất. |
| **`CAN_RF0R`** | `FMP0[1:0]` (Bit 1:0)| RO Counter | Số lượng bản tin đang nằm trong FIFO 0 ($0 \rightarrow 3$). |
| | `RFOM0` (Bit 5) | `rs` (`1`b) | **Release FIFO 0 Output Mailbox:** Giải phóng bản tin vừa đọc. |

---

# 💻 BƯỚC 3: GÕ CODE & MỔ XẺ BUG PHẦN CỨNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 3

```text
drivers/
├── inc/
│   ├── Reg.h          <-- Khai báo Base Address & Struct CAN_TypeDef
│   └── can.h          <-- Khai báo Struct CAN_Frame_t, Enum & API Prototypes
└── src/
    └── can.c          <-- Triển khai cấu hình GPIO AF9, bxCAN 500kbps, Filter & Ngắt ISR
src/
└── main.c             <-- Gửi Frame định kỳ & Xử lý Frame nhận được
```

---

### 📂 KHỐI 1: FILE HEADER GIAO DIỆN [ `drivers/inc/can.h` ]

#### TODO 1 [File: `drivers/inc/can.h`]: Định nghĩa Cấu trúc Bản tin CAN Frame & Prototypes
```c
#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Cấu trúc bản tin CAN chuẩn 2.0A/B
 */
typedef struct {
    uint32_t id;       /* 11-bit Standard ID hoặc 29-bit Extended ID */
    uint8_t  dlc;      /* Độ dài dữ liệu Data Length Code (0 đến 8 bytes) */
    uint8_t  data[8];   /* Mảng 8 bytes dữ liệu */
    bool     is_ext;   /* true = Extended ID, false = Standard ID */
    bool     is_rtr;   /* true = Remote Transmission Request, false = Data Frame */
} CAN_Frame_t;

/* Khởi tạo ngoại vi CAN1 ở tốc độ 500 kbps (Sample Point ~87.5%) */
void CAN1_Init(void);

/* Cấu hình Filter Bank nhận dải ID mong muốn */
void CAN1_Filter_Config(uint16_t filter_id, uint16_t filter_mask);

/* Gửi một bản tin CAN qua Mailbox trống */
bool CAN1_Transmit(const CAN_Frame_t *pFrame);

/* Đọc bản tin từ FIFO nhận */
bool CAN1_Receive(CAN_Frame_t *pFrame);

#endif /* CAN_H */
```

---

### 📂 KHỐI 2: FILE SOURCE DRIVER [ `drivers/src/can.c` ]

#### TODO 2 [File: `drivers/src/can.c`]: Cấu hình GPIO AF9 cho PB8 (RX) và PB9 (TX)
```c
#include "can.h"
#include "Reg.h"

static void CAN1_GPIO_Config(void)
{
    /* 1. Cấp clock cho GPIOB */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* 2. Cài đặt PB8 và PB9 sang Alternate Function Mode (10b) */
    GPIOB->MODER &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->MODER |=  ((2U << (8 * 2)) | (2U << (9 * 2)));

    /* 3. Đặt Output Speed High cho PB9 (TX) */
    GPIOB->OSPEEDR |= ((3U << (8 * 2)) | (3U << (9 * 2)));

    /* 4. Gán Alternate Function AF9 (1001b) vào AFRH8 và AFRH9 */
    GPIOB->AFR[1] &= ~((0xFU << ((8 - 8) * 4)) | (0xFU << ((9 - 8) * 4)));
    GPIOB->AFR[1] |=  ((9U   << ((8 - 8) * 4)) | (9U   << ((9 - 8) * 4)));
}
```

#### TODO 3 [File: `drivers/src/can.c`]: Khởi tạo bxCAN Core & Cài đặt Bit Timing 500kbps
```c
void CAN1_Init(void)
{
    /* 1. Cấu hình chân GPIO AF9 */
    CAN1_GPIO_Config();

    /* 2. Cấp clock cho CAN1 (Bus APB1) */
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* 3. Thoát Sleep Mode và Yêu cầu vào Initialization Mode */
    CAN1->MCR &= ~CAN_MCR_SLEEP;        /* Xóa cờ SLEEP */
    CAN1->MCR |=  CAN_MCR_INRQ;         /* Đặt cờ INRQ = 1 */
    while (!(CAN1->MSR & CAN_MSR_INAK)); /* Chờ phần cứng xác nhận INAK = 1 */

    /* 4. Cấu hình MCR: Bật ABOM (Auto Bus-Off Recovery), FIFO Priority */
    CAN1->MCR |= (CAN_MCR_ABOM | CAN_MCR_TXFP);

    /* 5. Cấu hình Bit Timing CAN_BTR: Baudrate 500kbps at PCLK1 = 54MHz */
    /* BRP = 6 (nạp 5), TS1 = 15 (nạp 14 = 0xE), TS2 = 2 (nạp 1), SJW = 1 (nạp 0) */
    /* NOTE: Nếu thực hành trên Kit đơn lẻ (STM32F746G-DISCO không có Transceiver IC ngoài), */
    /* hãy bật thêm bit CAN_BTR_LBKM (Bit 30: Loopback Mode) để tự lặp nội bộ kiểm tra code. */
    CAN1->BTR = (5U << 0)  |  /* BRP[9:0] = 5 */
                (14U << 16)|  /* TS1[3:0] = 14 */
                (1U << 20) |  /* TS2[2:0] = 1 */
                (0U << 24) |  /* SJW[1:0] = 0 */
                CAN_BTR_LBKM; /* Bật Loopback Mode để Self-Test không cần Transceiver ngoài */

    /* 6. Cấu hình Filter mặc định (Accept All để test ban đầu) */
    CAN1_Filter_Config(0x0000, 0x0000);

    /* 7. Thoát Initialization Mode để chuyển sang Normal Mode */
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK); /* Chờ INAK = 0 */

    /* 8. Kích hoạt Ngắt nhận FIFO 0 trong NVIC */
    CAN1->IER |= CAN_IER_FMPIE0; /* Bật ngắt FIFO 0 Message Pending */
    NVIC_SetPriority(CAN1_RX0_IRQn, 4);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
}
```

#### TODO 4 [File: `drivers/src/can.c`]: Cấu hình 28 Filter Banks (Mask Mode)
```c
void CAN1_Filter_Config(uint16_t filter_id, uint16_t filter_mask)
{
    /* 1. Mở khóa cấu hình Filter qua bit FINIT trong CAN1->FMR */
    CAN1->FMR |= CAN_FMR_FINIT;

    /* 2. Tắt Filter Bank 0 trước khi sửa */
    CAN1->FA1R &= ~(1U << 0);

    /* 3. Chọn chế độ 32-bit (FSC0 = 1) và Chế độ Mask Mode (FBM0 = 0) */
    CAN1->FS1R |=  (1U << 0); /* 32-bit scale */
    CAN1->FM1R &= ~(1U << 0); /* Id/Mask mode */

    /* 4. Gán Filter ID và Mask ID vào Register 0 của Bank 0 */
    /* Đối với Standard ID 11-bit: ID dịch sang trái 21 bit */
    CAN1->sFilterRegister[0].FR1 = ((uint32_t)filter_id   << 21);
    CAN1->sFilterRegister[0].FR2 = ((uint32_t)filter_mask << 21);

    /* 5. Gán Filter Bank 0 trỏ vào FIFO 0 */
    CAN1->FFA1R &= ~(1U << 0); /* 0 = FIFO 0 */

    /* 6. Kích hoạt Filter Bank 0 */
    CAN1->FA1R |= (1U << 0);

    /* 7. Khóa lại cấu hình Filter */
    CAN1->FMR &= ~CAN_FMR_FINIT;
}
```

#### TODO 5 [File: `drivers/src/can.c`]: Hàm Truyền Bản Tin (Transmit Mailbox)
```c
bool CAN1_Transmit(const CAN_Frame_t *pFrame)
{
    uint8_t mailbox_index;

    /* 1. Kiểm tra xem có Transmit Mailbox nào đang trống không */
    if (CAN1->TSR & CAN_TSR_TME0) {
        mailbox_index = 0;
    } else if (CAN1->TSR & CAN_TSR_TME1) {
        mailbox_index = 1;
    } else if (CAN1->TSR & CAN_TSR_TME2) {
        mailbox_index = 2;
    } else {
        return false; /* Cả 3 Mailboxes đều đang bận */
    }

    /* 2. Cài đặt Identifier vào thanh ghi TIR */
    if (pFrame->is_ext) {
        CAN1->sTxMailBox[mailbox_index].TIR = (pFrame->id << 3) | (1U << 2); /* IDE = 1 */
    } else {
        CAN1->sTxMailBox[mailbox_index].TIR = (pFrame->id << 21);            /* IDE = 0 */
    }

    /* 3. Cài đặt DLC */
    CAN1->sTxMailBox[mailbox_index].TDTR &= ~(0xFU);
    CAN1->sTxMailBox[mailbox_index].TDTR |=  (pFrame->dlc & 0xFU);

    /* 4. Nạp 8 bytes dữ liệu vào TDLR (Data Low) và TDHR (Data High) */
    CAN1->sTxMailBox[mailbox_index].TDLR = ((uint32_t)pFrame->data[0] << 0)  |
                                           ((uint32_t)pFrame->data[1] << 8)  |
                                           ((uint32_t)pFrame->data[2] << 16) |
                                           ((uint32_t)pFrame->data[3] << 24);

    CAN1->sTxMailBox[mailbox_index].TDHR = ((uint32_t)pFrame->data[4] << 0)  |
                                           ((uint32_t)pFrame->data[5] << 8)  |
                                           ((uint32_t)pFrame->data[6] << 16) |
                                           ((uint32_t)pFrame->data[7] << 24);

    /* 5. Kích hoạt yêu cầu truyền (Transmit Request TXRQ) */
    CAN1->sTxMailBox[mailbox_index].TIR |= (1U << 0);

    return true;
}
```

#### TODO 6 [File: `drivers/src/can.c`]: Trình Phục Vụ Ngắt Nhận `CAN1_RX0_IRQHandler()`
```c
volatile uint32_t can_rx_msg_count = 0;
CAN_Frame_t latest_rx_frame;

void CAN1_RX0_IRQHandler(void)
{
    /* Kiểm tra cờ FMP0 (FIFO 0 Message Pending > 0) */
    if ((CAN1->RF0R & CAN_RF0R_FMP0_Msk) != 0) {
        /* 1. Đọc Identifier */
        if (CAN1->sFIFOMailBox[0].RIR & (1U << 2)) {
            latest_rx_frame.is_ext = true;
            latest_rx_frame.id = (CAN1->sFIFOMailBox[0].RIR >> 3);
        } else {
            latest_rx_frame.is_ext = false;
            latest_rx_frame.id = (CAN1->sFIFOMailBox[0].RIR >> 21);
        }

        /* 2. Đọc DLC */
        latest_rx_frame.dlc = (uint8_t)(CAN1->sFIFOMailBox[0].RDTR & 0xFU);

        /* 3. Đọc dữ liệu từ RDLR và RDHR */
        uint32_t low_word  = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t high_word = CAN1->sFIFOMailBox[0].RDHR;

        latest_rx_frame.data[0] = (uint8_t)(low_word >> 0);
        latest_rx_frame.data[1] = (uint8_t)(low_word >> 8);
        latest_rx_frame.data[2] = (uint8_t)(low_word >> 16);
        latest_rx_frame.data[3] = (uint8_t)(low_word >> 24);

        latest_rx_frame.data[4] = (uint8_t)(high_word >> 0);
        latest_rx_frame.data[5] = (uint8_t)(high_word >> 8);
        latest_rx_frame.data[6] = (uint8_t)(high_word >> 16);
        latest_rx_frame.data[7] = (uint8_t)(high_word >> 24);

        can_rx_msg_count++;

        /* 4. GIẢI PHÓNG FIFO 0 MAILBOX (Ghi gán trực tiếp bit RFOM0 = 1) */
        CAN1->RF0R = CAN_RF0R_RFOM0;
    }
}
```

---

### 📂 KHỐI 3: FILE MAIN CHÍNH [ `src/main.c` ]

#### TODO 7 [File: `src/main.c`]: Khởi chạy Hệ thống & Phát/Nhận Bản tin CAN
```c
#include "Sys_Clock.h"
#include "can.h"

int main(void)
{
    /* 1. Khởi động nhịp xung 216MHz Over-drive */
    System_Clock_Init();

    /* 2. Khởi tạo bxCAN 500kbps */
    CAN1_Init();

    /* 3. Chuẩn bị bản tin gửi mẫu */
    CAN_Frame_t tx_frame;
    tx_frame.id = 0x123;
    tx_frame.dlc = 4;
    tx_frame.is_ext = false;
    tx_frame.is_rtr = false;
    tx_frame.data[0] = 0xDE;
    tx_frame.data[1] = 0xAD;
    tx_frame.data[2] = 0xBE;
    tx_frame.data[3] = 0xEF;

    while (1) {
        /* Gửi bản tin CAN mỗi chu kỳ */
        CAN1_Transmit(&tx_frame);

        /* Delay vòng lặp */
        for (volatile int i = 0; i < 1000000; i++);
    }
}
```

---

## 3.2. Mổ xẻ 5 Bug Phần Cứng "Kinh Điển" trong Ngày 3

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 LỖI KINH ĐIỂN VÀ CÁCH KHẮC PHỤC                                │
├───────────────────────────┬──────────────────────────────────────┬───────────────────────────────┤
│ 1. Quên Clock CAN1 khi    │ CAN2 phụ thuộc 28 Filter Banks do    │ BẮT BUỘC cấp clock CAN1 và    │
│    chỉ dùng CAN2          │ CAN1 Master quản lý. Nếu không bật   │ cấu hình CAN1->FMR kể cả khi  │
│                           │ CAN1, CAN2 không nhận được bản tin.  │ chỉ dùng ngoại vi CAN2.       │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 2. Dùng |= trên CAN_RF0R  │ Bit RFOM0 là dạng Write-1-to-Clear/  │ BẮT BUỘC gán trực tiếp:       │
│    (W1C Register)         │ Set. Dùng |= làm đọc và ghi đè nhầm  │ CAN1->RF0R = CAN_RF0R_RFOM0;  │
│                           │ các cờ FOVR0/FULL0 của FIFO.         │ để giải phóng mailbox.        │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 3. Lệch vị trí dịch bit   │ Trong thanh ghi TIR/RIR, Standard ID │ Luôn dịch Standard ID sang    │
│    Standard ID (11-bit)   │ nằm từ bit [31:21]. Nếu dịch nhầm    │ trái 21 bit:                  │
│                           │ sang bit 0, ID sẽ bị sai lệch hoàn   │ CAN1->TIR = (id << 21);       │
│                           │ toàn trên bus.                       │                               │
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 4. Thiếu CAN Transceiver  │ STM32F746-Discovery không có chip    │ Bắt buộc gắn thêm module      │
│    ngoài trên board       │ Transceiver trên board. Nối dây trực │ ngoài (SN65HVD230) vào chân   │
│                           │ tiếp MCU vào bus sẽ không thể chạy.  │ PB8/PB9 để tạo điện áp vi sai.│
├───────────────────────────┼──────────────────────────────────────┼───────────────────────────────┤
│ 5. Bỏ qua cấu hình Filter │ Mặc định sau Reset, mọi Filter đều   │ Bắt buộc mở Filter Bank 0     │
│    (Drop toàn bộ gói tin) │ bị tắt (Disabled). Nếu không cấu hình│ ở chế độ Accept-All hoặc đúng │
│                           │ Filter, phần cứng sẽ vứt bỏ 100% frame│ ID mong muốn để mở cổng nhận. │
└───────────────────────────┴──────────────────────────────────────┴───────────────────────────────┘
```

---

# 🎙️ BƯỚC 4: PHỎNG VẤN & THIẾT KẾ NÂNG CAO (INTERVIEW PREP)

## 4.1. Bộ 5 Câu Hỏi Phỏng Vấn Chuyên Sâu (Top 5 Deep-Dive Questions)

### ❓ Câu 1: Cơ chế Phân định Quyền ưu tiên (Arbitration) trên CAN Bus hoạt động dựa trên nguyên lý vật lý nào?
* **Trả lời:** CAN Bus sử dụng cấu trúc dây nối logic **Wired-AND** thông qua mạch vi sai. Mức logic 0 được định nghĩa là **Dominant (Thống trị)**, mức logic 1 là **Recessive (Ẩn)**. Khi nhiều node đồng thời truyền dữ liệu, mỗi node vừa phát vừa lắng nghe đường truyền. Nếu node A phát mức 1 nhưng nghe thấy bus bị kéo xuống mức 0 (do node B có ID nhỏ hơn phát mức 0), node A lập tức dừng phát để nhường bus cho node B mà không làm hỏng dữ liệu trên bus (Non-destructive bitwise arbitration).

---

### ❓ Câu 2: Tại sao khi tính toán Bit Timing cho CAN trên STM32F7, điểm lấy mẫu (Sample Point) lại được khuyến nghị đặt ở $87.5\%$ thay vì $50\%$?
* **Trả lời:** Chuẩn công nghiệp ô tô CiA 301 và ISO 11898-1 khuyến nghị Sample Point trong dải $75\% \sim 90\%$, tối ưu ở $87.5\%$. Lý do: Trong môi trường cáp dài và có độ trễ truyền dẫn (Propagation Delay) của linh kiện Transceiver và cách ly quang, xung điện áp cần thời gian lan truyền trên đường dây. Đặt điểm lấy mẫu muộn ở $87.5\%$ cho phép tối đa hóa đoạn $T_{Prop\_Seg}$, đảm bảo tín hiệu điện áp vi sai đã phản hồi và ổn định hoàn toàn trước khi chip đo mẫu.

---

### ❓ Câu 3: Giải thích mối quan hệ kiến trúc giữa CAN1 và CAN2 trên STM32F7 liên quan đến 28 Filter Banks?
* **Trả lời:** Trên STM32F7, `CAN1` là **Master** và `CAN2` là **Slave**. Toàn bộ 28 Filter Banks đều thuộc khối điều khiển logic của CAN1. Thanh ghi `CAN1->FMR` chứa trường `CAN2SB[5:0]` (CAN2 Start Bank) đóng vai trò làm con trỏ phân chia: các Bank từ $0$ đến $CAN2SB - 1$ được cấp cho CAN1, các Bank từ $CAN2SB$ đến $27$ được cấp cho CAN2. Nếu không cấp xung nhịp APB1 cho CAN1, toàn bộ bộ lọc của CAN2 sẽ bị vô hiệu hóa.

---

### ❓ Câu 4: Phân biệt sự khác nhau giữa Identifier Mask Mode và Identifier List Mode trong Filter Bank?
* **Trả lời:**
  * **Identifier Mask Mode:** Dùng 1 thanh ghi ID và 1 thanh ghi Mặt nạ (Mask). Bit nào trong Mask bằng 1 thì bit tương ứng của frame nhận vào phải trùng khớp $100\%$ với Filter ID; bit nào bằng 0 thì không quan tâm (don't care). Cho phép lọc cả một dải ID lớn.
  * **Identifier List Mode:** Cả 2 thanh ghi đều dùng làm ID tuyệt đối. Gói tin nhận vào phải có ID khớp chính xác với 1 trong 2 giá trị đã chỉ định. Dùng khi hệ thống chỉ cần lắng nghe một vài ID cụ thể.

---

### ❓ Câu 5: Trình bày cơ chế Bus-Off trong mạng CAN ô tô? Làm thế nào để hệ thống tự phục hồi mà không cần can thiệp Reset chip?
* **Trả lời:** Khi một node gặp sự cố phần cứng hoặc nhiễu đường truyền làm bộ đếm lỗi truyền $TEC > 255$, phần cứng CAN tự động ngắt kết nối ngõ ra TX để cách ly node lỗi (Trạng thái **Bus-Off**), bảo vệ toàn bộ mạng không bị tê liệt. Để phục hồi:
  1. Nếu bật bit **`ABOM = 1` (Automatic Bus-Off Management)** trong `CAN_MCR`: Sau khi phát hiện bus yên lặng (đếm đủ 128 lần chuỗi 11-bit recessive), phần cứng bxCAN tự động kéo $TEC = 0, REC = 0$ và tái hòa nhập mạng về trạng thái **Error Active**.
  2. Nếu `ABOM = 0`: Phần mềm phải bắt ngắt lỗi `CAN1_SCE_IRQHandler`, yêu cầu vào lại chế độ Init rồi thoát ra để reset bộ đếm lỗi.

---

## 4.2. Kịch bản Trả lời Phỏng vấn 60 Giây (Elevator Pitch)

> *"Trong thiết kế giao tiếp mạng ô tô trên STM32F746, em trực tiếp phát triển driver Bare-metal cho khối **bxCAN** với tốc độ **500 kbps** chuẩn CiA 301. Em tính toán chính xác $f_{PCLK1}=54\text{MHz}$ với $BRP=6, TS1=15, TS2=2$ để đạt điểm lấy mẫu **Sample Point 88.9%**, đảm bảo khả năng chống nhiễu tối đa trên bus vi sai. Em làm chủ cơ chế chia sẻ **28 Filter Banks** do CAN1 Master quản lý, cấu hình chế độ **Identifier Mask Mode 32-bit** để lọc phần cứng các gói tin mong muốn với 0% CPU Load. Em xây dựng trình phục vụ ngắt nhận **`CAN1_RX0_IRQHandler`** đọc dữ liệu từ FIFO 3 tầng và giải phóng mailbox bằng lệnh gán trực tiếp trên thanh ghi W1C `CAN_RF0R`. Đồng thời, em tích hợp cơ chế tự phục hồi **Automotive Bus-Off Recovery (ABOM)** để bảo vệ hệ thống không bị cô lập vĩnh viễn khi mạng CAN xảy ra sự cố."*
