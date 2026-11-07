#include "uart_pms7003.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "PMS7003";

typedef enum {
    FSM_IDLE = 0,
    FSM_GOT_42,
    FSM_GOT_4D,
    FSM_PAYLOAD,
    FSM_CHECKSUM,
} pms_fsm_state_t;

static pms7003_data_t s_latest_data = {0};
static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_data_mutex = NULL;
static volatile uint32_t s_uart_errors = 0;

static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[TASK_STACK_UART];
static TaskHandle_t s_task_handle = NULL;

static void pms7003_task(void *pvParameters)
{
    uint8_t rx_byte = 0;
    pms_fsm_state_t state = FSM_IDLE;
    uint8_t frame_buf[32] = {0};
    uint8_t byte_idx = 0;
    uint16_t frame_len = 0;

    ESP_LOGI(TAG, "PMS7003 UART FSM parser task started");

    while (1) {
        // Read 1 byte at a time from UART ring buffer with 100ms timeout
        int len = uart_read_bytes(PMS_UART_PORT, &rx_byte, 1, pdMS_TO_TICKS(100));
        if (len <= 0) {
            continue;
        }

        switch (state) {
            case FSM_IDLE:
                if (rx_byte == 0x42) {
                    frame_buf[0] = 0x42;
                    state = FSM_GOT_42;
                }
                break;

            case FSM_GOT_42:
                if (rx_byte == 0x4D) {
                    frame_buf[1] = 0x4D;
                    byte_idx = 2;
                    state = FSM_GOT_4D;
                } else if (rx_byte == 0x42) {
                    // Stray 0x42, stay in GOT_42
                    state = FSM_GOT_42;
                } else {
                    state = FSM_IDLE;
                }
                break;

            case FSM_GOT_4D:
                frame_buf[byte_idx++] = rx_byte;
                if (byte_idx == 4) {
                    frame_len = ((uint16_t)frame_buf[2] << 8) | frame_buf[3];
                    if (frame_len == 28) {
                        state = FSM_PAYLOAD;
                    } else {
                        // Length invalid for PMS7003 standard frame
                        s_uart_errors++;
                        state = FSM_IDLE;
                    }
                }
                break;

            case FSM_PAYLOAD:
                frame_buf[byte_idx++] = rx_byte;
                if (byte_idx == 30) {
                    // All 26 payload bytes collected (bytes 4 to 29)
                    state = FSM_CHECKSUM;
                }
                break;

            case FSM_CHECKSUM:
                frame_buf[byte_idx++] = rx_byte;
                if (byte_idx == 32) {
                    // Frame complete: validate 16-bit checksum
                    uint16_t calculated_sum = 0;
                    for (int i = 0; i < 30; i++) {
                        calculated_sum += frame_buf[i];
                    }
                    uint16_t expected_sum = ((uint16_t)frame_buf[30] << 8) | frame_buf[31];

                    if (calculated_sum == expected_sum) {
                        // Checksum valid! Unpack metrics
                        if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
                            s_latest_data.pm1_0_std = ((uint16_t)frame_buf[4] << 8) | frame_buf[5];
                            s_latest_data.pm2_5_std = ((uint16_t)frame_buf[6] << 8) | frame_buf[7];
                            s_latest_data.pm10_std  = ((uint16_t)frame_buf[8] << 8) | frame_buf[9];
                            s_latest_data.pm1_0_atm = ((uint16_t)frame_buf[10] << 8) | frame_buf[11];
                            s_latest_data.pm2_5_atm = ((uint16_t)frame_buf[12] << 8) | frame_buf[13];
                            s_latest_data.pm10_atm  = ((uint16_t)frame_buf[14] << 8) | frame_buf[15];
                            s_latest_data.valid = true;
                            xSemaphoreGive(s_data_mutex);
                        }
                    } else {
                        s_uart_errors++;
                        ESP_LOGW(TAG, "PMS7003 checksum mismatch: calc=0x%04X, exp=0x%04X",
                                 calculated_sum, expected_sum);
                    }
                    // Reset to wait for next frame
                    byte_idx = 0;
                    state = FSM_IDLE;
                }
                break;
        }
    }
}

esp_err_t pms7003_init(void)
{
    if (s_task_handle != NULL) {
        return ESP_OK;
    }

    if (s_data_mutex == NULL) {
        s_data_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_data_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    uart_config_t uart_config = {
        .baud_rate = PMS_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(PMS_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(PMS_UART_PORT, PMS_UART_TX_PIN, PMS_UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install UART driver with Ring Buffer (PMS_UART_BUF_SIZE)
    ret = uart_driver_install(PMS_UART_PORT, PMS_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create parser task statically (NFR-3)
    s_task_handle = xTaskCreateStatic(
        pms7003_task,
        "pms_fsm",
        TASK_STACK_UART,
        NULL,
        TASK_PRIORITY_UART,
        s_task_stack,
        &s_task_tcb
    );

    if (s_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create static PMS7003 task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "PMS7003 UART driver & FSM parser initialized on RX=GPIO%d, TX=GPIO%d",
             PMS_UART_RX_PIN, PMS_UART_TX_PIN);
    return ESP_OK;
}

esp_err_t pms7003_get_data(pms7003_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    if (s_data_mutex == NULL) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        *data = s_latest_data;
        xSemaphoreGive(s_data_mutex);
        return data->valid ? ESP_OK : ESP_ERR_NOT_FOUND;
    }
    return ESP_ERR_TIMEOUT;
}

uint32_t pms7003_get_error_count(void)
{
    return s_uart_errors;
}
