#define _POSIX_C_SOURCE 200809L
#include "ui_model.h"

#include "rule_config_store.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef CONFIG_APP_BLE_GATT_DEVICE_NAME
#define CONFIG_APP_BLE_GATT_DEVICE_NAME "M5StickS3-Control"
#endif

static void ui_copy_text(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0u) return;
    const char *src = text != NULL ? text : "";
    size_t len = strnlen(src, dest_size - 1u);
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static const char *ui_rule_source_label(rule_source_t source)
{
    switch (source) {
    case RULE_SOURCE_KEY1_SHORT: return "Key1 Short";
    case RULE_SOURCE_KEY2_SHORT: return "Key2 Short";
    case RULE_SOURCE_BLE_CONNECTED: return "BLE Connected";
    case RULE_SOURCE_WIFI_CONNECTED: return "Wi-Fi Connected";
    default: return rule_source_name(source);
    }
}

static const char *ui_rule_action_label(rule_action_kind_t action_kind)
{
    switch (action_kind) {
    case RULE_ACTION_BLE_MESSAGE: return "BLE Event";
    case RULE_ACTION_HTTP_POST: return "HTTP POST";
    case RULE_ACTION_LOCAL_UI: return "Local UI";
    case RULE_ACTION_IR_SEND: return "IR";
    default: return rule_action_name(action_kind);
    }
}

static void automation_label(ui_automation_state_t *slot)
{
    ui_copy_text(slot->trigger_label, sizeof(slot->trigger_label), ui_rule_source_label(slot->trigger_source));
    ui_copy_text(slot->action_label, sizeof(slot->action_label), ui_rule_action_label(slot->action_kind));
}

static void apply_trigger_preset(rule_condition_t *when, rule_source_t source)
{
    if (when == NULL) return;
    memset(when, 0, sizeof(*when));
    when->source = source;
    when->sustain_ms = 0u;
    switch (source) {
    case RULE_SOURCE_KEY1_SHORT:
    case RULE_SOURCE_KEY2_SHORT:
    case RULE_SOURCE_BLE_CONNECTED:
    case RULE_SOURCE_WIFI_CONNECTED:
    default:
        when->comparator = RULE_COMPARATOR_EQ;
        when->threshold = rule_value_bool(true);
        break;
    }
}

static void apply_action_preset(rule_action_t *action, rule_action_kind_t action_kind)
{
    if (action == NULL) return;
    memset(action, 0, sizeof(*action));
    action->type = action_kind;
    switch (action_kind) {
    case RULE_ACTION_HTTP_POST:
        snprintf(action->http_url, sizeof(action->http_url), "http://192.168.4.1/rule-event");
        action->timeout_ms = 3000u;
        break;
    case RULE_ACTION_IR_SEND:
        action->ir_protocol = RULE_IR_PROTOCOL_NEC;
        action->ir_carrier_hz = 38000u;
        action->ir_repeat_count = 0u;
        action->timeout_ms = 250u;
        break;
    case RULE_ACTION_BLE_MESSAGE:
    case RULE_ACTION_LOCAL_UI:
    default:
        action->timeout_ms = 1000u;
        break;
    }
}

void ui_runtime_init(ui_runtime_t *ui)
{
    if (ui == NULL) return;
    memset(ui, 0, sizeof(*ui));
    ui_nav_init(&ui->nav);
    ui->menu_active = true;
    ui->ap.channel = 6u;
    for (uint8_t i = 0; i < UI_AUTOMATION_VISIBLE_COUNT; ++i) {
        ui->automations[i].rule_index = i;
        ui->automations[i].trigger_source = (i == 0u) ? RULE_SOURCE_KEY1_SHORT : RULE_SOURCE_KEY2_SHORT;
        ui->automations[i].action_kind = RULE_ACTION_LOCAL_UI;
        automation_label(&ui->automations[i]);
    }
    ui_runtime_refresh_status_bar(ui);
    ui_runtime_refresh_bluetooth(ui);
}

ui_wifi_flow_state_t *ui_runtime_wifi_flow(ui_runtime_t *ui, ui_flow_id_t flow)
{
    if (ui == NULL) return NULL;
    if (flow == UI_FLOW_CONFIG_WIFI) return &ui->config_wifi;
    if (flow == UI_FLOW_CONNECT_WIFI) return &ui->connect_wifi;
    return NULL;
}

const ui_wifi_flow_state_t *ui_runtime_wifi_flow_const(const ui_runtime_t *ui, ui_flow_id_t flow)
{
    if (ui == NULL) return NULL;
    if (flow == UI_FLOW_CONFIG_WIFI) return &ui->config_wifi;
    if (flow == UI_FLOW_CONNECT_WIFI) return &ui->connect_wifi;
    return NULL;
}

void ui_runtime_set_toast(ui_runtime_t *ui, ui_toast_kind_t kind, const char *text, uint32_t ttl_ms)
{
    if (ui == NULL) return;
    ui->toast.kind = kind;
    ui_copy_text(ui->toast.text, sizeof(ui->toast.text), text);
    ui->toast.until_tick_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) + ttl_ms;
    ui->dirty = true;
}

