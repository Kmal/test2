#include "trigger_sources.h"

#include <string.h>

bool trigger_emit_fact(trigger_adapter_t *adapter, trigger_fact_t *fact)
{
    if (adapter == NULL || adapter->sink == NULL || fact == NULL) {
        return false;
    }
    fact->sequence = adapter->next_sequence++;
    return adapter->sink(fact, adapter->user_ctx);
}

void trigger_adapter_init(trigger_adapter_t *adapter, trigger_fact_sink_t sink, void *user_ctx)
{
    if (adapter == NULL) {
        return;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->sink = sink;
    adapter->user_ctx = user_ctx;
    adapter->next_sequence = 1;
}

size_t trigger_emit_sound_facts(trigger_adapter_t *adapter, const audio_level_metrics_t *metrics, uint32_t uptime_ms)
{
    if (adapter == NULL || metrics == NULL) {
        return 0;
    }

    size_t emitted = 0;
    trigger_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    fact.source = RULE_SOURCE_SOUND_RMS_DBFS;
    fact.value = rule_value_i32(metrics->rms_dbfs_q8);
    fact.uptime_ms = uptime_ms;
    if (trigger_emit_fact(adapter, &fact)) {
        ++emitted;
    }

    memset(&fact, 0, sizeof(fact));
    fact.source = RULE_SOURCE_SOUND_PEAK_DBFS;
    fact.value = rule_value_i32(metrics->peak_dbfs_q8);
    fact.uptime_ms = uptime_ms;
    if (trigger_emit_fact(adapter, &fact)) {
        ++emitted;
    }

    memset(&fact, 0, sizeof(fact));
    fact.source = RULE_SOURCE_SOUND_CLIPPED;
    fact.value = rule_value_bool(metrics->clipped_samples > 0);
    fact.uptime_ms = uptime_ms;
    if (trigger_emit_fact(adapter, &fact)) {
        ++emitted;
    }

    return emitted;
}

size_t trigger_emit_button_event(trigger_adapter_t *adapter, button_state_event_t event, uint32_t uptime_ms)
{
    if (adapter == NULL || event == BUTTON_STATE_EVENT_NONE) {
        return 0;
    }

    size_t emitted = 0;
    trigger_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    fact.value = rule_value_bool(true);
    fact.uptime_ms = uptime_ms;

    if (event == BUTTON_STATE_EVENT_KEY1_SHORT || event == BUTTON_STATE_EVENT_BOTH_SHORT) {
        fact.source = RULE_SOURCE_KEY1_SHORT;
        if (trigger_emit_fact(adapter, &fact)) {
            ++emitted;
        }
    }
    if (event == BUTTON_STATE_EVENT_KEY2_SHORT || event == BUTTON_STATE_EVENT_BOTH_SHORT) {
        memset(fact.source_key, 0, sizeof(fact.source_key));
        fact.source = RULE_SOURCE_KEY2_SHORT;
        if (trigger_emit_fact(adapter, &fact)) {
            ++emitted;
        }
    }
    return emitted;
}
