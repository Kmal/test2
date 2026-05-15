#include "board_audio_power.h"

#include "board_sticks3.h"
#include "m5pm1.h"

#include "esp_log.h"

#define BOARD_M5PM1_SPK_AMP_GPIO 3u
#define M5PM1_GPIO_FUNC_GPIO 0u

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

esp_err_t board_speaker_amp_set(i2c_port_t port, bool enable)
{
    esp_err_t err = m5pm1_gpio_set_function(port, BOARD_M5PM1_ADDR, BOARD_M5PM1_SPK_AMP_GPIO, M5PM1_GPIO_FUNC_GPIO);
    if (err == ESP_OK) {
        err = m5pm1_gpio_set_mode(port, BOARD_M5PM1_ADDR, BOARD_M5PM1_SPK_AMP_GPIO, true);
    }
    if (err == ESP_OK) {
        err = m5pm1_gpio_set_drive(port, BOARD_M5PM1_ADDR, BOARD_M5PM1_SPK_AMP_GPIO, true);
    }
    if (err == ESP_OK) {
        err = m5pm1_gpio_set_output(port, BOARD_M5PM1_ADDR, BOARD_M5PM1_SPK_AMP_GPIO, enable);
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "M5PM1 PYG3 speaker amplifier %s", enable ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG, "M5PM1 PYG3 speaker amplifier %s failed: %s",
                 enable ? "enable" : "disable", esp_err_to_name(err));
    }
    return err;
}

esp_err_t board_speaker_amp_pulse(i2c_port_t port, uint8_t pulse_count)
{
    (void)port;
    (void)pulse_count;
    /*
     * The source-backed StickS3 speaker action uses the documented PYG3
     * amplifier enable/disable GPIO sequence. AW8737 pulse/gain modes are a
     * separate M5PM1 register feature and are not needed for the bounded tone
     * action, so keep this legacy helper fail-closed until a feature explicitly
     * requires source-backed gain-pulse control.
     */
    ESP_LOGW(TAG, "speaker amplifier pulse/gain control is not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}
