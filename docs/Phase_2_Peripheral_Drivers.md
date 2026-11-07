# Phase 2: Peripheral Driver Hardening

> **Timeline:** Days 4–7 · **Prerequisites:** Phase 1 completed (hardware validated, serial logging working)

---

## 2.1 Objective

Implement robust, production-grade drivers for all peripherals:
- **I2C** bus with mutex arbitration and 9-pulse recovery (SHT31 + SSD1306)
- **UART** stream parser with ISR Ring Buffer and 5-state FSM (PMS7003)
- **ADC** with eFuse calibration and 16× oversampling (LDR)

All drivers must comply with **NFR-3**: zero dynamic heap allocation (`malloc`/`new`) in the operational loop.

---

## 2.2 Task Breakdown

```
Phase 2
├── Task 2.1: I2C Bus Manager (mutex-guarded)          [Day 4]
│   ├── i2c_manager.c/.h
│   └── Shared FreeRTOS mutex for bus arbitration
├── Task 2.2: I2C Bus Recovery (9-pulse)                [Day 4]
│   ├── i2c_recovery.c/.h
│   └── SDA-held-LOW detection + manual SCL clocking
├── Task 2.3: SHT31 Temperature/Humidity Driver         [Day 5]
│   ├── sht31_driver.c/.h
│   └── CRC-8 validation, measurement commands
├── Task 2.4: SSD1306 OLED Display Driver               [Day 5]
│   ├── ssd1306_driver.c/.h
│   └── I2C command/data protocol, text rendering
├── Task 2.5: ADC LDR Driver with Calibration           [Day 6]
│   ├── adc_light.c/.h
│   └── eFuse calibration + 16× oversampling filter
└── Task 2.6: UART PMS7003 FSM Parser                   [Day 6–7]
    ├── uart_pms7003.c/.h
    └── ISR Ring Buffer + 5-state FSM decoder
```

---

## 2.3 Detailed Task Guides

### Task 2.1: I2C Bus Manager — `i2c_manager.c/.h`

**PRD Reference:** FR-2.1 (I2C Bus Arbitration)

**Purpose:** Provide a shared, thread-safe I2C bus interface using a FreeRTOS mutex. All peripheral drivers (SHT31, SSD1306) must acquire this mutex before any I2C transaction.

**Key Design Decisions:**
- Use the **new I2C master driver** (`esp_driver_i2c`) from ESP-IDF v5.x, not the legacy `driver/i2c.h`
- Create a single `i2c_master_bus_handle_t` shared across all devices
- Wrap all transactions in `xSemaphoreTake(i2c_mutex, portMAX_DELAY)` / `xSemaphoreGive(i2c_mutex)`
- Disable internal pull-ups (`gpio_pullup_dis`) — external 4.7kΩ are wired (HSR-2)

**Interface Design:**

```c
// i2c_manager.h
#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * Initialize the shared I2C master bus and create the arbitration mutex.
 * Must be called once from app_main() before any I2C device init.
 */
esp_err_t i2c_manager_init(void);

/**
 * Acquire the I2C bus mutex. Blocks until available.
 * Every call MUST be paired with i2c_manager_release().
 */
void i2c_manager_acquire(void);

/**
 * Release the I2C bus mutex.
 */
void i2c_manager_release(void);

/**
 * Get the shared I2C master bus handle (for registering devices).
 */
i2c_master_bus_handle_t i2c_manager_get_bus(void);

#endif // I2C_MANAGER_H
```

**Implementation Notes:**
- The mutex must be created with `xSemaphoreCreateMutexStatic()` (NFR-3: no dynamic allocation)
- Configure I2C with `i2c_master_bus_config_t`:
  - `i2c_port = I2C_MASTER_PORT` (I2C_NUM_0)
  - `sda_io_num = I2C_SDA_PIN` (GPIO 6)
  - `scl_io_num = I2C_SCL_PIN` (GPIO 7)
  - `clk_source = I2C_CLK_SRC_DEFAULT`
  - `flags.enable_internal_pullup = false` ← Critical! External 4.7kΩ are used

**Acceptance Criteria:**
- [ ] I2C bus initializes at 400 kHz without errors
- [ ] Multiple tasks can safely share the bus via the mutex
- [ ] No bus collisions when SHT31 reads and OLED writes happen concurrently from different FreeRTOS tasks

---

### Task 2.2: I2C Bus Recovery — `i2c_recovery.c/.h`

**PRD Reference:** FR-2.2 (I2C Bus Recovery)

