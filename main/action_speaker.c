#include "action_speaker.h"

#include "board_audio.h"
#include "board_audio_power.h"
#include "board_i2s.h"
#include "board_sticks3.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define SPEAKER_SAMPLE_RATE_HZ BOARD_I2S_SAMPLE_RATE
#define SPEAKER_PCM_CHUNK_FRAMES 128u
#define SPEAKER_SILENCE_FRAMES 64u
#define SPEAKER_DEFAULT_TIMEOUT_MS 100u
#define SPEAKER_FULL_SCALE_Q15 32767

typedef struct {
    uint32_t phase;
    uint32_t step;
    int32_t amplitude;
} speaker_square_osc_t;

bool action_speaker_validate(const action_speaker_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (config->frequency_hz < ACTION_SPEAKER_MIN_FREQUENCY_HZ ||
        config->frequency_hz > ACTION_SPEAKER_MAX_FREQUENCY_HZ) {
        return false;
    }
    if (config->duration_ms < ACTION_SPEAKER_MIN_DURATION_MS ||
        config->duration_ms > ACTION_SPEAKER_MAX_DURATION_MS) {
        return false;
    }
    if (config->volume_percent == 0 || config->volume_percent > ACTION_SPEAKER_MAX_VOLUME_PERCENT) {
        return false;
    }
    if (config->timeout_ms == 0 || config->timeout_ms > 1000u) {
        return false;
    }
    return true;
}

bool action_speaker_config_from_action(const rule_action_t *action, action_speaker_config_t *config)
{
    if (action == NULL || config == NULL || action->type != RULE_ACTION_SPEAKER_TONE) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    config->frequency_hz = action->speaker_frequency_hz;
    config->duration_ms = action->speaker_duration_ms;
    config->volume_percent = action->speaker_volume_percent;
    config->timeout_ms = action->timeout_ms == 0 ? SPEAKER_DEFAULT_TIMEOUT_MS : action->timeout_ms;
    return action_speaker_validate(config);
}

static int32_t speaker_next_square_sample(speaker_square_osc_t *osc)
{
    const int32_t sample = (osc->phase & 0x80000000u) ? osc->amplitude : -osc->amplitude;
    osc->phase += osc->step;
    return sample;
}

static size_t speaker_fill_tone_frames(int32_t *frames, size_t max_frames, speaker_square_osc_t *osc, size_t remaining_frames)
{
    const size_t count = remaining_frames < max_frames ? remaining_frames : max_frames;
    for (size_t i = 0; i < count; ++i) {
        const int32_t sample = speaker_next_square_sample(osc);
        frames[i] = (int32_t)((uint32_t)(uint16_t)sample << 16u);
    }
    return count;
}

static esp_err_t speaker_write_all(const action_speaker_ops_t *ops, const void *src, size_t size, uint32_t timeout_ms)
{
    size_t total = 0;
    while (total < size) {
        size_t written = 0;
        esp_err_t err = ops->write_pcm((const uint8_t *)src + total, size - total, &written, timeout_ms, ops->ctx);
        if (err != ESP_OK) {
            return err;
        }
        if (written == 0) {
            return ESP_ERR_TIMEOUT;
        }
        total += written;
    }
    return ESP_OK;
}

esp_err_t action_speaker_play_tone_with_ops(const action_speaker_config_t *config, const action_speaker_ops_t *ops)
{
    if (!action_speaker_validate(config) || ops == NULL || ops->audio_init_playback == NULL ||
        ops->speaker_amp_set == NULL || ops->write_pcm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ops->audio_init_playback(ops->ctx);
    if (err != ESP_OK) {
        return err;
    }

    err = ops->speaker_amp_set(true, ops->ctx);
    if (err != ESP_OK) {
        if (ops->audio_deinit != NULL) {
            (void)ops->audio_deinit(ops->ctx);
        }
        return err;
    }

    if (ops->delay_ms != NULL) {
        ops->delay_ms(5, ops->ctx);
    }

    speaker_square_osc_t osc = {
        .phase = 0,
        .step = (uint32_t)(((uint64_t)config->frequency_hz << 32u) / SPEAKER_SAMPLE_RATE_HZ),
        .amplitude = ((int32_t)SPEAKER_FULL_SCALE_Q15 * (int32_t)config->volume_percent) / 100,
    };
    int32_t frames[SPEAKER_PCM_CHUNK_FRAMES];
    size_t remaining = ((size_t)config->duration_ms * SPEAKER_SAMPLE_RATE_HZ) / 1000u;
    if (remaining == 0) {
        remaining = 1;
    }

    while (remaining > 0) {
        const size_t count = speaker_fill_tone_frames(frames, SPEAKER_PCM_CHUNK_FRAMES, &osc, remaining);
        err = speaker_write_all(ops, frames, count * sizeof(frames[0]), config->timeout_ms);
        if (err != ESP_OK) {
            (void)ops->speaker_amp_set(false, ops->ctx);
            if (ops->audio_deinit != NULL) {
                (void)ops->audio_deinit(ops->ctx);
            }
            return err;
        }
        remaining -= count;
    }

    memset(frames, 0, SPEAKER_SILENCE_FRAMES * sizeof(frames[0]));
    err = speaker_write_all(ops, frames, SPEAKER_SILENCE_FRAMES * sizeof(frames[0]), config->timeout_ms);
    esp_err_t amp_err = ops->speaker_amp_set(false, ops->ctx);
    esp_err_t deinit_err = ESP_OK;
    if (ops->audio_deinit != NULL) {
        deinit_err = ops->audio_deinit(ops->ctx);
    }
    return err != ESP_OK ? err : (amp_err != ESP_OK ? amp_err : deinit_err);
}

#ifdef ESP_PLATFORM
static esp_err_t real_audio_init_playback(void *ctx)
{
    (void)ctx;
    const board_audio_config_t config = {
        .profile = BOARD_AUDIO_PROFILE_PLAYBACK_ONLY,
        .probe_m5pm1 = true,
        .require_audio_power_enable = true,
    };
    return board_audio_init(&config);
}

static esp_err_t real_audio_deinit(void *ctx)
{
    (void)ctx;
    return board_audio_deinit();
}

static esp_err_t real_speaker_amp_set(bool enable, void *ctx)
{
    (void)ctx;
    return board_speaker_amp_set(BOARD_I2C_PORT, enable);
}

static esp_err_t real_write_pcm(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms, void *ctx)
{
    (void)ctx;
    return board_i2s_write(src, size, bytes_written, timeout_ms);
}

static void real_delay_ms(uint32_t ms, void *ctx)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms));
}
#endif

bool action_speaker_play_tone(const action_speaker_config_t *config)
{
#ifdef ESP_PLATFORM
    const action_speaker_ops_t ops = {
        .audio_init_playback = real_audio_init_playback,
        .audio_deinit = real_audio_deinit,
        .speaker_amp_set = real_speaker_amp_set,
        .write_pcm = real_write_pcm,
        .delay_ms = real_delay_ms,
        .ctx = NULL,
    };
    return action_speaker_play_tone_with_ops(config, &ops) == ESP_OK;
#else
    (void)config;
    return false;
#endif
}

bool action_speaker_send_event(const rule_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    action_speaker_config_t config;
    if (!action_speaker_config_from_action(&event->action_config, &config)) {
        return false;
    }
    return action_speaker_play_tone(&config);
}
