#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t starts;
    uint32_t stops;
    uint32_t write_calls;
    uint32_t write_failures;
    uint32_t samples_written;
    uint8_t volume_percent;
    bool muted;
} uac_speaker_sink_stats_t;

typedef struct {
    esp_err_t (*init_playback)(void *ctx);
    esp_err_t (*write_i16)(const int16_t *src, size_t samples, size_t *samples_written, uint32_t timeout_ms, void *ctx);
    esp_err_t (*set_volume_percent)(uint8_t volume_percent, void *ctx);
    esp_err_t (*set_mute)(bool muted, void *ctx);
    esp_err_t (*deinit)(void *ctx);
    void *ctx;
} uac_speaker_sink_ops_t;

esp_err_t uac_speaker_sink_init(const uac_speaker_sink_ops_t *ops);
esp_err_t uac_speaker_sink_start(void);
esp_err_t uac_speaker_sink_write_i16(const int16_t *src, size_t samples, size_t *samples_written, uint32_t timeout_ms);
esp_err_t uac_speaker_sink_set_volume_percent(uint8_t volume_percent);
esp_err_t uac_speaker_sink_set_mute(bool muted);
esp_err_t uac_speaker_sink_stop(void);
uac_speaker_sink_stats_t uac_speaker_sink_get_stats(void);
void uac_speaker_sink_reset_for_test(void);

#ifdef __cplusplus
}
#endif
