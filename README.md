# ESP32-C6 Smart Home Central Controller Firmware (Rev 2.1)

> **Autonomous, local-first embedded hub for deterministic sensor telemetry, actuator control, and dual-clock real-time scheduling on the ESP32-C6 RISC-V SoC.**

---

## 1. Điểm Nổi Bật Của Bản Phát Hành (Key Highlights)

* **Chuẩn Hóa Ngôn Ngữ (Pure C & Python):**
  * **Firmware:** 100% C (C11) trên nền **ESP-IDF v5.3.5** và **FreeRTOS Kernel** (hoàn toàn loại bỏ Rust).
  * **Cấp phát bộ nhớ tĩnh 100% (NFR-3):** Tuyệt đối không dùng `malloc`/`new` trong vòng lặp chính; toàn bộ Task, Queue, Mutex đều sử dụng `StaticSemaphore_t` và `StaticTask_t`.
  * **Kiểm thử máy trạm (Host Tools):** Bộ script **Python 3** (`test_harness.py`, `pms7003_simulator.py`) hỗ trợ kiểm thử tự động, đọc chuỗi JSON qua USB CDC và tiêm lỗi kiểm chứng máy trạng thái.
* **Bảo Toàn 100% Danh Mục Linh Kiện (BOM):**
  * Giữ nguyên toàn bộ 12 linh kiện phần cứng đã chuẩn bị (ESP32-C6, SHT31, OLED SSD1306, LDR, PMS7003, Relay Opto, Buzzer, tụ lọc kép $100\mu\text{F} \parallel 0.1\mu\text{F}$, trở kéo ngoài $4.7\text{ k}\Omega$).
* **Kiến Trúc Phân Lớp Gọn Gàng:**
  * Mã nguồn được phân chia trực quan vào `main/include/`, `main/drivers/`, `main/services/`.

---

## 2. Sơ Đồ Chân Phần Cứng (Hardware Pinout Quick Reference)

| GPIO | Chức năng / Chế độ | Thiết bị kết nối | Lưu ý an toàn điện tử |
| :---: | :--- | :--- | :--- |
| **GPIO 6** | I2C SDA | SHT31 & SSD1306 OLED | Trở kéo ngoài $4.7\text{ k}\Omega$ lên 3.3V (tắt pull-up nội) |
| **GPIO 7** | I2C SCL | SHT31 & SSD1306 OLED | Trở kéo ngoài $4.7\text{ k}\Omega$ lên 3.3V |
| **GPIO 0** | ADC1_CH0 | Quang trở LDR | Hiệu chuẩn eFuse, lấy mẫu vượt mức $16\times$ chống nhiễu |
| **GPIO 4** | UART1_RX | PMS7003 PM2.5 TX Line | Ring Buffer ngắt + Máy trạng thái 5-State FSM |
| **GPIO 5** | UART1_TX | PMS7003 PM2.5 RX Line | Tốc độ truyền chuẩn 9600 baud |
| **GPIO 18** | Output (GPIO) | Module Relay 1 kênh | Kích hoạt qua Opto cách ly quang; có diode xả dòng cảm ứng |
| **GPIO 19** | Output (GPIO) | Còi báo Active Buzzer | Phát tiếng bíp cảnh báo bất đồng bộ |
| **GPIO 8/9** | **CẤM NỐI** | *Chân Strapping SoC* | **Tuyệt đối không nối tải ngoài** khi khởi động chip |
| **USB Native** | CDC/JTAG | Máy trạm HP EliteBook | Nạp firmware, xem log và xuất chuỗi JSON Telemetry |

---

## 3. Cấu Trúc Thư Mục Dự Án (Project Structure)

