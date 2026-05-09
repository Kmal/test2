/*
 * Main firmware entry point for M5Stack StickS3 board bring-up.
 *
 * The previous application attempted to expose a Classic Bluetooth HFP
 * microphone. That transport is incompatible with StickS3 because ESP32-S3
 * does not support Bluetooth Classic / BR/EDR. Until a StickS3-compatible
 * transport is selected, this firmware initializes the documented status
 * peripherals and a capture-only ES8311 audio profile while keeping local
 * speaker/DAC output disabled.
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board_audio.h"
#include "status_ui.h"
#include "sdkconfig.h"

#if CONFIG_APP_TRANSPORT_HFP_LEGACY && defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32-S3 does not support Bluetooth Classic / BR/EDR; legacy HFP is not available on StickS3"
#endif

#if CONFIG_APP_TRANSPORT_HFP_LEGACY
#include "transport_hfp_legacy.h"
#endif

static const char *TAG = "STICKS3_APP";

static void key1_pressed_cb(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "KEY1 pressed; no transport action is assigned until a StickS3-compatible transport is selected");
}

static void key2_pressed_cb(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "KEY2 pressed; no transport action is assigned until a StickS3-compatible transport is selected");
}

void app_main(void)
{
#if CONFIG_APP_TRANSPORT_HFP_LEGACY
    transport_hfp_legacy_run();
#else
    const status_ui_button_handlers_t status_handlers = {
        .key1_pressed = key1_pressed_cb,
        .key2_pressed = key2_pressed_cb,
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(status_ui_init(&status_handlers));
    status_ui_set_state(STATUS_UI_STATE_BOOTING);

    const board_audio_config_t audio_config = {
        .profile = BOARD_AUDIO_PROFILE_CAPTURE_ONLY,
        .probe_m5pm1 = true,
        .require_audio_power_enable = false,
    };
    ESP_ERROR_CHECK(board_audio_init(&audio_config));

    status_ui_set_monitoring_enabled(false);
    status_ui_set_service_enabled(false);
    status_ui_set_state(STATUS_UI_STATE_NO_TRANSPORT);
    ESP_LOGW(TAG, "No StickS3-compatible audio transport is selected; see docs/transport-feasibility.md");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}
