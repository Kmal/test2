#include "status_ui.h"

#include "board_sticks3.h"
#include "audio_metrics.h"
#include "app_wifi.h"
#include "ui_nav.h"
#include "ui_model.h"

#include <ctype.h>
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

#if CONFIG_APP_STATUS_UI_LCD
#include "board_i2c.h"
#include "m5pm1.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#endif

#define STATUS_UI_DEBOUNCE_MS 50
#define STATUS_UI_POLL_MS 25
#define STATUS_UI_TASK_STACK 2048
#define STATUS_UI_TASK_PRIORITY 5

#if CONFIG_APP_STATUS_UI_LCD
#define STATUS_UI_LCD_TASK_STACK 4096
#define STATUS_UI_LCD_TASK_PRIORITY 4
#ifdef CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#define STATUS_UI_LCD_REFRESH_MS CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#else
#define STATUS_UI_LCD_REFRESH_MS 100
#endif
#ifdef CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#define STATUS_UI_KEYBOARD_TIMEOUT_MS CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#else
#define STATUS_UI_KEYBOARD_TIMEOUT_MS 0
#endif
#define STATUS_UI_LCD_TEXT_SCALE 2
#define STATUS_UI_LCD_BG 0x0000
#define STATUS_UI_LCD_HEADER_BG 0x001F
#define STATUS_UI_LCD_TEXT 0xFFFF
#define STATUS_UI_LCD_DIM 0x8410
#define STATUS_UI_LCD_OK 0x07E0
#define STATUS_UI_LCD_WARN 0xFFE0
#define STATUS_UI_LCD_ERR 0xF800
#define STATUS_UI_LCD_LINE_HEIGHT 16
#define STATUS_UI_LCD_LEFT_PAD 4
#define STATUS_UI_LCD_TOP_PAD 4
#define STATUS_UI_KEYBOARD_MAX_TEXT UI_TEXT_WIFI_PASSWORD_MAX
#define STATUS_UI_KEYBOARD_LONG_MS 600
#define UI_LCD_W BOARD_LCD_H_RES
#define UI_LCD_H BOARD_LCD_V_RES
#define UI_STATUS_BAR_H 18
#define UI_TITLE_Y 22
#define UI_BODY_Y 42
#define UI_LINE_H 16
#define UI_BOTTOM_HINT_H 14
#define UI_LEFT_PAD 4
#define UI_RIGHT_PAD 4
#define UI_MENU_MAX_VISIBLE_ROWS 9
#define UI_COLOR_BG STATUS_UI_LCD_BG
#define UI_COLOR_BAR STATUS_UI_LCD_HEADER_BG
#define UI_COLOR_TEXT STATUS_UI_LCD_TEXT
#define UI_COLOR_DIM STATUS_UI_LCD_DIM
#define UI_COLOR_OK STATUS_UI_LCD_OK
#define UI_COLOR_WARN STATUS_UI_LCD_WARN
#define UI_COLOR_ERR STATUS_UI_LCD_ERR
#define UI_KEYBOARD_KEY_COUNT 14
#define UI_KEYBOARD_CHAR_KEY_COUNT 9
#define UI_KEYBOARD_CONTROL_KEY_COUNT 5
#define UI_KEYBOARD_OVERLAY_H 108
#define UI_KEYBOARD_MULTI_TAP_TIMEOUT_MS 800
#endif

static const char *TAG = "STATUS_UI";

static status_ui_button_handlers_t s_handlers;
static status_ui_state_t s_state = STATUS_UI_STATE_BOOTING;
static bool s_monitoring_enabled = false;
static bool s_service_enabled = false;
static uint32_t s_key1_press_count = 0;
static uint32_t s_key2_press_count = 0;
static status_ui_sound_meter_snapshot_t s_sound_snapshot;
static app_display_mode_t s_display_mode = APP_DISPLAY_VU;
static ui_runtime_t s_ui;
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;

#if CONFIG_APP_STATUS_UI_LCD
typedef enum {
    STATUS_UI_KEYBOARD_EVENT_SELECT = 0,
    STATUS_UI_KEYBOARD_EVENT_NEXT,
    STATUS_UI_KEYBOARD_EVENT_PREV,
    STATUS_UI_KEYBOARD_EVENT_OK,
} status_ui_keyboard_event_t;
static bool keyboard_is_active(void);
static void keyboard_queue_event(status_ui_keyboard_event_t event);
static bool status_ui_keyboard_active_locked(void);
static void status_ui_route_button_to_keyboard(status_ui_input_t input);
static void status_ui_route_button_to_menu(status_ui_input_t input);
#endif

#if CONFIG_APP_STATUS_UI_LCD
typedef enum {
    UI_KEYBOARD_MODE_TEXT = 0,
    UI_KEYBOARD_MODE_PASSWORD,
    UI_KEYBOARD_MODE_NUMERIC,
    UI_KEYBOARD_MODE_SYMBOL
} ui_keyboard_mode_t;

typedef enum {
    UI_KEYBOARD_RESULT_NONE = 0,
    UI_KEYBOARD_RESULT_OK,
    UI_KEYBOARD_RESULT_CANCEL
} ui_keyboard_result_t;

typedef enum {
    UI_KEY_KIND_CHAR = 0,
    UI_KEY_KIND_OK,
    UI_KEY_KIND_DELETE,
    UI_KEY_KIND_SPACE,
    UI_KEY_KIND_MODE,
    UI_KEY_KIND_CANCEL
} ui_key_kind_t;

typedef struct {
    ui_key_kind_t kind;
    const char *label;
    const char *chars_text;
    const char *chars_symbol;
    char numeric_char;
} ui_key_def_t;

typedef struct {
    bool active;
    ui_keyboard_mode_t mode;
    ui_keyboard_result_t result;
    bool secret;
    char title[24];
    char text[STATUS_UI_KEYBOARD_MAX_TEXT + 1];
    size_t max_len;
    uint8_t selected_key;
    uint8_t last_key;
    uint8_t cycle_index;
    bool has_pending_cycle;
    uint32_t last_key_tick_ms;
} ui_keyboard_state_t;

typedef struct {
    bool active;
    ui_menu_item_t item;
} ui_keyboard_menu_edit_t;

static QueueHandle_t s_keyboard_queue;
static ui_keyboard_state_t s_keyboard;
static ui_keyboard_menu_edit_t s_keyboard_edit;
static bool ui_keyboard_open(ui_keyboard_state_t *kb, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret);
static void ui_keyboard_close(ui_keyboard_state_t *kb);
static void ui_keyboard_handle_next(ui_keyboard_state_t *kb);
static void ui_keyboard_handle_prev(ui_keyboard_state_t *kb);
static void ui_keyboard_handle_select(ui_keyboard_state_t *kb);
static void ui_keyboard_commit_pending(ui_keyboard_state_t *kb);
static bool status_ui_keyboard_open_menu_edit(ui_runtime_t *ui, const ui_menu_item_t *item, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret);
static void status_ui_keyboard_handle_menu_event(status_ui_keyboard_event_t event);
static bool status_ui_keyboard_read_line_mode(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, ui_keyboard_mode_t mode, bool secret, uint32_t timeout_ms);
#endif

