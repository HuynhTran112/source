# 🏆 [NGÀY 14] CẨM NANG TOÀN DIỆN TỔNG KẾT: ĐÓNG GÓI PORTFOLIO, VIẾT CV KỸ THUẬT & BỘ 20 CÂU HỎI PHỎNG VẤN SÁT HẠCH LÕI BARE-METAL & RTOS
## Lộ trình 4 Bước: Chiến Lược Định Vị CV ➔ Sơ Đồ Toàn Cảnh Dự Án ➔ Đóng Gói GitHub ➔ Bộ 20 Câu Hỏi Sát Hạch

> **Mục tiêu:** Hoàn thiện chặng đường 14 ngày chinh phục hệ thống nhúng cao cấp trên STM32F746 (ARM Cortex-M7): Đóng gói toàn bộ mã nguồn dự án Gateway đa kiến trúc (**Dual-Architecture: Bare-Metal + Zephyr RTOS + LVGL**) thành một **Portfolio chuẩn kỹ sư chuyên nghiệp**, xây dựng 2 kịch bản trình bày CV riêng biệt nhắm trúng **Job Automotive** và **Job Firmware thuần**, trang bị bộ **Top 20 Câu Hỏi Phỏng Vấn Sát Hạch Chuyên Sâu (Master Interview Cheat Sheet)** bao quát từ thanh ghi vi mô đến hệ điều hành đa luồng, và hoàn thiện kỹ năng thuyết trình dự án 2 phút theo chuẩn phương pháp STAR.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào kỹ năng thực chiến kỹ thuật, cấu trúc thư mục tiêu chuẩn, thuật ngữ chuyên ngành và bộ câu hỏi hỏi vặn sắc bén — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 14                                    │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: ĐỊNH VỊ   │ BƯỚC 2: TOÀN CẢNH │ BƯỚC 3: ĐÓNG GÓI GITHUB    │ BƯỚC 4: SÁT HẠCH PHỎNG VẤN │
│ • Chiến lược 2 cột│ • Sơ đồ Kiến trúc │ • Cấu trúc thư mục chuẩn   │ • Bộ 20 câu hỏi vặn lõi    │
│   Automotive vs   │   Tổng thể C4     │ • Mục "Known Issues &      │   Cortex-M7 & Zephyr RTOS  │
│   Firmware thuần  │ • Dòng chảy dữ    │   Lessons Learned"         │ • Kịch bản 2 phút STAR     │
│ • Từ khóa ghi điểm│   liệu Gateway    │ • Chiến lược Git Commits   │   (Elevator Pitch)         │
│   trong CV        │                   │                            │                            │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

# 🧠 BƯỚC 1: CHIẾN LƯỢC ĐỊNH VỊ CV & NGHỆ THUẬT KỂ CHUYỆN DỰ ÁN (CV POSITIONING)

Cùng một cơ sở mã nguồn và phần cứng STM32F746 bạn đã viết trong 14 ngày, bạn có thể ứng tuyển vào **hai phân khúc công việc khác nhau** bằng cách thay đổi trọng tâm nhấn mạnh trong CV:

| Tiêu chí so sánh | Hướng 1: Ứng tuyển Kỹ sư Ô Tô (Automotive ECU / Gateway) | Hướng 2: Ứng tuyển Kỹ sư Firmware / Embedded C thuần |
| :--- | :--- | :--- |
| **Tiêu đề Dự án trong CV** | **Automotive CAN Gateway & Digital Instrument Cluster** | **Dual-Architecture Embedded Display & Communication System** |
| **Mục tiêu nhấn mạnh** | Độ tin cậy mạng truyền thông xe hơi, an toàn chức năng, chuẩn giao tiếp quốc tế. | Kỹ năng lập trình thanh ghi Bare-metal, kiến trúc đa luồng RTOS, tối ưu hóa bộ nhớ. |
| **Từ khóa "Ghi điểm"** | `CAN Bus`, `Vector DBC`, `AUTOSAR E2E`, `ISO 11898-1 Bus-Off`, `ISO 26262`, `Rolling Counter`. | `ARM Cortex-M7`, `L1 D-Cache Coherency`, `FMC SDRAM`, `DMA2D Chrom-ART`, `Zephyr RTOS`, `IPC`. |
| **Tài liệu chứng minh** | Ma trận giải mã DBC, thuật toán E2E CRC-8, máy trạng thái Bus-Off Recovery. | Bảng đo đạc hiệu năng DWT Cycle Counter, bản đồ căn lề 32-byte Alignment, MPU Stack Guard. |

---

# 🗺️ BƯỚC 2: SƠ ĐỒ TOÀN CẢNH HỆ THỐNG ĐA KIẾN TRÚC (MASTER ARCHITECTURE)

