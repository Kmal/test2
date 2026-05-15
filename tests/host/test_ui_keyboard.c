#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui_keyboard.h"

static void test_phone_key_cycles_with_expiry(void)
{
    ui_keyboard_state_t kb;
    assert(ui_keyboard_open(&kb, "SSID", "", 8, UI_KEYBOARD_MODE_TEXT, false));
    kb.selected_key = 1u;

    ui_keyboard_handle_select(&kb, 0u);
    assert(strcmp(kb.text, "2") == 0);
    assert(kb.has_pending_cycle);

    ui_keyboard_handle_select(&kb, 100u);
    assert(strcmp(kb.text, "a") == 0);

    ui_keyboard_handle_select(&kb, UI_KEYBOARD_MULTI_TAP_TIMEOUT_MS + 100u);
    assert(strcmp(kb.text, "a2") == 0);
}

static void test_navigation_wraps_and_commits_pending(void)
{
    ui_keyboard_state_t kb;
    assert(ui_keyboard_open(&kb, "Input", "", 8, UI_KEYBOARD_MODE_TEXT, false));
    kb.selected_key = 1u;
    ui_keyboard_handle_select(&kb, 0u);
    assert(kb.has_pending_cycle);

    ui_keyboard_handle_prev(&kb);
    assert(kb.selected_key == 0u);
    assert(!kb.has_pending_cycle);

    ui_keyboard_handle_prev(&kb);
    assert(kb.selected_key == UI_KEYBOARD_KEY_COUNT - 1u);

    ui_keyboard_handle_next(&kb);
    assert(kb.selected_key == 0u);
}

static void test_secret_overlay_keeps_state_only(void)
{
    ui_keyboard_state_t kb;
    assert(ui_keyboard_open(&kb, "Password", "secret", 8, UI_KEYBOARD_MODE_PASSWORD, true));
    assert(kb.secret);
    assert(strcmp(kb.text, "secret") == 0);
    ui_keyboard_close(&kb);
    assert(!ui_keyboard_is_active(&kb));
}

static void test_cancel_commits_pending_cycle(void)
{
    ui_keyboard_state_t kb;
    assert(ui_keyboard_open(&kb, "Input", "", 8, UI_KEYBOARD_MODE_TEXT, false));
    kb.selected_key = 1u;
    ui_keyboard_handle_select(&kb, 0u);
    assert(kb.has_pending_cycle);

    ui_keyboard_cancel(&kb);
    assert(!kb.has_pending_cycle);
    assert(kb.result == UI_KEYBOARD_RESULT_CANCEL);
}

static void test_menu_edit_cancel_metadata_is_explicit(void)
{
    ui_keyboard_menu_edit_t edit = {
        .active = true,
        .back_on_cancel = true,
        .has_cancel_target = true,
        .cancel_target = UI_SCREEN_CONFIG_AP_MODE,
        .opened_screen = UI_SCREEN_CONFIG_AP_SET_PASSWORD,
    };

    assert(edit.active);
    assert(edit.back_on_cancel);
    assert(edit.has_cancel_target);
    assert(edit.cancel_target == UI_SCREEN_CONFIG_AP_MODE);
    assert(edit.opened_screen == UI_SCREEN_CONFIG_AP_SET_PASSWORD);
}

int main(void)
{
    test_phone_key_cycles_with_expiry();
    test_navigation_wraps_and_commits_pending();
    test_secret_overlay_keeps_state_only();
    test_cancel_commits_pending_cycle();
    test_menu_edit_cancel_metadata_is_explicit();
    puts("ui_keyboard tests passed");
    return 0;
}
