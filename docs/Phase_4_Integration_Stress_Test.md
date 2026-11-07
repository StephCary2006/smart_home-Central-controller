# Phase 4: Integration, Automation & 72-Hour Stress Test

> **Timeline:** Days 12–15 · **Prerequisites:** Phase 1–3 completed (all drivers + timekeeping working)

---

## 4.1 Objective

1. **Build the Rule Engine** — Link sensor threshold events to actuator actions via monotonic timers
2. **Build the Python Test Harness** — Automated USB telemetry validation on the EliteBook
3. **Execute the 72-Hour Endurance Test** — Continuous operation monitoring heap stability, bus errors, and timer accuracy

---

## 4.2 Task Breakdown

```
Phase 4
├── Task 4.1: Rule Engine                               [Day 12]
│   ├── rule_engine.c/.h
│   ├── Sensor threshold → Actuator action mapping
│   └── Monotonic timer integration for delayed actions
├── Task 4.2: JSON Telemetry Output                     [Day 12–13]
│   ├── telemetry.c/.h
│   ├── Single-line JSON over USB CDC (NFR-4)
│   └── System health metrics (heap, uptime, bus errors)
├── Task 4.3: Python USB Test Harness                   [Day 13–14]
│   ├── tools/test_harness.py
│   ├── Parse JSON telemetry from COM port
│   └── Automated pass/fail validation + logging
└── Task 4.4: 72-Hour Stress Test                       [Day 14–15]
    ├── Continuous endurance run
    ├── Monitor: heap, bus errors, timer drift, sensor data
    └── Generate stability report
```

---

## 4.3 Detailed Task Guides

### Task 4.1: Rule Engine — `rule_engine.c/.h`

**Purpose:** Define IF-THEN rules that connect sensor readings to actuator actions. Example rules:
- IF temperature > 35°C THEN turn ON relay (fan) for 15 minutes (monotonic timer)
- IF PM2.5 > 150 µg/m³ THEN sound buzzer alarm
- IF light level < 500 mV THEN turn ON relay (light) at 18:00 (cron)

**Interface Design:**

```c
// rule_engine.h
#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include "esp_err.h"

// Maximum number of rules (statically allocated)
#define MAX_RULES 16

// Sensor source types
typedef enum {
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_LIGHT,
    SENSOR_PM25,
} sensor_type_t;

// Comparison operator
typedef enum {
    CMP_GREATER_THAN,
    CMP_LESS_THAN,
    CMP_EQUAL,
} cmp_operator_t;

// Actuator action types
typedef enum {
    ACTION_RELAY_ON,
    ACTION_RELAY_OFF,
    ACTION_RELAY_ON_TIMED,   // ON for N seconds, then auto-OFF
    ACTION_BUZZER_BEEP,
    ACTION_BUZZER_OFF,
} action_type_t;

// Rule definition
typedef struct {
    const char *name;
    sensor_type_t sensor;
    cmp_operator_t op;
    float threshold;
    action_type_t action;
    uint32_t action_param;      // e.g., timer duration in seconds
    uint32_t cooldown_sec;      // Minimum time between re-triggers
    bool enabled;
} rule_def_t;

/**
 * Initialize the rule engine.
 */
esp_err_t rule_engine_init(void);

/**
 * Register a new rule.
 */
esp_err_t rule_engine_add(const rule_def_t *rule);

/**
 * Evaluate all rules against current sensor data.
 * Call this periodically from the main sensor loop.
 */
void rule_engine_evaluate(float temperature, float humidity,
                           int light_mv, uint16_t pm25);

#endif // RULE_ENGINE_H
```

**Implementation Notes:**

1. **Rule evaluation loop:**
   ```c
   void rule_engine_evaluate(float temp, float hum, int light, uint16_t pm25)
   {
       for (int i = 0; i < rule_count; i++) {
           rule_t *r = &rules[i];
           if (!r->enabled) continue;

           float value = get_sensor_value(r->sensor, temp, hum, light, pm25);
           bool triggered = compare(value, r->op, r->threshold);

           if (triggered && !in_cooldown(r)) {
               execute_action(r);
               r->last_triggered = timer_engine_now_us();
           }
       }
   }
   ```