Dưới đây là sơ đồ kiến trúc tổng thể toàn bộ dự án bạn đã hoàn thành qua 14 ngày:

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           DUAL-ARCHITECTURE STM32F746 AUTOMOTIVE GATEWAY                               │
├────────────────────────────────────────────────────┬────────────────────────────────────────────────────┤
│           KIẾN TRÚC 1: BARE-METAL SUBSYSTEM        │          KIẾN TRÚC 2: ZEPHYR RTOS SUBSYSTEM        │
│          (Tối ưu hóa phần cứng tuyệt đối)          │             (Đa nhiệm & Mở rộng quy mô)            │
├────────────────────────────────────────────────────┼────────────────────────────────────────────────────┤
│                                                    │                                                    │
│  [ NGÀY 1: 216MHz Over-Drive & Reset Logging ]     │  [ NGÀY 7: Board Bring-Up & Devicetree Overlay ]   │
│  • PLL M=25, N=432, P=2, VOS Scale 1, 7 WS        │  • pinctrl, app.overlay, MPU Stack Guard           │
│  • Đọc cờ RCC_CSR (RMVF), quản lý Stack/Heap       │  • Deferred Logging qua ST-Link Console            │
│                                                    │                                                    │
│  [ NGÀY 2: UART RX DMA Ring Buffer & D-Cache ]     │  [ NGÀY 8: Zephyr CAN Bus Subsystem & MsgQ ]       │
│  • DMA2 Stream 2 Channel 4 Circular                │  • can_add_rx_filter_msgq() Zero-Lock IPC          │
│  • Căn lề 32B, Invalidate L1 D-Cache, ngắt IDLE    │  • Kéo chân Transceiver STB xuống LOW (PJ5)        │
│                                                    │                                                    │
│  [ NGÀY 3: bxCAN Controller & 28 Filter Banks ]    │  [ NGÀY 9: Zephyr Display & LVGL Cluster GUI ]     │
│  • Bit Timing CiA 301 500kbps (Sample Point 87.5%) │  • Ràng buộc chosen zephyr,display = &ltdc         │
│  • Chia sẻ 28 Filters với CAN1 Master (CAN2SB)     │  • Kim đồng hồ Arc, VDB 20% SRAM, Mutex bảo vệ     │
│                                                    │                                                    │
│  [ NGÀY 4: FMC SDRAM 108MHz & LTDC Display ]       │  [ NGÀY 10: Multi-Threading & Shell CLI ]          │
│  • Chuỗi 5 lệnh JEDEC, Refresh Counter = 1667      │  • Luồng CAN Worker (Prio 4) -> MsgQ -> Model (Prio5)│
│  • Double Buffering VSYNC Reload triệt tiêu Tearing │  • Mutex Priority Inheritance, Zephyr Shell UART  │
│                                                    │                                                    │
│  [ NGÀY 5: DMA2D Chrom-ART & NVIC Matrix ]         │  [ NGÀY 11: Vector DBC Signal Decoding Engine ]    │
│  • Đổ màu R2M, PFC ARGB->RGB565, Line Offset OOR   │  • Zero-Copy Bit Unpacking Intel (@1) vs Motorola (@0)│
│  • NVIC Grouping 4: CAN(1) > UART(2) > GUI(3)      │  • Fixed-point Scaling, Signed Sign Extension      │
│                                                    │                                                    │
│  [ NGÀY 6: Hardware Reliability & Watchdogs ]      │  [ NGÀY 12: Signal Supervision & Bus-Off FSM ]     │
│  • IWDG chạy LSI 32kHz, WWDG cửa sổ APB1           │  • E2E CRC-8 (0x2F), Rolling Counter, Timeout 100ms│
│  • CSS tự động chuyển xung HSI & NMI Failover      │  • Máy trạng thái phục hồi ISO 11898-1 Bus-Off     │
│                                                    │                                                    │
├────────────────────────────────────────────────────┴────────────────────────────────────────────────────┤
│                         [ NGÀY 13: BENCHMARKING & HOST UNIT TESTING ]                                   │
│  • DWT Cycle Counter 4.63ns: Boot Bare-metal (18.4ms) vs Zephyr (142.6ms)                               │
│  • Bộ kiểm thử tự động Unity Framework chạy trên PC Host: 'make test' chạy 100% ca test trong 0.05s    │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2.1. Ma Trận Tra Cứu Tài Liệu Kỹ Thuật Toàn Diện (Master Documentation & Lookup Matrix)

Bảng tổng hợp vị trí tra cứu chuẩn xác xuyên suốt toàn bộ dự án để bạn tự tin giải trình trong bất kỳ buổi phỏng vấn kỹ thuật chuyên sâu nào:

| Ngày | Khối Chức Năng / Ngoại Vi | Tài Liệu Bắt Buộc | Từ Khóa Tra Cứu (`Ctrl + F`) | Vị Trí Chương / Section / Binding Đích |
| :---: | :--- | :--- | :--- | :--- |
| **Day 00** | Nền tảng Thanh ghi & C Struct | `RM0385` | `Register boundary addresses` | Chapter 2 (Memory Map -> Table 1) & Chapter 6 (GPIO). |
| **Day 01** | Clock Tree, PLL & Over-drive | `RM0385` & `DS10610` | `RCC register map`, `Over-drive` | Chapter 5 (RCC Section 5.3) & Chapter 4 (PWR Over-drive). |
| **Day 02** | UART RX DMA & D-Cache Coherency | `RM0385` & `PM0253` | `DMA register map`, `Channel selection` | Chapter 13 (DMA Section 13.3) & PM0253 Chapter 4 (L1-Cache). |
| **Day 03** | bxCAN, Bit Timing & 28 Filters | `RM0385` & `DS10610` | `CAN register map`, `CAN_BTR` | Chapter 31 (bxCAN Section 31.9) & DS10610 (Table 11 Pin AF9). |
| **Day 04** | FMC SDRAM, LTDC & MPU | `RM0385` & `PM0253` | `FMC register map`, `Memory protection unit` | RM0385 Chapter 13, 18 & PM0253 Chapter 4 (Section 4.5 MPU). |
| **Day 05** | DMA2D Chrom-ART & NVIC Matrix | `RM0385` & `PM0253` | `DMA2D register map`, `AIRCR` | RM0385 Chapter 10 & PM0253 Chapter 4 (Section 4.3 NVIC). |
| **Day 06** | IWDG, WWDG, CSS & HardFault | `RM0385` & `PM0253` | `IWDG register map`, `Configurable fault` | RM0385 Chapter 25, 26, 38 & PM0253 Chapter 4 (Section 4.3.9 CFSR). |
| **Day 07** | Zephyr Bring-Up & MPU Guard | `Zephyr SDK` & `DTS` | `CONFIG_MPU_STACK_GUARD`, `gpio-leds` | `zephyr/dts/bindings/gpio/gpio-leds.yaml` & `stm32f746g_disco.dts`. |
| **Day 08** | Zephyr CAN Driver & Pinctrl | `Zephyr SDK` & `DTS` | `CONFIG_CAN`, `st,stm32-can` | `zephyr/dts/bindings/can/st,stm32-can.yaml` & `pinctrl.dtsi`. |
| **Day 09** | Zephyr Display LTDC & LVGL | `Zephyr SDK` & `LVGL` | `CONFIG_LVGL`, `st,stm32-ltdc` | `zephyr/dts/bindings/display/st,stm32-ltdc.yaml` & `display.h`. |
| **Day 10** | Zephyr Shell CLI & IPC MsgQ | `Zephyr SDK` & `Kconfig`| `CONFIG_SHELL`, `CONFIG_THREAD_ANALYZER` | `zephyr/include/zephyr/shell/shell.h` & `kernel.h`. |
| **Day 11** | Vector DBC Unpacking & CRC-8 | `Vector DBC` & `AUTOSAR`| `BO_`, `SG_`, `E2E Profile` | Vector DBC Specification & AUTOSAR E2E Protocol Spec. |
| **Day 12** | ISO 26262, MISRA-C & Bus-Off | `ISO 26262` & `MISRA` | `FTTI`, `MISRA-C:2012 Rule 21.3` | Tiêu chuẩn ISO 26262-5/6 & Zephyr `drivers/watchdog.h`. |
| **Day 13** | DWT Cycle Counter & Unity Test | `PM0253` & `CMSIS` | `Data watchpoint and trace (DWT)` | PM0253 Chapter 4 (Section 4.8 DWT) & `core_cm7.h`. |
| **Day 14** | Master Capstone & Automotive CV | `All Specs` | `Dual-Architecture Gateway` | Báo cáo kiến trúc 2 tầng (Bare-metal + Zephyr RTOS). |

---

# 📦 BƯỚC 3: CẤU TRÚC REPOSITORY GITHUB CHUẨN CÔNG NGHIỆP

Để hồ sơ của bạn nổi bật hoàn toàn so với các ứng viên sinh viên thông thường, repository GitHub phải được sắp xếp khoa học và có mục **"Known Issues & Lessons Learned"**:

