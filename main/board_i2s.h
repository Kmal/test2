#pragma once

#include "board_audio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_i2s_init_profile(board_audio_profile_t profile);

#ifdef __cplusplus
}
#endif
