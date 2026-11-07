#ifndef ADC_LIGHT_H
#define ADC_LIGHT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ADC1 Channel 0 (GPIO 0) with eFuse calibration curve.
 *
 * @return ESP_OK on success, or error code.
 */
esp_err_t adc_light_init(void);

/**
 * @brief Read calibrated light sensor voltage in millivolts with 16x oversampling.
 * Rejects high-frequency noise and mains ripple.
 *
 * @param voltage_mv Output pointer receiving voltage in mV (0 to ~3300 mV).
 * @return ESP_OK on success.
 */
esp_err_t adc_light_read(int *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif // ADC_LIGHT_H
