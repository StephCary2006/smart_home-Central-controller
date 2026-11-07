# **Product Requirements Document (PRD)**

## **ESP32-C6 Smart Home Central Controller Firmware (Rev 2.1 — Pure C & Python Edition)**

* **Author / Lead Engineer:** Solo Developer
* **Target Hardware:** ESP32-C6 SoC (32-bit RISC-V core @ 160 MHz)
* **Development Workstation:** HP EliteBook 8570w (Windows 10/11)
* **Firmware Framework:** ESP-IDF v5.3.5 (Pure C11, FreeRTOS)
* **Testing & Simulation Stack:** Python 3 (`pyserial`, `pytest`)
* **Target Release:** v2.1 (Hardened Architecture, Pure C/Python, Zero Rust Dependency)

---

## **1. Executive Summary & Core Objectives**

The ESP32-C6 Smart Home Central Controller is an autonomous, local-first embedded hub designed for deterministic sensor telemetry, actuator control, and real-time scheduling.

Revision 2.1 standardizes the engineering stack strictly on **Pure C (ESP-IDF v5.3.5)** for firmware and **Python 3** for host test harnesses and hardware simulators. It completely eliminates any external Rust requirements while preserving 100% of the Bill of Materials (BOM) and circuit protection techniques.

```
[Application & Rule Engine (C)]
├── [Timekeeping Subsystem] (SNTP | POSIX Wall-Clock | esp_timer Monotonic)
├── [Sensor & Actuator Subsystem] (I2C Mutex + 9-Clock Recovery | UART FSM)
├── [FreeRTOS HAL] (Static Allocation, Queues, Mutexes, Zero malloc in loop)
└── [ESP32-C6 Hardware & Protection] (Decoupling | Opto-Relay | Pull-ups)
         │
         ▼ (USB CDC Serial JSON Telemetry)
[Host Test Harness & PMS7003 Simulator (Python 3)]
```

---

## **2. Hardware Safety & Electrical Protection Requirements**

### **2.1. Power Delivery & Brownout Suppression (HSR-1)**
* **Root Risk:** Simultaneous Wi-Fi TX power bursts ($\approx 350\text{--}400\text{ mA}$) and relay inductive coil energization induce transient voltage drops on the 3.3V/5V rails, triggering the ESP32-C6 Hardware Brownout Detector.
* **Mitigation Specification:**
  * The relay actuator must utilize an **opto-isolated (photocoupler) module** with an integrated freewheeling/flyback diode across the coil.
  * A dual-capacitor bulk decoupling network ($100\mu\text{F}$ low-ESR electrolytic in parallel with a $0.1\mu\text{F}$ ceramic capacitor) must be placed directly across the ESP32-C6 power input terminals to suppress high-frequency transient spikes.

### **2.2. I2C Signal Integrity & Pull-Up Network (HSR-2)**
* **Root Risk:** Sharing the $400\text{ kHz}$ Fast-Mode I2C bus between the SHT31 sensor and the high-traffic SSD1306 OLED display using internal pull-up resistors ($\approx 45\text{ k}\Omega$) causes unacceptable rise times ($t_r$), leading to corrupted packets and bus lockup.
* **Mitigation Specification:**
  * Disable internal software pull-ups (`enable_internal_pullup = false`) and enforce external physical pull-up resistors ($4.7\text{ k}\Omega$) tied from SDA (GPIO 6) and SCL (GPIO 7) to the 3.3V power rail.

### **2.3. Boot Pin & Strapping Isolation (HSR-3)**
* **Specification:** GPIO 8 and GPIO 9 are designated hardware strapping pins on the ESP32-C6. They must **never** be connected to external pull-up/pull-down loads, relays, or floating inputs that could compromise chip boot mode selection during power-on reset (POR).

---

## **3. Detailed Functional Requirements**

### **3.1. Time & Scheduling Subsystem (FR-1)**

