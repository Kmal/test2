#include "board_i2s.h"
#include "board_audio_clock.h"
#include "driver/i2s_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

static int32_t s_raw[256];
static size_t s_raw_bytes;

const board_audio_clock_profile_t *board_audio_clock_get_profile(void)
{
    static const board_audio_clock_profile_t profile = {
        .sample_rate_hz = 16000,
        .mclk_hz = 12288000,
        .bclk_hz = 512000,
        .lrck_hz = 16000,
        .bits_per_sample = 16,
        .channels = 1,
    };
    return &profile;
}

esp_err_t i2s_new_channel(const i2s_chan_config_t *chan_cfg, i2s_chan_handle_t *tx_handle, i2s_chan_handle_t *rx_handle)
{
    (void)chan_cfg;
    if (tx_handle != NULL) { *tx_handle = (i2s_chan_handle_t)2; }
    if (rx_handle != NULL) { *rx_handle = (i2s_chan_handle_t)1; }
    return ESP_OK;
}
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t handle, const i2s_std_config_t *std_cfg) { (void)handle; (void)std_cfg; return ESP_OK; }
esp_err_t i2s_channel_enable(i2s_chan_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t i2s_channel_disable(i2s_chan_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t i2s_del_channel(i2s_chan_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t i2s_channel_write(i2s_chan_handle_t handle, const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)handle; (void)src; (void)timeout_ms;
    if (bytes_written != NULL) { *bytes_written = size; }
    return ESP_OK;
}
esp_err_t i2s_channel_read(i2s_chan_handle_t handle, void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)handle; (void)timeout_ms;
    size_t to_copy = s_raw_bytes < size ? s_raw_bytes : size;
    memcpy(dest, s_raw, to_copy);
    if (bytes_read != NULL) { *bytes_read = to_copy; }
    return ESP_OK;
}

static void set_raw(const int32_t *raw, size_t words)
{
    memcpy(s_raw, raw, words * sizeof(raw[0]));
    s_raw_bytes = words * sizeof(raw[0]);
}

static void test_decode_samples(void)
{
    ASSERT_EQ(ESP_OK, board_i2s_init_profile(BOARD_AUDIO_PROFILE_CAPTURE_ONLY));
    int32_t raw[] = {
        ((int32_t)1234) << 16,
        (int32_t)((uint32_t)0xFB2E0000u),
        0,
        ((int32_t)32767) << 16,
        (int32_t)((uint32_t)0x80000000u),
    };
    int16_t samples[8];
    size_t read = 0;
    set_raw(raw, sizeof(raw) / sizeof(raw[0]));
    ASSERT_EQ(ESP_OK, board_i2s_read_mono_i16(samples, 8, &read, 20));
    ASSERT_EQ(5, read);
    ASSERT_EQ(1234, samples[0]);
    ASSERT_EQ(-1234, samples[1]);
    ASSERT_EQ(0, samples[2]);
    ASSERT_EQ(32767, samples[3]);
    ASSERT_EQ(-32768, samples[4]);
}

static void test_partial_raw_read_handling(void)
{
    int32_t raw[] = { ((int32_t)42) << 16, (int32_t)((uint32_t)0xFFF90000u) };
    int16_t samples[1];
    size_t read = 0;
    set_raw(raw, sizeof(raw) / sizeof(raw[0]));
    ASSERT_EQ(ESP_OK, board_i2s_read_mono_i16(samples, 1, &read, 20));
    ASSERT_EQ(1, read);
    ASSERT_EQ(42, samples[0]);
}

int main(void)
{
    test_decode_samples();
    test_partial_raw_read_handling();
    puts("board_i2s_decode tests passed");
    return 0;
}
