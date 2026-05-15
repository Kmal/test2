#include "status_ui.h"


#include "board_sticks3.h"
#include "app_wifi.h"
#include "app_time.h"
#include "sdkconfig.h"
#include "ui_nav.h"
#include "ui_model.h"
#include "status_lcd.h"
#include "ui_keyboard.h"
#include "ui_render.h"
#if CONFIG_APP_TRANSPORT_BLE_GATT_RULE_EVENTS
#include "transport_ble_gatt.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_APP_STATUS_UI_LCD
#include "freertos/queue.h"
#endif


#define STATUS_UI_DEBOUNCE_MS 50
#define STATUS_UI_POLL_MS 25
#define STATUS_UI_TASK_STACK 3072
#define STATUS_UI_TASK_PRIORITY 5
#if CONFIG_APP_STATUS_UI_LCD
#define STATUS_UI_INPUT_TASK_STACK 6144
#define STATUS_UI_INPUT_TASK_PRIORITY 4
#define STATUS_UI_BUTTON_DOUBLE_MS 250
#define STATUS_UI_BUTTON_LONG_MS 600
/* Match Bruce StickS3 two-button navigation: KEY1 selects, KEY2 single-clicks next,
 * double-clicks previous, and long-presses back/escape. */
#endif

#if CONFIG_APP_STATUS_UI_LCD
#ifdef CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#define STATUS_UI_KEYBOARD_TIMEOUT_MS CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#else
#define STATUS_UI_KEYBOARD_TIMEOUT_MS 0
#endif
#endif

static const char *TAG = "STATUS_UI";

static status_ui_button_handlers_t s_handlers;
static status_ui_state_t s_state = STATUS_UI_STATE_BOOTING;
static bool s_service_enabled = false;
static uint32_t s_key1_press_count = 0;
static uint32_t s_key2_press_count = 0;
static ui_runtime_t s_ui;
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;

#if CONFIG_APP_STATUS_UI_LCD
static QueueHandle_t s_virtual_keyboard_queue;
static QueueHandle_t s_input_queue;
static bool status_ui_keyboard_open_menu_edit(ui_runtime_t *ui, const ui_menu_item_t *item, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret);
static void status_ui_keyboard_handle_menu_event(status_ui_keyboard_event_t event);
static void status_ui_render_lcd(void *ctx);
#endif

#if CONFIG_APP_STATUS_UI_LCD
static bool keyboard_is_active(void);
static void keyboard_queue_event(status_ui_keyboard_event_t event);
static bool status_ui_keyboard_active_locked(void);
static void status_ui_route_button_to_keyboard(status_ui_input_t input);
static void status_ui_route_button_to_menu(status_ui_input_t input);
static void status_ui_input_task(void *arg);
#endif



typedef struct {
    gpio_num_t gpio;
    const char *name;
    void (*handler)(void *ctx);
    bool stable_pressed;
    bool last_sample_pressed;
    TickType_t last_change_tick;
#if CONFIG_APP_STATUS_UI_LCD
    TickType_t pressed_since_tick;
    TickType_t released_tick;
    bool long_sent;
    bool waiting_single;
#endif
} status_button_t;

static const char *bool_label(bool enabled)
{
    return enabled ? "on" : "off";
}

const char *status_ui_state_name(status_ui_state_t state)
{
    switch (state) {
    case STATUS_UI_STATE_BOOTING:
        return "booting";
    case STATUS_UI_STATE_NO_TRANSPORT:
        return "no transport selected";
    case STATUS_UI_STATE_READY:
        return "ready";
    case STATUS_UI_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void status_ui_set_state(status_ui_state_t state)
{
    bool changed = false;
    portENTER_CRITICAL(&s_state_mux);
    if (s_state != state) {
        s_state = state;
        changed = true;
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (changed) {
        ESP_LOGI(TAG, "status: %s", status_ui_state_name(state));
    }
}

status_ui_state_t status_ui_get_state(void)
{
    status_ui_state_t state;
    portENTER_CRITICAL(&s_state_mux);
    state = s_state;
    portEXIT_CRITICAL(&s_state_mux);
    return state;
}

void status_ui_set_service_enabled(bool enabled)
{
    bool changed;
    portENTER_CRITICAL(&s_state_mux);
    changed = s_service_enabled != enabled;
    s_service_enabled = enabled;
    s_ui.dirty = true;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "web UI service: %s", bool_label(enabled));
    if (changed && s_handlers.service_enabled_changed != NULL) {
        s_handlers.service_enabled_changed(enabled, s_handlers.ctx);
    }
}

bool status_ui_get_service_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_state_mux);
    enabled = s_service_enabled;
    portEXIT_CRITICAL(&s_state_mux);
    return enabled;
}

void status_ui_open_screen(ui_screen_id_t screen)
{
    portENTER_CRITICAL(&s_state_mux);
    if (screen == UI_SCREEN_MAIN) {
        ui_nav_init(&s_ui.nav);
    } else {
        ui_nav_enter(&s_ui.nav, screen);
    }
    s_ui.menu_active = true;
    s_ui.dirty = true;
    portEXIT_CRITICAL(&s_state_mux);
}

ui_screen_id_t status_ui_get_screen(void)
{
    ui_screen_id_t screen;
    portENTER_CRITICAL(&s_state_mux);
    screen = s_ui.nav.current;
    portEXIT_CRITICAL(&s_state_mux);
    return screen;
}

#if CONFIG_APP_STATUS_UI_LCD
static ui_wifi_flow_state_t *status_ui_get_wifi_state_for_item(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    return item != NULL ? ui_runtime_wifi_flow(ui, item->flow) : NULL;
}

static void status_ui_clear_wifi_state(ui_wifi_flow_state_t *wifi)
{
    if (wifi == NULL) return;
    memset(&wifi->scan_results, 0, sizeof(wifi->scan_results));
    wifi->selected_scan_index = 0u;
    wifi->ssid[0] = '\0';
    wifi->password[0] = '\0';
    wifi->has_selected_ssid = false;
    wifi->has_password = false;
    wifi->scan_valid = false;
    wifi->last_error[0] = '\0';
    wifi->web_url[0] = '\0';
}

