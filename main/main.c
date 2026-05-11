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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "action_http.h"
#include "action_ir.h"
#include "app_mode.h"
#include "board_audio.h"
#include "board_sticks3.h"
#include "rule_config_store.h"
#include "rule_runtime.h"
#include "rule_web.h"
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
static rule_runtime_t s_rule_runtime;
static rule_config_store_t s_rule_store;
static rule_web_t s_rule_web;
static SemaphoreHandle_t s_rule_mutex;
static TaskHandle_t s_rule_gpio_task;
static TaskHandle_t s_rule_network_task;
static bool s_rule_network_state_known;
static bool s_rule_network_ready;
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
static TaskHandle_t s_rule_ble_task;
static bool s_rule_ble_state_known;
static bool s_rule_ble_connected;
#endif



static action_result_t app_send_http_rule_action(const rule_event_t *event, void *ctx)
{
    (void)ctx;
    action_result_t result = {
        .code = ACTION_RESULT_OK,
    };
    if (event != NULL) {
        result.sequence = event->sequence;
        result.rule_id = event->rule_id;
        result.action = event->action;
    }
    action_http_result_t http = action_http_post_event(event != NULL ? &event->action_config : NULL, event);
    if (http == ACTION_HTTP_RESULT_NOT_READY) {
        result.code = ACTION_RESULT_NOT_STARTED;
    } else if (http != ACTION_HTTP_RESULT_OK) {
        result.code = ACTION_RESULT_UNSUPPORTED;
    }
    return result;
}


static action_result_t app_send_ir_rule_action(const rule_event_t *event, void *ctx)
{
    (void)ctx;
    action_result_t result = {
        .code = ACTION_RESULT_OK,
    };
    if (event != NULL) {
        result.sequence = event->sequence;
        result.rule_id = event->rule_id;
        result.action = event->action;
    }
    if (!action_ir_send_event(event)) {
        result.code = ACTION_RESULT_UNSUPPORTED;
    }
    return result;
}


static action_result_t app_send_local_ui_rule_action(const rule_event_t *event, void *ctx)
{
    (void)ctx;
    action_result_t result = {
        .code = ACTION_RESULT_OK,
    };
    if (event != NULL) {
        result.sequence = event->sequence;
        result.rule_id = event->rule_id;
        result.action = event->action;
    }
    status_ui_set_state(STATUS_UI_STATE_READY);
    status_ui_set_service_enabled(true);
    return result;
}

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
static action_result_t app_send_ble_rule_action(const rule_event_t *event, void *ctx)
{
    (void)ctx;
    action_result_t result = {
        .code = ACTION_RESULT_OK,
    };
    if (event != NULL) {
        result.sequence = event->sequence;
        result.rule_id = event->rule_id;
        result.action = event->action;
    }
    esp_err_t err = transport_ble_send_rule_event(event);
    if (err == ESP_ERR_INVALID_STATE) {
        result.code = ACTION_RESULT_NOT_STARTED;
    } else if (err != ESP_OK) {
        result.code = ACTION_RESULT_UNSUPPORTED;
    }
    return result;
}
#endif




static void app_emit_wifi_connected_rule_fact(bool connected, uint32_t uptime_ms)
{
    trigger_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    fact.source = RULE_SOURCE_WIFI_CONNECTED;
    fact.value = rule_value_bool(connected);
    fact.uptime_ms = uptime_ms;
    if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)trigger_emit_fact(&s_rule_runtime.trigger_adapter, &fact);
        xSemaphoreGive(s_rule_mutex);
    }
}

