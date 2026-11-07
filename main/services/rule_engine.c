#include "rule_engine.h"
#include "relay_ctrl.h"
#include "buzzer_ctrl.h"
#include "timer_engine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "RULE_ENGINE";

typedef struct {
    rule_def_t def;
    int64_t last_triggered_us;
    timer_handle_t timer_handle;
    bool in_use;
} internal_rule_t;

static internal_rule_t s_rules[MAX_RULES];
static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_rule_mutex = NULL;

static void relay_auto_off_cb(void *arg)
{
    ESP_LOGI(TAG, "Timed relay duration expired: turning OFF relay");
    relay_set(false);
    buzzer_beep_async(1, 100);
}

static float get_sensor_value(sensor_type_t sensor, float temp, float hum,
                              int light_mv, uint16_t pm25)
{
    switch (sensor) {
        case SENSOR_TEMPERATURE: return temp;
        case SENSOR_HUMIDITY:    return hum;
        case SENSOR_LIGHT:       return (float)light_mv;
        case SENSOR_PM25:        return (float)pm25;
        default:                 return 0.0f;
    }
}

static bool evaluate_condition(float val, cmp_operator_t op, float threshold)
{
    switch (op) {
        case CMP_GREATER_THAN: return (val > threshold);
        case CMP_LESS_THAN:    return (val < threshold);
        case CMP_EQUAL:        return (val == threshold);
        default:               return false;
    }
}

esp_err_t rule_engine_init(void)
{
    if (s_rule_mutex == NULL) {
        s_rule_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        if (s_rule_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_rule_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_RULES; i++) {
            s_rules[i].in_use = false;
            s_rules[i].last_triggered_us = 0;
            s_rules[i].timer_handle = -1;
        }
        xSemaphoreGive(s_rule_mutex);
    }

    ESP_LOGI(TAG, "Rule Engine initialized (max %d rules, static allocation)", MAX_RULES);
    return ESP_OK;
}

esp_err_t rule_engine_add(const rule_def_t *rule)
{
    if (rule == NULL) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_rule_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int free_idx = -1;
    for (int i = 0; i < MAX_RULES; i++) {
        if (!s_rules[i].in_use) {
            free_idx = i;
            break;
        }
    }

    if (free_idx == -1) {
        xSemaphoreGive(s_rule_mutex);
        ESP_LOGE(TAG, "No free rule slots in rule engine");
        return ESP_ERR_NO_MEM;
    }

    s_rules[free_idx].def = *rule;
    s_rules[free_idx].last_triggered_us = 0;
    s_rules[free_idx].timer_handle = -1;
    s_rules[free_idx].in_use = true;

    xSemaphoreGive(s_rule_mutex);
    ESP_LOGI(TAG, "Rule '%s' registered successfully", rule->name ? rule->name : "unnamed");
    return ESP_OK;
}

void rule_engine_evaluate(float temperature, float humidity,
                           int light_mv, uint16_t pm25)
{
    if (s_rule_mutex == NULL) return;

    int64_t now_us = timer_engine_now_us();

    if (xSemaphoreTake(s_rule_mutex, 0) == pdTRUE) {
        for (int i = 0; i < MAX_RULES; i++) {
            if (!s_rules[i].in_use || !s_rules[i].def.enabled) continue;

            float val = get_sensor_value(s_rules[i].def.sensor, temperature, humidity, light_mv, pm25);
            bool triggered = evaluate_condition(val, s_rules[i].def.op, s_rules[i].def.threshold);

            if (triggered) {
                // Check monotonic cooldown
                int64_t elapsed_us = now_us - s_rules[i].last_triggered_us;
                int64_t cooldown_us = (int64_t)s_rules[i].def.cooldown_sec * 1000000ULL;

                if (s_rules[i].last_triggered_us == 0 || elapsed_us >= cooldown_us) {
                    s_rules[i].last_triggered_us = now_us;
                    ESP_LOGI(TAG, "Rule '%s' triggered! (val=%.1f, th=%.1f)",
                             s_rules[i].def.name, val, s_rules[i].def.threshold);

                    switch (s_rules[i].def.action) {
                        case ACTION_RELAY_ON:
                            relay_set(true);
                            break;
                        case ACTION_RELAY_OFF:
                            relay_set(false);
                            break;
                        case ACTION_RELAY_ON_TIMED: {
                            relay_set(true);
                            uint32_t duration_ms = s_rules[i].def.action_param * 1000;
                            if (duration_ms == 0) duration_ms = 10000; // Default 10s
                            timer_engine_cancel(s_rules[i].timer_handle);
                            timer_engine_create_oneshot("relay_timed", duration_ms,
                                                        relay_auto_off_cb, NULL,
                                                        &s_rules[i].timer_handle);
                            break;
                        }
                        case ACTION_BUZZER_BEEP: {
                            uint8_t count = (uint8_t)s_rules[i].def.action_param;
                            if (count == 0) count = 2;
                            buzzer_beep_async(count, 120);
                            break;
                        }
                        case ACTION_BUZZER_OFF:
                            buzzer_set(false);
                            break;
                    }
                }
            }
        }
        xSemaphoreGive(s_rule_mutex);
    }
}
