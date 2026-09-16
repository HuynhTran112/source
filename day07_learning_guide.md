# 🏆 [NGÀY 7] CẨM NANG TOÀN DIỆN ZEPHYR RTOS: BOARD BRING-UP, DEVICETREE OVERLAY & HỆ THỐNG ĐA LUỒNG (MULTI-THREADING)
## Lộ trình 4 Bước: Kiến Trúc RTOS ➔ Thực Chiến Devicetree/Kconfig ➔ Gõ Code Ứng Dụng ➔ Phỏng Vấn Chuyên Sâu

> **Mục tiêu:** Chuyển dịch toàn diện từ tư duy Bare-metal sang hệ điều hành thời gian thực **Zephyr RTOS** trên STM32F746: Làm chủ cơ chế tách biệt phần cứng bằng cây thiết bị **Devicetree (`.dts`/`.overlay`)** và bộ cấu hình nhân **Kconfig (`prj.conf`)**, cấu hình ánh xạ chân vật lý qua **Pinctrl**, tạo và điều phối đa luồng **Kernel Multi-Threading (`k_thread`)**, thiết lập lá chắn bảo vệ tràn ngăn xếp bằng phần cứng **`CONFIG_MPU_STACK_GUARD`** và hệ thống nhật ký bất đồng bộ **Zephyr Logging (`LOG_MODULE_REGISTER`)**.  
> **Nguyên tắc kỹ thuật:** **Đi thẳng vào cơ chế phân tầng hệ thống, cấu trúc Devicetree node, Kconfig symbols, bảng so sánh tài nguyên và phân chia file rõ ràng — KHÔNG dùng ví dụ ẩn dụ ngoài lề dài dòng.**

---

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           LỘ TRÌNH 5 BƯỚC CHINH PHỤC NGÀY 7                                     │
├───────────────────┬───────────────────┬───────────────────┬────────────────────────────┬────────┤
│ BƯỚC 0: TOOLCHAIN │ BƯỚC 1: KIẾN TRÚC │ BƯỚC 2: THỰC CHIẾN│ BƯỚC 3: GÕ CODE ỨNG DỤNG   │ BƯỚC 4:│
│ • Cài đặt Host    │ • Triết lý Zephyr │ • Soạn prj.conf   │ • Gắn nhãn file cụ thể     │ PHỎNG  │
│   tools (CMake...)│ • Devicetree vs   │ • Viết overlay    │ • TODO 1 [prj.conf]        │ VẤN    │
│ • Python & West   │   Compile Macros  │ • Bảng Kconfig    │ • TODO 2 [app.overlay]     │ • Bộ 5 │
│ • Zephyr SDK ARM  │ • Multi-threading │ • Pinctrl Node    │ • TODO 3 [CMakeLists.txt]  │   câu  │
│ • Sanity Blinky   │ • MPU Stack Guard │ • Khóa Device     │ • TODO 4-5 [src/main.c]    │   hỏi  │
│   stm32f746g_disco│ • Zephyr Logging  │   Binding API     │ • Mổ xẻ 5 Bug hệ thống     │   vặn  │
└───────────────────┴───────────────────┴───────────────────┴────────────────────────────┴────────┘
```

---

## 🛠️ RÀ SOÁT 7 QUY TẮC ZEPHYR RTOS CỐT LÕI (CHUYÊN CHO NGÀY 7)

| STT | Quy tắc Zephyr RTOS | Thể hiện cụ thể trong Ngày 7 (Board Bring-Up) |
| :---: | :--- | :--- |
| **1** | **Compile-Time Evaluation** | Devicetree trong Zephyr **hoàn toàn được tính toán lúc biên dịch (Compile-time)**. Mọi hàm `DT_PROP` hay `DEVICE_DT_GET` sinh ra hằng số tĩnh, tiêu thụ 0 byte RAM và 0 chu kỳ khởi động. |
| **2** | **Device Ready Check** | Trước khi dùng bất kỳ ngoại vi nào lấy từ `DEVICE_DT_GET`, **BẮT BUỘC** phải gọi `device_is_ready(dev)`. Tuyệt đối không gọi hàm API khi thiết bị chưa sẵn sàng (tránh lỗi Kernel Oops / HardFault). |
| **3** | **Explicit Thread Stack** | Khai báo ngăn xếp cho Thread bằng macro `K_THREAD_STACK_DEFINE(name, size)`. Tuyệt đối không dùng mảng C thông thường `uint8_t stack[size]` vì sẽ làm sai lệch căn lề (Alignment) của phần cứng MPU. |
| **4** | **MPU Stack Protection** | Luôn bật `CONFIG_MPU_STACK_GUARD=y` trong `prj.conf`. Phần cứng MPU sẽ đặt 1 trang nhớ 32 bytes cấm ghi (Guard Region) ở đáy ngăn xếp để bắt trọn vẹn lỗi Stack Overflow ngay chu kỳ ghi đầu tiên. |
| **5** | **Preemptive vs Cooperative** | Phân biệt Priority: Giá trị âm (từ `-CONFIG_NUM_COOP_PRIO` đến `-1`) là luồng Cooperative (không bị chen ngang), giá trị dương (từ `0` đến `CONFIG_NUM_PREEMPT_PRIO-1`) là luồng Preemptive. |
| **6** | **Deferred Logging** | Dùng `CONFIG_LOG_MODE_DEFERRED=y`. Tuyệt đối không dùng `printf` thô hay log Blocking trong ISR/Real-time threads để không làm trôi thời gian thực (Jitter). |
| **7** | **Pinctrl Dependency** | Trong STM32 Devicetree, chân GPIO không được cấu hình tự do bằng thanh ghi trong driver mà phải được khai báo tập trung trong node `&pinctrl` của file overlay. |

---

# ⚙️ BƯỚC 0: HƯỚNG DẪN CÀI ĐẶT MÔI TRƯỜNG PHÁT TRIỂN ZEPHYR RTOS (TOOLCHAIN & SDK SETUP)

> **Tầm quan trọng:** Khác với Bare-metal (chỉ cần cài ARM GCC và Make), Zephyr RTOS là một hệ sinh thái mã nguồn mở hiện đại gồm hàng trăm module phần mềm. Việc thiết lập đúng công cụ quản trị đa kho mã nguồn **`west`**, bộ biên dịch chéo chính thức **Zephyr SDK** và trình biên dịch cây thiết bị **DTC** là điều kiện tiên quyết bắt buộc trước khi bước vào viết code!

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                        HỆ THỐNG CÔNG CỤ XÂY DỰNG ZEPHYR RTOS (BUILD SYSTEM)                      │
├───────────────────┬─────────────────────────────────────────────────────────────────────────────┤
│ 1. Python 3.10+   │ Môi trường chạy công cụ `west`, kconfiglib, và bộ tiền xử lý Devicetree    │
│ 2. West Meta-Tool │ Quản lý Git repository đa repo, tải module con, gọi lệnh build và nạp flash │
│ 3. CMake & Ninja  │ Hệ thống sinh file biên dịch và thực thi build code song song siêu tốc      │
│ 4. DTC (devicetree│ Trình biên dịch cú pháp cây thiết bị từ `.dts`/`.overlay` sang mã C macro   │
│ 5. Zephyr SDK     │ Bộ Cross-Compiler (`arm-zephyr-eabi-gcc`), GDB Debugger, Newlib C library   │
└───────────────────┴─────────────────────────────────────────────────────────────────────────────┘
```

