#include "i2c_manager.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "I2C_MGR";

static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_i2c_mutex = NULL;
static i2c_master_bus_handle_t s_bus_handle = NULL;
static volatile uint32_t s_error_count = 0;

esp_err_t i2c_manager_init(void)
{
    if (s_i2c_mutex == NULL) {
        s_i2c_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_i2c_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create static I2C mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_bus_handle != NULL) {
        return ESP_OK; // Already initialized
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = false, // HSR-2: Enforce external 4.7k physical pull-ups
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        s_error_count++;
        return ret;
    }

    ESP_LOGI(TAG, "I2C master bus initialized on SDA=GPIO%d, SCL=GPIO%d (400kHz)",
             I2C_SDA_PIN, I2C_SCL_PIN);
    return ESP_OK;
}

esp_err_t i2c_manager_deinit(void)
{
    if (s_bus_handle != NULL) {
        esp_err_t ret = i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
        return ret;
    }
    return ESP_OK;
}

void i2c_manager_acquire(void)
{
    if (s_i2c_mutex != NULL) {
        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    }
}

void i2c_manager_release(void)
{
    if (s_i2c_mutex != NULL) {
        xSemaphoreGive(s_i2c_mutex);
    }
}

i2c_master_bus_handle_t i2c_manager_get_bus(void)
{
    return s_bus_handle;
}

uint32_t i2c_manager_get_error_count(void)
{
    return s_error_count;
}

void i2c_manager_increment_error(void)
{
    s_error_count++;
}