2. **Timed relay action:**
   - When `ACTION_RELAY_ON_TIMED` is triggered:
     - Turn relay ON immediately
     - Start a **monotonic one-shot timer** for `action_param` seconds
     - Timer callback turns relay OFF and optionally beeps

3. **Cooldown mechanism:**
   - Use `timer_engine_now_us()` (monotonic) to track last trigger time
   - Prevent re-triggering within `cooldown_sec` to avoid relay chatter

**Example Rules:**

```c
rule_def_t rules[] = {
    {
        .name = "high_temp_fan",
        .sensor = SENSOR_TEMPERATURE,
        .op = CMP_GREATER_THAN,
        .threshold = 35.0f,
        .action = ACTION_RELAY_ON_TIMED,
        .action_param = 900,    // 15 minutes
        .cooldown_sec = 300,    // 5 minute cooldown
        .enabled = true,
    },
    {
        .name = "pm25_alarm",
        .sensor = SENSOR_PM25,
        .op = CMP_GREATER_THAN,
        .threshold = 150.0f,
        .action = ACTION_BUZZER_BEEP,
        .action_param = 5,      // 5 beeps
        .cooldown_sec = 60,     // 1 minute cooldown
        .enabled = true,
    },
};
```

**Acceptance Criteria:**
- [ ] Rules trigger correctly when sensor thresholds are crossed
- [ ] Timed relay actions use monotonic timers (not wall-clock)
- [ ] Cooldown prevents rapid re-triggering
- [ ] All rule storage is statically allocated (NFR-3)

---

### Task 4.2: JSON Telemetry Output — `telemetry.c/.h`

**PRD Reference:** NFR-4 (Telemetry Portability)

**Purpose:** Format all system metrics into single-line JSON structures over USB CDC serial for consumption by the Python test harness.

**Interface Design:**

```c
// telemetry.h
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "esp_err.h"
#include "sht31_driver.h"
#include "uart_pms7003.h"

/**
 * Initialize telemetry output.
 */
esp_err_t telemetry_init(void);

/**
 * Emit a telemetry JSON line to USB serial.
 * Call this every ~1 second from the main loop.
 */
void telemetry_emit(const sht31_data_t *env,
                     int light_mv,
                     const pms7003_data_t *pm,
                     bool relay_state,
                     bool sntp_synced);

#endif // TELEMETRY_H
```

**JSON Output Format:**

Each line is a complete JSON object (no multi-line, no pretty-print):

```json
{"ts":1234567890,"up_s":3600,"temp":28.5,"hum":65.2,"light":1850,"pm25":42,"relay":0,"sntp":1,"heap":180000,"heap_min":170000,"i2c_err":0,"uart_err":0}
```

**Field Definitions:**

| Field | Type | Description |
|---|---|---|
| `ts` | int | POSIX timestamp (seconds since epoch), 0 if not synced |
| `up_s` | int | Uptime in seconds (monotonic) |
| `temp` | float | Temperature in °C |
| `hum` | float | Relative humidity in % |
| `light` | int | LDR voltage in mV |
| `pm25` | int | PM2.5 concentration in µg/m³ |
| `relay` | int | Relay state: 0=OFF, 1=ON |
| `sntp` | int | NTP sync status: 0=not synced, 1=synced |
| `heap` | int | Free heap bytes (current) |
| `heap_min` | int | Minimum free heap since boot (watermark) |
| `i2c_err` | int | Cumulative I2C error count |
| `uart_err` | int | Cumulative UART/FSM error count |

**Implementation Notes:**

