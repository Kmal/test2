#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_mode.h"
#include "audio_metrics.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef app_runtime_state_t (*sound_meter_runtime_getter_t)(void *ctx);

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t metrics_window_ms;
    uint32_t i2s_read_timeout_ms;
    size_t pcm_chunk_bytes;
    bool enable_ble_telemetry;
    bool enable_lcd_updates;
    sound_meter_runtime_getter_t get_runtime;
    void *runtime_ctx;
} sound_meter_config_t;

typedef struct {
    uint32_t windows_completed;
    uint32_t i2s_read_errors;
    uint32_t i2s_zero_reads;
    uint32_t telemetry_publish_errors;
    uint32_t lcd_updates;
    uint32_t last_sequence;
    int64_t last_update_ms;
} sound_meter_stats_t;

esp_err_t sound_meter_start(const sound_meter_config_t *config);
bool sound_meter_get_latest(audio_level_metrics_t *out);
void sound_meter_get_stats(sound_meter_stats_t *out);
void sound_meter_set_enabled(bool enabled);
bool sound_meter_get_enabled(void);

#ifdef __cplusplus
}
#endif
