#pragma once

#include "audio_metrics.h"
#include "button_state.h"
#include "rule_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*trigger_fact_sink_t)(const trigger_fact_t *fact, void *user_ctx);

typedef struct {
    trigger_fact_sink_t sink;
    void *user_ctx;
    uint32_t next_sequence;
} trigger_adapter_t;

void trigger_adapter_init(trigger_adapter_t *adapter, trigger_fact_sink_t sink, void *user_ctx);
bool trigger_emit_fact(trigger_adapter_t *adapter, trigger_fact_t *fact);
size_t trigger_emit_sound_facts(trigger_adapter_t *adapter, const audio_level_metrics_t *metrics, uint32_t uptime_ms);
size_t trigger_emit_button_event(trigger_adapter_t *adapter, button_state_event_t event, uint32_t uptime_ms);
