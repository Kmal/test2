#include "uac_mic_source.h"

#include "board_audio.h"
#include "board_i2s.h"

#include <string.h>

static uac_mic_source_ops_t s_ops;
static bool s_initialized;
static bool s_started;
static uac_mic_source_stats_t s_stats;

static esp_err_t real_init_capture(void *ctx)
{
    (void)ctx;
    const board_audio_config_t config = {
        .profile = BOARD_AUDIO_PROFILE_CAPTURE_ONLY,
        .probe_m5pm1 = true,
        .require_audio_power_enable = false,
    };
    return board_audio_init(&config);
}

static esp_err_t real_read_i16(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms, void *ctx)
{
    (void)ctx;
    return board_i2s_read_mono_i16(dest, max_samples, samples_read, timeout_ms);
}

static esp_err_t real_deinit(void *ctx)
{
    (void)ctx;
    return board_audio_deinit();
}

esp_err_t uac_mic_source_init(const uac_mic_source_ops_t *ops)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ops == NULL) {
        s_ops = (uac_mic_source_ops_t){
            .init_capture = real_init_capture,
            .read_i16 = real_read_i16,
            .deinit = real_deinit,
            .ctx = NULL,
        };
    } else {
        if (ops->init_capture == NULL || ops->read_i16 == NULL || ops->deinit == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        s_ops = *ops;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_initialized = true;
    return ESP_OK;
}

esp_err_t uac_mic_source_start(void)
{
    if (!s_initialized || s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = s_ops.init_capture(s_ops.ctx);
    if (err != ESP_OK) {
        return err;
    }
    s_started = true;
    s_stats.starts++;
    return ESP_OK;
}

esp_err_t uac_mic_source_read_i16(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (dest == NULL || max_samples == 0 || samples_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_stats.read_calls++;
    esp_err_t err = s_ops.read_i16(dest, max_samples, samples_read, timeout_ms, s_ops.ctx);
    if (err == ESP_OK) {
        s_stats.samples_read += (uint32_t)*samples_read;
    } else {
        s_stats.read_failures++;
    }
    return err;
}

esp_err_t uac_mic_source_stop(void)
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

uac_mic_source_stats_t uac_mic_source_get_stats(void)
{
    return s_stats;
}

void uac_mic_source_reset_for_test(void)
{
    memset(&s_ops, 0, sizeof(s_ops));
    memset(&s_stats, 0, sizeof(s_stats));
    s_initialized = false;
    s_started = false;
}
