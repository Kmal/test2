#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RULE_CONFIG_SCHEMA_VERSION 1u
#define RULE_MAX_RULES 8u
#define RULE_MAX_ACTIONS_PER_RULE 3u
#define RULE_NAME_MAX 32u
#define RULE_SOURCE_KEY_MAX 32u
#define RULE_HTTP_URL_MAX 128u
#define RULE_HTTP_AUTH_MAX 96u
#define RULE_ERROR_MAX 96u
#define RULE_MIN_COOLDOWN_MS 100u
#define RULE_MAX_COOLDOWN_MS 86400000u
#define RULE_MAX_SUSTAIN_MS 3600000u
#define RULE_MAX_HTTP_TIMEOUT_MS 10000u
#define RULE_SPEAKER_MIN_FREQUENCY_HZ 20u
#define RULE_SPEAKER_MAX_FREQUENCY_HZ 8000u
#define RULE_SPEAKER_MIN_DURATION_MS 10u
#define RULE_SPEAKER_MAX_DURATION_MS 5000u
#define RULE_SPEAKER_MAX_VOLUME_PERCENT 74u
#define RULE_SPEAKER_MAX_TIMEOUT_MS 1000u
#define RULE_GPIO_UNUSED_PIN (-1)
#define RULE_GPIO_MIN_DEBOUNCE_MS 5u
#define RULE_GPIO_MAX_DEBOUNCE_MS 5000u

typedef enum {
    RULE_VALUE_BOOL = 0,
    RULE_VALUE_I32,
} rule_value_kind_t;

typedef struct {
    rule_value_kind_t kind;
    union {
        bool bool_value;
        int32_t i32_value;
    } as;
} rule_value_t;

typedef enum {
    RULE_SOURCE_INVALID = 0,
    RULE_SOURCE_SOUND_RMS_DBFS,
    RULE_SOURCE_SOUND_PEAK_DBFS,
    RULE_SOURCE_SOUND_CLIPPED,
    RULE_SOURCE_KEY1_SHORT,
    RULE_SOURCE_KEY2_SHORT,
    RULE_SOURCE_BLE_CONNECTED,
    RULE_SOURCE_WIFI_CONNECTED,
    RULE_SOURCE_BATTERY_PERCENT,
    RULE_SOURCE_POWER_USB_PRESENT,
    RULE_SOURCE_BMI270_MOTION,
    RULE_SOURCE_HAT_PIR_MOTION,
    RULE_SOURCE_HAT_ENV3_TEMPERATURE_C,
    RULE_SOURCE_HAT_ENV3_HUMIDITY_RH,
    RULE_SOURCE_HAT_ENV3_PRESSURE_HPA,
    RULE_SOURCE_HAT_LIGHT_LUX,
    RULE_SOURCE_HAT_TOF_DISTANCE_MM,
    RULE_SOURCE_HAT_NCIR_TEMPERATURE_C,
    RULE_SOURCE_HAT_THERMAL_AVG_C,
    RULE_SOURCE_HAT_THERMAL_MAX_C,
    RULE_SOURCE_HAT_HEART_RATE_BPM,
    RULE_SOURCE_HAT_ADC_VOLTAGE_MV,
    RULE_SOURCE_GPIO_DIGITAL,
    RULE_SOURCE_GPIO_EDGE,
    RULE_SOURCE_GPIO_PULSE_COUNT,
    RULE_SOURCE_GPIO_FREQUENCY_HZ,
    RULE_SOURCE_ADC_VOLTAGE_MV,
    RULE_SOURCE_COUNT,
} rule_source_t;

typedef enum {
    RULE_COMPARATOR_INVALID = 0,
    RULE_COMPARATOR_EQ,
    RULE_COMPARATOR_NE,
    RULE_COMPARATOR_GT,
    RULE_COMPARATOR_GTE,
    RULE_COMPARATOR_LT,
    RULE_COMPARATOR_LTE,
    RULE_COMPARATOR_COUNT,
} rule_comparator_t;

