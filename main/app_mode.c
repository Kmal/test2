#include "app_mode.h"

#include <stddef.h>

void app_runtime_state_init(app_runtime_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->app_mode = APP_MODE_CONTROL;
    state->mode_change_count = 0;
}

const char *app_mode_name(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_CONTROL:
        return "control";
    default:
        return "unknown";
    }
}