void ui_runtime_clear_toast(ui_runtime_t *ui)
{
    if (ui == NULL) return;
    memset(&ui->toast, 0, sizeof(ui->toast));
    ui->dirty = true;
}

void ui_runtime_refresh_status_bar(ui_runtime_t *ui)
{
    if (ui == NULL) return;
    time_t now = time(NULL);
    struct tm local;
    if (now > 0 && localtime_r(&now, &local) != NULL && local.tm_year >= 120) {
        ui->status_bar.time_valid = true;
        snprintf(ui->status_bar.time_hhmm, sizeof(ui->status_bar.time_hhmm), "%02d:%02d", local.tm_hour, local.tm_min);
    } else {
        ui->status_bar.time_valid = false;
        snprintf(ui->status_bar.time_hhmm, sizeof(ui->status_bar.time_hhmm), "--:--");
    }
    ui->status_bar.battery_valid = false;
    ui->status_bar.battery_percent = 0u;
}

void ui_runtime_refresh_bluetooth(ui_runtime_t *ui)
{
    if (ui == NULL) return;
    ui_copy_text(ui->bluetooth.device_name, sizeof(ui->bluetooth.device_name), CONFIG_APP_BLE_GATT_DEVICE_NAME);
    ui_copy_text(ui->bluetooth.status_text, sizeof(ui->bluetooth.status_text), ui->bluetooth.ble_connected ? "connected" : "disconnected");
}

bool ui_runtime_load_ap_config(ui_runtime_t *ui)
{
    if (ui == NULL) return false;
    app_wifi_config_t config;
    if (!app_wifi_get_config(&config)) {
        snprintf(ui->ap.last_error, sizeof(ui->ap.last_error), "AP config load failed");
        return false;
    }
    ui_copy_text(ui->ap.ap_name, sizeof(ui->ap.ap_name), config.ap_ssid[0] ? config.ap_ssid : "StickS3-Setup");
    ui_copy_text(ui->ap.ap_password, sizeof(ui->ap.ap_password), config.ap_password);
    ui->ap.channel = config.ap_channel >= UI_AP_CHANNEL_MIN && config.ap_channel <= UI_AP_CHANNEL_MAX ? config.ap_channel : 6u;
    ui->ap.loaded_from_config = true;
    return true;
}

bool ui_runtime_load_automation(ui_runtime_t *ui, uint8_t automation_index)
{
    if (ui == NULL || automation_index >= UI_AUTOMATION_VISIBLE_COUNT) return false;
    automation_config_t config;
    automation_config_set_defaults(&config);
    rule_config_store_t store = {0};
    if (rule_config_store_open(&store)) {
        (void)rule_config_store_load(&store, &config);
        rule_config_store_close(&store);
    }
    ui_automation_state_t *slot = &ui->automations[automation_index];
    slot->rule_index = automation_index;
    if (automation_index < RULE_MAX_RULES) {
        automation_rule_t *rule = &config.rules[automation_index];
        slot->enabled = rule->enabled;
        slot->trigger_source = rule->when.source;
        slot->action_kind = rule->action_count > 0u ? rule->actions[0].type : RULE_ACTION_LOCAL_UI;
        slot->loaded = true;
        automation_label(slot);
        return true;
    }
    return false;
}

bool ui_runtime_save_automation(ui_runtime_t *ui, uint8_t automation_index)
{
    if (ui == NULL || automation_index >= UI_AUTOMATION_VISIBLE_COUNT) return false;
    automation_config_t config;
    automation_config_set_defaults(&config);
    rule_config_store_t store = {0};
    if (!rule_config_store_open(&store)) return false;
    (void)rule_config_store_load(&store, &config);
    ui_automation_state_t *slot = &ui->automations[automation_index];
    automation_rule_t *rule = &config.rules[automation_index];
    if (config.rule_count <= automation_index) config.rule_count = automation_index + 1u;
    rule->enabled = slot->enabled;
    if (rule->id == 0u) rule->id = (uint32_t)automation_index + 1u;
    if (rule->name[0] == '\0') {
        snprintf(rule->name, sizeof(rule->name), "Automation %u", (unsigned)automation_index + 1u);
    }
    apply_trigger_preset(&rule->when, slot->trigger_source);
    rule->action_count = 1u;
    apply_action_preset(&rule->actions[0], slot->action_kind);
    if (rule->cooldown_ms < RULE_MIN_COOLDOWN_MS || rule->cooldown_ms > RULE_MAX_COOLDOWN_MS) {
        rule->cooldown_ms = 1000u;
    }
    char error[RULE_ERROR_MAX] = {0};
    bool ok = automation_config_validate(&config, error, sizeof(error)) && rule_config_store_save(&store, &config);
    rule_config_store_close(&store);
    automation_label(slot);
    slot->loaded = ok;
    if (!ok) {
        ui_copy_text(slot->last_error, sizeof(slot->last_error), error[0] ? error : "Automation save failed");
    } else {
        slot->last_error[0] = '\0';
    }
    return ok;
}
