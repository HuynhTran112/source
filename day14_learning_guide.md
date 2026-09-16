# 🏆 [NGÀY 14] ĐÓNG GÓI PORTFOLIO CHUYÊN NGHIỆP: VIẾT CV KỸ THUẬT & BỘ 20 CÂU HỎI PHỎNG VẤN SÁT HẠCH LÕI BARE-METAL & ZEPHYR RTOS
## Chuyên khảo Kỹ thuật: Chiến Lược Định Vị CV 2 Hướng, Mục "Known Issues" Ghi Điểm Tuyệt Đối & Master Interview Cheat Sheet

> **Mục tiêu chuyên sâu:** Hoàn tất chặng đường 14 ngày chinh phục hệ thống nhúng cao cấp trên STM32F746 (ARM Cortex-M7):
> 1. **Chiến Lược Định Vị CV 2 Cột Riêng Biệt:** Cách tùy biến cùng một dự án thành 2 bản CV chuyên nghiệp nhắm trúng **Job Automotive** (các công ty ô tô như Ban Vien, Bosch, VinFast, LG VS) và **Job Firmware Thuần / BSP** (các công ty bán dẫn và IoT như Renesas, Qualcomm, NXP, Viettel).
> 2. **Vũ Khí Bí Mật: Mục "Known Issues & Lessons Learned" Trên GitHub:** Tại sao việc khoe các lỗi hóc búa đã vượt qua (D-Cache stale, UART ORE, Transceiver STB, Timer wrap-around) lại khiến Technical Lead đánh giá bạn cao hơn 95% ứng viên khác?
> 3. **Bộ 20 Câu Hỏi Phỏng Vấn Sát Hạch Cốt Lõi (Master Interview Cheat Sheet):** 20 câu hỏi truy vấn sâu nhất từ thanh ghi Bare-metal Cortex-M7 đến cơ chế nội tại Zephyr RTOS và an toàn chức năng ô tô.
> 4. **Kịch Bản Thuyết Trình Dự Án 2 Phút Chuẩn Phương Pháp STAR:** Nghệ thuật trình bày dự án tự tin, sắc bén và thuyết phục nhà tuyển dụng ngay từ 60 giây đầu tiên.

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                      SƠ ĐỒ TOÀN CẢNH DỰ ÁN DUAL-ARCHITECTURE CAN GATEWAY                        │
├─────────────────────────────────────────────────┬───────────────────────────────────────────────┤
│        KIẾN TRÚC 1: BARE-METAL SUBSYSTEM        │       KIẾN TRÚC 2: ZEPHYR RTOS SUBSYSTEM      │
│         (Tối ưu hóa phần cứng tuyệt đối)        │           (Đa nhiệm & Mở rộng quy mô)         │
├─────────────────────────────────────────────────┼───────────────────────────────────────────────┤
│ • Ngày 1: Clock 216MHz Over-Drive & Reset CSR   │ • Ngày 7: Devicetree Overlay & MPU Stack Guard│
│ • Ngày 2: UART RX DMA Circular & D-Cache Align  │ • Ngày 8: CAN Subsystem & Zero-Lock MsgQ      │
│ • Ngày 3: bxCAN 500kbps & 28 Filter Banks       │ • Ngày 9: FMC SDRAM, LTDC & Zero-Copy LVGL    │
│ • Ngày 4: FMC SDRAM 108MHz & LTDC Display 60FPS │ • Ngày 10: k_msgq, Priority Inheritance & CLI │
│ • Ngày 5: DMA2D Chrom-ART & NVIC Priority Matrix│ • Ngày 11: Vector DBC Intel/Motorola Decoding │
│ • Ngày 6: Watchdogs (IWDG/WWDG) & CSS Failover  │ • Ngày 12: AUTOSAR E2E & Bus-Off Recovery FSM │
├─────────────────────────────────────────────────┴───────────────────────────────────────────────┤
│ • Ngày 13: Đo lường thực nghiệm DWT Cycle Counter (18.4ms vs 142.6ms) & Host Unit Testing       │
│ • Ngày 14: Đóng gói Portfolio GitHub & Chiến lược phỏng vấn tuyển dụng kỹ sư cấp cao           │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

# PHẦN 1: CHIẾN LƯỢC ĐỊNH VỊ CV & NGHỆ THUẬT KỂ CHUYỆN DỰ ÁN

Cùng một sản phẩm bạn đã phát triển, bạn có thể ứng tuyển thành công vào **2 phân khúc công việc khác nhau** bằng cách nhấn mạnh các khía cạnh kỹ thuật tương ứng:

