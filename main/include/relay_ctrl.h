#ifndef RELAY_CTRL_H
#define RELAY_CTRL_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Opto-isolated Relay GPIO pin (GPIO 18).
 */
esp_err_t relay_init(void);

/**
 * @brief Set relay state (ON / OFF).
 * Respects RELAY_ACTIVE_LOW setting.
 */
esp_err_t relay_set(bool on);

/**
 * @brief Get current logical state of the relay.
 *
 * @return true if energised / active.
 */
bool relay_get_state(void);

/**
 * @brief Toggle current relay state.
 */
esp_err_t relay_toggle(void);

#ifdef __cplusplus
}
#endif

#endif // RELAY_CTRL_H
