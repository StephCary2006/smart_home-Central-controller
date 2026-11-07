#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "hal/uart_types.h"
#include "hal/i2c_types.h"

// ============================================================
// ESP32-C6 Smart Home Controller — Hardware Pin Configuration
// Target SoC: ESP32-C6 (RISC-V @ 160 MHz)
// ============================================================

// --- I2C Bus (400 kHz Fast-Mode) ---
#define I2C_MASTER_PORT         I2C_NUM_0
#define I2C_SDA_PIN             GPIO_NUM_6
#define I2C_SCL_PIN             GPIO_NUM_7
#define I2C_FREQ_HZ             400000          // 400 kHz Fast-Mode

// --- I2C Device Addresses ---
#define SHT31_I2C_ADDR          0x44            // Default SHT31 address (ADDR pin to GND)
#define SSD1306_I2C_ADDR        0x3C            // Default SSD1306 0.96" OLED address

// --- ADC (LDR Photoresistor) ---
#define LDR_ADC_UNIT            ADC_UNIT_1
#define LDR_ADC_CHANNEL         ADC_CHANNEL_0   // GPIO 0 = ADC1_CH0
#define LDR_ADC_ATTEN           ADC_ATTEN_DB_12 // Full range 0 - ~3.3V
#define LDR_OVERSAMPLE_COUNT    16              // 16x oversampling

// --- UART (PMS7003 PM2.5 Sensor) ---
#define PMS_UART_PORT           UART_NUM_1
#define PMS_UART_RX_PIN         GPIO_NUM_4
#define PMS_UART_TX_PIN         GPIO_NUM_5
#define PMS_UART_BAUD           9600
#define PMS_UART_BUF_SIZE       256

// --- Relay Actuator (Opto-Isolated) ---
#define RELAY_GPIO_PIN          GPIO_NUM_18
#define RELAY_ACTIVE_LOW        0               // Set to 1 if relay module triggers on LOW

// --- Active Buzzer ---
#define BUZZER_GPIO_PIN         GPIO_NUM_19

// --- Strapping Pins (DO NOT USE) ---
// GPIO 8 and GPIO 9 are designated hardware strapping pins on the ESP32-C6.
// They must NEVER be connected to external loads during boot (HSR-3).

// --- Wi-Fi & SNTP Defaults ---
#ifndef CONFIG_ESP_WIFI_SSID
#define CONFIG_ESP_WIFI_SSID    "SmartHome_AP"
#endif

#ifndef CONFIG_ESP_WIFI_PASS
#define CONFIG_ESP_WIFI_PASS    "12345678"
#endif

#define SNTP_SERVER_NAME        "pool.ntp.org"
#define SNTP_TIMEZONE           "ICT-7"         // GMT+7 (Vietnam / Indochina Time)

// --- FreeRTOS Task Priorities ---
#define TASK_PRIORITY_SENSOR    5
#define TASK_PRIORITY_DISPLAY   4
#define TASK_PRIORITY_UART      5
#define TASK_PRIORITY_SNTP      3
#define TASK_PRIORITY_CRON      4
#define TASK_PRIORITY_TELEMETRY 3

// --- FreeRTOS Task Stack Sizes ---
#define TASK_STACK_SENSOR       4096
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_UART         4096
#define TASK_STACK_SNTP         4096
#define TASK_STACK_CRON         4096
#define TASK_STACK_TELEMETRY    4096

#endif // BOARD_CONFIG_H