**Purpose:** If a peripheral holds SDA LOW for >50ms (bus freeze), recover the bus without a full MCU reboot.

**Recovery Algorithm:**
1. Check if SDA (GPIO 6) is held LOW
2. If stuck, de-init the I2C driver temporarily
3. Switch SCL (GPIO 7) to GPIO output mode
4. Clock **9 manual SCL pulses** (HIGH→LOW) to flush the stuck slave's shift register
5. Generate an I2C STOP condition (SDA LOW→HIGH while SCL is HIGH)
6. Re-init the I2C driver

```c
// i2c_recovery.h
#ifndef I2C_RECOVERY_H
#define I2C_RECOVERY_H

#include "esp_err.h"

/**
 * Attempt to recover a locked I2C bus by clocking 9 SCL pulses.
 * This should complete in < 100ms (NFR-2).
 *
 * @return ESP_OK if bus recovered, ESP_FAIL if still stuck
 */
esp_err_t i2c_bus_recover(void);

/**
 * Check if the I2C bus is in a stuck state (SDA held LOW).
 *
 * @return true if bus is stuck
 */
bool i2c_bus_is_stuck(void);

#endif // I2C_RECOVERY_H
```

**Implementation Notes:**
- Use `gpio_get_level(I2C_SDA_PIN)` to detect if SDA is LOW
- For the 9 pulses, configure SCL as `GPIO_MODE_OUTPUT` and toggle with ~5µs delays
- After recovery, re-initialize the I2C master bus via `i2c_manager_init()`
- Total recovery must complete in **< 100ms** (NFR-2)

**Acceptance Criteria:**
- [ ] Bus recovery completes in < 100ms
- [ ] After recovery, I2C communication resumes normally
- [ ] No MCU reboot required for recovery

---

### Task 2.3: SHT31 Temperature/Humidity Driver — `sht31_driver.c/.h`

**PRD Reference:** FR-2.1 (I2C Bus Arbitration — SHT31)

**Purpose:** Read temperature and humidity from the SHT31 sensor over the shared I2C bus.

**Interface Design:**

```c
// sht31_driver.h
#ifndef SHT31_DRIVER_H
#define SHT31_DRIVER_H

#include "esp_err.h"

typedef struct {
    float temperature_c;    // Temperature in Celsius
    float humidity_pct;     // Relative humidity in %
} sht31_data_t;

/**
 * Initialize the SHT31 sensor on the shared I2C bus.
 */
esp_err_t sht31_init(void);

/**
 * Read temperature and humidity. Thread-safe (acquires I2C mutex).
 */
esp_err_t sht31_read(sht31_data_t *data);

#endif // SHT31_DRIVER_H
```

**SHT31 I2C Protocol Summary:**

| Step | Command | Description |
|---|---|---|
| 1 | Write `0x24 0x00` | Single-shot measurement, high repeatability |
| 2 | Wait ~15ms | Measurement in progress |
| 3 | Read 6 bytes | `[Temp_MSB, Temp_LSB, Temp_CRC, Hum_MSB, Hum_LSB, Hum_CRC]` |
| 4 | Validate CRC-8 | Polynomial: `x^8 + x^5 + x^4 + 1` (0x31), Init: 0xFF |
| 5 | Convert raw values | `T(°C) = -45 + 175 × (raw / 65535)` · `RH(%) = 100 × (raw / 65535)` |

**Implementation Notes:**
- Always wrap reads in `i2c_manager_acquire()` / `i2c_manager_release()`
- Implement CRC-8 validation for data integrity
- Log warnings if CRC check fails (do not crash)

**Acceptance Criteria:**
- [ ] Temperature readings are within reasonable range (15–45°C indoors)
- [ ] Humidity readings are within 0–100%
- [ ] CRC validation catches corrupted data

---

### Task 2.4: SSD1306 OLED Display Driver — `ssd1306_driver.c/.h`

**PRD Reference:** FR-2.1 (I2C Bus Arbitration — SSD1306)

**Purpose:** Drive the 128×64 OLED display for local visual output (clock, sensor readings, timer status).

**Interface Design:**

```c
// ssd1306_driver.h
#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include "esp_err.h"

/**
 * Initialize the SSD1306 OLED display.
 */
esp_err_t ssd1306_init(void);

/**
 * Clear the display buffer and update.
 */
esp_err_t ssd1306_clear(void);

/**
 * Write a string at the specified row (0-7 for 128x64).
 */
esp_err_t ssd1306_write_line(uint8_t row, const char *text);

/**
 * Flush the display buffer to the screen.
 */
esp_err_t ssd1306_update(void);

#endif // SSD1306_DRIVER_H
```

**SSD1306 I2C Protocol:**
- **Command byte:** `0x00` prefix for commands, `0x40` prefix for data
- **Initialization sequence:** Display OFF → Set MUX ratio → Set display offset → Set start line → Set segment remap → Set COM scan direction → Set COM pins → Set contrast → Resume display → Set normal display → Set clock → Enable charge pump → Display ON
- All I2C transactions must acquire the shared mutex

**Acceptance Criteria:**
- [ ] Display initializes and shows content
- [ ] Text rendering works on all 8 rows
- [ ] No I2C collisions with SHT31 reads (mutex tested)

---

### Task 2.5: ADC LDR Driver — `adc_light.c/.h`

**PRD Reference:** FR-2.3 (Calibrated ADC)

**Purpose:** Read ambient light level from the LDR photoresistor using calibrated ADC with 16× oversampling.

**Interface Design:**

```c
// adc_light.h
#ifndef ADC_LIGHT_H
#define ADC_LIGHT_H

#include "esp_err.h"

/**
 * Initialize ADC1 for LDR reading with eFuse calibration.
 */
esp_err_t adc_light_init(void);

/**
 * Read calibrated light level with 16x oversampling.
 * @param voltage_mv  Output: averaged voltage in millivolts
 * @return ESP_OK on success
 */
esp_err_t adc_light_read(int *voltage_mv);

#endif // ADC_LIGHT_H
```

**Implementation Notes:**
- Use the **ADC Oneshot** driver from ESP-IDF v5.x (`esp_adc/adc_oneshot.h`)
- Apply **eFuse calibration** via `adc_cali_create_scheme_curve_fitting()` or `adc_cali_create_scheme_line_fitting()`
- Configure for **12-bit resolution** with `ADC_ATTEN_DB_12` (full 0–3.3V range)
- **16× oversampling:** Take 16 consecutive readings, sum them, divide by 16

```c
// Oversampling pseudocode
int32_t sum = 0;
for (int i = 0; i < LDR_OVERSAMPLE_COUNT; i++) {
    int raw;
    adc_oneshot_read(adc_handle, LDR_ADC_CHANNEL, &raw);
    int mv;
    adc_cali_raw_to_voltage(cali_handle, raw, &mv);
    sum += mv;
}
*voltage_mv = sum / LDR_OVERSAMPLE_COUNT;
```

**Acceptance Criteria:**
- [ ] ADC reads return calibrated voltage values (0–3300 mV range)
- [ ] Covering the LDR produces consistently lower voltage readings
- [ ] 16× oversampling produces smoother readings than single samples
- [ ] No dynamic allocation in the read path

---

### Task 2.6: UART PMS7003 FSM Parser — `uart_pms7003.c/.h`

**PRD Reference:** FR-2.4 (UART Stream FSM Parser)

**Purpose:** Parse continuous PMS7003 byte streams using a FreeRTOS Ring Buffer and a 5-state Finite State Machine (FSM) to handle frame misalignment and dropped bytes.

**PMS7003 Frame Format (32 bytes):**

```
Byte:  0    1    2    3    4   ...  29   30   31
     [0x42 0x4D] [LenH LenL] [Data...] [CkH CkL]
      ─Start─    ─Length─     ─Payload─  ─Checksum─
```

**5-State FSM Design:**

```
┌─────────┐  0x42   ┌─────────┐  0x4D   ┌─────────┐
│  IDLE   │────────▶│ GOT_42  │────────▶│ GOT_4D  │
│(State 0)│         │(State 1)│         │(State 2)│
└─────────┘         └────┬────┘         └────┬────┘
     ▲                   │ ≠0x4D              │
     │                   ▼                    ▼  Read length
     │              Back to IDLE        ┌─────────┐
     │                              ┌──▶│PAYLOAD  │
     │                              │   │(State 3)│
     │                              │   └────┬────┘
     │                              │        │ All bytes read
     │                              │        ▼
     │                              │   ┌─────────┐
     │                              │   │CHECKSUM │
     └──────────────────────────────┘   │(State 4)│
                                        └─────────┘
                                  Validate checksum → emit frame
```

**Interface Design:**

