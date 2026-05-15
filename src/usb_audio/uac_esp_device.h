#pragma once

#include "esp_err.h"
#include "uac_device_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uac_esp_device_start(uac_device_adapter_t *adapter);

#ifdef __cplusplus
}
#endif