#if CONFIG_APP_STATUS_UI_LCD
static esp_lcd_panel_io_handle_t s_panel_io;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_framebuffer;
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
    bool long_sent;
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

void status_ui_set_monitoring_enabled(bool enabled)
{
    portENTER_CRITICAL(&s_state_mux);
    s_monitoring_enabled = enabled;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(enabled));
}

bool status_ui_get_monitoring_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_state_mux);
    enabled = s_monitoring_enabled;
    portEXIT_CRITICAL(&s_state_mux);
    return enabled;
}

void status_ui_set_service_enabled(bool enabled)
{
    portENTER_CRITICAL(&s_state_mux);
    s_service_enabled = enabled;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "transport service: %s", bool_label(enabled));
}

bool status_ui_get_service_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_state_mux);
    enabled = s_service_enabled;
    portEXIT_CRITICAL(&s_state_mux);
    return enabled;
}

void status_ui_set_sound_meter_snapshot(const status_ui_sound_meter_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_mux);
    s_sound_snapshot = *snapshot;
    portEXIT_CRITICAL(&s_state_mux);
}

bool status_ui_get_sound_meter_snapshot(status_ui_sound_meter_snapshot_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_state_mux);
    *out = s_sound_snapshot;
    portEXIT_CRITICAL(&s_state_mux);
    return out->valid;
}

void status_ui_set_display_mode(app_display_mode_t mode)
{
    portENTER_CRITICAL(&s_state_mux);
    s_display_mode = mode;
    s_sound_snapshot.display_mode = (uint8_t)mode;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "display mode: %s", app_display_mode_name(mode));
}

app_display_mode_t status_ui_get_display_mode(void)
{
    app_display_mode_t mode;
    portENTER_CRITICAL(&s_state_mux);
    mode = s_display_mode;
    portEXIT_CRITICAL(&s_state_mux);
    return mode;
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

static bool status_ui_wifi_connect_and_save(ui_runtime_t *ui, ui_wifi_flow_state_t *wifi)
{
    if (ui == NULL || wifi == NULL || wifi->ssid[0] == '\0') {
        ui_runtime_set_toast(ui, UI_TOAST_WARNING, "SSID required", 2000u);
        return false;
    }
    bool ok = app_wifi_connect(wifi->ssid, wifi->password, true);
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? "Wi-Fi saved" : "Wi-Fi connect failed", 2500u);
    return ok;
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
    ui_runtime_set_toast(ui, ok ? UI_TOAST_SUCCESS : UI_TOAST_ERROR, ok ? "AP Mode started" : "AP start failed", 2500u);
    return ok;
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
    (void)ui_nav_enter(&ui->nav, item->target);
    return true;
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
    status_ui_sound_meter_snapshot_t snapshot;
    if (status_ui_get_sound_meter_snapshot(&snapshot)) {
        ui->bluetooth.ble_connected = snapshot.ble_connected;
        ui->bluetooth.metrics_notify_enabled = snapshot.ble_metrics_notify_enabled;
        ui->bluetooth.pcm_notify_enabled = snapshot.ble_pcm_notify_enabled;
    }
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

static const rule_source_t s_trigger_preset_sources[] = { RULE_SOURCE_SOUND_RMS_DBFS, RULE_SOURCE_SOUND_PEAK_DBFS, RULE_SOURCE_KEY1_SHORT, RULE_SOURCE_KEY2_SHORT, RULE_SOURCE_BLE_CONNECTED, RULE_SOURCE_WIFI_CONNECTED };
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
        activate = true;
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
            s_ui.menu_active = false;
        } else if (s_ui.menu_active) {
            (void)ui_nav_back(&s_ui.nav);
        }
        s_ui.dirty = true;
        break;
    default:
        break;
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (activate) {
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
    active = s_keyboard.active;
    portEXIT_CRITICAL(&s_state_mux);
    return active;
}

static void keyboard_queue_event(status_ui_keyboard_event_t event)
{
    if (s_keyboard_queue != NULL) {
        (void)xQueueSend(s_keyboard_queue, &event, 0);
    }
}

static bool status_ui_keyboard_active_locked(void)
{
    return s_keyboard.active;
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
    if (s_keyboard_edit.active) {
        status_ui_keyboard_handle_menu_event(event);
    } else {
        keyboard_queue_event(event);
    }
}

static void status_ui_route_button_to_menu(status_ui_input_t input)
{
    status_ui_handle_input(input);
}
#endif

static void maybe_dispatch_button(status_button_t *button, TickType_t now)
{
    bool pressed = button_is_pressed(button->gpio);

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
        (now - button->pressed_since_tick) >= pdMS_TO_TICKS(STATUS_UI_KEYBOARD_LONG_MS)) {
        if (keyboard_is_active() && button->gpio == BOARD_BUTTON_KEY1_GPIO) {
            button->long_sent = true;
            if (s_keyboard_edit.active) {
                status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_OK);
            } else {
                keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_OK);
            }
        } else if (keyboard_is_active() && button->gpio == BOARD_BUTTON_KEY2_GPIO) {
            button->long_sent = true;
            if (s_keyboard_edit.active) {
                status_ui_keyboard_handle_menu_event(STATUS_UI_KEYBOARD_EVENT_PREV);
            } else {
                keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_PREV);
            }
        } else if (s_ui.menu_active && button->gpio == BOARD_BUTTON_KEY2_GPIO) {
            button->long_sent = true;
            status_ui_route_button_to_menu(STATUS_UI_INPUT_BACK);
        } else if (!s_ui.menu_active && button->gpio == BOARD_BUTTON_KEY1_GPIO) {
            button->long_sent = true;
            status_ui_open_screen(UI_SCREEN_MAIN);
        }
    }
#endif

    if (pressed == button->stable_pressed) {
        return;
    }

    button->stable_pressed = pressed;
#if CONFIG_APP_STATUS_UI_LCD
    if (keyboard_is_active()) {
        if (pressed) {
            record_button_press(button->gpio);
            button->pressed_since_tick = now;
            button->long_sent = false;
            ESP_LOGI(TAG, "keyboard button pressed: %s", button->name);
        } else if (!button->long_sent) {
            status_ui_keyboard_event_t event = button->gpio == BOARD_BUTTON_KEY1_GPIO ?
                                             STATUS_UI_KEYBOARD_EVENT_SELECT : STATUS_UI_KEYBOARD_EVENT_NEXT;
            if (s_keyboard_edit.active) {
                status_ui_keyboard_handle_menu_event(event);
            } else {
                keyboard_queue_event(event);
            }
        }
        return;
    }
#endif
#if CONFIG_APP_STATUS_UI_LCD
    if (s_ui.menu_active) {
        if (pressed) {
            record_button_press(button->gpio);
            button->pressed_since_tick = now;
            button->long_sent = false;
            ESP_LOGI(TAG, "menu button pressed: %s", button->name);
        } else if (!button->long_sent) {
            status_ui_route_button_to_menu(button->gpio == BOARD_BUTTON_KEY1_GPIO ?
                                           STATUS_UI_INPUT_NEXT : STATUS_UI_INPUT_SELECT);
        }
        return;
    }