```text
stm32f746-dual-can-gateway/
├── docs/                      <-- Toàn bộ 14 cẩm nang kỹ thuật chuyên sâu (Day 00 -> 14)
├── baremetal/                 <-- Mã nguồn thuần thanh ghi Phase 1
│   ├── drivers/               <-- Driver: Sys_Clock, uart_dma, can, sdram, ltdc, dma2d, watchdog
│   └── src/main.c
├── zephyr_rtos/               <-- Mã nguồn hệ điều hành Phase 2
│   ├── app.overlay            <-- Cây thiết bị Devicetree
│   ├── prj.conf               <-- Cấu hình Kconfig
│   └── src/                   <-- main.c, can_gateway, gui_cluster, cli_shell, dbc_decoder
├── test/                      <-- Bộ Unit Test chạy trên PC host bằng Unity Framework
│   ├── test_dbc_decoder.c
│   └── Makefile
└── README.md                  <-- Trang chủ Portfolio cực kỳ chuyên nghiệp
```

### 💡 Trích Đoạn Vàng Trong File `README.md`: "Known Issues & Lessons Learned"
*(Đây là phần các nhà tuyển dụng cấp cao thích đọc nhất vì nó thể hiện tư duy xử lý sự cố thực tế!)*

> ### 🛠️ Known Issues & Engineering Lessons Learned:
> 1. **D-Cache False Sharing Corruption (Solved):** Khi bật L1 D-Cache trên Cortex-M7, việc gọi hàm `SCB_InvalidateDCache_by_Addr()` cho UART RX Ring Buffer vô tình làm hỏng các biến toàn cục nằm cạnh.  
>    * *Khắc phục:* Buộc căn lề bộ đệm đúng **$32\text{ bytes}$** (`__attribute__((aligned(32)))`) bằng đúng độ dài một Cache Line vật lý.
> 2. **Motorola Big-Endian Zig-Zag Bit Order (Solved):** Khi giải mã tín hiệu góc lái `Steering_Angle` từ mạng CAN, giá trị bị sai lệch hoàn toàn so với mô phỏng.  
>    * *Khắc phục:* Nhận diện sự khác biệt cốt lõi: Start Bit của Motorola là MSB (không phải LSB như Intel). Viết lại hàm `DBC_UnpackRaw` hỗ trợ bước nhảy lùi zíc-zắc qua các ranh giới byte.
> 3. **IWDG Reset During Breakpoint Debugging (Solved):** Khi đặt breakpoint trong Keil/VS Code, chip bị reset liên tục sau mỗi 2 giây.  
>    * *Khắc phục:* Khai báo bit `DBG_IWDG_STOP` trong thanh ghi `DBGMCU->APB1FZ` để đóng băng bộ đếm Watchdog mỗi khi lõi CPU bị tạm dừng.

---

# 🎯 BƯỚC 4: BỘ 20 CÂU HỎI PHỎNG VẤN SÁT HẠCH KỸ THUẬT LÕI (MASTER CHEAT SHEET)

Dưới đây là 20 câu hỏi kỹ thuật hóc búa nhất thường được các chuyên gia phỏng vấn tại các tập đoàn lớn (Bosch, FPT Software Automotive, LG Electronics, VinFast, Qualcomm) đặt ra:

---

### 🟢 NHÓM 1: LÕI ARM CORTEX-M7, BỘ NHỚ & CLOCK (NGÀY 1, 4, 5, 13)

#### 1. D-Cache Coherency trên Cortex-M7 là gì? Tại sao DMA nhận dữ liệu vào SRAM mà CPU lại đọc ra dữ liệu cũ?
* **Trả lời:** Lõi Cortex-M7 tích hợp L1 Data Cache ($16\text{ KB}$). Khi CPU đọc SRAM, dữ liệu được nhân bản vào Cache Line ($32\text{ bytes}$). Khối DMA hoạt động độc lập trên AXI Bus, ghi thẳng vào SRAM vật lý mà **không đi qua D-Cache** (STM32F7 không có Hardware Cache Snooping cho DMA). Khi CPU đọc lại, nó thấy Cache Line vẫn còn giá trị cũ nên đọc ngay từ Cache $\implies$ Sai lệch dữ liệu. Khắc phục: CPU phải gọi `SCB_InvalidateDCache_by_Addr()` trước khi đọc để ép xóa bản sao cũ.

#### 2. Tại sao mọi bộ đệm DMA trên Cortex-M7 bắt buộc phải căn lề 32-byte (`aligned(32)`)?
* **Trả lời:** Một Cache Line trên Cortex-M7 có kích thước cố định là $32\text{ bytes}$. Nếu bộ đệm DMA không căn lề 32-byte hoặc kích thước không phải bội số của 32, nó sẽ nằm chung Cache Line với các biến RAM khác (hiện tượng **False Sharing**). Khi ta gọi lệnh Invalidate Cache Line của bộ đệm DMA, phần dữ liệu mới chưa kịp lưu của các biến RAM nằm cạnh sẽ bị xóa sổ theo, gây sập chương trình ngẫu nhiên.

