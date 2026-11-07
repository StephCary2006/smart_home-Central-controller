# Sơ Đồ & Cẩm Nang Hướng Dẫn Đi Dây Mạch Phần Cứng (Hardware Wiring Guide)
## Dự Án: ESP32-C6 Smart Home Central Controller (Rev 2.1)

> **Mục tiêu:** Hướng dẫn chi tiết cách bố trí linh kiện, chọn màu dây và cắm chính xác từng chân trên **Breadboard MB-102 (830 lỗ)**; tuân thủ nghiêm ngặt các quy tắc bảo vệ điện tử (HSR-1, HSR-2, HSR-3) để hệ thống hoạt động ổn định 24/7 không bị sụt áp brownout hay kẹt bus.

---

## 1. Bảng Quy Ước Màu Dây Cắm (Dupont Wire Color Standards)

Để tránh cắm nhầm dẫn đến chập cháy linh kiện, hãy quy ước màu dây thống nhất như sau:

| Màu dây | Chức năng quy ước | Điểm xuất phát $\rightarrow$ Điểm đến |
| :---: | :--- | :--- |
| 🔴 **Đỏ** | **Nguồn +5V / VIN** | Chân 5V ESP32-C6 $\rightarrow$ Rail (+) trên Breadboard $\rightarrow$ Relay VCC, PMS7003 VCC |
| 🟠 **Cam** | **Nguồn +3.3V** | Chân 3.3V ESP32-C6 $\rightarrow$ Rail (+) phụ $\rightarrow$ SHT31, SSD1306, LDR, Trở kéo $4.7\text{ k}\Omega$ |
| ⚫ **Đen / Nâu**| **Mass / Ground (GND)** | Chân GND ESP32-C6 $\rightarrow$ Rail (−) trên Breadboard $\rightarrow$ Toàn bộ GND linh kiện |
| 🟢 **Xanh lá** | **I2C SDA (Dữ liệu)** | `GPIO 6` $\rightarrow$ Chân SDA của SHT31 và SSD1306 OLED |
| 🟡 **Vàng** | **I2C SCL (Xung nhịp)** | `GPIO 7` $\rightarrow$ Chân SCL của SHT31 và SSD1306 OLED |
| 🔵 **Xanh dương**| **ADC Analog Input** | `GPIO 0` $\rightarrow$ Chân tín hiệu AO (Analog Output) của module quang trở LDR |
| 🟣 **Tím** | **UART RX (Nhận dữ liệu)** | `GPIO 4` $\rightarrow$ Chân TXD của cảm biến bụi PMS7003 (hoặc CP2102) |
| ⚪ **Trắng** | **UART TX (Gửi lệnh)** | `GPIO 5` $\rightarrow$ Chân RXD của cảm biến bụi PMS7003 |
| 🔘 **Xám** | **Relay Trigger** | `GPIO 18` $\rightarrow$ Chân IN của Module Relay cách ly Opto |
| 🌸 **Hồng** | **Buzzer Trigger** | `GPIO 19` $\rightarrow$ Chân I/O (+) của Module Còi Active Buzzer |

---

## 2. Bảng Nối Dây Chi Tiết Từng Module (Pin-to-Pin Mapping Table)

### 2.1. Khối Vi Điều Khiển & Mạch Nguồn (ESP32-C6 & Power Decoupling)

| Chân ESP32-C6 | Nối vào đâu trên Breadboard | Ghi chú an toàn điện tử |
| :--- | :--- | :--- |
| **5V / VIN** | Rail (+) chính 5V của Breadboard | Nguồn cấp từ cổng USB máy tính qua chip nguồn của mạch DevKit |
| **GND** | Rail (−) chính của Breadboard | Nối chung toàn bộ Mass của hệ thống |
| **3.3V** | Hàng cắm cấp nguồn 3.3V | Cung cấp nguồn sạch cho cảm biến I2C và ADC |
| **GPIO 8 & 9** | **KHÔNG NỐI DÂY (BỎ TRỐNG)** | **HSR-3: Strapping Pins.** Tuyệt đối không nối tải ngoài lúc khởi động |

> [!CAUTION]
> **Vị trí đặt tụ lọc nguồn (HSR-1):**
> - Cắm tụ hóa **$100\mu\text{F}$** song song với tụ gốm **$0.1\mu\text{F}$ (104)** trực tiếp giữa rail **5V** và **GND** ngay sát vị trí chân cắm của ESP32-C6.
> - **Chú ý cực tính:** Chân dài của tụ hóa nối vào **5V (+)**; chân ngắn có vạch màu xám/trắng trên thân nối vào **GND (−)**. Cắm ngược cực tụ hóa sẽ bị nổ hoặc phù tụ!

---

### 2.2. Khối Màn Hình OLED & Cảm Biến I2C (Shared 400kHz I2C Bus)

