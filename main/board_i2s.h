#pragma once

#include <stddef.h>

#include "board_audio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_i2s_init_profile(board_audio_profile_t profile);
esp_err_t board_i2s_read(void *dest, size_t size, size_t *bytes_read, TickType_t timeout_ticks);
esp_err_t board_i2s_write(const void *src, size_t size, size_t *bytes_written, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