#### 3. Tại sao khi ép xung lên 216MHz trên STM32F7, bắt buộc phải tăng Flash Wait States TRƯỚC khi chuyển đổi PLL?
* **Trả lời:** Bộ nhớ Flash bên trong silicon chỉ có thể truy xuất ở tốc độ tối đa khoảng $30\text{ MHz}$. Ở $216\text{ MHz}$, 1 chu kỳ CPU chỉ tốn $4.63\text{ ns}$, trong khi Flash cần tới $30\text{ ns}$ mới lấy xong 1 lệnh. Nếu chuyển xung CPU lên 216MHz trước khi tăng Wait States lên 7 WS, CPU sẽ đọc phải các lệnh rác từ Flash chưa kịp phản hồi $\implies$ Kích hoạt ngoại lệ **`HardFault`** chết chip ngay lập tức!

#### 4. Chuỗi bắt tay 2 bước kích hoạt chế độ Over-drive trong khối PWR diễn ra như thế nào?
* **Trả lời:** Để đạt tần số $> 180\text{ MHz}$, điện áp lõi $1.2\text{V}$ (Scale 1) không đủ sức làm các transistor đóng ngắt kịp. Quy trình bắt tay phần cứng:
  1. Cấp clock PWR $\rightarrow$ Bật `ODEN` trong `PWR_CR1`.
  2. Polling chờ cờ `ODRDY = 1` trong `PWR_CSR1` (Xác nhận nguồn áp Over-drive sẵn sàng).
  3. Bật bit chuyển mạch `ODSWEN` trong `PWR_CR1`.
  4. Polling chờ cờ `ODSWRDY = 1` trong `PWR_CSR1` (Xác nhận mạch đã chuyển áp sang lõi thành công).

#### 5. Công thức tính toán thanh ghi làm tươi FMC SDRAM Refresh Rate Counter (`SDRTR`) là gì?
* **Trả lời:** Chip SDRAM cần làm tươi 4096 hàng trong vòng $64\text{ ms}$.
  * Thời gian làm tươi một hàng: $T_{refresh} = 64\text{ ms} / 4096 = 15.625\mu\text{s}$.
  * Tần số FMC Clock $= 108\text{ MHz} \implies \text{Chu kỳ } T_{clk} = 9.26\text{ ns}$.
  * Công thức chuẩn RM0385: $\text{COUNT} = (T_{refresh} \times f_{FMC}) - 20 = (15.625\mu\text{s} \times 108\text{MHz}) - 20 = \mathbf{1667} \text{ (Hex: } \texttt{0x0683})$.

---

### 🔵 NHÓM 2: NGOẠI VI GIAO TIẾP & BARE-METAL DRIVERS (NGÀY 2, 3, 5, 6)

#### 6. Tại sao tuyệt đối cấm dùng phép toán `REG |= FLAG` trên các thanh ghi cờ ngắt dạng W1C (`USART_ICR`, `DMA_LIFCR`, `DMA2D_IFCR`)?
* **Trả lời:** Thanh ghi `W1C` (Write 1 to Clear) hoặc `w` (Write-only) có đặc tính: Ghi `1` thì xóa cờ, ghi `0` thì giữ nguyên. Khi dùng `|=`, CPU đọc toàn bộ thanh ghi ra (lúc này các kênh khác đang có cờ ngắt hoặc cờ lỗi $= 1$), sau đó ghi ngược lại toàn bộ $\implies$ Thao tác này vô tình **xóa sạch toàn bộ cờ ngắt và cờ lỗi của các kênh/mailbox khác** trước khi hàm phục vụ ngắt của chúng kịp kiểm tra! Khắc phục: Luôn ghi gán trực tiếp: `REG = FLAG`.

#### 7. Cơ chế Dual-Trigger trong UART RX DMA Ring Buffer hoạt động như thế nào?
* **Trả lời:** Sử dụng 2 nguồn kích hoạt ngắt độc lập:
  * **DMA Half-Transfer (HT) & Transfer-Complete (TC):** Kích hoạt khi nhận liên tục các luồng dữ liệu lớn dài đúng $50\%$ hoặc $100\%$ dung lượng Ring Buffer.
  * **USART IDLE Line Interrupt:** Kích hoạt khi đường truyền RX rơi vào trạng thái rảnh `HIGH` liên tục trong 1 frame time (báo hiệu thiết bị đã truyền xong một gói tin có độ dài bất kỳ, chưa làm đầy buffer). Sự kết hợp này giúp đọc dữ liệu tức thì mà tải CPU xấp xỉ $0\%$.

