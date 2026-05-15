#include "rule_types.h"
#include "capability_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

static automation_config_t valid_config(void)
{
    automation_config_t config;
    automation_config_set_defaults(&config);
    config.rules[0].enabled = true;
    return config;
}

static void set_http_url(automation_config_t *config, const char *url)
{
    config->rules[0].actions[0].type = RULE_ACTION_HTTP_POST;
    config->rules[0].actions[0].timeout_ms = 1000;
    (void)snprintf(config->rules[0].actions[0].http_url, RULE_HTTP_URL_MAX, "%s", url);
}

static void make_gpio_rule(automation_config_t *config, int pin, rule_gpio_profile_t profile)
{
    config->rules[0].when.source = RULE_SOURCE_GPIO_DIGITAL;
    (void)snprintf(config->rules[0].when.source_key, RULE_SOURCE_KEY_MAX, "gpio.digital.0");
    config->rules[0].when.gpio.pin = pin;
    config->rules[0].when.gpio.profile = profile;
    config->rules[0].when.gpio.debounce_ms = 20;
}

static void test_defaults_validate(void)
{
    automation_config_t config;
    char error[RULE_ERROR_MAX];
    automation_config_set_defaults(&config);
    ASSERT_EQ(RULE_CONFIG_SCHEMA_VERSION, config.schema_version);
    ASSERT_TRUE(config.rule_count > 0);
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
    ASSERT_FALSE(config.rules[0].enabled);
}

static void test_rejects_invalid_enum_and_counts(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].when.source = RULE_SOURCE_COUNT;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    config.rules[0].action_count = RULE_MAX_ACTIONS_PER_RULE + 1;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    config.rule_count = RULE_MAX_RULES + 1;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));
}

static void test_rejects_bad_cooldown_hat_and_unsafe_gpio(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].cooldown_ms = 0;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    config.rules[0].actions[0].type = RULE_ACTION_HAT_OPERATION;
    config.rules[0].actions[0].hat_operation = RULE_HAT_OPERATION_RELAY_SET;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    config.rules[0].when.source = RULE_SOURCE_HAT_PIR_MOTION;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    make_gpio_rule(&config, 39, RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW);
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    make_gpio_rule(&config, 4, RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW);
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
}


static void test_ble_connected_source_is_supported(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].when.source = RULE_SOURCE_BLE_CONNECTED;
    config.rules[0].when.threshold = rule_value_bool(true);
    config.rules[0].when.comparator = RULE_COMPARATOR_EQ;
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
    ASSERT_TRUE(capability_source_supported(RULE_SOURCE_BLE_CONNECTED));
}


static void test_wifi_connected_source_is_supported(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].when.source = RULE_SOURCE_WIFI_CONNECTED;
    config.rules[0].when.threshold = rule_value_bool(true);
    config.rules[0].when.comparator = RULE_COMPARATOR_EQ;
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
    ASSERT_TRUE(capability_source_supported(RULE_SOURCE_WIFI_CONNECTED));
}

static void test_http_url_validation(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    set_http_url(&config, "ftp://example.invalid");
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    set_http_url(&config, "https://");
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    set_http_url(&config, "https:///path");
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    set_http_url(&config, "https://example.invalid/bad path");
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    set_http_url(&config, "https://example.invalid/hook");
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
}

static void test_ir_action_validation(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].actions[0].type = RULE_ACTION_IR_SEND;
    config.rules[0].actions[0].ir_protocol = RULE_IR_PROTOCOL_NEC;
    config.rules[0].actions[0].ir_carrier_hz = 38000;
    config.rules[0].actions[0].ir_repeat_count = 2;
    config.rules[0].actions[0].timeout_ms = 100;
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
    config.rules[0].actions[0].ir_repeat_count = 99;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));
}

static void test_speaker_action_validation(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].actions[0].type = RULE_ACTION_SPEAKER_TONE;
    config.rules[0].actions[0].speaker_frequency_hz = 7000;
    config.rules[0].actions[0].speaker_duration_ms = 100;
    config.rules[0].actions[0].speaker_volume_percent = 50;
    config.rules[0].actions[0].timeout_ms = 100;
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));

    config.rules[0].actions[0].speaker_volume_percent = 75;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));
    config.rules[0].actions[0].speaker_volume_percent = 50;

    config.rules[0].actions[0].speaker_frequency_hz = 9000;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));
}

static void test_comparator_value_type_support(void)
{
    char error[RULE_ERROR_MAX];
    automation_config_t config = valid_config();
    config.rules[0].when.threshold = rule_value_bool(true);
    config.rules[0].when.comparator = RULE_COMPARATOR_GT;
    ASSERT_FALSE(automation_config_validate(&config, error, sizeof(error)));

    config = valid_config();
    config.rules[0].when.threshold = rule_value_i32(1);
    config.rules[0].when.comparator = RULE_COMPARATOR_GT;
    ASSERT_TRUE(automation_config_validate(&config, error, sizeof(error)));
}

