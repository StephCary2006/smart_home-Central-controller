#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared I2C master bus and static arbitration mutex.
 * Must be called once before any I2C peripheral is registered or used.
 * Internal pull-ups are disabled to enforce external physical 4.7k pull-ups (HSR-2).
 *
 * @return ESP_OK on success, or error code from ESP-IDF driver.
 */
esp_err_t i2c_manager_init(void);

/**
 * @brief De-initialize the I2C master bus (used during bus recovery).
 */
esp_err_t i2c_manager_deinit(void);

/**
 * @brief Acquire the shared I2C bus mutex.
 * Blocks current task until the bus becomes available.
 */
void i2c_manager_acquire(void);

/**
 * @brief Release the shared I2C bus mutex.
 */
void i2c_manager_release(void);

/**
 * @brief Get the shared I2C master bus handle.
 *
 * @return i2c_master_bus_handle_t or NULL if uninitialized.
 */
i2c_master_bus_handle_t i2c_manager_get_bus(void);

/**
 * @brief Get the cumulative I2C error count for telemetry monitoring.
 */
uint32_t i2c_manager_get_error_count(void);

/**
 * @brief Increment I2C error counter.
 */
void i2c_manager_increment_error(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_MANAGER_H
