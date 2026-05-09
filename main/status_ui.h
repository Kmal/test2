#ifndef STATUS_UI_H
#define STATUS_UI_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_UI_STATE_BOOTING = 0,
    STATUS_UI_STATE_NO_TRANSPORT,
    STATUS_UI_STATE_READY,
    STATUS_UI_STATE_ERROR,
} status_ui_state_t;

typedef struct {
    void (*key1_pressed)(void *ctx);
    void (*key2_pressed)(void *ctx);
    void *ctx;
} status_ui_button_handlers_t;

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers);
void status_ui_set_state(status_ui_state_t state);
status_ui_state_t status_ui_get_state(void);
const char *status_ui_state_name(status_ui_state_t state);
void status_ui_set_monitoring_enabled(bool enabled);
bool status_ui_get_monitoring_enabled(void);
void status_ui_set_service_enabled(bool enabled);
bool status_ui_get_service_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_UI_H