#endif
    if (pressed) {
        record_button_press(button->gpio);
#if CONFIG_APP_STATUS_UI_LCD
        button->pressed_since_tick = now;
        button->long_sent = false;
#endif
        ESP_LOGI(TAG, "button pressed: %s", button->name);
        if (button->handler != NULL) {
            button->handler(s_handlers.ctx);
        }
    }
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
static const uint8_t *glyph_rows(char c)
{
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t glyph_0[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t glyph_1[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t glyph_2[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t glyph_3[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t glyph_4[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t glyph_5[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t glyph_6[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t glyph_7[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t glyph_8[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t glyph_9[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t glyph_d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t glyph_f[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
    static const uint8_t glyph_h[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_i[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t glyph_j[7] = {7, 2, 2, 2, 18, 18, 12};
    static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t glyph_l[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t glyph_m[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t glyph_q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t glyph_w[7] = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t glyph_x[7] = {17, 17, 10, 4, 10, 17, 17};
    static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};
    static const uint8_t glyph_z[7] = {31, 1, 2, 4, 8, 16, 31};
    static const uint8_t glyph_colon[7] = {0, 4, 4, 0, 4, 4, 0};
    static const uint8_t glyph_dash[7] = {0, 0, 0, 14, 0, 0, 0};
    static const uint8_t glyph_dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t glyph_slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const uint8_t glyph_percent[7] = {24, 25, 2, 4, 8, 19, 3};
    static const uint8_t glyph_at[7] = {14, 17, 23, 21, 23, 16, 14};
    static const uint8_t glyph_underscore[7] = {0, 0, 0, 0, 0, 0, 31};
    static const uint8_t glyph_plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t glyph_question[7] = {14, 17, 1, 2, 4, 0, 4};
    static const uint8_t glyph_lparen[7] = {2, 4, 8, 8, 8, 4, 2};
    static const uint8_t glyph_rparen[7] = {8, 4, 2, 2, 2, 4, 8};
    static const uint8_t glyph_hash[7] = {10, 31, 10, 10, 31, 10, 0};
    static const uint8_t glyph_dollar[7] = {4, 15, 20, 14, 5, 30, 4};
    static const uint8_t glyph_amp[7] = {12, 18, 20, 8, 21, 18, 13};
    static const uint8_t glyph_equal[7] = {0, 0, 31, 0, 31, 0, 0};
    static const uint8_t glyph_bang[7] = {4, 4, 4, 4, 4, 0, 4};
    static const uint8_t glyph_caret[7] = {4, 10, 17, 0, 0, 0, 0};
    static const uint8_t glyph_star[7] = {0, 21, 14, 31, 14, 21, 0};
    static const uint8_t glyph_gt[7] = {16, 8, 4, 2, 4, 8, 16};

    switch ((char)toupper((unsigned char)c)) {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'A': return glyph_a;
    case 'B': return glyph_b;
    case 'C': return glyph_c;
    case 'D': return glyph_d;
    case 'E': return glyph_e;
    case 'F': return glyph_f;
    case 'G': return glyph_g;
    case 'H': return glyph_h;
    case 'I': return glyph_i;
    case 'J': return glyph_j;
    case 'K': return glyph_k;
    case 'L': return glyph_l;
    case 'M': return glyph_m;
    case 'N': return glyph_n;
    case 'O': return glyph_o;
    case 'P': return glyph_p;
    case 'Q': return glyph_q;
    case 'R': return glyph_r;
    case 'S': return glyph_s;
    case 'T': return glyph_t;
    case 'U': return glyph_u;
    case 'V': return glyph_v;
    case 'W': return glyph_w;
    case 'X': return glyph_x;
    case 'Y': return glyph_y;
    case 'Z': return glyph_z;
    case ':': return glyph_colon;
    case '-': return glyph_dash;
    case '.': return glyph_dot;
    case '/': return glyph_slash;
    case '%': return glyph_percent;
    case '@': return glyph_at;
    case '_': return glyph_underscore;
    case '+': return glyph_plus;
    case '?': return glyph_question;
    case '(': return glyph_lparen;
    case ')': return glyph_rparen;
    case '#': return glyph_hash;
    case '$': return glyph_dollar;
    case '&': return glyph_amp;
    case '=': return glyph_equal;
    case '!': return glyph_bang;
    case '^': return glyph_caret;
    case '*': return glyph_star;
    case '>': return glyph_gt;
    default: return blank;
    }
}

static void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int x_end = x + w;
    int y_end = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x_end > BOARD_LCD_H_RES) {
        x_end = BOARD_LCD_H_RES;
    }
    if (y_end > BOARD_LCD_V_RES) {
        y_end = BOARD_LCD_V_RES;
    }
    for (int row = y; row < y_end; ++row) {
        for (int col = x; col < x_end; ++col) {
            s_framebuffer[row * BOARD_LCD_H_RES + col] = color;
        }
    }
}

static void lcd_draw_char(int x, int y, char c, uint16_t color, uint8_t scale)
{
    const uint8_t *rows = glyph_rows(c);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1U << (4 - col))) != 0) {
                lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void lcd_draw_text(int x, int y, const char *text, uint16_t color, uint8_t scale)
{
    int cursor_x = x;
    while (*text != '\0' && cursor_x < BOARD_LCD_H_RES) {
        if (*text != ' ') {
            lcd_draw_char(cursor_x, y, *text, color, scale);
        }
        cursor_x += 6 * scale;
        ++text;
    }
}


static const ui_key_def_t s_9key_defs[UI_KEYBOARD_CHAR_KEY_COUNT] = {
    { UI_KEY_KIND_CHAR, "1", "1.,!?", "1.,!?", '1' },
    { UI_KEY_KIND_CHAR, "2", "2abc", "2@#$", '2' },
    { UI_KEY_KIND_CHAR, "3", "3def", "3%&*", '3' },
    { UI_KEY_KIND_CHAR, "4", "4ghi", "4-_+", '4' },
    { UI_KEY_KIND_CHAR, "5", "5jkl", "5=:/", '5' },
    { UI_KEY_KIND_CHAR, "6", "6mno", "6;()'", '6' },
    { UI_KEY_KIND_CHAR, "7", "7pqrs", "7[]{}", '7' },
    { UI_KEY_KIND_CHAR, "8", "8tuv", "8<>\\", '8' },
    { UI_KEY_KIND_CHAR, "9", "9wxyz", "9`~|", '9' },
};

static const ui_key_def_t s_9key_controls[UI_KEYBOARD_CONTROL_KEY_COUNT] = {
    { UI_KEY_KIND_OK, "OK", NULL, NULL, 0 },
    { UI_KEY_KIND_DELETE, "DEL", NULL, NULL, 0 },
    { UI_KEY_KIND_SPACE, "SPC", NULL, NULL, 0 },
    { UI_KEY_KIND_MODE, "MODE", NULL, NULL, 0 },
    { UI_KEY_KIND_CANCEL, "ESC", NULL, NULL, 0 },
};

static void lcd_draw_text_clipped(int x, int y, const char *text, uint16_t color, uint8_t scale, size_t max_chars)
{
    char buf[40];
    if (max_chars >= sizeof(buf)) max_chars = sizeof(buf) - 1u;
    size_t i = 0;
    while (i < max_chars && text != NULL && text[i] != '\0') { buf[i] = text[i]; ++i; }
    buf[i] = '\0';
    lcd_draw_text(x, y, buf, color, scale);
}

static void keyboard_snapshot(ui_keyboard_state_t *out)
{
    portENTER_CRITICAL(&s_state_mux);
    *out = s_keyboard;
    portEXIT_CRITICAL(&s_state_mux);
}

static const char *ui_keyboard_chars_for_key(const ui_keyboard_state_t *kb, const ui_key_def_t *key)
{
    if (kb->mode == UI_KEYBOARD_MODE_NUMERIC) return NULL;
    return kb->mode == UI_KEYBOARD_MODE_SYMBOL ? key->chars_symbol : key->chars_text;
}

static void ui_keyboard_append_char(ui_keyboard_state_t *kb, char ch)
{
    size_t len = strlen(kb->text);
    if (len < kb->max_len && len + 1u < sizeof(kb->text)) {
        kb->text[len] = ch;
        kb->text[len + 1u] = '\0';
    }
}

static void ui_keyboard_commit_pending(ui_keyboard_state_t *kb)
{
    kb->has_pending_cycle = false;
}

static void ui_keyboard_backspace(ui_keyboard_state_t *kb)
{
    size_t len = strlen(kb->text);
    if (len > 0u) kb->text[len - 1u] = '\0';
    kb->has_pending_cycle = false;
}

static bool ui_keyboard_open(ui_keyboard_state_t *kb, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret)
{
    if (kb == NULL || max_len == 0u) return false;
    memset(kb, 0, sizeof(*kb));
    kb->active = true;
    kb->mode = mode;
    kb->secret = secret;
    kb->max_len = max_len > STATUS_UI_KEYBOARD_MAX_TEXT ? STATUS_UI_KEYBOARD_MAX_TEXT : max_len;
    kb->selected_key = 0u;
    snprintf(kb->title, sizeof(kb->title), "%s", title != NULL ? title : "Input");
    snprintf(kb->text, sizeof(kb->text), "%s", initial != NULL ? initial : "");
    kb->text[kb->max_len] = '\0';
    return true;
}

static void ui_keyboard_close(ui_keyboard_state_t *kb)
{
    if (kb != NULL) kb->active = false;
}

static bool ui_keyboard_is_active(const ui_keyboard_state_t *kb)
{
    return kb != NULL && kb->active;
}


static bool status_ui_keyboard_open_menu_edit(ui_runtime_t *ui,
                                              const ui_menu_item_t *item,
                                              const char *title,
                                              const char *initial,
                                              size_t max_len,
                                              ui_keyboard_mode_t mode,
                                              bool secret)
{
    if (ui == NULL || item == NULL || s_panel == NULL || s_framebuffer == NULL) {
        return false;
    }
    if (max_len > STATUS_UI_KEYBOARD_MAX_TEXT) {
        max_len = STATUS_UI_KEYBOARD_MAX_TEXT;
    }
    portENTER_CRITICAL(&s_state_mux);
    bool opened = ui_keyboard_open(&s_keyboard, title, initial, max_len, mode, secret);
    if (opened) {
        s_keyboard_edit.active = true;
        s_keyboard_edit.item = *item;
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
        (void)ui_nav_enter(&s_ui.nav, item->target);
        break;
    }
    case UI_ACTION_WIFI_ENTER_PASSWORD: {
        ui_wifi_flow_state_t *wifi = status_ui_get_wifi_state_for_item(&s_ui, item);
        if (wifi == NULL) return;
        snprintf(wifi->password, sizeof(wifi->password), "%s", text != NULL ? text : "");
        wifi->has_password = true;
        (void)ui_nav_enter(&s_ui.nav, item->target);
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
    if (!s_keyboard_edit.active) {
        portEXIT_CRITICAL(&s_state_mux);
        keyboard_queue_event(event);
        return;
    }
    switch (event) {
    case STATUS_UI_KEYBOARD_EVENT_SELECT:
        ui_keyboard_handle_select(&s_keyboard);
        break;
    case STATUS_UI_KEYBOARD_EVENT_NEXT:
        ui_keyboard_handle_next(&s_keyboard);
        break;
    case STATUS_UI_KEYBOARD_EVENT_PREV:
        ui_keyboard_handle_prev(&s_keyboard);
        break;
    case STATUS_UI_KEYBOARD_EVENT_OK:
        ui_keyboard_commit_pending(&s_keyboard);
        s_keyboard.result = UI_KEYBOARD_RESULT_OK;
        break;
    default:
        break;
    }
    result = s_keyboard.result;
    if (result != UI_KEYBOARD_RESULT_NONE) {
        item = s_keyboard_edit.item;
        snprintf(text, sizeof(text), "%s", s_keyboard.text);
        s_keyboard_edit.active = false;
        ui_keyboard_close(&s_keyboard);
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (result != UI_KEYBOARD_RESULT_NONE) {
        status_ui_complete_menu_keyboard_edit(&item, result, text);
    }
}

static void ui_keyboard_handle_next(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    ui_keyboard_commit_pending(kb);
    kb->selected_key = (uint8_t)((kb->selected_key + 1u) % UI_KEYBOARD_KEY_COUNT);
}

static void ui_keyboard_handle_prev(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    ui_keyboard_commit_pending(kb);
    kb->selected_key = kb->selected_key == 0u ? UI_KEYBOARD_KEY_COUNT - 1u : kb->selected_key - 1u;
}

static void ui_keyboard_handle_select(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    if (kb->selected_key < UI_KEYBOARD_CHAR_KEY_COUNT) {
        const ui_key_def_t *key = &s_9key_defs[kb->selected_key];
        if (kb->mode == UI_KEYBOARD_MODE_NUMERIC) {
            ui_keyboard_append_char(kb, key->numeric_char);
            return;
        }
        const char *chars = ui_keyboard_chars_for_key(kb, key);
        if (chars == NULL || chars[0] == '\0') return;
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (kb->has_pending_cycle && (now_ms - kb->last_key_tick_ms) > UI_KEYBOARD_MULTI_TAP_TIMEOUT_MS) {
            ui_keyboard_commit_pending(kb);
        }
        if (kb->has_pending_cycle && kb->last_key == kb->selected_key && strlen(kb->text) > 0u) {
            kb->cycle_index = (uint8_t)((kb->cycle_index + 1u) % strlen(chars));
            kb->text[strlen(kb->text) - 1u] = chars[kb->cycle_index];
        } else {
            ui_keyboard_commit_pending(kb);
            kb->last_key = kb->selected_key;
            kb->cycle_index = 0u;
            kb->has_pending_cycle = true;
            ui_keyboard_append_char(kb, chars[0]);
        }
        kb->last_key_tick_ms = now_ms;
        return;
    }
    const ui_key_def_t *control = &s_9key_controls[kb->selected_key - UI_KEYBOARD_CHAR_KEY_COUNT];
    switch (control->kind) {
    case UI_KEY_KIND_OK: ui_keyboard_commit_pending(kb); kb->result = UI_KEYBOARD_RESULT_OK; break;
    case UI_KEY_KIND_DELETE: ui_keyboard_backspace(kb); break;
    case UI_KEY_KIND_SPACE: ui_keyboard_commit_pending(kb); ui_keyboard_append_char(kb, ' '); break;
    case UI_KEY_KIND_MODE:
        ui_keyboard_commit_pending(kb);
        if (kb->mode == UI_KEYBOARD_MODE_TEXT || kb->mode == UI_KEYBOARD_MODE_PASSWORD) kb->mode = UI_KEYBOARD_MODE_SYMBOL;
        else if (kb->mode == UI_KEYBOARD_MODE_SYMBOL) kb->mode = kb->secret ? UI_KEYBOARD_MODE_PASSWORD : UI_KEYBOARD_MODE_TEXT;
        break;
    case UI_KEY_KIND_CANCEL: kb->result = UI_KEYBOARD_RESULT_CANCEL; break;
    default: break;
    }
}

static void ui_render_keyboard_overlay(const ui_keyboard_state_t *kb)
{
    if (!ui_keyboard_is_active(kb)) return;
    int y0 = BOARD_LCD_V_RES - UI_KEYBOARD_OVERLAY_H;
    if (y0 < 0) y0 = 0;
    lcd_fill_rect(0, y0, BOARD_LCD_H_RES, UI_KEYBOARD_OVERLAY_H, 0x1082);
    lcd_draw_text_clipped(UI_LEFT_PAD, y0 + 4, kb->title, STATUS_UI_LCD_TEXT, 1, 18);
    char display[STATUS_UI_KEYBOARD_MAX_TEXT + 1];
    size_t len = strlen(kb->text);
    for (size_t i = 0; i < len && i < sizeof(display) - 1u; ++i) display[i] = kb->secret ? '*' : kb->text[i];
    display[len < sizeof(display) ? len : sizeof(display) - 1u] = '\0';
    lcd_draw_text_clipped(UI_LEFT_PAD, y0 + 18, display, STATUS_UI_LCD_OK, 1, 19);
    const int key_w = BOARD_LCD_H_RES / 5;
    for (uint8_t i = 0; i < UI_KEYBOARD_KEY_COUNT; ++i) {
        const bool selected = kb->selected_key == i;
        const int col = i % 5;
        const int row = i / 5;
        const int x = col * key_w;
        const int y = y0 + 34 + row * 22;
        const char *label = i < UI_KEYBOARD_CHAR_KEY_COUNT ? s_9key_defs[i].label : s_9key_controls[i - UI_KEYBOARD_CHAR_KEY_COUNT].label;
        lcd_fill_rect(x + 1, y, key_w - 2, 20, selected ? STATUS_UI_LCD_OK : 0x2104);
        lcd_draw_text(x + 4, y + 6, label, selected ? STATUS_UI_LCD_BG : STATUS_UI_LCD_TEXT, 1);
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
    if (delta > 0) ui_keyboard_handle_next(&s_keyboard); else ui_keyboard_handle_prev(&s_keyboard);
    portEXIT_CRITICAL(&s_state_mux);
}

static void keyboard_select(void)
{
    portENTER_CRITICAL(&s_state_mux);
    ui_keyboard_handle_select(&s_keyboard);
    portEXIT_CRITICAL(&s_state_mux);
}

static bool status_ui_keyboard_read_line_mode(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, ui_keyboard_mode_t mode, bool secret, uint32_t timeout_ms)
{
    if (out == NULL || out_len == 0 || max_len == 0 || s_keyboard_queue == NULL || s_panel == NULL || s_framebuffer == NULL) return false;
    if (max_len >= out_len) max_len = out_len - 1u;
    if (max_len > STATUS_UI_KEYBOARD_MAX_TEXT) max_len = STATUS_UI_KEYBOARD_MAX_TEXT;
    xQueueReset(s_keyboard_queue);
    portENTER_CRITICAL(&s_state_mux);
    (void)ui_keyboard_open(&s_keyboard, title, initial, max_len, mode, secret);
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
        if (xQueueReceive(s_keyboard_queue, &event, wait) == pdTRUE) {
            if (event == STATUS_UI_KEYBOARD_EVENT_SELECT) keyboard_select();
            else if (event == STATUS_UI_KEYBOARD_EVENT_NEXT) keyboard_move(1);
            else if (event == STATUS_UI_KEYBOARD_EVENT_PREV) keyboard_move(-1);
            else if (event == STATUS_UI_KEYBOARD_EVENT_OK) {
                portENTER_CRITICAL(&s_state_mux);
                ui_keyboard_commit_pending(&s_keyboard);
                s_keyboard.result = UI_KEYBOARD_RESULT_OK;
                portEXIT_CRITICAL(&s_state_mux);
            }
        }
        portENTER_CRITICAL(&s_state_mux);
        ui_keyboard_result_t result = s_keyboard.result;
        if (result == UI_KEYBOARD_RESULT_OK) snprintf(out, out_len, "%s", s_keyboard.text);
        if (result != UI_KEYBOARD_RESULT_NONE) ui_keyboard_close(&s_keyboard);
        portEXIT_CRITICAL(&s_state_mux);
        if (result != UI_KEYBOARD_RESULT_NONE) { ok = result == UI_KEYBOARD_RESULT_OK; break; }
    }
    portENTER_CRITICAL(&s_state_mux);
    s_keyboard.active = false;
    portEXIT_CRITICAL(&s_state_mux);
    return ok;
}

bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms)
{
    return status_ui_keyboard_read_line_mode(title, initial, out, out_len, max_len, secret ? UI_KEYBOARD_MODE_PASSWORD : UI_KEYBOARD_MODE_TEXT, secret, timeout_ms);
}

static uint16_t state_color(status_ui_state_t state)
{
    switch (state) {
    case STATUS_UI_STATE_READY:
        return STATUS_UI_LCD_OK;
    case STATUS_UI_STATE_BOOTING:
    case STATUS_UI_STATE_NO_TRANSPORT:
        return STATUS_UI_LCD_WARN;
    case STATUS_UI_STATE_ERROR:
        return STATUS_UI_LCD_ERR;
    default:
        return STATUS_UI_LCD_DIM;
    }
}


typedef struct {
    status_ui_state_t state;
    bool monitoring_enabled;
    bool service_enabled;
    uint32_t key1_count;
    uint32_t key2_count;
    status_ui_sound_meter_snapshot_t snapshot;
    app_display_mode_t display_mode;
} status_ui_lcd_context_t;

static const char *display_title(app_display_mode_t mode)
{
    switch (mode) {
    case APP_DISPLAY_VU:
        return "METER";
    case APP_DISPLAY_NUMERIC:
        return "NUMERIC";
    case APP_DISPLAY_BLE_STATUS:
        return "BLE";
    case APP_DISPLAY_DIAGNOSTICS:
        return "DIAG";
    default:
        return "APP";
    }
}

static void status_ui_render_app_shell(const char *title, const status_ui_lcd_context_t *ctx, uint16_t accent)
{
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, accent);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD, title, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

    char line[32];
    snprintf(line, sizeof(line), "K1 VIEW K2 APP");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, BOARD_LCD_V_RES - 18, line, STATUS_UI_LCD_DIM, 1);
    snprintf(line, sizeof(line), "%.10s", status_ui_state_name(ctx->state));
    lcd_draw_text(BOARD_LCD_H_RES - 64, BOARD_LCD_V_RES - 18, line, state_color(ctx->state), 1);
}

static void format_dbfs_q8(char *buf, size_t len, int32_t dbfs_q8)
{
    int32_t whole = dbfs_q8 / 256;
    int32_t frac = dbfs_q8 % 256;
    if (frac < 0) {
        frac = -frac;
    }
    frac = (frac * 10) / 256;
    snprintf(buf, len, "%ld.%ld", (long)whole, (long)frac);
}

static void lcd_draw_horizontal_bar(int x, int y, int w, int h, uint16_t percent, uint16_t fill, uint16_t bg)
{
    if (percent > 100) {
        percent = 100;
    }
    lcd_fill_rect(x, y, w, h, bg);
    lcd_fill_rect(x, y, (w * percent) / 100, h, fill);
}

static uint16_t vu_color(const status_ui_sound_meter_snapshot_t *snap)
{
    if ((snap->flags & AUDIO_METRICS_FLAG_CLIPPING) != 0 || snap->vu_percent >= 90) {
        return STATUS_UI_LCD_ERR;
    }
    if (snap->vu_percent >= 65) {
        return STATUS_UI_LCD_WARN;
    }
    return STATUS_UI_LCD_OK;
}

static void status_ui_render_vu_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    if (snap->calibration_active) {
        char line[32];
        lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 40, "KEEP QUIET", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
        snprintf(line, sizeof(line), "%lu/%lu", (unsigned long)snap->calibration_collected_windows,
                 (unsigned long)snap->calibration_required_windows);
        lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 68, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
        uint16_t percent = snap->calibration_required_windows == 0
                               ? 0
                               : (uint16_t)((snap->calibration_collected_windows * 100U) /
                                            snap->calibration_required_windows);
        lcd_draw_horizontal_bar(4, 100, BOARD_LCD_H_RES - 8, 24, percent, STATUS_UI_LCD_WARN, STATUS_UI_LCD_DIM);
        return;
    }
    char rms[16];
    char peak[16];
    char line[32];
    format_dbfs_q8(rms, sizeof(rms), snap->rms_dbfs_q8);
    format_dbfs_q8(peak, sizeof(peak), snap->peak_dbfs_q8);
    snprintf(line, sizeof(line), "RMS %sDB", rms);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 36, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PK %sDB", peak);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 56, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_horizontal_bar(4, 90, BOARD_LCD_H_RES - 8, 28, snap->vu_percent, vu_color(snap), STATUS_UI_LCD_DIM);
    snprintf(line, sizeof(line), "VU %u%%", (unsigned)snap->vu_percent);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 124, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "BLE %s", snap->ble_connected ? "ON" : "OFF");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 148, line, snap->ble_connected ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "MODE %.8s", app_mode_name((app_mode_t)snap->app_mode));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 168, line, STATUS_UI_LCD_DIM, 1);
    snprintf(line, sizeof(line), "CLIP %u", (unsigned)snap->clipped_samples);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 184, line, snap->clipped_samples ? STATUS_UI_LCD_ERR : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_numeric_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    char rms[16];
    char peak[16];
    char line[32];
    format_dbfs_q8(rms, sizeof(rms), snap->rms_dbfs_q8);
    format_dbfs_q8(peak, sizeof(peak), snap->peak_dbfs_q8);
    snprintf(line, sizeof(line), "RMS:%s", rms);
    lcd_draw_text(4, 34, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PK:%s", peak);
    lcd_draw_text(4, 54, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "VU:%u%%", (unsigned)snap->vu_percent);
    lcd_draw_text(4, 74, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "ZC:%lu", (unsigned long)snap->zero_crossings);
    lcd_draw_text(4, 94, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "FLG:%lx", (unsigned long)snap->flags);
    lcd_draw_text(4, 114, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "SEQ:%lu", (unsigned long)snap->sequence);
    lcd_draw_text(4, 134, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_ble_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    char line[32];
    snprintf(line, sizeof(line), "CONN %s", snap->ble_connected ? "YES" : "NO");
    lcd_draw_text(4, 36, line, snap->ble_connected ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "METR %s", snap->ble_metrics_notify_enabled ? "YES" : "NO");
    lcd_draw_text(4, 56, line, snap->ble_metrics_notify_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PCM %s", snap->ble_pcm_notify_enabled ? "YES" : "NO");
    lcd_draw_text(4, 76, line, snap->ble_pcm_notify_enabled ? STATUS_UI_LCD_WARN : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "SEQ %lu", (unsigned long)snap->sequence);
    lcd_draw_text(4, 96, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_diagnostics_lcd(const status_ui_lcd_context_t *ctx)
{
    int y = 32;
    char line[32];
    snprintf(line, sizeof(line), "STATE");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "%.12s", status_ui_state_name(ctx->state));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, state_color(ctx->state), STATUS_UI_LCD_TEXT_SCALE);

    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    snprintf(line, sizeof(line), "BLE SVC: %s", bool_label(ctx->service_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, ctx->service_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "MON: %s", bool_label(ctx->monitoring_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, ctx->monitoring_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "PCM: %d HZ", BOARD_I2S_SAMPLE_RATE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY1:%lu", (unsigned long)ctx->key1_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY2:%lu", (unsigned long)ctx->key2_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "UP:%lus", (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000U));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

#ifdef CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME
    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME, STATUS_UI_LCD_DIM, 1);
#endif
}

static void status_ui_render_waiting_lcd(void)
{
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 46, "WAITING", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 70, "FOR PCM", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 104, "CHECK I2S", STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 124, "CLOCK/CODEC", STATUS_UI_LCD_DIM, 1);
}



static void status_ui_format_time_24h(char out[6])
{
    if (out == NULL) return;
    ui_runtime_refresh_status_bar(&s_ui);
    snprintf(out, 6, "%s", s_ui.status_bar.time_valid ? s_ui.status_bar.time_hhmm : "--:--");
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

static void ui_render_clear(void)
{
    lcd_fill_rect(0, 0, UI_LCD_W, UI_LCD_H, UI_COLOR_BG);
}

static void ui_render_status_bar(const ui_status_bar_state_t *status)
{
    lcd_fill_rect(0, 0, UI_LCD_W, UI_STATUS_BAR_H, UI_COLOR_BAR);
    lcd_draw_text(UI_LEFT_PAD, 5, (status != NULL && status->time_valid) ? status->time_hhmm : "--:--", UI_COLOR_TEXT, 1);
    char battery[8];
    if (status != NULL && status->battery_valid) snprintf(battery, sizeof(battery), "%u%%", (unsigned)status->battery_percent);
    else snprintf(battery, sizeof(battery), "--%%");
    lcd_draw_text(UI_LCD_W - 28, 5, battery, UI_COLOR_TEXT, 1);
}

static void ui_render_title(const char *title)
{
    lcd_draw_text_clipped(UI_LEFT_PAD, UI_TITLE_Y, title != NULL ? title : "", UI_COLOR_TEXT, 1, 20);
}

static void ui_render_bottom_hints(const char *hints)
{
    lcd_draw_text_clipped(UI_LEFT_PAD, UI_LCD_H - UI_BOTTOM_HINT_H, hints, UI_COLOR_DIM, 1, 24);
}

static void ui_render_menu(const ui_runtime_t *ui, const ui_screen_def_t *screen)
{
    int y = UI_BODY_Y;
    size_t start = ui->nav.scroll_offset;
    for (size_t i = start; screen != NULL && i < screen->item_count && y < UI_LCD_H - UI_BOTTOM_HINT_H; ++i) {
        const bool selected = i == ui->nav.selected_index;
        char line[40];
        snprintf(line, sizeof(line), "%c %s", selected ? '>' : ' ', screen->items[i].label);
        lcd_draw_text_clipped(UI_LEFT_PAD, y, line, selected ? UI_COLOR_OK : UI_COLOR_TEXT, 1, 21);
        y += UI_LINE_H;
    }
}

static void ui_render_wifi_scan_results(const ui_runtime_t *ui, const ui_wifi_flow_state_t *wifi)
{
    (void)ui;
    int y = UI_BODY_Y;
    if (wifi == NULL || wifi->scan_results.count == 0u) {
        lcd_draw_text_clipped(UI_LEFT_PAD, y, wifi != NULL && wifi->last_error[0] ? wifi->last_error : "No networks", UI_COLOR_WARN, 1, 20);
        return;
    }
    size_t visible = wifi->scan_results.count < 7u ? wifi->scan_results.count : 7u;
    size_t first = wifi->selected_scan_index >= visible ? wifi->selected_scan_index - visible + 1u : 0u;
    for (size_t row = 0; row < visible && first + row < wifi->scan_results.count; ++row) {
        size_t i = first + row;
        char line[48];
        snprintf(line, sizeof(line), "%c%s %ddBm ch%u", i == wifi->selected_scan_index ? '>' : ' ', wifi->scan_results.items[i].ssid[0] ? wifi->scan_results.items[i].ssid : "(hidden)", wifi->scan_results.items[i].rssi, (unsigned)wifi->scan_results.items[i].channel);
        lcd_draw_text_clipped(UI_LEFT_PAD, y, line, i == wifi->selected_scan_index ? UI_COLOR_OK : UI_COLOR_TEXT, 1, 21);
        y += UI_LINE_H;
    }
}

static void ui_render_ap_url(const ui_runtime_t *ui)
{
    char line[72];
    int y = UI_BODY_Y;
    snprintf(line, sizeof(line), "SSID: %s", ui->ap.ap_name[0] ? ui->ap.ap_name : "-");
    lcd_draw_text_clipped(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1, 20); y += UI_LINE_H;
    snprintf(line, sizeof(line), "URL: %s", ui->ap.url[0] ? ui->ap.url : "-");
    lcd_draw_text_clipped(UI_LEFT_PAD, y, line, UI_COLOR_OK, 1, 20); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Channel: %u", (unsigned)ui->ap.channel);
    lcd_draw_text(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Status: %s", ui->ap.started ? "started" : "not started");
    lcd_draw_text(UI_LEFT_PAD, y, line, ui->ap.started ? UI_COLOR_OK : UI_COLOR_WARN, 1);
}

static void ui_render_bluetooth_status(const ui_runtime_t *ui)
{
    char line[56];
    int y = UI_BODY_Y;
    snprintf(line, sizeof(line), "Connected: %s", ui->bluetooth.ble_connected ? "yes" : "no"); lcd_draw_text(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Metrics: %s", ui->bluetooth.metrics_notify_enabled ? "on" : "off"); lcd_draw_text(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1); y += UI_LINE_H;
    snprintf(line, sizeof(line), "PCM: %s", ui->bluetooth.pcm_notify_enabled ? "on" : "off"); lcd_draw_text(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Name: %s", ui->bluetooth.device_name); lcd_draw_text_clipped(UI_LEFT_PAD, y, line, UI_COLOR_TEXT, 1, 20);
}

static void ui_render_automation_detail(const ui_runtime_t *ui, const ui_screen_def_t *screen)
{
    uint8_t index = screen->id == UI_SCREEN_AUTOMATION_2 ? 1u : 0u;
    const ui_automation_state_t *slot = &ui->automations[index];
    char line[48];
    int y = UI_BODY_Y;
    snprintf(line, sizeof(line), "Enabled: %s", slot->enabled ? "on" : "off"); lcd_draw_text(UI_LEFT_PAD, y, line, UI_COLOR_DIM, 1); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Trigger: %s", slot->trigger_label); lcd_draw_text_clipped(UI_LEFT_PAD, y, line, UI_COLOR_DIM, 1, 20); y += UI_LINE_H;
    snprintf(line, sizeof(line), "Action: %s", slot->action_label); lcd_draw_text_clipped(UI_LEFT_PAD, y, line, UI_COLOR_DIM, 1, 20); y += UI_LINE_H;
    for (size_t i = 0; screen != NULL && i < screen->item_count && y < UI_LCD_H - UI_BOTTOM_HINT_H; ++i) {
        bool selected = i == ui->nav.selected_index;
        snprintf(line, sizeof(line), "%c %s", selected ? '>' : ' ', screen->items[i].label);
        lcd_draw_text_clipped(UI_LEFT_PAD, y, line, selected ? UI_COLOR_OK : UI_COLOR_TEXT, 1, 21);
        y += UI_LINE_H;
    }
}

static void ui_render_settings(const ui_runtime_t *ui)
{
    ui_render_menu(ui, ui_nav_current(&ui->nav));
}

static void ui_render_toast(const ui_toast_t *toast)
{
    if (toast == NULL || toast->kind == UI_TOAST_NONE || toast->text[0] == '\0') return;
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (toast->until_tick_ms != 0u && now_ms > toast->until_tick_ms) {
        ui_runtime_clear_toast(&s_ui);
        return;
    }
    uint16_t color = toast->kind == UI_TOAST_ERROR ? UI_COLOR_ERR : (toast->kind == UI_TOAST_WARNING ? UI_COLOR_WARN : UI_COLOR_OK);
    lcd_fill_rect(0, UI_LCD_H - 30, UI_LCD_W, 16, 0x2104);
    lcd_draw_text_clipped(UI_LEFT_PAD, UI_LCD_H - 27, toast->text, color, 1, 22);
}

static void ui_render_screen(const ui_runtime_t *ui, const ui_screen_def_t *screen)
{
    ui_render_clear();
    ui_render_status_bar(&ui->status_bar);
    ui_render_title(screen != NULL ? screen->title : "Main");
    if (screen == NULL) return;
    switch (screen->id) {
    case UI_SCREEN_CONFIG_WIFI_SCAN:
        ui_render_wifi_scan_results(ui, &ui->config_wifi);
        break;
    case UI_SCREEN_CONNECT_WIFI_SCAN:
        ui_render_wifi_scan_results(ui, &ui->connect_wifi);
        break;
    case UI_SCREEN_CONFIG_AP_SHOW_URL:
        ui_render_ap_url(ui);
        break;
    case UI_SCREEN_CONNECT_BLUETOOTH:
        ui_render_bluetooth_status(ui);
        break;
    case UI_SCREEN_AUTOMATION_1:
    case UI_SCREEN_AUTOMATION_2:
        ui_render_automation_detail(ui, screen);
        break;
    case UI_SCREEN_SETTINGS:
        ui_render_settings(ui);
        break;
    default:
        ui_render_menu(ui, screen);
        break;
    }
    ui_render_bottom_hints("K1 NEXT K2 SEL HOLD BACK");
}

static void status_ui_render_menu_lcd(const ui_runtime_t *ui)
{
    status_ui_update_time(&((ui_runtime_t *)ui)->status_bar);
    status_ui_update_battery(&((ui_runtime_t *)ui)->status_bar);
    if (ui->nav.current == UI_SCREEN_CONNECT_BLUETOOTH) {
        status_ui_bluetooth_refresh((ui_runtime_t *)ui);
    }
    ui_render_screen(ui, ui_nav_current(&ui->nav));
    ui_render_toast(&ui->toast);
}


static void status_ui_render_lcd(void)
{
    status_ui_lcd_context_t ctx;

    portENTER_CRITICAL(&s_state_mux);
    ctx.state = s_state;
    ctx.monitoring_enabled = s_monitoring_enabled;
    ctx.service_enabled = s_service_enabled;
    ctx.key1_count = s_key1_press_count;
    ctx.key2_count = s_key2_press_count;
    ctx.snapshot = s_sound_snapshot;
    ctx.display_mode = s_display_mode;
    portEXIT_CRITICAL(&s_state_mux);

    uint16_t accent = STATUS_UI_LCD_HEADER_BG;
    if (ctx.snapshot.valid && (ctx.snapshot.flags & AUDIO_METRICS_FLAG_CLIPPING) != 0) {
        accent = STATUS_UI_LCD_ERR;
    }
    if (s_ui.menu_active) {
        status_ui_render_menu_lcd(&s_ui);
        if (keyboard_is_active()) {
            status_ui_render_keyboard_lcd();
        }
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        }
        return;
    }

    status_ui_render_app_shell(display_title(ctx.display_mode), &ctx, accent);

    if (!ctx.snapshot.valid && ctx.display_mode != APP_DISPLAY_DIAGNOSTICS) {
        status_ui_render_waiting_lcd();
    } else {
        ctx.snapshot.display_mode = (uint8_t)ctx.display_mode;
        switch (ctx.display_mode) {
        case APP_DISPLAY_VU:
            status_ui_render_vu_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_NUMERIC:
            status_ui_render_numeric_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_BLE_STATUS:
            status_ui_render_ble_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_DIAGNOSTICS:
        default:
            status_ui_render_diagnostics_lcd(&ctx);
            break;
        }
    }

    if (keyboard_is_active()) {
        status_ui_render_keyboard_lcd();
    }

    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
    }
}

static void status_ui_lcd_task(void *arg)
{
    (void)arg;

    while (true) {
        status_ui_render_lcd();
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_LCD_REFRESH_MS));
    }
}

static esp_err_t status_ui_lcd_init(void)
{
    ESP_LOGI(TAG, "LCD init start: ST7789P3 %dx%d host=%d pixel_clock=%d Hz",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, BOARD_LCD_HOST, BOARD_LCD_PIXEL_CLOCK_HZ);
    esp_err_t err = board_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during shared I2C setup: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: shared I2C bus ready");

    err = m5pm1_enable_lcd_power(BOARD_I2C_PORT, BOARD_M5PM1_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during M5PM1 LCD/L3B power enable: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: M5PM1 LCD/L3B power enabled");

    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << BOARD_LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&bl_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed configuring backlight GPIO%d: %s",
                 BOARD_LCD_BL_GPIO, esp_err_to_name(err));
        return err;
    }
    gpio_set_level(BOARD_LCD_BL_GPIO, !BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD init step ok: backlight GPIO%d configured off_level=%d on_level=%d",
             BOARD_LCD_BL_GPIO, !BOARD_LCD_BL_ON_LEVEL, BOARD_LCD_BL_ON_LEVEL);

    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SCLK_GPIO,
        .mosi_io_num = BOARD_LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_LOGI(TAG, "LCD init step: SPI bus host=%d MOSI=GPIO%d SCLK=GPIO%d max_transfer=%u",
             BOARD_LCD_HOST, BOARD_LCD_MOSI_GPIO, BOARD_LCD_SCLK_GPIO,
             (unsigned)buscfg.max_transfer_sz);
    err = spi_bus_initialize(BOARD_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during SPI bus init: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .cs_gpio_num = BOARD_LCD_CS_GPIO,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = BOARD_LCD_CMD_BITS,
        .lcd_param_bits = BOARD_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST, &io_config, &s_panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed creating panel IO: DC=GPIO%d CS=GPIO%d err=%s",
                 BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, esp_err_to_name(err));
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: panel IO DC=GPIO%d CS=GPIO%d queue_depth=%d",
             BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, io_config.trans_queue_depth);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(s_panel_io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed creating ST7789 panel: RST=GPIO%d err=%s",
                 BOARD_LCD_RST_GPIO, esp_err_to_name(err));
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    err = esp_lcd_panel_reset(s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed resetting panel: %s", esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(s_panel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed initialising panel: %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_invert_color(s_panel, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed setting color inversion: %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_set_gap(s_panel, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed setting panel gap %d,%d: %s",
                     BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP, esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_disp_on_off(s_panel, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed enabling panel display: %s", esp_err_to_name(err));
        }
    }
    if (err != ESP_OK) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    ESP_LOGI(TAG, "LCD init step ok: panel configured gap=%d,%d inversion=on",
             BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP);

    s_framebuffer = heap_caps_malloc(BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "LCD init failed allocating framebuffer: %u bytes DMA/internal",
                 (unsigned)(BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t)));
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t created = xTaskCreate(status_ui_lcd_task, "status_ui_lcd",
                                     STATUS_UI_LCD_TASK_STACK, NULL,
                                     STATUS_UI_LCD_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "LCD init failed starting refresh task");
        heap_caps_free(s_framebuffer);
        s_framebuffer = NULL;
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return ESP_ERR_NO_MEM;
    }

    gpio_set_level(BOARD_LCD_BL_GPIO, BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD init step ok: backlight enabled GPIO%d level=%d",
             BOARD_LCD_BL_GPIO, BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD debug UI ready: ST7789P3 %dx%d gap=%d,%d MOSI=%d SCLK=%d DC=%d CS=%d RST=%d BL=%d",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP, BOARD_LCD_MOSI_GPIO, BOARD_LCD_SCLK_GPIO,
             BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, BOARD_LCD_RST_GPIO, BOARD_LCD_BL_GPIO);
    return ESP_OK;
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
    if (s_keyboard_queue == NULL) {
        s_keyboard_queue = xQueueCreate(8, sizeof(status_ui_keyboard_event_t));
        if (s_keyboard_queue == NULL) {
            ESP_LOGE(TAG, "failed to create virtual keyboard queue");
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
    err = status_ui_lcd_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD debug UI disabled: %s", esp_err_to_name(err));
    }
#endif

    ESP_LOGI(TAG, "status UI ready; StickS3 keys KEY1=%d KEY2=%d",
             BOARD_BUTTON_KEY1_GPIO, BOARD_BUTTON_KEY2_GPIO);
    ESP_LOGI(TAG, "status: %s", status_ui_state_name(status_ui_get_state()));
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(status_ui_get_monitoring_enabled()));
    ESP_LOGI(TAG, "transport service: %s", bool_label(status_ui_get_service_enabled()));
    return ESP_OK;
}
