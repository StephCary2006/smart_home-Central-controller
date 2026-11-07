#include "relay_ctrl.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "RELAY";
static bool s_relay_state = false;

esp_err_t relay_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure relay GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    relay_set(false); // Default to OFF on boot
    ESP_LOGI(TAG, "Relay initialized on GPIO %d (Opto-isolated)", RELAY_GPIO_PIN);
    return ESP_OK;
}

esp_err_t relay_set(bool on)
{
    s_relay_state = on;
    int level = on ? (RELAY_ACTIVE_LOW ? 0 : 1) : (RELAY_ACTIVE_LOW ? 1 : 0);
    esp_err_t ret = gpio_set_level(RELAY_GPIO_PIN, level);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Relay set to %s (GPIO level %d)", on ? "ON" : "OFF", level);
    }
    return ret;
}

bool relay_get_state(void)
{
    return s_relay_state;
}

esp_err_t relay_toggle(void)
{
    return relay_set(!s_relay_state);
}