#### 8. Khi cổng UART bị lỗi tràn phần cứng Overrun Error (ORE), tại sao DMA ngừng nhận dữ liệu? Cách phục hồi ra sao?
* **Trả lời:** Khi CPU bận không phục vụ kịp hoặc có xung nhiễu, byte mới đè lên byte cũ trong `USART_RDR` $\implies$ Cờ `ORE` dựng lên. Trong kiến trúc STM32, cờ `ORE` sẽ tự động **khóa mạch yêu cầu DMA (DMA Request)** của UART. DMA sẽ đứng im vĩnh viễn dù bus vẫn có dữ liệu. Để phục hồi: Bắt buộc hàm ngắt phải kiểm tra cờ `ORE` trong `ISR` và ghi `1` vào bit `ORECF` trong `USART_ICR` để mở khóa lại luồng DMA.

#### 9. Tại sao khi cấu hình CAN2 trên STM32F7, bắt buộc phải cấp clock và cấu hình thanh ghi `CAN1->FMR`?
* **Trả lời:** Trong silicon của STM32, khối phần cứng chứa **28 Filter Banks là tài nguyên dùng chung và do CAN1 làm Master quản lý độc quyền**. Thanh ghi cấu hình phân chia ranh giới bộ lọc `CAN2SB[5:0]` nằm trong `CAN1->FMR`. Nếu không cấp clock cho CAN1 và không đưa `CAN1->FMR` vào chế độ cấu hình (`FINIT = 1`), thì CAN2 hoàn toàn không được cấp bất kỳ Filter Bank nào và sẽ vứt bỏ toàn bộ frame nhận được ngoài bus!

#### 10. Điểm lấy mẫu (Sample Point) trong Bit Timing mạng CAN là gì? Tại sao chuẩn ô tô yêu cầu 87.5%?
* **Trả lời:** Sample Point là thời điểm bộ điều khiển CAN đọc mức điện áp trên bus trong 1 chu kỳ bit. Với $f_{PCLK1} = 54\text{ MHz}$, Baudrate $500\text{ kbps}$, 1 bit gồm $18\text{ Time Quanta (tq)}$. Ta cài $\text{Sync\_Seg} = 1$, $\text{Prop\_Seg} + \text{Phase\_Seg1} = 14$, $\text{Phase\_Seg2} = 3 \implies \text{Sample Point} = (1 + 14) / 18 = \mathbf{83.33\% \sim 87.5\%}$. Chuẩn ô tô đặt điểm lấy mẫu ở cuối bit ($> 80\%$) để triệt tiêu ảnh hưởng của độ trễ lan truyền vật lý trên đường cáp dài và thời gian trễ của chip thu phát Transceiver.

#### 11. Thanh ghi độ lệch dòng `DMA2D_OOR` được tính như thế nào? Đơn vị của nó là gì?
* **Trả lời:** Khi vẽ hình chữ nhật $W_{box} \times H_{box}$ lên màn hình $W_{screen} \times H_{screen}$, thanh ghi Output Offset Register được tính: $\text{OOR} = W_{screen} - W_{box}$. Đơn vị của `OOR` là **số pixel, KHÔNG PHẢI số byte**! Nếu cấu hình sai thành byte, hình vẽ sẽ bị biến dạng xé xéo trên màn hình.

#### 12. Mạch Clock Security System (CSS) hoạt động ra sao khi mất thạch anh ngoài HSE?
* **Trả lời:** Khi HSE mất dao động, mạch phần cứng CSS tự động thực hiện 3 hành động tức thì: (1) Tự động ngắt kết nối HSE và chuyển nguồn SYSCLK sang dao động nội **HSI 16MHz**, (2) Vô hiệu hóa bộ nhân PLL, (3) Phát tín hiệu ngắt bất khả kháng **NMI (Exception 2)** để CPU nhảy vào `NMI_Handler` thực thi quy trình hạ cánh an toàn (Fail-Safe), không bao giờ bị treo cứng.

---

### 🟡 NHÓM 3: ZEPHYR RTOS, MULTI-THREADING & IPC (NGÀY 7, 8, 9, 10)

#### 13. Devicetree trong Zephyr RTOS được nạp lúc Compile-time hay Run-time? Điểm vượt trội so với Linux là gì?
* **Trả lời:** Devicetree trong Zephyr được xử lý **$100\%$ lúc biên dịch (Compile-time)**. Bộ tiền xử lý Python đọc `.dts`/`.overlay` và sinh ra mã nguồn C macro tĩnh trong `devicetree_generated.h`. Khác với Linux nhúng phải nạp file `.dtb` vào RAM lúc boot (tốn hàng chục KB RAM và chu kỳ CPU), Zephyr tiêu tốn **đúng 0 byte RAM** và cho phép Compiler loại bỏ mã chết (Dead Code Elimination) tối đa.