```c
void telemetry_emit(const sht31_data_t *env, int light_mv,
                     const pms7003_data_t *pm, bool relay_state,
                     bool sntp_synced)
{
    time_t now = sntp_synced ? time(NULL) : 0;
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_s = (uint32_t)(uptime_us / 1000000);
    uint32_t heap_free = esp_get_free_heap_size();
    uint32_t heap_min = esp_get_minimum_free_heap_size();

    printf("{\"ts\":%ld,\"up_s\":%lu,\"temp\":%.1f,\"hum\":%.1f,"
           "\"light\":%d,\"pm25\":%u,\"relay\":%d,\"sntp\":%d,"
           "\"heap\":%lu,\"heap_min\":%lu,\"i2c_err\":%d,\"uart_err\":%d}\n",
           (long)now, uptime_s,
           env->temperature_c, env->humidity_pct,
           light_mv, pm->pm2_5_std,
           relay_state ? 1 : 0, sntp_synced ? 1 : 0,
           heap_free, heap_min,
           i2c_error_count, uart_error_count);
}
```

**Acceptance Criteria:**
- [ ] Each telemetry line is valid JSON (parseable by Python `json.loads()`)
- [ ] Lines are single-line (no embedded newlines)
- [ ] All fields present in every emission
- [ ] `heap` and `heap_min` values are non-zero and reasonable
- [ ] Error counters increment correctly when errors occur

---

### Task 4.3: Python USB Test Harness — `tools/test_harness.py`

**Purpose:** Automated validation script running on the EliteBook 8570w. Connects to the ESP32-C6 USB serial port, parses JSON telemetry, and validates system behavior.

**Script Outline:**

```python
#!/usr/bin/env python3
"""
ESP32-C6 Smart Home Controller — Automated Test Harness
Reads JSON telemetry from USB serial and validates system health.
"""

import serial
import json
import time
import sys
from datetime import datetime

# Configuration
COM_PORT = "COM3"           # Adjust for your system
BAUD_RATE = 115200
TEST_DURATION_SEC = 300     # 5-minute quick test (72h for endurance)

# Thresholds
MIN_HEAP_BYTES = 50000      # Minimum acceptable free heap
MAX_I2C_ERRORS = 10         # Maximum acceptable I2C errors
MAX_UART_ERRORS = 10        # Maximum acceptable UART errors
TEMP_MIN = 10.0             # Minimum reasonable temperature
TEMP_MAX = 50.0             # Maximum reasonable temperature

class TestHarness:
    def __init__(self, port, baud):
        self.ser = serial.Serial(port, baud, timeout=2)
        self.results = {
            "total_lines": 0,
            "valid_json": 0,
            "invalid_json": 0,
            "heap_warnings": 0,
            "temp_out_of_range": 0,
            "i2c_errors_max": 0,
            "uart_errors_max": 0,
            "uptime_gaps": 0,
            "start_time": datetime.now().isoformat(),
        }
        self.last_uptime = 0

    def parse_line(self, line):
        try:
            data = json.loads(line)
            self.results["valid_json"] += 1
            return data
        except json.JSONDecodeError:
            self.results["invalid_json"] += 1
            return None

    def validate(self, data):
        # Check heap
        if data.get("heap", 0) < MIN_HEAP_BYTES:
            self.results["heap_warnings"] += 1
            print(f"⚠ LOW HEAP: {data['heap']} bytes")

        # Check temperature range
        temp = data.get("temp", 0)
        if temp < TEMP_MIN or temp > TEMP_MAX:
            self.results["temp_out_of_range"] += 1

        # Track error counters
        self.results["i2c_errors_max"] = max(
            self.results["i2c_errors_max"], data.get("i2c_err", 0))
        self.results["uart_errors_max"] = max(
            self.results["uart_errors_max"], data.get("uart_err", 0))

        # Check uptime continuity (detect reboots)
        uptime = data.get("up_s", 0)
        if uptime < self.last_uptime:
            self.results["uptime_gaps"] += 1
            print(f"⚠ REBOOT DETECTED: uptime went from {self.last_uptime}s to {uptime}s")
        self.last_uptime = uptime

    def run(self, duration_sec):
        print(f"Starting test ({duration_sec}s) on {COM_PORT}...")
        end_time = time.time() + duration_sec

        while time.time() < end_time:
            try:
                raw = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if not raw:
                    continue

                self.results["total_lines"] += 1
                data = self.parse_line(raw)
                if data:
                    self.validate(data)

                    # Print periodic status
                    if self.results["total_lines"] % 60 == 0:
                        print(f"[{self.results['total_lines']}] "
                              f"up={data.get('up_s',0)}s "
                              f"heap={data.get('heap',0)} "
                              f"temp={data.get('temp',0):.1f}°C")

            except Exception as e:
                print(f"Error: {e}")

        self.report()

    def report(self):
        r = self.results
        r["end_time"] = datetime.now().isoformat()
        r["duration_sec"] = (datetime.fromisoformat(r["end_time"])
                             - datetime.fromisoformat(r["start_time"])).total_seconds()

        print("\n" + "=" * 60)
        print("TEST REPORT")
        print("=" * 60)
        print(f"Duration:         {r['duration_sec']:.0f}s")
        print(f"Total lines:      {r['total_lines']}")
        print(f"Valid JSON:       {r['valid_json']}")
        print(f"Invalid JSON:     {r['invalid_json']}")
        print(f"Heap warnings:    {r['heap_warnings']}")
        print(f"Temp OOR:         {r['temp_out_of_range']}")
        print(f"Max I2C errors:   {r['i2c_errors_max']}")
        print(f"Max UART errors:  {r['uart_errors_max']}")
        print(f"Reboots detected: {r['uptime_gaps']}")
        print("=" * 60)

        # PASS/FAIL
        passed = (r["uptime_gaps"] == 0
                  and r["heap_warnings"] == 0
                  and r["i2c_errors_max"] <= MAX_I2C_ERRORS
                  and r["uart_errors_max"] <= MAX_UART_ERRORS
                  and r["invalid_json"] < r["total_lines"] * 0.01)

        if passed:
            print("RESULT: ✅ PASS")
        else:
            print("RESULT: ❌ FAIL")

        # Save report to file
        with open(f"test_report_{int(time.time())}.json", "w") as f:
            json.dump(r, f, indent=2)


if __name__ == "__main__":
    duration = int(sys.argv[1]) if len(sys.argv) > 1 else TEST_DURATION_SEC
    harness = TestHarness(COM_PORT, BAUD_RATE)
    harness.run(duration)
```