static void test_gpio_profile_support_direct(void)
{
    char error[RULE_ERROR_MAX];
    rule_gpio_config_t gpio = {
        .pin = 4,
        .profile = RULE_GPIO_PROFILE_FREQUENCY,
        .debounce_ms = 20,
    };
    ASSERT_FALSE(capability_gpio_source_profile_validate(RULE_SOURCE_GPIO_DIGITAL, &gpio, error, sizeof(error)));
    ASSERT_TRUE(capability_gpio_source_profile_validate(RULE_SOURCE_GPIO_FREQUENCY_HZ, &gpio, error, sizeof(error)));

    gpio.profile = RULE_GPIO_PROFILE_RISING_EDGE;
    ASSERT_TRUE(capability_source_supported(RULE_SOURCE_GPIO_EDGE));
    ASSERT_TRUE(capability_gpio_source_profile_validate(RULE_SOURCE_GPIO_EDGE, &gpio, error, sizeof(error)));

    gpio.profile = RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW;
    ASSERT_TRUE(capability_gpio_source_profile_validate(RULE_SOURCE_GPIO_DIGITAL, &gpio, error, sizeof(error)));
}

static void test_capability_json_excludes_disabled_actions(void)
{
    char json[4096];
    ASSERT_TRUE(capability_build_json(json, sizeof(json)) > 0);
    ASSERT_TRUE(strstr(json, "ble_message") != NULL);
    ASSERT_TRUE(strstr(json, "local_ui") != NULL);
    ASSERT_TRUE(strstr(json, "http_post\"],") == NULL);
    ASSERT_TRUE(strstr(json, "pin_conflicts") != NULL);
    ASSERT_TRUE(strstr(json, "hat_sources") != NULL);
    ASSERT_TRUE(strstr(json, "hat.thermal.avg_c") != NULL);
    ASSERT_TRUE(strstr(json, "hat.heart_rate.bpm") != NULL);
    ASSERT_TRUE(strstr(json, "hat.adc.voltage_mv") != NULL);
    ASSERT_TRUE(strstr(json, "rising_edge") != NULL);
    ASSERT_TRUE(strstr(json, "pulse_count") != NULL);
    ASSERT_TRUE(strstr(json, "frequency") != NULL);
}

static void test_enabled_sound_source_detection(void)
{
    automation_config_t config;
    automation_config_set_defaults(&config);
    ASSERT_FALSE(automation_config_has_enabled_sound_source(&config));
    ASSERT_FALSE(automation_config_has_enabled_source(&config, RULE_SOURCE_SOUND_RMS_DBFS));

    config.rules[0].enabled = true;
    ASSERT_TRUE(rule_source_is_sound(RULE_SOURCE_SOUND_RMS_DBFS));
    ASSERT_TRUE(rule_source_is_sound(RULE_SOURCE_SOUND_PEAK_DBFS));
    ASSERT_TRUE(rule_source_is_sound(RULE_SOURCE_SOUND_CLIPPED));
    ASSERT_FALSE(rule_source_is_sound(RULE_SOURCE_KEY1_SHORT));
    ASSERT_TRUE(automation_config_has_enabled_sound_source(&config));
    ASSERT_TRUE(automation_config_has_enabled_source(&config, RULE_SOURCE_SOUND_RMS_DBFS));

    config.rules[0].enabled = false;
    config.rules[1].enabled = true;
    ASSERT_FALSE(automation_config_has_enabled_sound_source(&config));
}

static void test_names_and_value_equality(void)
{
    ASSERT_TRUE(strcmp(rule_source_name(RULE_SOURCE_HAT_ENV3_HUMIDITY_RH), "hat.env3.humidity_rh") == 0);
    ASSERT_TRUE(strcmp(rule_source_name(RULE_SOURCE_HAT_THERMAL_MAX_C), "hat.thermal.max_c") == 0);
    ASSERT_TRUE(strcmp(rule_action_name(RULE_ACTION_IR_SEND), "ir_send") == 0);
    ASSERT_TRUE(strcmp(rule_action_name(RULE_ACTION_SPEAKER_TONE), "speaker_tone") == 0);
    ASSERT_TRUE(rule_value_equal(rule_value_i32(7), rule_value_i32(7)));
    ASSERT_FALSE(rule_value_equal(rule_value_i32(7), rule_value_bool(true)));
}

int main(void)
{
    test_defaults_validate();
    test_rejects_invalid_enum_and_counts();
    test_rejects_bad_cooldown_hat_and_unsafe_gpio();
    test_ble_connected_source_is_supported();
    test_wifi_connected_source_is_supported();
    test_http_url_validation();
    test_ir_action_validation();
    test_speaker_action_validation();
    test_comparator_value_type_support();
    test_gpio_profile_support_direct();
    test_capability_json_excludes_disabled_actions();
    test_enabled_sound_source_detection();
    test_names_and_value_equality();
    puts("rule_types tests passed");
    return 0;
}
