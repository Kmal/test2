#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_mode.h"
#include "audio_metrics.h"
#include "ble_sound_level_protocol.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*transport_ble_control_cb_t)(uint8_t command, void *ctx);
typedef size_t (*transport_ble_pcm_read_cb_t)(uint8_t *dst, size_t max_bytes, void *ctx);

typedef struct {
    app_mode_t app_mode;
    app_display_mode_t display_mode;
    bool sound_meter_enabled;
    uint32_t sample_rate_hz;
    uint32_t metrics_window_ms;
    uint32_t windows_completed;
    uint32_t i2s_read_errors;
} transport_ble_status_snapshot_t;

/**
 * Start the StickS3 Bluetooth Low Energy GATT audio telemetry transport.
 *
 * The service exposes optional raw PCM debug notifications on 0xFFF1,
 * sound-meter telemetry on 0xFFF2, control writes on 0xFFF3, and status reads
 * / notifications on 0xFFF4. None of these characteristics is an
 * operating-system-standard Bluetooth microphone profile.
 */
esp_err_t transport_ble_gatt_pcm_start(void);
esp_err_t transport_ble_gatt_pcm_publish_metrics(const audio_level_metrics_t *metrics,
                                                 app_mode_t app_mode,
                                                 app_display_mode_t display_mode);
void transport_ble_gatt_pcm_set_pcm_debug_enabled(bool enabled);
bool transport_ble_gatt_pcm_get_pcm_debug_enabled(void);
bool transport_ble_gatt_pcm_is_connected(void);
bool transport_ble_gatt_pcm_metrics_notify_enabled(void);
bool transport_ble_gatt_pcm_pcm_notify_enabled(void);
void transport_ble_gatt_pcm_set_control_callback(transport_ble_control_cb_t cb, void *ctx);
void transport_ble_gatt_pcm_set_pcm_reader(transport_ble_pcm_read_cb_t cb, void *ctx);
void transport_ble_gatt_pcm_update_status(const transport_ble_status_snapshot_t *status);

#ifdef __cplusplus
}
#endif
