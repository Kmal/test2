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

typedef struct sound_meter_stats sound_meter_stats_t;

typedef app_runtime_state_t (*sound_meter_runtime_getter_t)(void *ctx);
typedef esp_err_t (*sound_meter_publish_metrics_cb_t)(const audio_level_metrics_t *metrics,
                                                      app_mode_t app_mode,
                                                      app_display_mode_t display_mode,
                                                      void *ctx);
typedef bool (*sound_meter_bool_cb_t)(void *ctx);
typedef void (*sound_meter_status_update_cb_t)(const struct sound_meter_stats *stats,
                                               const audio_calibration_t *calibration,
                                               const app_runtime_state_t *runtime,
                                               bool enabled,
                                               void *ctx);
typedef app_runtime_state_t (*sound_meter_runtime_getter_t)(void *ctx);

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t metrics_window_ms;
    uint32_t i2s_read_timeout_ms;
    size_t pcm_chunk_bytes;
    bool enable_ble_telemetry;
    bool enable_lcd_updates;
    int32_t dbfs_floor_q8;
    int32_t dbfs_ceiling_q8;
    int32_t loud_threshold_q8;
    uint32_t telemetry_interval_ms;
    uint32_t calibration_windows;
    sound_meter_runtime_getter_t get_runtime;
    void *runtime_ctx;
    sound_meter_publish_metrics_cb_t publish_metrics;
    sound_meter_bool_cb_t transport_connected;
    sound_meter_bool_cb_t metrics_notify_enabled;
    sound_meter_bool_cb_t pcm_notify_enabled;
    sound_meter_bool_cb_t pcm_debug_enabled;
    sound_meter_status_update_cb_t status_update;
    void *transport_ctx;
} sound_meter_config_t;

struct sound_meter_stats {
    sound_meter_runtime_getter_t get_runtime;
    void *runtime_ctx;
} sound_meter_config_t;

typedef struct {
    uint32_t windows_completed;
    uint32_t i2s_read_errors;
    uint32_t i2s_zero_reads;
    uint32_t telemetry_publish_errors;
    uint32_t lcd_updates;
    uint32_t pcm_debug_ring_overruns;
    uint32_t last_sequence;
    int64_t last_update_ms;
};
    uint32_t last_sequence;
    int64_t last_update_ms;
} sound_meter_stats_t;

esp_err_t sound_meter_start(const sound_meter_config_t *config);
bool sound_meter_get_latest(audio_level_metrics_t *out);
void sound_meter_get_stats(sound_meter_stats_t *out);
bool sound_meter_get_calibration(audio_calibration_t *out);
void sound_meter_reset_calibration(void);
void sound_meter_set_enabled(bool enabled);
bool sound_meter_get_enabled(void);
size_t sound_meter_read_pcm_debug(uint8_t *dst, size_t max_bytes);
uint32_t sound_meter_get_pcm_debug_overruns(void);
void sound_meter_set_enabled(bool enabled);
bool sound_meter_get_enabled(void);

#ifdef __cplusplus
}
#endif
