#include "uac_speaker_sink.h"

#include "board_audio.h"
#include "board_i2s.h"
#include "board_sticks3.h"
#include "es8311.h"
#include "uac_config.h"

#include <string.h>

static uac_speaker_sink_ops_t s_ops;
static bool s_initialized;
static bool s_started;
static uac_speaker_sink_stats_t s_stats;

static esp_err_t real_init_playback(void *ctx)
{
    (void)ctx;
    const board_audio_config_t config = {
        .profile = BOARD_AUDIO_PROFILE_PLAYBACK_ONLY,
        .probe_m5pm1 = true,
        .require_audio_power_enable = true,
    };
    return board_audio_init(&config);
}

static esp_err_t real_write_i16(const int16_t *src, size_t samples, size_t *samples_written, uint32_t timeout_ms, void *ctx)
{
    (void)ctx;
    size_t bytes_written = 0;
    esp_err_t err = board_i2s_write(src, samples * sizeof(int16_t), &bytes_written, timeout_ms);
    if (samples_written != NULL) {
        *samples_written = bytes_written / sizeof(int16_t);
    }
    return err;
}

static esp_err_t real_set_volume_percent(uint8_t volume_percent, void *ctx)
{
    (void)ctx;
    const uint8_t es8311_volume = (uint8_t)((volume_percent * 0xBFu) / 100u);
    return es8311_set_dac_volume(BOARD_I2C_PORT, BOARD_ES8311_ADDR, es8311_volume);
}

static esp_err_t real_set_mute(bool muted, void *ctx)
{
    (void)ctx;
    return es8311_mute(BOARD_I2C_PORT, BOARD_ES8311_ADDR, muted);
}

static esp_err_t real_deinit(void *ctx)
{
    (void)ctx;
    return board_audio_deinit();
}

esp_err_t uac_speaker_sink_init(const uac_speaker_sink_ops_t *ops)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ops == NULL) {
        s_ops = (uac_speaker_sink_ops_t){
            .init_playback = real_init_playback,
            .write_i16 = real_write_i16,
            .set_volume_percent = real_set_volume_percent,
            .set_mute = real_set_mute,
            .deinit = real_deinit,
            .ctx = NULL,
        };
    } else {
        if (ops->init_playback == NULL || ops->write_i16 == NULL ||
            ops->set_volume_percent == NULL || ops->set_mute == NULL || ops->deinit == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        s_ops = *ops;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.volume_percent = UAC_AUDIO_SAFE_MAX_VOLUME_PERCENT;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t uac_speaker_sink_start(void)
{
    if (!s_initialized || s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = s_ops.init_playback(s_ops.ctx);
    if (err != ESP_OK) {
        return err;
    }
    s_started = true;
    s_stats.starts++;
    return ESP_OK;
}

esp_err_t uac_speaker_sink_write_i16(const int16_t *src, size_t samples, size_t *samples_written, uint32_t timeout_ms)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (src == NULL || samples == 0 || samples_written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_stats.write_calls++;
    esp_err_t err = s_ops.write_i16(src, samples, samples_written, timeout_ms, s_ops.ctx);
    if (err == ESP_OK) {
        s_stats.samples_written += (uint32_t)*samples_written;
    } else {
        s_stats.write_failures++;
    }
    return err;
}

esp_err_t uac_speaker_sink_set_volume_percent(uint8_t volume_percent)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t clamped = uac_audio_clamp_volume_percent(volume_percent);
    esp_err_t err = s_ops.set_volume_percent(clamped, s_ops.ctx);
    if (err == ESP_OK) {
        s_stats.volume_percent = clamped;
    }
    return err;
}

esp_err_t uac_speaker_sink_set_mute(bool muted)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = s_ops.set_mute(muted, s_ops.ctx);
    if (err == ESP_OK) {
        s_stats.muted = muted;
    }
    return err;
}

esp_err_t uac_speaker_sink_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ESP_OK;
    if (s_started) {
        err = s_ops.deinit(s_ops.ctx);
        s_stats.stops++;
    }
    s_started = false;
    return err;
}

uac_speaker_sink_stats_t uac_speaker_sink_get_stats(void)
{
    return s_stats;
}

void uac_speaker_sink_reset_for_test(void)
{
    memset(&s_ops, 0, sizeof(s_ops));
    memset(&s_stats, 0, sizeof(s_stats));
    s_initialized = false;
    s_started = false;
}
