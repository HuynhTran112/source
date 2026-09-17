# 🎬 HƯỚNG DẪN TẠO DỮ LIỆU & CHUẨN BỊ VIDEO CHO STM32F746G-DISCO

Tài liệu chi tiết hướng dẫn chọn nguồn video, tiêu chuẩn kích thước, tỷ lệ khung hình, định dạng thẻ nhớ MicroSD FAT32 và chuyển đổi video thành file nhị phân `.BIN` chuẩn RGB565 $480 \times 272$ cho trình phát Video Bare-Metal 60 FPS trên STM32F746G-DISCO.

---

## 📥 1. Nguồn Tải Video Miễn Phí & Chất Lượng Cao (Where to Download)

Để có video demo đẹp mắt nhất, bạn nên tải các đoạn video ngắn (khoảng 10 đến 30 giây), độ phân giải **720p hoặc 1080p** (tỷ lệ chuẩn 16:9), miễn phí bản quyền từ các nguồn uy tín sau:

### A. Các Website Tải Stock Video Ngắn Miễn Phí (Royalty-Free)
1. **[Pexels Videos](https://www.pexels.com/videos/):**
   - Rất nhiều video chuyển động mượt, quay chậm (slow motion), phong cảnh thiên nhiên, đường phố.
   - Gõ từ khóa gợi ý: `neon`, `car driving`, `abstract 3d`, `nature waterfall`, `cyberpunk`.
   - Chọn tải ở độ phân giải: **HD 720p** hoặc **Full HD 1080p** (định dạng `.mp4`).
2. **[Pixabay Videos](https://pixabay.com/videos/):**
   - Kho đồ họa chuyển động Motion Graphics và Animation cực kỳ phong phú.
   - Gõ từ khóa gợi ý: `animation`, `technology loop`, `car race`, `particles`.
3. **[Mixkit](https://mixkit.co/free-stock-video/):**
   - Chuyên các clip ngắn 10 - 15 giây thiết kế sẵn dạng vòng lặp (loop), rất hợp để làm video demo nhúng.

### B. Các Video Benchmark Huyền Thoại Cho Đồ Họa & Nhúng
* **Big Buck Bunny (Blender Foundation):** Video hoạt hình 3D kinh điển được mọi hãng bán dẫn (STMicroelectronics, NXP, TI) dùng làm video mẫu để test màn hình TFT LCD và bộ tăng tốc đồ họa Chrom-ART.
* **Bad Apple (PV 60 FPS):** Video chuyển động đen trắng tương phản cực cao ở tốc độ 60 FPS, dùng để kiểm chứng khả năng quét không rách hình (VSYNC reload) và đo đạc không sụt khung hình (0 dropped frames).
* **Đoạn video Camera hành trình / Xe chạy (Dashcam / Automotive driving):** Cực kỳ phù hợp để đồng bộ phong cách với dự án **Automotive CAN Gateway** trên CV của bạn!

---

## 🎨 2. Nên Tải Video Loại Gì? (Tiêu Chuẩn Lựa Chọn Video Tối Ưu)

Màn hình LCD Rocktech RK043FN48H trên kit Discovery sử dụng tấm nền màu **16-bit RGB565** (5 bit Red, 6 bit Green, 5 bit Blue = 65,536 màu). Để hình ảnh hiển thị đẹp nhất, bạn nên chọn video theo các tiêu chí sau:

### ✅ NÊN CHỌN:
1. **Video Hoạt Hình 3D / Motion Graphics / Anime:** 
   - Có các mảng màu đậm, biên độ sáng rõ ràng, độ tương phản cao. 
   - Khi hiển thị trên màn hình LCD 4.3 inch sẽ cực kỳ rực rỡ, sắc nét, tạo hiệu ứng thị giác ấn tượng (WOW Effect) khi đưa cho người phỏng vấn xem.
2. **Video Chuyển Động Nhanh (Xe đua, thể thao, nước bắn):**
   - Mục đích kỹ thuật: Chứng minh vi điều khiển kéo mượt 60 FPS, mắt thường nhìn thấy rõ sự mượt mà và không hề bị xé hình (Tearing).
3. **Video Có Tỷ Lệ Gốc 16:9:**
   - Màn hình STM32F746 có tỷ lệ $480 / 272 \approx 1.765$ (gần như trùng khớp hoàn toàn với $16:9 \approx 1.777$). Khi script Python co giãn về $480 \times 272$, hình ảnh không bị méo hay biến dạng tỷ lệ.

### ❌ NÊN TRÁNH:
* **Video Cảnh Đêm / Quá Tối:** Chuẩn RGB565 chỉ có 5 bit cho màu Đỏ/Xanh dương nên các vùng tối chuyển tiếp dần về màu đen sẽ xuất hiện hiện tượng phân tầng dải màu (Color Banding).
* **Video Có Quá Nhiều Chữ Nhỏ (Subtitles):** Chữ quá nhỏ khi thu nhỏ từ 1080p về 272 pixel dọc sẽ bị nhòe và khó đọc.

---

## ⏱️ 3. Kích Thước & Thời Lượng Video Bao Nhiêu Là Chuẩn?

### A. Công Thức Tính Dung Lượng File Binary `.BIN` Thô
Trong dự án này, video không dùng nén H.264 (để tránh quá tải CPU) mà lưu trữ dưới dạng chuỗi các frame RGB565 thô:
* **Dung lượng 1 khung hình:** 
  $$480 \times 272 \times 2\text{ bytes} = 261,120\text{ bytes} \approx 255\text{ KB}$$
* **Băng thông ở 30 FPS:**
  $$255\text{ KB} \times 30\text{ frames} \approx 7.65\text{ MB / giây}$$
* **Băng thông ở 60 FPS:**
  $$255\text{ KB} \times 60\text{ frames} \approx 15.3\text{ MB / giây}$$

### B. Bảng Khuyến Nghị Thời Lượng & Dung Lượng File:

| Tốc Độ Khung Hình | Thời Lượng Video | Dung Lượng File `.BIN` | Đánh Giá Kỹ Thuật |
| :--- | :--- | :--- | :--- |
| **60 FPS** | **15 giây** | **$\approx 230\text{ MB}$** | **Khuyên dùng số 1:** Cực mượt, phô diễn trọn vẹn tốc độ đọc SDMMC 18 MB/s. |
| **60 FPS** | **20 giây** | **$\approx 307\text{ MB}$** | Rất tốt, phù hợp demo một đoạn clip hoàn chỉnh. |
| **30 FPS** | **30 giây** | **$\approx 230\text{ MB}$** | Băng thông thấp (7.65 MB/s), an toàn tuyệt đối, thời gian xem dài hơn. |
| **60 FPS** | **> 1 phút** | **$> 920\text{ MB}$** | *Không khuyến khích:* Tốn dung lượng thẻ nhớ, copy lâu, không cần thiết khi phỏng vấn. |

> **Khuyến nghị chuẩn bị 3 video mẫu vào thẻ nhớ:**
> * `VIDEO1.BIN` (Hoạt hình 3D rực rỡ, 15s, 60 FPS $\approx 230\text{ MB}$)
> * `VIDEO2.BIN` (Xe hơi chạy / Chuyển động cao, 15s, 60 FPS $\approx 230\text{ MB}$)
> * `VIDEO3.BIN` (Phong cảnh thiên nhiên / Color Bar đồ họa, 20s, 30 FPS $\approx 150\text{ MB}$)
> 
> *Tổng dung lượng 3 file chỉ khoảng $\approx 610\text{ MB}$, chiếm chưa tới 10% thẻ 8GB. Thẻ nhớ đọc cực nhanh và nút bấm User Button (PI11) chuyển video luân phiên trong tích tắc!*

---

## 💾 4. Định Dạng Thẻ Nhớ MicroSD Sang FAT32 Chuẩn Công Nghiệp

### Bước 1: Format Thẻ Với Cluster Size Lớn (32KB / 64KB)
1. Cắm thẻ nhớ SanDisk 8GB / 16GB vào máy tính qua đầu đọc thẻ USB.
2. Mở **File Explorer** $\rightarrow$ Chuột phải vào ổ đĩa thẻ nhớ $\rightarrow$ chọn **Format...**
3. Cấu hình các thông số:
   - **File system:** `FAT32` (Default).
   - **Allocation unit size:** `32 kilobytes` hoặc `64 kilobytes`.  
     *(Cơ sở kỹ thuật: Cluster size lớn 32KB/64KB giúp giảm số lần tra cứu bảng FAT, tăng tốc độ đọc tuần tự đa khối Multi-Block DMA của SDMMC lên trên $18\text{ MB/s}$, triệt tiêu hoàn toàn hiện tượng nghẽn I/O).*
   - **Format options:** Tích chọn *Quick Format*.
4. Nhấn **Start** để hoàn tất định dạng trong 3 giây.

---

## 🛠️ 5. Hướng Dẫn Chuyển Đổi Video Bằng Script Python

Công cụ chuyển đổi đã được tích hợp sẵn ngay trong thư mục dự án tại [`tools/convert_video.py`](file:///d:/Project/STM32F7/tools/convert_video.py).

### Bước 1: Cài Đặt Thư Viện Python (Chỉ làm 1 lần)
Mở PowerShell hoặc Command Prompt và chạy:
```bash
pip install opencv-python numpy
```

### Bước 2: Chuyển Đổi Video MP4 Sang File Nhị Phân `.BIN`

#### Cách 1: Xuất video 60 FPS (Độ mượt cực đại)
```powershell
# Chuyển đổi clip MP4 thành VIDEO1.BIN ở tốc độ 60 FPS:
python tools/convert_video.py "D:\Videos\my_clip1.mp4" "VIDEO1.BIN" --fps 60
```

#### Cách 2: Xuất video 30 FPS (Tiết kiệm băng thông)
```powershell
# Chuyển đổi clip MP4 thành VIDEO2.BIN ở tốc độ 30 FPS:
python tools/convert_video.py "D:\Videos\my_clip2.mp4" "VIDEO2.BIN" --fps 30
```

### Bước 3: Copy File Vào Thẻ Nhớ
1. Copy các file đã chuyển đổi (`VIDEO1.BIN`, `VIDEO2.BIN`, `VIDEO3.BIN`) vào **thư mục gốc (Root Directory)** của thẻ nhớ (ví dụ: `E:\VIDEO1.BIN`).
2. Nhấn chuột phải vào ổ thẻ nhớ $\rightarrow$ chọn **Eject** (rút an toàn) để đảm bảo dữ liệu ghi vào thẻ không bị hỏng cache.
3. Cắm thẻ vào khe MicroSD (khe cắm kim loại nằm ở mặt dưới kit STM32F746G-DISCO).

---

## 🎮 6. Trải Nghiệm Trên Kit STM32F746G-DISCO

1. **Khởi động nguồn:**
   - Cắm cáp Micro-USB vào cổng ST-LINK trên board.
   - Màn hình LCD lập tức hiển thị Splash Screen với thanh tiến trình nạp hệ thống.
2. **Phát Video:**
   - Hệ thống tự động mount hệ thống file FAT32, quét tìm `VIDEO1.BIN` và phát mượt mà ở tốc độ 60 FPS không một vệt xé hình.
3. **Chuyển đổi video bằng Nút nhấn Phần cứng:**
   - Nhấn **Nút User Button màu xanh dương (chân PI11)** trên board: Hệ thống sẽ đóng file hiện tại và chuyển luân phiên sang video tiếp theo:  
     $$\text{VIDEO1.BIN} \longrightarrow \text{VIDEO2.BIN} \longrightarrow \text{VIDEO3.BIN} \longrightarrow \text{VIDEO1.BIN}$$
4. **Cơ chế An Toàn (Fault-Tolerant Benchmark):**
   - Nếu rút thẻ nhớ hoặc không tìm thấy file video: Hệ thống tự động kích hoạt chế độ biểu diễn đồ họa Benchmark Chrom-ART 60 FPS (Thanh dải màu Color Bar và khối đồ họa Sprite nảy trên màn hình).
