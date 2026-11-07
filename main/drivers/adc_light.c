#include "adc_light.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADC_LIGHT";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_cali_enabled = false;

esp_err_t adc_light_init(void)
{
    if (s_adc_handle != NULL) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = LDR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, LDR_ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attempt eFuse hardware calibration
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = LDR_ADC_UNIT,
        .chan = LDR_ADC_CHANNEL,
        .atten = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle);
    if (ret == ESP_OK) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "ADC curve fitting calibration initialized");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = LDR_ADC_UNIT,
        .atten = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &s_cali_handle);
    if (ret == ESP_OK) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "ADC line fitting calibration initialized");
    }
#endif

    if (!s_cali_enabled) {
        ESP_LOGW(TAG, "eFuse calibration not supported or empty; using default ratio conversion");
    }

    ESP_LOGI(TAG, "LDR ADC initialized on ADC1_CH0 (GPIO 0)");
    return ESP_OK;
}

esp_err_t adc_light_read(int *voltage_mv)
{
    if (voltage_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_adc_handle == NULL) {
        esp_err_t ret = adc_light_init();
        if (ret != ESP_OK) return ret;
    }

    int32_t sum_mv = 0;
    int raw = 0;

    // 16x oversampling to reject power supply ripple and noise (FR-2.3)
    for (int i = 0; i < LDR_OVERSAMPLE_COUNT; i++) {
        esp_err_t ret = adc_oneshot_read(s_adc_handle, LDR_ADC_CHANNEL, &raw);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            return ret;
        }

        int mv = 0;
        if (s_cali_enabled && s_cali_handle != NULL) {
            adc_cali_raw_to_voltage(s_cali_handle, raw, &mv);
        } else {
            // 12-bit ADC full range ~3300 mV (ADC_ATTEN_DB_12)
            mv = (raw * 3300) / 4095;
        }
        sum_mv += mv;
    }

    *voltage_mv = sum_mv / LDR_OVERSAMPLE_COUNT;
    return ESP_OK;
}
