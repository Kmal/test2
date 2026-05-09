/*
 * Minimal ES8311 codec driver for the M5Stack StickS3 board-support
 * firmware. The ES8311 is a low-power mono audio codec with an ADC, DAC,
 * analog microphone PGA, digital volume controls, mute controls and
 * power-management registers. This driver intentionally implements only the
 * project-tested StickS3 subset: fixed 12.288 MHz MCLK, 16-bit I2S slave
 * mode, ADC-only capture profile, and a full-duplex compatibility
 * profile. Configuration is performed over I2C while audio data is exchanged
 * via the I2S bus.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/i2s_types.h"

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
 * @brief ES8311 initialization profiles.
 *
 * ADC-only is used by the StickS3 BLE GATT PCM firmware so the microphone
 * path can be brought up without enabling the DAC/speaker-monitoring path.
 * Full-duplex preserves the historical behavior for non-StickS3 legacy code.
 */
typedef enum {
    ES8311_PROFILE_ADC_ONLY = 0,
    ES8311_PROFILE_FULL_DUPLEX,
} es8311_profile_t;

/**
 * @brief Initialise the ES8311 codec.
 *
 * Compatibility wrapper for the historical full-duplex behavior. New StickS3
 * code should call es8311_init_profile() so default boot can select the
 * ADC-only profile and keep DAC/speaker output disabled.
 *
 * @param i2c_num      I2C port used to communicate with the codec
 * @param i2c_addr     7-bit I2C address of the codec (M5StickS3: 0x18)
 * @param i2s_port     I2S port number used for audio data
 * @param sample_rate  Audio sample rate in Hz (supported: 8000, 16000, 48000)
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate);
esp_err_t es8311_init_profile(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port,
                              es8311_profile_t profile, int sample_rate);

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
