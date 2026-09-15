# 🏆 [NGÀY 9] CẨM NANG TOÀN DIỆN ZEPHYR DISPLAY & LVGL: FMC SDRAM BINDING & GIAO DIỆN TÁP-LÔ ĐỒ HỌA
## Lộ trình 4 Bước: Kiến Trúc Subsystem ➔ Thực Chiến Devicetree/Kconfig ➔ Gõ Code Giao Diện ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Làm chủ hệ thống hiển thị đồ họa chuyên nghiệp trên STM32F746 thông qua **Zephyr Display Subsystem & Thư viện đồ họa LVGL (Light and Versatile Graphics Library)**: Ràng buộc phần cứng bộ nhớ ngoài FMC SDRAM và bộ quét màn hình LTDC trên Devicetree (`zephyr,display = &ltdc;`), cấu hình hệ thống cấp phát bộ nhớ đối tượng đồ họa **LVGL Memory Pool**, xây dựng giao diện táp-lô ô tô (Digital Instrument Cluster: Đồng hồ tốc độ Arc, Thanh nhiệt độ, Cảnh báo đèn báo nguy hiểm), và kiểm soát nguyên tắc **Thread-Safety** khi cập nhật dữ liệu từ mạng CAN Bus.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phân tầng hệ thống, cấu trúc Devicetree node, Kconfig symbols, kiến trúc render đồ họa và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 4 BƯỚC CHINH PHỤC NGÀY 9                                     │
├───────────────────┬───────────────────┬────────────────────────────┬────────────────────────────┤
│ BƯỚC 1: KIẾN TRÚC │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE ỨNG DỤNG   │ BƯỚC 4: PHỎNG VẤN          │
│ • Zephyr Display  │ • Soạn prj.conf   │ • Gắn nhãn file cụ thể     │ • Bộ 5 câu hỏi vặn LVGL    │
│   Architecture    │ • Viết overlay    │ • TODO 1 [prj.conf]        │   & Zephyr Display Driver  │
│ • LVGL Pipeline & │   FMC & LTDC Node │ • TODO 2 [app.overlay]     │ • LVGL Thread-Safety Bug   │
│   Flush Callback  │ • Cấu hình chosen │ • TODO 3 [gui_cluster.h]   │ • Partial vs Full Buffer   │
│ • Dirty Area Redraw│  zephyr,display  │ • TODO 4 [gui_cluster.c]   │ • Kịch bản trả lời 60s     │
│ • Mutex Bảo Vệ GUI│ • Cấp phát VDB    │ • TODO 5 [src/main.c]      │   (Elevator Pitch)         │
│                   │   trong Kconfig   │ • Mổ xẻ 5 Bug đồ họa       │                            │
└───────────────────┴───────────────────┴────────────────────────────┴────────────────────────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC ZEPHYR DISPLAY & LVGL CỐT LÕI (CHUYÊN CHO NGÀY 9)

