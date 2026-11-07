#ifndef SHT31_DRIVER_H
#define SHT31_DRIVER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature_c;    // Temperature in Celsius
    float humidity_pct;     // Relative humidity in %
    bool valid;             // True if reading and CRC8 checksum succeeded
} sht31_data_t;

/**
 * @brief Initialize the SHT31 temperature and humidity sensor on the shared I2C bus.
 *
 * @return ESP_OK on success, or error code.
 */
esp_err_t sht31_init(void);

/**
 * @brief Read temperature and humidity from SHT31.
 * Thread-safe: automatically acquires and releases I2C mutex.
 * Validates CRC8 for both temperature and humidity packets.
 *
 * @param data Output data structure.
 * @return ESP_OK on valid read, ESP_FAIL on bus or CRC error.
 */
esp_err_t sht31_read(sht31_data_t *data);

#ifdef __cplusplus
}
#endif

#endif // SHT31_DRIVER_H
