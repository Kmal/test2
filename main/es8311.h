/*
 * Minimal ES8311 codec driver for the M5Stack Stick S3. Configuration is
 * performed over I2C while audio data is exchanged over I2S. This header
 * exposes a simple initialisation function that resets the codec, sets the
 * sample rate, and enables both ADC and DAC paths.
 */

#pragma once

#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/i2s.h"

/**
 * @brief Initialise the ES8311 codec
 *
 * This function performs a soft reset of the ES8311, configures the
 * clocking for the specified sample rate and enables both the ADC and
 * DAC.  The I2S peripheral should be configured before calling
 * this function.
 *
 * @param i2c_num   I2C port used to communicate with the codec
 * @param i2c_addr  7‑bit I2C address of the codec (typically 0x18)
 * @param i2s_port  I2S port number used for audio data
 * @param sample_rate  Audio sample rate in Hz (e.g. 16000)
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate);