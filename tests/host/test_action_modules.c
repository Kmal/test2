#include "action_hat.h"
#include "action_http.h"
#include "action_ir.h"
#include "action_speaker.h"
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

typedef struct {
    int init_calls;
    int deinit_calls;
    int amp_enable_calls;
    int amp_disable_calls;
    size_t bytes_written;
} fake_speaker_ctx_t;

static esp_err_t fake_speaker_init(void *ctx)
{
    fake_speaker_ctx_t *fake = (fake_speaker_ctx_t *)ctx;
    fake->init_calls++;
    return ESP_OK;
}

static esp_err_t fake_speaker_deinit(void *ctx)
{
    fake_speaker_ctx_t *fake = (fake_speaker_ctx_t *)ctx;
    fake->deinit_calls++;
    return ESP_OK;
}

static esp_err_t fake_speaker_amp_set(bool enable, void *ctx)
{
    fake_speaker_ctx_t *fake = (fake_speaker_ctx_t *)ctx;
    if (enable) {
        fake->amp_enable_calls++;
    } else {
        fake->amp_disable_calls++;
    }
    return ESP_OK;
}

static esp_err_t fake_speaker_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms, void *ctx)
{
    (void)src;
    (void)timeout_ms;
    fake_speaker_ctx_t *fake = (fake_speaker_ctx_t *)ctx;
    fake->bytes_written += size;
    if (bytes_written != NULL) {
        *bytes_written = size;
    }
    return ESP_OK;
}

static void test_speaker_tone_action_validation_and_playback(void)
{
    rule_action_t action = {
        .type = RULE_ACTION_SPEAKER_TONE,
        .speaker_frequency_hz = 7000,
        .speaker_duration_ms = 100,
        .speaker_volume_percent = 50,
        .timeout_ms = 100,
    };
    action_speaker_config_t config;
    ASSERT_TRUE(action_speaker_config_from_action(&action, &config));
    ASSERT_EQ(7000, config.frequency_hz);

    action.speaker_volume_percent = 75;
    ASSERT_FALSE(action_speaker_config_from_action(&action, &config));
    action.speaker_volume_percent = 50;
    ASSERT_TRUE(action_speaker_config_from_action(&action, &config));

    fake_speaker_ctx_t fake = {0};
    const action_speaker_ops_t ops = {
        .audio_init_playback = fake_speaker_init,
        .audio_deinit = fake_speaker_deinit,
        .speaker_amp_set = fake_speaker_amp_set,
        .write_pcm = fake_speaker_write,
        .ctx = &fake,
    };
    ASSERT_EQ(ESP_OK, action_speaker_play_tone_with_ops(&config, &ops));
    ASSERT_EQ(1, fake.init_calls);
    ASSERT_EQ(1, fake.deinit_calls);
    ASSERT_EQ(1, fake.amp_enable_calls);
    ASSERT_EQ(1, fake.amp_disable_calls);
    ASSERT_TRUE(fake.bytes_written >= 1600u * sizeof(int32_t));
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
    test_speaker_tone_action_validation_and_playback();
    puts("action_modules tests passed");
    return 0;
}