static bool status_ui_wifi_scan(ui_runtime_t *ui, ui_wifi_flow_state_t *wifi)
{
    if (ui == NULL || wifi == NULL) return false;
    status_ui_clear_wifi_state(wifi);
    bool ok = app_wifi_scan(&wifi->scan_results);
    wifi->scan_valid = ok;
    snprintf(wifi->last_error, sizeof(wifi->last_error), "%s", ok ? "" : (wifi->scan_results.error[0] ? wifi->scan_results.error : "Scan failed"));
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? "Scan complete" : "Scan failed", 2000u);
    return ok;
}

static bool status_ui_wifi_copy_selected_scan(ui_wifi_flow_state_t *wifi)
{
    if (wifi == NULL || wifi->scan_results.count == 0u || wifi->selected_scan_index >= wifi->scan_results.count) return false;
    snprintf(wifi->ssid, sizeof(wifi->ssid), "%s", wifi->scan_results.items[wifi->selected_scan_index].ssid);
    wifi->has_selected_ssid = wifi->ssid[0] != '\0';
    return wifi->has_selected_ssid;
}

static ui_screen_id_t status_ui_wifi_password_screen(ui_flow_id_t flow, bool manual)
{
    if (flow == UI_FLOW_CONFIG_WIFI) {
        return manual ? UI_SCREEN_CONFIG_WIFI_MANUAL_PASSWORD : UI_SCREEN_CONFIG_WIFI_ENTER_PASSWORD;
    }
    return manual ? UI_SCREEN_CONNECT_WIFI_MANUAL_PASSWORD : UI_SCREEN_CONNECT_WIFI_ENTER_PASSWORD;
}

static ui_screen_id_t status_ui_wifi_done_screen(ui_flow_id_t flow, bool manual)
{
    if (flow == UI_FLOW_CONFIG_WIFI) {
        return manual ? UI_SCREEN_CONFIG_WIFI_MANUAL_CONNECT_SAVE : UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE;
    }
    return manual ? UI_SCREEN_CONNECT_WIFI_MANUAL_CONNECT_SAVE : UI_SCREEN_CONNECT_WIFI_CONNECT_SAVE;
}

static bool status_ui_begin_wifi_password_edit(ui_runtime_t *ui, ui_flow_id_t flow, bool manual)
{
    if (ui == NULL) return false;
    ui_wifi_flow_state_t *wifi = ui_runtime_wifi_flow(ui, flow);
    if (wifi == NULL || wifi->ssid[0] == '\0') {
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "SSID required", 2000u);
        return false;
    }

    ui_menu_item_t password_item = {
        .label = "Enter Password",
        .target = status_ui_wifi_done_screen(flow, manual),
        .action = UI_ACTION_WIFI_ENTER_PASSWORD,
        .field = UI_FIELD_WIFI_PASSWORD,
        .flow = flow,
        .automation_index = 0u,
        .flags = manual ? 1u : 0u,
    };
    (void)ui_nav_enter(&ui->nav, status_ui_wifi_password_screen(flow, manual));
    return status_ui_keyboard_open_menu_edit(ui, &password_item, "Enter Password", wifi->password, UI_TEXT_WIFI_PASSWORD_MAX, UI_KEYBOARD_MODE_PASSWORD, true);
}

static bool status_ui_wifi_connect_with_password(ui_runtime_t *ui, ui_wifi_flow_state_t *wifi, const char *password, bool persist)
{
    if (ui == NULL || wifi == NULL || wifi->ssid[0] == '\0') {
        if (wifi != NULL) snprintf(wifi->last_error, sizeof(wifi->last_error), "SSID required");
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "SSID required", 2000u);
        return false;
    }
    wifi->web_url[0] = '\0';
    bool ok = app_wifi_connect(wifi->ssid, password != NULL ? password : "", persist);
    if (ok) {
        app_wifi_status_t status;
        if (app_wifi_get_status(&status)) {
            snprintf(wifi->web_url, sizeof(wifi->web_url), "%s", status.web_url);
        }
    }
    snprintf(wifi->last_error, sizeof(wifi->last_error), "%s", ok ? "Connected and saved" : "Wi-Fi connect failed");
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? "Wi-Fi saved" : "Wi-Fi connect failed", 2500u);
    return ok;
}

static bool status_ui_wifi_connect_and_save(ui_runtime_t *ui, ui_wifi_flow_state_t *wifi)
{
    return status_ui_wifi_connect_with_password(ui, wifi, wifi != NULL ? wifi->password : "", true);
}

static bool status_ui_wifi_try_saved_password(ui_runtime_t *ui, ui_wifi_flow_state_t *wifi, ui_flow_id_t flow, bool manual)
{
    app_wifi_config_t config;
    if (ui == NULL || wifi == NULL || !app_wifi_get_config(&config) || strcmp(config.sta_ssid, wifi->ssid) != 0 || config.sta_password[0] == '\0') {
        return status_ui_begin_wifi_password_edit(ui, flow, manual);
    }

    snprintf(wifi->password, sizeof(wifi->password), "%s", config.sta_password);
    wifi->has_password = true;
    if (status_ui_wifi_connect_with_password(ui, wifi, config.sta_password, false)) {
        if (flow == UI_FLOW_CONFIG_WIFI) status_ui_set_service_enabled(true);
        (void)ui_nav_enter(&ui->nav, status_ui_wifi_done_screen(flow, manual));
        return true;
    }

    if (app_wifi_last_connect_failed_due_to_password()) {
        wifi->password[0] = '\0';
        wifi->has_password = false;
        return status_ui_begin_wifi_password_edit(ui, flow, manual);
    }

    (void)ui_nav_enter(&ui->nav, status_ui_wifi_done_screen(flow, manual));
    return false;
}

static bool status_ui_ap_load_config(ui_runtime_t *ui)
{
    return ui_runtime_load_ap_config(ui);
}

static bool status_ui_ap_save_config(ui_runtime_t *ui)
{
    app_wifi_config_t config;
    if (ui == NULL || !app_wifi_get_config(&config)) return false;
    snprintf(config.ap_ssid, sizeof(config.ap_ssid), "%s", ui->ap.ap_name[0] ? ui->ap.ap_name : "StickS3-Setup");
    snprintf(config.ap_password, sizeof(config.ap_password), "%s", ui->ap.ap_password);
    config.ap_channel = ui->ap.channel >= UI_AP_CHANNEL_MIN && ui->ap.channel <= UI_AP_CHANNEL_MAX ? ui->ap.channel : 6u;
    return app_wifi_set_config(&config, true);
}