| STT | Quy tắc Zephyr Display / LVGL | Thể hiện cụ thể trong Ngày 9 (Display & LVGL) |
| :---: | :--- | :--- |
| **1** | **Chosen Display Node** | Trong Devicetree, bắt buộc phải có node `chosen { zephyr,display = &ltdc; };` để Zephyr biết màn hình chính được điều khiển bởi ngoại vi nào. |
| **2** | **FMC SDRAM Dependency** | LTDC Framebuffer nằm trên SDRAM ngoài. Bắt buộc phải bật `CONFIG_MEMC=y` và `CONFIG_MEMC_STM32_SDRAM=y` trong `prj.conf` để Zephyr khởi tạo chip SDRAM trước khi LTDC quét màn hình. |
| **3** | **LVGL Non-Thread-Safe** | **QUY TẮC SỐNG CÒN:** Thư viện LVGL **KHÔNG thread-safe**. Tuyệt đối không gọi các hàm vẽ `lv_obj_set_...()` trực tiếp từ luồng CAN Worker. Bắt buộc phải bảo vệ hàm gọi bằng Mutex (`k_mutex_lock/unlock`) hoặc cơ chế truyền thông điệp Message Queue. |
| **4** | **Dedicated LVGL Loop** | Luồng quản lý đồ họa phải gọi `lv_timer_handler()` (hoặc `lv_task_handler()`) định kỳ mỗi $5\text{ms} - 10\text{ms}$ kèm lệnh ngủ `k_msleep()`. Không để luồng này chạy vòng lặp đói không ngủ (Busy-loop). |
| **5** | **Pixel Format Uniformity** | Khớp định dạng màu tuyệt đối giữa LTDC (RGB565) và LVGL: `CONFIG_LV_COLOR_DEPTH_16=y`. Nếu lệch hệ màu (ví dụ LVGL chạy 32-bit nhưng LTDC nhận 16-bit), màn hình sẽ bị nhiễu màu hoàn toàn. |
| **6** | **Memory Pool Sizing** | Cấp phát đủ `CONFIG_LV_Z_MEM_POOL_SIZE` (tối thiểu 16KB). Nếu tạo nhiều đối tượng đồ họa phức tạp (Arc, Meter, Chart) mà pool bị tràn, hàm `lv_..._create()` sẽ trả về `NULL`. |
| **7** | **Dirty Area Optimization** | Tận dụng cơ chế Invalidation của LVGL: Chỉ vẽ lại vùng có giá trị thay đổi (Dirty Rectangle), không vẽ lại toàn bộ màn hình để tiết kiệm băng thông bus AXI. |

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG & CƠ CHẾ HOẠT ĐỘNG (SYSTEM ARCHITECTURE)

## 1.1. Kiến Trúc Phân Tầng Giữa Zephyr Display Driver & Thư Viện LVGL

Thay vì phải tự viết driver khởi tạo thanh ghi LTDC và tự quản lý buffer như ở Ngày 4, **Zephyr RTOS tích hợp sẵn một tầng keo liên kết (Glue Layer) hoàn hảo giữa thư viện đồ họa LVGL và phần cứng STM32:**

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                         ỨNG DỤNG TÁP-LÔ Ô TÔ (DASHBOARD UI)                            │
│   • lv_arc_set_value(speed_arc, 85)       <-- Cập nhật góc quay đồng hồ tốc độ        │
│   • lv_label_set_text(speed_label, "85")  <-- Cập nhật số hiển thị km/h                │
└───────────────────────────────────────────▲────────────────────────────────────────────┘
                                            │ (Gọi hàm API đồ họa)
┌───────────────────────────────────────────┴────────────────────────────────────────────┐
│                        THƯ VIỆN ĐỒ HỌA LVGL CORE (lvgl/)                               │
│   • Quản lý cây đối tượng (Screen -> Arc -> Label -> Container)                        │
│   • Khối Invalidation: Đánh dấu các vùng chữ nhật bị bẩn (Dirty Regions)              │
│   • Khối Render: Chuyển đổi lệnh vẽ thành mảng pixel (RGB565) trong Virtual Buffer     │
└───────────────────────────────────────────▲────────────────────────────────────────────┘
                                            │ (Kích hoạt flush_cb)
┌───────────────────────────────────────────┴────────────────────────────────────────────┐
│                  ZEPHYR DISPLAY SUBSYSTEM GLUE (display_stm32_ltdc.c)                  │
│   • Nhận mảng pixel từ LVGL qua display_write(dev, x, y, &desc, buf)                   │
│   • Sử dụng DMA2D Chrom-ART tăng tốc copy vào Framebuffer SDRAM                        │
└───────────────────────────────────────────▲────────────────────────────────────────────┘
                                            │ (Quét dòng liên tục 60 FPS)
┌───────────────────────────────────────────┴────────────────────────────────────────────┐
│                             PHẦN CỨNG BÁN DẪN (HARDWARE)                               │
│   • LTDC Controller (Pixel Clock 9.6MHz) ──► Chân RGB 24-bit (PI14, PK7...)            │
│   • FMC SDRAM IS42S32400F 108MHz (Địa chỉ 0xC000 0000) ──► Chứa Framebuffer            │
│   • Màn hình Rocktech LCD 480x272                                                      │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 1.2. Cơ Chế Invalidation & Virtual Display Buffer (VDB)

Để màn hình $480 \times 272$ hiển thị mượt mà mà không tốn quá nhiều RAM nội SRAM:
1. **Virtual Display Buffer (VDB):** LVGL chỉ cần một vùng nhớ đệm tạm thời (ví dụ $480 \times 40$ dòng $= 38.4\text{ KB}$ RAM nội SRAM).
2. **Cơ chế Dirty Area:**
   * Khi kim đồng hồ tốc độ nhích từ $80 \rightarrow 85\text{ km/h}$, chỉ có một vùng hình chữ nhật nhỏ khoảng $60 \times 30$ pixel bị thay đổi.
   * LVGL **chỉ vẽ lại đúng vùng bẩn $60 \times 30$ pixel** này vào VDB, sau đó gọi hàm `display_write()` để đẩy đúng vùng đó ra Framebuffer ngoài SDRAM.
   * **Hiệu quả:** Giảm tải lưu lượng bus FMC SDRAM tới **$90\%$**, triệt tiêu hoàn toàn hiện tượng nghẽn bus với mạng CAN.

---

## 1.3. Vấn Nạn Xung Đột Luồng Đồ Họa (LVGL Concurrency Trap)

* **Bản chất:** Các hàm nội bộ của LVGL thao tác trên danh sách liên kết kép (Doubly Linked List) lưu trữ các đối tượng widget.
* **Nguy cơ sập hệ thống (Crash):** Nếu luồng `can_rx_worker_thread` nhận được frame tốc độ xe và lập tức gọi `lv_label_set_text()`, đúng vào lúc luồng `gui_thread` đang chạy `lv_timer_handler()` để vẽ dở dang $\implies$ Con trỏ danh sách liên kết bị gãy $\implies$ **HardFault ngay lập tức!**
* 👉 **Giải pháp kỹ thuật bắt buộc:** Phải dùng một Mutex hệ thống `k_mutex` để bọc quanh mọi lời gọi hàm LVGL.

```text
Luồng CAN Worker:    k_mutex_lock(&gui_mutex) ──► lv_label_set_text() ──► k_mutex_unlock(&gui_mutex)
Luồng GUI Render:    k_mutex_lock(&gui_mutex) ──► lv_timer_handler()  ──► k_mutex_unlock(&gui_mutex)
```

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

> 🎯 **NGUYÊN TẮC TRA CỨU ĐỒ HỌA & SDRAM TRÊN ZEPHYR RTOS:**
> 1. **Tra cứu Kconfig LVGL & LTDC:** Mở `zephyr/modules/lvgl/Kconfig` hoặc gõ `west build -t menuconfig` tìm kiếm `CONFIG_LVGL`, `CONFIG_STM32_LTDC`.
> 2. **Tra cứu Devicetree Bindings FMC & LTDC:** Mở `zephyr/dts/bindings/display/st,stm32-ltdc.yaml` và `zephyr/dts/bindings/memory-controllers/st,stm32-fmc-sdram.yaml`.
> 3. **Tra cứu Display & LVGL API:** Mở header `zephyr/include/zephyr/drivers/display.h` và thư viện đồ họa `lvgl.h`.

---

## 2.1. Lộ trình Tra cứu Trực tiếp Màn hình LTDC & LVGL trên Zephyr (Display Lookup Methodology)

### 📖 Kênh 1: Cách Tra Cứu Thuộc Tính Node `&fmc` và `&ltdc` (Devicetree Bindings)
1. **Mở file Binding của mạch điều khiển FMC SDRAM:**
   * Đường dẫn: **`zephyr/dts/bindings/memory-controllers/st,stm32-fmc-sdram.yaml`**.
   * Tra cứu các tham số đã tính từ Bare-metal Ngày 4: `refresh-rate = <1667>`, `power-up-delay = <100>`, `num-auto-refresh = <8>`, `mode-register = <0x230>`.
2. **Mở file Binding của bộ quét màn hình LTDC:**
   * Đường dẫn: **`zephyr/dts/bindings/display/st,stm32-ltdc.yaml`**.
   * Xem cấu hình timing màn hình 480x272: `width`, `height`, `hsync-len`, `vsync-len`, `hback-porch`, `vback-porch`.
3. **Khai báo node `chosen`:**
   * Gán `zephyr,display = &ltdc;` để báo cho hệ điều hành biết LTDC là ngõ xuất đồ họa mặc định.

### 📖 Kênh 2: Cách Tra Cứu Tùy Chọn Bộ Nhớ Đồ Họa Kconfig (`prj.conf`)
1. **Kích thước Buffer vẽ ảo (Virtual Display Buffer - VDB):**
   * Tìm kiếm `CONFIG_LV_Z_VDB_SIZE`: Xác định phần trăm chiều cao màn hình được cấp phát làm RAM đệm vẽ (ví dụ `20` = 20% màn hình, tương đương 54 dòng quét).
2. **Vùng nhớ động đối tượng giao diện (Memory Pool):**
   * Tìm kiếm `CONFIG_LV_Z_MEM_POOL_SIZE`: Cấp phát kích thước heap riêng cho LVGL tạo widget (ví dụ `16384` bytes = 16KB).

### 📖 Kênh 3: Cách Tra Cứu API Điều Khiển Hiển Thị
1. **Zephyr Display Subsystem:**
   * Mở file: **`zephyr/include/zephyr/drivers/display.h`** ➔ Xem hàm `display_get_capabilities()`, `display_write()`.
2. **LVGL GUI Engine:**
   * Mở file: **`lvgl.h`** ➔ Xem các hàm dựng giao diện xe hơi: `lv_meter_create()`, `lv_meter_set_scale_ticks()`, `lv_meter_add_needle_line()`, và hàm chạy bộ đếm thời gian `lv_timer_handler()`.

---

## 2.2. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS |
| :--- | :---: | :--- |
| **`CONFIG_DISPLAY`** | `y` | Bật hệ thống Display Subsystem của Zephyr. |
| **`CONFIG_STM32_LTDC`** | `y` | Bật driver điều khiển LTDC trên dòng chip STM32F7. |
| **`CONFIG_MEMC`** | `y` | Bật bộ điều khiển bộ nhớ ngoài (Memory Controller). |
| **`CONFIG_MEMC_STM32_SDRAM`** | `y` | Kích hoạt driver FMC SDRAM tự động khởi tạo chip RAM lúc boot. |
| **`CONFIG_LVGL`** | `y` | Tích hợp toàn bộ thư viện đồ họa LVGL vào dự án. |
| **`CONFIG_LV_COLOR_DEPTH_16`**| `y` | Định dạng màu 16-bit RGB565 tương thích màn hình LCD. |
| **`CONFIG_LV_Z_MEM_POOL_SIZE`**| `16384` | Cấp phát 16KB RAM cho LVGL Dynamic Object Pool. |
| **`CONFIG_LV_Z_VDB_SIZE`** | `20` | Virtual Display Buffer chiếm 20% màn hình (khoảng 54 dòng). |

---

## 2.3. Khai Báo Ràng Buộc Phần Cứng Trong File Overlay (`app.overlay`)

