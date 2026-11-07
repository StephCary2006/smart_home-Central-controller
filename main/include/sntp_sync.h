#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "esp_err.h"
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sntp_time_step_cb_t)(void);

/**
 * @brief Initialize Wi-Fi in Station mode and spawn isolated background SNTP task.
 * Network connection drops or DNS failures will NEVER block sensor polling or monotonic timers (FR-1.3).
 *
 * @param wifi_ssid Wi-Fi AP SSID.
 * @param wifi_password Wi-Fi AP Password.
 */
esp_err_t sntp_sync_init(const char *wifi_ssid, const char *wifi_password);

/**
 * @brief Check if POSIX wall-clock has been successfully synchronized at least once.
 */
bool sntp_sync_is_synced(void);

/**
 * @brief Get formatted wall-clock time string (e.g. "%Y-%m-%d %H:%M:%S" or "%H:%M:%S").
 */
esp_err_t sntp_sync_get_time_str(char *buf, size_t buf_len, const char *format);

/**
 * @brief Get current POSIX timestamp (time_t). Returns 0 if never synced.
 */
time_t sntp_sync_get_time(void);

/**
 * @brief Register callback invoked when an NTP step adjustment occurs.
 */
void sntp_sync_register_time_step_cb(sntp_time_step_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif // SNTP_SYNC_H