static bool status_ui_ap_refresh_url(ui_runtime_t *ui)
{
    if (ui == NULL) return false;
    app_wifi_status_t status;
    if (!app_wifi_get_status(&status)) return false;
    snprintf(ui->ap.url, sizeof(ui->ap.url), "%s", status.web_url);
    ui->ap.started = status.ap_started;
    return true;
}

static bool status_ui_ap_start(ui_runtime_t *ui)
{
    if (ui == NULL) return false;
    if (!ui->ap.loaded_from_config) (void)status_ui_ap_load_config(ui);
    if (!status_ui_ap_save_config(ui)) return false;
    bool ok = app_wifi_start_ap_configured(ui->ap.ap_name, ui->ap.ap_password, ui->ap.channel, true);
    (void)status_ui_ap_refresh_url(ui);
    if (ok) status_ui_set_service_enabled(true);
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? "AP Mode started" : "AP start failed", 2500u);
    return ok;
}

static bool status_ui_action_web_ui_wifi_mode(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    (void)item;
    if (ui == NULL) return false;
    if (!app_wifi_is_sta_connected()) {
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "Connect Wi-Fi first", 2500u);
        (void)ui_nav_enter(&ui->nav, UI_SCREEN_CONNECT_WIFI);
        return false;
    }

    app_wifi_status_t status;
    ui_wifi_flow_state_t *wifi = ui_runtime_wifi_flow(ui, UI_FLOW_CONFIG_WIFI);
    if (wifi != NULL) {
        wifi->web_url[0] = '\0';
        if (app_wifi_get_status(&status)) {
            snprintf(wifi->ssid, sizeof(wifi->ssid), "%s", status.sta_ssid);
            snprintf(wifi->web_url, sizeof(wifi->web_url), "%s", status.web_url);
        }
        wifi->has_selected_ssid = wifi->ssid[0] != '\0';
        snprintf(wifi->last_error, sizeof(wifi->last_error), "Web UI enabled");
    }
    status_ui_set_service_enabled(true);
    ui_runtime_set_toast(ui, UI_TOAST_SUCCESS, "Web UI enabled", 2500u);
    (void)ui_nav_enter(&ui->nav, UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE);
    return true;
}

static bool status_ui_action_wifi_scan(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(ui, item);
    bool ok = status_ui_wifi_scan(ui, wifi);
    (void)ui_nav_enter(&ui->nav, item->target);
    return ok;
}

static bool status_ui_action_wifi_select_ssid(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(ui, item);
    if (wifi == NULL || !status_ui_wifi_copy_selected_scan(wifi)) {
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "No Wi-Fi found", 2000u);
        return false;
    }
    return status_ui_wifi_try_saved_password(ui, wifi, item->flow, false);
}

static bool status_ui_select_current_scan(ui_runtime_t *ui, ui_flow_id_t flow)
{
    ui_wifi_flow_state_t *wifi = ui_runtime_wifi_flow(ui, flow);
    ui_menu_item_t item = {
        .label = "Select SSID",
        .target = status_ui_wifi_password_screen(flow, false),
        .action = UI_ACTION_WIFI_SELECT_SSID,
        .field = UI_FIELD_WIFI_SSID,
        .flow = flow,
    };
    if (wifi == NULL || !status_ui_wifi_copy_selected_scan(wifi)) {
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "No Wi-Fi found", 2000u);
        return false;
    }
    return status_ui_action_wifi_select_ssid(ui, &item);
}

static bool status_ui_action_wifi_enter_ssid(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(ui, item);
    if (wifi == NULL) return false;
    return status_ui_keyboard_open_menu_edit(ui, item, "Enter SSID", wifi->ssid, UI_TEXT_SSID_MAX, UI_KEYBOARD_MODE_TEXT, false);
}

static bool status_ui_action_wifi_enter_password(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(ui, item);
    if (wifi == NULL) return false;
    return status_ui_keyboard_open_menu_edit(ui, item, "Enter Password", wifi->password, UI_TEXT_WIFI_PASSWORD_MAX, UI_KEYBOARD_MODE_PASSWORD, true);
}

static bool status_ui_action_wifi_connect_and_save(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    return status_ui_wifi_connect_and_save(ui, status_ui_get_wifi_state_for_item(ui, item));
}

static bool status_ui_action_ap_enter_name(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    if (!ui->ap.loaded_from_config) (void)status_ui_ap_load_config(ui);
    return status_ui_keyboard_open_menu_edit(ui, item, "Set AP Name", ui->ap.ap_name, UI_TEXT_AP_NAME_MAX, UI_KEYBOARD_MODE_TEXT, false);
}

static bool status_ui_action_ap_enter_password(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    if (!ui->ap.loaded_from_config) (void)status_ui_ap_load_config(ui);
    return status_ui_keyboard_open_menu_edit(ui, item, "Set AP Password", ui->ap.ap_password, UI_TEXT_AP_PASSWORD_MAX, UI_KEYBOARD_MODE_PASSWORD, true);
}

static bool status_ui_action_ap_enter_channel(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    if (!ui->ap.loaded_from_config) (void)status_ui_ap_load_config(ui);
    char channel[4];
    snprintf(channel, sizeof(channel), "%u", (unsigned)ui->ap.channel);
    return status_ui_keyboard_open_menu_edit(ui, item, "Set Channel", channel, 2, UI_KEYBOARD_MODE_NUMERIC, false);
}

static bool status_ui_action_ap_start_mode(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    bool ok = status_ui_ap_start(ui);
    if (ok) (void)ui_nav_enter(&ui->nav, UI_SCREEN_CONFIG_AP_SHOW_URL);
    (void)item;
    return ok;
}

static bool status_ui_action_ap_show_url(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    (void)status_ui_ap_refresh_url(ui);
    (void)ui_nav_enter(&ui->nav, item->target);
    return true;
}

static void status_ui_bluetooth_refresh(ui_runtime_t *ui)
{
    if (ui == NULL) return;
#if CONFIG_APP_TRANSPORT_BLE_GATT_RULE_EVENTS
    ui->bluetooth.ble_connected = transport_ble_gatt_is_connected();
#else
    ui->bluetooth.ble_connected = false;
#endif
    ui_runtime_refresh_bluetooth(ui);
}