**Usage:**

```powershell
# Quick 5-minute validation
python tools/test_harness.py 300

# Full 72-hour endurance test
python tools/test_harness.py 259200
```

**Acceptance Criteria:**
- [ ] Script connects to ESP32-C6 USB serial successfully
- [ ] Parses JSON telemetry lines correctly
- [ ] Detects reboots via uptime gap detection
- [ ] Generates pass/fail report with all metrics
- [ ] Saves report to JSON file

---

### Task 4.4: 72-Hour Endurance Stress Test

**Purpose:** Validate the complete system under continuous operation for 72 hours. This tests long-term heap stability, bus reliability, timer accuracy, and overall firmware robustness.

**Test Setup:**

1. Flash the complete integrated firmware (all phases)
2. Connect all peripherals (SHT31, SSD1306, LDR, PMS7003, Relay, Buzzer)
3. Configure Wi-Fi with a stable AP
4. Start the Python test harness in 72-hour mode
5. Leave the system running undisturbed

**Monitoring Targets:**

| Metric | Pass Criteria | Failure Indicator |
|---|---|---|
| **Heap stability** | `heap_min` does not decrease continuously | Memory leak (heap_min trends toward 0) |
| **Uptime continuity** | Zero reboots (uptime never resets) | Brownout, watchdog, panic resets |
| **I2C bus health** | < 10 total I2C errors over 72h | Bus lockup, SDA stuck LOW |
| **UART parser health** | < 10 total UART errors over 72h | FSM stuck, ring buffer overflow |
| **Timer accuracy** | Periodic timers jitter < 50ms | Timer drift or missed triggers |
| **NTP sync** | `sntp` field stays 1 after initial sync | Wi-Fi or DNS persistent failure |
| **Sensor data validity** | Temperature 10–50°C, humidity 0–100% | Sensor failure or wiring issue |
| **Relay switching** | Relay toggles correctly per rules/cron | GPIO failure, brownout on switch |
| **JSON telemetry** | < 1% invalid JSON lines | Buffer overflow, serial corruption |

**Post-Test Analysis:**

