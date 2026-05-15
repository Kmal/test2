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
#include <stdio.h>

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#ifndef ES8311_VERIFY_REGISTERS
#define ES8311_VERIFY_REGISTERS 1
#endif

#define ES8311_I2C_TIMEOUT_MS           1000

#define ES8311_REG_RESET                0x00
#define ES8311_REG_CLK_MANAGER_1        0x01
#define ES8311_REG_CLK_MANAGER_2        0x02
#define ES8311_REG_CLK_MANAGER_3        0x03
#define ES8311_REG_CLK_MANAGER_4        0x04
#define ES8311_REG_CLK_MANAGER_5        0x05
#define ES8311_REG_CLK_MANAGER_6        0x06
#define ES8311_REG_CLK_MANAGER_7        0x07
#define ES8311_REG_CLK_MANAGER_8        0x08
#define ES8311_REG_SDP_DAC              0x09
#define ES8311_REG_SDP_ADC              0x0A
#define ES8311_REG_SYSTEM_0D            0x0D
#define ES8311_REG_SYSTEM_0E            0x0E
#define ES8311_REG_SYSTEM_12            0x12
#define ES8311_REG_SYSTEM_13            0x13
#define ES8311_REG_MIC_GAIN             0x14
#define ES8311_REG_ADC_RAMP             0x15
#define ES8311_REG_ADC_SCALE            0x16
#define ES8311_REG_ADC_VOLUME           0x17
#define ES8311_REG_ADC_EQ_HPF           0x1C
#define ES8311_REG_DAC_MUTE             0x31
#define ES8311_REG_DAC_VOLUME           0x32
#define ES8311_REG_DAC_EQ               0x37

#define ES8311_RESET_ASSERT             0x1F
/*
 * Reg0x00 bit7 (CSM_ON) must be set after reset so the codec state machine
 * leaves power-down.  Releasing reset with 0x00 keeps the ES8311 control port
 * alive over I2C but leaves the digital audio path silent, which makes the ESP
 * I2S RX channel wait forever and return ESP_ERR_TIMEOUT.
 */
#define ES8311_RESET_POWER_UP           0x80
#define ES8311_CLK1_MCLK_INPUT_ENABLE   0x3F
#define ES8311_CLK2_12M288_16K          0x40
#define ES8311_CLK3_12M288_16K          0x10
#define ES8311_CLK4_12M288_16K          0x20
#define ES8311_CLK5_12M288_16K          0x00
#define ES8311_CLK6_12M288_16K          0x04
#define ES8311_CLK7_12M288_16K          0x00
#define ES8311_CLK8_12M288_16K          0xFF
#define ES8311_SDP_16BIT_I2S            0x0C
#define ES8311_SYSTEM0D_ANALOG_UP       0x01
#define ES8311_SYSTEM0E_ADC_PGA_UP      0x02
#define ES8311_SYSTEM12_DAC_UP          0x00
#define ES8311_SYSTEM12_DAC_DOWN        0x02
#define ES8311_SYSTEM13_OUTPUT_UP       0x10
#define ES8311_MIC_GAIN_MASK            0x1F
#define ES8311_MIC_ANALOG_ENABLE        0x10
#define ES8311_DEFAULT_MIC_GAIN         ES8311_MIC_GAIN_30DB
#define ES8311_ADC_SCALE_STANDARD       0x24
#define ES8311_ADC_VOLUME_BOOST         0xC8
#define ES8311_ADC_EQ_HPF_ENABLE        0x6A
#define ES8311_DAC_EQ_BYPASS            0x08
#define ES8311_DAC_MUTE_BIT             BIT(5)
#define ES8311_DAC_DEFAULT_VOLUME       0xBF

static const char *TAG_CODEC = "ES8311";

static esp_err_t es8311_first_error(esp_err_t current, esp_err_t next)
{
    return (current != ESP_OK) ? current : next;
}

