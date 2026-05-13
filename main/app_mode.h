#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODE_CONTROL = 0,
    APP_MODE_COUNT,
} app_mode_t;

typedef struct {
    app_mode_t app_mode;
    unsigned int mode_change_count;
} app_runtime_state_t;

void app_runtime_state_init(app_runtime_state_t *state);
const char *app_mode_name(app_mode_t mode);

#ifdef __cplusplus
}
#endif
