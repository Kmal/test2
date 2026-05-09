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
    es8311_profile_t codec_profile = (profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX) ?
                                     ES8311_PROFILE_FULL_DUPLEX : ES8311_PROFILE_ADC_ONLY;
    return es8311_init_profile(BOARD_I2C_PORT, BOARD_ES8311_ADDR, BOARD_I2S_PORT,
                               codec_profile, BOARD_I2S_SAMPLE_RATE);
}

static esp_err_t real_cleanup_on_failure(esp_err_t cause, void *ctx)
{
    (void)ctx;
    (void)cause;
    /*
     * Current failure policy is diagnostic hard-fail by the caller. No source-
     * backed L3B disable sequence exists yet, so cleanup avoids guessed power
     * writes and only reports the cause.
     */
    ESP_LOGE(TAG, "audio init failed before completion: %s", esp_err_to_name(cause));
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
        config->profile != BOARD_AUDIO_PROFILE_FULL_DUPLEX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->probe_m5pm1 && ops->m5pm1_probe == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->require_audio_power_enable && ops->audio_power_enable == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ops->i2c_init(ops->ctx);
    if (err != ESP_OK) {
        return finish_or_cleanup(err, ops);
    }

    if (config->probe_m5pm1) {
        err = ops->m5pm1_probe(ops->ctx);
        if (err != ESP_OK) {
            return finish_or_cleanup(err, ops);
        }
    }

    if (config->require_audio_power_enable) {
        err = ops->audio_power_enable(ops->ctx);
        if (err != ESP_OK) {
            return finish_or_cleanup(err, ops);
        }
    }

    err = ops->i2s_init_profile(config->profile, ops->ctx);
    if (err != ESP_OK) {
        return finish_or_cleanup(err, ops);
    }

    err = ops->es8311_init_profile(config->profile, ops->ctx);
    if (err != ESP_OK) {
        return finish_or_cleanup(err, ops);
    }

    ESP_LOGI(TAG, "audio profile initialised: %s",
             config->profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX ? "full-duplex" : "capture-only");
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