After the 72-hour test, review:
1. `test_report_*.json` — Automated test results
2. Serial log — Any ESP_LOGW/ESP_LOGE messages
3. `heap_min` trend — Should be stable (no leak)
4. Error counters — Should be near zero

---

## 4.4 Final System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         app_main()                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Rule Engine   │  │ Cron Sched.  │  │ Timer Engine │      │
│  │ (sensor →     │  │ (wall-clock  │  │ (monotonic   │      │
│  │  actuator)    │  │  events)     │  │  countdowns) │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                  │              │
│  ┌──────▼─────────────────▼──────────────────▼───────┐      │
│  │              Telemetry (JSON → USB CDC)            │      │
│  └───────────────────────┬───────────────────────────┘      │
│                          │                                  │
├──────────────────────────┼──────────────────────────────────┤
│                     DRIVER LAYER                            │
│                                                             │
│  ┌─────────────┐  ┌────────────┐  ┌───────────────┐        │
│  │ I2C Manager  │  │ ADC Light  │  │ UART PMS7003  │        │
│  │  (Mutex)     │  │ (16× avg)  │  │ (FSM Parser)  │        │
│  ├──────┬──────┤  └────────────┘  └───────────────┘        │
│  │ SHT31│SSD1306│                                           │
│  └──────┴──────┘                                            │
│                                                             │
│  ┌─────────────┐  ┌────────────┐  ┌───────────────┐        │
│  │ Relay Ctrl   │  │ Buzzer Ctrl│  │ SNTP Sync     │        │
│  │ (GPIO 18)    │  │ (GPIO 19)  │  │ (Wi-Fi + NTP) │        │
│  └─────────────┘  └────────────┘  └───────────────┘        │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                   FreeRTOS + ESP-IDF HAL                    │
├─────────────────────────────────────────────────────────────┤
│              ESP32-C6 Hardware (RISC-V @ 160 MHz)           │
│         Decoupling Caps │ 4.7kΩ Pull-ups │ Opto-Relay      │
└─────────────────────────────────────────────────────────────┘
```

---

## 4.5 Phase 4 Completion Checklist

| # | Checkpoint | Status |
|:---:|---|:---:|
| 1 | Rule engine evaluates sensor thresholds correctly | ☐ |
| 2 | Timed relay actions use monotonic timers | ☐ |
| 3 | Cooldown prevents rule chatter | ☐ |
| 4 | JSON telemetry emitted every ~1 second | ☐ |
| 5 | All JSON fields present and valid | ☐ |
| 6 | `heap` and `heap_min` reported correctly | ☐ |
| 7 | Python test harness connects and parses JSON | ☐ |
| 8 | Reboot detection via uptime gap works | ☐ |
| 9 | 5-minute quick test passes | ☐ |
| 10 | 72-hour endurance test — zero reboots | ☐ |
| 11 | 72-hour endurance test — heap stable (no leak) | ☐ |
| 12 | 72-hour endurance test — < 10 I2C errors | ☐ |
| 13 | 72-hour endurance test — < 10 UART errors | ☐ |
| 14 | 72-hour endurance test — < 1% invalid JSON | ☐ |
| 15 | Final test report saved as JSON | ☐ |

---

## 4.6 Project Completion Summary

Upon successful completion of all 4 phases:

| Deliverable | Status |
|---|---|
| Hardware validated with brownout-proof power delivery | Phase 1 |
| Mutex-guarded I2C with 9-pulse bus recovery | Phase 2 |
| UART FSM parser with ring buffer | Phase 2 |
| Calibrated ADC with 16× oversampling | Phase 2 |
| Monotonic countdown/interval timer engine | Phase 3 |
| Non-blocking SNTP synchronization | Phase 3 |
| Wall-clock cron scheduler with NTP step compensation | Phase 3 |
| Sensor → Actuator rule engine | Phase 4 |
| JSON telemetry over USB CDC | Phase 4 |
| Python automated test harness | Phase 4 |
| 72-hour stability certification | Phase 4 |

> **Congratulations!** The ESP32-C6 Smart Home Central Controller Firmware v2.0 is complete. 🎉

---

> **Back to:** [Implementation Roadmap](Implementation_Roadmap.md)