static void app_rule_network_state_task(void *ctx)
{
    (void)ctx;
    while (true) {
        const bool ready = action_http_network_ready();
        const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (!s_rule_network_state_known || ready != s_rule_network_ready) {
            s_rule_network_state_known = true;
            s_rule_network_ready = ready;
            app_emit_wifi_connected_rule_fact(ready, uptime_ms);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
static void app_emit_ble_connected_rule_fact(bool connected, uint32_t uptime_ms)
{
    trigger_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    fact.source = RULE_SOURCE_BLE_CONNECTED;
    fact.value = rule_value_bool(connected);
    fact.uptime_ms = uptime_ms;
    if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)trigger_emit_fact(&s_rule_runtime.trigger_adapter, &fact);
        xSemaphoreGive(s_rule_mutex);
    }
}

static void app_rule_ble_state_task(void *ctx)
{
    (void)ctx;
    while (true) {
        const bool connected = transport_ble_gatt_pcm_is_connected();
        const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (!s_rule_ble_state_known || connected != s_rule_ble_connected) {
            s_rule_ble_state_known = true;
            s_rule_ble_connected = connected;
            app_emit_ble_connected_rule_fact(connected, uptime_ms);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif

static void app_rule_gpio_poll_task(void *ctx)
{
    (void)ctx;
    while (true) {
        const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            (void)rule_runtime_poll_gpio(&s_rule_runtime, uptime_ms);
            xSemaphoreGive(s_rule_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void app_rule_runtime_init(void)
{
    automation_config_t config;
    if (!rule_config_store_open(&s_rule_store) || !rule_config_store_load(&s_rule_store, &config)) {
        automation_config_set_defaults(&config);
    }
    if (s_rule_mutex == NULL) {
        s_rule_mutex = xSemaphoreCreateMutex();
    }
    if (s_rule_mutex != NULL) {
        (void)xSemaphoreTake(s_rule_mutex, portMAX_DELAY);
    }
    (void)rule_runtime_init(&s_rule_runtime, &config);
    rule_runtime_set_http_sender(&s_rule_runtime, app_send_http_rule_action, NULL);
    rule_runtime_set_ir_sender(&s_rule_runtime, app_send_ir_rule_action, NULL);
    rule_runtime_set_local_ui_sender(&s_rule_runtime, app_send_local_ui_rule_action, NULL);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    rule_runtime_set_ble_sender(&s_rule_runtime, app_send_ble_rule_action, NULL);
#endif
    (void)rule_web_start(&s_rule_web, &s_rule_runtime, &s_rule_store);
    if (s_rule_mutex != NULL) {
        xSemaphoreGive(s_rule_mutex);
    }
    if (s_rule_gpio_task == NULL) {
        (void)xTaskCreate(app_rule_gpio_poll_task, "rule_gpio_poll", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_gpio_task);
    }
    if (s_rule_network_task == NULL) {
        (void)xTaskCreate(app_rule_network_state_task, "rule_net_state", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_network_task);
    }
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    if (s_rule_ble_task == NULL) {
        (void)xTaskCreate(app_rule_ble_state_task, "rule_ble_state", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_ble_task);
    }
#endif
}

static void app_process_rule_metrics(const audio_level_metrics_t *metrics)
{
    const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, 0) == pdTRUE) {
        (void)rule_runtime_process_metrics(&s_rule_runtime, metrics, uptime_ms);
        xSemaphoreGive(s_rule_mutex);
    }
}

static void app_emit_button_rule_fact(button_state_event_t event)
{
    const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)rule_runtime_process_button_event(&s_rule_runtime, event, uptime_ms);
        xSemaphoreGive(s_rule_mutex);
    }
}

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
        transport_ble_gatt_pcm_set_pcm_debug_enabled(false);
#endif
        status_ui_set_monitoring_enabled(true);
        break;
    case APP_MODE_PCM_DEBUG_STREAM:
        sound_meter_set_enabled(true);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM && CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG
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
    app_process_rule_metrics(metrics);
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
    app_emit_button_rule_fact(BUTTON_STATE_EVENT_KEY1_SHORT);
    app_cycle_display_mode();
}

static void key2_pressed_cb(void *ctx)
{
    (void)ctx;
    app_emit_button_rule_fact(BUTTON_STATE_EVENT_KEY2_SHORT);
    app_cycle_app_mode();
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
    app_rule_runtime_init();

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
