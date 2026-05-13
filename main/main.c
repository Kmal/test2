/*
 * Main firmware entry point for the M5Stack StickS3 configuration and local
 * automation application.
 */

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "action_http.h"
#include "action_ir.h"
#include "app_mode.h"
#include "app_time.h"
#include "app_wifi.h"
#include "board_sticks3.h"
#include "rule_config_store.h"
#include "rule_runtime.h"
#include "rule_web.h"
#include "status_ui.h"
#include "sdkconfig.h"

#if CONFIG_APP_TRANSPORT_HFP_LEGACY && defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32-S3 does not support Bluetooth Classic / BR/EDR; legacy HFP is not available on StickS3"
#endif

#if CONFIG_APP_TRANSPORT_HFP_LEGACY
#include "transport_hfp_legacy.h"
#endif

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
#include "transport_ble_gatt.h"
#endif

static const char *TAG = "STICKS3_APP";
static app_runtime_state_t s_runtime_state;
static rule_runtime_t s_rule_runtime;
static rule_config_store_t s_rule_store;
static rule_web_t s_rule_web;
static automation_config_t s_rule_config;
static bool s_rule_runtime_ready;
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

static esp_err_t app_network_stack_init(void)
{
    ESP_LOGI(TAG, "network stack init: starting");
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "network stack init: esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "network stack init: esp_netif ready");

    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "network stack init: default event loop already exists");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "network stack init: default event loop failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "network stack init: default event loop ready");
    return ESP_OK;
}

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
        const bool connected = transport_ble_gatt_is_connected();
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

static void app_emit_button_rule_fact(button_state_event_t event)
{
    const uint32_t uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (s_rule_mutex != NULL && xSemaphoreTake(s_rule_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)rule_runtime_process_button_event(&s_rule_runtime, event, uptime_ms);
        xSemaphoreGive(s_rule_mutex);
    }
}

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
    ESP_LOGI(TAG, "rule runtime init: loading automation config");
    if (!rule_config_store_open(&s_rule_store) || !rule_config_store_load(&s_rule_store, &s_rule_config)) {
        automation_config_set_defaults(&s_rule_config);
        ESP_LOGW(TAG, "rule runtime init: using default automation config");
    } else {
        ESP_LOGI(TAG, "rule runtime init: automation config loaded from NVS");
    }
    if (s_rule_mutex == NULL) {
        s_rule_mutex = xSemaphoreCreateMutex();
        ESP_LOGI(TAG, "rule runtime init: mutex %s", s_rule_mutex != NULL ? "created" : "create failed");
    }
    if (s_rule_mutex != NULL) {
        (void)xSemaphoreTake(s_rule_mutex, portMAX_DELAY);
    }
    (void)rule_runtime_init(&s_rule_runtime, &s_rule_config);
    s_rule_runtime_ready = true;
    ESP_LOGI(TAG, "rule runtime init: core runtime ready");
    rule_runtime_set_http_sender(&s_rule_runtime, app_send_http_rule_action, NULL);
    rule_runtime_set_ir_sender(&s_rule_runtime, app_send_ir_rule_action, NULL);
    rule_runtime_set_local_ui_sender(&s_rule_runtime, app_send_local_ui_rule_action, NULL);
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    rule_runtime_set_ble_sender(&s_rule_runtime, app_send_ble_rule_action, NULL);
#endif
    if (rule_web_start(&s_rule_web, &s_rule_runtime, &s_rule_store)) {
        ESP_LOGI(TAG, "rule runtime init: rule web server started");
    } else {
        ESP_LOGE(TAG, "rule runtime init: rule web server failed to start");
    }
    if (s_rule_mutex != NULL) {
        xSemaphoreGive(s_rule_mutex);
    }
    if (s_rule_gpio_task == NULL) {
        BaseType_t created = xTaskCreate(app_rule_gpio_poll_task, "rule_gpio_poll", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_gpio_task);
        ESP_LOGI(TAG, "rule runtime init: gpio poll task %s", created == pdPASS ? "created" : "create failed");
    }
    if (s_rule_network_task == NULL) {
        BaseType_t created = xTaskCreate(app_rule_network_state_task, "rule_net_state", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_network_task);
        ESP_LOGI(TAG, "rule runtime init: network state task %s", created == pdPASS ? "created" : "create failed");
    }
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    if (s_rule_ble_task == NULL) {
        BaseType_t created = xTaskCreate(app_rule_ble_state_task, "rule_ble_state", 3072, NULL, tskIDLE_PRIORITY + 1, &s_rule_ble_task);
        ESP_LOGI(TAG, "rule runtime init: BLE state task %s", created == pdPASS ? "created" : "create failed");
    }
