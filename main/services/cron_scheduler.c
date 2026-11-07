#include "cron_scheduler.h"
#include "sntp_sync.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "CRON";

typedef struct {
    bool in_use;
    char name[16];
    cron_schedule_t schedule;
    cron_callback_t callback;
    void *arg;
    time_t last_fired_sec;
} cron_job_t;

static cron_job_t s_jobs[MAX_CRON_JOBS];
static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_cron_mutex = NULL;

static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[TASK_STACK_CRON];
static TaskHandle_t s_task_handle = NULL;

static bool cron_matches(const struct tm *now, const cron_schedule_t *sched)
{
    if (sched->hour != -1 && sched->hour != now->tm_hour) return false;
    if (sched->minute != -1 && sched->minute != now->tm_min) return false;
    if (sched->second != -1 && sched->second != now->tm_sec) return false;
    if (sched->weekdays != 0x7F) {
        if (!(sched->weekdays & (1 << now->tm_wday))) return false;
    }
    return true;
}

static void cron_worker_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    ESP_LOGI(TAG, "Cron scheduler task started (1Hz cadence)");

    while (1) {
        // Run with deterministic 1000ms cadence (NFR-1)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));

        if (!sntp_sync_is_synced()) {
            continue; // Wait until initial NTP synchronization
        }

        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        if (xSemaphoreTake(s_cron_mutex, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < MAX_CRON_JOBS; i++) {
                if (!s_jobs[i].in_use) continue;

                // Match schedule and ensure it hasn't fired in this exact second
                if (now != s_jobs[i].last_fired_sec && cron_matches(&tm_now, &s_jobs[i].schedule)) {
                    s_jobs[i].last_fired_sec = now;
                    cron_callback_t cb = s_jobs[i].callback;
                    void *arg = s_jobs[i].arg;
                    const char *name = s_jobs[i].name;

                    ESP_LOGI(TAG, "Triggering cron job '%s' at %02d:%02d:%02d",
                             name, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

                    if (cb != NULL) {
                        cb(arg);
                    }
                }
            }
            xSemaphoreGive(s_cron_mutex);
        }
    }
}

void cron_scheduler_on_time_step(void)
{
    ESP_LOGI(TAG, "NTP phase jump / step detected: recalculating cron triggers");
    if (s_cron_mutex && xSemaphoreTake(s_cron_mutex, portMAX_DELAY) == pdTRUE) {
        time_t now = time(NULL);
        for (int i = 0; i < MAX_CRON_JOBS; i++) {
            if (s_jobs[i].in_use) {
                // Prevent immediate double-firing if time stepped across trigger
                s_jobs[i].last_fired_sec = now;
            }
        }
        xSemaphoreGive(s_cron_mutex);
    }
}

esp_err_t cron_scheduler_init(void)
{
    if (s_cron_mutex == NULL) {
        s_cron_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_cron_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_cron_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_CRON_JOBS; i++) {
            s_jobs[i].in_use = false;
            s_jobs[i].last_fired_sec = 0;
        }
        xSemaphoreGive(s_cron_mutex);
    }

    // Register callback for NTP step compensation
    sntp_sync_register_time_step_cb(cron_scheduler_on_time_step);

    if (s_task_handle == NULL) {
        s_task_handle = xTaskCreateStatic(
            cron_worker_task,
            "cron_task",
            TASK_STACK_CRON,
            NULL,
            TASK_PRIORITY_CRON,
            s_task_stack,
            &s_task_tcb
        );
        if (s_task_handle == NULL) return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Cron scheduler initialized (max %d jobs, static memory)", MAX_CRON_JOBS);
    return ESP_OK;
}

esp_err_t cron_scheduler_add(const char *name,
                              const cron_schedule_t *schedule,
                              cron_callback_t callback,
                              void *arg,
                              cron_handle_t *handle)
{
    if (schedule == NULL || callback == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_cron_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int free_idx = -1;
    for (int i = 0; i < MAX_CRON_JOBS; i++) {
        if (!s_jobs[i].in_use) {
            free_idx = i;
            break;
        }
    }

    if (free_idx == -1) {
        xSemaphoreGive(s_cron_mutex);
        ESP_LOGE(TAG, "No free cron job slots");
        return ESP_ERR_NO_MEM;
    }

    s_jobs[free_idx].in_use = true;
    s_jobs[free_idx].schedule = *schedule;
    s_jobs[free_idx].callback = callback;
    s_jobs[free_idx].arg = arg;
    s_jobs[free_idx].last_fired_sec = 0;
    strncpy(s_jobs[free_idx].name, name ? name : "cron", sizeof(s_jobs[free_idx].name) - 1);

    xSemaphoreGive(s_cron_mutex);
    *handle = free_idx;
    return ESP_OK;
}

esp_err_t cron_scheduler_remove(cron_handle_t handle)
{
    if (handle < 0 || handle >= MAX_CRON_JOBS) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_cron_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_jobs[handle].in_use = false;
    xSemaphoreGive(s_cron_mutex);
    return ESP_OK;
}
