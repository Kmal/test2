#include "board_i2s.h"

#include "board_audio_clock.h"
#include "board_sticks3.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stddef.h>

#ifndef I2S_PIN_NO_CHANGE
#define I2S_PIN_NO_CHANGE (-1)
#endif

static const char *TAG = "BOARD_I2S";

esp_err_t board_i2s_init_profile(board_audio_profile_t profile)
{
    const board_audio_clock_profile_t *clock = board_audio_clock_get_profile();
    i2s_mode_t mode = I2S_MODE_MASTER | I2S_MODE_RX;
    if (profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX) {
        mode |= I2S_MODE_TX;
    } else if (profile != BOARD_AUDIO_PROFILE_CAPTURE_ONLY) {
        return ESP_ERR_INVALID_ARG;
    }

    i2s_config_t i2s_config = {
        .mode = mode,
        .sample_rate = clock->sample_rate_hz,
        .bits_per_sample = BOARD_I2S_BITS,
        .channel_format = BOARD_I2S_CHANNEL_FMT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = clock->mclk_hz,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = BOARD_I2S_BCK_IO,
        .ws_io_num = BOARD_I2S_WS_IO,
        .data_out_num = (profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX) ? BOARD_I2S_DO_IO : I2S_PIN_NO_CHANGE,
        .data_in_num = BOARD_I2S_DI_IO,
        .mck_io_num = BOARD_I2S_MCLK_IO,
    };

    esp_err_t err = i2s_driver_install(BOARD_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2s_set_pin(BOARD_I2S_PORT, &pin_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "I2S %s profile ready: Fs=%d MCLK=%d",
                 profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX ? "full-duplex" : "capture-only",
                 clock->sample_rate_hz, clock->mclk_hz);
    } else {
        ESP_LOGE(TAG, "I2S pin config failed: %s", esp_err_to_name(err));
    }
    return err;
}