#endif
}

static void app_publish_ble_status(void)
{
#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    transport_ble_status_snapshot_t status = {
        .app_mode = s_runtime_state.app_mode,
    };
    transport_ble_gatt_update_status(&status);
#endif
}

static void key1_pressed_cb(void *ctx)
{
    (void)ctx;
    app_emit_button_rule_fact(BUTTON_STATE_EVENT_KEY1_SHORT);
}

static void key2_pressed_cb(void *ctx)
{
    (void)ctx;
    app_emit_button_rule_fact(BUTTON_STATE_EVENT_KEY2_SHORT);
}

static void automation_config_changed_cb(void *ctx)
{
    (void)ctx;
    automation_config_t config;
    automation_config_set_defaults(&config);
    if (!s_rule_store.opened && !rule_config_store_open(&s_rule_store)) {
        ESP_LOGW(TAG, "status UI automation update: failed to open config store");
        return;
    }
    bool loaded = rule_config_store_load(&s_rule_store, &config);
    if (!loaded) {
        ESP_LOGW(TAG, "status UI automation update: failed to reload config");
        return;
    }
    if (!s_rule_runtime_ready) {
        s_rule_config = config;
        ESP_LOGI(TAG, "status UI automation update: runtime refresh deferred until init");
        return;
    }
    if (s_rule_mutex != NULL) {
        (void)xSemaphoreTake(s_rule_mutex, portMAX_DELAY);
    }
    bool replaced = rule_runtime_replace_config(&s_rule_runtime, &config);
    if (replaced) {
        s_rule_config = config;
        ESP_LOGI(TAG, "status UI automation update: runtime config refreshed");
    } else {
        ESP_LOGW(TAG, "status UI automation update: runtime config refresh failed");
    }
    if (s_rule_mutex != NULL) {
        xSemaphoreGive(s_rule_mutex);
    }
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
    ESP_LOGI(TAG, "app_main: starting, reset_reason=%d, free_heap=%lu",
             (int)esp_reset_reason(), (unsigned long)esp_get_free_heap_size());
    app_runtime_state_init(&s_runtime_state);
    ESP_LOGI(TAG, "app_main: runtime defaults mode=%s",
             app_mode_name(s_runtime_state.app_mode));

    const status_ui_button_handlers_t status_handlers = {
        .key1_pressed = key1_pressed_cb,
        .key2_pressed = key2_pressed_cb,
        .automation_config_changed = automation_config_changed_cb,
    };

    ESP_LOGI(TAG, "app_main: NVS init start");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "app_main: NVS reinitializing after %s", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "app_main: NVS ready");
    (void)app_time_init();
    ESP_ERROR_CHECK(app_network_stack_init());

    ESP_LOGI(TAG, "app_main: status UI init start");
    ESP_ERROR_CHECK(status_ui_init(&status_handlers));
    status_ui_set_state(STATUS_UI_STATE_BOOTING);

    if (app_wifi_start()) {
        ESP_LOGI(TAG, "app_main: Wi-Fi/web UI network ready or setup AP started");
    } else {
        ESP_LOGW(TAG, "app_main: Wi-Fi/web UI network startup unavailable");
    }
    app_rule_runtime_init();

#if CONFIG_APP_TRANSPORT_BLE_GATT_PCM
    ESP_LOGI(TAG, "app_main: BLE GATT rule-event init start");
    ret = transport_ble_gatt_start();
    if (ret != ESP_OK) {
        status_ui_set_service_enabled(false);
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        ESP_LOGE(TAG, "BLE GATT rule-event transport failed to start: %s; staying alive", esp_err_to_name(ret));
        app_idle_forever();
    }
    app_publish_ble_status();
    status_ui_set_service_enabled(true);
    status_ui_set_state(STATUS_UI_STATE_READY);
    ESP_LOGI(TAG, "Bluetooth LE rule-event transport and configuration UI are running");
#else
    status_ui_set_service_enabled(false);
    status_ui_set_state(STATUS_UI_STATE_NO_TRANSPORT);
    ESP_LOGW(TAG, "No StickS3 BLE rule-event transport is selected; local UI remains available");
#endif

    app_idle_forever();
#endif
}
