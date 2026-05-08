/*
 * Minimal ES8311 codec driver for the M5Stack Stick S3.  The ES8311 is a
 * low-power mono audio codec with an ADC, DAC, analog microphone PGA,
 * digital volume controls, mute controls and power-management registers.
 * Configuration is performed over I2C while audio data is exchanged via
 * the I2S bus.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/i2s.h"

/**
 * @brief Analog microphone PGA gain values for ES8311 register 0x14.
 */
typedef enum {
    ES8311_MIC_GAIN_0DB = 0,
    ES8311_MIC_GAIN_3DB,
    ES8311_MIC_GAIN_6DB,
    ES8311_MIC_GAIN_9DB,
    ES8311_MIC_GAIN_12DB,
    ES8311_MIC_GAIN_15DB,
    ES8311_MIC_GAIN_18DB,
    ES8311_MIC_GAIN_21DB,
    ES8311_MIC_GAIN_24DB,
    ES8311_MIC_GAIN_27DB,
    ES8311_MIC_GAIN_30DB,
} es8311_mic_gain_t;

/**
 * @brief Initialise the ES8311 codec.
 *
 * This function performs a soft reset, configures clock dividers for
 * MCLK = sample_rate * 256, sets both serial ports to 16-bit I2S, selects
 * the Mic1 differential input, enables the ADC/DAC paths, and logs the
 * selected microphone gain and ADC/DAC volume values. The I2S peripheral
 * should be configured before calling this function.
 *
 * @param i2c_num      I2C port used to communicate with the codec
 * @param i2c_addr     7-bit I2C address of the codec (M5StickS3: 0x18)
 * @param i2s_port     I2S port number used for audio data
 * @param sample_rate  Audio sample rate in Hz (supported: 8000, 16000, 48000)
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate);

/**
 * @brief Set the analog microphone PGA gain.
 *
 * @param gain One of es8311_mic_gain_t, from 0 dB to 30 dB in 3 dB steps
 */
esp_err_t es8311_set_mic_gain(i2c_port_t i2c_num, uint8_t i2c_addr, es8311_mic_gain_t gain);

/**
 * @brief Set the DAC digital volume register.
 *
 * ES8311 volume uses register 0x32: 0x00 is -95.5 dB, 0xBF is 0 dB,
 * and 0xFF is +32 dB.
 */
esp_err_t es8311_set_dac_volume(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t volume);

/**
 * @brief Mute or unmute the DAC output path.
 */
esp_err_t es8311_mute(i2c_port_t i2c_num, uint8_t i2c_addr, bool mute);

/**
 * @brief Put the codec into a low-power standby state.
 */
esp_err_t es8311_power_down(i2c_port_t i2c_num, uint8_t i2c_addr);

/**
 * @brief Restore the codec from the low-power standby state.
 *
 * This restores power-related registers but does not reprogram the full
 * sample-rate clock tree; call es8311_init() after a complete reset.
 */
esp_err_t es8311_power_up(i2c_port_t i2c_num, uint8_t i2c_addr);
