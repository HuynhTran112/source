# FRESHER EMBEDDED STUDY GUIDE & TECHNICAL COMPILATION

Tài liệu này tổng hợp toàn bộ lộ trình ôn tập, ánh xạ câu hỏi Mock Test (77 câu) sang các topic trên website [EmbeddedInterviewLab](https://embeddedinterviewlab.com), và hệ thống hóa các kiến thức cốt lõi (C/C++, MCU, Automotive, Bare-metal registers) dành cho Fresher Embedded Engineer.

---

## 🎯 1. Phân Tích Đề Thi & Chiến Lược Ôn Tập

### Cấu Trúc Đề Thi (77 Câu)
Dựa trên kết quả đánh giá ban đầu, phần lớn điểm số tập trung ở mảng lập trình **C/C++ (52%)** và **Automotive Embedded (6%)**. Tuy nhiên, điểm hiện tại ở hai mảng này đang ở mức báo động (C/C++: 10/40; Automotive: 0/5). Đây chính là cơ hội lớn nhất để cải thiện điểm số nhanh chóng.

| Section | Số Câu | Tỷ Trọng | Mục Tiêu Điểm |
| :--- | :---: | :---: | :---: |
| **C & C++ Coding** | 40 | 52% | **32+/40** (Trọng tâm số 1) |
| **Data Structures & Algorithms (DSA)** | 10 | 13% | **8/10** |
| **IT Knowledge** | 6 | 8% | **5/6** |
| **Gen AI & Basic Prompt Engineering** | 5 | 6% | **4/5** |
| **Power Skills & Behaviours** | 5 | 6% | **5/5** |
| **Automotive Embedded Basics** | 5 | 6% | **4/5** (Trọng tâm số 2) |
| **Cloud Computing Basics** | 3 | 4% | **2/3** |
| **CI/CD** | 3 | 4% | **3/3** |
| **Tổng** | **77** | **100%** | **63/77 (~82%)** |

### Lộ Trình Phân Bổ Thời Gian
* **Level 1 (50% thời gian):** Tập trung sâu vào C/C++ Core (Pointers, Arrays, Memory Layout, structs/alignment, qualifiers `volatile`, `static`, `extern`, `const`, bitwise, undefined behavior, malloc/free).
* **Level 2 (25% thời gian):** Peripherals & MCU (Interrupts, GPIO, UART, SPI, I2C, CAN, Register-level coding, Clock tree, Watchdog).
* **Level 3 (15% thời gian):** DSA cơ bản (Array, Hash table, Stack/Queue, Linked list, Binary search, Two-pointers, Bitwise algorithms).
* **Level 4 (10% thời gian):** IT, Git, CI/CD, Cloud, GenAI, Power Skills & Automotive Theory.

---

## 🗺️ 2. Ánh Xạ Câu Hỏi Mock Test Sang Topic Trên EmbeddedInterviewLab

Để ôn tập hiệu quả nhất, hãy truy cập trực tiếp các bài viết sau trên [EmbeddedInterviewLab](https://embeddedinterviewlab.com/topics/):

### 🔴 Nhóm 1: Core C/C++ & Memory (Giải Quyết 40 Câu C/C++ Coding)

1. **[Data Types & Memory Layout](https://embeddedinterviewlab.com/topics/data-types-memory/)**
   * *Nội dung:* Các phân vùng bộ nhớ `.text`, `.data`, `.bss`, stack, heap; vòng đời biến static/global.
   * *Giải quyết câu hỏi:* **Q56** (.data), **Q57** (.bss), **Q58** (.text), **Q59** (data copy), **Q60** (bss zero init).
2. **[Pointers, Arrays & Pointer Arithmetic](https://embeddedinterviewlab.com/topics/pointers-arrays/)**
   * *Nội dung:* Cách hoạt động của pointer, toán tử `*` và `&`, kích thước con trỏ (32-bit vs 64-bit), array decay, dangling pointer, uninitialized pointer.
   * *Giải quyết câu hỏi:* **Q1** (Array index access time), **Q45** (Array size), **Q46** (Out-of-bounds), **Q47** (`sizeof` array vs pointer), **Q48** (Dereference), **Q49** (Assign via pointer), **Q50** (Address-of operator), **Q51** (Pointer size), **Q52** (NULL pointer), **Q53** (Wild pointer danger), **Q54** (Dangling pointer definition), **Q74** (Return local variable address).
3. **[volatile and const](https://embeddedinterviewlab.com/topics/volatile-const/)**
   * *Nội dung:* Tác động của compiler optimization, biến thay đổi ngoài luồng xử lý thông thường (HW registers, ISR-shared), con trỏ hằng `const int * p` vs hằng con trỏ `int * const p`.
   * *Giải quyết câu hỏi:* **Q40** (`const` vs pointer syntax), **Q43** (`volatile` definition), **Q44** (ISR modified variables).
4. **[Structs, Unions & Bitfields](https://embeddedinterviewlab.com/topics/structs-unions-bitfields/)**
   * *Nội dung:* Cấu trúc dữ liệu nhúng, padding và alignment, union dùng cho register overlays/type punning, bitfields để map phần cứng.
   * *Giải quyết câu hỏi:* Hỗ trợ trả lời các câu hỏi về Bit manipulation và tối ưu bộ nhớ.
5. **[Memory Alignment & Endianness](https://embeddedinterviewlab.com/topics/memory-alignment-endianness/)**
   * *Nội dung:* Quy tắc alignment, byte swapping, Big Endian vs Little Endian.
   * *Giải quyết câu hỏi:* Các câu hỏi phỏng vấn nâng cao về tối ưu struct.
6. **[Function Pointers & Callbacks](https://embeddedinterviewlab.com/topics/function-pointers-callbacks/)**
   * *Nội dung:* Cú pháp con trỏ hàm, tạo callback event-driven, bảng nhảy (dispatch tables).
   * *Giải quyết câu hỏi:* **Q75** (Cú pháp `int (*fp)(int, int)`).
7. **[Inline Functions & Macros](https://embeddedinterviewlab.com/topics/inline-macros/)**
   * *Nội dung:* Macro `#define` vs `inline`, preprocessor directives (`#ifdef`, `#pragma once`), từ khóa `static` và `extern`.
   * *Giải quyết câu hỏi:* **Q38** / **Q39** (Increment expressions), **Q41** (Static local variable), **Q42** (Extern keyword).
8. **[Embedded C Code Patterns](https://embeddedinterviewlab.com/topics/code-patterns/)**
   * *Nội dung:* Thiết kế ring buffer, bitwise patterns, xử lý lỗi, undefined behavior thường gặp.
   * *Giải quyết câu hỏi:* **Q73** (Division by zero UB).

### 🔴 Nhóm 2: MCU, Peripherals & Build Systems (Giải Quyết Phần Embedded & Automotive)

1. **[Compilation Pipeline](https://embeddedinterviewlab.com/topics/compilation-pipeline/)**
   * *Nội dung:* Quá trình biên dịch 4 giai đoạn: Preprocessor -> Compiler -> Assembler -> Linker.
2. **[Memory Layout & Startup](https://embeddedinterviewlab.com/topics/memory-layout-startup/)**
   * *Nội dung:* Từ `Reset_Handler` đến `main()`, quá trình boot của MCU, sao chép data và zero bss.
   * *Giải quyết câu hỏi:* **Q59** & **Q60** (Sequence khởi động MCU).
3. **[Linker Scripts](https://embeddedinterviewlab.com/topics/linker-scripts/)**
   * *Nội dung:* Định nghĩa vùng nhớ FLASH/RAM (`MEMORY`) và ánh xạ các section `.text`, `.data`, `.bss` (`SECTIONS`).
4. **[Interrupts and priorities](https://embeddedinterviewlab.com/topics/interrupts-priorities/)**
   * *Nội dung:* Thiết kế ISR, cấu hình mức ưu tiên NVIC, nested interrupts, race conditions.
   * *Giải quyết câu hỏi:* **Q69** (Interrupt concept), **Q70** (Short ISR rationale), **Q71** (Shared variable data race).
5. **[Driver design: HAL vs bare metal](https://embeddedinterviewlab.com/topics/driver-design-hal/)**
   * *Nội dung:* Lập trình thanh ghi, trừu tượng hóa phần cứng (HAL/LL), luồng giao tiếp thanh ghi ngoại vi.
   * *Giải quyết câu hỏi:* **Q67** (Memory-mapped peripheral registers), **Q68** (Peripheral init sequence).
6. **[Watchdog Timer](https://embeddedinterviewlab.com/topics/watchdog/)**
   * *Nội dung:* Cách thức hoạt động của Independent Watchdog (IWDG) và Window Watchdog (WWDG).
   * *Giải quyết câu hỏi:* **Q37** & **Q72** (Watchdog timer purpose).
7. **[CAN](https://embeddedinterviewlab.com/topics/can/)**
   * *Nội dung:* Giao thức CAN, arbitration (phân xử trọng tài bằng ID), bit timing, CAN_H / CAN_L.
   * *Giải quyết câu hỏi:* **Q33** (CAN definition), **Q34** (Message-based & arbitration).

### 🟠 Nhóm 3: RTOS, Networks & Tools (Giải Quyết Phần IT & CI/CD)

1. **[RTOS fundamentals](https://embeddedinterviewlab.com/topics/rtos-fundamentals/)**
   * *Nội dung:* So sánh Task/Thread vs Process, quản lý bộ nhớ RTOS, context switch.
   * *Giải quyết câu hỏi:* **Q14** (Process vs Thread address space sharing).
2. **[RTOS Synchronization Primitives](https://embeddedinterviewlab.com/topics/rtos-sync-primitives/)**
   * *Nội dung:* Mutex vs Semaphore, deadlock, priority inversion (nghịch đảo ưu tiên).
3. **[TCP/IP Fundamentals](https://embeddedinterviewlab.com/topics/tcp-ip-fundamentals/)** & **[Sockets API Basics](https://embeddedinterviewlab.com/topics/sockets-api-basics/)**
   * *Nội dung:* Stack TCP/IP, TCP vs UDP, DNS, OSI model layers, Socket API lifecycle.
   * *Giải quyết câu hỏi:* **Q11** (TCP connection-oriented), **Q12** (DNS purpose), **Q13** (Network layer routing).
4. **[Testing and code coverage](https://embeddedinterviewlab.com/topics/testing-and-coverage/)**
   * *Nội dung:* Unit testing (Unity/CppUTest), Static analysis, luồng CI/CD cho phần cứng nhúng.
   * *Giải quyết câu hỏi:* **Q30** (CI definition), **Q31** (CI purpose), **Q32** (Pipeline sequence).
5. **[Industry safety and security standards](https://embeddedinterviewlab.com/topics/industry-standards-overview/)**
   * *Nội dung:* Tiêu chuẩn an toàn chức năng ISO 26262, tiêu chuẩn phần mềm AUTOSAR, MISRA C.
   * *Giải quyết câu hỏi:* **Q35** (ECU definition), **Q36** (AUTOSAR definition).

---

## 🛠️ 3. Quy Tắc Lập Trình Register Bare-Metal & Kiến Trúc Hệ Thống

Để chuẩn bị tốt nhất cho phần lập trình thanh ghi (Register-level coding) của STM32/ARM Cortex-M, cần tuân thủ nghiêm ngặt các quy tắc kỹ thuật sau đây (được trích dẫn chi tiết từ tài liệu nền tảng [day00_baremetal_foundations.md](file:///d:/Project/STM32F7/docs/day00_baremetal_foundations.md)):

### 1. Reset Value (Giá Trị Reset)
Trước khi cấu hình bất kỳ thanh ghi nào, luôn tra cứu **Reset Value** trong Reference Manual để hiểu trạng thái ban đầu của phần cứng. Không bao giờ giả định các bit mặc định là 0.

### 2. Phân Biệt Quyền Truy Cập (RO, RW, W1C)
* **RO (Read-Only):** Chỉ đọc (ví dụ thanh ghi trạng thái).
* **RW (Read-Write):** Đọc và ghi bình thường.
* **W1C (Write 1 to Clear):** Ghi 1 để xóa cờ (thường là các cờ ngắt).
  * ⚠️ **CỰC KỲ QUAN TRỌNG:** Không bao giờ sử dụng phép toán OR-equal (`REG |= FLAG`) trên thanh ghi chứa bit W1C, vì điều này có thể vô tình xóa các cờ ngắt khác đang được set. Hãy ghi đè trực tiếp giá trị bit đó:
    ```c
    USART1->SR = USART_SR_RXNE; // Ghi thẳng 1 vào bit cần xóa, các bit khác ghi 0
    ```

### 3. Quy Tắc RMW (Read-Modify-Write) Cho Multi-bit Fields
Khi cấu hình các trường bit gồm nhiều bit liền kề (ví dụ: Mode pins trong GPIO MODER, Prescaler), luôn sử dụng mẫu thiết kế **Xóa trước - Set sau (Clear-then-Set)**:
```c
// Cấu hình GPIOA Pin 5 thành Output (01)
GPIOA->MODER &= ~(3UL << (5 * 2)); // Bước 1: Xóa sạch 2 bit cấu hình tại pin 5 (Mặt nạ)
GPIOA->MODER |=  (1UL << (5 * 2)); // Bước 2: Ghi giá trị mong muốn (01 - Output)
```

### 4. Định Nghĩa Trực Tiếp Bộ Nhớ Với Qualifier `volatile`
Mọi con trỏ ánh xạ bộ nhớ (Memory-mapped IO pointers) và biến chia sẻ giữa ISR với luồng `main()` bắt buộc phải khai báo với `volatile` để tránh compiler tối ưu hóa sai:
```c
#define GPIOA_MODER (*(volatile uint32_t *)0x40020000UL)
```

### 5. Quy Trình 5 Bước Cho Interrupt & NVIC Pipeline
Khi cấu hình ngắt phần cứng, luôn thực hiện theo đúng trình tự:
```
[Ngoại vi kích hoạt Sự kiện] ➔ [Set Peripheral Interrupt Flag] ➔ [Enable Peripheral Interrupt] ➔ [Enable NVIC IRQ Channel] ➔ [Viết IRQ Handler & Xóa Flag (W1C/Read-Clear)]
```

### 6. Quy Trình 4 Bước Khởi Tạo Ngoại Vi (Peripheral Init)
Mỗi khi khởi tạo một ngoại vi bare-metal (như UART, SPI), phải triển khai đủ 4 bước tuần tự:
1. **Clock Enable (Cấp Clock):** Kích hoạt clock cho ngoại vi trước tiên thông qua các thanh ghi `RCC_AHBxENR` hoặc `RCC_APBxENR`. (Nếu không cấp clock, mọi thao tác ghi vào thanh ghi ngoại vi đều bị bỏ qua).
2. **Pin Multiplexing (Cấu Hình Chân):** Cấu hình các chân GPIO tương ứng sang chế độ Alternate Function (`MODER`, `AFRH`/`AFRL`).
3. **Control & Config (Cấu Hình Thanh Ghi Điều Khiển):** Thiết lập thông số hoạt động của ngoại vi (baudrate, stop bits, parity trong `CR1`, `BRR`,...).
4. **Data Exchange (Trao Đổi Dữ Liệu):** Đọc/Ghi dữ liệu qua thanh ghi dữ liệu (`TDR`, `RDR`).

---

## 💻 4. 10 Bài Tập Code C Kinh Điển Cần Tự Viết Bằng Tay

Đừng chỉ học lý thuyết trắc nghiệm. Hãy tự viết lại các hàm sau để rèn luyện tư duy pointer và bitwise:

### 1. Hàm tính độ dài chuỗi (`strlen`)
```c
size_t my_strlen(const char *s) {
    const char *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}
```

### 2. Hàm so sánh chuỗi (`strcmp`)
```c
int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
```

### 3. Hàm copy bộ nhớ (`memcpy`)
```c
void *my_memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
```

### 4. Đảo ngược chuỗi ký tự (`reverse_string`)
```c
void reverse(char *str) {
    if (!str) return;
    int len = 0;
    while (str[len]) len++;
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}
```

### 5. Hàm Set/Clear/Toggle Bit
```c
void set_bit(uint32_t *reg, uint8_t bit) {
    *reg |= (1UL << bit);
}

void clear_bit(uint32_t *reg, uint8_t bit) {
    *reg &= ~(1UL << bit);
}

void toggle_bit(uint32_t *reg, uint8_t bit) {
    *reg ^= (1UL << bit);
}
```

### 6. Đếm số lượng bit 1 trong một từ 32-bit (Hamming Weight)
```c
uint32_t count_bits(uint32_t x) {
    uint32_t count = 0;
    while (x) {
        x &= (x - 1); // Xóa bit 1 cuối cùng bên phải
        count++;
    }
    return count;
}
```

### 7. Kiểm tra một số có phải là lũy thừa của 2 (Power of Two)
```c
bool is_power_of_two(uint32_t x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}
```

### 8. Tìm số đơn độc (Single Number) trong mảng mà các số khác xuất hiện 2 lần
```c
int find_single_number(int *arr, int size) {
    int result = 0;
    for (int i = 0; i < size; i++) {
        result ^= arr[i]; // XOR triệt tiêu các cặp số giống nhau
    }
    return result;
}
```

### 9. Thuật toán Two Sum sử dụng Hash Map (hoặc Two Pointers nếu mảng đã sort)
```c
// Giải pháp Two Pointers đối với mảng đã sắp xếp tăng dần:
bool two_sum_sorted(int *arr, int size, int target, int *idx1, int *idx2) {
    int left = 0;
    int right = size - 1;
    while (left < right) {
        int sum = arr[left] + arr[right];
        if (sum == target) {
            *idx1 = left;
            *idx2 = right;
            return true;
        } else if (sum < target) {
            left++;
        } else {
            right--;
        }
    }
    return false;
}
```

### 10. Khai báo Con trỏ hàm & Callback Pattern
```c
// Định nghĩa kiểu dữ liệu con trỏ hàm nhận 2 tham số int và trả về int
typedef int (*math_op_t)(int, int);

int add(int a, int b) { return a + b; }
int subtract(int a, int b) { return a - b; }

// Sử dụng con trỏ hàm làm tham số (Callback Pattern)
int execute_op(int x, int y, math_op_t operation) {
    if (operation != NULL) {
        return operation(x, y);
    }
    return 0;
}
```
