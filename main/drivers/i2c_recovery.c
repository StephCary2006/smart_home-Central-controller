#include "i2c_recovery.h"
#include "i2c_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"

static const char *TAG = "I2C_RECOVERY";

bool i2c_bus_is_stuck(void)
{
    // Configure SDA as input with no pullups to sample physical level
    gpio_set_direction(I2C_SDA_PIN, GPIO_MODE_INPUT);
    return (gpio_get_level(I2C_SDA_PIN) == 0);
}

esp_err_t i2c_bus_recover(void)
{
    ESP_LOGW(TAG, "Initiating 9-pulse I2C bus recovery sequence...");
    i2c_manager_increment_error();

    i2c_manager_acquire();

    // 1. De-initialize I2C bus driver
    i2c_manager_deinit();

    // 2. Configure SCL as Output Open-Drain and SDA as Input
    gpio_config_t scl_conf = {
        .pin_bit_mask = (1ULL << I2C_SCL_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&scl_conf);

    gpio_config_t sda_conf = {
        .pin_bit_mask = (1ULL << I2C_SDA_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sda_conf);

    // Initial state: SDA high, SCL high
    gpio_set_level(I2C_SDA_PIN, 1);
    gpio_set_level(I2C_SCL_PIN, 1);
    esp_rom_delay_us(10);

    // 3. Clock 9 pulses on SCL to clear slave shift register
    for (int i = 0; i < 9; i++) {
        gpio_set_level(I2C_SCL_PIN, 0);
        esp_rom_delay_us(5);
        gpio_set_level(I2C_SCL_PIN, 1);
        esp_rom_delay_us(5);

        // Check if SDA released early
        if (gpio_get_level(I2C_SDA_PIN) == 1) {
            ESP_LOGD(TAG, "SDA released at clock pulse %d", i + 1);
        }
    }

    // 4. Generate I2C STOP condition:
    // SCL LOW -> SDA LOW -> SCL HIGH -> SDA HIGH
    gpio_set_level(I2C_SCL_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(I2C_SDA_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(I2C_SCL_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(I2C_SDA_PIN, 1);
    esp_rom_delay_us(10);

    // 5. Verify bus is freed
    bool still_stuck = (gpio_get_level(I2C_SDA_PIN) == 0);

    // 6. Re-init I2C master driver
    esp_err_t ret = i2c_manager_init();

    i2c_manager_release();

    if (still_stuck || ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus recovery failed! SDA still LOW");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C bus successfully recovered in < 100ms (NFR-2)");
    return ESP_OK;
}
