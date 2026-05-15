#include "ui_nav.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define UI_NAV_MAX_VISIBLE_ROWS 9u
#define UI_ITEM_FLAGS(label_, target_, action_, field_, flow_, automation_index_, flags_) \
    { .label = (label_), .target = (target_), .action = (action_), .field = (field_), \
      .flow = (flow_), .automation_index = (automation_index_), .flags = (flags_) }
#define UI_ITEM(label_, target_, action_, field_, flow_, automation_index_) \
    UI_ITEM_FLAGS(label_, target_, action_, field_, flow_, automation_index_, 0u)

static const ui_menu_item_t s_main_items[] = {
    UI_ITEM("Web UI", UI_SCREEN_CONFIG_WEB_UI, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_NONE, 0),
    UI_ITEM("Connect to Wi-Fi", UI_SCREEN_CONNECT_WIFI, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_NONE, 0),
    UI_ITEM("BLE (no Classic)", UI_SCREEN_CONNECT_BLUETOOTH, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_NONE, 0),
    UI_ITEM("All automations", UI_SCREEN_AUTOMATIONS, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_NONE, 0),
    UI_ITEM("Settings", UI_SCREEN_SETTINGS, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_NONE, 0),
};

static const ui_menu_item_t s_config_web_ui_items[] = {
    UI_ITEM("Wi-Fi Mode", UI_SCREEN_CONFIG_WIFI_MODE, UI_ACTION_WEB_UI_WIFI_MODE, UI_FIELD_NONE, UI_FLOW_CONFIG_WIFI, 0),
    UI_ITEM("AP Mode", UI_SCREEN_CONFIG_AP_MODE, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_CONFIG_AP, 0),
};

static const ui_menu_item_t s_config_wifi_mode_items[] = {
    UI_ITEM("Scan Nearby Wi-Fi", UI_SCREEN_CONFIG_WIFI_SCAN, UI_ACTION_WIFI_SCAN, UI_FIELD_NONE, UI_FLOW_CONFIG_WIFI, 0),
    UI_ITEM("Hidden / Manual SSID", UI_SCREEN_CONFIG_WIFI_ENTER_SSID, UI_ACTION_WIFI_ENTER_SSID, UI_FIELD_WIFI_SSID, UI_FLOW_CONFIG_WIFI, 0),
};

static const ui_menu_item_t s_config_ap_mode_items[] = {
    UI_ITEM("Set AP Name", UI_SCREEN_CONFIG_AP_SET_NAME, UI_ACTION_AP_ENTER_NAME, UI_FIELD_AP_NAME, UI_FLOW_CONFIG_AP, 0),
    UI_ITEM("Set AP Password", UI_SCREEN_CONFIG_AP_SET_PASSWORD, UI_ACTION_AP_ENTER_PASSWORD, UI_FIELD_AP_PASSWORD, UI_FLOW_CONFIG_AP, 0),
    UI_ITEM("Set Channel", UI_SCREEN_CONFIG_AP_SET_CHANNEL, UI_ACTION_AP_ENTER_CHANNEL, UI_FIELD_AP_CHANNEL, UI_FLOW_CONFIG_AP, 0),
    UI_ITEM("Start AP Mode", UI_SCREEN_CONFIG_AP_START, UI_ACTION_AP_START_MODE, UI_FIELD_NONE, UI_FLOW_CONFIG_AP, 0),
    UI_ITEM("Show AP URL", UI_SCREEN_CONFIG_AP_SHOW_URL, UI_ACTION_AP_SHOW_URL, UI_FIELD_NONE, UI_FLOW_CONFIG_AP, 0),
};

static const ui_menu_item_t s_connect_wifi_items[] = {
    UI_ITEM("Scan Nearby Wi-Fi", UI_SCREEN_CONNECT_WIFI_SCAN, UI_ACTION_WIFI_SCAN, UI_FIELD_NONE, UI_FLOW_CONNECT_WIFI, 0),
    UI_ITEM("Hidden / Manual SSID", UI_SCREEN_CONNECT_WIFI_ENTER_SSID, UI_ACTION_WIFI_ENTER_SSID, UI_FIELD_WIFI_SSID, UI_FLOW_CONNECT_WIFI, 0),
};