### 1.1. Bảng Định Vị 2 Hướng Ứng Tuyển: Automotive vs Firmware Thuần

| Tiêu Chí So Sánh | Hướng 1: Ứng Tuyển Kỹ Sư Phần Mềm Ô Tô (Automotive) | Hướng 2: Ứng Tuyển Kỹ Sư Firmware / BSP / IoT |
| :--- | :--- | :--- |
| **Tiêu đề Dự án trong CV** | **Automotive CAN Gateway & Digital Instrument Cluster** | **Dual-Architecture Embedded Display & Communication System** |
| **Nhà tuyển dụng mục tiêu** | Ban Vien, Bosch, FPT Automotive, VinFast, LG VS, Hyundai Mobis. | Renesas, Qualcomm, NXP, Viettel, FPT Software, các công ty IoT. |
| **Trọng tâm kỹ thuật** | Độ tin cậy mạng truyền thông ô tô, giao thức Vector DBC, an toàn chức năng AUTOSAR E2E, Bus-Off Recovery ISO 11898-1. | Lập trình thanh ghi Bare-metal Cortex-M7, giải quyết D-Cache Coherency, Zephyr RTOS đa luồng, tối ưu hóa RAM/Flash. |
| **Từ khóa ghi điểm ATS** | `CAN Bus`, `Vector DBC`, `AUTOSAR E2E Profile 1`, `ISO 11898-1 Bus-Off FSM`, `CiA 301`, `Rolling Counter`. | `ARM Cortex-M7`, `L1 D-Cache Coherency`, `FMC SDRAM`, `DMA2D Chrom-ART`, `Zephyr RTOS`, `k_msgq`, `MPU Stack Guard`. |
| **Bằng chứng kỹ thuật** | Ma trận giải mã tín hiệu DBC, thuật toán bảng Lookup CRC-8 (0x2F), máy trạng thái phục hồi lỗi mạng xe hơi. | Bảng số liệu thực nghiệm DWT Cycle Counter, bản đồ căn lề 32 bytes DMA, báo cáo Unit Test chạy tự động trên PC. |

---

# PHẦN 2: MỤC "KNOWN ISSUES & LESSONS LEARNED" TRÊN GITHUB

Ứng viên non tay thường viết README kiểu: *"Dự án chạy hoàn hảo không có lỗi gì"*. Các Technical Lead đọc vào sẽ biết ngay là code copy trên mạng!

Ngược lại, khi bạn đưa mục **"Sự Cố Kỹ Thuật Đã Giải Quyết (Known Issues & Engineering Lessons Learned)"** vào README, nhà tuyển dụng sẽ bị thuyết phục 100% rằng bạn là người **thực chiến tự tay debug**:

### 5 Bài Học Thực Chiến Kinh Điển Nên Đưa Vào Portfolio:

1. **Lỗi UART Overrun Error (ORE) làm đóng băng DMA (Ngày 2):**
   * *Hiện tượng:* Sau khi nhận dữ liệu rác hoặc CPU bận, bộ đệm DMA ngừng nhận hoàn toàn dù chân RX vẫn có tín hiệu.
   * *Nguyên nhân:* Cờ ORE bật lên trong thanh ghi `USART_ISR` khiến ngoại vi khóa mạch bắt tay với DMA.
   * *Giải pháp:* Luôn xóa cờ ORE bằng cách ghi `1` vào bit `ORECF` trong thanh ghi `USART_ICR`.
2. **Lỗi Sọc Màn Hình Do Bất Đồng Bộ D-Cache Cortex-M7 (Ngày 4 & 9):**
   * *Hiện tượng:* Màn hình táp-lô bị sọc ngang hoặc vỡ hình khi kim đồng hồ chuyển động.
   * *Nguyên nhân:* CPU ghi pixel vào L1 D-Cache (Write-Back) nhưng chưa xả xuống RAM ngoài SDRAM, trong khi LTDC/DMA lại đọc từ SDRAM.
   * *Giải pháp:* Dùng MPU cấu hình phân vùng Framebuffer trên SDRAM thành chế độ `Non-cacheable`.
3. **Lỗi Mạng CAN "Im Lặng Hoàn Toàn" Do Chân Transceiver Standby (Ngày 8):**
   * *Hiện tượng:* Lệnh `can_send()` chạy không lỗi nhưng trên máy đo xung không có bất kỳ tín hiệu nào xuất hiện trên đường truyền.
   * *Nguyên nhân:* Chân STB của IC Transceiver TJA1050 bị thả nổi hoặc kéo HIGH khiến chip ngủ đông.
   * *Giải pháp:* Cấu hình chân GPIO kéo chân STB xuống mức LOW trước khi khởi động mạng CAN.