> 📖 **Hướng Dẫn Tra Cứu Tài Liệu Cài Đặt Chính Thức Từ Zephyr Project:**
> 1. **Mở trình duyệt truy cập:** `https://docs.zephyrproject.org/latest/develop/getting_started/index.html`
> 2. **Tìm các đề mục cốt lõi:**
>    * Mục 1: **Install dependencies** (Chọn tab tương ứng với hệ điều hành Windows hoặc Linux / Ubuntu).
>    * Mục 2: **Get Zephyr and install Python dependencies** (Lệnh `west init` và `west update`).
>    * Mục 3: **Install Zephyr SDK** (Tải bundle `zephyr-sdk-0.16.8` hoặc dùng `west sdk install`).
>    * Mục 4: **Build your first sample application** (Kiểm tra với ứng dụng mẫu `samples/basic/blinky`).

---

### 0.1. Cài Đặt Các Công Cụ Nền Tảng (Host Tools)

#### Cách 1: Trên Hệ Điều Hành Windows (PowerShell)
Mở **PowerShell dưới quyền Administrator** (Chuột phải vào PowerShell/Terminal chọn **Run as Administrator**) và sử dụng trình quản lý gói **Chocolatey**:
```powershell
# 1. Dọn dẹp thư mục Chocolatey hỏng nếu lần cài trước bị gián đoạn/thiếu quyền Admin
Remove-Item -Recurse -Force "C:\ProgramData\chocolatey" -ErrorAction SilentlyContinue

# 2. Cài đặt trình quản lý gói Chocolatey (nếu máy tính chưa có)
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# 3. Nạp lại biến môi trường PATH vào phiên làm việc hiện tại
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# 4. Cài đặt toàn bộ bộ công cụ nền tảng cho Zephyr
choco install -y cmake --installargs 'ADD_CMAKE_TO_PATH=System'
choco install -y ninja gperf python git dtc-msys2
```

> 💡 **Mẹo:** Nếu máy không có quyền Administrator, bạn có thể dùng lệnh `winget install Kitware.CMake Ninja-build.Ninja Python.Python.3.11 Git.Git` tích hợp sẵn trên Windows.

#### Cách 2: Trên Hệ Điều Hành Linux / Ubuntu (hoặc Windows WSL2)
```bash
# Cập nhật danh sách gói và cài đặt các công cụ biên dịch
sudo apt update
sudo apt install -y --no-install-recommends \
  git cmake ninja-build gperf ccache dfu-util \
  device-tree-compiler wget python3-dev python3-pip \
  python3-venv xz-utils file make gcc gcc-multilib \
  g++-multilib libsdl2-dev libmagic1
```

---

### 0.2. Thiết Lập Môi Trường Ảo Python & Cài Đặt Meta-Tool `west` Trên Ổ D:\

> 💡 **Khuyến nghị lưu trữ:** Toàn bộ mã nguồn Zephyr, module mở rộng (HAL ST, LVGL, CMSIS) và Zephyr SDK chiếm khoảng **5 GB đến 8 GB**. Do đó, hướng dẫn chuẩn dưới đây sẽ thiết lập **trực tiếp 100% lên ổ `D:\`** để bảo vệ dung lượng ổ hệ điều hành `C:\`. *(Nếu máy bạn chỉ có ổ `C:\` hoặc chạy Linux, chỉ cần thay `D:\` thành `C:\` hoặc `~/`)*.

> ⚠️ **Lưu ý phiên bản Python:** Zephyr v3.7.0 bắt buộc tối thiểu **Python 3.10 trở lên** (khuyên dùng Python 3.11 hoặc 3.12). Hãy kiểm tra bằng lệnh `python --version` trước khi tạo môi trường ảo.

Mở **PowerShell** và thực hiện:
```powershell
# 1. Chuyển dấu nhắc lệnh sang ổ đĩa D và tạo thư mục làm việc
D:
mkdir D:\zephyrproject
cd D:\zephyrproject

# 2. Tạo môi trường ảo Python riêng biệt tại D:\zephyrproject\.venv
python -m venv D:\zephyrproject\.venv

# 3. Kích hoạt môi trường ảo (Dấu nhắc sẽ hiện (.venv) ở đầu dòng)
D:\zephyrproject\.venv\Scripts\Activate.ps1

# 4. Nâng cấp pip và cài đặt công cụ quản trị đa kho mã nguồn west
# LƯU Ý TRÊN WINDOWS: Bắt buộc dùng `python -m pip` thay vì `pip install --upgrade pip`
# để tránh lỗi [WinError 5] Access is denied do Windows khóa file pip.exe đang chạy!
python -m pip install --upgrade pip
pip install west
```

> 🛠️ **Xử lý sự cố nếu pip bị lỗi `ModuleNotFoundError: No module named 'pip'`:**
> Chạy lệnh `python -m ensurepip` để khôi phục lại pip, sau đó chạy tiếp `python -m pip install --upgrade pip`. Nếu môi trường ảo hoàn toàn mới, bạn có thể xóa thư mục `.venv` bằng `Remove-Item -Recurse -Force .venv` rồi chạy lại từ bước 2.

---

### 0.3. Tải Mã Nguồn Zephyr RTOS Vào `D:\zephyrproject` & Cài Đặt Dependencies

```powershell
# 1. Khởi tạo workspace tải về D:\zephyrproject (chọn bản LTS ổn định v3.7.0)
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v3.7.0 D:\zephyrproject

# 2. Chuyển vào thư mục và đồng bộ toàn bộ các module con (HAL ST, LVGL, CMSIS, mbedTLS...)
cd D:\zephyrproject
west update

# 3. Xuất gói CMake để hệ thống tự nhận diện đường dẫn Zephyr
west zephyr-export

# 4. Cài đặt toàn bộ danh mục thư viện Python bắt buộc của Zephyr (DTC parser, Kconfiglib...)
pip install -r D:\zephyrproject\zephyr\scripts\requirements.txt
```

---

### 0.4. Cài Đặt Bộ Trình Biên Dịch Chéo Zephyr SDK Vào `D:\zephyr-sdk-0.16.8`

Zephyr cung cấp bộ công cụ **Zephyr SDK** độc lập chứa trình biên dịch tối ưu hóa `arm-zephyr-eabi-gcc`. Để tiết kiệm thời gian và dung lượng (thay vì tải bản full 1.4 GB), ta chỉ tải bản **Minimal (~48 MB)** kèm riêng **ARM Toolchain (~85 MB)**:

```powershell
# 1. Tải công cụ giải nén siêu nhẹ 7zr.exe (500 KB) vào ổ D
curl.exe -L -o D:\7zr.exe https://www.7-zip.org/a/7zr.exe