static const ui_menu_item_t s_connect_bluetooth_items[] = {
    UI_ITEM("Show BLE Status", UI_SCREEN_CONNECT_BLUETOOTH, UI_ACTION_BLE_SHOW_STATUS, UI_FIELD_NONE, UI_FLOW_BLUETOOTH, 0),
};

static const ui_menu_item_t s_automations_items[] = {
    UI_ITEM("Automation 1", UI_SCREEN_AUTOMATION_1, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_AUTOMATION, 0),
    UI_ITEM("Automation 2", UI_SCREEN_AUTOMATION_2, UI_ACTION_NAVIGATE, UI_FIELD_NONE, UI_FLOW_AUTOMATION, 1),
};

static const ui_menu_item_t s_automation_1_items[] = {
    UI_ITEM("Enable Flag", UI_SCREEN_AUTOMATION_1_ENABLE, UI_ACTION_AUTOMATION_TOGGLE_ENABLE, UI_FIELD_AUTOMATION_ENABLE, UI_FLOW_AUTOMATION, 0),
    UI_ITEM("Trigger", UI_SCREEN_AUTOMATION_1_TRIGGER, UI_ACTION_NAVIGATE, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 0),
    UI_ITEM("Action", UI_SCREEN_AUTOMATION_1_ACTION, UI_ACTION_NAVIGATE, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 0),
};

static const ui_menu_item_t s_automation_2_items[] = {
    UI_ITEM("Enable Flag", UI_SCREEN_AUTOMATION_2_ENABLE, UI_ACTION_AUTOMATION_TOGGLE_ENABLE, UI_FIELD_AUTOMATION_ENABLE, UI_FLOW_AUTOMATION, 1),
    UI_ITEM("Trigger", UI_SCREEN_AUTOMATION_2_TRIGGER, UI_ACTION_NAVIGATE, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 1),
    UI_ITEM("Action", UI_SCREEN_AUTOMATION_2_ACTION, UI_ACTION_NAVIGATE, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 1),
};

static const ui_menu_item_t s_automation_1_trigger_items[] = {
    UI_ITEM_FLAGS("Key1 Short", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 0, 0),
    UI_ITEM_FLAGS("Key2 Short", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 0, 1),
    UI_ITEM_FLAGS("BLE Connected", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 0, 2),
    UI_ITEM_FLAGS("Wi-Fi Connected", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 0, 3),
};

static const ui_menu_item_t s_automation_1_action_items[] = {
    UI_ITEM_FLAGS("BLE Event", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 0, 0),
    UI_ITEM_FLAGS("HTTP POST", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 0, 1),
    UI_ITEM_FLAGS("Local UI", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 0, 2),
    UI_ITEM_FLAGS("IR", UI_SCREEN_AUTOMATION_1, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 0, 3),
};

static const ui_menu_item_t s_automation_2_trigger_items[] = {
    UI_ITEM_FLAGS("Key1 Short", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 1, 0),
    UI_ITEM_FLAGS("Key2 Short", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 1, 1),
    UI_ITEM_FLAGS("BLE Connected", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 1, 2),
    UI_ITEM_FLAGS("Wi-Fi Connected", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_TRIGGER, UI_FIELD_AUTOMATION_TRIGGER, UI_FLOW_AUTOMATION, 1, 3),
};

static const ui_menu_item_t s_automation_2_action_items[] = {
    UI_ITEM_FLAGS("BLE Event", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 1, 0),
    UI_ITEM_FLAGS("HTTP POST", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 1, 1),
    UI_ITEM_FLAGS("Local UI", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 1, 2),
    UI_ITEM_FLAGS("IR", UI_SCREEN_AUTOMATION_2, UI_ACTION_AUTOMATION_EDIT_ACTION, UI_FIELD_AUTOMATION_ACTION, UI_FLOW_AUTOMATION, 1, 3),
};

static const ui_menu_item_t s_settings_items[] = {
    UI_ITEM("Display", UI_SCREEN_SETTINGS, UI_ACTION_SETTINGS_OPEN, UI_FIELD_NONE, UI_FLOW_SETTINGS, 0),
    UI_ITEM("About", UI_SCREEN_SETTINGS, UI_ACTION_SETTINGS_OPEN, UI_FIELD_NONE, UI_FLOW_SETTINGS, 0),
};

