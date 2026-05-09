#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_audio_power_enable(i2c_port_t port);
esp_err_t board_speaker_amp_pulse(i2c_port_t port, uint8_t pulse_count);

#ifdef __cplusplus
}
#endif
