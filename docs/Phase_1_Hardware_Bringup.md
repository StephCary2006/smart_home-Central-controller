# Phase 1: Hardware Bring-up & Electrical Validation

> **Timeline:** Days 1–3 · **Prerequisites:** Breadboard, all BOM components, USB-C cable

---

## 1.1 Objective

Validate the complete hardware assembly, establish USB serial communication with the development workstation (HP EliteBook 8570w), and confirm power integrity under worst-case load conditions (simultaneous Wi-Fi TX + relay switching).

---

## 1.2 Breadboard Wiring Steps

### Step 1: Power Rails & Decoupling (HSR-1)

**Goal:** Suppress brownout voltage sags from Wi-Fi TX bursts and relay coil activation.

| Action | Detail |
|---|---|
| Place ESP32-C6-DevKitC-1 on breadboard | Center it to leave room on both sides |
| Connect 5V/VIN rail | From DevKit 5V pin to breadboard + rail |
| Connect GND rail | From DevKit GND pin to breadboard − rail |
| Place 100µF electrolytic capacitor | **Across** 5V and GND rails, **observe polarity** (+/−). Place as close to ESP32-C6 power pins as possible |
| Place 0.1µF (100nF) ceramic capacitor | **In parallel** with the 100µF cap across the same 5V/GND rails |

```
         ┌─────────────────────────────────┐
    5V ──┤(+) 100µF ∥ 0.1µF (−)           ├── GND
         │          Breadboard             │
         │     ┌──────────────┐            │
    5V ──┤─────┤ ESP32-C6     ├────────────┤── GND
         │     │  DevKitC-1   │            │
         └─────┴──────────────┴────────────┘
```

> **CAUTION:** The 100µF electrolytic capacitor is **polarized**. The longer leg is positive (+). Reversing polarity can cause the capacitor to fail or explode.

---

### Step 2: I2C Pull-Up Network (HSR-2)

**Goal:** Guarantee fast signal rise-time on the shared 400 kHz I2C bus.

| Action | Detail |
|---|---|
| Wire 4.7kΩ resistor from GPIO 6 (SDA) to 3.3V rail | External pull-up for SDA |
| Wire 4.7kΩ resistor from GPIO 7 (SCL) to 3.3V rail | External pull-up for SCL |
| Connect SHT31 module | SDA → GPIO 6, SCL → GPIO 7, VCC → 3.3V, GND → GND |
| Connect SSD1306 OLED | SDA → GPIO 6, SCL → GPIO 7, VCC → 3.3V, GND → GND |

```
    3.3V ──┬──[4.7kΩ]──┬── GPIO 6 (SDA) ──── SHT31 SDA ──── SSD1306 SDA
           │           │
           └──[4.7kΩ]──┼── GPIO 7 (SCL) ──── SHT31 SCL ──── SSD1306 SCL
                        │
    GND ────────────────┴── SHT31 GND ──── SSD1306 GND
```

> **IMPORTANT:** In firmware, internal pull-ups MUST be disabled via `gpio_pullup_dis()`. The external 4.7kΩ resistors replace the weak ~45kΩ internal ones.

---

### Step 3: Relay Module

| Action | Detail |
|---|---|
| Connect relay signal pin to **GPIO 18** | Optocoupler trigger input |
| Connect relay VCC to 5V rail | Power for the relay coil |
| Connect relay GND to GND rail | Common ground |

> **NOTE:** Do NOT use GPIO 8 or GPIO 9 for the relay. These are strapping pins (HSR-3).

---

### Step 4: Buzzer Module

| Action | Detail |
|---|---|
| Connect buzzer signal pin to **GPIO 19** | PWM or digital output |
| Connect buzzer VCC to 3.3V or 5V | Depends on module spec |
| Connect buzzer GND to GND rail | Common ground |

---

### Step 5: LDR Photoresistor Module (Pre-wire for Phase 2)

| Action | Detail |
|---|---|
| Connect LDR analog output to **GPIO 0** | ADC1_CH0 input |
| Connect VCC and GND | Per module spec |

---

### Step 6: PMS7003 / UART Connection (Pre-wire for Phase 2)

| Action | Detail |
|---|---|
| Connect PMS7003 TX → **GPIO 4** (UART1_RX) | Data line from sensor |
| Connect PMS7003 RX → **GPIO 5** (UART1_TX) | Command line to sensor |
| Connect VCC (5V) and GND | Per sensor spec |

> **TIP:** If PMS7003 is not available yet, use a CP2102 USB-to-UART bridge as a substitute (see BOM notes).

---

## 1.3 Firmware Tasks

### Task 1.3.1: Create `board_config.h` — Pin Definitions

Create a centralized header file with all hardware pin assignments and constants.

**File:** `main/board_config.h`