# 2. Tải bản SDK Minimal và toolchain ARM Cortex-M bằng dịch vụ Windows BITS (nhanh và không bị nghẽn mạng như curl)
Start-BitsTransfer -Source https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_windows-x86_64_minimal.7z -Destination D:\zephyr-sdk-minimal.7z
Start-BitsTransfer -Source https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/toolchain_windows-x86_64_arm-zephyr-eabi.7z -Destination D:\toolchain-arm.7z
# (Hoặc tải trực tiếp bằng trình duyệt Chrome/Edge nếu muốn tải thủ công rồi copy vào D:\)

# 3. Giải nén vào D:\zephyr-sdk-0.16.8
D:\7zr.exe x D:\zephyr-sdk-minimal.7z -oD:\ -y
D:\7zr.exe x D:\toolchain-arm.7z -oD:\zephyr-sdk-0.16.8 -y

# 4. Đăng ký SDK với CMake (chạy trực tiếp bằng CMake, không chạy setup.cmd vì script này đòi cài wget)
cd D:\zephyr-sdk-0.16.8
cmake -P cmake\zephyr_sdk_export.cmake

# 5. Dọn dẹp các file nén tạm để giải phóng dung lượng
Remove-Item D:\zephyr-sdk-minimal.7z, D:\toolchain-arm.7z, D:\7zr.exe -Force

# 6. Thiết lập 3 Biến Môi Trường Windows cố định vĩnh viễn
setx ZEPHYR_BASE "D:\zephyrproject\zephyr"
setx ZEPHYR_SDK_INSTALL_DIR "D:\zephyr-sdk-0.16.8"
setx ZEPHYR_TOOLCHAIN_VARIANT "zephyr"
```

> 💡 **Lưu ý quan trọng sau khi setx:** Đóng cửa sổ PowerShell hiện tại và mở lại một cửa sổ mới để Windows cập nhật biến môi trường vừa tạo.

---

### 0.5. Cài Đặt Bộ Nạp Flash & Kiểm Tra Hoạt Động (Sanity Check Blinky)

Zephyr mặc định sử dụng **OpenOCD** làm runner để nạp code xuống kit STM32 qua cổng ST-Link:

```powershell
# 1. Cài đặt OpenOCD thông qua Chocolatey (chỉ cần chạy 1 lần duy nhất)
choco install -y openocd

# 2. Kích hoạt môi trường ảo Python
D:\zephyrproject\.venv\Scripts\Activate.ps1

# 3. Biên dịch ứng dụng mẫu blinky với board stm32f746g_disco
cd D:\zephyrproject
west build -b stm32f746g_disco zephyr/samples/basic/blinky -p auto

# 4. Cắm kit STM32F746G-DISCO vào máy qua cổng USB ST-Link và nạp firmware
west flash
```

> 💡 **Phương án nạp dự phòng qua STM32CubeProgrammer CLI:**
> Nếu không dùng OpenOCD, bạn có thể nạp thẳng file nhị phân bằng STM32CubeProgrammer có sẵn:
> ```powershell
> & "D:\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe" -c port=SWD -w D:\zephyrproject\build\zephyr\zephyr.bin 0x08000000 -v -rst
> ```

#### 🔍 Giải thích trạng thái LED trên bo mạch sau khi nạp:
* **`LD1` (LED xanh lá cạnh nút User):** Nhấp nháy chu kỳ 1 giây (1s sáng / 1s tắt) -> **Code Zephyr đang chạy thành công 100%!**
* **`LD7` (LED cạnh cổng USB ST-Link):** 
  - Khi đang nạp (`west flash`): Chớp liên tục Đỏ / Xanh.
  - Khi nạp xong: Đứng yên màu **ĐỎ** (ST-Link giải phóng kết nối, ở chế độ Idle) hoặc màu **XANH LÁ** (nếu Windows đang giữ kết nối ổ đĩa ảo/cổng COM). Cả hai trạng thái đều hoàn toàn bình thường.

---

### 0.6. Quy Trình Viết Code & Thao Tác Hằng Ngày Trong Antigravity

Trong Zephyr RTOS, bạn **KHÔNG CẦN mở STM32CubeIDE nữa**, toàn bộ quá trình viết code, cấu hình và nạp chip diễn ra trực tiếp ngay trong **Antigravity** (hoặc VS Code):

#### 1. Cấu trúc một thư mục ứng dụng Zephyr chuẩn:
```text
my_zephyr_app/
├── CMakeLists.txt     <-- Khai báo dự án CMake, liên kết hệ sinh thái Zephyr
├── prj.conf           <-- Bật/tắt các module tính năng của hệ điều hành (Kconfig)
├── app.overlay        <-- Cấu hình sơ đồ chân cắm phần cứng (Devicetree)
└── src/
    └── main.c         <-- Mã nguồn C ứng dụng logic của bạn
