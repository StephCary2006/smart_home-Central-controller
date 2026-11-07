#ifndef CRON_SCHEDULER_H
#define CRON_SCHEDULER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CRON_JOBS 8

typedef void (*cron_callback_t)(void *arg);
typedef int cron_handle_t;

typedef struct {
    int8_t hour;      // 0 - 23, or -1 for every hour
    int8_t minute;    // 0 - 59, or -1 for every minute
    int8_t second;    // 0 - 59, or -1 for every second
    uint8_t weekdays; // Bitmask: bit0=Sun, bit1=Mon, ..., bit6=Sat. 0x7F = every day
} cron_schedule_t;

/**
 * @brief Initialize the static wall-clock cron scheduler task (FR-1.2).
 */
esp_err_t cron_scheduler_init(void);

/**
 * @brief Add a wall-clock cron event.
 */
esp_err_t cron_scheduler_add(const char *name,
                              const cron_schedule_t *schedule,
                              cron_callback_t callback,
                              void *arg,
                              cron_handle_t *handle);

/**
 * @brief Remove a registered cron job.
 */
esp_err_t cron_scheduler_remove(cron_handle_t handle);

/**
 * @brief Notification hook invoked when an NTP step adjustment happens.
 * Recomputes all upcoming trigger deadlines to avoid double-firing or skipped triggers.
 */
void cron_scheduler_on_time_step(void);

#ifdef __cplusplus
}
#endif

#endif // CRON_SCHEDULER_H
