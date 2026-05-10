/*
 * Main firmware entry point for M5Stack StickS3 board bring-up.
 *
 * The previous application attempted to expose a Classic Bluetooth HFP
 * microphone. That transport is incompatible with StickS3 because ESP32-S3
 * does not support Bluetooth Classic / BR/EDR. The default StickS3-compatible
 * transport is now a Bluetooth Low Energy GATT PCM stream. The firmware
 * initializes the documented status peripherals and a capture-only ES8311 audio profile while
 * keeping local speaker/DAC output disabled.
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_mode.h"
#include "board_audio.h"
#include "board_sticks3.h"
#include "sound_meter.h"
#include "status_ui.h"
#include "sdkconfig.h"

#if CONFIG_APP_TRANSPORT_HFP_LEGACY && defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32-S3 does not support Bluetooth Classic / BR/EDR; legacy HFP is not available on StickS3"
#endif

#if CONFIG_APP_TRANSPORT_HFP_LEGACY
#include "transport_hfp_legacy.h"
#endif

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
#include "transport_ble_gatt_pcm.h"
#endif

static const char *TAG = "STICKS3_APP";
static app_runtime_state_t s_runtime_state;
static portMUX_TYPE s_runtime_mux = portMUX_INITIALIZER_UNLOCKED;

static app_runtime_state_t app_get_runtime_snapshot(void *ctx)
{
    (void)ctx;
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    return state;
}

static void app_apply_runtime_state(const app_runtime_state_t *state)
{
    status_ui_set_display_mode(state->display_mode);
    switch (state->app_mode) {
    case APP_MODE_SOUND_METER:
        sound_meter_set_enabled(true);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
#if CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
        transport_ble_gatt_pcm_set_pcm_debug_enabled(false);
#endif
        status_ui_set_monitoring_enabled(true);
        break;
    case APP_MODE_PCM_DEBUG_STREAM:
        sound_meter_set_enabled(true);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM && CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
#if CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
        transport_ble_gatt_pcm_set_pcm_debug_enabled(state->ble_pcm_debug_enabled);
#else
        (void)state;
#endif
        status_ui_set_monitoring_enabled(true);
        break;
    case APP_MODE_CALIBRATION:
        sound_meter_reset_calibration();
        sound_meter_set_enabled(true);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
        transport_ble_gatt_pcm_set_pcm_debug_enabled(false);
#endif
        status_ui_set_monitoring_enabled(true);
        break;
    case APP_MODE_PAUSED:
        sound_meter_set_enabled(false);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
        transport_ble_gatt_pcm_set_pcm_debug_enabled(false);
#endif
        status_ui_set_monitoring_enabled(false);
        break;
    default:
        break;
    }
}

static void app_set_mode(app_mode_t mode)
{
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    s_runtime_state.app_mode = mode;
    s_runtime_state.ble_pcm_debug_enabled = mode == APP_MODE_PCM_DEBUG_STREAM;
    s_runtime_state.mode_change_count++;
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    app_apply_runtime_state(&state);
    ESP_LOGI(TAG, "app mode: %s", app_mode_name(state.app_mode));
}

static void app_cycle_app_mode(void)
{
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    app_runtime_next_app_mode(&s_runtime_state);
#if !CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
    if (s_runtime_state.app_mode == APP_MODE_PCM_DEBUG_STREAM) {
        app_runtime_next_app_mode(&s_runtime_state);
    }
#endif
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    app_apply_runtime_state(&state);
    ESP_LOGI(TAG, "app mode: %s", app_mode_name(state.app_mode));
}

static void app_cycle_display_mode(void)
{
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    app_runtime_next_display_mode(&s_runtime_state);
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    status_ui_set_display_mode(state.display_mode);
    ESP_LOGI(TAG, "display mode: %s", app_display_mode_name(state.display_mode));
}

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
static esp_err_t app_handle_control_command(uint8_t command, void *ctx)
{
    (void)ctx;
    switch ((ble_control_command_t)command) {
    case BLE_CONTROL_CYCLE_APP_MODE:
        app_cycle_app_mode();
        return ESP_OK;
    case BLE_CONTROL_CYCLE_DISPLAY_MODE:
        app_cycle_display_mode();
        return ESP_OK;
    case BLE_CONTROL_ENABLE_PCM_DEBUG:
#if CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
        app_set_mode(APP_MODE_PCM_DEBUG_STREAM);
        return ESP_OK;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case BLE_CONTROL_DISABLE_PCM_DEBUG:
        app_set_mode(APP_MODE_SOUND_METER);
        return ESP_OK;
    case BLE_CONTROL_ENTER_CALIBRATION:
        app_set_mode(APP_MODE_CALIBRATION);
        return ESP_OK;
    case BLE_CONTROL_PAUSE:
        app_set_mode(APP_MODE_PAUSED);
        return ESP_OK;
    case BLE_CONTROL_RESUME_SOUND_METER:
        app_set_mode(APP_MODE_SOUND_METER);
        return ESP_OK;
    default:
        ESP_LOGW(TAG, "unknown BLE control command: 0x%02x", command);
        return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t app_publish_metrics_adapter(const audio_level_metrics_t *metrics,
                                             app_mode_t app_mode,
                                             app_display_mode_t display_mode,
                                             void *ctx)
{
    (void)ctx;
    return transport_ble_gatt_pcm_publish_metrics(metrics, app_mode, display_mode);
}

static bool app_transport_connected_adapter(void *ctx)
{
    (void)ctx;
    return transport_ble_gatt_pcm_is_connected();
}

static bool app_metrics_notify_adapter(void *ctx)
{
    (void)ctx;
    return transport_ble_gatt_pcm_metrics_notify_enabled();
}

static bool app_pcm_notify_adapter(void *ctx)
{
    (void)ctx;
    return transport_ble_gatt_pcm_pcm_notify_enabled();
}

static bool app_pcm_debug_adapter(void *ctx)
{
    (void)ctx;
    return transport_ble_gatt_pcm_get_pcm_debug_enabled();
}

static size_t app_pcm_debug_read_adapter(uint8_t *dst, size_t max_bytes, void *ctx)
{
    (void)ctx;
    return sound_meter_read_pcm_debug(dst, max_bytes);
}

static void app_status_update_adapter(const sound_meter_stats_t *stats,
                                      const audio_calibration_t *calibration,
                                      const app_runtime_state_t *runtime,
                                      bool enabled,
                                      void *ctx)
{
    (void)ctx;
    transport_ble_status_snapshot_t status = {
        .app_mode = runtime->app_mode,
        .display_mode = runtime->display_mode,
        .sound_meter_enabled = enabled,
        .sample_rate_hz = BOARD_I2S_SAMPLE_RATE,
        .metrics_window_ms = CONFIG_APP_SOUND_METER_WINDOW_MS,
        .windows_completed = stats->windows_completed,
        .i2s_read_errors = stats->i2s_read_errors,
    };
    transport_ble_gatt_pcm_update_status(&status);

    if (calibration != NULL && calibration->valid && runtime->app_mode == APP_MODE_CALIBRATION) {
        app_set_mode(APP_MODE_SOUND_METER);
    }
}
#endif

static void key1_pressed_cb(void *ctx)
{
    (void)ctx;
    app_cycle_display_mode();
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    app_runtime_next_display_mode(&s_runtime_state);
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    status_ui_set_display_mode(state.display_mode);
    ESP_LOGI(TAG, "KEY1 display mode: %s", app_display_mode_name(state.display_mode));
}

static void key2_pressed_cb(void *ctx)
{
    (void)ctx;
    app_cycle_app_mode();
    app_runtime_state_t state;
    portENTER_CRITICAL(&s_runtime_mux);
    app_runtime_next_app_mode(&s_runtime_state);
#if !CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
    if (s_runtime_state.app_mode == APP_MODE_PCM_DEBUG_STREAM) {
        app_runtime_next_app_mode(&s_runtime_state);
    }
#endif
    state = s_runtime_state;
    portEXIT_CRITICAL(&s_runtime_mux);
    app_apply_runtime_state(&state);
    ESP_LOGI(TAG, "KEY2 app mode: %s", app_mode_name(state.app_mode));
}

static void app_idle_forever(void)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
#if CONFIG_APP_TRANSPORT_HFP_LEGACY
    transport_hfp_legacy_run();
#else
    app_runtime_state_init(&s_runtime_state);

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
        .probe_m5pm1 = false,
        .require_audio_power_enable = true,
    };
    ret = board_audio_init(&audio_config);
    if (ret != ESP_OK) {
        status_ui_set_service_enabled(false);
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        ESP_LOGE(TAG, "audio initialisation failed: %s; staying alive for diagnostics", esp_err_to_name(ret));
        app_idle_forever();
    }

    status_ui_set_monitoring_enabled(true);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    transport_ble_gatt_pcm_set_control_callback(app_handle_control_command, NULL);
    transport_ble_gatt_pcm_set_pcm_reader(app_pcm_debug_read_adapter, NULL);
    ret = transport_ble_gatt_pcm_start();
    if (ret != ESP_OK) {
        status_ui_set_service_enabled(false);
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        ESP_LOGE(TAG, "BLE GATT PCM transport failed to start: %s; staying alive for diagnostics", esp_err_to_name(ret));
        app_idle_forever();
    }
    status_ui_set_service_enabled(true);
#if CONFIG_APP_SOUND_METER_ENABLE
    const sound_meter_config_t meter_config = {
        .sample_rate_hz = BOARD_I2S_SAMPLE_RATE,
        .metrics_window_ms = CONFIG_APP_SOUND_METER_WINDOW_MS,
        .i2s_read_timeout_ms = 100,
        .pcm_chunk_bytes = BOARD_PCM_CHUNK_SIZE,
        .enable_ble_telemetry = true,
        .enable_lcd_updates = true,
        .dbfs_floor_q8 = CONFIG_APP_SOUND_METER_DBFS_FLOOR * 256,
        .dbfs_ceiling_q8 = 0,
        .loud_threshold_q8 = CONFIG_APP_SOUND_METER_LOUD_DBFS * 256,
        .telemetry_interval_ms = 1000 / CONFIG_APP_SOUND_METER_TELEMETRY_HZ,
        .calibration_windows = CONFIG_APP_SOUND_METER_CALIBRATION_WINDOWS,
        .get_runtime = app_get_runtime_snapshot,
        .publish_metrics = app_publish_metrics_adapter,
        .transport_connected = app_transport_connected_adapter,
        .metrics_notify_enabled = app_metrics_notify_adapter,
        .pcm_notify_enabled = app_pcm_notify_adapter,
        .pcm_debug_enabled = app_pcm_debug_adapter,
        .status_update = app_status_update_adapter,
        .get_runtime = app_get_runtime_snapshot,
    };
    ret = sound_meter_start(&meter_config);
    if (ret != ESP_OK) {
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        ESP_LOGE(TAG, "sound meter failed to start: %s; staying alive for diagnostics", esp_err_to_name(ret));
        app_idle_forever();
    }
#endif
    status_ui_set_display_mode(s_runtime_state.display_mode);
    status_ui_set_state(STATUS_UI_STATE_READY);
    ESP_LOGI(TAG, "Bluetooth LE sound-meter telemetry is running");
#else
    status_ui_set_service_enabled(false);
    status_ui_set_state(STATUS_UI_STATE_NO_TRANSPORT);
    ESP_LOGW(TAG, "No StickS3-compatible audio transport is selected; see docs/transport-feasibility.md");
#endif

    app_idle_forever();
#endif
}
