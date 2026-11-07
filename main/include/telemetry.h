#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "esp_err.h"
#include "sht31_driver.h"
#include "uart_pms7003.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB CDC telemetry output system.
 */
esp_err_t telemetry_init(void);

/**
 * @brief Emit single-line compact JSON metrics string over USB CDC (NFR-4).
 * Format:
 * {"ts":...,"up_s":...,"temp":...,"hum":...,"light":...,"pm25":...,"relay":...,"sntp":...,"heap":...,"heap_min":...,"i2c_err":...,"uart_err":...}
 */
void telemetry_emit(const sht31_data_t *env,
                     int light_mv,
                     const pms7003_data_t *pm,
                     bool relay_state,
                     bool sntp_synced);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_H
