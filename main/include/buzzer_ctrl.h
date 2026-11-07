#ifndef BUZZER_CTRL_H
#define BUZZER_CTRL_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Active Buzzer GPIO pin (GPIO 19).
 */
esp_err_t buzzer_init(void);

/**
 * @brief Set buzzer output level directly.
 */
void buzzer_set(bool on);

/**
 * @brief Emit acoustic beep pulses asynchronously without blocking caller task.
 *
 * @param count Number of beeps.
 * @param duration_ms Duration of each beep in milliseconds.
 */
void buzzer_beep_async(uint8_t count, uint16_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_CTRL_H