4. **Hiểm Họa Priority Inversion Giữa Luồng CAN Và Luồng GUI (Ngày 10):**
   * *Hiện tượng:* Luồng CAN thời gian thực bị trễ nhịp ngẫu nhiên khi màn hình đang render hình ảnh phức tạp.
   * *Nguyên nhân:* Dùng sai cơ chế khóa khiến luồng trung bình chen ngang luồng thấp đang giữ tài nguyên chung.
   * *Giải pháp:* Chuyển sang dùng `k_mutex` có cơ chế Priority Inheritance và áp dụng mô hình Message Queue `k_msgq`.
5. **Lỗi Tràn Số Bộ Đếm Thời Gian Sau 49.7 Ngày Hoạt Động (Ngày 12):**
   * *Hiện tượng:* Hệ thống giám sát mất tín hiệu bị đóng băng sau 49.7 ngày vận hành liên tục.
   * *Nguyên nhân:* So sánh thời gian dạng `now >= last + timeout` bị sai khi biến đếm 32-bit tràn qua mốc 0.
   * *Giải pháp:* Áp dụng triệt để quy tắc phép trừ số nguyên không dấu `(uint32_t)(now - last) >= timeout`.

---

# PHẦN 3: BỘ 20 CÂU HỎI SÁT HẠCH CỐT LÕI (MASTER INTERVIEW CHEAT SHEET)

Đây là bảng tổng hợp 20 câu hỏi kỹ thuật hay gặp nhất khi ứng tuyển vị trí Kỹ sư Nhúng (Fresher / Junior) tại các công ty lớn:

### Nhóm 1: Lõi Vi Xử Lý ARM Cortex-M7 & Thanh Ghi Bare-Metal (Ngày 1 - 6)
1. **Q:** *Tại sao thanh ghi phần cứng bắt buộc phải dùng từ khóa `volatile`?*  
   **A:** Để ngăn trình biên dịch tối ưu hóa (Optimization) xóa bỏ các thao tác đọc/ghi vào địa chỉ thanh ghi vật lý của chip.
2. **Q:** *Trên ARM Cortex-M7, L1 D-Cache ảnh hưởng thế nào đến bộ đệm của DMA?*  
   **A:** Gây ra lỗi bất đồng bộ bộ nhớ (Cache Incoherency). Bắt buộc phải căn lề bộ đệm 32 bytes và gọi hàm `SCB_InvalidateDCache_by_Addr()` trước khi CPU đọc dữ liệu do DMA ghi vào.
3. **Q:** *Quy tắc Clear-then-Set khi cấu hình trường bit đa ô (Bitfield) là gì?*  
   **A:** Luôn xóa trường bit về 0 bằng `REG &= ~(MASK << POS)` trước khi gán giá trị mới bằng `REG |= (VAL << POS)` để không bị ghi đè lẫn lộn với các bit cũ.
4. **Q:** *Thao tác xóa cờ ngắt dạng W1C (Write 1 to Clear) khác gì với cờ thông thường?*  
   **A:** Ghi trực tiếp bit `1` vào cờ để xóa. Tuyệt đối không dùng toán tử `REG |= FLAG` vì sẽ vô tình xóa luôn các cờ lỗi khác nằm cùng thanh ghi.
5. **Q:** *Tại sao để chạy tốc độ cực đại 216 MHz trên STM32F7 lại bắt buộc phải bật Over-Drive Mode?*  
   **A:** Bộ ổn áp nguồn nội (Internal Voltage Regulator) ở chế độ thông thường không đủ điện áp duy trì độ trễ transistor ở 216 MHz. Bật Over-Drive trong thanh ghi `PWR_CR1` giúp cấp điện áp ổn định cho lõi CPU.
6. **Q:** *Phân biệt Independent Watchdog (IWDG) và Window Watchdog (WWDG)?*  
   **A:** IWDG dùng xung nhịp độc lập LSI 32kHz (bảo vệ chống treo nguồn/xung), chỉ cần cho ăn trước khi timeout. WWDG dùng xung nhịp bus APB1, bắt buộc phải cho ăn đúng trong một "cửa sổ thời gian" quy định (bảo vệ chống luồng chạy sai trật tự).

