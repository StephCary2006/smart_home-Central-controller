#ifndef TIMER_ENGINE_H
#define TIMER_ENGINE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TIMERS 8

typedef void (*timer_callback_t)(void *arg);
typedef int timer_handle_t;

/**
 * @brief Initialize the static monotonic timer engine.
 * Driven exclusively by hardware crystal oscillator (esp_timer / strictly invariant).
 * Completely immune to NTP time adjustments (FR-1.1).
 */
esp_err_t timer_engine_init(void);

/**
 * @brief Create a one-shot monotonic countdown timer.
 */
esp_err_t timer_engine_create_oneshot(const char *name,
                                      uint32_t duration_ms,
                                      timer_callback_t callback,
                                      void *arg,
                                      timer_handle_t *handle);

/**
 * @brief Create a periodic repeating monotonic timer.
 */
esp_err_t timer_engine_create_periodic(const char *name,
                                       uint32_t interval_ms,
                                       timer_callback_t callback,
                                       void *arg,
                                       timer_handle_t *handle);

/**
 * @brief Cancel and free a monotonic timer slot.
 */
esp_err_t timer_engine_cancel(timer_handle_t handle);

/**
 * @brief Get remaining time for an active one-shot timer in milliseconds.
 */
uint32_t timer_engine_remaining_ms(timer_handle_t handle);

/**
 * @brief Get current monotonic hardware time in microseconds.
 */
int64_t timer_engine_now_us(void);

/**
 * @brief Get current monotonic system uptime in seconds.
 */
uint32_t timer_engine_uptime_sec(void);

#ifdef __cplusplus
}
#endif

#endif // TIMER_ENGINE_H
