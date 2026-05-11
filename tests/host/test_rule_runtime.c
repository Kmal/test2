#include "rule_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

static automation_config_t runtime_config(void)
{
    automation_config_t config;
    automation_config_set_defaults(&config);
    config.rule_count = 1;
    config.rules[0].enabled = true;
    config.rules[0].id = 7;
    (void)snprintf(config.rules[0].name, RULE_NAME_MAX, "runtime button");
    config.rules[0].when.source = RULE_SOURCE_KEY1_SHORT;
    config.rules[0].when.comparator = RULE_COMPARATOR_EQ;
    config.rules[0].when.threshold = rule_value_bool(true);
    config.rules[0].when.sustain_ms = 0;
    config.rules[0].cooldown_ms = 100;
    config.rules[0].action_count = 1;
    config.rules[0].actions[0].type = RULE_ACTION_LOCAL_UI;
    return config;
}


static automation_config_t gpio_runtime_config(void)
{
    automation_config_t config = runtime_config();
    (void)snprintf(config.rules[0].name, RULE_NAME_MAX, "runtime gpio");
    config.rules[0].when.source = RULE_SOURCE_GPIO_DIGITAL;
    (void)snprintf(config.rules[0].when.source_key, RULE_SOURCE_KEY_MAX, "gpio.digital.4");
    config.rules[0].when.gpio.pin = 4;
    config.rules[0].when.gpio.profile = RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW;
    config.rules[0].when.gpio.debounce_ms = 10;
    return config;
}

static void test_runtime_gpio_poll_to_action_result(void)
{
    automation_config_t config = gpio_runtime_config();
    rule_runtime_t runtime;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    ASSERT_EQ(1, runtime.gpio_trigger_count);
    trigger_gpio_set_host_level(&runtime.gpio_triggers[0], false);
    ASSERT_EQ(0, rule_runtime_poll_gpio(&runtime, 100));
    trigger_gpio_set_host_level(&runtime.gpio_triggers[0], true);
    ASSERT_EQ(0, rule_runtime_poll_gpio(&runtime, 105));
    ASSERT_EQ(1, rule_runtime_poll_gpio(&runtime, 116));
    action_result_t result = rule_runtime_get_last_action_result(&runtime);
    ASSERT_EQ(ACTION_RESULT_OK, result.code);
    ASSERT_EQ(7, result.rule_id);
}


static void test_runtime_ble_connected_fact_to_action_result(void)
{
    automation_config_t config = runtime_config();
    (void)snprintf(config.rules[0].name, RULE_NAME_MAX, "runtime ble");
    config.rules[0].when.source = RULE_SOURCE_BLE_CONNECTED;
    config.rules[0].when.threshold = rule_value_bool(true);
    rule_runtime_t runtime;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    trigger_fact_t fact = {
        .source = RULE_SOURCE_BLE_CONNECTED,
        .value = {.kind = RULE_VALUE_BOOL, .as.bool_value = true},
        .uptime_ms = 10,
    };
    ASSERT_EQ(1, rule_runtime_process_fact(&runtime, &fact));
    ASSERT_EQ(1, rule_runtime_process_actions(&runtime));
    ASSERT_EQ(ACTION_RESULT_OK, rule_runtime_get_last_action_result(&runtime).code);
}


static void test_runtime_wifi_connected_fact_to_action_result(void)
{
    automation_config_t config = runtime_config();
    (void)snprintf(config.rules[0].name, RULE_NAME_MAX, "runtime wifi");
    config.rules[0].when.source = RULE_SOURCE_WIFI_CONNECTED;
    config.rules[0].when.threshold = rule_value_bool(true);
    rule_runtime_t runtime;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    trigger_fact_t fact = {
        .source = RULE_SOURCE_WIFI_CONNECTED,
        .value = {.kind = RULE_VALUE_BOOL, .as.bool_value = true},
        .uptime_ms = 10,
    };
    ASSERT_EQ(1, rule_runtime_process_fact(&runtime, &fact));
    ASSERT_EQ(1, rule_runtime_process_actions(&runtime));
    ASSERT_EQ(ACTION_RESULT_OK, rule_runtime_get_last_action_result(&runtime).code);
}

static void test_runtime_button_to_action_result(void)
{
    automation_config_t config = runtime_config();
    rule_runtime_t runtime;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    ASSERT_EQ(1, rule_runtime_process_button_event(&runtime, BUTTON_STATE_EVENT_KEY1_SHORT, 10));
    action_result_t result = rule_runtime_get_last_action_result(&runtime);
    ASSERT_EQ(ACTION_RESULT_OK, result.code);
    ASSERT_EQ(7, result.rule_id);
}


static action_result_t fake_runtime_local_ui_sender(const rule_event_t *event, void *ctx)
{
    int *calls = (int *)ctx;
    if (calls != NULL) {
        (*calls)++;
    }
    action_result_t result = {
        .code = ACTION_RESULT_OK,
        .sequence = event->sequence,
        .rule_id = event->rule_id,
        .action = event->action,
    };
    return result;
}

static void test_runtime_local_ui_callback(void)
{
    automation_config_t config = runtime_config();
    rule_runtime_t runtime;
    int calls = 0;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    rule_runtime_set_local_ui_sender(&runtime, fake_runtime_local_ui_sender, &calls);
    ASSERT_EQ(1, rule_runtime_process_button_event(&runtime, BUTTON_STATE_EVENT_KEY1_SHORT, 10));
    ASSERT_EQ(1, calls);
    ASSERT_EQ(ACTION_RESULT_OK, rule_runtime_get_last_action_result(&runtime).code);
}

static void test_replace_config_rejects_invalid(void)
{
    automation_config_t config = runtime_config();
    rule_runtime_t runtime;
    ASSERT_TRUE(rule_runtime_init(&runtime, &config));
    config.rules[0].cooldown_ms = 0;
    ASSERT_EQ(0, rule_runtime_replace_config(&runtime, &config));
}

int main(void)
{
    test_runtime_button_to_action_result();
    test_runtime_gpio_poll_to_action_result();
    test_runtime_ble_connected_fact_to_action_result();
    test_runtime_wifi_connected_fact_to_action_result();
    test_runtime_local_ui_callback();
    test_replace_config_rejects_invalid();
    puts("rule_runtime tests passed");
    return 0;
}