static bool status_ui_action_ble_show_status(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    (void)item;
    status_ui_bluetooth_refresh(ui);
    ui_runtime_set_toast(ui, UI_TOAST_INFO, "BLE status refreshed", 1500u);
    return true;
}

static void status_ui_notify_automation_config_changed(void)
{
    if (s_handlers.automation_config_changed != NULL) {
        s_handlers.automation_config_changed(s_handlers.ctx);
    }
}

static bool status_ui_action_automation_toggle_enable(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    uint8_t index = item->automation_index;
    (void)ui_runtime_load_automation(ui, index);
    ui->automations[index].enabled = !ui->automations[index].enabled;
    bool ok = ui_runtime_save_automation(ui, index);
    if (ok) {
        status_ui_notify_automation_config_changed();
    }
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ui->automations[index].enabled ? "Automation enabled" : "Automation disabled", 2000u);
    return ok;
}

static const rule_source_t s_trigger_preset_sources[] = { RULE_SOURCE_KEY1_SHORT, RULE_SOURCE_KEY2_SHORT, RULE_SOURCE_BLE_CONNECTED, RULE_SOURCE_WIFI_CONNECTED };
static const rule_action_kind_t s_action_preset_kinds[] = { RULE_ACTION_BLE_MESSAGE, RULE_ACTION_HTTP_POST, RULE_ACTION_LOCAL_UI, RULE_ACTION_IR_SEND };

static bool status_ui_action_automation_edit_trigger(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    uint8_t index = item->automation_index;
    (void)ui_runtime_load_automation(ui, index);
    ui_automation_state_t *slot = &ui->automations[index];
    size_t selected = item->flags < (sizeof(s_trigger_preset_sources) / sizeof(s_trigger_preset_sources[0])) ? item->flags : 0u;
    slot->trigger_source = s_trigger_preset_sources[selected];
    bool ok = ui_runtime_save_automation(ui, index);
    if (ok) {
        status_ui_notify_automation_config_changed();
        (void)ui_nav_back(&ui->nav);
    }
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? slot->trigger_label : slot->last_error, 1800u);
    return ok;
}

static bool status_ui_action_automation_edit_action(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    uint8_t index = item->automation_index;
    (void)ui_runtime_load_automation(ui, index);
    ui_automation_state_t *slot = &ui->automations[index];
    size_t selected = item->flags < (sizeof(s_action_preset_kinds) / sizeof(s_action_preset_kinds[0])) ? item->flags : 0u;
    slot->action_kind = s_action_preset_kinds[selected];
    bool ok = ui_runtime_save_automation(ui, index);
    if (ok) {
        status_ui_notify_automation_config_changed();
        (void)ui_nav_back(&ui->nav);
    }
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? slot->action_label : slot->last_error, 1800u);
    return ok;
}

static bool status_ui_action_settings_open(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    (void)item;
    ui_runtime_set_toast(ui, UI_TOAST_INFO, "Setting pending", 1500u);
    return true;
}
#endif

static void status_ui_dispatch_action(ui_runtime_t *ui, const ui_menu_item_t *item)
{
    if (ui == NULL || item == NULL) return;
    switch (item->action) {
    case UI_ACTION_NONE: break;
    case UI_ACTION_NAVIGATE: (void)ui_nav_enter(&ui->nav, item->target); break;
    case UI_ACTION_BACK: (void)ui_nav_back(&ui->nav); break;
#if CONFIG_APP_STATUS_UI_LCD
    case UI_ACTION_WEB_UI_WIFI_MODE: (void)status_ui_action_web_ui_wifi_mode(ui, item); break;
    case UI_ACTION_WIFI_SCAN: (void)status_ui_action_wifi_scan(ui, item); break;
    case UI_ACTION_WIFI_SELECT_SSID: (void)status_ui_action_wifi_select_ssid(ui, item); break;
    case UI_ACTION_WIFI_ENTER_SSID: (void)status_ui_action_wifi_enter_ssid(ui, item); break;
    case UI_ACTION_WIFI_ENTER_PASSWORD: (void)status_ui_action_wifi_enter_password(ui, item); break;
    case UI_ACTION_WIFI_CONNECT_AND_SAVE: (void)status_ui_action_wifi_connect_and_save(ui, item); break;
    case UI_ACTION_AP_ENTER_NAME: (void)status_ui_action_ap_enter_name(ui, item); break;
    case UI_ACTION_AP_ENTER_PASSWORD: (void)status_ui_action_ap_enter_password(ui, item); break;
    case UI_ACTION_AP_ENTER_CHANNEL: (void)status_ui_action_ap_enter_channel(ui, item); break;
    case UI_ACTION_AP_START_MODE: (void)status_ui_action_ap_start_mode(ui, item); break;
    case UI_ACTION_AP_SHOW_URL: (void)status_ui_action_ap_show_url(ui, item); break;
    case UI_ACTION_BLE_SHOW_STATUS: (void)status_ui_action_ble_show_status(ui, item); break;
    case UI_ACTION_AUTOMATION_TOGGLE_ENABLE: (void)status_ui_action_automation_toggle_enable(ui, item); break;
    case UI_ACTION_AUTOMATION_EDIT_TRIGGER: (void)status_ui_action_automation_edit_trigger(ui, item); break;
    case UI_ACTION_AUTOMATION_EDIT_ACTION: (void)status_ui_action_automation_edit_action(ui, item); break;
    case UI_ACTION_SETTINGS_OPEN: (void)status_ui_action_settings_open(ui, item); break;
#else
    default: break;
#endif
    }
    ui->dirty = true;
}

static void status_ui_activate_selected_item(void)
{
    const ui_menu_item_t *item = ui_nav_selected_item(&s_ui.nav);
    status_ui_dispatch_action(&s_ui, item);
}

static bool status_ui_is_wifi_done_screen(ui_screen_id_t screen)
{
    return screen == UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE ||
           screen == UI_SCREEN_CONFIG_WIFI_MANUAL_CONNECT_SAVE ||
           screen == UI_SCREEN_CONNECT_WIFI_CONNECT_SAVE ||
           screen == UI_SCREEN_CONNECT_WIFI_MANUAL_CONNECT_SAVE;
}

