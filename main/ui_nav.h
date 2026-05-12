#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_MAIN = 0,

    UI_SCREEN_CONFIG_WEB_UI,
    UI_SCREEN_CONFIG_WIFI_MODE,
    UI_SCREEN_CONFIG_WIFI_SCAN,
    UI_SCREEN_CONFIG_WIFI_SELECT_SSID,
    UI_SCREEN_CONFIG_WIFI_ENTER_PASSWORD,
    UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE,
    UI_SCREEN_CONFIG_WIFI_MANUAL,
    UI_SCREEN_CONFIG_WIFI_ENTER_SSID,
    UI_SCREEN_CONFIG_WIFI_MANUAL_PASSWORD,
    UI_SCREEN_CONFIG_WIFI_MANUAL_CONNECT_SAVE,

    UI_SCREEN_CONFIG_AP_MODE,
    UI_SCREEN_CONFIG_AP_SET_NAME,
    UI_SCREEN_CONFIG_AP_SET_PASSWORD,
    UI_SCREEN_CONFIG_AP_SET_CHANNEL,
    UI_SCREEN_CONFIG_AP_START,
    UI_SCREEN_CONFIG_AP_SHOW_URL,

    UI_SCREEN_CONNECT_WIFI,
    UI_SCREEN_CONNECT_WIFI_SCAN,
    UI_SCREEN_CONNECT_WIFI_SELECT_SSID,
    UI_SCREEN_CONNECT_WIFI_ENTER_PASSWORD,
    UI_SCREEN_CONNECT_WIFI_CONNECT_SAVE,
    UI_SCREEN_CONNECT_WIFI_MANUAL,
    UI_SCREEN_CONNECT_WIFI_ENTER_SSID,
    UI_SCREEN_CONNECT_WIFI_MANUAL_PASSWORD,
    UI_SCREEN_CONNECT_WIFI_MANUAL_CONNECT_SAVE,

    UI_SCREEN_CONNECT_BLUETOOTH,

    UI_SCREEN_AUTOMATIONS,
    UI_SCREEN_AUTOMATION_1,
    UI_SCREEN_AUTOMATION_1_ENABLE,
    UI_SCREEN_AUTOMATION_1_TRIGGER,
    UI_SCREEN_AUTOMATION_1_ACTION,
    UI_SCREEN_AUTOMATION_2,
    UI_SCREEN_AUTOMATION_2_ENABLE,
    UI_SCREEN_AUTOMATION_2_TRIGGER,
    UI_SCREEN_AUTOMATION_2_ACTION,

    UI_SCREEN_SETTINGS,

    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_NAVIGATE,
    UI_ACTION_BACK,

    UI_ACTION_WIFI_SCAN,
    UI_ACTION_WIFI_SELECT_SSID,
    UI_ACTION_WIFI_ENTER_SSID,
    UI_ACTION_WIFI_ENTER_PASSWORD,
    UI_ACTION_WIFI_CONNECT_AND_SAVE,

    UI_ACTION_AP_ENTER_NAME,
    UI_ACTION_AP_ENTER_PASSWORD,
    UI_ACTION_AP_ENTER_CHANNEL,
    UI_ACTION_AP_START_MODE,
    UI_ACTION_AP_SHOW_URL,

    UI_ACTION_BLE_SHOW_STATUS,

    UI_ACTION_AUTOMATION_TOGGLE_ENABLE,
    UI_ACTION_AUTOMATION_EDIT_TRIGGER,
    UI_ACTION_AUTOMATION_EDIT_ACTION,

    UI_ACTION_SETTINGS_OPEN
} ui_action_t;

typedef enum {
    UI_FIELD_NONE = 0,
    UI_FIELD_WIFI_SSID,
    UI_FIELD_WIFI_PASSWORD,
    UI_FIELD_AP_NAME,
    UI_FIELD_AP_PASSWORD,
    UI_FIELD_AP_CHANNEL,
    UI_FIELD_AUTOMATION_ENABLE,
    UI_FIELD_AUTOMATION_TRIGGER,
    UI_FIELD_AUTOMATION_ACTION
} ui_field_id_t;

typedef enum {
    UI_FLOW_NONE = 0,
    UI_FLOW_CONFIG_WIFI,
    UI_FLOW_CONNECT_WIFI,
    UI_FLOW_CONFIG_AP,
    UI_FLOW_BLUETOOTH,
    UI_FLOW_AUTOMATION,
    UI_FLOW_SETTINGS
} ui_flow_id_t;

typedef struct {
    const char *label;
    ui_screen_id_t target;
    ui_action_t action;
    ui_field_id_t field;
    ui_flow_id_t flow;
    uint8_t automation_index;
    uint32_t flags;
} ui_menu_item_t;

typedef struct {
    ui_screen_id_t id;
    ui_screen_id_t parent;
    const char *title;
    const ui_menu_item_t *items;
    size_t item_count;
    ui_flow_id_t flow;
} ui_screen_def_t;

#define UI_NAV_STACK_DEPTH 8

typedef struct {
    ui_screen_id_t current;
    ui_screen_id_t stack[UI_NAV_STACK_DEPTH];
    uint8_t stack_depth;
    uint8_t selected_index;
    uint8_t scroll_offset;
    bool dirty;
} ui_nav_state_t;

void ui_nav_init(ui_nav_state_t *state);
const ui_screen_def_t *ui_nav_get_screen(ui_screen_id_t id);
const ui_screen_def_t *ui_nav_current(const ui_nav_state_t *state);
bool ui_nav_enter(ui_nav_state_t *state, ui_screen_id_t target);
bool ui_nav_back(ui_nav_state_t *state);
bool ui_nav_next(ui_nav_state_t *state);
bool ui_nav_prev(ui_nav_state_t *state);
const ui_menu_item_t *ui_nav_selected_item(const ui_nav_state_t *state);
bool ui_nav_activate(ui_nav_state_t *state, const ui_menu_item_t **selected);

#ifdef __cplusplus
}
#endif