```
Projec2/
├── CMakeLists.txt                  # Root CMake build file
├── sdkconfig                       # Cấu hình ESP-IDF v5.3.5 cho ESP32-C6
├── PRD.md                          # Tài liệu yêu cầu sản phẩm (Rev 2.1)
├── BOM_Bill_of_materials.md        # Danh mục linh kiện phần cứng (BOM)
├── WORK_SPLIT.md                   # Bảng phân chia công việc chi tiết 15 ngày
├── README.md                       # Tài liệu hướng dẫn này
├── docs/                           # Hướng dẫn chi tiết từng Phase & Git
│   ├── HARDWARE_WIRING_GUIDE.md    # Cẩm nang & sơ đồ đi dây Breadboard chi tiết
│   ├── GIT_SETUP_GUIDE.md          # Hướng dẫn thiết lập Git & đẩy lên GitHub
│   ├── Implementation_Roadmap.md   # Lộ trình tổng thể 4 Phase
│   ├── Phase_1_Hardware_Bringup.md # Phase 1: Lắp mạch & chống sụt áp
│   ├── Phase_2_Peripheral_Drivers.md# Phase 2: Drivers ngoại vi & phục hồi I2C
│   ├── Phase_3_Timekeeping_Scheduling.md # Phase 3: Đồng hồ kép & Cron
│   └── Phase_4_Integration_Stress_Test.md # Phase 4: Rule Engine & Test 72h
├── main/
│   ├── CMakeLists.txt              # Đăng ký component & module C
│   ├── main.c                      # Điểm khởi chạy app_main() & vòng lặp 1Hz
│   ├── include/                    # Toàn bộ 14 file header (.h)
│   │   ├── board_config.h          # Cấu hình chân GPIO & tham số hệ thống
│   │   ├── i2c_manager.h           # Mutex quản lý bus I2C Master 400kHz
│   │   ├── i2c_recovery.h          # Cơ chế 9-pulse phục hồi bus kẹt
│   │   ├── sht31_driver.h          # Driver nhiệt độ/độ ẩm SHT31 (CRC8)
│   │   ├── ssd1306_driver.h        # Driver màn hình OLED SSD1306 (8x8 font)
│   │   ├── adc_light.h             # Driver quang trở ADC LDR (16x oversample)
│   │   ├── uart_pms7003.h          # Parser 5-state FSM cảm biến bụi PM2.5
│   │   ├── relay_ctrl.h            # Điều khiển rơ-le cách ly quang
│   │   ├── buzzer_ctrl.h           # Điều khiển còi buzzer bất đồng bộ
│   │   ├── timer_engine.h          # Động cơ đếm lùi đơn điệu (esp_timer)
│   │   ├── sntp_sync.h             # Đồng bộ giờ NTP ngầm không gây nghẽn
│   │   ├── cron_scheduler.h        # Lập lịch sự kiện theo giờ thực POSIX
│   │   ├── rule_engine.h           # Luật tự động hóa cảm biến -> chấp hành
│   │   └── telemetry.h             # Xuất JSON Telemetry qua USB CDC
│   ├── drivers/                    # 8 module C điều khiển phần cứng
│   └── services/                   # 5 module C xử lý dịch vụ & hệ thống
└── tools/
    ├── requirements.txt            # Thư viện Python (pyserial, pytest)
    ├── test_harness.py             # Script kiểm thử cổng COM & xuất báo cáo JSON
    └── pms7003_simulator.py        # Script giả lập cảm biến bụi & tiêm lỗi UART
```

---

## 4. Hướng Dẫn Biên Dịch & Nạp Mạch (Quickstart Guide)

### Bước 1: Mở PowerShell và cấu hình môi trường ESP-IDF v5.3.5
Chạy các lệnh thiết lập đường dẫn một lần cho mỗi phiên làm việc:
```powershell
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.3.5"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.3_py3.11_env"
$env:PATH = "C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-13.2.0_20250707\riscv32-esp-elf\bin;C:\Espressif\python_env\idf5.3_py3.11_env\Scripts;" + $env:PATH
```

### Bước 2: Biên dịch dự án Firmware C
```powershell
cd c:\file_BK\LearninAI\Projec2
idf.py build
```

### Bước 3: Nạp chương trình vào mạch ESP32-C6
*(Thay `COM3` bằng cổng COM thực tế khi cắm cáp USB vào máy tính):*
```powershell
idf.py -p COM3 flash monitor
```
*(Nhấn `Ctrl + ]` để thoát khỏi màn hình monitor).*

---

## 5. Hướng Dẫn Sử Dụng Bộ Công Cụ Python Test Harness & Simulator

### Cài đặt thư viện Python
```powershell
pip install -r tools/requirements.txt
```

### 1. Tự kiểm thử (Self-Test không cần cắm mạch)
Kiểm tra bộ parser JSON và giả lập khung truyền PMS7003 ngay trên máy trạm:
```powershell
python tools/test_harness.py --self-test
python tools/pms7003_simulator.py --self-test
```

### 2. Chạy bài kiểm tra tự động 5 phút qua cổng USB CDC
Đọc luồng JSON từ mạch ESP32-C6, kiểm tra sụt áp/reboot, rò rỉ bộ nhớ heap và lưu báo cáo nghiệm thu:
```powershell
python tools/test_harness.py --port COM3 --duration 300
```

### 3. Chạy bài kiểm tra bền bỉ 72 giờ (Endurance Stress Test)
```powershell
python tools/test_harness.py --port COM3 --duration 259200 --output test_report_72h.json
```

### 4. Giả lập cảm biến bụi PMS7003 và tiêm lỗi (Fault Injection)
Nếu bạn dùng cáp USB-to-UART cắm vào chân `GPIO 4 (RX)` và `GPIO 5 (TX)`:
```powershell
# Bật phát frame kèm tiêm lỗi để kiểm tra FSM tự hồi phục:
python tools/pms7003_simulator.py --port COM4 --inject-noise
```
