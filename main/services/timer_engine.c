#include "timer_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "TIMER_ENGINE";

typedef struct {
    bool in_use;
    bool periodic;
    uint32_t duration_ms;
    int64_t expire_time_us;
    timer_callback_t callback;
    void *arg;
    esp_timer_handle_t esp_timer;
    char name[16];
} timer_slot_t;

static timer_slot_t s_timers[MAX_TIMERS];
static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_timer_mutex = NULL;

static void internal_timer_cb(void *arg)
{
    int index = (int)(intptr_t)arg;
    if (index < 0 || index >= MAX_TIMERS) return;

    timer_callback_t user_cb = NULL;
    void *user_arg = NULL;
    bool is_periodic = false;
    uint32_t duration_ms = 0;

    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_timers[index].in_use) {
            user_cb = s_timers[index].callback;
            user_arg = s_timers[index].arg;
            is_periodic = s_timers[index].periodic;
            duration_ms = s_timers[index].duration_ms;

            if (is_periodic) {
                s_timers[index].expire_time_us = esp_timer_get_time() + ((int64_t)duration_ms * 1000);
            } else {
                s_timers[index].in_use = false;
            }
        }
        xSemaphoreGive(s_timer_mutex);
    }

    if (user_cb != NULL) {
        user_cb(user_arg);
    }
}

esp_err_t timer_engine_init(void)
{
    if (s_timer_mutex == NULL) {
        s_timer_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_timer_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_TIMERS; i++) {
            s_timers[i].in_use = false;
            s_timers[i].esp_timer = NULL;
        }
        xSemaphoreGive(s_timer_mutex);
    }

    ESP_LOGI(TAG, "Monotonic Timer Engine initialized (max %d slots, NFR-3 static allocation)", MAX_TIMERS);
    return ESP_OK;
}

esp_err_t timer_engine_create_oneshot(const char *name,
                                      uint32_t duration_ms,
                                      timer_callback_t callback,
                                      void *arg,
                                      timer_handle_t *handle)
{
    if (callback == NULL || handle == NULL || duration_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int free_idx = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].in_use) {
            free_idx = i;
            break;
        }
    }

    if (free_idx == -1) {
        xSemaphoreGive(s_timer_mutex);
        ESP_LOGE(TAG, "No free monotonic timer slots");
        return ESP_ERR_NO_MEM;
    }

    // Clean previous timer instance if any
    if (s_timers[free_idx].esp_timer != NULL) {
        esp_timer_stop(s_timers[free_idx].esp_timer);
        esp_timer_delete(s_timers[free_idx].esp_timer);
        s_timers[free_idx].esp_timer = NULL;
    }

    esp_timer_create_args_t timer_args = {
        .callback = internal_timer_cb,
        .arg = (void *)(intptr_t)free_idx,
        .dispatch_method = ESP_TIMER_TASK,
        .name = name ? name : "oneshot",
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_timers[free_idx].esp_timer);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_timer_mutex);
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_timers[free_idx].in_use = true;
    s_timers[free_idx].periodic = false;
    s_timers[free_idx].duration_ms = duration_ms;
    s_timers[free_idx].expire_time_us = esp_timer_get_time() + ((int64_t)duration_ms * 1000);
    s_timers[free_idx].callback = callback;
    s_timers[free_idx].arg = arg;
    strncpy(s_timers[free_idx].name, name ? name : "oneshot", sizeof(s_timers[free_idx].name) - 1);

    ret = esp_timer_start_once(s_timers[free_idx].esp_timer, (uint64_t)duration_ms * 1000);
    xSemaphoreGive(s_timer_mutex);

    *handle = free_idx;
    return ret;
}

esp_err_t timer_engine_create_periodic(const char *name,
                                       uint32_t interval_ms,
                                       timer_callback_t callback,
                                       void *arg,
                                       timer_handle_t *handle)
{
    if (callback == NULL || handle == NULL || interval_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int free_idx = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].in_use) {
            free_idx = i;
            break;
        }
    }

    if (free_idx == -1) {
        xSemaphoreGive(s_timer_mutex);
        ESP_LOGE(TAG, "No free monotonic timer slots");
        return ESP_ERR_NO_MEM;
    }

    if (s_timers[free_idx].esp_timer != NULL) {
        esp_timer_stop(s_timers[free_idx].esp_timer);
        esp_timer_delete(s_timers[free_idx].esp_timer);
        s_timers[free_idx].esp_timer = NULL;
    }

    esp_timer_create_args_t timer_args = {
        .callback = internal_timer_cb,
        .arg = (void *)(intptr_t)free_idx,
        .dispatch_method = ESP_TIMER_TASK,
        .name = name ? name : "periodic",
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_timers[free_idx].esp_timer);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_timer_mutex);
        return ret;
    }

    s_timers[free_idx].in_use = true;
    s_timers[free_idx].periodic = true;
    s_timers[free_idx].duration_ms = interval_ms;
    s_timers[free_idx].expire_time_us = esp_timer_get_time() + ((int64_t)interval_ms * 1000);
    s_timers[free_idx].callback = callback;
    s_timers[free_idx].arg = arg;
    strncpy(s_timers[free_idx].name, name ? name : "periodic", sizeof(s_timers[free_idx].name) - 1);

    ret = esp_timer_start_periodic(s_timers[free_idx].esp_timer, (uint64_t)interval_ms * 1000);
    xSemaphoreGive(s_timer_mutex);

    *handle = free_idx;
    return ret;
}

esp_err_t timer_engine_cancel(timer_handle_t handle)
{
    if (handle < 0 || handle >= MAX_TIMERS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_timers[handle].in_use) {
        if (s_timers[handle].esp_timer != NULL) {
            esp_timer_stop(s_timers[handle].esp_timer);
            esp_timer_delete(s_timers[handle].esp_timer);
            s_timers[handle].esp_timer = NULL;
        }
        s_timers[handle].in_use = false;
    }

    xSemaphoreGive(s_timer_mutex);
    return ESP_OK;
}

uint32_t timer_engine_remaining_ms(timer_handle_t handle)
{
    if (handle < 0 || handle >= MAX_TIMERS) return 0;

    uint32_t remaining = 0;
    if (xSemaphoreTake(s_timer_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_timers[handle].in_use) {
            int64_t now = esp_timer_get_time();
            int64_t diff = s_timers[handle].expire_time_us - now;
            if (diff > 0) {
                remaining = (uint32_t)(diff / 1000);
            }
        }
        xSemaphoreGive(s_timer_mutex);
    }
    return remaining;
}

int64_t timer_engine_now_us(void)
{
    return esp_timer_get_time();
}

uint32_t timer_engine_uptime_sec(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}