### Nhóm 2: Hệ Điều Hành Zephyr RTOS & Kiến Trúc Phần Mềm (Ngày 7 - 10)
7. **Q:** *Devicetree trong Zephyr RTOS được nạp lúc runtime hay compile-time? Lợi ích là gì?*  
   **A:** Xử lý hoàn toàn lúc Compile-time. Công cụ Python sinh ra macro C tĩnh, tiêu thụ đúng 0 byte RAM và 0 ns thời gian duyệt cây lúc khởi động.
8. **Q:** *Zephyr Driver Model quản lý tính đa hình phần cứng trong ngôn ngữ C như thế nào?*  
   **A:** Dùng cấu trúc `struct device` gồm 3 con trỏ: `.config` (ROM tĩnh từ DTS), `.data` (RAM động), và `.api` (Bảng con trỏ hàm API chung).
9. **Q:** *Phân biệt Cooperative Thread và Preemptive Thread trong Zephyr?*  
   **A:** Cooperative có Priority âm, chạy độc quyền không bao giờ bị chiếm quyền. Preemptive có Priority dương, lập lịch theo mức ưu tiên và chia sẻ thời gian (Time-slicing).
10. **Q:** *Cơ chế `CONFIG_MPU_STACK_GUARD` bảo vệ hệ thống trước lỗi tràn ngăn xếp ra sao?*  
    **A:** Cấu hình phần cứng MPU đặt một vùng cấm ghi 32 bytes dưới đáy Stack của mỗi luồng. Khi có lệnh ghi đè vào đây, ngắt phần cứng `MemManage Fault` lập tức nổ ra tại chỗ.
11. **Q:** *Hiểm họa Priority Inversion là gì? Zephyr giải quyết bằng cách nào?*  
    **A:** Là hiện tượng luồng ưu tiên cao bị gián tiếp chết đói do luồng trung bình chen ngang luồng thấp đang giữ tài nguyên chung. Zephyr dùng `k_mutex` tự động nâng mức ưu tiên của luồng thấp (Priority Inheritance) để hóa giải.
12. **Q:** *Khi nào nên dùng `k_msgq` và khi nào nên dùng `k_fifo`?*  
    **A:** Dùng `k_msgq` khi dữ liệu nhỏ (< 32 bytes) cần sao chép theo giá trị an toàn trong bộ đệm tĩnh. Dùng `k_fifo` khi truyền dữ liệu lớn (ảnh, gói tin mạng) chỉ chuyển con trỏ để đạt tốc độ cao.
13. **Q:** *Tại sao Zephyr lại thiết kế System Workqueue (`k_work`)?*  
    **A:** Để chia sẻ chung một ngăn xếp luồng cho các tác vụ chạy định kỳ hoặc ngắn hạn, tránh việc tạo quá nhiều Thread gây lãng phí hàng chục KB RAM làm Stack.

### Nhóm 3: Giao Thức Mạng Xe Hơi & An Toàn Chức Năng (Ngày 11 - 13)
14. **Q:** *Trở đầu cuối 120 Ohm trên bus CAN có tác dụng gì? Mắc ở đâu?*  
    **A:** Mắc ở 2 đầu xa nhất của đường dây CAN để phối hợp trở kháng đặc tính của cáp xoắn đôi, triệt tiêu sóng phản xạ làm méo dạng tín hiệu vi sai.
15. **Q:** *Phân biệt Start Bit giữa chuẩn Intel và chuẩn Motorola trong tệp Vector DBC?*  
    **A:** Intel quy ước Start Bit là bit LSB (trọng số nhỏ nhất). Motorola quy ước Start Bit là bit MSB (trọng số lớn nhất) và các bit tiếp theo lan lùi theo hình răng cưa.
16. **Q:** *Tại sao chuẩn an toàn ô tô MISRA-C lại hạn chế dùng số thực `float` khi giải mã tín hiệu?*  
    **A:** Vì số thực dấu phẩy động IEEE 754 có tính phi tất định (sai số làm tròn trôi nổi) và tốn chu kỳ FPU. Số nguyên định điểm (Fixed-Point) đảm bảo tính toán nhanh gấp 10 lần và kết quả tất định 100%.
17. **Q:** *Ba lớp phòng ngự an toàn của chuẩn AUTOSAR E2E là gì?*  
    **A:** Rolling Counter (chống lặp gói tin), E2E CRC-8 kết hợp Data ID bí mật (chống sai lệch và giả mạo dữ liệu), và Timeout Supervision (chống tín hiệu đông đá).
18. **Q:** *Tại sao khi xe bị Bus-Off, tiêu chuẩn an toàn lại cấm việc tự động phục hồi tức thì?*  
    **A:** Vì nếu đường dây đang bị chập điện vật lý liên tục, việc cố gắng kết nối lại ngay lập tức sẽ phát sinh lỗi phá hủy băng thông mạng xe hoặc làm cháy chip Transceiver. Bắt buộc phải có khoảng trễ làm dịu (100ms) trước khi phục hồi.