*Sử dụng chung 2 chân `GPIO 6 (SDA)` và `GPIO 7 (SCL)`:*

| Chân Module SSD1306 (OLED) | Chân Module SHT31 | Nối vào ESP32-C6 | Trở kéo vật lý ngoài (HSR-2) |
| :---: | :---: | :---: | :--- |
| **VCC** | **VCC** | **3.3V Rail** | Không cắm lên 5V để tránh cháy cảm biến |
| **GND** | **GND** | **GND Rail** | Mass chung |
| **SDA** | **SDA** | **GPIO 6** | Nối 1 trở $4.7\text{ k}\Omega$ từ **GPIO 6** lên **3.3V** |
| **SCL** | **SCL** | **GPIO 7** | Nối 1 trở $4.7\text{ k}\Omega$ từ **GPIO 7** lên **3.3V** |

---

### 2.3. Khối Cơ Cấu Chấp Hành (Relay & Buzzer)

| Chân Module Relay 1 kênh | Nối vào ESP32-C6 / Nguồn | Chức năng & Bảo vệ |
| :---: | :--- | :--- |
| **VCC** | **5V Rail (+)** | Nguồn cuộn hút Relay (được lọc bởi tụ $100\mu\text{F}$) |
| **GND** | **GND Rail (−)** | Mass chung |
| **IN** | **GPIO 18** | Tín hiệu kích qua Optocoupler cách ly quang |

| Chân Module Active Buzzer | Nối vào ESP32-C6 / Nguồn | Chức năng |
| :---: | :--- | :--- |
| **VCC (+)** | **3.3V hoặc 5V** | Nguồn cho còi (tùy thông số module buzzer) |
| **GND (−)** | **GND Rail (−)** | Mass chung |
| **I/O (S)** | **GPIO 19** | Xung kích phát tiếng bíp bất đồng bộ |

---

### 2.4. Khối Cảm Biến Quang Trở & Cảm Biến Bụi (ADC & UART)

| Chân Module Quang Trở LDR | Nối vào ESP32-C6 / Nguồn | Chức năng |
| :---: | :--- | :--- |
| **VCC** | **3.3V Rail** | Nguồn ổn định cho cầu phân áp LDR |
| **GND** | **GND Rail** | Mass chung |
| **AO (Analog Out)** | **GPIO 0** | Tín hiệu điện áp tương tự nối vào `ADC1_CH0` |
| **DO (Digital Out)** | *Bỏ trống* | Không cần dùng chân số so sánh LM393 |

| Chân Cảm Biến PMS7003 | Nối vào ESP32-C6 / Nguồn | Chức năng |
| :---: | :--- | :--- |
| **VCC (Pin 1 & 2)** | **5V Rail (+)** | Nguồn nuôi quạt laser bên trong cảm biến bụi |
| **GND (Pin 3 & 4)** | **GND Rail (−)** | Mass chung |
| **TXD (Pin 5)** | **GPIO 4** | Đường truyền dữ liệu UART từ cảm biến vào MCU |
| **RXD (Pin 6)** | **GPIO 5** | Đường nhận lệnh điều khiển UART từ MCU |
| **RESET / SET** | *Mắc lên 3.3V hoặc để trống* | Mặc định cảm biến ở chế độ hoạt động bình thường |

---

## 3. Sơ Đồ Trực Quan Bố Trí Trên Breadboard MB-102 (ASCII Layout)

```
        Rail Dương (+) 5V   [====🔴====🔴====🔴====🔴====🔴====🔴====🔴====]
        Rail Âm    (−) GND  [====⚫====⚫====⚫====⚫====⚫====⚫====⚫====]
                              │    │
                  ┌───────────┴────┴───────────┐
                  │ Tụ: [100µF ∥ 0.1µF]       │ (Đặt sát chân 5V & GND)
                  └────────────────────────────┘
                              │    │
┌─────────────────────────────▼────▼────────────────────────────────────────┐
│                          ESP32-C6 DevKit                                  │
│  [5V] [GND] [3V3] [IO0] [IO4] [IO5] [IO6] [IO7] [IO8] [IO9] [IO18] [IO19]  │
└───┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬──────┬────┘
    │     │     │     │     │     │     │     │     │     │     │      │
    │     │     │     │     │     │     │     │   (CẤM) (CẤM)   │      │
    │     │     │     │     │     │     │     │               │      │
    │     │     │     │     │     │     │     └─────────┐     │      │
    │     │     │     │     │     │     │ (Trở 4.7kΩ)   │     │      │
    │     │     │     │     │     │     └─────[ 3.3V ]──┘     │      │
    │     │     │     │     │     │                           │      │
    │     │     │     │     │     └──────────────┐            │      │
    │     │     │     │     │        (Trở 4.7kΩ) │            │      │
    │     │     │     │     └────────────[ 3.3V ]┘            │      │
    │     │     │     │                                       │      │
    │     │     │     └──────► [LDR Photoresistor AO]         │      │
    │     │     │                                             │      │
    │     │     └────────────► [VCC: SHT31, OLED, LDR]        │      │
    │     │                                                   │      │
    │     ├──────────────────► [TXD PMS7003 / CP2102]         │      │
    │     └──────────────────► [RXD PMS7003]                  │      │
    │                                                         │      │
    └─────────────────────────────────────────────────────────┼──────► [Relay IN]
                                                              └──────► [Buzzer I/O]
```

