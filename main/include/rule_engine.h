#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RULES 16

typedef enum {
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_LIGHT,
    SENSOR_PM25,
} sensor_type_t;

typedef enum {
    CMP_GREATER_THAN,
    CMP_LESS_THAN,
    CMP_EQUAL,
} cmp_operator_t;

typedef enum {
    ACTION_RELAY_ON,
    ACTION_RELAY_OFF,
    ACTION_RELAY_ON_TIMED,   // Energize relay for N seconds using monotonic timer, then auto shutoff
    ACTION_BUZZER_BEEP,      // Sound acoustic buzzer alarm
    ACTION_BUZZER_OFF,
} action_type_t;

typedef struct {
    const char *name;
    sensor_type_t sensor;
    cmp_operator_t op;
    float threshold;
    action_type_t action;
    uint32_t action_param;      // Duration in seconds (for ACTION_RELAY_ON_TIMED) or count (for BEEP)
    uint32_t cooldown_sec;      // Cooldown in seconds to prevent chatter
    bool enabled;
} rule_def_t;

/**
 * @brief Initialize static rule engine.
 */
esp_err_t rule_engine_init(void);

/**
 * @brief Register a rule definition.
 */
esp_err_t rule_engine_add(const rule_def_t *rule);

/**
 * @brief Evaluate all enabled rules against current sensor telemetry.
 * Uses monotonic timer timestamps for jitter-free cooldown enforcement.
 */
void rule_engine_evaluate(float temperature, float humidity,
                           int light_mv, uint16_t pm25);

#ifdef __cplusplus
}
#endif

#endif // RULE_ENGINE_H