#### 14. Cơ chế `CONFIG_MPU_STACK_GUARD` bắt lỗi tràn ngăn xếp (Stack Overflow) như thế nào?
* **Trả lời:** Thay vì kiểm tra thụ động bằng phần mềm (mã số ảo Canary), Zephyr lập trình khối phần cứng **MPU của Cortex-M7** để đặt một vùng cấm truy cập kích thước $32\text{ bytes}$ ở đáy ngăn xếp của luồng đang chạy. Nếu luồng sử dụng quá mức và ghi vào vùng này, ngoại lệ phần cứng **`MemManage Fault`** nổ ra ngay lập tức tại chu kỳ lệnh vi phạm, dừng hệ thống và chỉ điểm chính xác luồng phạm quy.

#### 15. Sự cố Đảo ngược mức ưu tiên (Priority Inversion) là gì? Kế thừa ưu tiên (Priority Inheritance) trong `k_mutex` giải quyết nó ra sao?
* **Trả lời:** Xảy ra khi Luồng Thấp giữ Mutex, Luồng Cao cần Mutex nên phải chờ. Một Luồng Trung Bình nhảy vào chiếm CPU, gián tiếp ngăn Luồng Thấp nhả Mutex, khiến Luồng Cao bị treo vô hạn sau Luồng Trung Bình. Giải pháp: Khi Luồng Cao chờ Mutex, Zephyr **tự động nâng mức ưu tiên của Luồng Thấp lên bằng Luồng Cao**. Luồng Trung Bình không thể chen ngang được nữa, giúp Luồng Thấp nhanh chóng nhả Mutex cho Luồng Cao chạy.

#### 16. Tại sao thư viện đồ họa LVGL không Thread-safe? Cách khắc phục chuẩn trong RTOS là gì?
* **Trả lời:** Để tiết kiệm bộ nhớ cho MCU, các hàm LVGL thao tác trên danh sách liên kết dùng chung mà không có khóa nội tại. Nếu nhiều luồng cùng gọi hàm LVGL đồng thời, danh sách liên kết sẽ bị gãy và gây crash HardFault. Khắc phục: Bắt buộc dùng chung một Mutex (`k_mutex_lock/unlock`) bọc quanh mọi lời gọi hàm LVGL và hàm định kỳ `lv_timer_handler()`.

---

### 🟣 NHÓM 4: GIAO THỨC AUTOMOTIVE, AN TOÀN CHỨC NĂNG & TEST (NGÀY 11, 12, 13)

#### 17. Sự khác biệt cốt lõi giữa định dạng Intel (Little-Endian) và Motorola (Big-Endian) trong file Vector DBC?
* **Trả lời:** 
  * **Intel (`@1`):** Start Bit là LSB (Bit trọng số nhỏ nhất). Tín hiệu phát triển tiến lên theo chiều tăng chỉ số bit ($0 \rightarrow 1 \rightarrow 2 \dots$).
  * **Motorola (`@0`):** Start Bit là **MSB (Bit trọng số lớn nhất)**! Tín hiệu phát triển lùi về LSB trong cùng byte, sau đó nhảy zíc-zắc sang MSB của byte tiếp theo.

#### 18. Tiêu chuẩn AUTOSAR E2E (End-to-End) Protection bảo vệ khung tin CAN gồm những thành phần nào?
* **Trả lời:** 3 tầng bảo vệ: (1) **Rolling Counter (4-bit):** Chống lỗi lặp gói tin hoặc ECU bị chết đứng, (2) **Mã E2E CRC-8 (Đa thức 0x2F):** Chống lỗi biến dạng bit do nhiễu điện từ, tính kèm mã bí mật **`Data ID`** để chống giả mạo nguồn phát, (3) **Timeout Supervision:** Phát hiện mất gói tin và đứt cáp truyền thông thời gian thực.

#### 19. Tại sao phép trừ số nguyên không dấu `(uint32_t)(now - last) >= TIMEOUT` luôn đúng kể cả khi bộ đếm thời gian bị tràn số sau 49.7 ngày?
* **Trả lời:** Khi biến `uint32_t` tràn số qua mốc $2^{32}-1$ về $0$, phép trừ bù hai không dấu trong tập lệnh ARM tự động triệt tiêu phần tràn bit (Mod arithmetic). Khoảng cách chênh lệch giữa hai thời điểm luôn trả về số dương chính xác tuyệt đối mà không bao giờ bị âm hay bị lỗi logic.

