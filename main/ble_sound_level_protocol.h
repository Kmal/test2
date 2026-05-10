#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GATT_PCM_SERVICE_UUID 0xFFF0
#define BLE_GATT_PCM_CHAR_UUID 0xFFF1
#define BLE_GATT_SOUND_LEVEL_CHAR_UUID 0xFFF2
#define BLE_GATT_CONTROL_CHAR_UUID 0xFFF3
#define BLE_GATT_STATUS_CHAR_UUID 0xFFF4

#define BLE_SOUND_LEVEL_MAGIC 0x4d4c354dU /* "M5LM" little-endian */
#define BLE_SOUND_LEVEL_VERSION 1U
#define BLE_SOUND_STATUS_MAGIC 0x5354354dU /* "M5TS" little-endian */
#define BLE_SOUND_STATUS_VERSION 1U

typedef enum {
    BLE_CONTROL_CYCLE_APP_MODE = 0x01,
    BLE_CONTROL_CYCLE_DISPLAY_MODE = 0x02,
    BLE_CONTROL_ENABLE_PCM_DEBUG = 0x03,
    BLE_CONTROL_DISABLE_PCM_DEBUG = 0x04,
    BLE_CONTROL_ENTER_CALIBRATION = 0x05,
    BLE_CONTROL_PAUSE = 0x06,
    BLE_CONTROL_RESUME_SOUND_METER = 0x07,
} ble_control_command_t;

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

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t packet_bytes;
    uint32_t uptime_ms;
    uint8_t app_mode;
    uint8_t display_mode;
    uint8_t ble_connected;
    uint8_t metrics_notify_enabled;
    uint8_t pcm_notify_enabled;
    uint8_t pcm_debug_enabled;
    uint8_t sound_meter_enabled;
    uint8_t reserved0;
    uint32_t sample_rate_hz;
    uint32_t metrics_window_ms;
    uint32_t windows_completed;
    uint32_t i2s_read_errors;
} ble_sound_status_packet_t;

#ifdef __cplusplus
}
#endif
