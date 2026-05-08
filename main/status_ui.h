#ifndef STATUS_UI_H
#define STATUS_UI_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_UI_STATE_BOOTING = 0,
    STATUS_UI_STATE_DISCOVERABLE,
    STATUS_UI_STATE_PAIRED,
    STATUS_UI_STATE_HFP_CONNECTED,
    STATUS_UI_STATE_AUDIO_STREAMING,
    STATUS_UI_STATE_ERROR,
} status_ui_state_t;

typedef struct {
    void (*clear_pairing)(void *ctx);
    void (*toggle_monitoring)(void *ctx);
    void (*toggle_discoverable)(void *ctx);
    void *ctx;
} status_ui_button_handlers_t;

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers);
void status_ui_set_state(status_ui_state_t state);
status_ui_state_t status_ui_get_state(void);
const char *status_ui_state_name(status_ui_state_t state);
void status_ui_set_monitoring_enabled(bool enabled);
bool status_ui_get_monitoring_enabled(void);
void status_ui_set_discoverable_enabled(bool enabled);
bool status_ui_get_discoverable_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_UI_H
