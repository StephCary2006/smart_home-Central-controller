#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"
#include "i2c_manager.h"
#include "i2c_recovery.h"
#include "sht31_driver.h"
#include "ssd1306_driver.h"
#include "adc_light.h"
#include "uart_pms7003.h"
#include "relay_ctrl.h"
#include "buzzer_ctrl.h"
#include "timer_engine.h"
#include "sntp_sync.h"
#include "cron_scheduler.h"
#include "rule_engine.h"
#include "telemetry.h"

static const char *TAG = "APP_MAIN";

static void morning_cron_cb(void *arg)
{
    ESP_LOGI(TAG, "Executing scheduled morning event (06:00:00 GMT+7)");
    buzzer_beep_async(3, 100);
}

void app_main(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "ESP32-C6 Smart Home Central Controller Firmware v2.1");
    ESP_LOGI(TAG, "Target: ESP32-C6 RISC-V @ 160MHz | Architecture: Pure C + FreeRTOS");
    ESP_LOGI(TAG, "============================================================");

    // 1. Initialize Actuators (Relay & Buzzer)
    ESP_ERROR_CHECK(relay_init());
    ESP_ERROR_CHECK(buzzer_init());

    // 2. Initialize Shared I2C Bus & check recovery
    if (i2c_bus_is_stuck()) {
        ESP_LOGW(TAG, "I2C bus stuck on boot, executing 9-pulse recovery...");
        i2c_bus_recover();
    } else {
        i2c_manager_init();
    }

    // 3. Initialize I2C Peripherals (Fault-tolerant)
    esp_err_t err_sht = sht31_init();
    if (err_sht != ESP_OK) {
        ESP_LOGW(TAG, "SHT31 sensor offline/unconnected at boot: %s", esp_err_to_name(err_sht));
    }

    esp_err_t err_oled = ssd1306_init();
    if (err_oled != ESP_OK) {
        ESP_LOGW(TAG, "SSD1306 OLED offline/unconnected at boot: %s", esp_err_to_name(err_oled));
    }

    // 4. Initialize ADC Light Sensor (GPIO 0, 16x oversampling)
    ESP_ERROR_CHECK(adc_light_init());

    // 5. Initialize UART PMS7003 Sensor & 5-State FSM parser task
    ESP_ERROR_CHECK(pms7003_init());

    // 6. Initialize Monotonic Timer Engine (esp_timer)
    ESP_ERROR_CHECK(timer_engine_init());

    // 7. Initialize Non-blocking Wi-Fi & SNTP Background Sync Task
    sntp_sync_init(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASS);

    // 8. Initialize Wall-Clock Cron Scheduler
    ESP_ERROR_CHECK(cron_scheduler_init());
    cron_schedule_t morning_sched = {
        .hour = 6,
        .minute = 0,
        .second = 0,
        .weekdays = 0x7F // Every day
    };
    cron_handle_t morning_handle;
    cron_scheduler_add("morning_event", &morning_sched, morning_cron_cb, NULL, &morning_handle);

    // 9. Initialize Rule Engine & register default safety rules
    ESP_ERROR_CHECK(rule_engine_init());

    // Rule 1: High temperature -> Energize relay (exhaust fan) for 15 mins (900s)
    rule_def_t r_temp = {
        .name = "high_temp_fan",
        .sensor = SENSOR_TEMPERATURE,
        .op = CMP_GREATER_THAN,
        .threshold = 35.0f,
        .action = ACTION_RELAY_ON_TIMED,
        .action_param = 900,
        .cooldown_sec = 300,
        .enabled = true,
    };
    rule_engine_add(&r_temp);

    // Rule 2: High PM2.5 air pollution -> Sound acoustic buzzer alarm
    rule_def_t r_pm = {
        .name = "pm25_alarm",
        .sensor = SENSOR_PM25,
        .op = CMP_GREATER_THAN,
        .threshold = 150.0f,
        .action = ACTION_BUZZER_BEEP,
        .action_param = 3,
        .cooldown_sec = 60,
        .enabled = true,
    };
    rule_engine_add(&r_pm);

    // 10. Initialize USB CDC JSON Telemetry
    ESP_ERROR_CHECK(telemetry_init());

    // Startup chirp: 2 quick beeps
    buzzer_beep_async(2, 80);

    ESP_LOGI(TAG, "All subsystems initialized successfully! Entering operational loop (1Hz cadence)...");

    // 11. Main Loop — Deterministic 1Hz sensor polling & telemetry cadence (NFR-1)
    TickType_t last_wake_time = xTaskGetTickCount();
    sht31_data_t env_data = {0};
    int light_mv = 0;
    pms7003_data_t pm_data = {0};
    char display_line[32];
    char time_buf[32];

    while (1) {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));

        // Read sensors
        esp_err_t ret_sht = sht31_read(&env_data);
        if (ret_sht != ESP_OK && i2c_bus_is_stuck()) {
            i2c_bus_recover();
        }

        adc_light_read(&light_mv);
        pms7003_get_data(&pm_data);

        // Evaluate rule engine
        rule_engine_evaluate(env_data.temperature_c, env_data.humidity_pct,
                             light_mv, pm_data.pm2_5_std);

        // Update SSD1306 OLED display if ready
        if (ssd1306_is_ready()) {
            ssd1306_clear();

            ssd1306_write_line(0, " ESP32-C6 HUB ");

            if (sntp_sync_is_synced()) {
                sntp_sync_get_time_str(time_buf, sizeof(time_buf), "%H:%M:%S (NTP)");
            } else {
                snprintf(time_buf, sizeof(time_buf), "Up: %lus (NoNTP)", (unsigned long)timer_engine_uptime_sec());
            }
            ssd1306_write_line(1, time_buf);

            if (env_data.valid) {
                snprintf(display_line, sizeof(display_line), "T:%.1fC H:%.0f%%",
                         env_data.temperature_c, env_data.humidity_pct);
            } else {
                snprintf(display_line, sizeof(display_line), "T:--.-C H:--%%");
            }
            ssd1306_write_line(2, display_line);

            snprintf(display_line, sizeof(display_line), "Light: %dmV", light_mv);
            ssd1306_write_line(3, display_line);

            if (pm_data.valid) {
                snprintf(display_line, sizeof(display_line), "PM2.5: %u ug/m3", pm_data.pm2_5_std);
            } else {
                snprintf(display_line, sizeof(display_line), "PM2.5: -- ug/m3");
            }
            ssd1306_write_line(4, display_line);

            snprintf(display_line, sizeof(display_line), "Relay: %s",
                     relay_get_state() ? "ACTIVE (ON)" : "OFF");
            ssd1306_write_line(5, display_line);

            uint32_t heap_k = (uint32_t)esp_get_free_heap_size() / 1024;
            snprintf(display_line, sizeof(display_line), "Heap: %luKB free", (unsigned long)heap_k);
            ssd1306_write_line(6, display_line);

            ssd1306_update();
        }

        // Emit single-line compact JSON Telemetry over USB CDC (NFR-4)
        telemetry_emit(&env_data, light_mv, &pm_data, relay_get_state(), sntp_sync_is_synced());
    }
}
