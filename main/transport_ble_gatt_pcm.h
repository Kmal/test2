#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the StickS3 Bluetooth Low Energy GATT PCM microphone transport.
 *
 * The transport advertises a custom BLE service and sends little-endian 16-bit
 * mono PCM frames from the ES8311 capture path as notifications after a client
 * subscribes to the PCM characteristic CCCD.
 */
esp_err_t transport_ble_gatt_pcm_start(void);

#ifdef __cplusplus
}
#endif
