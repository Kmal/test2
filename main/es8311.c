/*
 * Basic ES8311 codec initialisation.  The purpose of this file is
 * to provide a small set of helper functions sufficient for
 * recording and playback on the M5Stack Stick S3.  It resets the
 * ES8311 and configures it for I2S master mode, 16 bit samples and
 * the desired sampling rate.  More detailed control (e.g. mic
 * gain, de‑emphasis filters, power management) can be achieved
 * using the full Espressif codec framework.
 */

#include "es8311.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG_CODEC = "ES8311";

// Write a single 8‑bit value to a codec register
static esp_err_t es8311_write_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C write reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

/*
 * The register map for the ES8311 is described in the datasheet.  The
 * following sequence performs a soft reset and configures basic
 * parameters: clock, format and enabling ADC/DAC.  It is a
 * simplified configuration derived from example code in Espressif’s
 * audio drivers and may not be optimal for all use cases.  See the
 * ES8311 datasheet for additional settings.
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate)
{
    (void)i2s_port; // currently unused
    esp_err_t ret;

    // Soft reset the codec
    ret = es8311_write_reg(i2c_num, i2c_addr, 0x00, 0x30);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = es8311_write_reg(i2c_num, i2c_addr, 0x00, 0x00);
    if (ret != ESP_OK) return ret;

    // Power up and configure analog blocks: enable microphone bias
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x01, 0x30); // MICBIAS=2.4V, enable VB
    // Configure the power management of ADC and DAC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x02, 0x10); // Start internal bias
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x03, 0x00);

    // Configure global settings: I2S master, MCLK from internal PLL
    // Set system clock (MCLK) ratio for desired sample rate.  The
    // ES8311 datasheet suggests 0x1b for 16 kHz with 12.288 MHz MCLK.
    uint8_t clk_div = 0x1b;
    if (sample_rate == 8000) clk_div = 0x3b;
    else if (sample_rate == 16000) clk_div = 0x1b;
    else if (sample_rate == 48000) clk_div = 0x00;
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x0B, clk_div);

    // Set I2S format: 16‑bit I2S, left justified off
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x11, 0x10); // ADC word length 16 bits
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x12, 0x10); // DAC word length 16 bits
    // Select I2S mode (no DSP) and slave mode; the ESP32 provides clock
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x0D, 0x00);

    // Enable DAC and ADC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x14, 0x1a); // Enable DAC, headphone output
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x15, 0x06); // Power up DAC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x16, 0x30); // Enable DAC and ADC
    ret |= es8311_write_reg(i2c_num, i2c_addr, 0x17, 0x30); // ADC power on

    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 initialised (sample_rate=%d)", sample_rate);
    }
    return ret;
}