void status_ui_handle_input(status_ui_input_t input)
{
#if CONFIG_APP_STATUS_UI_LCD
    portENTER_CRITICAL(&s_state_mux);
    bool keyboard_active = status_ui_keyboard_active_locked();
    portEXIT_CRITICAL(&s_state_mux);
    if (keyboard_active) {
        status_ui_route_button_to_keyboard(input);
        return;
    }
#endif

    bool activate = false;
    bool select_scan = false;
    bool disable_web_ui_service = false;
    ui_flow_id_t select_scan_flow = UI_FLOW_NONE;
    portENTER_CRITICAL(&s_state_mux);
    if (!s_ui.menu_active && input != STATUS_UI_INPUT_BACK) {
        s_ui.menu_active = true;
        ui_nav_init(&s_ui.nav);
        s_ui.dirty = true;
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }

    const ui_screen_def_t *screen = ui_nav_current(&s_ui.nav);
    ui_wifi_flow_state_t *scan_wifi = NULL;
    if (screen != NULL && (s_ui.nav.current == UI_SCREEN_CONFIG_WIFI_SCAN || s_ui.nav.current == UI_SCREEN_CONNECT_WIFI_SCAN)) {
        scan_wifi = ui_runtime_wifi_flow(&s_ui, screen->flow);
    }

    switch (input) {
    case STATUS_UI_INPUT_SELECT:
        if (scan_wifi != NULL) {
            select_scan = true;
            select_scan_flow = screen->flow;
        } else {
            activate = true;
        }
        break;
    case STATUS_UI_INPUT_NEXT:
        if (scan_wifi != NULL && scan_wifi->scan_results.count > 0u) {
            scan_wifi->selected_scan_index = (scan_wifi->selected_scan_index + 1u) % scan_wifi->scan_results.count;
        } else {
            (void)ui_nav_next(&s_ui.nav);
        }
        s_ui.dirty = true;
        break;
    case STATUS_UI_INPUT_PREV:
        if (scan_wifi != NULL && scan_wifi->scan_results.count > 0u) {
            scan_wifi->selected_scan_index = scan_wifi->selected_scan_index == 0u ? scan_wifi->scan_results.count - 1u : scan_wifi->selected_scan_index - 1u;
        } else {
            (void)ui_nav_prev(&s_ui.nav);
        }
        s_ui.dirty = true;
        break;
    case STATUS_UI_INPUT_BACK:
        if (s_ui.menu_active && s_ui.nav.current == UI_SCREEN_MAIN) {
            s_ui.menu_active = true;
        } else if (s_ui.menu_active && status_ui_is_wifi_done_screen(s_ui.nav.current)) {
            disable_web_ui_service = true;
            ui_nav_init(&s_ui.nav);
        } else if (s_ui.menu_active) {
            (void)ui_nav_back(&s_ui.nav);
        }
        s_ui.dirty = true;
        break;
    default:
        break;
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (disable_web_ui_service) {
        status_ui_set_service_enabled(false);
    }
    if (select_scan) {
        (void)status_ui_select_current_scan(&s_ui, select_scan_flow);
    } else if (activate) {
        status_ui_activate_selected_item();
    }
}


static bool button_is_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == BOARD_BUTTON_ACTIVE_LEVEL;
}

static void record_button_press(gpio_num_t gpio)
{
    portENTER_CRITICAL(&s_state_mux);
    if (gpio == BOARD_BUTTON_KEY1_GPIO) {
        ++s_key1_press_count;
    } else if (gpio == BOARD_BUTTON_KEY2_GPIO) {
        ++s_key2_press_count;
    }
    portEXIT_CRITICAL(&s_state_mux);
}

#if CONFIG_APP_STATUS_UI_LCD
static bool keyboard_is_active(void)
{
    bool active;
    portENTER_CRITICAL(&s_state_mux);
    active = ui_keyboard_session_state()->active;
    portEXIT_CRITICAL(&s_state_mux);
    return active;
}

static void keyboard_queue_event(status_ui_keyboard_event_t event)
{
    if (s_virtual_keyboard_queue != NULL) {
        (void)xQueueSend(s_virtual_keyboard_queue, &event, 0);
    }
}

static bool status_ui_keyboard_active_locked(void)
{
    return ui_keyboard_session_state()->active;
}

static void status_ui_route_button_to_keyboard(status_ui_input_t input)
{
    status_ui_keyboard_event_t event = STATUS_UI_KEYBOARD_EVENT_PREV;
    switch (input) {
    case STATUS_UI_INPUT_SELECT:
        event = STATUS_UI_KEYBOARD_EVENT_SELECT;
        break;
    case STATUS_UI_INPUT_NEXT:
        event = STATUS_UI_KEYBOARD_EVENT_NEXT;
        break;
    case STATUS_UI_INPUT_PREV:
    case STATUS_UI_INPUT_BACK:
        event = STATUS_UI_KEYBOARD_EVENT_PREV;
        break;
    default:
        return;
    }
    if (ui_keyboard_session_edit()->active) {
        status_ui_keyboard_handle_menu_event(event);
    } else {
        keyboard_queue_event(event);
    }
}

static void status_ui_route_button_to_menu(status_ui_input_t input)
{
    if (s_input_queue != NULL) {
        (void)xQueueSend(s_input_queue, &input, 0);
    } else {
        status_ui_handle_input(input);
    }
}

static void status_ui_input_task(void *arg)
{
    (void)arg;
    while (true) {
        status_ui_input_t input;
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            status_ui_handle_input(input);
        }
    }
}
#endif

static void status_ui_dispatch_key1_short(status_button_t *button)
{
#if CONFIG_APP_STATUS_UI_LCD
    if (keyboard_is_active()) {
        if (ui_keyboard_session_edit()->active) {
            status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_SELECT);
        } else {
            keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_SELECT);
        }
    } else if (s_ui.menu_active) {
        status_ui_route_button_to_menu(STATUS_UI_INPUT_SELECT);
    } else if (button->handler != NULL) {
        button->handler(s_handlers.ctx);
    }
#else
    if (button->handler != NULL) {
        button->handler(s_handlers.ctx);
    }
#endif
}

#if CONFIG_APP_STATUS_UI_LCD
static void status_ui_dispatch_key2_single(void)
{
    if (keyboard_is_active()) {
        if (ui_keyboard_session_edit()->active) {
            status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_NEXT);
        } else {
            keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_NEXT);
        }
    } else if (s_ui.menu_active) {
        status_ui_route_button_to_menu(STATUS_UI_INPUT_NEXT);
    } else if (s_handlers.key2_pressed != NULL) {
        s_handlers.key2_pressed(s_handlers.ctx);
    }
}

static void status_ui_dispatch_key2_double(void)
{
    if (keyboard_is_active()) {
        if (ui_keyboard_session_edit()->active) {
            status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_PREV);
        } else {
            keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_PREV);
        }
    } else if (s_ui.menu_active) {
        status_ui_route_button_to_menu(STATUS_UI_INPUT_PREV);
    }
}

static void status_ui_dispatch_key2_long(void)
{
    if (keyboard_is_active()) {
        if (ui_keyboard_session_edit()->active) {
            status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_PREV);
        } else {
            keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_PREV);
        }
    } else if (s_ui.menu_active) {
        status_ui_route_button_to_menu(STATUS_UI_INPUT_BACK);
    }
}
#endif

static void maybe_dispatch_button(status_button_t *button, TickType_t now)
{
    bool pressed = button_is_pressed(button->gpio);

#if CONFIG_APP_STATUS_UI_LCD
    if (button->gpio == BOARD_BUTTON_KEY2_GPIO && button->waiting_single && !button->stable_pressed &&
        (now - button->released_tick) > pdMS_TO_TICKS(STATUS_UI_BUTTON_DOUBLE_MS)) {
        button->waiting_single = false;
        status_ui_dispatch_key2_single();
    }
#endif

    if (pressed != button->last_sample_pressed) {
        button->last_sample_pressed = pressed;
        button->last_change_tick = now;
        return;
    }

    if ((now - button->last_change_tick) < pdMS_TO_TICKS(STATUS_UI_DEBOUNCE_MS)) {
        return;
    }

#if CONFIG_APP_STATUS_UI_LCD
    if (pressed && button->stable_pressed && !button->long_sent &&
        (now - button->pressed_since_tick) >= pdMS_TO_TICKS(STATUS_UI_BUTTON_LONG_MS)) {
        button->long_sent = true;
        if (button->gpio == BOARD_BUTTON_KEY2_GPIO) {
            button->waiting_single = false;
            status_ui_dispatch_key2_long();
        } else if (!s_ui.menu_active && button->gpio == BOARD_BUTTON_KEY1_GPIO) {
            status_ui_open_screen(UI_SCREEN_MAIN);
        }
    }
#endif

    if (pressed == button->stable_pressed) {
        return;
    }

    button->stable_pressed = pressed;
    if (pressed) {
        record_button_press(button->gpio);
#if CONFIG_APP_STATUS_UI_LCD
        button->pressed_since_tick = now;
        button->long_sent = false;
#endif
        ESP_LOGI(TAG, "button pressed: %s", button->name);
        return;
    }

#if CONFIG_APP_STATUS_UI_LCD
    if (button->long_sent) {
        return;
    }
    if (button->gpio == BOARD_BUTTON_KEY1_GPIO) {
        status_ui_dispatch_key1_short(button);
    } else if (button->gpio == BOARD_BUTTON_KEY2_GPIO) {
        if (button->waiting_single && (now - button->released_tick) <= pdMS_TO_TICKS(STATUS_UI_BUTTON_DOUBLE_MS)) {
            button->waiting_single = false;
            status_ui_dispatch_key2_double();
        } else {
            button->waiting_single = true;
            button->released_tick = now;
        }
    }
#else
    if (button->handler != NULL) {
        button->handler(s_handlers.ctx);
    }
#endif
}

static void status_ui_button_task(void *arg)
{
    (void)arg;

    status_button_t buttons[] = {
        {
            .gpio = BOARD_BUTTON_KEY1_GPIO,
            .name = "KEY1",
            .handler = s_handlers.key1_pressed,
        },
        {
            .gpio = BOARD_BUTTON_KEY2_GPIO,
            .name = "KEY2",
            .handler = s_handlers.key2_pressed,
        },
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        buttons[i].stable_pressed = button_is_pressed(buttons[i].gpio);
        buttons[i].last_sample_pressed = buttons[i].stable_pressed;
        buttons[i].last_change_tick = xTaskGetTickCount();
    }

    while (true) {
        TickType_t now = xTaskGetTickCount();
        for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            maybe_dispatch_button(&buttons[i], now);
        }
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_POLL_MS));
    }
}

#if CONFIG_APP_STATUS_UI_LCD

