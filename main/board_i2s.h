#pragma once

#include <stddef.h>
#include <stdint.h>

#include "board_audio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_i2s_init_profile(board_audio_profile_t profile);
esp_err_t board_i2s_deinit(void);
esp_err_t board_i2s_read(void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms);
esp_err_t board_i2s_read_mono_i16(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms);
esp_err_t board_i2s_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
