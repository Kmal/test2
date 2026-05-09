/*
 * Basic ES8311 codec initialisation.  The purpose of this file is
 * to provide a small set of helper functions sufficient for
 * recording and playback on the M5Stack Stick S3.  It resets the
 * ES8311 and configures it for I2S slave mode, 16-bit samples and
 * the desired sampling rate.  The ESP32-S3 I2S peripheral is the clock
 * master and drives MCLK/BCLK/LRCLK.  More detailed control (e.g. mic
 * gain, de-emphasis filters, power management) can be achieved
 * using the full Espressif codec framework.
 */

#include "es8311.h"
#include "register_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stddef.h>

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#ifndef ES8311_VERIFY_REGISTERS
#define ES8311_VERIFY_REGISTERS 1
#endif

#define ES8311_I2C_TIMEOUT_MS           1000

#define ES8311_REG_RESET                0x00
#define ES8311_REG_POWER_1              0x01
#define ES8311_REG_POWER_2              0x02
#define ES8311_REG_POWER_3              0x03
#define ES8311_REG_CLK_MANAGER          0x0B
#define ES8311_REG_I2S_MODE             0x0D
#define ES8311_REG_ADC_FORMAT           0x11
#define ES8311_REG_DAC_FORMAT           0x12
#define ES8311_REG_MIC_GAIN             0x14
#define ES8311_REG_DAC_POWER            0x15
#define ES8311_REG_ADC_DAC_POWER        0x16
#define ES8311_REG_ADC_POWER            0x17
#define ES8311_REG_DAC_MUTE             0x31
#define ES8311_REG_DAC_VOLUME           0x32

#define ES8311_RESET_ASSERT             0x1F
#define ES8311_RESET_RELEASE            0x00
#define ES8311_POWER1_UP                0x30
#define ES8311_POWER1_DOWN              0x00
#define ES8311_POWER2_ADC_PGA_UP        0x10
#define ES8311_POWER2_ADC_PGA_DOWN      0x00
#define ES8311_POWER3_UP                0x00
#define ES8311_CLK_12M288_TO_16K        0x1B
#define ES8311_CLK_12M288_TO_8K         0x3B
#define ES8311_CLK_12M288_TO_48K        0x00
#define ES8311_I2S_SLAVE_MODE           0x00
#define ES8311_FORMAT_16BIT             0x10
#define ES8311_MIC_GAIN_MASK            0x0F
#define ES8311_DEFAULT_MIC_GAIN         ES8311_MIC_GAIN_30DB
#define ES8311_DAC_POWER_UP             0x06
#define ES8311_DAC_POWER_DOWN           0x00
#define ES8311_ADC_DAC_POWER_UP         0x30
#define ES8311_ADC_DAC_POWER_DOWN       0x00
#define ES8311_ADC_POWER_UP             0x30
#define ES8311_ADC_POWER_DOWN           0x00
#define ES8311_DAC_MUTE_BIT             BIT(5)
#define ES8311_DAC_DEFAULT_VOLUME       0xBF

static const char *TAG_CODEC = "ES8311";

static esp_err_t es8311_first_error(esp_err_t current, esp_err_t next)
{
    return (current != ESP_OK) ? current : next;
}

