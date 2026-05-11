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

static void test_home_network_hierarchy(void)
{
    ui_nav_state_t nav;
    ui_nav_init(&nav);
    const ui_screen_def_t *home = ui_nav_current(&nav);
    ASSERT_TRUE(home != NULL);
    ASSERT_TRUE(strcmp(home->title, "Home") == 0);
    ASSERT_TRUE(find_item(home, "Network") >= 0);

    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK));
    const ui_screen_def_t *network = ui_nav_current(&nav);
    ASSERT_TRUE(network != NULL);
    ASSERT_TRUE(find_item(network, "Wi-Fi Mode") >= 0);
    ASSERT_TRUE(find_item(network, "AP Mode") >= 0);
    ASSERT_TRUE(find_item(network, "Network Status") >= 0);
}

static void test_selection_and_back(void)
{
    ui_nav_state_t nav;
    ui_nav_init(&nav);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK));
    ASSERT_TRUE(ui_nav_next(&nav));
    const ui_menu_item_t *item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "AP Mode") == 0);
    ASSERT_TRUE(ui_nav_activate(&nav, &item));
    ASSERT_TRUE(nav.current == UI_SCREEN_NETWORK_AP);
    ASSERT_TRUE(ui_nav_back(&nav));
    ASSERT_TRUE(nav.current == UI_SCREEN_NETWORK);
}

static void test_wifi_actions_are_direct(void)
{
    ui_nav_state_t nav;
    const ui_menu_item_t *item = NULL;
    ui_nav_init(&nav);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK_WIFI));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Scan Nearby Wi-Fi") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_START_WIFI_SCAN);
    ASSERT_TRUE(ui_nav_next(&nav));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Hidden / Manual SSID") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_NAVIGATE);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK_WIFI_MANUAL_SSID));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Enter SSID") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_OPEN_KEYBOARD);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK_WIFI_SAVED));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Show Saved SSID") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_NONE);
    ASSERT_TRUE(ui_nav_enter(&nav, UI_SCREEN_NETWORK_AP));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Set AP Name") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_OPEN_KEYBOARD);
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    ASSERT_TRUE(ui_nav_next(&nav));
    item = ui_nav_selected_item(&nav);
    ASSERT_TRUE(item != NULL);
    ASSERT_TRUE(strcmp(item->label, "Show AP URL") == 0);
    ASSERT_TRUE(item->action == UI_ITEM_ACTION_NAVIGATE);
}

int main(void)
{
    test_home_network_hierarchy();
    test_selection_and_back();
    test_wifi_actions_are_direct();
    puts("ui_nav tests passed");
    return 0;
}