```

#### 2. Chu trình 4 bước làm việc mỗi khi mở máy:
1. **Mở terminal Antigravity** ➔ Kích hoạt môi trường (chỉ gõ 1 lần khi mở terminal mới):
   ```powershell
   D:\zephyrproject\.venv\Scripts\Activate.ps1
   ```
2. **Viết/Chỉnh sửa code:** Mở trực tiếp các file `src/main.c`, `prj.conf`, `app.overlay` trong trình soạn thảo Antigravity để code.
3. **Biên dịch (Build):**
   ```powershell
   west build -b stm32f746g_disco <đường_dẫn_thư_mục_app>
   # Ví dụ nếu đang đứng ngay trong thư mục app:
   west build -b stm32f746g_disco
   ```
4. **Nạp code (Flash):**
   ```powershell
   west flash
   ```

---

# 🧠 BƯỚC 1: KIẾN TRÚC HỆ THỐNG (SO SÁNH TRỰC DIỆN VỚI FREERTOS)

Để không bị bỡ ngỡ khi chuyển từ lập trình truyền thống sang Zephyr RTOS, hãy đối chiếu trực diện 4 thành phần cốt lõi của Zephyr với Bare-Metal và FreeRTOS:

### 1.1. Bảng Đối Chiếu 3 Mô Hình Lập Trình (Chi Tiết Ưu / Nhược Điểm & Đánh Giá Chuyên Sâu)

| Khía cạnh Kỹ thuật | 1. Bare-Metal (Ngày 0-6) | 2. FreeRTOS (ST HAL / CubeMX) | 3. Zephyr RTOS (Ngày 7-14) | Đánh Giá Ưu / Nhược Điểm & Trade-off Kỹ Thuật |
| :--- | :--- | :--- | :--- | :--- |
| **1. Cấu hình chân phần cứng (Pinmux)** | Gõ trực tiếp thanh ghi: `GPIOI->MODER`, `AFRH` | Dùng STM32CubeMX click chuột sinh code C (`MX_GPIO_Init()`) | Dùng file văn bản **Devicetree (`app.overlay`)** để mô tả chân | • **Bare-Metal:** Tối ưu hiệu năng tuyệt đối ($0\text{ ns}$ overhead), nhưng phụ thuộc chặt vào mã chip; đổi vi điều khiển phải viết lại toàn bộ.<br>• **FreeRTOS:** Kéo thả trực quan ban đầu, nhưng code sinh ra phân mảnh, khó quản lý Git diff và dễ bị CubeMX ghi đè khi re-generate.<br>• **Zephyr:** Tách biệt 100% phần cứng khỏi mã nguồn C; đổi sang bo mạch chip NXP/TI chỉ cần sửa file `.overlay` mà giữ nguyên $100\%$ code logic C. Nhược điểm: Cú pháp DTS trừu tượng, dốc học tập ban đầu cao. |
| **2. Bật/tắt tính năng OS & Subsystem** | Không có hệ điều hành (Tự code logic bằng cờ phần mềm) | Sửa macro `#define` trong file **`FreeRTOSConfig.h`** | Gõ cờ `CONFIG_XXX=y` trong file **`prj.conf` (Kconfig)** | • **Bare-Metal:** Tiết kiệm Flash/RAM tối đa, nhưng thiếu chuẩn mực khi tích hợp các stack phức tạp (TCP/IP, BLE, CANopen).<br>• **FreeRTOS:** Macro C truyền thống, đơn giản, nhưng thiếu cơ chế tự động giải quyết phụ thuộc (Dependency Resolution).<br>• **Zephyr:** Chuẩn Kconfig Linux cực mạnh, tự động nạp driver và bật các thư viện phụ thuộc lúc compile-time; có menu trực quan `west build -t menuconfig`. Nhược điểm: Phụ thuộc vào công cụ Python và CMake/West. |
| **3. Tạo luồng đa nhiệm (Multi-Tasking)** | Vòng lặp đơn `while(1)` trong hàm `main()` + Ngắt ISR | Gọi hàm động **`xTaskCreate()`** hoặc tĩnh `xTaskCreateStatic()` | Dùng macro tĩnh **`K_THREAD_DEFINE()`** hoặc `k_thread_create()` | • **Bare-Metal:** $0\text{ byte}$ RAM cho Stack luồng, nhưng mã nguồn bị nghẽn (Blocking) nếu có tác vụ chạy lâu; khó đáp ứng đa nhiệm thời gian thực.<br>• **FreeRTOS:** Linh hoạt, tạo task lúc runtime dễ dàng; tuy nhiên `xTaskCreate` dùng Heap dễ gây phân mảnh RAM nếu tạo/xóa task liên tục.<br>• **Zephyr:** `K_THREAD_DEFINE` cấp phát tĩnh toàn bộ Stack và Thread Control Block lúc biên dịch ($0\%$ phân mảnh RAM, an toàn cho chuẩn MISRA-C/ISO 26262). |
| **4. Mức ưu tiên (Priority Model)** | Mức ưu tiên ngắt NVIC (Số bé ưu tiên cao, 0 là khẩn cấp nhất) | Số càng LỚN $\rightarrow$ Ưu tiên càng CAO (Priority 5 > 1) | Số càng NHỎ $\rightarrow$ Ưu tiên càng CAO (Priority 0 > 5, giống NVIC) | • **Bare-Metal:** Chỉ có ưu tiên ngắt NVIC, không có ưu tiên mức tác vụ phần mềm.<br>• **FreeRTOS:** Quy ước ngược với phần cứng ARM Cortex-M NVIC, khiến lập trình viên mới dễ bị nhầm lẫn giữa NVIC Priority và FreeRTOS Task Priority.<br>• **Zephyr:** Nhất quán hoàn hảo với chuẩn phần cứng ARM Cortex-M: Số nhỏ hơn là mức ưu tiên cao hơn ($0$ là cao nhất trong dải Preemptive). Ngoài ra có thêm dải số âm (Negative Priorities) dành riêng cho Cooperative Threads (không bao giờ bị chiếm quyền). |
| **5. Giao tiếp liên luồng (IPC)** | Biến toàn cục `volatile` kết hợp cờ hiệu (Flags) | `xQueueSend()` / `xQueueReceive()` | `k_msgq_put()` / `k_msgq_get()` | • **Bare-Metal:** Tốc độ tức thì nhưng dễ gặp lỗi Race Condition, xé vụn dữ liệu (Torn Read/Write), CPU phải chạy vòng lặp đói (Busy Polling).<br>• **FreeRTOS:** Hàng đợi an toàn, nhưng phải phân biệt rạch ròi 2 hàm riêng biệt: hàm trong Thread (`xQueueSend`) và hàm trong ngắt (`xQueueSendFromISR`), rất dễ gây lỗi crash nếu gọi nhầm.<br>• **Zephyr:** Dùng **1 hàm duy nhất** `k_msgq_put()` cho cả ngữ cảnh Thread lẫn ngắt ISR (chỉ cần truyền timeout `K_NO_WAIT`). Copy theo giá trị an toàn, tự động đưa Thread vào trạng thái Sleep ($0\%$ CPU) khi hàng đợi rỗng. |

---

### 1.2. Devicetree (`app.overlay`) Là Gì? (Bản Đồ Phần Cứng)

* **Bản chất:** Thay vì vào CubeMX click chuột cấu hình chân `PI1` rồi sinh ra hàng trăm dòng code C của hãng, bạn chỉ cần mô tả chân đó trong file text **`app.overlay`**:
  ```dts
  / {
      aliases {
          led0 = &green_led; /* Gán nhãn ngắn gọn cho đèn LED */
      };
      leds {
          compatible = "gpio-leds";
          green_led: led_0 {
              gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>; /* Chân PI1, tích cực mức cao */
              label = "User Green LED";
          };
      };
  };
  ```
* **Tại sao lại tối ưu?**
  * **Tách rời code và phần cứng:** Khi chuyển sang một bo mạch khác (ví dụ LED dời sang chân `PB7`), bạn **giữ nguyên 100% code C logic**, chỉ cần sửa đúng số chân trong file `.overlay`!

---

### 1.3. Kconfig (`prj.conf`) Là Gì? (Công Tắc Bật/Tắt Tính Năng)

* **Bản chất:** Tương tự như file `FreeRTOSConfig.h`, nhưng Kconfig của Zephyr được chuẩn hóa theo phong cách nhân Linux. Cần dùng ngoại vi nào, bạn chỉ cần gõ `CONFIG_<TÊN>=y` vào file **`prj.conf`**:
  ```properties
  CONFIG_GPIO=y               # Bật driver điều khiển GPIO
  CONFIG_SERIAL=y             # Bật giao tiếp nối tiếp UART
  CONFIG_CONSOLE=y            # Bật màn hình Console ST-Link
  CONFIG_LOG=y                # Bật hệ thống ghi log
  CONFIG_MPU_STACK_GUARD=y    # Bật phần cứng MPU tự động bắt lỗi tràn ngăn xếp
  ```