| Requirement ID | Module | Specification | Failure Mitigation & Acceptance Criteria |
| :--- | :--- | :--- | :--- |
| **FR-1.1** | **Monotonic Countdown Engine** | Relative time tracking (e.g., $10\text{ s}$ delay, $15\text{ min}$ relay timeout). | **Mandatory:** Driven exclusively by Monotonic Clocks (`esp_timer_get_time()`). **Zero vulnerability** to step adjustments or NTP phase jumps. |
| **FR-1.2** | **Wall-Clock Scheduler (Cron)** | Absolute wall-clock event execution (e.g., triggering at 06:00:00 GMT+7). | Synchronized via POSIX time (`time()`). Recalculates next trigger delta when wall-clock is stepped forward/backward by NTP (`cron_scheduler_on_time_step()`). |
| **FR-1.3** | **Non-Blocking NTP Client** | SNTP daemon querying `pool.ntp.org` at periodic intervals. | Runs in an isolated background FreeRTOS task. Wi-Fi dropouts or DNS failures must never block sensor polling or monotonic timers. Automatic reconnection with exponential backoff. |

```
[System Clock Architecture]
             +--------------------> POSIX Wall-Clock (time()) ----> Cron / Scheduled Events
[NTP Sync] --+ (Step/Slew Adjust)
               
[Hardware XTAL] ------------------> Monotonic Timer (esp_timer) -> Countdown / Interval Timers
                                                       (Strictly Invariant)
```

### **3.2. Sensor Ingestion & Bus Robustness Subsystem (FR-2)**

| Requirement ID | Bus / Protocol | Target Peripheral | Implementation & Resilience Logic |
| :--- | :--- | :--- | :--- |
| **FR-2.1** | **I2C Bus Arbitration** | SHT31 Temp/Humidity & SSD1306 OLED | All read/write operations wrapped in a shared FreeRTOS Mutex (`xSemaphoreTake(i2c_mutex, portMAX_DELAY)`). Prevents display refreshes from colliding with sensor reads. |
| **FR-2.2** | **I2C Bus Recovery** | Shared I2C Bus (GPIO 6 / 7) | If SDA is held LOW by a peripheral for $> 50\text{ ms}$ (bus freeze), the MCU switches SCL to GPIO output mode, clocks **9 manual SCL pulses** to release the slave shift register, and issues an I2C STOP condition. Runs in $< 100\text{ ms}$ without system reboot. |
| **FR-2.3** | **Calibrated ADC** | LDR Photoresistor (GPIO 0) | Factory eFuse calibration curve applied; $16\times$ oversampling moving average filter to reject electrical ripple. |
| **FR-2.4** | **UART Stream FSM Parser** | PMS7003 PM2.5 (GPIO 4 / 5) | Raw data ingestion via FreeRTOS Ring Buffer, decoded using a 5-state **Finite State Machine (FSM)**. Eliminates frame misalignment due to dropped bytes. |

---

## **4. Hardware Pinout & Interface Mapping (100% BOM Preservation)**

| Pin Name | Interface / Mode | Connected Device | Hardware Protection / Critical Constraints |
| :--- | :--- | :--- | :--- |
| **GPIO 6** | I2C SDA | SHT31 & SSD1306 | Dedicated external $4.7\text{ k}\Omega$ pull-up resistor to 3.3V. |
| **GPIO 7** | I2C SCL | SHT31 & SSD1306 | Dedicated external $4.7\text{ k}\Omega$ pull-up resistor to 3.3V. |
| **GPIO 0** | ADC1_CH0 | LDR Photoresistor Module | 12-bit mode, eFuse attenuation calibration enabled. |
| **GPIO 4** | UART1_RX | PM2.5 Data TX Line | Ring-buffered ISR ingress with 5-state FSM framing parser. |
| **GPIO 5** | UART1_TX | PM2.5 Command Line | Standard 3.3V asynchronous serial output. |
| **GPIO 18** | Output (GPIO) | 1-Channel Relay Module | Optocoupler isolated trigger; active-low / active-high driver. |
| **GPIO 19** | Output (GPIO) | Active Buzzer | Acoustic countdown/alarm indicator. |
| **GPIO 8/9** | **Reserved** | *Unconnected / Isolated* | **Strapping Pins:** Do not connect loads during boot. |
| **Native USB** | USB CDC/JTAG | EliteBook 8570w Host | Serial logging, flashing, JTAG runtime debugging. |

