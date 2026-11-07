#include "buzzer_ctrl.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "BUZZER";

typedef struct {
    uint8_t count;
    uint16_t duration_ms;
} beep_cmd_t;

static StaticQueue_t s_queue_buffer;
static uint8_t s_queue_storage[4 * sizeof(beep_cmd_t)];
static QueueHandle_t s_beep_queue = NULL;

static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[2048];
static TaskHandle_t s_task_handle = NULL;

static void buzzer_worker_task(void *pvParameters)
{
    beep_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_beep_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            for (uint8_t i = 0; i < cmd.count; i++) {
                buzzer_set(true);
                vTaskDelay(pdMS_TO_TICKS(cmd.duration_ms));
                buzzer_set(false);
                vTaskDelay(pdMS_TO_TICKS(cmd.duration_ms));
            }
        }
    }
}

esp_err_t buzzer_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure buzzer GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    buzzer_set(false);

    if (s_beep_queue == NULL) {
        s_beep_queue = xQueueCreateStatic(4, sizeof(beep_cmd_t), s_queue_storage, &s_queue_buffer);
    }

    if (s_task_handle == NULL) {
        s_task_handle = xTaskCreateStatic(
            buzzer_worker_task,
            "buzzer_task",
            2048,
            NULL,
            3,
            s_task_stack,
            &s_task_tcb
        );
    }

    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d (Active buzzer)", BUZZER_GPIO_PIN);
    return ESP_OK;
}

void buzzer_set(bool on)
{
    gpio_set_level(BUZZER_GPIO_PIN, on ? 1 : 0);
}

void buzzer_beep_async(uint8_t count, uint16_t duration_ms)
{
    if (s_beep_queue != NULL && count > 0) {
        beep_cmd_t cmd = {.count = count, .duration_ms = duration_ms};
        xQueueSend(s_beep_queue, &cmd, 0); // Non-blocking send
    }
}
