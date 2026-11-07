#ifndef UART_PMS7003_H
#define UART_PMS7003_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t pm1_0_std;     // PM1.0 concentration (Standard / CF=1, ug/m3)
    uint16_t pm2_5_std;     // PM2.5 concentration (Standard / CF=1, ug/m3)
    uint16_t pm10_std;      // PM10 concentration (Standard / CF=1, ug/m3)
    uint16_t pm1_0_atm;     // PM1.0 concentration (Atmospheric environment, ug/m3)
    uint16_t pm2_5_atm;     // PM2.5 concentration (Atmospheric environment, ug/m3)
    uint16_t pm10_atm;      // PM10 concentration (Atmospheric environment, ug/m3)
    bool valid;             // True if latest frame was parsed and checksum verified
} pms7003_data_t;

/**
 * @brief Initialize UART1, ring buffer, and start the 5-state FSM parsing task.
 *
 * @return ESP_OK on success, or error code.
 */
esp_err_t pms7003_init(void);

/**
 * @brief Get the latest validated PMS7003 PM metrics.
 * Thread-safe copy from FSM parser task.
 *
 * @param data Output data structure.
 * @return ESP_OK if valid data returned, ESP_ERR_NOT_FOUND if no valid frame yet.
 */
esp_err_t pms7003_get_data(pms7003_data_t *data);

/**
 * @brief Get cumulative UART and FSM framing error count for telemetry monitoring.
 */
uint32_t pms7003_get_error_count(void);

#ifdef __cplusplus
}
#endif

#endif // UART_PMS7003_H