* **Tại sao lại tiện?**
  * Không cần nhớ tên hàm khởi tạo driver phức tạp, chỉ cần bật cờ `CONFIG`, Zephyr sẽ tự động nạp driver tương ứng vào lúc biên dịch.

---

### 1.4. Cơ Chế Đa Luồng (Multi-Threading) So Với FreeRTOS

Trong Zephyr, mỗi tác vụ độc lập được gọi là một **Thread** (tương đương với **Task** trong FreeRTOS):

#### So sánh cách tạo luồng:
* **Bên FreeRTOS:**
  ```c
  xTaskCreate(vWorkerTask, "Worker", 512, NULL, 2, &xTaskHandle);
  ```
* **Bên Zephyr RTOS (Cách 1: Khai báo tĩnh bằng macro cực kỳ tiện lợi):**
  ```c
  // Tên luồng, Kích thước Stack, Hàm thực thi, Tham số 1, 2, 3, Mức ưu tiên, Tùy chọn, Thời gian delay khởi động
  K_THREAD_DEFINE(worker_tid, 1024, worker_entry, NULL, NULL, NULL, 5, 0, 0);
  ```

#### Quy tắc mức ưu tiên (Priority Rules):
1. **Dải ưu tiên Preemptive (Số dương: 0, 1, 2...):**
   * Số càng bé -> Ưu tiên càng cao (Luồng Priority 0 có quyền ngắt ngang luồng Priority 5).
   * Điểm này **ngược lại với FreeRTOS** nhưng **giống hệt quy tắc ngắt NVIC của vi điều khiển ARM Cortex-M** (IRQ Priority 0 là khẩn cấp nhất).
2. **Bảo vệ ngăn xếp bằng phần cứng (`CONFIG_MPU_STACK_GUARD=y`):**
   * Trong FreeRTOS, nếu 1 Task bị tràn Stack, nó sẽ ghi đè làm hỏng bộ nhớ của Task bên cạnh mà bạn không hề hay biết.
   * Trong Zephyr, phần cứng MPU tự động đặt một "vùng cấm" dưới đáy ngăn xếp. Nếu Task dùng quá dung lượng stack, **chip sẽ chặn đứng ngay lập tức và in tên Task gây lỗi ra màn hình**, giúp gỡ lỗi cực nhanh.

---

# 📑 BƯỚC 2: THỰC CHIẾN CẤU HÌNH DEVICETREE & KCONFIG (SETUP & LOOKUP)

> 🎯 **NGUYÊN TẮC TRA CỨU TRONG HỆ ĐIỀU HÀNH ZEPHYR RTOS:**
> Trong RTOS, ta không tra cứu Base Address hay Bitmask của thanh ghi trong RM0385 nữa. Mọi thông tin phần cứng được chuẩn hóa qua **3 trụ cột tra cứu**:
> 1. **Tra cứu Kconfig:** Định hình tính năng nhân, kích thước stack, hệ thống log (`menuconfig` hoặc Kconfig Search).
> 2. **Tra cứu Devicetree Bindings (`.yaml`):** Định dạng cú pháp và các thuộc tính hợp lệ của từng node thiết bị.
> 3. **Tra cứu Board DTS Gốc (`.dts`):** Xem sơ đồ phần cứng có sẵn của bo mạch STM32F746G-Discovery.

---

## 2.1. Lộ trình Tra cứu Trực tiếp trong Zephyr RTOS (Zephyr Lookup Methodology)

### 📖 Kênh 1: Cách Tra Cứu Tùy Chọn Kconfig
1. **Tra cứu trực quan qua GUI / TUI:**
   * Trong thư mục dự án, chạy lệnh: **`west build -t menuconfig`** (hoặc `guiconfig`).
   * Nhấn phím **`/`** ➔ Gõ từ khóa cần tìm: ví dụ gõ **`MPU_STACK_GUARD`** hoặc **`LOG_MODE_DEFERRED`**.
   * Hệ thống sẽ hiển thị chính xác: Macro Kconfig đầy đủ, kiểu dữ liệu (`bool`/`int`), giá trị mặc định (`default`), các điều kiện phụ thuộc (`depends on`), và file khai báo.
2. **Tra cứu online:** Truy cập `https://docs.zephyrproject.org/latest/kconfig.html` ➔ Gõ tên symbol vào ô tìm kiếm.

### 📖 Kênh 2: Cách Tra Cứu Thuộc Tính Node Devicetree (Bindings `.yaml`)
Khi cần thêm một node thiết bị vào file `app.overlay`, để biết node đó hỗ trợ những thuộc tính gì:
1. **Tìm file Schema `.yaml` tương ứng trong Zephyr SDK:**
   * Node `compatible = "gpio-leds";` ➔ Mở file: **`zephyr/dts/bindings/gpio/gpio-leds.yaml`**.
   * Node cổng GPIO STM32 ➔ Mở file: **`zephyr/dts/bindings/gpio/st,stm32-gpio.yaml`**.
2. **Đọc mục `properties:` trong file `.yaml`:**
   * Bạn sẽ thấy rõ các thuộc tính bắt buộc (`required: true`) và tùy chọn (như `gpios`, `label`).

### 📖 Kênh 3: Cách Tra Cứu Bản Đồ Phần Cứng Của Bo Mạch (`stm32f746g_disco.dts`)
Để biết bo mạch STM32F746-Discovery đã khai báo những ngoại vi nào:
1. **Mở file DTS gốc của bo:**
   * Đường dẫn: **`zephyr/boards/arm/stm32f746g_disco/stm32f746g_disco.dts`**.
2. **Kiểm tra phần `aliases` và các chân nối ngoài:**
   * Nhìn vào mục `leds`: Bạn sẽ thấy nhãn `green_led: led_0` gắn vào `gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>;` (Chân **`PI1`**).
   * Nhìn vào mục `buttons`: Bạn sẽ thấy nhãn `user_button` gắn vào `gpios = <&gpioi 11 GPIO_ACTIVE_LOW>;` (Chân **`PI11`**).

---

## 2.2. Bảng Cấu Hình Tính Năng Kconfig (`prj.conf`)

Tập tin `prj.conf` kích hoạt các subsystem cần thiết cho dự án:

