# NGUYEN VAN A
**Ho Chi Minh City, Vietnam** | +84 901-234-567 | yourname@email.com  
[linkedin.com/in/yourprofile](https://linkedin.com) | [github.com/yourusername](https://github.com)

---

## EDUCATION
**Ho Chi Minh City University of Technology and Education** | *Ho Chi Minh City, Vietnam*  
*Bachelor of Engineering in Computer Engineering Technology* | *2022 – 2026*  
- **Cumulative GPA: 3.02 / 4.0** -- Graduated  
- **TOEIC Listening and Reading: 785 / 990**

---

## TECHNICAL SKILLS
- **Languages:** C, Dart, Python, PowerShell, ARM Assembly
- **RTOS and Kernels:** Zephyr RTOS with DeviceTree and Kconfig, FreeRTOS
- **Hardware Platforms:** STM32F7, ARM Cortex-M7, ESP32-S3, ESP32, ST-LINK, Logic Analyzer, Oscilloscope
- **Peripherals and Buses:** CAN 2.0B, UART DMA, SPI, I2C, RS485, Modbus, SDMMC, SDHC, FMC SDRAM, LTDC, DMA2D
- **Protocols and Standards:** Vector DBC, AUTOSAR E2E Profile 1 with CRC-8, ISO 11898-1, FAT32 ChaN FatFs, BLE NimBLE, MQTT
- **Toolchains and Frameworks:** West CLI, CMake, Ninja, Kconfig, DeviceTree Compiler, ESP-IDF, Flutter, Git

---

## PROFESSIONAL EXPERIENCE

### **Tep Bac Joint Stock Company** | *Ho Chi Minh City, Vietnam*  
*Embedded Firmware Intern and Collaborator* | *Jan 2026 – Jul 2026*
- Assembled, calibrated, and deployed 10 ENVISOR E7 monitoring stations using ESP32 and FreeRTOS. Transmitted pH and Dissolved Oxygen telemetry to cloud servers via RS485 Modbus, I2C, and MQTT with 99.8% uptime.
- Developed embedded C drivers for 2 industrial sensors: Water Level and Turbidity. Implemented moving average digital filters to cut signal noise by 40% in real shrimp pond environments.
- Collaborated in a 2-member team to verify RS485 bus timing with oscilloscopes, calibrated 10 sensor sets in saltwater, and reduced FreeRTOS task latency to under 50 ms.

---

## TECHNICAL PROJECTS

### **Automotive CAN Gateway and Telematics Node** | [GitHub Repo](https://github.com/yourusername/stm32f7-zephyr-can-gateway) | *Zephyr RTOS, West, CMake, DeviceTree, C*
*Sep 2026*
- Built an Automotive CAN Gateway on STM32F746 Cortex-M7 running at 216 MHz using Zephyr RTOS and DeviceTree. Processed over 1000 CAN frames per second at 500 kbps with under 2% CPU load.
- Configured asynchronous CAN 2.0B reception with 6 hardware filter banks into Zephyr k_msgq. Built a DBC decoding engine parsing 12 vehicle signals in under 15 microseconds using fixed-point integer math.
- Implemented AUTOSAR E2E Profile 1 CRC-8 validation, automatic ISO 11898-1 Bus-Off recovery within 100 ms, and a 115200 baud Zephyr Shell CLI verified with a 24 MHz logic analyzer.

### **High-Speed Bare-Metal TFT Display and SDHC Subsystem** | [GitHub Repo](https://github.com/yourusername/stm32f7-baremetal-tft-sdhc) | *STM32F7, C, FMC, SDMMC, DMA2D*
*Aug 2026 – Sep 2026*
- Developed bare-metal register drivers in C and Assembly for 8MB FMC SDRAM at 108 MHz and 480x272 LTDC LCD. Achieved constant 60 FPS video playback with 0 dropped frames and no tearing via VSYNC reload.
- Built a bare-metal storage driver for 16GB MicroSD SDHC cards over 4-bit 48 MHz SDMMC bus with 512-byte LBA addressing, achieving 18 MB/s read speed with ChaN FatFs FAT32.
- Accelerated frame rendering with DMA2D Chrom-ART to reduce CPU utilization from 85% to 0%. Configured MPU non-cacheable memory regions to eliminate D-Cache visual artifacts, and wrote Python and PowerShell video tools.

### **Wearable IoT Smartwatch and Sensor Hub System** | [GitHub Repo](https://github.com/yourusername/esp32s3-smartwatch-flutter) | *ESP32-S3, FreeRTOS, I2C, SPI, UART, BLE, Flutter*
*Jan 2026 – Jul 2026*
- Developed a wearable system on dual-core 240 MHz ESP32-S3 with 2MB PSRAM using ESP-IDF and FreeRTOS. Allocated two 150 KB LVGL v8 framebuffers in PSRAM, saving over 300 KB of internal SRAM for RTOS tasks.
- Wrote peripheral drivers for MAX30102 heart rate sensor at 100 Hz, BMI270 pedometer, and GT-U8 GPS at 9600 baud. Optimized 32.768 kHz RTC deep sleep to reduce standby current to 25 microamps.
- Connected the watch to a Flutter mobile app over NimBLE Bluetooth with 20 ms sync latency. Mirrored real-time Google Maps navigation instructions and supported 1.2 MB firmware updates via Wi-Fi HTTPS OTA.
