#include "trigger_sources.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

typedef struct {
    trigger_fact_t facts[8];
    size_t count;
} fact_capture_t;

static bool capture_sink(const trigger_fact_t *fact, void *user_ctx)
{
    fact_capture_t *capture = (fact_capture_t *)user_ctx;
    ASSERT_TRUE(capture->count < sizeof(capture->facts) / sizeof(capture->facts[0]));
    capture->facts[capture->count++] = *fact;
    return true;
}

static void test_sound_metrics_emit_three_facts(void)
{
    fact_capture_t capture;
    memset(&capture, 0, sizeof(capture));
    trigger_adapter_t adapter;
    trigger_adapter_init(&adapter, capture_sink, &capture);
    audio_level_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.rms_dbfs_q8 = -100;
    metrics.peak_dbfs_q8 = -50;
    metrics.clipped_samples = 2;
    ASSERT_EQ(3, trigger_emit_sound_facts(&adapter, &metrics, 123));
    ASSERT_EQ(RULE_SOURCE_SOUND_RMS_DBFS, capture.facts[0].source);
    ASSERT_EQ(-100, capture.facts[0].value.as.i32_value);
    ASSERT_EQ(RULE_SOURCE_SOUND_PEAK_DBFS, capture.facts[1].source);
    ASSERT_EQ(RULE_SOURCE_SOUND_CLIPPED, capture.facts[2].source);
    ASSERT_TRUE(capture.facts[2].value.as.bool_value);
    ASSERT_EQ(123, capture.facts[2].uptime_ms);
}

static void test_both_short_emits_key1_and_key2(void)
{
    fact_capture_t capture;
    memset(&capture, 0, sizeof(capture));
    trigger_adapter_t adapter;
    trigger_adapter_init(&adapter, capture_sink, &capture);
    ASSERT_EQ(2, trigger_emit_button_event(&adapter, BUTTON_STATE_EVENT_BOTH_SHORT, 77));
    ASSERT_EQ(RULE_SOURCE_KEY1_SHORT, capture.facts[0].source);
    ASSERT_EQ(RULE_SOURCE_KEY2_SHORT, capture.facts[1].source);
    ASSERT_TRUE(capture.facts[0].value.as.bool_value);
    ASSERT_EQ(77, capture.facts[1].uptime_ms);
}

int main(void)
{
    test_sound_metrics_emit_three_facts();
    test_both_short_emits_key1_and_key2();
    puts("trigger_sources tests passed");
    return 0;
}
