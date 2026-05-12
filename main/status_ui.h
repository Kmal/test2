#ifndef STATUS_UI_H
#define STATUS_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "app_mode.h"
#include "ui_nav.h"
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
    void (*automation_config_changed)(void *ctx);
    void *ctx;
} status_ui_button_handlers_t;

typedef enum {
    STATUS_UI_INPUT_SELECT = 0,
    STATUS_UI_INPUT_NEXT,
    STATUS_UI_INPUT_PREV,
    STATUS_UI_INPUT_BACK,
} status_ui_input_t;

typedef struct {
    bool valid;
    uint32_t sequence;
    uint32_t age_ms;
    int32_t rms_dbfs_q8;
    int32_t peak_dbfs_q8;
    uint16_t rms_percent;
    uint16_t peak_percent;
    uint16_t vu_percent;
    uint16_t clipped_samples;
    uint32_t zero_crossings;
    uint32_t flags;
    uint8_t app_mode;
    uint8_t display_mode;
    bool ble_connected;
    bool ble_metrics_notify_enabled;
    bool ble_pcm_notify_enabled;
    bool calibration_active;
    uint32_t calibration_collected_windows;
    uint32_t calibration_required_windows;
    int32_t calibration_noise_floor_dbfs_q8;
} status_ui_sound_meter_snapshot_t;

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers);
void status_ui_set_state(status_ui_state_t state);
status_ui_state_t status_ui_get_state(void);
const char *status_ui_state_name(status_ui_state_t state);
void status_ui_set_monitoring_enabled(bool enabled);
bool status_ui_get_monitoring_enabled(void);
void status_ui_set_service_enabled(bool enabled);
bool status_ui_get_service_enabled(void);
void status_ui_set_sound_meter_snapshot(const status_ui_sound_meter_snapshot_t *snapshot);
bool status_ui_get_sound_meter_snapshot(status_ui_sound_meter_snapshot_t *out);
void status_ui_set_display_mode(app_display_mode_t mode);
app_display_mode_t status_ui_get_display_mode(void);
bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms);
void status_ui_open_screen(ui_screen_id_t screen);
ui_screen_id_t status_ui_get_screen(void);
void status_ui_handle_input(status_ui_input_t input);

#ifdef __cplusplus
}
#endif

#endif // STATUS_UI_H
