#include "ui_nav.h"

#define ITEM(label_, target_, action_) { .label = (label_), .target = (target_), .action = (action_), .flags = 0u }
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const ui_menu_item_t s_home_items[] = {
    ITEM("Dashboard", UI_SCREEN_DASHBOARD, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Network", UI_SCREEN_NETWORK, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Automation", UI_SCREEN_AUTOMATION, UI_ITEM_ACTION_NAVIGATE),
    ITEM("System", UI_SCREEN_SYSTEM, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Help", UI_SCREEN_HELP, UI_ITEM_ACTION_NAVIGATE),
};

static const ui_menu_item_t s_dashboard_items[] = {
    ITEM("VU Meter", UI_SCREEN_DASHBOARD_VU, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Numeric Meter", UI_SCREEN_DASHBOARD_NUMERIC, UI_ITEM_ACTION_NAVIGATE),
    ITEM("BLE Status", UI_SCREEN_DASHBOARD_BLE, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Diagnostics", UI_SCREEN_DASHBOARD_DIAGNOSTICS, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_HOME, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_network_items[] = {
    ITEM("Wi-Fi Mode", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_NAVIGATE),
    ITEM("AP Mode", UI_SCREEN_NETWORK_AP, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Network Status", UI_SCREEN_NETWORK_STATUS, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_HOME, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_items[] = {
    ITEM("Scan Nearby Wi-Fi", UI_SCREEN_NETWORK_WIFI_SCAN, UI_ITEM_ACTION_START_WIFI_SCAN),
    ITEM("Hidden / Manual SSID", UI_SCREEN_NETWORK_WIFI_MANUAL_SSID, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Saved Wi-Fi", UI_SCREEN_NETWORK_WIFI_SAVED, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_NETWORK, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_scan_items[] = {
    ITEM("Select Network", UI_SCREEN_NETWORK_WIFI_SELECT, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Rescan", UI_SCREEN_NETWORK_WIFI_SCAN, UI_ITEM_ACTION_START_WIFI_SCAN),
    ITEM("Back", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_select_items[] = {
    ITEM("Enter Password", UI_SCREEN_NETWORK_WIFI_PASSWORD, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Connect + Save", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_CONNECT_WIFI),
    ITEM("Back", UI_SCREEN_NETWORK_WIFI_SCAN, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_password_items[] = {
    ITEM("Connect + Save", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_CONNECT_WIFI),
    ITEM("Back", UI_SCREEN_NETWORK_WIFI_SELECT, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_manual_items[] = {
    ITEM("Enter SSID", UI_SCREEN_NETWORK_WIFI_MANUAL_SSID, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Enter Password", UI_SCREEN_NETWORK_WIFI_PASSWORD, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Connect + Save", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_CONNECT_WIFI),
    ITEM("Back", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_wifi_saved_items[] = {
    ITEM("Show Saved SSID", UI_SCREEN_NETWORK_WIFI_SAVED, UI_ITEM_ACTION_NONE),
    ITEM("Reconnect", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_CONNECT_WIFI),
    ITEM("Forget Saved Credentials", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_FORGET_WIFI),
    ITEM("Back", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_ap_items[] = {
    ITEM("Set AP Name", UI_SCREEN_NETWORK_AP_NAME, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Set AP Password", UI_SCREEN_NETWORK_AP_PASSWORD, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Set Channel", UI_SCREEN_NETWORK_AP_CHANNEL, UI_ITEM_ACTION_OPEN_KEYBOARD),
    ITEM("Start AP", UI_SCREEN_NETWORK_AP_CONFIRM, UI_ITEM_ACTION_START_AP),
    ITEM("Show AP URL", UI_SCREEN_NETWORK_AP_CONFIRM, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_NETWORK, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_ap_edit_items[] = {
    ITEM("Save AP Config", UI_SCREEN_NETWORK_AP, UI_ITEM_ACTION_SAVE_AP_CONFIG),
    ITEM("Start AP", UI_SCREEN_NETWORK_AP_CONFIRM, UI_ITEM_ACTION_START_AP),
    ITEM("Back", UI_SCREEN_NETWORK_AP, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_status_items[] = {
    ITEM("Back", UI_SCREEN_NETWORK, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_automation_items[] = {
    ITEM("WHEN Trigger", UI_SCREEN_AUTOMATION_TRIGGER, UI_ITEM_ACTION_NAVIGATE),
    ITEM("DO Action", UI_SCREEN_AUTOMATION_ACTION, UI_ITEM_ACTION_NAVIGATE),
    ITEM("GPIO Safety", UI_SCREEN_AUTOMATION_GPIO, UI_ITEM_ACTION_NAVIGATE),
    ITEM("HAT Probe", UI_SCREEN_AUTOMATION_HAT, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Import / Export JSON", UI_SCREEN_AUTOMATION_JSON, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_HOME, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_system_items[] = {
    ITEM("Brightness", UI_SCREEN_SYSTEM, UI_ITEM_ACTION_NONE),
    ITEM("Display Mode", UI_SCREEN_DASHBOARD, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Calibration", UI_SCREEN_DASHBOARD_DIAGNOSTICS, UI_ITEM_ACTION_NAVIGATE),
    ITEM("About", UI_SCREEN_HELP, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_HOME, UI_ITEM_ACTION_BACK),
};

static const ui_menu_item_t s_help_items[] = {
    ITEM("Button Guide", UI_SCREEN_HELP, UI_ITEM_ACTION_NONE),
    ITEM("Wi-Fi Help", UI_SCREEN_NETWORK_WIFI, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Web UI Help", UI_SCREEN_NETWORK_STATUS, UI_ITEM_ACTION_NAVIGATE),
    ITEM("Back", UI_SCREEN_HOME, UI_ITEM_ACTION_BACK),
};

static const ui_screen_def_t s_screens[UI_SCREEN_COUNT] = {
    [UI_SCREEN_HOME] = { UI_SCREEN_HOME, UI_SCREEN_HOME, "Home", s_home_items, ARRAY_COUNT(s_home_items) },
    [UI_SCREEN_DASHBOARD] = { UI_SCREEN_DASHBOARD, UI_SCREEN_HOME, "Dashboard", s_dashboard_items, ARRAY_COUNT(s_dashboard_items) },
    [UI_SCREEN_DASHBOARD_VU] = { UI_SCREEN_DASHBOARD_VU, UI_SCREEN_DASHBOARD, "VU Meter", NULL, 0u },
    [UI_SCREEN_DASHBOARD_NUMERIC] = { UI_SCREEN_DASHBOARD_NUMERIC, UI_SCREEN_DASHBOARD, "Numeric Meter", NULL, 0u },
    [UI_SCREEN_DASHBOARD_BLE] = { UI_SCREEN_DASHBOARD_BLE, UI_SCREEN_DASHBOARD, "BLE Status", NULL, 0u },
    [UI_SCREEN_DASHBOARD_DIAGNOSTICS] = { UI_SCREEN_DASHBOARD_DIAGNOSTICS, UI_SCREEN_DASHBOARD, "Diagnostics", NULL, 0u },
    [UI_SCREEN_NETWORK] = { UI_SCREEN_NETWORK, UI_SCREEN_HOME, "Network", s_network_items, ARRAY_COUNT(s_network_items) },
    [UI_SCREEN_NETWORK_WIFI] = { UI_SCREEN_NETWORK_WIFI, UI_SCREEN_NETWORK, "Wi-Fi Mode", s_wifi_items, ARRAY_COUNT(s_wifi_items) },
    [UI_SCREEN_NETWORK_WIFI_SCAN] = { UI_SCREEN_NETWORK_WIFI_SCAN, UI_SCREEN_NETWORK_WIFI, "Scan Nearby Wi-Fi", s_wifi_scan_items, ARRAY_COUNT(s_wifi_scan_items) },
    [UI_SCREEN_NETWORK_WIFI_SELECT] = { UI_SCREEN_NETWORK_WIFI_SELECT, UI_SCREEN_NETWORK_WIFI_SCAN, "Select Network", s_wifi_select_items, ARRAY_COUNT(s_wifi_select_items) },
    [UI_SCREEN_NETWORK_WIFI_PASSWORD] = { UI_SCREEN_NETWORK_WIFI_PASSWORD, UI_SCREEN_NETWORK_WIFI_SELECT, "Enter Password", s_wifi_password_items, ARRAY_COUNT(s_wifi_password_items) },
    [UI_SCREEN_NETWORK_WIFI_MANUAL_SSID] = { UI_SCREEN_NETWORK_WIFI_MANUAL_SSID, UI_SCREEN_NETWORK_WIFI, "Hidden / Manual SSID", s_wifi_manual_items, ARRAY_COUNT(s_wifi_manual_items) },
    [UI_SCREEN_NETWORK_WIFI_SAVED] = { UI_SCREEN_NETWORK_WIFI_SAVED, UI_SCREEN_NETWORK_WIFI, "Saved Wi-Fi", s_wifi_saved_items, ARRAY_COUNT(s_wifi_saved_items) },
    [UI_SCREEN_NETWORK_AP] = { UI_SCREEN_NETWORK_AP, UI_SCREEN_NETWORK, "AP Mode", s_ap_items, ARRAY_COUNT(s_ap_items) },
    [UI_SCREEN_NETWORK_AP_NAME] = { UI_SCREEN_NETWORK_AP_NAME, UI_SCREEN_NETWORK_AP, "Set AP Name", s_ap_edit_items, ARRAY_COUNT(s_ap_edit_items) },
    [UI_SCREEN_NETWORK_AP_PASSWORD] = { UI_SCREEN_NETWORK_AP_PASSWORD, UI_SCREEN_NETWORK_AP, "Set AP Password", s_ap_edit_items, ARRAY_COUNT(s_ap_edit_items) },
    [UI_SCREEN_NETWORK_AP_CHANNEL] = { UI_SCREEN_NETWORK_AP_CHANNEL, UI_SCREEN_NETWORK_AP, "Set Channel", s_ap_edit_items, ARRAY_COUNT(s_ap_edit_items) },
    [UI_SCREEN_NETWORK_AP_CONFIRM] = { UI_SCREEN_NETWORK_AP_CONFIRM, UI_SCREEN_NETWORK_AP, "AP Ready", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_NETWORK_STATUS] = { UI_SCREEN_NETWORK_STATUS, UI_SCREEN_NETWORK, "Network Status", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_AUTOMATION] = { UI_SCREEN_AUTOMATION, UI_SCREEN_HOME, "Automation", s_automation_items, ARRAY_COUNT(s_automation_items) },
    [UI_SCREEN_AUTOMATION_TRIGGER] = { UI_SCREEN_AUTOMATION_TRIGGER, UI_SCREEN_AUTOMATION, "WHEN Trigger", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_AUTOMATION_ACTION] = { UI_SCREEN_AUTOMATION_ACTION, UI_SCREEN_AUTOMATION, "DO Action", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_AUTOMATION_GPIO] = { UI_SCREEN_AUTOMATION_GPIO, UI_SCREEN_AUTOMATION, "GPIO Safety", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_AUTOMATION_HAT] = { UI_SCREEN_AUTOMATION_HAT, UI_SCREEN_AUTOMATION, "HAT Probe", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_AUTOMATION_JSON] = { UI_SCREEN_AUTOMATION_JSON, UI_SCREEN_AUTOMATION, "Import / Export JSON", s_status_items, ARRAY_COUNT(s_status_items) },
    [UI_SCREEN_SYSTEM] = { UI_SCREEN_SYSTEM, UI_SCREEN_HOME, "System", s_system_items, ARRAY_COUNT(s_system_items) },
    [UI_SCREEN_HELP] = { UI_SCREEN_HELP, UI_SCREEN_HOME, "Help", s_help_items, ARRAY_COUNT(s_help_items) },
};

void ui_nav_init(ui_nav_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->current = UI_SCREEN_HOME;
    state->previous = UI_SCREEN_HOME;
    state->selected_index = 0u;
    state->scroll_offset = 0u;
    state->dirty = true;
}

const ui_screen_def_t *ui_nav_get_screen(ui_screen_id_t id)
{
    if ((unsigned)id >= UI_SCREEN_COUNT || s_screens[id].title == NULL) {
        return NULL;
    }
    return &s_screens[id];
}

const ui_screen_def_t *ui_nav_current(const ui_nav_state_t *state)
{
    return state != NULL ? ui_nav_get_screen(state->current) : NULL;
}

bool ui_nav_enter(ui_nav_state_t *state, ui_screen_id_t target)
{
    const ui_screen_def_t *screen = ui_nav_get_screen(target);
    if (state == NULL || screen == NULL) {
        return false;
    }
    state->previous = state->current;
    state->current = target;
    state->selected_index = 0u;
    state->scroll_offset = 0u;
    state->dirty = true;
    return true;
}

bool ui_nav_back(ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL) {
        return false;
    }
    if (screen->id == UI_SCREEN_HOME) {
        state->dirty = true;
        return true;
    }
    return ui_nav_enter(state, screen->parent);
}

bool ui_nav_next(ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL || screen->item_count == 0u) {
        return false;
    }
    state->selected_index = (uint8_t)((state->selected_index + 1u) % screen->item_count);
    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    state->dirty = true;
    return true;
}

bool ui_nav_prev(ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (state == NULL || screen == NULL || screen->item_count == 0u) {
        return false;
    }
    state->selected_index = state->selected_index == 0u ? (uint8_t)(screen->item_count - 1u) : (uint8_t)(state->selected_index - 1u);
    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    state->dirty = true;
    return true;
}

const ui_menu_item_t *ui_nav_selected_item(const ui_nav_state_t *state)
{
    const ui_screen_def_t *screen = ui_nav_current(state);
    if (screen == NULL || screen->items == NULL || state->selected_index >= screen->item_count) {
        return NULL;
    }
    return &screen->items[state->selected_index];
}

bool ui_nav_activate(ui_nav_state_t *state, const ui_menu_item_t **selected)
{
    const ui_menu_item_t *item = ui_nav_selected_item(state);
    if (selected != NULL) {
        *selected = item;
    }
    if (state == NULL || item == NULL) {
        return false;
    }
    if (item->action == UI_ITEM_ACTION_BACK) {
        return ui_nav_back(state);
    }
    if (item->action == UI_ITEM_ACTION_NAVIGATE) {
        return ui_nav_enter(state, item->target);
    }
    state->dirty = true;
    return true;
}