```dts
/ {
    chosen {
        zephyr,display = &ltdc;
    };
};

/* Kích hoạt bộ điều khiển bộ nhớ ngoài FMC SDRAM */
&fmc {
    status = "okay";
    sdram {
        status = "okay";
        power-up-delay = <100>;
        num-auto-refresh = <8>;
        mode-register = <0x230>;
        refresh-rate = <1667>;
    };
};

/* Kích hoạt khối quét màn hình đồ họa LTDC */
&ltdc {
    status = "okay";
    pinctrl-0 = <&ltdc_r0_pi15 &ltdc_r1_pj0 &ltdc_r2_pj1 &ltdc_r3_pj2
                 &ltdc_r4_pj3 &ltdc_r5_pj4 &ltdc_r6_pj5 &ltdc_r7_pj6
                 &ltdc_g0_pj7 &ltdc_g1_pj8 &ltdc_g2_pj9 &ltdc_g3_pj10
                 &ltdc_g4_pj11 &ltdc_g5_pk0 &ltdc_g6_pk1 &ltdc_g7_pk2
                 &ltdc_b0_pj12 &ltdc_b1_pj13 &ltdc_b2_pj14 &ltdc_b3_pj15
                 &ltdc_b4_pk3 &ltdc_b5_pk4 &ltdc_b6_pk5 &ltdc_b7_pk6
                 &ltdc_de_pk7 &ltdc_clk_pi14 &ltdc_hsync_pi12 &ltdc_vsync_pi13>;
    pinctrl-names = "default";

    clocks = <&rcc STM32_CLOCK_BUS_APB2 0x04000000>;
    
    width = <480>;
    height = <272>;
    def-refresh-rate = <60>;
};
```

---

# 💻 BƯỚC 3: GÕ CODE GIAO DIỆN & MỔ XẺ BUG ĐỒ HỌA (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 9

```text
zephyr_gateway/
├── app.overlay        <-- Ràng buộc chosen zephyr,display và cấu hình FMC/LTDC
├── prj.conf           <-- Bật CONFIG_DISPLAY, CONFIG_LVGL, cấu hình VDB
└── src/
    ├── gui_cluster.h  <-- Khai báo API giao diện táp-lô & Mutex bảo vệ
    ├── gui_cluster.c  <-- Tạo Widget Arc, Label, Bar và hàm cập nhật an toàn
    └── main.c         <-- Khởi tạo màn hình và luồng định kỳ lv_timer_handler
```

---

### 📂 KHỐI 1: FILE HEADER GIAO DIỆN TÁP-LÔ [ `src/gui_cluster.h` ]

#### TODO 1 [File: `src/gui_cluster.h`]: Khai Báo API Cập Nhật Giao Diện
```c
#ifndef GUI_CLUSTER_H
#define GUI_CLUSTER_H

#include <stdint.h>
#include <zephyr/kernel.h>

/* Khởi tạo toàn bộ các thành phần đồ họa táp-lô ô tô */
void GUI_Cluster_Init(void);

/* Cập nhật tốc độ xe (km/h) an toàn từ luồng bất kỳ */
void GUI_Cluster_UpdateSpeed(uint16_t speed_kmh);

/* Cập nhật vòng tua động cơ (RPM) an toàn từ luồng bất kỳ */
void GUI_Cluster_UpdateRPM(uint16_t rpm);

/* Mutex bảo vệ an toàn luồng cho toàn bộ thư viện LVGL */
extern struct k_mutex g_gui_mutex;

#endif /* GUI_CLUSTER_H */
```

---

### 📂 KHỐI 2: FILE SOURCE WIDGET ĐỒ HỌA [ `src/gui_cluster.c` ]

