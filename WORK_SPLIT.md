# Kế Hoạch Phân Chia Công Việc Chi Tiết (Work Split & Execution Plan)
## Dự Án: ESP32-C6 Smart Home Central Controller Firmware (Rev 2.1)

* **Vai trò:** Kỹ sư phần mềm nhúng / Lập trình viên đơn lẻ (Solo Embedded Developer)
* **Thời gian hoàn thành:** 15 Ngày (Chia thành 4 Phases)
* **Ngôn ngữ & Công cụ:**
  * **Firmware:** Pure C11 (ESP-IDF v5.3.5 & FreeRTOS trên chip ESP32-C6 RISC-V)
  * **Testing & Tools:** Python 3 (`pyserial`, `pytest` trên máy trạm HP EliteBook 8570w)
  * **Quản lý mã nguồn:** Git (`main`, `develop`, `feature/phase-*`)

---

## 1. Tổng Quan Lộ Trình 4 Phase (15 Days Timeline)

```
 Ngày 1 ─── 3        Ngày 4 ─── 7        Ngày 8 ─── 11       Ngày 12 ─── 15
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   PHASE 1    │───▶│   PHASE 2    │───▶│   PHASE 3    │───▶│   PHASE 4    │
│ Hardware     │    │ Peripheral   │    │ Dual-Clock   │    │ Integration, │
│ Bring-up     │    │ Drivers (C)  │    │ Timekeeping  │    │ Test Harness │
│ & Power Prot │    │ & Robustness │    │ & Scheduling │    │ & 72h Stress │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

---

## 2. Chi Tiết Nhiệm Vụ Từng Ngày & Sản Phẩm Bàn Giao

### 📅 PHASE 1: Lắp Mạch Phần Cứng, Chống Sụt Áp & Khởi Động Hệ Thống (Ngày 1 – Ngày 3)
*Mục tiêu: Đảm bảo phần cứng cắm đúng sơ đồ, nguồn điện ổn định tuyệt đối (không bị brownout reset), nạp và debug được qua cổng USB.*

| Ngày | Buổi | Hạng mục công việc chi tiết | File / Module phụ trách | Tiêu chí nghiệm thu (Acceptance Criteria) |
| :---: | :---: | :--- | :--- | :--- |
| **Ngày 1** | Sáng | Cắm mạch Breadboard MB-102: Đặt ESP32-C6 ở trung tâm, gắn cặp tụ lọc nguồn $100\mu\text{F} \parallel 0.1\mu\text{F}$ sát chân nguồn 5V/GND (HSR-1). | Mạch phần cứng | Tụ phân cực đúng chiều (+/-), không chạm chập. |
|  | Chiều | Lắp 2 điện trở kéo lên ngoài $4.7\text{ k}\Omega$ từ SDA (`GPIO 6`) và SCL (`GPIO 7`) lên 3.3V (HSR-2). Cô lập `GPIO 8/9` (HSR-3). | Mạch phần cứng | Đo đồng hồ VOM: SDA & SCL đạt mức cao 3.3V khi thả nổi. |
| **Ngày 2** | Sáng | Nối dây Relay có cách ly Opto (`GPIO 18`), Còi Buzzer (`GPIO 19`), LDR (`GPIO 0`), PMS7003 (`GPIO 4/5`). | Mạch phần cứng | Kiểm tra cách ly Opto của Relay hoạt động độc lập với MCU. |
|  | Chiều | Tạo `main/include/board_config.h`, viết driver cơ bản `relay_ctrl.c` và `buzzer_ctrl.c`. | `board_config.h`<br>`relay_ctrl.c`<br>`buzzer_ctrl.c` | Relay nhảy tạch tạch theo nhịp, còi kêu bíp rõ ràng, không reset chip. |
| **Ngày 3** | Sáng | Viết chương trình kiểm tra truyền thông USB CDC/JTAG Serial logging ở tốc độ baud 115200. | `main/main.c` | Log xuất đều đặn qua `idf.py monitor` trên HP EliteBook. |
|  | Chiều | **Bài test sụt áp Brownout (HSR-1):** Bật Wi-Fi phát công suất cao liên tục đồng thời đóng/ngắt Relay chu kỳ 500ms trong 10 phút. | `main/main.c` | **PASS:** Không xuất hiện thông báo `ESP_RST_BROWNOUT` trong log. |

---

### 📅 PHASE 2: Viết Driver C Ngoại Vi & Xử Lý Sự Cố Đường Truyền (Ngày 4 – Ngày 7)
*Mục tiêu: Hoàn thiện toàn bộ các driver C cho SHT31, OLED SSD1306, ADC LDR, UART PMS7003 và cơ chế tự phục hồi bus I2C trong <100ms.*

| Ngày | Buổi | Hạng mục công việc chi tiết | File / Module phụ trách | Tiêu chí nghiệm thu (Acceptance Criteria) |
| :---: | :---: | :--- | :--- | :--- |
| **Ngày 4** | Sáng | Lập trình `i2c_manager.c/.h`: Cấu hình I2C Master 400kHz với ESP-IDF v5.x, tắt pull-up nội, tạo Mutex cấp phát tĩnh (`xSemaphoreCreateMutexStatic`). | `i2c_manager.c/.h` | Mutex ngăn ngừa xung đột bus giữa màn hình và cảm biến. |
|  | Chiều | Lập trình `i2c_recovery.c/.h`: Phát hiện SDA bị kéo LOW $>50\text{ ms}$, phát 9 xung SCL thủ công để giải phóng thanh ghi dịch và tạo xung STOP. | `i2c_recovery.c/.h` | Phục hồi thành công bus I2C trong $<100\text{ ms}$ mà không cần reboot MCU (NFR-2). |
| **Ngày 5** | Sáng | Lập trình `sht31_driver.c/.h`: Gửi lệnh đo độ chính xác cao `0x24 0x00`, đọc 6 byte, kiểm tra mã dư thừa tuần hoàn CRC-8 đa thức `0x31`. | `sht31_driver.c/.h` | Giá trị nhiệt độ $15\text{--}45^\circ\text{C}$, độ ẩm $0\text{--}100\%$, CRC check chuẩn. |
|  | Chiều | Lập trình `ssd1306_driver.c/.h`: Khởi tạo màn hình OLED qua I2C, bảng font $8\times 8$ tĩnh, hỗ trợ ghi 8 dòng chữ và cập nhật framebuffer. | `ssd1306_driver.c/.h` | Hiển thị sắc nét chữ và số trên cả 8 dòng màn hình OLED. |
| **Ngày 6** | Sáng | Lập trình `adc_light.c/.h`: Cấu hình ADC1 Channel 0 (`GPIO 0`), nạp đường cong hiệu chuẩn eFuse, bộ lọc lấy mẫu vượt mức $16\times$. | `adc_light.c/.h` | Điện áp đọc mượt mà ($0\text{--}3300\text{ mV}$), che tay vào quang trở điện áp giảm đều. |
|  | Chiều | Viết công cụ Python `tools/pms7003_simulator.py` hỗ trợ phát chuỗi byte frame 32-byte và chế độ tiêm lỗi `--inject-noise`. | `tools/pms7003_simulator.py` | Tạo đúng cấu trúc khung `0x42 0x4D` kèm checksum 16-bit. |
| **Ngày 7** | Sáng | Lập trình `uart_pms7003.c/.h`: Cài đặt UART1 (`GPIO 4/5`), Ring Buffer và Task FreeRTOS chạy máy trạng thái hữu hạn 5-State FSM. | `uart_pms7003.c/.h` | Bắt đúng frame PM1.0, PM2.5, PM10; loại bỏ byte rác giữa luồng. |
|  | Chiều | Tích hợp toàn bộ driver vào `main/main.c`, kiểm tra không dùng `malloc` trong vòng lặp đọc dữ liệu (NFR-3). | `main/main.c` | Toàn bộ 4 ngoại vi chạy đồng thời không bị nghẽn. |

---

### 📅 PHASE 3: Kiến Trúc Đồng Hồ Kép & Lập Lịch Cron (Ngày 8 – Ngày 11)
*Mục tiêu: Tách biệt hoàn toàn đồng hồ đơn điệu (đếm lùi rơ-le) và đồng hồ thực POSIX (lập lịch theo giờ), xử lý bù bước nhảy khi NTP cập nhật.*

| Ngày | Buổi | Hạng mục công việc chi tiết | File / Module phụ trách | Tiêu chí nghiệm thu (Acceptance Criteria) |
| :---: | :---: | :--- | :--- | :--- |
| **Ngày 8** | Sáng | Lập trình `timer_engine.c/.h`: Quản lý 8 bộ timer cấp phát tĩnh bằng `esp_timer`, hỗ trợ one-shot và periodic. | `timer_engine.c/.h` | Đếm lùi chính xác tuyệt đối theo phần cứng thạch anh (FR-1.1). |
|  | Chiều | Tích hợp callback hẹn giờ cho Relay và Còi báo: Hết giờ tự động ngắt tải và phát tiếng bíp thông báo. | `timer_engine.c` | Đếm lùi hoàn toàn không bị ảnh hưởng khi chỉnh giờ hệ thống. |
| **Ngày 9** | Sáng | Lập trình `sntp_sync.c/.h`: Kết nối Wi-Fi Station với chế độ tự kết nối lại không gây nghẽn, khởi chạy SNTP ngầm hỏi giờ từ `pool.ntp.org`. | `sntp_sync.c/.h` | Mất mạng hoặc sai DNS không bao giờ làm đơ việc đọc cảm biến (FR-1.3). |
|  | Chiều | Thiết lập múi giờ Việt Nam `ICT-7` (`GMT+7`), bắt sự kiện thông báo đồng bộ giờ thành công (`time_sync_notification_cb`). | `sntp_sync.c` | Lấy đúng chuỗi ngày giờ địa phương Việt Nam. |
| **Ngày 10** | Sáng | Lập trình `cron_scheduler.c/.h`: Quản lý lịch sự kiện theo ngày/giờ/phút/giây trên nền FreeRTOS tĩnh. | `cron_scheduler.c/.h` | Kích hoạt đúng thời điểm mong muốn (ví dụ 06:00:00 hàng ngày). |
|  | Chiều | Cơ chế bù bước nhảy NTP (`cron_scheduler_on_time_step`): Khi SNTP đồng bộ làm giờ hệ thống nhảy, tự tính lại hạn kích hoạt tiếp theo. | `cron_scheduler.c` | Chống kích hoạt kép và chống bỏ sót lịch khi giờ nhảy lùi hoặc tiến. |
| **Ngày 11** | Cả ngày | Kiểm thử tích hợp Đồng hồ kép: Hiển thị giờ NTP lên OLED, chạy song song timer đếm lùi 30s của rơ-le, ngắt Wi-Fi thử nghiệm. | `main/main.c` | Hệ thống vận hành trơn tru cả khi có mạng lẫn mất mạng. |

---

### 📅 PHASE 4: Rule Engine, USB Telemetry, Bộ Test Python & Chạy Tải 72h (Ngày 12 – Ngày 15)
*Mục tiêu: Đóng gói toàn bộ luật tự động hóa, định dạng chuỗi JSON USB CDC, viết Test Harness tự động và chạy kiểm thử bền bỉ 72 giờ.*

| Ngày | Buổi | Hạng mục công việc chi tiết | File / Module phụ trách | Tiêu chí nghiệm thu (Acceptance Criteria) |
| :---: | :---: | :--- | :--- | :--- |
| **Ngày 12** | Sáng | Lập trình `rule_engine.c/.h`: Cấu trúc mảng tĩnh 16 luật `IF-THEN`, ánh xạ vượt ngưỡng cảm biến sang hành động cơ cấu chấp hành. | `rule_engine.c/.h` | Hỗ trợ so sánh `>`, `<`, `==` kèm thời gian trễ nghỉ (`cooldown_sec`). |
|  | Chiều | Đăng ký các luật bảo vệ thực tế: Nhiệt độ $>35^\circ\text{C}$ bật quạt Relay 15 phút; Bụi $\text{PM2.5}>150\,\mu\text{g/m}^3$ kêu còi báo động. | `rule_engine.c` | Luật kích hoạt chuẩn xác, không bị hiện tượng rơ-le đóng ngắt liên tục. |
| **Ngày 13** | Sáng | Lập trình `telemetry.c/.h`: Xuất chuỗi JSON 1 dòng duy nhất qua USB CDC (chu kỳ 1s) chứa toàn bộ cảm biến, heap, lỗi I2C/UART (NFR-4). | `telemetry.c/.h` | Chuỗi JSON đúng chuẩn RFC 8259, không bị ngắt dòng giữa chừng. |
|  | Chiều | Hoàn thiện `tools/test_harness.py`: Đọc cổng COM, kiểm tra rò rỉ heap (`heap_min`), phát hiện khởi động lại qua `up_s`, chấm điểm PASS/FAIL. | `tools/test_harness.py` | Chạy chế độ `--self-test` đạt 100% PASS. |
| **Ngày 14** | Sáng | Chạy bài kiểm thử liên hợp nhanh 5 phút (`python tools/test_harness.py --duration 300`). | Toàn hệ thống | Không có cảnh báo heap, 0 reboot, dữ liệu cảm biến hợp lệ. |
|  | Chiều | Bắt đầu kích hoạt bài kiểm thử độ bền **72-Hour Continuous Endurance Run** trên máy trạm HP EliteBook 8570w. | Test harness | Ghi log liên tục ra file `test_report_*.json`. |
| **Ngày 15** | Cả ngày | Giám sát kết quả 72h: Phân tích biểu đồ heap, kiểm tra số lỗi bus I2C ($<10$), hoàn tất tài liệu báo cáo nghiệm thu và đóng gói Git Release. | `README.md`<br>Báo cáo JSON | **CHỨNG NHẬN V2.1:** Hoàn thành xuất sắc toàn bộ tiêu chí NFR và FR! |

---

## 3. Ma Trận Trách Nhiệm & Phân Tầng Kỹ Thuật

```
┌────────────────────────────────────────────────────────┐
│                   ỨNG DỤNG & DỊCH VỤ                   │
│   rule_engine.c   │   cron_scheduler.c   │   telemetry.c│
├────────────────────────────────────────────────────────┤
│                   THỜI GIAN & HỆ THỐNG                  │
│       timer_engine.c (Monotonic)  │  sntp_sync.c (NTP)  │
├────────────────────────────────────────────────────────┤
│                    DRIVERS NGOẠI VI                    │
│   i2c_manager.c   │   sht31_driver.c     │ ssd1306_driver.c│
│   i2c_recovery.c  │   adc_light.c        │ uart_pms7003.c │
│   relay_ctrl.c    │   buzzer_ctrl.c      │                │
├────────────────────────────────────────────────────────┤
│             ESP-IDF v5.3.5 HAL & FREERTOS KERNEL       │
├────────────────────────────────────────────────────────┤
│                   PHẦN CỨNG ESP32-C6                   │
│     Tụ lọc 100µF/0.1µF │ Trở kéo 4.7kΩ │ Relay Opto   │
└────────────────────────────────────────────────────────┘
```
