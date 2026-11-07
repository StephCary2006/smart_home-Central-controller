#include "sht31_driver.h"
#include "i2c_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SHT31";
static i2c_master_dev_handle_t s_sht31_handle = NULL;

static uint8_t sht31_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

esp_err_t sht31_init(void)
{
    if (s_sht31_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = i2c_manager_get_bus();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not ready");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT31_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_sht31_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHT31 device to I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SHT31 device registered at I2C address 0x%02X", SHT31_I2C_ADDR);
    return ESP_OK;
}

esp_err_t sht31_read(sht31_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data->valid = false;

    if (s_sht31_handle == NULL) {
        esp_err_t init_ret = sht31_init();
        if (init_ret != ESP_OK) {
            return init_ret;
        }
    }

    i2c_manager_acquire();

    // Send Single Shot Measurement command (High Repeatability, Clock Stretching disabled)
    uint8_t cmd[2] = {0x24, 0x00};
    esp_err_t ret = i2c_master_transmit(s_sht31_handle, cmd, sizeof(cmd), pdMS_TO_TICKS(50));
    if (ret != ESP_OK) {
        i2c_manager_release();
        i2c_manager_increment_error();
        return ret;
    }

    // Wait ~15-20ms for measurement completion
    vTaskDelay(pdMS_TO_TICKS(20));

    // Read 6 bytes: [Temp_MSB, Temp_LSB, Temp_CRC, Hum_MSB, Hum_LSB, Hum_CRC]
    uint8_t rx[6] = {0};
    ret = i2c_master_receive(s_sht31_handle, rx, sizeof(rx), pdMS_TO_TICKS(50));
    i2c_manager_release();

    if (ret != ESP_OK) {
        i2c_manager_increment_error();
        return ret;
    }

    // Validate CRC8
    if (sht31_crc8(&rx[0], 2) != rx[2]) {
        ESP_LOGW(TAG, "Temperature CRC mismatch! Expected: 0x%02X, Got: 0x%02X",
                 sht31_crc8(&rx[0], 2), rx[2]);
        i2c_manager_increment_error();
        return ESP_ERR_INVALID_CRC;
    }

    if (sht31_crc8(&rx[3], 2) != rx[5]) {
        ESP_LOGW(TAG, "Humidity CRC mismatch! Expected: 0x%02X, Got: 0x%02X",
                 sht31_crc8(&rx[3], 2), rx[5]);
        i2c_manager_increment_error();
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_temp = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t raw_hum = ((uint16_t)rx[3] << 8) | rx[4];

    data->temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    data->humidity_pct = 100.0f * ((float)raw_hum / 65535.0f);
    if (data->humidity_pct > 100.0f) data->humidity_pct = 100.0f;
    if (data->humidity_pct < 0.0f) data->humidity_pct = 0.0f;
    data->valid = true;

    return ESP_OK;
}
