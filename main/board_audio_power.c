#include "board_audio_power.h"

#include "esp_log.h"

static const char *TAG = "BOARD_PWR";

esp_err_t board_audio_power_enable(i2c_port_t port)
{
    (void)port;
    /*
     * Source gate: StickS3 documents M5PM1 G2 as PYG2_L3B_EN, but this
     * repository does not yet contain enough source-backed evidence for the
     * enable polarity and safe setup order. Do not guess register writes.
     */
    ESP_LOGW(TAG, "M5PM1 L3B audio power enable is blocked pending source-backed polarity/sequence evidence");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t board_speaker_amp_pulse(i2c_port_t port, uint8_t pulse_count)
{
    (void)port;
    (void)pulse_count;
    /*
     * Speaker amplifier control must remain explicit and source-backed. The
     * no-transport firmware never calls this path.
     */
    ESP_LOGW(TAG, "speaker amplifier pulse is blocked pending source-backed M5PM1/AW8737 sequence evidence");
    return ESP_ERR_NOT_SUPPORTED;
}
