#include "transport_hfp_legacy.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HFP_LEGACY";

void transport_hfp_legacy_run(void)
{
    ESP_LOGE(TAG, "Legacy Bluetooth Classic HFP transport is quarantined and not supported by StickS3/ESP32-S3");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