| Kconfig Symbol | Giá trị | Ý nghĩa Kỹ thuật trong Zephyr RTOS | Đánh Giá Kỹ Thuật, Ưu / Nhược Điểm & Chi Phí Tài Nguyên |
| :--- | :---: | :--- | :--- |
| **`CONFIG_GPIO`** | `y` | Bật hệ thống driver điều khiển GPIO chuẩn Zephyr. | • **Ưu điểm:** Cung cấp API GPIO đồng nhất (`gpio_pin_configure_dt`, `gpio_pin_set_dt`), độc lập với phần cứng chip.<br>• **Chi phí:** Tăng khoảng $\approx 1.2\text{ KB}$ Flash ROM. |
| **`CONFIG_SERIAL`** | `y` | Bật giao tiếp nối tiếp UART/USART. | • **Ưu điểm:** Nền tảng cho Console, Logging và truyền thông Gateway.<br>• **Chi phí:** Tăng $\approx 2.5\text{ KB}$ Flash, chiếm 1 bộ đệm truyền nhận UART trong SRAM. |
| **`CONFIG_CONSOLE`** | `y` | Điều hướng đầu ra Console sang cổng nối tiếp ST-Link. | • **Ưu điểm:** Cho phép in debug qua `printk()` và xem trạng thái bo mạch trực tiếp từ terminal PC.<br>• **Hạn chế:** Nếu in quá nhiều mà không dùng chế độ Deferred sẽ làm chậm các luồng thời gian thực. |
| **`CONFIG_UART_CONSOLE`** | `y` | Sử dụng UART làm kênh Console chuẩn. | • **Ưu điểm:** Đơn giản, độ tin cậy cao, chạy được ngay khi khởi động bo mạch mà không cần thiết lập USB phức tạp.<br>• **Trade-off:** Chiếm dụng ngoại vi USART1 trên bo STM32F746-Disco. |
| **`CONFIG_LOG`** | `y` | Bật hệ thống ghi nhật ký Zephyr Logging Subsystem. | • **Ưu điểm:** Hỗ trợ lọc mức độ log (`ERR`, `WRN`, `INF`, `DBG`), định dạng màu sắc ANSI và gắn nhãn theo module.<br>• **Chi phí:** Tiêu tốn khoảng $\approx 4\text{ KB}$ Flash và $\approx 1\text{ KB}$ RAM cho cấu trúc metadata log. |
| **`CONFIG_LOG_MODE_DEFERRED`**| `y` | Nhật ký ghi vào RAM đệm, chỉ in ra UART lúc CPU rảnh rỗi. | • **Ưu điểm Cốt tử:** Giải phóng hoàn toàn luồng thời gian thực khỏi độ trễ in UART ($1\text{ - }10\text{ ms}$/lệnh in); triệt tiêu hiện tượng giật khung hình giao diện táp-lô.<br>• **Nhược điểm:** Tốn thêm Ring Buffer RAM ($\approx 1024\text{ bytes}$); nếu CPU gặp HardFault ngay lập tức thì các log chưa kịp đẩy ra UART có thể bị mất. |
| **`CONFIG_MPU_STACK_GUARD`** | `y` | Bật mạch phần cứng MPU giám sát tràn ngăn xếp của từng luồng. | • **Ưu điểm Vượt trội:** Bắt lỗi Stack Overflow bằng ngắt MemManage ngay thời điểm vi phạm; ngăn chặn $100\%$ lỗi ghi đè phá hỏng RAM của luồng bên cạnh.<br>• **Chi phí:** Không tốn chu kỳ CPU khi chạy bình thường (phần cứng MPU tự kiểm tra địa chỉ bus AXI/AHB). Mỗi Stack luồng phải căn lề $32\text{ bytes}$ tương ứng với 1 Region MPU. |
| **`CONFIG_THREAD_NAME`** | `y` | Cho phép gán tên chuỗi cho từng luồng phục vụ gỡ lỗi. | • **Ưu điểm:** In rõ tên luồng (`"can_rx"`, `"gui_task"`) khi xem lệnh `thread-analyzer` hoặc lúc xảy ra lỗi Panic thay vì chỉ in địa chỉ hex vô nghĩa.<br>• **Chi phí:** Tốn thêm $\approx 16\text{ bytes}$ RAM cho mỗi Thread Control Block để lưu chuỗi tên. |

---

## 2.3. Ánh Xạ Chân Phần Cứng trong File Overlay (`app.overlay`)

Bo mạch STM32F746G-Discovery có đèn LED màu xanh lá tại chân **`PI1`** và nút bấm User B1 tại chân **`PI11`**. Ta khai báo cấu trúc phần cứng chuẩn trong Devicetree:

```dts
/ {
    aliases {
        led0 = &green_led;
    };

    leds {
        compatible = "gpio-leds";
        green_led: led_0 {
            gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>;
            label = "User Green LED PI1";
        };
    };
};
```

---

# 💻 BƯỚC 3: GÕ CODE ỨNG DỤNG & MỔ XẺ BUG HỆ THỐNG (CODING & DEBUGS)

## 3.1. Phân chia Cấu trúc File Dự án cho Ngày 7

```text
zephyr_gateway/
├── CMakeLists.txt     <-- Khai báo dự án Zephyr CMake
├── prj.conf           <-- Cấu hình hệ điều hành Kconfig
├── app.overlay        <-- Cây thiết bị phần cứng Devicetree cho STM32F746-Disco
└── src/
    └── main.c         <-- Tạo luồng Worker, điều khiển GPIO và chạy Logging
```

---

### 📂 KHỐI 1: CẤU HÌNH DỰ ÁN [ `CMakeLists.txt` & `prj.conf` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 1:
1. **Quy chuẩn hệ thống build Zephyr CMake:**
   - **Mở Zephyr Docs** ➔ Tìm kiếm: `Application Development Primer` ➔ Section `CMakeLists.txt`.
   - Cú pháp bắt buộc: Lệnh `find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})` nạp toàn bộ toolchain, Kconfig, Devicetree và thư viện hệ điều hành trước khi định nghĩa target.

#### TODO 1 [File: `CMakeLists.txt`]: Tích Hợp Dự Án Zephyr RTOS
```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(can_gateway_zephyr)

target_sources(app PRIVATE src/main.c)
```

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 2:
1. **Tra cứu Kconfig qua công cụ Menuconfig:**
   - Trong terminal, gõ: `west build -t menuconfig` ➔ Bấm phím `/` để tra cứu symbol:
     - Gõ `GPIO`: Bật `CONFIG_GPIO=y` để biên dịch driver GPIO của STM32.
     - Gõ `LOG_MODE_DEFERRED`: Bật hệ thống ghi log bất đồng bộ (tránh giật lag).
     - Gõ `MPU_STACK_GUARD`: Kích hoạt phần cứng MPU bảo vệ ngăn xếp luồng.
     - Gõ `HEAP_MEM_POOL_SIZE`: Cấp phát vùng nhớ Heap chung cho hệ điều hành.

