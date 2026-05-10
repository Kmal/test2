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
#define BLE_GATT_PCM_SERVICE_UUID 0xFFF0
#define BLE_GATT_PCM_CHAR_UUID 0xFFF1
#define BLE_GATT_SOUND_LEVEL_CHAR_UUID 0xFFF2
#define BLE_GATT_CONTROL_CHAR_UUID 0xFFF3
#define BLE_GATT_STATUS_CHAR_UUID 0xFFF4
#define BLE_SOUND_LEVEL_MAGIC 0x4d4c354dU /* "M5LM" little-endian */
#define BLE_SOUND_LEVEL_VERSION 1U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t packet_bytes;
    uint32_t sequence;
    uint32_t uptime_ms;
    uint32_t sample_rate_hz;
    uint16_t window_ms;
    uint16_t flags;
    int32_t rms_dbfs_q8;
    int32_t peak_dbfs_q8;
    uint16_t rms_percent;
    uint16_t peak_percent;
    uint16_t vu_percent;
    uint16_t clipped_samples;
    uint8_t app_mode;
    uint8_t display_mode;
    uint8_t reserved0;
    uint8_t reserved1;
} ble_sound_level_packet_t;

/**
 * Start the StickS3 Bluetooth Low Energy GATT audio telemetry transport.
 *
 * The service exposes optional raw PCM debug notifications on 0xFFF1,
 * sound-meter telemetry on 0xFFF2, control writes on 0xFFF3, and status reads
 * / notifications on 0xFFF4. None of these characteristics is an
 * The service exposes optional raw PCM debug notifications on 0xFFF1 and the
 * sound-meter telemetry stream on 0xFFF2. Neither characteristic is an
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