#### TODO 2 [File: `src/gui_cluster.c`]: Khởi Tạo Widget Đồng Hồ Vòng Cung (Arc)
```c
#include "gui_cluster.h"
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gui_cluster, LOG_LEVEL_INF);

/* Định nghĩa Mutex dùng chung toàn hệ thống */
K_MUTEX_DEFINE(g_gui_mutex);

static lv_obj_t *s_speed_arc;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_rpm_bar;

void GUI_Cluster_Init(void)
{
    k_mutex_lock(&g_gui_mutex, K_FOREVER);

    /* 1. Thiết lập màu nền toàn màn hình màu xám đen sang trọng */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

    /* 2. Tạo Widget Đồng Hồ Tốc Độ (Arc Widget) */
    s_speed_arc = lv_arc_create(scr);
    lv_obj_set_size(s_speed_arc, 180, 180);
    lv_obj_align(s_speed_arc, LV_ALIGN_CENTER, 0, -10);
    lv_arc_set_rotation(s_speed_arc, 135);
    lv_arc_set_bg_angles(s_speed_arc, 0, 270);
    lv_arc_set_range(s_speed_arc, 0, 240); /* Tối đa 240 km/h */
    lv_arc_set_value(s_speed_arc, 0);
    lv_obj_remove_style(s_speed_arc, NULL, LV_PART_KNOB); /* Ẩn núm vặn */
    lv_obj_clear_flag(s_speed_arc, LV_OBJ_FLAG_CLICKABLE); /* Khóa cảm ứng */

    /* Định dạng màu sắc kim đồng hồ (Xanh Cyan) */
    lv_obj_set_style_arc_color(s_speed_arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_speed_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_speed_arc, lv_color_hex(0x2A323D), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_speed_arc, 12, LV_PART_MAIN);

    /* 3. Tạo Label Hiển Thị Số Tốc Độ Ở Tâm */
    s_speed_label = lv_label_create(scr);
    lv_label_set_text(s_speed_label, "0");
    lv_obj_set_style_text_color(s_speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_speed_label, &lv_font_montserrat_28, 0);
    lv_obj_align(s_speed_label, LV_ALIGN_CENTER, 0, -15);

    /* Label đơn vị km/h */
    lv_obj_t *unit_label = lv_label_create(scr);
    lv_label_set_text(unit_label, "KM/H");
    lv_obj_set_style_text_color(unit_label, lv_color_hex(0x8A9BA8), 0);
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 15);

    /* 4. Tạo Thanh Đo Vòng Tua Động Cơ (RPM Bar) ở đáy màn hình */
    s_rpm_bar = lv_bar_create(scr);
    lv_obj_set_size(s_rpm_bar, 300, 10);
    lv_obj_align(s_rpm_bar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(s_rpm_bar, 0, 8000); /* 0 - 8000 RPM */
    lv_bar_set_value(s_rpm_bar, 1000, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_rpm_bar, lv_color_hex(0xFF3D00), LV_PART_INDICATOR);

    k_mutex_unlock(&g_gui_mutex);
    LOG_INF("Khởi tạo toàn bộ đối tượng đồ họa Cluster thành công!");
}

void GUI_Cluster_UpdateSpeed(uint16_t speed_kmh)
{
    /* BẮT BUỘC: KHÓA MUTEX TRƯỚC KHI THAO TÁC TRÊN ĐỐI TƯỢNG LVGL */
    k_mutex_lock(&g_gui_mutex, K_FOREVER);

    if (speed_kmh > 240) {
        speed_kmh = 240;
    }
    lv_arc_set_value(s_speed_arc, speed_kmh);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", speed_kmh);
    lv_label_set_text(s_speed_label, buf);

    k_mutex_unlock(&g_gui_mutex);
}

void GUI_Cluster_UpdateRPM(uint16_t rpm)
{
    k_mutex_lock(&g_gui_mutex, K_FOREVER);
    lv_bar_set_value(s_rpm_bar, rpm, LV_ANIM_ON);
    k_mutex_unlock(&g_gui_mutex);
}
```

---

### 📂 KHỐI 3: LUỒNG QUẢN TRỊ ĐỒ HỌA [ `src/main.c` ]

