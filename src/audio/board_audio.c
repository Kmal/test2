#include "board_audio.h"

#include "board_audio_power.h"
#include "board_i2c.h"
#include "board_i2s.h"
#include "board_sticks3.h"
#include "es8311.h"
#include "m5pm1.h"
#include "esp_log.h"

#include <stddef.h>

static const char *TAG = "BOARD_AUDIO";

static esp_err_t real_i2c_init(void *ctx)
{
    (void)ctx;
    return board_i2c_init();
}

static esp_err_t real_m5pm1_probe(void *ctx)
{
    (void)ctx;
    m5pm1_identity_t identity = {0};
    return m5pm1_probe(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &identity);
}

static esp_err_t real_audio_power_enable(void *ctx)
{
    (void)ctx;
    return board_audio_power_enable(BOARD_I2C_PORT);
}

static esp_err_t real_i2s_init_profile(board_audio_profile_t profile, void *ctx)
{
    (void)ctx;
    return board_i2s_init_profile(profile);
}

static esp_err_t real_es8311_init_profile(board_audio_profile_t profile, void *ctx)
{
    (void)ctx;
    es8311_profile_t codec_profile = ES8311_PROFILE_ADC_ONLY;
    if (profile == BOARD_AUDIO_PROFILE_PLAYBACK_ONLY) {
        codec_profile = ES8311_PROFILE_DAC_ONLY;
    } else if (profile == BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER) {
        codec_profile = ES8311_PROFILE_ADC_DAC;
    }
    return es8311_init_profile(BOARD_I2C_PORT, BOARD_ES8311_ADDR, BOARD_I2S_PORT,
                               codec_profile, BOARD_I2S_SAMPLE_RATE);
}

static esp_err_t real_cleanup_on_failure(esp_err_t cause, void *ctx)
{
    (void)ctx;
    (void)cause;
    /*
     * Current failure policy is diagnostic hard-fail by the caller. No source-
     * backed L3B disable sequence exists yet, so cleanup avoids guessed rail
     * writes, but any codec/I2S resources already touched by the init sequence
     * are returned to a safe idle state.
     */
    ESP_LOGE(TAG, "audio init failed before completion: %s", esp_err_to_name(cause));
    (void)es8311_power_down(BOARD_I2C_PORT, BOARD_ES8311_ADDR);
    (void)board_i2s_deinit();
    return ESP_OK;
}

static esp_err_t finish_or_cleanup(esp_err_t err, const board_audio_ops_t *ops)
{
    if (err == ESP_OK) {
        return ESP_OK;
    }
    if (ops->cleanup_on_failure != NULL) {
        esp_err_t cleanup_err = ops->cleanup_on_failure(err, ops->ctx);
        if (cleanup_err != ESP_OK) {
            return cleanup_err;
        }
    }
    return err;
}

esp_err_t board_audio_init_with_ops(const board_audio_config_t *config, const board_audio_ops_t *ops)
{
    if (config == NULL || ops == NULL ||
        ops->i2c_init == NULL || ops->i2s_init_profile == NULL || ops->es8311_init_profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->profile != BOARD_AUDIO_PROFILE_CAPTURE_ONLY &&
        config->profile != BOARD_AUDIO_PROFILE_PLAYBACK_ONLY &&
        config->profile != BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->probe_m5pm1 && ops->m5pm1_probe == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->require_audio_power_enable && ops->audio_power_enable == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "audio init start: profile=%s probe_m5pm1=%s require_power=%s",
             config->profile == BOARD_AUDIO_PROFILE_PLAYBACK_ONLY ? "playback-only" :
             (config->profile == BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER ? "simultaneous-mic-speaker" : "capture-only"),
             config->probe_m5pm1 ? "yes" : "no",
             config->require_audio_power_enable ? "yes" : "no");

    ESP_LOGI(TAG, "audio init step: shared I2C bus");
    esp_err_t err = ops->i2c_init(ops->ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "audio init step failed: shared I2C bus: %s", esp_err_to_name(err));
        return finish_or_cleanup(err, ops);
    }
    ESP_LOGI(TAG, "audio init step ok: shared I2C bus");

    if (config->probe_m5pm1) {
        ESP_LOGI(TAG, "audio init step: M5PM1 probe");
        err = ops->m5pm1_probe(ops->ctx);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "audio init step failed: M5PM1 probe: %s", esp_err_to_name(err));
            return finish_or_cleanup(err, ops);
        }
        ESP_LOGI(TAG, "audio init step ok: M5PM1 probe");
    }

    if (config->require_audio_power_enable) {
        ESP_LOGI(TAG, "audio init step: audio power rail enable");
        err = ops->audio_power_enable(ops->ctx);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "audio init step failed: audio power rail enable: %s", esp_err_to_name(err));
            return finish_or_cleanup(err, ops);
        }
        ESP_LOGI(TAG, "audio init step ok: audio power rail enable");
    }

    ESP_LOGI(TAG, "audio init step: I2S clocks/channel setup");
    err = ops->i2s_init_profile(config->profile, ops->ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "audio init step failed: I2S clocks/channel setup: %s", esp_err_to_name(err));
        return finish_or_cleanup(err, ops);
    }
    ESP_LOGI(TAG, "audio init step ok: I2S clocks/channel setup");

    ESP_LOGI(TAG, "audio init step: ES8311 codec setup");
    err = ops->es8311_init_profile(config->profile, ops->ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "audio init step failed: ES8311 codec setup: %s", esp_err_to_name(err));
        return finish_or_cleanup(err, ops);
    }
    ESP_LOGI(TAG, "audio init step ok: ES8311 codec setup");

    ESP_LOGI(TAG, "audio profile initialised: %s",
             config->profile == BOARD_AUDIO_PROFILE_PLAYBACK_ONLY ? "playback-only" :
             (config->profile == BOARD_AUDIO_PROFILE_SIMULTANEOUS_MIC_SPEAKER ? "simultaneous-mic-speaker" : "capture-only"));
    return ESP_OK;
}

esp_err_t board_audio_init(const board_audio_config_t *config)
{
    static const board_audio_ops_t real_ops = {
        .i2c_init = real_i2c_init,
        .m5pm1_probe = real_m5pm1_probe,
        .audio_power_enable = real_audio_power_enable,
        .i2s_init_profile = real_i2s_init_profile,
        .es8311_init_profile = real_es8311_init_profile,
        .cleanup_on_failure = real_cleanup_on_failure,
        .ctx = NULL,
    };
    return board_audio_init_with_ops(config, &real_ops);
}

esp_err_t board_audio_deinit(void)
{
    esp_err_t err = es8311_power_down(BOARD_I2C_PORT, BOARD_ES8311_ADDR);
    esp_err_t i2s_err = board_i2s_deinit();
    if (err == ESP_OK) {
        err = i2s_err;
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "audio resources released");
    } else {
        ESP_LOGW(TAG, "audio resource release had errors: %s", esp_err_to_name(err));
    }
    return err;
}
