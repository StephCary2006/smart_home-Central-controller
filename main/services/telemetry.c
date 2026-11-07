#include "telemetry.h"
#include "i2c_manager.h"
#include "uart_pms7003.h"
#include "timer_engine.h"
#include "sntp_sync.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdio.h>

static const char *TAG = "TELEMETRY";

esp_err_t telemetry_init(void)
{
    ESP_LOGI(TAG, "USB CDC JSON Telemetry initialized (NFR-4 compliance)");
    return ESP_OK;
}

void telemetry_emit(const sht31_data_t *env,
                     int light_mv,
                     const pms7003_data_t *pm,
                     bool relay_state,
                     bool sntp_synced)
{
    time_t ts = sntp_sync_get_time();
    uint32_t uptime_s = timer_engine_uptime_sec();
    uint32_t heap_free = (uint32_t)esp_get_free_heap_size();
    uint32_t heap_min  = (uint32_t)esp_get_minimum_free_heap_size();
    uint32_t i2c_err   = i2c_manager_get_error_count();
    uint32_t uart_err  = pms7003_get_error_count();

    float temp = env ? env->temperature_c : 0.0f;
    float hum  = env ? env->humidity_pct : 0.0f;
    uint16_t pm25 = pm ? pm->pm2_5_std : 0;

    // Emit single-line compact JSON terminated by newline
    printf("{\"ts\":%ld,\"up_s\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"pm25\":%u,\"relay\":%d,\"sntp\":%d,\"heap\":%lu,\"heap_min\":%lu,\"i2c_err\":%lu,\"uart_err\":%lu}\n",
           (long)ts,
           (unsigned long)uptime_s,
           temp,
           hum,
           light_mv,
           (unsigned int)pm25,
           relay_state ? 1 : 0,
           sntp_synced ? 1 : 0,
           (unsigned long)heap_free,
           (unsigned long)heap_min,
           (unsigned long)i2c_err,
           (unsigned long)uart_err);
    fflush(stdout);
}