---

## 4. Quy Trình Kiểm Thử & Đóng Điện Từng Bước (Hardware Bring-Up)

Thực hiện kiểm tra tuần tự để không bao giờ xảy ra tình trạng sốc điện hay sụt áp:

### Bước 1: Kiểm tra nguội (Cold Check bằng VOM) — Chưa cắm điện
1. Cắm toàn bộ linh kiện theo đúng sơ đồ trên.
2. Chuyển đồng hồ VOM về thang đo thông mạch (tiếng bíp):
   - Chạm 1 que vào chân **5V**, 1 que vào chân **GND**: Đồng hồ **KHÔNG ĐƯỢC KÊU BÍP** (nếu kêu là bị chập nguồn, phải rút ra kiểm tra lại ngay!).
   - Chạm 1 que vào **3.3V**, 1 que vào **GND**: Đồng hồ **KHÔNG ĐƯỢC KÊU BÍP**.
   - Kiểm tra cực tính tụ hóa $100\mu\text{F}$: Đảm bảo vạch xám (-) nối đúng vào rail GND.

### Bước 2: Cấp nguồn & Đo điện áp tĩnh (Warm Check)
1. Cắm cáp USB-C từ máy tính vào mạch ESP32-C6.
2. Đèn LED đỏ báo nguồn trên kit ESP32-C6 sáng rõ.
3. Chuyển đồng hồ VOM về thang đo điện áp một chiều DC 20V:
   - Đo giữa chân **5V** và **GND**: Đạt từ $4.85\text{ V} \rightarrow 5.15\text{ V}$.
   - Đo giữa chân **3.3V** và **GND**: Đạt từ $3.27\text{ V} \rightarrow 3.33\text{ V}$.
   - Dùng ngón tay chạm nhẹ lên chip ESP32-C6: Chip hoàn toàn mát hoặc chỉ ấm nhẹ, không có linh kiện nào nóng rát.

### Bước 3: Quan sát khởi động tự động của Firmware
Khi cắm điện, firmware v2.1 sẽ tự động chạy:
1. **Âm thanh:** Còi Buzzer phát ra **2 tiếng bíp ngắn** (`tít - tít`) báo hiệu hệ thống đã nạp thành công.
2. **Relay:** Giữ nguyên trạng thái ngắt (OFF).
3. **Màn hình OLED:** Sau khoảng 1-2 giây sẽ sáng và hiển thị 7 dòng thông số:
   ```
    ESP32-C6 HUB 
   Up: 5s (NoNTP)
   T:28.5C H:62%
   Light: 1820mV
   PM2.5: 35 ug/m3
   Relay: OFF
   Heap: 185KB free
   ```

### Bước 4: Kiểm tra phản hồi phần cứng (Functional Check)
* **Test quang trở LDR:** Lấy bàn tay che lên cảm biến quang trở $\rightarrow$ Dòng `Light:` trên OLED sẽ giảm điện áp từ $\approx 1800\text{ mV}$ xuống dưới $600\text{ mV}$.
* **Test cảm biến SHT31:** Thổi hơi ấm vào cảm biến SHT31 $\rightarrow$ Độ ẩm `H:` tăng lên $80\text{--}90\%$ và nhiệt độ `T:` tăng nhẹ.
* **Test Relay & Quạt:** Khi nhiệt độ phòng cao hơn $35^\circ\text{C}$ (hoặc lấy máy sấy hơ ấm nhẹ cảm biến), Relay sẽ đóng tách một tiếng và OLED đổi sang `Relay: ACTIVE (ON)` trong vòng 15 phút.

### Bước 5: Chạy công cụ kiểm thử tự động Python trên Laptop
Mở PowerShell tại máy tính HP EliteBook:
```powershell
# Chạy kiểm thử 5 phút qua cổng COM (thay COM3 bằng cổng nhận ESP32-C6):
python tools/test_harness.py --port COM3 --duration 300
```
Quan sát terminal hiển thị các gói tin JSON được phân tích tự động. Kết thúc 5 phút, báo cáo sẽ hiện:
`FINAL RESULT: [PASSED] SYSTEM FULLY CERTIFIED`.