static esp_err_t es8311_clock_divider_for_rate(int sample_rate, uint8_t *clk_div)
{
    if (clk_div == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (sample_rate) {
    case 8000:
        *clk_div = ES8311_CLK_12M288_TO_8K;
        return ESP_OK;
    case 16000:
        *clk_div = ES8311_CLK_12M288_TO_16K;
        return ESP_OK;
    case 48000:
        *clk_div = ES8311_CLK_12M288_TO_48K;
        return ESP_OK;
    default:
        ESP_LOGE(TAG_CODEC, "Unsupported ES8311 sample rate %d Hz", sample_rate);
        return ESP_ERR_NOT_SUPPORTED;
    }
}

// Write a single 8-bit value to a codec register via the shared StickS3 I2C bus.
static esp_err_t es8311_write_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    esp_err_t err = register_bus_write_u8(i2c_num, i2c_addr, reg, val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C write reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

// Read a single 8-bit value from a codec register via the shared StickS3 I2C bus.
static esp_err_t es8311_read_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t *val)
{
    esp_err_t err = register_bus_read_u8(i2c_num, i2c_addr, reg, val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C read reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t es8311_write_reg_checked(i2c_port_t i2c_num, uint8_t i2c_addr,
                                          uint8_t reg, uint8_t val, bool verify)
{
    esp_err_t err = es8311_write_reg(i2c_num, i2c_addr, reg, val);
    if (err != ESP_OK || !verify || !ES8311_VERIFY_REGISTERS) {
        return err;
    }

    /*
     * Conservative verification policy: only registers used by the existing
     * project sequence are read back, and future source-audited ES8311 tables
     * must classify any volatile/write-only registers before adding checks.
     */
    uint8_t readback = 0;
    err = es8311_read_reg(i2c_num, i2c_addr, reg, &readback);
    if (err != ESP_OK) {
        return err;
    }

    if (readback != val) {
        ESP_LOGE(TAG_CODEC, "Register verify mismatch reg 0x%02x: wrote 0x%02x read 0x%02x",
                 reg, val, readback);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t es8311_update_reg_bits(i2c_port_t i2c_num, uint8_t i2c_addr,
                                        uint8_t reg, uint8_t mask, uint8_t val,
                                        bool verify)
{
    uint8_t current = 0;
    esp_err_t err = es8311_read_reg(i2c_num, i2c_addr, reg, &current);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t next = (current & ~mask) | (val & mask);
    return es8311_write_reg_checked(i2c_num, i2c_addr, reg, next, verify);
}

/*
 * The sequence below is the project's current minimal ES8311 programming
 * sequence for StickS3. It supports an ADC-only profile for default capture boot
 * and a full-duplex compatibility profile for quarantined legacy code. Future
 * changes must classify ES8311 registers as exact-readback-safe, masked,
 * volatile, or write-only before adding stricter hardware verification.
 */
esp_err_t es8311_init_profile(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port,
                              es8311_profile_t profile, int sample_rate)
{
    (void)i2s_port; // The codec is configured over I2C; I2S is configured by the caller.

    if (profile != ES8311_PROFILE_ADC_ONLY && profile != ES8311_PROFILE_FULL_DUPLEX) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t clk_div = 0;
    esp_err_t ret = es8311_clock_divider_for_rate(sample_rate, &clk_div);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG_CODEC, "Initialising ES8311 at I2C 0x%02x (sample_rate=%d, profile=%s)",
             i2c_addr, sample_rate,
             profile == ES8311_PROFILE_ADC_ONLY ? "adc-only" : "full-duplex");

    ret = es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_ASSERT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed during reset assert");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ret = ESP_OK;
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_RELEASE));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_POWER_1, ES8311_POWER1_UP));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_POWER_2, ES8311_POWER2_ADC_PGA_UP));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_POWER_3, ES8311_POWER3_UP));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER, clk_div));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_FORMAT, ES8311_FORMAT_16BIT));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_DAC_FORMAT, ES8311_FORMAT_16BIT));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_I2S_MODE, ES8311_I2S_SLAVE_MODE));
    ret = es8311_first_error(ret, es8311_set_mic_gain(i2c_num, i2c_addr, ES8311_DEFAULT_MIC_GAIN));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_DAC_POWER, ES8311_ADC_DAC_POWER_UP));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_POWER, ES8311_ADC_POWER_UP));

    if (profile == ES8311_PROFILE_FULL_DUPLEX) {
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_UP));
        ret = es8311_first_error(ret, es8311_set_dac_volume(i2c_num, i2c_addr, ES8311_DAC_DEFAULT_VOLUME));
        ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));
    } else {
        /*
         * StickS3 Bluetooth LE GATT PCM mode is capture-only. Keep the DAC path muted
         * and avoid powering it up until a source-backed monitoring/playback
         * feature explicitly requests the speaker path.
         */
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_DOWN));
        ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, true));
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 initialised");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate)
{
    return es8311_init_profile(i2c_num, i2c_addr, i2s_port, ES8311_PROFILE_FULL_DUPLEX, sample_rate);
}

esp_err_t es8311_set_mic_gain(i2c_port_t i2c_num, uint8_t i2c_addr, es8311_mic_gain_t gain)
{
    if (gain < ES8311_MIC_GAIN_0DB || gain > ES8311_MIC_GAIN_30DB) {
        return ESP_ERR_INVALID_ARG;
    }

    return es8311_update_reg_bits(i2c_num, i2c_addr, ES8311_REG_MIC_GAIN,
                                  ES8311_MIC_GAIN_MASK, (uint8_t)gain, true);
}

esp_err_t es8311_set_dac_volume(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t volume)
{
    return es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_VOLUME, volume, true);
}

esp_err_t es8311_mute(i2c_port_t i2c_num, uint8_t i2c_addr, bool mute)
{
    uint8_t value = mute ? ES8311_DAC_MUTE_BIT : 0;
    return es8311_update_reg_bits(i2c_num, i2c_addr, ES8311_REG_DAC_MUTE,
                                  ES8311_DAC_MUTE_BIT, value, true);
}

esp_err_t es8311_power_down(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    esp_err_t ret = ESP_OK;
    ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_ADC_DAC_POWER,
                                                           ES8311_ADC_DAC_POWER_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_ADC_POWER,
                                                           ES8311_ADC_POWER_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_DAC_POWER,
                                                           ES8311_DAC_POWER_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_POWER_2,
                                                           ES8311_POWER2_ADC_PGA_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_POWER_1,
                                                           ES8311_POWER1_DOWN, true));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered down");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power down failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t es8311_power_up(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    esp_err_t ret = ESP_OK;
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_RESET,
                                                           ES8311_RESET_RELEASE, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_POWER_1,
                                                           ES8311_POWER1_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_POWER_2,
                                                           ES8311_POWER2_ADC_PGA_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_POWER_3,
                                                           ES8311_POWER3_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_DAC_POWER,
                                                           ES8311_DAC_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_ADC_DAC_POWER,
                                                           ES8311_ADC_DAC_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_ADC_POWER,
                                                           ES8311_ADC_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered up");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power up failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