static void keyboard_snapshot(ui_keyboard_state_t *out)
{
    portENTER_CRITICAL(&s_state_mux);
    ui_keyboard_commit_expired_pending(ui_keyboard_session_state(), (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    *out = *ui_keyboard_session_state();
    portEXIT_CRITICAL(&s_state_mux);
}


static bool status_ui_keyboard_open_menu_edit(ui_runtime_t *ui,
                                              const ui_menu_item_t *item,
                                              const char *title,
                                              const char *initial,
                                              size_t max_len,
                                              ui_keyboard_mode_t mode,
                                              bool secret)
{
    if (ui == NULL || item == NULL || !status_lcd_is_ready()) {
        return false;
    }
    if (max_len > STATUS_UI_KEYBOARD_MAX_TEXT) {
        max_len = STATUS_UI_KEYBOARD_MAX_TEXT;
    }
    portENTER_CRITICAL(&s_state_mux);
    bool opened = ui_keyboard_open(ui_keyboard_session_state(), title, initial, max_len, mode, secret);
    if (opened) {
        ui_keyboard_session_edit()->active = true;
        ui_keyboard_session_edit()->item = *item;
        ui->dirty = true;
    }
    portEXIT_CRITICAL(&s_state_mux);
    return opened;
}

static void status_ui_complete_menu_keyboard_edit(const ui_menu_item_t *item,
                                                  ui_keyboard_result_t result,
                                                  const char *text)
{
    if (item == NULL) {
        return;
    }
    if (result == UI_KEYBOARD_RESULT_CANCEL) {
        ui_runtime_set_toast(&s_ui, UI_TOAST_INFO, "Input cancelled", 1200u);
        return;
    }
    if (result != UI_KEYBOARD_RESULT_OK) {
        return;
    }

    switch (item->action) {
    case UI_ACTION_WIFI_ENTER_SSID: {
        ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(&s_ui, item);
        if (wifi == NULL) return;
        snprintf(wifi->ssid, sizeof(wifi->ssid), "%s", text != NULL ? text : "");
        wifi->has_selected_ssid = wifi->ssid[0] != '\0';
        if (!wifi->has_selected_ssid) {
            ui_runtime_set_toast(&s_ui, UI_TOAST_WARNING, "SSID required", 2000u);
            return;
        }
        (void)status_ui_wifi_try_saved_password(&s_ui, wifi, item->flow, true);
        break;
    }
    case UI_ACTION_WIFI_ENTER_PASSWORD: {
        ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(&s_ui, item);
        if (wifi == NULL) return;
        snprintf(wifi->password, sizeof(wifi->password), "%s", text != NULL ? text : "");
        wifi->has_password = true;
        if (status_ui_wifi_connect_and_save(&s_ui, wifi)) {
            if (item->flow == UI_FLOW_CONFIG_WIFI) status_ui_set_service_enabled(true);
            (void)ui_nav_enter(&s_ui.nav, item->target);
        } else {
            wifi->password[0] = '\0';
            wifi->has_password = false;
            (void)status_ui_begin_wifi_password_edit(&s_ui, item->flow, (item->flags & 1u) != 0u);
        }
        break;
    }
    case UI_ACTION_AP_ENTER_NAME:
        snprintf(s_ui.ap.ap_name, sizeof(s_ui.ap.ap_name), "%s", text != NULL ? text : "");
        (void)status_ui_ap_save_config(&s_ui);
        (void)ui_nav_enter(&s_ui.nav, item->target);
        break;
    case UI_ACTION_AP_ENTER_PASSWORD:
        snprintf(s_ui.ap.ap_password, sizeof(s_ui.ap.ap_password), "%s", text != NULL ? text : "");
        (void)status_ui_ap_save_config(&s_ui);
        (void)ui_nav_enter(&s_ui.nav, item->target);
        break;
    case UI_ACTION_AP_ENTER_CHANNEL: {
        unsigned parsed = (unsigned)strtoul(text != NULL ? text : "", NULL, 10);
        if (parsed < UI_AP_CHANNEL_MIN || parsed > UI_AP_CHANNEL_MAX) {
            ui_runtime_set_toast(&s_ui, UI_TOAST_WARNING, "Channel 1-13 only", 2500u);
            return;
        }
        s_ui.ap.channel = (uint8_t)parsed;
        (void)status_ui_ap_save_config(&s_ui);
        (void)ui_nav_enter(&s_ui.nav, item->target);
        break;
    }
    default:
        break;
    }
    s_ui.dirty = true;
}

static void status_ui_keyboard_handle_menu_event(status_ui_keyboard_event_t event)
{
    ui_keyboard_result_t result = UI_KEYBOARD_RESULT_NONE;
    ui_menu_item_t item = {0};
    char text[STATUS_UI_KEYBOARD_MAX_TEXT + 1] = {0};

    portENTER_CRITICAL(&s_state_mux);
    if (!ui_keyboard_session_edit()->active) {
        portEXIT_CRITICAL(&s_state_mux);
        keyboard_queue_event(event);
        return;
    }
    switch (event) {
    case STATUS_UI_KEYBOARD_EVENT_SELECT:
        ui_keyboard_handle_select(ui_keyboard_session_state(), (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
        break;
    case STATUS_UI_KEYBOARD_EVENT_NEXT:
        ui_keyboard_handle_next(ui_keyboard_session_state());
        break;
    case STATUS_UI_KEYBOARD_EVENT_PREV:
        ui_keyboard_handle_prev(ui_keyboard_session_state());
        break;
    case STATUS_UI_KEYBOARD_EVENT_OK:
        ui_keyboard_commit_pending(ui_keyboard_session_state());
        ui_keyboard_session_state()->result = UI_KEYBOARD_RESULT_OK;
        break;
    default:
        break;
    }
    result = ui_keyboard_session_state()->result;
    if (result != UI_KEYBOARD_RESULT_NONE) {
        item = ui_keyboard_session_edit()->item;
        snprintf(text, sizeof(text), "%s", ui_keyboard_session_state()->text);
        ui_keyboard_session_edit()->active = false;
        ui_keyboard_close(ui_keyboard_session_state());
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (result != UI_KEYBOARD_RESULT_NONE) {
        status_ui_complete_menu_keyboard_edit(&item, result, text);
    }
}


static void status_ui_render_keyboard_lcd(void)
{
    ui_keyboard_state_t kb;
    keyboard_snapshot(&kb);
    ui_render_keyboard_overlay(&kb);
}

static void keyboard_move(int delta)
{
    portENTER_CRITICAL(&s_state_mux);
    if (delta > 0) ui_keyboard_handle_next(ui_keyboard_session_state()); else ui_keyboard_handle_prev(ui_keyboard_session_state());
    portEXIT_CRITICAL(&s_state_mux);
}

static void keyboard_select(void)
{
    portENTER_CRITICAL(&s_state_mux);
    ui_keyboard_handle_select(ui_keyboard_session_state(), (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    portEXIT_CRITICAL(&s_state_mux);
}

static bool status_ui_keyboard_read_line_mode(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, ui_keyboard_mode_t mode, bool secret, uint32_t timeout_ms)
{
    if (out == NULL || out_len == 0 || max_len == 0 || s_virtual_keyboard_queue == NULL || !status_lcd_is_ready()) return false;
    if (max_len >= out_len) max_len = out_len - 1u;
    if (max_len > STATUS_UI_KEYBOARD_MAX_TEXT) max_len = STATUS_UI_KEYBOARD_MAX_TEXT;
    xQueueReset(s_virtual_keyboard_queue);
    portENTER_CRITICAL(&s_state_mux);
    (void)ui_keyboard_open(ui_keyboard_session_state(), title, initial, max_len, mode, secret);
    portEXIT_CRITICAL(&s_state_mux);
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    bool ok = false;
    while (true) {
        status_ui_keyboard_event_t event;
        TickType_t wait = pdMS_TO_TICKS(100);
        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            if (elapsed >= timeout) break;
            TickType_t remaining = timeout - elapsed;
            if (remaining < wait) wait = remaining;
        }
        if (xQueueReceive(s_virtual_keyboard_queue, &event, wait) == pdTRUE) {
            if (event == STATUS_UI_KEYBOARD_EVENT_SELECT) keyboard_select();
            else if (event == STATUS_UI_KEYBOARD_EVENT_NEXT) keyboard_move(1);
            else if (event == STATUS_UI_KEYBOARD_EVENT_PREV) keyboard_move(-1);
            else if (event == STATUS_UI_KEYBOARD_EVENT_OK) {
                portENTER_CRITICAL(&s_state_mux);
                ui_keyboard_commit_pending(ui_keyboard_session_state());
                ui_keyboard_session_state()->result = UI_KEYBOARD_RESULT_OK;
                portEXIT_CRITICAL(&s_state_mux);
            }
        }
        portENTER_CRITICAL(&s_state_mux);
        ui_keyboard_result_t result = ui_keyboard_session_state()->result;
        if (result == UI_KEYBOARD_RESULT_OK) snprintf(out, out_len, "%s", ui_keyboard_session_state()->text);
        if (result != UI_KEYBOARD_RESULT_NONE) ui_keyboard_close(ui_keyboard_session_state());
        portEXIT_CRITICAL(&s_state_mux);
        if (result != UI_KEYBOARD_RESULT_NONE) { ok = result == UI_KEYBOARD_RESULT_OK; break; }
    }
    portENTER_CRITICAL(&s_state_mux);
    ui_keyboard_session_state()->active = false;
    portEXIT_CRITICAL(&s_state_mux);
    return ok;
}

bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms)
{
    return status_ui_keyboard_read_line_mode(title, initial, out, out_len, max_len, secret ? UI_KEYBOARD_MODE_PASSWORD : UI_KEYBOARD_MODE_TEXT, secret, timeout_ms);
}

static void status_ui_format_time_24h(char out[6])
{
    if (out == NULL) return;
    if (!app_time_format_hhmm_24h(out)) {
        uint32_t uptime_minutes = (uint32_t)((xTaskGetTickCount() * portTICK_PERIOD_MS) / 60000u);
        uint32_t hour = (uptime_minutes / 60u) % 24u;
        uint32_t minute = uptime_minutes % 60u;
        snprintf(out, 6, "%02u:%02u", (unsigned)hour, (unsigned)minute);
    }
}

static void status_ui_update_time(ui_status_bar_state_t *bar)
{
    if (bar == NULL) return;
    status_ui_format_time_24h(bar->time_hhmm);
    bar->time_valid = strcmp(bar->time_hhmm, "--:--") != 0;
}

#if defined(__GNUC__)
extern bool board_power_get_battery_percent(uint8_t *out_percent) __attribute__((weak));
#endif

static void status_ui_update_battery(ui_status_bar_state_t *bar)
{
    if (bar == NULL) return;
    bar->battery_valid = false;
    bar->battery_percent = 0u;
#if defined(__GNUC__)
    if (board_power_get_battery_percent != NULL) {
        uint8_t percent = 0u;
        if (board_power_get_battery_percent(&percent)) {
            if (percent > 100u) {
                percent = 100u;
            }
            bar->battery_percent = percent;
            bar->battery_valid = true;
        }
    }
#endif
}

static void status_ui_render_menu_lcd(ui_runtime_t *ui)
{
    status_ui_update_time(&ui->status_bar);
    status_ui_update_battery(&ui->status_bar);
    ui->status_bar.wifi_connected = app_wifi_is_sta_connected();
    if (ui->nav.current == UI_SCREEN_CONNECT_BLUETOOTH) {
        status_ui_bluetooth_refresh(ui);
    }
    if (ui->toast.kind != UI_TOAST_NONE && ui->toast.until_tick_ms != 0u) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (now_ms > ui->toast.until_tick_ms) {
            ui_runtime_clear_toast(ui);
        }
    }
    ui_render_screen(ui, ui_nav_current(&ui->nav));
    ui_render_toast(&ui->toast);
}

static void status_ui_render_lcd(void *ctx)
{
    (void)ctx;
    status_ui_render_menu_lcd(&s_ui);
    if (keyboard_is_active()) {
        status_ui_render_keyboard_lcd();
    }
}

#else
bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms)
{
    (void)title;
    (void)initial;
    (void)out;
    (void)out_len;
    (void)max_len;
    (void)secret;
    (void)timeout_ms;
    return false;
}
#endif

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers)
{
#if CONFIG_APP_STATUS_UI_LCD
    if (s_virtual_keyboard_queue == NULL) {
        s_virtual_keyboard_queue = xQueueCreate(8, sizeof(status_ui_keyboard_event_t));
        if (s_virtual_keyboard_queue == NULL) {
            ESP_LOGE(TAG, "failed to create virtual keyboard queue");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_input_queue == NULL) {
        s_input_queue = xQueueCreate(8, sizeof(status_ui_input_t));
        if (s_input_queue == NULL) {
            ESP_LOGE(TAG, "failed to create status UI input queue");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return ESP_ERR_NO_MEM;
        }
        BaseType_t input_created = xTaskCreate(status_ui_input_task, "status_ui_input",
                                               STATUS_UI_INPUT_TASK_STACK, NULL,
                                               STATUS_UI_INPUT_TASK_PRIORITY, NULL);
        if (input_created != pdPASS) {
            ESP_LOGE(TAG, "failed to start status UI input task");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    ui_runtime_init(&s_ui);

    if (handlers != NULL) {
        memcpy(&s_handlers, handlers, sizeof(s_handlers));
    } else {
        memset(&s_handlers, 0, sizeof(s_handlers));
    }

    uint64_t pin_mask = (1ULL << BOARD_BUTTON_KEY1_GPIO) |
                        (1ULL << BOARD_BUTTON_KEY2_GPIO);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure StickS3 keys: %s", esp_err_to_name(err));
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return err;
    }

    BaseType_t created = xTaskCreate(status_ui_button_task, "status_ui_buttons",
                                     STATUS_UI_TASK_STACK, NULL,
                                     STATUS_UI_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to start button task");
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_APP_STATUS_UI_LCD
    err = status_lcd_init(status_ui_render_lcd, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD debug UI disabled: %s", esp_err_to_name(err));
    }
#endif

    ESP_LOGI(TAG, "status UI ready; StickS3 keys KEY1=%d KEY2=%d",
             BOARD_BUTTON_KEY1_GPIO, BOARD_BUTTON_KEY2_GPIO);
    ESP_LOGI(TAG, "status: %s", status_ui_state_name(status_ui_get_state()));
    ESP_LOGI(TAG, "web UI service: %s", bool_label(status_ui_get_service_enabled()));
    return ESP_OK;
}
