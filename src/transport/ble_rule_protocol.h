#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_RULE_SERVICE_UUID 0xFFF0
#define BLE_RULE_STATUS_CHAR_UUID 0xFFF4
#define BLE_RULE_EVENT_CHAR_UUID 0xFFF5

#define BLE_RULE_STATUS_MAGIC 0x5354354dU /* "M5TS" little-endian */
#define BLE_RULE_STATUS_VERSION 1U
#define BLE_RULE_EVENT_MAGIC 0x4552354dU /* "M5RE" little-endian */
#define BLE_RULE_EVENT_VERSION 1U
#define BLE_RULE_EVENT_NAME_BYTES 20U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t packet_bytes;
    uint32_t uptime_ms;
    uint8_t app_mode;
    uint8_t ble_connected;
    uint8_t status_notify_enabled;
    uint8_t rule_event_notify_enabled;
} ble_rule_status_packet_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t packet_bytes;
    uint32_t sequence;
    uint32_t uptime_ms;
    uint32_t rule_id;
    uint16_t source;
    uint16_t action;
    int32_t measured_i32;
    uint32_t fire_count;
    char rule_name[BLE_RULE_EVENT_NAME_BYTES];
} ble_rule_event_packet_t;

#ifdef __cplusplus
}
#endif
