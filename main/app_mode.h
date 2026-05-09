#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODE_SOUND_METER = 0,
    APP_MODE_PCM_DEBUG_STREAM,
    APP_MODE_CALIBRATION,
    APP_MODE_PAUSED,
    APP_MODE_COUNT,
} app_mode_t;

typedef enum {
    APP_DISPLAY_VU = 0,
    APP_DISPLAY_NUMERIC,
    APP_DISPLAY_BLE_STATUS,
    APP_DISPLAY_DIAGNOSTICS,
    APP_DISPLAY_COUNT,
} app_display_mode_t;

typedef struct {
    app_mode_t app_mode;
    app_display_mode_t display_mode;
    bool ble_telemetry_enabled;
    bool ble_pcm_debug_enabled;
    bool lcd_enabled;
    bool hold_peak_enabled;
    unsigned int mode_change_count;
} app_runtime_state_t;

void app_runtime_state_init(app_runtime_state_t *state);
void app_runtime_next_app_mode(app_runtime_state_t *state);
void app_runtime_next_display_mode(app_runtime_state_t *state);
const char *app_mode_name(app_mode_t mode);
const char *app_display_mode_name(app_display_mode_t mode);

#ifdef __cplusplus
}
#endif