#### TODO 3 [File: `src/main.c`]: Khởi Động Vòng Lặp Render Định Kỳ
```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
#include "gui_cluster.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

#define GUI_THREAD_STACK_SIZE 4096
#define GUI_THREAD_PRIORITY   6 /* Mức ưu tiên đồ họa trung bình */

void gui_render_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Lỗi: Màn hình LCD LTDC chưa sẵn sàng!");
        return;
    }

    /* Khởi tạo giao diện táp-lô */
    GUI_Cluster_Init();

    LOG_INF("Luồng Render đồ họa LVGL bắt đầu hoạt động...");

    while (1) {
        /* BƯỚC QUAN TRỌNG: KHÓA MUTEX KHI GỌI BỘ LẬP LỊCH LVGL TIMER HANDLER */
        k_mutex_lock(&g_gui_mutex, K_FOREVER);
        lv_timer_handler();
        k_mutex_unlock(&g_gui_mutex);

        /* Ngủ 10ms (Tương đương tốc độ làm tươi tối đa 100 FPS) */
        k_msleep(10);
    }
}

K_THREAD_DEFINE(gui_tid, GUI_THREAD_STACK_SIZE,
                gui_render_thread, NULL, NULL, NULL,
                GUI_THREAD_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("Khởi động hệ thống Dashboard Display Gateway...");
    
    /* Vòng lặp test cập nhật số liệu giả lập */
    uint16_t test_speed = 0;
    while (1) {
        k_sleep(K_MSEC(100));
        test_speed = (test_speed + 2) % 240;
        GUI_Cluster_UpdateSpeed(test_speed);
        GUI_Cluster_UpdateRPM(test_speed * 30);
    }
    return 0;
}
```

---

## 3.2. Mổ xẻ 5 Bug Đồ Họa "Kinh Điển" trong Ngày 9

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG ZEPHYR DISPLAY & LVGL                            │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA PHẦN MỀM / PHẦN CỨNG   │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. Màn hình sọc   │ Quên `CONFIG_MEMC=y` khiến bộ điều khiển  │ Bật đầy đủ `CONFIG_MEMC=y` và   │
│    nhiễu ngẫu nhiên│ SDRAM ngoài không nạp lệnh JEDEC init.    │ `CONFIG_MEMC_STM32_SDRAM=y`.    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. HardFault ngẫu │ Luồng CAN gọi thẳng hàm LVGL mà không có  │ Luôn bọc mọi hàm LVGL bằng      │
│    nhiên khi chạy │ Mutex, làm gãy con trỏ danh sách Object.  │ `k_mutex_lock(&gui_mutex)`.     │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Sai lệch hệ màu│ LVGL nạp màu ARGB8888 trong khi LTDC chỉ  │ Cài `CONFIG_LV_COLOR_DEPTH_16=y`│
│    (Đỏ thành Xanh)│ nhận định dạng 16-bit RGB565.             │ đồng bộ tuyệt đối với LTDC.     │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Widget không   │ Bộ nhớ `CONFIG_LV_Z_MEM_POOL_SIZE` bị     │ Tăng kích thước Pool lên 16KB   │
│    hiển thị (NULL)│ cạn kiệt, không cấp phát được đối tượng.  │ hoặc 32KB trong file `prj.conf`.│
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Giao diện giật │ Gọi `lv_timer_handler()` mà không có lệnh │ Thêm `k_msleep(10);` sau mỗi lần│
│    đơ (Lag/Freeze)│ `k_msleep()`, chiếm dụng 100% CPU.        │ chạy vòng lặp handler.          │
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Tại sao thư viện đồ họa LVGL lại không Thread-Safe? Giải pháp kiến trúc an toàn trong RTOS là gì?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * LVGL được thiết kế tối ưu hóa bộ nhớ cho hệ thống nhúng (Microcontrollers), do đó nó không tích hợp sẵn các cơ chế khóa Semaphore hay Mutex bên trong từng hàm để tiết kiệm dung lượng RAM và chu kỳ lệnh. Cây đối tượng đồ họa (Object Tree) và hệ thống Style sử dụng các danh sách liên kết dùng chung.
  * Nếu nhiều luồng (ví dụ luồng CAN Worker và luồng GUI Timer) cùng sửa đổi thuộc tính đối tượng hoặc duyệt cây vẽ cùng lúc, xung đột cuộc đua (Race Condition) sẽ phá hủy các con trỏ liên kết và gây lỗi `MemManage` hoặc `HardFault`.
  * **Giải pháp chuẩn:** Có 2 phương án:
    1. **Sử dụng Mutex (`k_mutex`):** Bọc toàn bộ các lệnh truy cập LVGL và hàm `lv_timer_handler()` bằng cùng một Mutex.
    2. **Mô hình Hàng đợi (Actor Pattern):** Luồng ngoài chỉ gửi dữ liệu số thô vào một Message Queue (`k_msgq`), luồng GUI là luồng duy nhất được phép lấy dữ liệu ra và gọi hàm cập nhật widget.

### ❓ Câu 2: Cơ chế "Dirty Area Invalidation" của LVGL hoạt động như thế nào?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Khi một widget bị sửa đổi (ví dụ kim đồng hồ đổi góc quay), LVGL không xóa toàn bộ màn hình để vẽ lại. Nó tính toán tọa độ bao quanh phần diện tích bị ảnh hưởng và đánh dấu đó là một "Vùng bẩn" (Dirty Area).
  * Trong chu kỳ gọi `lv_timer_handler()`, LVGL chỉ render đúng các pixel nằm trong vùng bẩn này vào bộ đệm ảo VDB, sau đó phát tín hiệu qua hàm `display_write()` tới driver phần cứng LTDC. Điều này giúp giảm thiểu tới $80\% - 95\%$ khối lượng dữ liệu phải trung chuyển qua bus AXI và bộ nhớ SDRAM ngoài, giải phóng băng thông cho các ngoại vi truyền thông khác.

### ❓ Câu 3: Làm thế nào để đảm bảo chip SDRAM ngoài được khởi tạo đúng lúc trên Zephyr RTOS?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * Zephyr quản lý thứ tự khởi động driver bằng thông số **Init Priority**.
  * Driver điều khiển bộ nhớ ngoài FMC SDRAM (`drivers/memc/memc_stm32_sdram.c`) được đăng ký với mức ưu tiên `CONFIG_MEMC_INIT_PRIORITY` (mặc định là mức rất sớm: `POST_KERNEL, 0`).
  * Trong khi đó, driver quét màn hình LTDC (`drivers/display/display_stm32_ltdc.c`) khởi động ở mức muộn hơn (`POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY`). Điều này đảm bảo chuỗi lệnh cấu hình JEDEC (Precharge, Auto-Refresh, Mode Register) của SDRAM đã hoàn tất $100\%$ trước khi LTDC phát lệnh đọc Framebuffer đầu tiên ra màn hình.

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Tại Ngày 9, em xây dựng hệ thống hiển thị táp-lô ô tô kỹ thuật số bằng cách tích hợp thư viện **LVGL** lên nền tảng **Zephyr Display Subsystem**.  
> Em cấu hình Devicetree ràng buộc chip bộ nhớ ngoài **FMC SDRAM** và bộ điều khiển **LTDC** ($480 \times 272$), tận dụng cơ chế **Virtual Display Buffer (VDB)** chiếm vỏn vẹn 20% khung hình trong RAM nội để giảm thiểu tối đa tài nguyên SRAM cần thiết.  
> Để bảo vệ tính toàn vẹn hệ thống trong môi trường đa luồng thời gian thực, em thiết lập cơ chế đồng bộ hóa luồng nghiêm ngặt bằng **`k_mutex`**, đảm bảo luồng CAN Worker và luồng Render đồ họa không bao giờ xung đột tài nguyên trên cây đối tượng LVGL. Kết quả là giao diện táp-lô hiển thị kim đồng hồ Arc và thanh vòng tua máy chuyển động mượt mà ở tần số quét cao mà không hề gây nghẽn bus bộ nhớ."*