```c
// uart_pms7003.h
#ifndef UART_PMS7003_H
#define UART_PMS7003_H

#include "esp_err.h"

typedef struct {
    uint16_t pm1_0_std;     // PM1.0 concentration (standard, µg/m³)
    uint16_t pm2_5_std;     // PM2.5 concentration (standard, µg/m³)
    uint16_t pm10_std;      // PM10 concentration (standard, µg/m³)
    uint16_t pm1_0_atm;     // PM1.0 concentration (atmospheric, µg/m³)
    uint16_t pm2_5_atm;     // PM2.5 concentration (atmospheric, µg/m³)
    uint16_t pm10_atm;      // PM10 concentration (atmospheric, µg/m³)
    bool valid;             // true if checksum passed
} pms7003_data_t;

/**
 * Initialize UART for PMS7003 and start the parser task.
 */
esp_err_t pms7003_init(void);

/**
 * Get the latest parsed PM data. Thread-safe.
 */
esp_err_t pms7003_get_data(pms7003_data_t *data);

#endif // UART_PMS7003_H
```

**Implementation Notes:**
- Install UART driver with `uart_driver_install()` using the built-in ring buffer
- Use `uart_read_bytes()` in a dedicated FreeRTOS task to consume incoming bytes
- Process each byte through the FSM state machine
- On valid frame: copy parsed data to a shared structure protected by a mutex or atomic copy
- **Ring buffer** absorbs bursts; FSM handles byte-level alignment recovery

**FSM State Descriptions:**

| State | Name | Action |
|:---:|---|---|
| 0 | `IDLE` | Wait for `0x42`. Discard all other bytes |
| 1 | `GOT_START_1` | Received `0x42`. Next byte must be `0x4D` or reset to IDLE |
| 2 | `GOT_START_2` | Received `0x42 0x4D`. Read 2 bytes for frame length |
| 3 | `PAYLOAD` | Read remaining payload bytes into buffer |
| 4 | `CHECKSUM` | Validate checksum (sum of bytes 0..29 == bytes 30..31). If valid, emit frame. Reset to IDLE |

**Acceptance Criteria:**
- [ ] FSM correctly parses valid PMS7003 frames
- [ ] FSM recovers from mid-stream byte drops without getting stuck
- [ ] Checksum validation rejects corrupted frames
- [ ] No dynamic allocation in the parsing loop
- [ ] Parser runs in a dedicated FreeRTOS task, does not block other tasks

---

## 2.4 Integration Test — All Peripherals Running

After all drivers are implemented, create an integration test in `app_main()`:

```c
void app_main(void)
{
    // Initialize all subsystems
    i2c_manager_init();
    sht31_init();
    ssd1306_init();
    adc_light_init();
    pms7003_init();

    sht31_data_t env;
    int light_mv;
    pms7003_data_t pm;

    while (1) {
        // Read sensors
        sht31_read(&env);
        adc_light_read(&light_mv);
        pms7003_get_data(&pm);

        // Display on OLED
        char line[32];
        snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", env.temperature_c, env.humidity_pct);
        ssd1306_write_line(0, line);
        snprintf(line, sizeof(line), "Light: %dmV", light_mv);
        ssd1306_write_line(1, line);
        snprintf(line, sizeof(line), "PM2.5: %u ug/m3", pm.pm2_5_std);
        ssd1306_write_line(2, line);
        ssd1306_update();

        // Log to USB serial (JSON for NFR-4)
        ESP_LOGI("TEL", "{\"temp\":%.1f,\"hum\":%.0f,\"light\":%d,\"pm25\":%u}",
                 env.temperature_c, env.humidity_pct, light_mv, pm.pm2_5_std);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 2.5 Phase 2 Completion Checklist

| # | Checkpoint | Status |
|:---:|---|:---:|
| 1 | `i2c_manager` initialized with static mutex, 400 kHz bus | ☐ |
| 2 | `i2c_recovery` 9-pulse routine tested, completes < 100ms | ☐ |
| 3 | `sht31_driver` reads temperature/humidity with CRC validation | ☐ |
| 4 | `ssd1306_driver` displays text on all 8 rows | ☐ |
| 5 | I2C mutex prevents SHT31/SSD1306 bus collisions | ☐ |
| 6 | `adc_light` reads calibrated voltage with 16× oversampling | ☐ |
| 7 | `uart_pms7003` FSM parser decodes valid frames | ☐ |
| 8 | FSM parser recovers from mid-stream byte drops | ☐ |
| 9 | All drivers use zero `malloc`/`new` in operational loop (NFR-3) | ☐ |
| 10 | JSON telemetry output visible on USB serial (NFR-4) | ☐ |

---

> **Next:** Proceed to [Phase 3: Dual Timekeeping & Scheduling Engine](Phase_3_Timekeeping_Scheduling.md)