---

## **5. Non-Functional Requirements (NFR)**

* **NFR-1 (Deterministic Concurrency):** Critical sensor polling tasks must yield via FreeRTOS `vTaskDelayUntil()` to guarantee jitter-free 1Hz cadence.
* **NFR-2 (Bus Lockup Resilience):** Automatic recovery from I2C bus lockup must execute in $< 100\text{ ms}$ without requiring a full MCU system reboot.
* **NFR-3 (Memory Safety & Static Guarantees):** Dynamic heap allocation (`malloc`/`new`) is strictly forbidden inside the main operational loop. All FreeRTOS Queues, Mutexes, and buffers must be statically allocated.
* **NFR-4 (Telemetry Portability):** Serial output must format live metrics into single-line JSON structures over USB CDC for direct consumption by host Python test runners:
  ```json
  {"ts":1730900000,"up_s":3600,"temp":28.5,"hum":65.2,"light":1850,"pm25":42,"relay":0,"sntp":1,"heap":180000,"heap_min":170000,"i2c_err":0,"uart_err":0}
  ```

---

## **6. Software Architecture & Layering**

```
main/
├── include/                      # Centralized Header Files
│   ├── board_config.h            # Hardware pin mappings & constants
│   ├── i2c_manager.h             # Mutex-arbitrated I2C bus driver
│   ├── i2c_recovery.h            # 9-pulse bus recovery
│   ├── sht31_driver.h            # SHT31 Temp/Humidity driver (CRC8)
│   ├── ssd1306_driver.h          # 128x64 OLED display driver (8x8 font)
│   ├── adc_light.h               # LDR light sensor with eFuse & 16x oversampling
│   ├── uart_pms7003.h            # 5-State FSM UART PM2.5 parser
│   ├── relay_ctrl.h              # Opto-isolated relay controller
│   ├── buzzer_ctrl.h             # Async active buzzer controller
│   ├── timer_engine.h            # Monotonic countdown/periodic timer engine
│   ├── sntp_sync.h               # Non-blocking Wi-Fi & SNTP client
│   ├── cron_scheduler.h          # Wall-clock cron scheduler
│   ├── rule_engine.h             # IF-THEN sensor-actuator automation engine
│   └── telemetry.h               # JSON metrics serializer
├── drivers/                      # Hardware Driver Implementations (.c)
├── services/                     # Business Logic & OS Service Implementations (.c)
└── main.c                        # app_main() entry point & deterministic loop
tools/
├── test_harness.py               # Python serial telemetry validator & report generator
├── pms7003_simulator.py          # Python UART frame generator & fault injector
└── requirements.txt              # Host Python dependencies (pyserial, pytest)
```

---

## **7. Verification & Acceptance Criteria**

1. **Firmware Build:** 100% clean compilation using ESP-IDF v5.3.5 (`idf.py build`) producing `Projec2.bin` and `Projec2.elf` without compiler warnings.
2. **Offline Unit / Self-Test:** `python tools/test_harness.py --self-test` and `python tools/pms7003_simulator.py --self-test` pass 100%.
3. **Hardware Bring-up:** No brownouts during simultaneous Wi-Fi TX + relay activation.
4. **Endurance Run:** Python test harness executes 72-hour continuous test on HP EliteBook 8570w with 0 reboots, stable heap watermark, and $<10$ total bus errors.
