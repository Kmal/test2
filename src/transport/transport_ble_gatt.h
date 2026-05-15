#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_mode.h"
#include "ble_rule_protocol.h"
#include "rule_engine.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    app_mode_t app_mode;
} transport_ble_status_snapshot_t;

/**
 * Start the StickS3 Bluetooth Low Energy GATT rule-event transport.
 *
 * The service exposes a readable/notifiable status characteristic and a
 * readable/notifiable rule-event characteristic for local automation events.
 */
esp_err_t transport_ble_gatt_start(void);
bool transport_ble_gatt_is_connected(void);
bool transport_ble_status_notify_enabled(void);
bool transport_ble_rule_event_notify_enabled(void);
esp_err_t transport_ble_send_rule_event(const rule_event_t *event);
void transport_ble_gatt_update_status(const transport_ble_status_snapshot_t *status);

#ifdef __cplusplus
}
#endif