typedef enum {
    RULE_ACTION_INVALID = 0,
    RULE_ACTION_BLE_MESSAGE,
    RULE_ACTION_HTTP_POST,
    RULE_ACTION_HAT_OPERATION,
    RULE_ACTION_IR_SEND,
    RULE_ACTION_LOCAL_UI,
    RULE_ACTION_SPEAKER_TONE,
    RULE_ACTION_COUNT,
} rule_action_type_t;

typedef enum {
    RULE_IR_PROTOCOL_NEC = 0,
    RULE_IR_PROTOCOL_COUNT,
} rule_ir_protocol_t;

typedef enum {
    RULE_HAT_OPERATION_NONE = 0,
    RULE_HAT_OPERATION_RELAY_SET,
    RULE_HAT_OPERATION_LED_SET,
    RULE_HAT_OPERATION_BUZZER_TONE,
    RULE_HAT_OPERATION_SERVO_SET,
    RULE_HAT_OPERATION_DAC_SET,
    RULE_HAT_OPERATION_COUNT,
} rule_hat_operation_t;

typedef enum {
    RULE_GPIO_PROFILE_NONE = 0,
    RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW,
    RULE_GPIO_PROFILE_RISING_EDGE,
    RULE_GPIO_PROFILE_FALLING_EDGE,
    RULE_GPIO_PROFILE_DEBOUNCED_CONTACT,
    RULE_GPIO_PROFILE_PULSE_COUNT,
    RULE_GPIO_PROFILE_FREQUENCY,
    RULE_GPIO_PROFILE_COUNT,
} rule_gpio_profile_t;

typedef struct {
    int pin;
    rule_gpio_profile_t profile;
    bool active_low;
    uint32_t debounce_ms;
} rule_gpio_config_t;

typedef struct {
    rule_source_t source;
    char source_key[RULE_SOURCE_KEY_MAX];
    rule_value_t value;
    uint32_t uptime_ms;
    uint32_t sequence;
} trigger_fact_t;

typedef struct {
    rule_source_t source;
    char source_key[RULE_SOURCE_KEY_MAX];
    rule_comparator_t comparator;
    rule_value_t threshold;
    uint32_t sustain_ms;
    rule_gpio_config_t gpio;
} rule_condition_t;

typedef struct {
    rule_action_type_t type;
    char http_url[RULE_HTTP_URL_MAX];
    char http_bearer_token[RULE_HTTP_AUTH_MAX];
    uint32_t timeout_ms;
    rule_hat_operation_t hat_operation;
    rule_ir_protocol_t ir_protocol;
    uint32_t ir_carrier_hz;
    uint16_t ir_address;
    uint16_t ir_command;
    uint8_t ir_repeat_count;
    uint32_t speaker_frequency_hz;
    uint32_t speaker_duration_ms;
    uint8_t speaker_volume_percent;
} rule_action_t;

typedef struct {
    bool enabled;
    uint32_t id;
    char name[RULE_NAME_MAX];
    rule_condition_t when;
    size_t action_count;
    rule_action_t actions[RULE_MAX_ACTIONS_PER_RULE];
    uint32_t cooldown_ms;
} automation_rule_t;

typedef struct {
    uint32_t schema_version;
    size_t rule_count;
    automation_rule_t rules[RULE_MAX_RULES];
} automation_config_t;

const char *rule_source_name(rule_source_t source);
const char *rule_action_name(rule_action_type_t action);
bool rule_value_equal(rule_value_t left, rule_value_t right);
void automation_config_set_defaults(automation_config_t *config);
bool automation_rule_validate(const automation_rule_t *rule, char *error, size_t error_len);
bool automation_config_validate(const automation_config_t *config, char *error, size_t error_len);
bool rule_source_is_gpio(rule_source_t source);
bool rule_source_is_sound(rule_source_t source);
bool automation_config_has_enabled_source(const automation_config_t *config, rule_source_t source);
bool automation_config_has_enabled_sound_source(const automation_config_t *config);

static inline rule_value_t rule_value_bool(bool value)
{
    rule_value_t out = {.kind = RULE_VALUE_BOOL, .as.bool_value = value};
    return out;
}

static inline rule_value_t rule_value_i32(int32_t value)
{
    rule_value_t out = {.kind = RULE_VALUE_I32, .as.i32_value = value};
    return out;
}