static esp_err_t es8311_validate_sample_rate(int sample_rate)
{
    switch (sample_rate) {
    case 16000:
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


static void es8311_log_register_snapshot(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    const uint8_t regs[] = {
        ES8311_REG_RESET,
        ES8311_REG_CLK_MANAGER_1,
        ES8311_REG_CLK_MANAGER_2,
        ES8311_REG_CLK_MANAGER_3,
        ES8311_REG_CLK_MANAGER_4,
        ES8311_REG_CLK_MANAGER_6,
        ES8311_REG_CLK_MANAGER_8,
        ES8311_REG_SDP_ADC,
        ES8311_REG_SYSTEM_0D,
        ES8311_REG_SYSTEM_0E,
        ES8311_REG_SYSTEM_12,
        ES8311_REG_SYSTEM_13,
        ES8311_REG_MIC_GAIN,
        ES8311_REG_ADC_VOLUME,
        ES8311_REG_DAC_MUTE,
    };
    char snapshot[160];
    size_t used = 0;

    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        uint8_t value = 0;
        esp_err_t err = es8311_read_reg(i2c_num, i2c_addr, regs[i], &value);
        if (err != ESP_OK) {
            ESP_LOGW(TAG_CODEC, "register snapshot aborted at reg 0x%02x: %s",
                     regs[i], esp_err_to_name(err));
            return;
        }
        int written = snprintf(snapshot + used, sizeof(snapshot) - used,
                               "%s%02x=%02x", i == 0 ? "" : " ", regs[i], value);
        if (written < 0 || (size_t)written >= sizeof(snapshot) - used) {
            break;
        }
        used += (size_t)written;
    }
    ESP_LOGI(TAG_CODEC, "ES8311 register snapshot: %s", snapshot);
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
 * and a DAC-only profile for speaker actions. Future changes must classify
 * ES8311 registers as exact-readback-safe, masked,
 * volatile, or write-only before adding stricter hardware verification.
 */
esp_err_t es8311_init_profile(i2c_port_t i2c_num, uint8_t i2c_addr, int i2s_port,
                              es8311_profile_t profile, int sample_rate)
{
    (void)i2s_port; // The codec is configured over I2C; I2S is configured by the caller.

    if (profile != ES8311_PROFILE_ADC_ONLY &&
        profile != ES8311_PROFILE_DAC_ONLY) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = es8311_validate_sample_rate(sample_rate);
    if (ret != ESP_OK) {
        return ret;
    }

    const bool adc_enabled = profile != ES8311_PROFILE_DAC_ONLY;
    const bool dac_enabled = profile != ES8311_PROFILE_ADC_ONLY;
    const char *profile_name = profile == ES8311_PROFILE_ADC_ONLY ? "adc-only" : "dac-only";
    (void)profile_name;

    ESP_LOGI(TAG_CODEC, "Initialising ES8311 at I2C 0x%02x (sample_rate=%d, profile=%s)",
             i2c_addr, sample_rate, profile_name);

    ret = es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_ASSERT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed during reset assert");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG_CODEC, "ES8311 reset asserted; waiting for analog rails to settle");

    ret = ESP_OK;
    ESP_LOGI(TAG_CODEC, "ES8311 configuring clock tree for 12.288 MHz MCLK / %d Hz sample rate", sample_rate);
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_POWER_UP));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_1, ES8311_CLK1_MCLK_INPUT_ENABLE));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_2, ES8311_CLK2_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_3, ES8311_CLK3_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_4, ES8311_CLK4_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_5, ES8311_CLK5_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_6, ES8311_CLK6_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_7, ES8311_CLK7_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_CLK_MANAGER_8, ES8311_CLK8_12M288_16K));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SDP_DAC, ES8311_SDP_16BIT_I2S));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SDP_ADC, ES8311_SDP_16BIT_I2S));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SYSTEM_0D, ES8311_SYSTEM0D_ANALOG_UP));
    if (adc_enabled) {
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SYSTEM_0E, ES8311_SYSTEM0E_ADC_PGA_UP));
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_EQ_HPF, ES8311_ADC_EQ_HPF_ENABLE));
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_SCALE, ES8311_ADC_SCALE_STANDARD));
        ret = es8311_first_error(ret, es8311_set_mic_gain(i2c_num, i2c_addr, ES8311_DEFAULT_MIC_GAIN));
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_ADC_VOLUME, ES8311_ADC_VOLUME_BOOST));
    }
    if (dac_enabled) {
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SYSTEM_13, ES8311_SYSTEM13_OUTPUT_UP));
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SYSTEM_12, ES8311_SYSTEM12_DAC_UP));
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_DAC_EQ, ES8311_DAC_EQ_BYPASS));
        ret = es8311_first_error(ret, es8311_set_dac_volume(i2c_num, i2c_addr, ES8311_DAC_DEFAULT_VOLUME));
        ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));
    } else {
        /*
         * Optional StickS3 audio capture mode is ADC-only. Keep the DAC path muted
         * and avoid powering it up until a source-backed playback feature explicitly
         * requests the speaker path.
         */
        ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_SYSTEM_12, ES8311_SYSTEM12_DAC_DOWN));
        ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, true));
    }

    if (ret == ESP_OK) {
        es8311_log_register_snapshot(i2c_num, i2c_addr);
        ESP_LOGI(TAG_CODEC, "ES8311 initialised: profile=%s adc=%s dac=%s adc_volume=0x%02x mic_gain=%u dac_muted=%s",
                 profile_name, adc_enabled ? "on" : "off", dac_enabled ? "on" : "off",
                 adc_enabled ? ES8311_ADC_VOLUME_BOOST : 0u,
                 adc_enabled ? (unsigned)ES8311_DEFAULT_MIC_GAIN : 0u,
                 dac_enabled ? "no" : "yes");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed: %s", esp_err_to_name(ret));
    }
    return ret;
}


esp_err_t es8311_set_mic_gain(i2c_port_t i2c_num, uint8_t i2c_addr, es8311_mic_gain_t gain)
{
    if (gain < ES8311_MIC_GAIN_0DB || gain > ES8311_MIC_GAIN_30DB) {
        return ESP_ERR_INVALID_ARG;
    }

    return es8311_update_reg_bits(i2c_num, i2c_addr, ES8311_REG_MIC_GAIN,
                                  ES8311_MIC_GAIN_MASK, ES8311_MIC_ANALOG_ENABLE | (uint8_t)gain, true);
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
                                                           ES8311_REG_ADC_VOLUME, 0x00, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_DAC_VOLUME, 0x00, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_SYSTEM_12, ES8311_SYSTEM12_DAC_DOWN, true));

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
                                                           ES8311_RESET_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_SYSTEM_0D,
                                                           ES8311_SYSTEM0D_ANALOG_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_SYSTEM_0E,
                                                           ES8311_SYSTEM0E_ADC_PGA_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_ADC_VOLUME,
                                                           ES8311_ADC_VOLUME_BOOST, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr,
                                                           ES8311_REG_SYSTEM_12,
                                                           ES8311_SYSTEM12_DAC_UP, true));
    ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered up");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power up failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
