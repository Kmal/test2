#include "app_mode.h"

#include <stddef.h>

void app_runtime_state_init(app_runtime_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->app_mode = APP_MODE_SOUND_METER;
    state->display_mode = APP_DISPLAY_VU;
    state->ble_telemetry_enabled = true;
    state->ble_pcm_debug_enabled = false;
    state->lcd_enabled = true;
    state->hold_peak_enabled = true;
    state->mode_change_count = 0;
}

void app_runtime_next_app_mode(app_runtime_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->app_mode = (app_mode_t)(((int)state->app_mode + 1) % APP_MODE_COUNT);
    state->ble_pcm_debug_enabled = state->app_mode == APP_MODE_PCM_DEBUG_STREAM;
    state->mode_change_count++;
}

void app_runtime_next_display_mode(app_runtime_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->display_mode = (app_display_mode_t)(((int)state->display_mode + 1) % APP_DISPLAY_COUNT);
    state->mode_change_count++;
}

const char *app_mode_name(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_SOUND_METER:
        return "sound meter";
    case APP_MODE_PCM_DEBUG_STREAM:
        return "pcm debug";
    case APP_MODE_CALIBRATION:
        return "calibration";
    case APP_MODE_PAUSED:
        return "paused";
    default:
        return "unknown";
    }
}

const char *app_display_mode_name(app_display_mode_t mode)
{
    switch (mode) {
    case APP_DISPLAY_VU:
        return "vu";
    case APP_DISPLAY_NUMERIC:
        return "numeric";
    case APP_DISPLAY_BLE_STATUS:
        return "ble status";
    case APP_DISPLAY_DIAGNOSTICS:
        return "diagnostics";
    default:
        return "unknown";
    }
}