static const ui_screen_def_t s_screens[UI_SCREEN_COUNT] = {
    [UI_SCREEN_MAIN] = { UI_SCREEN_MAIN, UI_SCREEN_MAIN, "Main", s_main_items, ARRAY_COUNT(s_main_items), UI_FLOW_NONE },
    [UI_SCREEN_CONFIG_WEB_UI] = { UI_SCREEN_CONFIG_WEB_UI, UI_SCREEN_MAIN, "Web UI", s_config_web_ui_items, ARRAY_COUNT(s_config_web_ui_items), UI_FLOW_NONE },
    [UI_SCREEN_CONFIG_WIFI_MODE] = { UI_SCREEN_CONFIG_WIFI_MODE, UI_SCREEN_CONFIG_WEB_UI, "Wi-Fi Mode", s_config_wifi_mode_items, ARRAY_COUNT(s_config_wifi_mode_items), UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_SCAN] = { UI_SCREEN_CONFIG_WIFI_SCAN, UI_SCREEN_CONFIG_WIFI_MODE, "Scan Nearby Wi-Fi", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_SELECT_SSID] = { UI_SCREEN_CONFIG_WIFI_SELECT_SSID, UI_SCREEN_CONFIG_WIFI_SCAN, "Select SSID", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_ENTER_PASSWORD] = { UI_SCREEN_CONFIG_WIFI_ENTER_PASSWORD, UI_SCREEN_CONFIG_WIFI_SELECT_SSID, "Enter Password", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE] = { UI_SCREEN_CONFIG_WIFI_CONNECT_SAVE, UI_SCREEN_CONFIG_WIFI_SELECT_SSID, "Wi-Fi Saved", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_MANUAL] = { UI_SCREEN_CONFIG_WIFI_MANUAL, UI_SCREEN_CONFIG_WIFI_MODE, "Hidden / Manual SSID", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_ENTER_SSID] = { UI_SCREEN_CONFIG_WIFI_ENTER_SSID, UI_SCREEN_CONFIG_WIFI_MODE, "Enter SSID", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_MANUAL_PASSWORD] = { UI_SCREEN_CONFIG_WIFI_MANUAL_PASSWORD, UI_SCREEN_CONFIG_WIFI_ENTER_SSID, "Enter Password", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_WIFI_MANUAL_CONNECT_SAVE] = { UI_SCREEN_CONFIG_WIFI_MANUAL_CONNECT_SAVE, UI_SCREEN_CONFIG_WIFI_ENTER_SSID, "Wi-Fi Saved", NULL, 0u, UI_FLOW_CONFIG_WIFI },
    [UI_SCREEN_CONFIG_AP_MODE] = { UI_SCREEN_CONFIG_AP_MODE, UI_SCREEN_CONFIG_WEB_UI, "AP Mode", s_config_ap_mode_items, ARRAY_COUNT(s_config_ap_mode_items), UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONFIG_AP_SET_NAME] = { UI_SCREEN_CONFIG_AP_SET_NAME, UI_SCREEN_CONFIG_AP_MODE, "Set AP Name", s_config_ap_mode_items, ARRAY_COUNT(s_config_ap_mode_items), UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONFIG_AP_SET_PASSWORD] = { UI_SCREEN_CONFIG_AP_SET_PASSWORD, UI_SCREEN_CONFIG_AP_MODE, "Set AP Password", s_config_ap_mode_items, ARRAY_COUNT(s_config_ap_mode_items), UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONFIG_AP_SET_CHANNEL] = { UI_SCREEN_CONFIG_AP_SET_CHANNEL, UI_SCREEN_CONFIG_AP_MODE, "Set Channel", s_config_ap_mode_items, ARRAY_COUNT(s_config_ap_mode_items), UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONFIG_AP_START] = { UI_SCREEN_CONFIG_AP_START, UI_SCREEN_CONFIG_AP_MODE, "Start AP Mode", s_config_ap_mode_items, ARRAY_COUNT(s_config_ap_mode_items), UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONFIG_AP_SHOW_URL] = { UI_SCREEN_CONFIG_AP_SHOW_URL, UI_SCREEN_CONFIG_AP_MODE, "Show AP URL", NULL, 0u, UI_FLOW_CONFIG_AP },
    [UI_SCREEN_CONNECT_WIFI] = { UI_SCREEN_CONNECT_WIFI, UI_SCREEN_MAIN, "Connect to Wi-Fi", s_connect_wifi_items, ARRAY_COUNT(s_connect_wifi_items), UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_SCAN] = { UI_SCREEN_CONNECT_WIFI_SCAN, UI_SCREEN_CONNECT_WIFI, "Scan Nearby Wi-Fi", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_SELECT_SSID] = { UI_SCREEN_CONNECT_WIFI_SELECT_SSID, UI_SCREEN_CONNECT_WIFI_SCAN, "Select SSID", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_ENTER_PASSWORD] = { UI_SCREEN_CONNECT_WIFI_ENTER_PASSWORD, UI_SCREEN_CONNECT_WIFI_SELECT_SSID, "Enter Password", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_CONNECT_SAVE] = { UI_SCREEN_CONNECT_WIFI_CONNECT_SAVE, UI_SCREEN_CONNECT_WIFI_SELECT_SSID, "Wi-Fi Saved", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_MANUAL] = { UI_SCREEN_CONNECT_WIFI_MANUAL, UI_SCREEN_CONNECT_WIFI, "Hidden / Manual SSID", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_ENTER_SSID] = { UI_SCREEN_CONNECT_WIFI_ENTER_SSID, UI_SCREEN_CONNECT_WIFI, "Enter SSID", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_MANUAL_PASSWORD] = { UI_SCREEN_CONNECT_WIFI_MANUAL_PASSWORD, UI_SCREEN_CONNECT_WIFI_ENTER_SSID, "Enter Password", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_WIFI_MANUAL_CONNECT_SAVE] = { UI_SCREEN_CONNECT_WIFI_MANUAL_CONNECT_SAVE, UI_SCREEN_CONNECT_WIFI_ENTER_SSID, "Wi-Fi Saved", NULL, 0u, UI_FLOW_CONNECT_WIFI },
    [UI_SCREEN_CONNECT_BLUETOOTH] = { UI_SCREEN_CONNECT_BLUETOOTH, UI_SCREEN_MAIN, "BLE (no Classic)", s_connect_bluetooth_items, ARRAY_COUNT(s_connect_bluetooth_items), UI_FLOW_BLUETOOTH },
    [UI_SCREEN_AUTOMATIONS] = { UI_SCREEN_AUTOMATIONS, UI_SCREEN_MAIN, "All automations", s_automations_items, ARRAY_COUNT(s_automations_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_1] = { UI_SCREEN_AUTOMATION_1, UI_SCREEN_AUTOMATIONS, "Automation 1", s_automation_1_items, ARRAY_COUNT(s_automation_1_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_1_ENABLE] = { UI_SCREEN_AUTOMATION_1_ENABLE, UI_SCREEN_AUTOMATION_1, "Enable Flag", s_automation_1_items, ARRAY_COUNT(s_automation_1_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_1_TRIGGER] = { UI_SCREEN_AUTOMATION_1_TRIGGER, UI_SCREEN_AUTOMATION_1, "Trigger", s_automation_1_trigger_items, ARRAY_COUNT(s_automation_1_trigger_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_1_ACTION] = { UI_SCREEN_AUTOMATION_1_ACTION, UI_SCREEN_AUTOMATION_1, "Action", s_automation_1_action_items, ARRAY_COUNT(s_automation_1_action_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_2] = { UI_SCREEN_AUTOMATION_2, UI_SCREEN_AUTOMATIONS, "Automation 2", s_automation_2_items, ARRAY_COUNT(s_automation_2_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_2_ENABLE] = { UI_SCREEN_AUTOMATION_2_ENABLE, UI_SCREEN_AUTOMATION_2, "Enable Flag", s_automation_2_items, ARRAY_COUNT(s_automation_2_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_2_TRIGGER] = { UI_SCREEN_AUTOMATION_2_TRIGGER, UI_SCREEN_AUTOMATION_2, "Trigger", s_automation_2_trigger_items, ARRAY_COUNT(s_automation_2_trigger_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_AUTOMATION_2_ACTION] = { UI_SCREEN_AUTOMATION_2_ACTION, UI_SCREEN_AUTOMATION_2, "Action", s_automation_2_action_items, ARRAY_COUNT(s_automation_2_action_items), UI_FLOW_AUTOMATION },
    [UI_SCREEN_SETTINGS] = { UI_SCREEN_SETTINGS, UI_SCREEN_MAIN, "Settings", s_settings_items, ARRAY_COUNT(s_settings_items), UI_FLOW_SETTINGS },
};

static void reset_cursor(ui_nav_state_t *state)
{
    state->selected_index = 0u;
    state->scroll_offset = 0u;
    state->dirty = true;
}

static void update_scroll(ui_nav_state_t *state, const ui_screen_def_t *screen)
{
    if (state == NULL || screen == NULL || screen->item_count <= UI_NAV_MAX_VISIBLE_ROWS) {
        if (state != NULL) {
            state->scroll_offset = 0u;
        }
        return;
    }
    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    } else if (state->selected_index >= (uint8_t)(state->scroll_offset + UI_NAV_MAX_VISIBLE_ROWS)) {
        state->scroll_offset = (uint8_t)(state->selected_index - UI_NAV_MAX_VISIBLE_ROWS + 1u);
    }
}

void ui_nav_init(ui_nav_state_t *state)
{
    if (state == NULL) return;
    state->current = UI_SCREEN_MAIN;
    state->stack_depth = 0u;
    reset_cursor(state);
}

const ui_screen_def_t *ui_nav_get_screen(ui_screen_id_t id)
{
    if ((unsigned)id >= UI_SCREEN_COUNT || s_screens[id].title == NULL) return NULL;
    return &s_screens[id];
}

const ui_screen_def_t *ui_nav_current(const ui_nav_state_t *state)
{
    return state != NULL ? ui_nav_get_screen(state->current) : NULL;
}

bool ui_nav_enter(ui_nav_state_t *state, ui_screen_id_t target)
{
    if (state == NULL || ui_nav_get_screen(target) == NULL) return false;
    if (state->current != target && state->stack_depth < UI_NAV_STACK_DEPTH) {
        state->stack[state->stack_depth++] = state->current;
    }
    state->current = target;
    reset_cursor(state);
    return true;
}

bool ui_nav_back(ui_nav_state_t *state)
{
    if (state == NULL) return false;
    if (state->stack_depth > 0u) {
        state->current = state->stack[--state->stack_depth];
    } else {
        const ui_screen_def_t *screen = ui_nav_current(state);
        state->current = (screen != NULL) ? screen->parent : UI_SCREEN_MAIN;
    }
    reset_cursor(state);
    return true;
}

bool ui_nav_next(ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL || screen->item_count == 0u) return false;
    state->selected_index = (uint8_t)((state->selected_index + 1u) % screen->item_count);
    update_scroll(state, screen);
    state->dirty = true;
    return true;
}

bool ui_nav_prev(ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL || screen->item_count == 0u) return false;
    state->selected_index = state->selected_index == 0u ? (uint8_t)(screen->item_count - 1u) : (uint8_t)(state->selected_index - 1u);
    update_scroll(state, screen);
    state->dirty = true;
    return true;
}

const ui_menu_item_t *ui_nav_selected_item(const ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL || screen->items == NULL || state->selected_index >= screen->item_count) return NULL;
    return &screen->items[state->selected_index];
}

bool ui_nav_activate(ui_nav_state_t *state, const ui_menu_item_t **selected)
{
    const ui_menu_item_t *item = ui_nav_selected_item(state);
    if (selected != NULL) *selected = item;
    if (state == NULL || item == NULL) return false;
    if (item->action == UI_ACTION_NAVIGATE) return ui_nav_enter(state, item->target);
    if (item->action == UI_ACTION_BACK) return ui_nav_back(state);
    state->dirty = true;
    return true;
}
