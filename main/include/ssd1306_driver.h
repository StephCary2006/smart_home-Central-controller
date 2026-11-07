#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8) // 8 rows of text

/**
 * @brief Initialize the SSD1306 OLED display on the shared I2C bus.
 *
 * @return ESP_OK on success, or error code.
 */
esp_err_t ssd1306_init(void);

/**
 * @brief Check if the OLED display was detected and initialized properly.
 */
bool ssd1306_is_ready(void);

/**
 * @brief Clear the internal display buffer.
 * Call ssd1306_update() to push changes to screen.
 */
esp_err_t ssd1306_clear(void);

/**
 * @brief Write an ASCII text line to a row (0 - 7).
 *
 * @param row Row index (0 to 7).
 * @param text Null-terminated ASCII string (up to 16 characters for 8x8 font).
 */
esp_err_t ssd1306_write_line(uint8_t row, const char *text);

/**
 * @brief Push the internal framebuffer to the SSD1306 controller via I2C.
 * Thread-safe: acquires I2C mutex.
 */
esp_err_t ssd1306_update(void);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_DRIVER_H
