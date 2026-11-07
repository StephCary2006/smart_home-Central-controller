# Implementation Roadmap — ESP32-C6 Smart Home Central Controller

> **Target:** ESP32-C6-DevKitC-1 (RISC-V @ 160 MHz) · **Framework:** ESP-IDF v5.3.5 (Pure C & Python) · **Timeline:** 15 Days

---

## Roadmap Overview

```
Day 1──────3    Day 4──────7    Day 8──────11   Day 12─────15
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ PHASE 1  │───▶│ PHASE 2  │───▶│ PHASE 3  │───▶│ PHASE 4  │
│ Hardware │    │ Drivers  │    │ Timekeep │    │ Integrate│
│ Bring-up │    │ Hardening│    │ & Sched. │    │ & Stress │
└──────────┘    └──────────┘    └──────────┘    └──────────┘
```

---

## Phase Summary

| Phase | Name | Days | Core Objective | Key Deliverables |
|:---:|---|:---:|---|---|
| **1** | [Hardware Bring-up & Electrical Validation](Phase_1_Hardware_Bringup.md) | 1–3 | Validate breadboard wiring, USB flashing, and power integrity | USB Serial logging, GPIO test, relay switching under Wi-Fi TX without brownout |
| **2** | [Peripheral Driver Hardening](Phase_2_Peripheral_Drivers.md) | 4–7 | Build robust, mutex-guarded drivers for all peripherals | I2C driver (SHT31+SSD1306), I2C 9-pulse recovery, UART FSM parser, ADC calibration |
| **3** | [Dual Timekeeping & Scheduling Engine](Phase_3_Timekeeping_Scheduling.md) | 8–11 | Implement monotonic timer engine, SNTP sync, and wall-clock cron scheduler | `esp_timer` countdown engine, SNTP background task, cron scheduler with NTP step compensation |
| **4** | [Integration, Automation & Stress Test](Phase_4_Integration_Stress_Test.md) | 12–15 | Wire everything together, build test harness, run 72-hour endurance test | Rule engine, Python USB telemetry harness, 72-hour stability report |

---

## Target Project File Structure

```
Projec2/
├── CMakeLists.txt                  # Root CMake configuration
├── sdkconfig                       # ESP32-C6 config
├── PRD.md                          # Product Requirements Document (Rev 2.1)
├── BOM_Bill_of_materials.md        # Bill of Materials (100% preserved)
├── WORK_SPLIT.md                   # 15-Day Solo Developer Work Breakdown
├── README.md                       # Comprehensive User Guide
├── docs/                           # Phase & Git Documentation
│   ├── HARDWARE_WIRING_GUIDE.md    # Cẩm nang & sơ đồ đi dây Breadboard chi tiết
│   ├── GIT_SETUP_GUIDE.md          # Git repository & branching guide
│   ├── Implementation_Roadmap.md   # This overview document
│   ├── Phase_1_Hardware_Bringup.md
│   ├── Phase_2_Peripheral_Drivers.md
│   ├── Phase_3_Timekeeping_Scheduling.md
│   └── Phase_4_Integration_Stress_Test.md
├── main/
│   ├── CMakeLists.txt              # Component registration
│   ├── main.c                      # app_main() entry point & 1Hz loop
│   ├── include/                    # Header files
│   │   ├── board_config.h
│   │   ├── i2c_manager.h
│   │   ├── i2c_recovery.h
│   │   ├── sht31_driver.h
│   │   ├── ssd1306_driver.h
│   │   ├── adc_light.h
│   │   ├── uart_pms7003.h
│   │   ├── relay_ctrl.h
│   │   ├── buzzer_ctrl.h
│   │   ├── timer_engine.h
│   │   ├── sntp_sync.h
│   │   ├── cron_scheduler.h
│   │   ├── rule_engine.h
│   │   └── telemetry.h
│   ├── drivers/                    # Peripheral hardware driver implementations
│   └── services/                   # Application services & scheduling logic
└── tools/
    ├── requirements.txt            # Python dependencies (pyserial, pytest)
    ├── test_harness.py             # Python USB validation script & pass/fail reporter
    └── pms7003_simulator.py        # Python PMS7003 UART frame generator & fault injector
```

---

## Hardware Pinout Quick Reference

| GPIO | Interface | Device | Phase |
|:---:|---|---|:---:|
| **0** | ADC1_CH0 | LDR Photoresistor | 2 |
| **4** | UART1_RX | PMS7003 PM2.5 TX | 2 |
| **5** | UART1_TX | PMS7003 PM2.5 CMD | 2 |
| **6** | I2C SDA | SHT31 + SSD1306 | 1–2 |
| **7** | I2C SCL | SHT31 + SSD1306 | 1–2 |
| **8/9** | **RESERVED** | Strapping pins — DO NOT connect | — |
| **18** | GPIO Output | Relay Module (opto-isolated) | 1 |
| **19** | GPIO/PWM Output | Active Buzzer | 1 |
| **USB** | CDC/JTAG | EliteBook 8570w | 1 |

---

## Non-Functional Requirements Checklist

| NFR | Requirement | Enforced In |
|---|---|---|
| **NFR-1** | Sensor tasks use `vTaskDelayUntil()` for jitter-free cadence | Phase 2, 4 |
| **NFR-2** | I2C bus recovery in < 100ms without MCU reboot | Phase 2 |
| **NFR-3** | No `malloc`/`new` in main loop — all static allocation | All Phases |
| **NFR-4** | JSON telemetry over USB CDC for Python test runners | Phase 4 |
