#include "board_i2s.h"

#include "board_audio_clock.h"
#include "board_sticks3.h"

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const char *TAG = "BOARD_I2S";

static i2s_chan_handle_t s_rx_handle;
static i2s_chan_handle_t s_tx_handle;
static board_audio_profile_t s_active_profile;
static bool s_i2s_ready;


static void board_i2s_cleanup_partial(bool rx_enabled, bool tx_enabled)
{
    if (s_tx_handle != NULL) {
        if (tx_enabled) {
            (void)i2s_channel_disable(s_tx_handle);
        }
        (void)i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
    }
    if (s_rx_handle != NULL) {
        if (rx_enabled) {
            (void)i2s_channel_disable(s_rx_handle);
        }
        (void)i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }
    s_i2s_ready = false;
}

static esp_err_t board_i2s_enable_channel(i2s_chan_handle_t handle, const char *name)
{
    esp_err_t err = i2s_channel_enable(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S %s channel enable failed: %s", name, esp_err_to_name(err));
    }
    return err;
}

esp_err_t board_i2s_init_profile(board_audio_profile_t profile)
{
    if (profile != BOARD_AUDIO_PROFILE_CAPTURE_ONLY && profile != BOARD_AUDIO_PROFILE_FULL_DUPLEX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_i2s_ready) {
        return (s_active_profile == profile) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    s_rx_handle = NULL;
    s_tx_handle = NULL;
    bool rx_enabled = false;
    bool tx_enabled = false;

    const board_audio_clock_profile_t *clock = board_audio_clock_get_profile();
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BOARD_I2S_PORT, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg,
                                    (profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX) ? &s_tx_handle : NULL,
                                    &s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel allocation failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(clock->sample_rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK_IO,
            .bclk = BOARD_I2S_BCK_IO,
            .ws = BOARD_I2S_WS_IO,
            .dout = (profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX) ? BOARD_I2S_DO_IO : I2S_GPIO_UNUSED,
            .din = BOARD_I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_768;
    /*
     * The StickS3 ES8311 clock profile is 16-bit mono payload in a 32-bit I2S
     * slot: Fs=16 kHz, BCLK=512 kHz, MCLK=12.288 MHz.  The ESP-IDF default
     * mono helper derives BCLK from the 16-bit payload width, which can leave
     * the codec without the expected serial clock and cause RX timeouts.
     */
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    err = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX standard-mode init failed: %s", esp_err_to_name(err));
        board_i2s_cleanup_partial(rx_enabled, tx_enabled);
        return err;
    }
    if (s_tx_handle != NULL) {
        err = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S TX standard-mode init failed: %s", esp_err_to_name(err));
            board_i2s_cleanup_partial(rx_enabled, tx_enabled);
            return err;
        }
    }

    err = board_i2s_enable_channel(s_rx_handle, "RX");
    if (err != ESP_OK) {
        board_i2s_cleanup_partial(rx_enabled, tx_enabled);
        return err;
    }
    rx_enabled = true;
    if (s_tx_handle != NULL) {
        err = board_i2s_enable_channel(s_tx_handle, "TX");
        if (err != ESP_OK) {
            board_i2s_cleanup_partial(rx_enabled, tx_enabled);
            return err;
        }
        tx_enabled = true;
    }

    s_active_profile = profile;
    s_i2s_ready = true;
    ESP_LOGI(TAG, "I2S standard %s profile ready: Fs=%d MCLK=%d BCLK=%d LRCK=%d bits=%d channels=%d slot_bits=%d",
             profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX ? "full-duplex" : "capture-only",
             clock->sample_rate_hz, clock->mclk_hz, clock->bclk_hz, clock->lrck_hz,
             clock->bits_per_sample, clock->channels, (int)std_cfg.slot_cfg.slot_bit_width);
    ESP_LOGI(TAG, "I2S pins: MCLK=GPIO%d BCLK=GPIO%d WS=GPIO%d DIN=GPIO%d DOUT=%s",
             BOARD_I2S_MCLK_IO, BOARD_I2S_BCK_IO, BOARD_I2S_WS_IO, BOARD_I2S_DI_IO,
             profile == BOARD_AUDIO_PROFILE_FULL_DUPLEX ? "GPIO14" : "unused");
    return ESP_OK;
}

esp_err_t board_i2s_read(void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    if (!s_i2s_ready || s_rx_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(s_rx_handle, dest, size, bytes_read, timeout_ms);
}

esp_err_t board_i2s_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms)
{
    if (!s_i2s_ready || s_tx_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_write(s_tx_handle, src, size, bytes_written, timeout_ms);
}
