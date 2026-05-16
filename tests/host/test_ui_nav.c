#include "ui_nav.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr) assert(expr)

static int find_item(const ui_screen_def_t *screen, const char *label)
{
    for (size_t i = 0; screen != NULL && i < screen->item_count; ++i) {
        if (strcmp(screen->items[i].label, label) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void test_main_setup_hierarchy(void)
{
    ui_nav_state_t nav;
    ui_nav_init(&nav);
    const ui_screen_def_t *main = ui_nav_current(&nav);
    ASSERT_TRUE(main != NULL);
    ASSERT_TRUE(strcmp(main->title, "Main") == 0);
    ASSERT_TRUE(find_item(main, "Web UI") >= 0);
    ASSERT_TRUE(find_item(main, "Connect to Wi-Fi") >= 0);
    ASSERT_TRUE(find_item(main, "BLE Status") >= 0);

    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONFIG_WEB_UI));
    const ui_screen_def_t *config = ui_nav_current(&nav);
    ASSERT_TRUE(config != NULL);
    ASSERT_TRUE(find_item(config, "Wi-Fi Mode") >= 0);
    const ui_menu_item_t *wifi_mode = ui_nav_selected_item(&nav);
    ASSERT_TRUE(wifi_mode != NULL);
    ASSERT_TRUE(strcmp(wifi_mode->label, "Wi-Fi Mode") == 0);
    ASSERT_TRUE(wifi_mode->action == UI_ACTION_WEB_UI_WIFI_MODE);
    ASSERT_TRUE(find_item(config, "AP Mode") >= 0);
}

static void test_selection_and_back(void)
{
    ui_nav_state_t nav;
    ui_nav_init(&nav);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONFIG_WEB_UI));
    ASSERT_TRUE(ui_nav_next(&nav));
    const ui_menu_item_t *item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "AP Mode") == 0);
    ASSERT_TRUE(ui_nav_activate(&nav, &item));
    ASSERT_TRUE(nav.current == UI_SCREEN_CONFIG_AP_MODE);
    ASSERT_TRUE(ui_nav_back(&nav));
    ASSERT_TRUE(nav.current == UI_SCREEN_CONFIG_WEB_UI);
}

static void test_wifi_actions_are_direct(void)
{
    ui_nav_state_t nav;
    const ui_menu_item_t *item = NULL;
    ui_nav_init(&nav);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONNECT_WIFI));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Scan Nearby Wi-Fi") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_WIFI_SCAN);
    ASSERT_TRUE(ui_nav_next(&nav));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Hidden / Manual SSID") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_WIFI_ENTER_SSID);
    ASSERT_TRUE(item->target == UI_SCREEN_CONNECT_WIFI_ENTER_SSID);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONNECT_WIFI_SCAN));
    const ui_screen_def_t *scan = ui_nav_current(&nav);
    ASSERT_TRUE(scan != NULL);
    ASSERT_TRUE(scan->item_count == 0u);
    ASSERT_TRUE(ui_nav_selected_item(&nav) == NULL);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONNECT_WIFI_ENTER_PASSWORD));
    const ui_screen_def_t *password = ui_nav_current(&nav);
    ASSERT_TRUE(password != NULL);
    ASSERT_TRUE(password->item_count == 0u);
    ASSERT_TRUE(ui_nav_selected_item(&nav) == NULL);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_CONFIG_AP_MODE));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Set AP Name") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_AP_ENTER_NAME);
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Show AP URL") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_AP_SHOW_URL);
}

static void test_settings_hierarchy(void)
{
    ui_nav_state_t nav;
    const ui_menu_item_t *item = NULL;
    ui_nav_init(&nav);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_SETTINGS));
    const ui_screen_def_t *settings = ui_nav_current(&nav);
    ASSERT_TRUE(settings != NULL);
    ASSERT_TRUE(find_item(settings, "Display") >= 0);
    ASSERT_TRUE(find_item(settings, "Device") >= 0);
    ASSERT_TRUE(find_item(settings, "Connectivity") >= 0);
    ASSERT_TRUE(find_item(settings, "Automation") >= 0);
    ASSERT_TRUE(find_item(settings, "Hardware") >= 0);
    ASSERT_TRUE(find_item(settings, "Maintenance") >= 0);
    ASSERT_TRUE(find_item(settings, "About") >= 0);
    for (size_t i = 0; i < settings->item_count; ++i) {
        ASSERT_TRUE(settings->items[i].action == UI_ACTION_NAVIGATE);
    }

    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_SETTINGS_DISPLAY));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Edit Timezone") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_SETTINGS_EDIT_TIMEZONE);

    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_SETTINGS_CONNECTIVITY));
    const ui_screen_def_t *connectivity = ui_nav_current(&nav);
    ASSERT_TRUE(connectivity != NULL);
    ASSERT_TRUE(find_item(connectivity, "Web UI Service") >= 0);
    nav.selected_index = (uint8_t)find_item(connectivity, "Web UI Service");
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(item->action == UI_ACTION_SETTINGS_TOGGLE_WEB_UI_SERVICE);

    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_SETTINGS_MAINTENANCE));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Restart Device") == 0);
    ASSERT_TRUE(item->action == UI_ACTION_SETTINGS_RESTART);
}

int main(void)
{
    test_main_setup_hierarchy();
    test_selection_and_back();
    test_wifi_actions_are_direct();
    test_settings_hierarchy();
    puts("ui_nav tests passed");
    return 0;
}
