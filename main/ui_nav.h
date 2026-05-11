#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_HOME = 0,

    UI_SCREEN_DASHBOARD,
    UI_SCREEN_DASHBOARD_VU,
    UI_SCREEN_DASHBOARD_NUMERIC,
    UI_SCREEN_DASHBOARD_BLE,
    UI_SCREEN_DASHBOARD_DIAGNOSTICS,

    UI_SCREEN_NETWORK,
    UI_SCREEN_NETWORK_WIFI,
    UI_SCREEN_NETWORK_WIFI_SCAN,
    UI_SCREEN_NETWORK_WIFI_SELECT,
    UI_SCREEN_NETWORK_WIFI_PASSWORD,
    UI_SCREEN_NETWORK_WIFI_MANUAL_SSID,
    UI_SCREEN_NETWORK_WIFI_SAVED,

    UI_SCREEN_NETWORK_AP,
    UI_SCREEN_NETWORK_AP_NAME,
    UI_SCREEN_NETWORK_AP_PASSWORD,
    UI_SCREEN_NETWORK_AP_CHANNEL,
    UI_SCREEN_NETWORK_AP_CONFIRM,

    UI_SCREEN_NETWORK_STATUS,

    UI_SCREEN_AUTOMATION,
    UI_SCREEN_AUTOMATION_TRIGGER,
    UI_SCREEN_AUTOMATION_ACTION,
    UI_SCREEN_AUTOMATION_GPIO,
    UI_SCREEN_AUTOMATION_HAT,
    UI_SCREEN_AUTOMATION_JSON,

    UI_SCREEN_SYSTEM,
    UI_SCREEN_HELP,

    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef enum {
    UI_ITEM_ACTION_NONE = 0,
    UI_ITEM_ACTION_NAVIGATE,
    UI_ITEM_ACTION_BACK,
    UI_ITEM_ACTION_START_WIFI_SCAN,
    UI_ITEM_ACTION_CONNECT_WIFI,
    UI_ITEM_ACTION_START_AP,
    UI_ITEM_ACTION_SAVE_AP_CONFIG,
    UI_ITEM_ACTION_FORGET_WIFI,
    UI_ITEM_ACTION_OPEN_KEYBOARD
} ui_item_action_t;

typedef struct {
    const char *label;
    ui_screen_id_t target;
    ui_item_action_t action;
    uint32_t flags;
} ui_menu_item_t;

typedef struct {
    ui_screen_id_t id;
    ui_screen_id_t parent;
    const char *title;
    const ui_menu_item_t *items;
    size_t item_count;
} ui_screen_def_t;

typedef struct {
    ui_screen_id_t current;
    ui_screen_id_t previous;
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
