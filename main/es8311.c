/*
 * Basic ES8311 codec initialisation.  The purpose of this file is
 * to provide a small set of helper functions sufficient for
 * recording and playback on the M5Stack Stick S3.  It resets the
 * ES8311 and configures it for I2S slave mode, 16-bit samples and
 * the desired sampling rate.  The ESP32-S3 I2S peripheral is the clock
 * master and drives MCLK/BCLK/LRCLK.  More detailed control (e.g. mic
 * gain, de‑emphasis filters, power management) can be achieved
 * using the full Espressif codec framework.
 */

#include "es8311.h"
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

static const char *TAG_CODEC = "ES8311";

#define ES8311_CLK_MANAGER_REG          0x0B
#define ES8311_CLK_12M288_TO_16K        0x1B
#define ES8311_CLK_12M288_TO_8K         0x3B
#define ES8311_CLK_12M288_TO_48K        0x00
#define ES8311_I2S_MODE_REG             0x0D
#define ES8311_I2S_SLAVE_MODE           0x00

// Write a single 8-bit value to a codec register
static esp_err_t es8311_write_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(ES8311_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C write reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

// Read a single 8-bit value from a codec register.
static esp_err_t es8311_read_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(ES8311_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
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

static esp_err_t es8311_configure_clock(i2c_port_t i2c_num, uint8_t i2c_addr, int sample_rate)
{
    const es8311_clock_config_t *clock = es8311_get_clock_config(sample_rate);
    if (clock == NULL) {
        ESP_LOGE(TAG_CODEC, "Unsupported ES8311 sample rate %d Hz for %dx MCLK", sample_rate, ES8311_MCLK_MULTIPLE);
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint8_t reg02 = ((clock->pre_div - 1) << 5) | (clock->pre_mult << 3);
    uint8_t reg03 = (clock->fs_mode << 6) | clock->adc_osr;
    uint8_t reg05 = ((clock->adc_div - 1) << 4) | (clock->dac_div - 1);
    uint8_t reg06 = (clock->bclk_div < 19) ? (clock->bclk_div - 1) : clock->bclk_div;

    esp_err_t ret = ESP_OK;
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_1, ES8311_CLKMGR1_MCLK_PIN_ENABLE, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_2, reg02, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_3, reg03, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_4, clock->dac_osr, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_5, reg05, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_6, reg06, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_7, clock->lrck_h, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_8, clock->lrck_l, true));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "Clock configured: sample_rate=%d Hz, mclk=%d Hz", sample_rate,
                 sample_rate * ES8311_MCLK_MULTIPLE);
    }
    return ret;
}

/*
 * The sequence below follows the ES8311 user guide/datasheet register map:
 * reset, MCLK/LRCK/BCLK dividers, I2S 16-bit serial ports, analog Mic1 PGA,
 * ADC/DAC volume, mute controls, equalizer bypass and power-up registers.
 * It is for the Stick S3 schematic where the ESP32-S3 drives MCLK/BCLK/LRCK
 * and the ES8311 is at I2C address 0x18 with Mic1 and differential DAC output.
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate)
{
    (void)i2s_port; // The codec is configured over I2C; I2S is configured by the caller.
    ESP_LOGI(TAG_CODEC, "Initialising ES8311 at I2C 0x%02x (sample_rate=%d)", i2c_addr, sample_rate);

    esp_err_t ret = es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_ASSERT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed during reset assert");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = es8311_write_reg(i2c_num, i2c_addr, 0x00, 0x00);
    if (ret != ESP_OK) return ret;

    // Power up and configure analog blocks: enable microphone bias
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x01, 0x30); // MICBIAS=2.4V, enable VB
    // Configure the power management of ADC and DAC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x02, 0x10); // Start internal bias
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x03, 0x00);

    // Keep the ES8311 in I2S slave mode.  The ESP32-S3 I2S master
    // supplies a 12.288 MHz MCLK, so the codec clock divider is chosen
    // for that MCLK frequency and the requested audio sample rate.
    uint8_t clk_div = ES8311_CLK_12M288_TO_16K;
    if (sample_rate == 8000) clk_div = ES8311_CLK_12M288_TO_8K;
    else if (sample_rate == 16000) clk_div = ES8311_CLK_12M288_TO_16K;
    else if (sample_rate == 48000) clk_div = ES8311_CLK_12M288_TO_48K;
    ret |= es8311_write_reg(i2c_num, i2c_addr, ES8311_CLK_MANAGER_REG, clk_div);

    // Set I2S format: 16-bit I2S, left justified off
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x11, 0x10); // ADC word length 16 bits
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x12, 0x10); // DAC word length 16 bits
    // Select I2S mode (no DSP) and slave mode; the ESP32-S3 provides clocks.
    ret |= es8311_write_reg(i2c_num, i2c_addr, ES8311_I2S_MODE_REG, ES8311_I2S_SLAVE_MODE);

    // Enable DAC and ADC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x14, 0x1a); // Enable DAC, headphone output
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x15, 0x06); // Power up DAC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x16, 0x30); // Enable DAC and ADC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x17, 0x30); // ADC power on

esp_err_t es8311_power_up(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    esp_err_t ret = es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_RELEASE, true);
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_1, ES8311_POWER1_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_2, ES8311_POWER2_ADC_PGA_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_CODEC_ON, true));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered up");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power up failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
