#include "action_hat.h"
#include "action_http.h"
#include "action_ir.h"
#include "rule_web.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

static void test_http_json_and_not_ready(void)
{
    rule_event_t event = {.sequence = 1, .uptime_ms = 2, .rule_id = 3, .source = RULE_SOURCE_KEY1_SHORT, .action = RULE_ACTION_HTTP_POST, .fire_count = 4};
    char json[160];
    ASSERT_TRUE(rule_event_to_json(&event, json, sizeof(json)));
    rule_action_t action = {.type = RULE_ACTION_HTTP_POST, .timeout_ms = 1000};
    snprintf(action.http_url, sizeof(action.http_url), "https://example.invalid/hook");
    ASSERT_FALSE(action_http_network_ready());
    ASSERT_EQ(ACTION_HTTP_RESULT_NOT_READY, action_http_post_event(&action, &event));
    action_http_set_network_ready(true);
    ASSERT_TRUE(action_http_network_ready());
    ASSERT_EQ(ACTION_HTTP_RESULT_NOT_READY, action_http_post_event(&action, &event));
    action_http_set_network_ready(false);
}

static void test_ir_and_hat_are_bounded_or_disabled(void)
{
    action_ir_config_t ir = {.protocol = RULE_IR_PROTOCOL_NEC, .carrier_hz = 38000, .repeat_count = 2, .timeout_ms = 100};
    ASSERT_TRUE(action_ir_validate(&ir));
    ir.repeat_count = 99;
    ASSERT_FALSE(action_ir_validate(&ir));
    ASSERT_FALSE(hat_operation_supported(RULE_HAT_OPERATION_RELAY_SET));
}

int main(void)
{
    test_http_json_and_not_ready();
    test_ir_and_hat_are_bounded_or_disabled();
    puts("action_modules tests passed");
    return 0;
}
