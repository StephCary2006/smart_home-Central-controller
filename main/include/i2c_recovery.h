#ifndef I2C_RECOVERY_H
#define I2C_RECOVERY_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if the I2C bus is stuck (SDA line held LOW by a slave device).
 *
 * @return true if SDA is stuck LOW.
 */
bool i2c_bus_is_stuck(void);

/**
 * @brief Perform a 9-clock SCL bus recovery sequence according to I2C specifications.
 * Steps:
 * 1. Acquire I2C mutex.
 * 2. De-init I2C master driver.
 * 3. Configure SCL as GPIO output and clock 9 manual pulses (5 us HIGH / 5 us LOW).
 * 4. Generate STOP condition (SDA LOW -> HIGH while SCL is HIGH).
 * 5. Re-init I2C master driver.
 * Total recovery time must be < 100 ms (NFR-2).
 *
 * @return ESP_OK if recovered successfully, ESP_FAIL if bus remains locked.
 */
esp_err_t i2c_bus_recover(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_RECOVERY_H