#### 20. Khi nào nên chọn kiến trúc Bare-Metal và khi nào nên chọn Zephyr RTOS cho sản phẩm thương mại?
* **Trả lời:** 
  * Chọn **Bare-Metal** khi: Ứng dụng an toàn chức năng cấp cao (ASIL-D), yêu cầu thời gian khởi động tức thì ($< 20\text{ms}$), tài nguyên phần cứng cực kỳ hạn chế (Flash $< 64\text{KB}$, RAM $< 16\text{KB}$), và yêu cầu độ trễ ngắt cứng đến từng nano-giây.
  * Chọn **Zephyr RTOS** khi: Hệ thống phức tạp, có giao diện đồ họa cao cấp (LVGL), kết nối nhiều giao thức mạng (CAN, Ethernet, BLE, Wi-Fi), cần nâng cấp OTA từ xa, yêu cầu chuẩn hóa Devicetree và cần mở rộng quy mô phát triển với đội ngũ kỹ sư lớn.

---

### 🎙️ KỊCH BẢN PHỎNG VẤN 2 PHÚT CHUẨN PHƯƠNG PHÁP STAR (ELEVATOR PITCH)

> **Nhà tuyển dụng:** *"Em hãy giới thiệu về dự án tâm đắc nhất của em và một lỗi kỹ thuật phức tạp nhất mà em từng tự tay giải quyết?"*  
>   
> **Ứng viên trả lời theo chuẩn STAR:**  
>   
> * **S - Situation (Bối cảnh):**  
> *"Dự án tâm đắc nhất của em là **Hệ thống Automotive CAN Gateway & Digital Instrument Cluster đa kiến trúc** trên nền vi điều khiển ARM Cortex-M7 (STM32F746 216MHz). Mục tiêu của dự án là nhận diện, giải mã tín hiệu mạng xe hơi theo chuẩn Vector DBC và hiển thị giao diện táp-lô thời gian thực, đồng thời so sánh định lượng trực diện giữa hai trường phái: **Bare-Metal Drivers** và **Zephyr RTOS**."*  
>   
> * **T - Task (Nhiệm vụ):**  
> *"Em chịu trách nhiệm tự tay viết toàn bộ driver thanh ghi từ đầu không dùng thư viện HAL gồm: Cấu hình xung 216MHz Over-Drive, UART DMA Ring Buffer, bộ điều khiển bxCAN 28 Filter Banks, FMC SDRAM 108MHz, và bộ quét màn hình đồ họa LTDC. Sau đó, em chuyển dịch toàn bộ hệ thống lên Zephyr RTOS kết hợp thư viện đồ họa LVGL và tích hợp chuẩn an toàn ô tô AUTOSAR E2E."*  
>   
> * **A - Action (Hành động & Giải quyết Bug khó nhất):**  
> *"Thử thách kỹ thuật khó nhất em gặp phải là **hiện tượng hỏng dữ liệu bộ nhớ ngẫu nhiên (Memory Corruption)** khi DMA nhận dữ liệu UART. Sau khi tra cứu tài liệu ARM Architecture Manual, em phát hiện lỗi xuất phát từ **D-Cache False Sharing**: Do bộ đệm DMA không được căn lề đúng $32\text{ bytes}$ (độ dài 1 Cache Line của Cortex-M7), lệnh Invalidate Cache của DMA đã vô tình xóa sổ các biến RAM nằm liền kề. Em đã giải quyết triệt để bằng cách ép căn lề `__attribute__((aligned(32)))` cho mọi bộ đệm truyền thông.  
> Đối với mạng CAN, em thiết lập cơ chế **Zero-Copy Bit Unpacking** hỗ trợ cả hai định dạng Intel và Motorola Big-Endian, tích hợp thuật toán kiểm tra **E2E CRC-8 kèm Data ID**, và thiết kế kiến trúc phân tầng ngắt **`NVIC_PriorityGroup_4`** để bảo vệ tính thời gian thực của gói tin an toàn."*  
>   
> * **R - Result (Kết quả định lượng):**  
> *"Bằng việc sử dụng bộ đếm chu kỳ phần cứng **DWT Cycle Counter (độ phân giải 4.63ns)**, em đã đo đạc và chứng minh phiên bản Bare-metal đạt thời gian boot thần tốc **$18.4\text{ ms}$** và chiếm dụng Flash chỉ **$26\text{ KB}$**, trong khi phiên bản Zephyr RTOS cung cấp khả năng mở rộng đa luồng an toàn với thời gian boot **$142.6\text{ ms}$**, cả hai đều vượt xa tiêu chuẩn ô tô ($< 2.0\text{s}$). Toàn bộ thuật toán giải mã cũng được em thiết lập bộ kiểm thử tự động **Host Unit Testing với Unity Framework** chạy kiểm tra $100\%$ ca test trên PC trước khi nạp vào xe thật."*
