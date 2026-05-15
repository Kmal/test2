#pragma once

#include "rule_engine.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Keep the action bounds aligned with rule validation. The integer cap is
 * 74% so configured values stay below the official StickS3 75% battery-volume
 * notice. */
#define ACTION_SPEAKER_MIN_FREQUENCY_HZ RULE_SPEAKER_MIN_FREQUENCY_HZ
#define ACTION_SPEAKER_MAX_FREQUENCY_HZ RULE_SPEAKER_MAX_FREQUENCY_HZ
#define ACTION_SPEAKER_MIN_DURATION_MS RULE_SPEAKER_MIN_DURATION_MS
#define ACTION_SPEAKER_MAX_DURATION_MS RULE_SPEAKER_MAX_DURATION_MS
#define ACTION_SPEAKER_MAX_VOLUME_PERCENT RULE_SPEAKER_MAX_VOLUME_PERCENT

typedef struct {
    uint32_t frequency_hz;
    uint32_t duration_ms;
    uint8_t volume_percent;
    uint32_t timeout_ms;
} action_speaker_config_t;

typedef struct {
    esp_err_t (*audio_init_playback)(void *ctx);
    esp_err_t (*audio_deinit)(void *ctx);
    esp_err_t (*speaker_amp_set)(bool enable, void *ctx);
    esp_err_t (*write_pcm)(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms, void *ctx);
    void (*delay_ms)(uint32_t ms, void *ctx);
    void *ctx;
} action_speaker_ops_t;

bool action_speaker_validate(const action_speaker_config_t *config);
bool action_speaker_config_from_action(const rule_action_t *action, action_speaker_config_t *config);
esp_err_t action_speaker_play_tone_with_ops(const action_speaker_config_t *config, const action_speaker_ops_t *ops);
bool action_speaker_play_tone(const action_speaker_config_t *config);
bool action_speaker_send_event(const rule_event_t *event);