#### TODO 2 [File: `prj.conf`]: Cấu Hình Subsystem Nhân Zephyr
```properties
# Bật tính năng điều khiển GPIO và Console UART
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y

# Bật hệ thống Zephyr Logging đa tầng
CONFIG_LOG=y
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Bảo vệ an toàn bộ nhớ và ngăn xếp luồng
CONFIG_MPU_STACK_GUARD=y
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y

# Cấp phát bộ nhớ Heap chung cho hệ thống (4KB)
CONFIG_HEAP_MEM_POOL_SIZE=4096
```

---

### 📂 KHỐI 2: ĐỊNH NGHĨA PHẦN CỨNG [ `app.overlay` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 3:
1. **Tra cứu Schema Devicetree Binding (.yaml):**
   - Mở file: `zephyr/dts/bindings/gpio/gpio-leds.yaml`:
     - Xem mục `compatible: "gpio-leds"`.
     - Xem thuộc tính `gpios`: Yêu cầu tham chiếu đến phandle của GPIO controller (ví dụ `&gpioi 1`) và cờ cực tính `GPIO_ACTIVE_HIGH`.
2. **Tra cứu chân nối bo mạch:**
   - Mở file: `zephyr/boards/arm/stm32f746g_disco/stm32f746g_disco.dts`: Đèn LED xanh gắn vào chân `PI1`.

#### TODO 3 [File: `app.overlay`]: Khai Báo Node LED & Thiết Bị Ngoại Vi
```dts
/ {
    aliases {
        led-status = &status_led;
    };

    leds {
        compatible = "gpio-leds";
        status_led: led_pi1 {
            gpios = <&gpioi 1 GPIO_ACTIVE_HIGH>;
            label = "Status LED PI1";
        };
    };
};

/* Bật bộ điều khiển GPIOI trên chip STM32F7 */
&gpioi {
    status = "okay";
};
```

---

### 📂 KHỐI 3: MÃ NGUỒN ỨNG DỤNG ĐA LUỒNG [ `src/main.c` ]

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 4:
1. **Khai báo Thread tĩnh bằng K_THREAD_DEFINE:**
   - **Mở Zephyr Docs** ➔ `Kernel Services -> Threads`:
     - Cú pháp `K_THREAD_DEFINE(name, stack_size, entry, p1, p2, p3, prio, options, delay)`.
     - Bộ nhớ stack được cấp tĩnh và tự động căn lề theo yêu cầu của MPU.
2. **Ánh xạ thiết bị Devicetree qua GPIO_DT_SPEC_GET:**
   - **Mở Zephyr Docs** ➔ `Devicetree API -> GPIO DT Spec`:
     - Macro `GPIO_DT_SPEC_GET(node_id, prop)` chuyển đổi node trong file overlay thành struct `struct gpio_dt_spec` chứa con trỏ `device` và số chân pin tại compile-time.

#### TODO 4 [File: `src/main.c`]: Khởi Tạo Luồng Độc Lập & Nhận Diện Thiết Bị Devicetree
```c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* Đăng ký Module Nhật ký với Zephyr Logging */
LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

/* Kích thước ngăn xếp và Mức ưu tiên của các luồng */
#define BLINK_STACK_SIZE   1024
#define BLINK_PRIORITY     7

#define MONITOR_STACK_SIZE 2048
#define MONITOR_PRIORITY   5

/* Lấy thông số cấu hình LED từ Devicetree thông qua alias 'led-status' */
static const struct gpio_dt_spec s_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_status), gpios);

/* Prototype cho các hàm thực thi của luồng */
void blink_thread_entry(void *p1, void *p2, void *p3);
void monitor_thread_entry(void *p1, void *p2, void *p3);

/* BƯỚC 1: ĐỊNH NGHĨA VÀ KHỞI TẠO TĨNH CÁC LUỒNG BẰNG K_THREAD_DEFINE */
K_THREAD_DEFINE(blink_tid, BLINK_STACK_SIZE,
                blink_thread_entry, NULL, NULL, NULL,
                BLINK_PRIORITY, 0, 0);

K_THREAD_DEFINE(monitor_tid, MONITOR_STACK_SIZE,
                monitor_thread_entry, NULL, NULL, NULL,
                MONITOR_PRIORITY, 0, 0);
```

#### 📖 Hướng Dẫn Tra Cứu Tài Liệu Cho TODO 5:
1. **Kiểm tra ngoại vi sẵn sàng và cấu hình:**
   - **Mở Zephyr Docs** ➔ `Device Driver Model`:
     - Bắt buộc kiểm tra `gpio_is_ready_dt()` trước khi truy cập ngoại vi.
     - Hàm `gpio_pin_configure_dt(&s_led, GPIO_OUTPUT_ACTIVE)` cấu hình chiều xuất dữ liệu.
     - Hàm `gpio_pin_toggle_dt(&s_led)` đảo trạng thái chân LED.
2. **Cơ chế nhường CPU và điều phối:**
   - Hàm `k_msleep(500)` đưa luồng vào trạng thái ngủ, nhường CPU cho các luồng khác thực thi.

#### TODO 5 [File: `src/main.c`]: Thân Luồng Thực Thi & Kiểm Tra Sẵn Sàng Thiết Bị
```c
void blink_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    /* BƯỚC 2: KIỂM TRA TÍNH SẴN SÀNG CỦA NGOẠI VI (DEVICE READY CHECK) */
    if (!gpio_is_ready_dt(&s_led)) {
        LOG_ERR("Lỗi: Ngoại vi GPIO điều khiển LED chưa sẵn sàng!");
        return;
    }

    /* BƯỚC 3: CẤU HÌNH HƯỚNG XUẤT CHO CHÂN GPIO */
    int ret = gpio_pin_configure_dt(&s_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Lỗi: Không thể cấu hình chân LED PI1 (Mã lỗi: %d)", ret);
        return;
    }

    LOG_INF("Khởi tạo luồng Blink LED thành công! Bắt đầu chớp tắt...");

    while (1) {
        gpio_pin_toggle_dt(&s_led);
        /* Tự động chuyển CPU sang trạng thái Sleep và nhường tài nguyên cho luồng khác */
        k_msleep(500);
    }
}

void monitor_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Khởi tạo luồng Hệ Thống Giám Sát (Monitor Worker) thành công!");

    uint32_t heartbeat_count = 0;
    while (1) {
        heartbeat_count++;
        LOG_INF("[HEARTBEAT #%u] Hệ điều hành Zephyr RTOS đang chạy ổn định!", heartbeat_count);
        
        /* Định kỳ 2 giây in thông điệp trạng thái một lần */
        k_sleep(K_SECONDS(2));
    }
}

int main(void)
{
    LOG_INF("=================================================");
    LOG_INF("     STM32F746 DUAL-GATEWAY ZEPHYR BRING-UP      ");
    LOG_INF("=================================================");
    
    /* Hàm main kết thúc, nhường hoàn toàn quyền điều khiển cho bộ lập lịch Zephyr */
    return 0;
}
```

---

