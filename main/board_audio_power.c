#include "board_audio_power.h"

#include "board_sticks3.h"
#include "m5pm1.h"

#include "esp_log.h"

static const char *TAG = "BOARD_PWR";

esp_err_t board_audio_power_enable(i2c_port_t port)
{
    esp_err_t err = m5pm1_enable_l3b_power(port, BOARD_M5PM1_ADDR);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "M5PM1 L3B rail enabled for StickS3 ES8311 audio power");
    } else {
        ESP_LOGE(TAG, "M5PM1 L3B audio power enable failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t board_speaker_amp_pulse(i2c_port_t port, uint8_t pulse_count)
{
    (void)port;
    (void)pulse_count;
    /*
     * Speaker amplifier control must remain explicit and source-backed. The
     * default control firmware never calls this path.
     */
    ESP_LOGW(TAG, "speaker amplifier pulse is blocked pending source-backed M5PM1/AW8737 sequence evidence");
    return ESP_ERR_NOT_SUPPORTED;
}
