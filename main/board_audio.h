#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_AUDIO_PROFILE_CAPTURE_ONLY = 0,
    BOARD_AUDIO_PROFILE_FULL_DUPLEX,
} board_audio_profile_t;

typedef struct {
    board_audio_profile_t profile;
    bool probe_m5pm1;
    bool require_audio_power_enable;
} board_audio_config_t;

typedef struct {
    esp_err_t (*i2c_init)(void *ctx);
    esp_err_t (*m5pm1_probe)(void *ctx);
    esp_err_t (*audio_power_enable)(void *ctx);
    esp_err_t (*i2s_init_profile)(board_audio_profile_t profile, void *ctx);
    esp_err_t (*es8311_init_profile)(board_audio_profile_t profile, void *ctx);
    esp_err_t (*cleanup_on_failure)(esp_err_t cause, void *ctx);
    void *ctx;
} board_audio_ops_t;

esp_err_t board_audio_init_with_ops(const board_audio_config_t *config, const board_audio_ops_t *ops);
esp_err_t board_audio_init(const board_audio_config_t *config);
esp_err_t board_audio_deinit(void);

#ifdef __cplusplus
}
#endif