## 3.2. Mổ xẻ 5 Bug Hệ Thống "Kinh Điển" trong Ngày 7

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 5 BẪY HỆ THỐNG ZEPHYR RTOS                                      │
├───────────────────┬───────────────────────────────────────────┬─────────────────────────────────┤
│ HIỆN TƯỢNG BUG    │ NGUYÊN NHÂN SÂU XA HỆ ĐIỀU HÀNH           │ GIẢI PHÁP SỬA CODE CHUẨN XÁC    │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 1. `DEVICE_DT_GET`│ Quên bật Kconfig tương ứng trong `prj.conf`│ Kiểm tra `prj.conf`: phải có    │
│    trả về con trỏ │ (ví dụ thiếu `CONFIG_GPIO=y`). Driver     │ `CONFIG_GPIO=y`, kiểm tra node  │
│    NULL hoặc treo │ không được biên dịch vào mã máy.          │ `status = "okay";` ở overlay.   │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 2. Kernel Panic   │ Cấp phát Stack luồng quá nhỏ (ví dụ 256B) │ Tăng kích thước Stack lên 1KB   │
│    (MemManage)    │ khiến biến cục bộ đè vào MPU Guard Region.│ hoặc 2KB; tránh đệ quy/mảng to. │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 3. Các luồng khác │ Luồng Cooperative (Priority âm) chạy vòng │ Thêm `k_yield()` hoặc           │
│    bị chết đói    │ lặp vô tận tính toán mà không nhường CPU. │ `k_msleep()` vào trong thân lặp.│
│    (Starvation)   │                                           │                                 │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 4. Mất log hoặc in│ Dùng `LOG_MODE_IMMEDIATE=y` trong luồng   │ Đổi sang `LOG_MODE_DEFERRED=y`  │
│    chậm trễ ngắt  │ tần số cao làm nghẽn UART Console.        │ để log được đệm vào RAM.        │
├───────────────────┼───────────────────────────────────────────┼─────────────────────────────────┤
│ 5. Lỗi biên dịch  │ Tên alias trong file overlay gõ sai khác  │ Kiểm tra khớp từng ký tự giữa   │
│    macro DT_ALIAS │ với chuỗi truyền vào macro trong C code.  │ `DT_ALIAS(led_status)` & overlay│
└───────────────────┴───────────────────────────────────────────┴─────────────────────────────────┘
```

---

# 🎯 BƯỚC 4: BỘ CÂU HỎI PHỎNG VẤN & KỊCH BẢN TRẢ LỜI CHUYÊN SÂU

### ❓ Câu 1: Devicetree trong Zephyr RTOS được nạp và xử lý vào thời điểm nào? Nó tối ưu hơn Linux nhúng ra sao?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Trong Zephyr RTOS, Devicetree được xử lý **hoàn toàn ở giai đoạn Biên dịch (Compile-time)** bằng bộ công cụ Python. Nó trích xuất dữ liệu từ các file `.dts`/`.overlay` và sinh ra trực tiếp mã nguồn C macro tĩnh (`devicetree_generated.h`).
  * Khác với Linux nhúng phải nạp file nhị phân `.dtb` vào RAM và duyệt cây lúc boot (tốn hàng chục KB RAM và chu kỳ CPU), Zephyr không tốn một byte RAM nào cho việc lưu cấu trúc cây lúc chạy. Trình biên dịch GCC có thể thực hiện tối ưu hóa hằng số (Constant Folding) và loại bỏ mã chết (Dead Code Elimination) ngay từ lúc build.

### ❓ Câu 2: Cơ chế `CONFIG_MPU_STACK_GUARD` phát hiện lỗi tràn ngăn xếp (Stack Overflow) như thế nào?
* **Trả lời chuẩn Kỹ sư RTOS:** 
  * Thay vì dùng phương pháp kiểm tra phần mềm thụ động (như ghi mẫu số ảo Canary `0xA5A5A5A5` ở đáy stack rồi chờ bộ lập lịch kiểm tra định kỳ), Zephyr kích hoạt khối phần cứng **MPU (Memory Protection Unit)** của ARM Cortex-M7.
  * MPU sẽ cấu hình một vùng đệm kích thước 32 bytes nằm ngay dưới đáy ngăn xếp của luồng thành vùng cấm truy cập (`No Access`). Cứ mỗi lần CPU chuyển đổi ngữ cảnh (Context Switch), MPU tự động tái lập trình vùng cấm này tương ứng với luồng chuẩn bị chạy. Nếu con trỏ ngăn xếp `SP` vượt ranh giới và ghi đè vào đây, ngoại lệ phần cứng **`MemManage Fault`** lập tức nổ ra ngay tại chu kỳ lệnh vi phạm, bắt quả tang chính xác dòng lệnh gây lỗi.

### ❓ Câu 3: Sự khác biệt cơ bản giữa Luồng Cooperative và Luồng Preemptive trong Zephyr là gì?
* **Trả lời chuẩn Kỹ sư RTOS:**
  * **Cooperative Thread (Priority âm: `-CONFIG_NUM_COOP_PRIO` đến `-1`):** Một khi đã chiếm CPU, nó sẽ nắm quyền thực thi độc quyền. Không có bất kỳ luồng nào khác (kể cả luồng có priority cao hơn) có thể chen ngang, trừ khi chính nó chủ động nhường quyền bằng lệnh `k_yield()` hoặc `k_sleep()`.
  * **Preemptive Thread (Priority không âm: `0` đến `CONFIG_NUM_PREEMPT_PRIO - 1`):** Bộ lập lịch có quyền tước quyền thực thi bất kỳ lúc nào nếu có một luồng khác có priority cao hơn sẵn sàng chạy hoặc khi hết khoảng thời gian chia sẻ (Time slice).

---

### 🎙️ KỊCH BẢN TRẢ LỜI PHỎNG VẤN 60 GIÂY (ELEVATOR PITCH)

> *"Sau khi làm chủ toàn bộ nền tảng Bare-metal ở tầng thanh ghi, em mở rộng dự án CAN Gateway lên hệ điều hành **Zephyr RTOS** để khai thác khả năng đa nhiệm và quản trị phần cứng hiện đại.  
> Em thiết kế file **`app.overlay`** để phân tách hoàn toàn tầng mô tả chân vật lý (Pinctrl) khỏi mã nguồn C, giúp ứng dụng có thể dễ dàng chuyển đổi sang các dòng chip STM32 khác chỉ bằng việc thay file cấu hình.  
> Để bảo vệ hệ thống trước các lỗi bộ nhớ thời gian thực, em cấu hình tính năng **`CONFIG_MPU_STACK_GUARD`** tận dụng khối MPU phần cứng của Cortex-M7 bắt lỗi tràn ngăn xếp tức thời, đồng thời áp dụng mô hình **Deferred Logging** để toàn bộ hoạt động in vết hệ thống được đẩy vào RAM đệm và xử lý lúc CPU rảnh rỗi, triệt tiêu hoàn toàn hiện tượng jitter hay trễ nhịp thời gian thực của các luồng truyền thông."*