19. **Q:** *Quy tắc toán học nào giúp loại trừ lỗi tràn số bộ đếm thời gian sau 49.7 ngày?*  
    **A:** Phép trừ số nguyên không dấu: `(uint32_t)(now - last) >= timeout`. Hệ thống bù hai đảm bảo hiệu số này luôn đúng kể cả khi biến đếm vừa quay vòng qua mốc 0.
20. **Q:** *Khối phần cứng ARM DWT Cycle Counter đo lường hiệu năng có độ phân giải bao nhiêu?*  
    **A:** Độ phân giải đúng bằng 1 chu kỳ xung nhịp CPU: Đạt 4.63 nano-giây tại tần số 216 MHz của vi điều khiển STM32F746.

---

# PHẦN 4: KỊCH BẢN THUYẾT TRÌNH DỰ ÁN 2 PHÚT (CHUẨN PHƯƠNG PHÁP STAR)

Khi người phỏng vấn nói: *"Em hãy giới thiệu tổng quan về dự án tâm đắc nhất của mình?"*, hãy áp dụng cấu trúc **STAR (Situation - Task - Action - Result)** để trả lời một cách tự tin và chuyên nghiệp:

### Kịch Bản 120 Giây Thuyết Trình Dự Án:

* **S - Situation (Bối cảnh):**  
  *"Trong các hệ thống Gateway trung tâm và táp-lô ô tô hiện đại, thiết bị vừa phải đáp ứng yêu cầu khởi động siêu tốc và độ trễ ngắt cực thấp, vừa phải có khả năng quản lý đa luồng phức tạp để hiển thị giao diện đồ họa mượt mà và xử lý an toàn dữ liệu mạng xe hơi."*

* **T - Task (Nhiệm vụ):**  
  *"Mục tiêu của em là phát triển một hệ thống **CAN Gateway & Digital Instrument Cluster** đa kiến trúc trên vi điều khiển hiệu năng cao **ARM Cortex-M7 (STM32F746)**: Xây dựng song song cả tầng driver thanh ghi Bare-metal cấp thấp và tầng hệ điều hành thời gian thực **Zephyr RTOS** kết hợp thư viện đồ họa **LVGL**."*

* **A - Action (Hành động & Kỹ thuật đã áp dụng):**  
  *"Để giải quyết bài toán này, em đã triển khai 3 trụ cột kỹ thuật:  
  1. **Ở tầng Bare-metal:** Em cấu hình hệ thống chạy ở tần số tối đa 216 MHz Over-Drive, xây dựng driver UART RX DMA Circular Buffer giải quyết triệt để lỗi căn lề 32 bytes của bộ nhớ đệm L1 D-Cache, và lập trình bộ điều khiển bxCAN với 28 ngân hàng bộ lọc Filter Banks phần cứng.  
  2. **Ở tầng Zephyr RTOS:** Em áp dụng mô hình Devicetree và Kconfig để cách ly hoàn toàn phần cứng khỏi mã nguồn C, kích hoạt MPU Stack Guard để bắt lỗi tràn ngăn xếp tức thì, và thiết kế luồng dữ liệu an toàn đa luồng bằng hàng đợi `k_msgq` kết hợp Mutex có tính năng Priority Inheritance.  
  3. **Về chuẩn công nghiệp ô tô:** Em xây dựng công cụ bóc tách bit tín hiệu ma trận Vector DBC hỗ trợ song song cả chuẩn Intel và Motorola bằng số nguyên định điểm Fixed-Point, đồng thời tích hợp 3 lớp phòng vệ an toàn chuẩn AUTOSAR E2E và máy trạng thái phục hồi lỗi mạng ISO 11898-1 Bus-Off FSM."*

* **R - Result (Kết quả định lượng sắc bén):**  
  *"Sử dụng khối phần cứng ARM DWT Cycle Counter để đo lường thực tế: Em chứng minh giải pháp Bare-metal đạt thời gian khởi động siêu tốc **18.4 mili-giây** với độ trễ ngắt chỉ **55.5 nano-giây**, trong khi nền tảng Zephyr RTOS vận hành ổn định giao diện táp-lô 60 FPS với dung lượng RAM tĩnh chỉ **44.8 KB** và thời gian boot **142.6 mili-giây**, vượt xa yêu cầu khắt khe dưới 2 giây của ngành công nghiệp ô tô."*