```c
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================
// ESP32-C6 Smart Home Controller — Hardware Pin Configuration
// ============================================================

// --- I2C Bus (400 kHz Fast-Mode) ---
#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_SDA_PIN         GPIO_NUM_6
#define I2C_SCL_PIN         GPIO_NUM_7
#define I2C_FREQ_HZ         400000      // 400 kHz Fast-Mode

// --- I2C Device Addresses ---
#define SHT31_I2C_ADDR      0x44        // Default SHT31 address
#define SSD1306_I2C_ADDR    0x3C        // Default SSD1306 address

// --- ADC (LDR Photoresistor) ---
#define LDR_ADC_CHANNEL     ADC_CHANNEL_0   // GPIO 0 = ADC1_CH0
#define LDR_ADC_UNIT        ADC_UNIT_1
#define LDR_ADC_ATTEN       ADC_ATTEN_DB_12 // Full range 0-3.3V
#define LDR_OVERSAMPLE_COUNT 16             // 16x oversampling

// --- UART (PMS7003 PM2.5 Sensor) ---
#define PMS_UART_PORT       UART_NUM_1
#define PMS_UART_RX_PIN     GPIO_NUM_4
#define PMS_UART_TX_PIN     GPIO_NUM_5
#define PMS_UART_BAUD       9600
#define PMS_UART_BUF_SIZE   256

// --- Relay Actuator (Opto-Isolated) ---
#define RELAY_GPIO_PIN      GPIO_NUM_18
#define RELAY_ACTIVE_LOW    0           // Set to 1 if relay triggers on LOW

// --- Active Buzzer ---
#define BUZZER_GPIO_PIN     GPIO_NUM_19

// --- Strapping Pins (DO NOT USE) ---
// GPIO 8 and GPIO 9 are reserved strapping pins.
// Never connect external loads to these during boot.

// --- FreeRTOS Task Priorities ---
#define TASK_PRIORITY_SENSOR    5
#define TASK_PRIORITY_DISPLAY   4
#define TASK_PRIORITY_UART      5
#define TASK_PRIORITY_SNTP      3
#define TASK_PRIORITY_CRON      4

// --- FreeRTOS Task Stack Sizes ---
#define TASK_STACK_SENSOR       4096
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_UART         4096
#define TASK_STACK_SNTP         4096
#define TASK_STACK_CRON         4096

#endif // BOARD_CONFIG_H
```

---

### Task 1.3.2: USB Serial Logging Verification

**Goal:** Confirm firmware flashing and serial logging work over native USB CDC/JTAG.

**Steps:**
1. Write a simple `app_main()` that logs a startup message
2. Flash the firmware via `idf.py flash`
3. Open serial monitor via `idf.py monitor`
4. Verify log output appears on the EliteBook

```c
// main.c — Phase 1: Basic bring-up test
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_config.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C6 Smart Home Controller v2.0");
    ESP_LOGI(TAG, "Phase 1: Hardware Bring-up");
    ESP_LOGI(TAG, "========================================");

    while (1) {
        ESP_LOGI(TAG, "Heartbeat — System running...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

**Acceptance Criteria:**
- [ ] Firmware flashes without errors via USB
- [ ] Serial monitor shows log output at correct baud rate
- [ ] Heartbeat message repeats every 2 seconds

---

### Task 1.3.3: GPIO Output Test (Relay & Buzzer)

**Goal:** Verify GPIO output control for the relay and buzzer.

```c
#include "driver/gpio.h"
#include "board_config.h"

// Initialize relay GPIO
static void relay_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(RELAY_GPIO_PIN, RELAY_ACTIVE_LOW ? 1 : 0); // OFF
}

// Initialize buzzer GPIO
static void buzzer_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BUZZER_GPIO_PIN, 0); // OFF
}
```

**Acceptance Criteria:**
- [ ] Relay clicks ON/OFF when GPIO 18 is toggled
- [ ] Buzzer sounds when GPIO 19 is set HIGH
- [ ] No unexpected resets during switching

---

### Task 1.3.4: Brownout Stress Test (Wi-Fi TX + Relay)

**Goal:** Validate that simultaneous Wi-Fi transmission and relay switching do NOT trigger brownout resets, thanks to the decoupling capacitor network.

**Test Procedure:**
1. Initialize Wi-Fi in station mode (connect to any AP or just start scanning)
2. Start a loop that toggles the relay ON/OFF every 500ms
3. Observe serial monitor for 5+ minutes
4. Check for brownout detector warnings or unexpected reboots

```c
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static void wifi_init_scan(void)
{
    // Initialize NVS (required for Wi-Fi)
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Trigger continuous Wi-Fi scanning (generates TX bursts)
    ESP_LOGI(TAG, "Starting Wi-Fi scan stress test...");
}
```

**Acceptance Criteria:**
- [ ] No brownout reset messages in serial output
- [ ] No `ESP_RST_BROWNOUT` in reset reason
- [ ] System runs stable for 5+ minutes under combined Wi-Fi + relay load
- [ ] ESP32-C6 brownout detector does NOT trip

---

## 1.4 Build & Flash Commands

```powershell
# Set up environment (run once per terminal session)
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.3.5"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.3_py3.11_env"
$env:PATH = "C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-13.2.0_20250707\riscv32-esp-elf\bin;" + $env:PATH

# Build
idf.py build

# Flash (replace COM_PORT with your actual port, e.g., COM3)
idf.py -p COM_PORT flash

# Monitor serial output
idf.py -p COM_PORT monitor

# Build + Flash + Monitor in one command
idf.py -p COM_PORT flash monitor
```

---

## 1.5 Phase 1 Completion Checklist

| # | Checkpoint | Status |
|:---:|---|:---:|
| 1 | Breadboard assembled with all components wired | ☐ |
| 2 | Decoupling capacitors (100µF ∥ 0.1µF) placed near ESP32-C6 power pins | ☐ |
| 3 | I2C pull-ups (4.7kΩ × 2) wired from GPIO 6/7 to 3.3V | ☐ |
| 4 | `board_config.h` created with all pin definitions | ☐ |
| 5 | USB Serial flashing and logging verified | ☐ |
| 6 | Relay GPIO toggle test passed | ☐ |
| 7 | Buzzer GPIO toggle test passed | ☐ |
| 8 | Brownout stress test (Wi-Fi + Relay) passed — 5 min stable | ☐ |
| 9 | GPIO 8/9 confirmed NOT connected to any external load | ☐ |

---

> **Next:** Proceed to [Phase 2: Peripheral Driver Hardening](Phase_2_Peripheral_Drivers.md)
