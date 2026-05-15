#include "ui_keyboard.h"

#include <stdio.h>
#include <string.h>

static ui_keyboard_state_t s_keyboard;
static ui_keyboard_menu_edit_t s_keyboard_edit;

const ui_key_def_t ui_keyboard_9key_defs[UI_KEYBOARD_KEY_COUNT] = {
    { UI_KEY_KIND_CHAR, "1", "1", "1", '1' },
    { UI_KEY_KIND_CHAR, "2ABC", "2abcABC", "2abc", '2' },
    { UI_KEY_KIND_CHAR, "3DEF", "3defDEF", "3def", '3' },
    { UI_KEY_KIND_CHAR, "-", "-", "-", '-' },
    { UI_KEY_KIND_CHAR, "4GHI", "4ghiGHI", "4ghi", '4' },
    { UI_KEY_KIND_CHAR, "5JKL", "5jklJKL", "5jkl", '5' },
    { UI_KEY_KIND_CHAR, "6MNO", "6mnoMNO", "6mno", '6' },
    { UI_KEY_KIND_CHAR, ".", ".", ".", '.' },
    { UI_KEY_KIND_CHAR, "7PQRS", "7pqrsPQRS", "7pqrs", '7' },
    { UI_KEY_KIND_CHAR, "8TUV", "8tuvTUV", "8tuv", '8' },
    { UI_KEY_KIND_CHAR, "9WXYZ", "9wxyzWXYZ", "9wxyz", '9' },
    { UI_KEY_KIND_DELETE, "DEL", NULL, NULL, 0 },
    { UI_KEY_KIND_CHAR, "*#(", "*#(", "*#(", '*' },
    { UI_KEY_KIND_CHAR, "0+", "0+", "0+", '0' },
    { UI_KEY_KIND_SPACE, "_", NULL, NULL, 0 },
    { UI_KEY_KIND_OK, "Next", NULL, NULL, 0 },
};

ui_keyboard_state_t *ui_keyboard_session_state(void)
{
    return &s_keyboard;
}

ui_keyboard_menu_edit_t *ui_keyboard_session_edit(void)
{
    return &s_keyboard_edit;
}

static const char *ui_keyboard_chars_for_key(const ui_keyboard_state_t *kb, const ui_key_def_t *key)
{
    if (kb->mode == UI_KEYBOARD_MODE_NUMERIC) return NULL;
    return kb->mode == UI_KEYBOARD_MODE_SYMBOL ? key->chars_symbol : key->chars_text;
}

static bool ui_keyboard_append_char(ui_keyboard_state_t *kb, char ch)
{
    if (kb == NULL) return false;
    size_t len = strlen(kb->text);
    if (len >= kb->max_len || len + 1u >= sizeof(kb->text)) {
        return false;
    }
    kb->text[len] = ch;
    kb->text[len + 1u] = '\0';
    return true;
}

void ui_keyboard_commit_pending(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    kb->has_pending_cycle = false;
}

void ui_keyboard_commit_expired_pending(ui_keyboard_state_t *kb, uint32_t now_ms)
{
    if (kb == NULL || !kb->has_pending_cycle) return;
    if ((now_ms - kb->last_key_tick_ms) >= UI_KEYBOARD_MULTI_TAP_TIMEOUT_MS) {
        ui_keyboard_commit_pending(kb);
    }
}

void ui_keyboard_cancel(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    ui_keyboard_commit_pending(kb);
    kb->result = UI_KEYBOARD_RESULT_CANCEL;
}

static void ui_keyboard_backspace(ui_keyboard_state_t *kb)
{
    size_t len = strlen(kb->text);
    if (len > 0u) kb->text[len - 1u] = '\0';
    kb->has_pending_cycle = false;
}

bool ui_keyboard_open(ui_keyboard_state_t *kb, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret)
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

void ui_keyboard_close(ui_keyboard_state_t *kb)
{
    if (kb != NULL) kb->active = false;
}

bool ui_keyboard_is_active(const ui_keyboard_state_t *kb)
{
    return kb != NULL && kb->active;
}

void ui_keyboard_handle_next(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    ui_keyboard_commit_pending(kb);
    kb->selected_key = (uint8_t)((kb->selected_key + 1u) % UI_KEYBOARD_KEY_COUNT);
}

void ui_keyboard_handle_prev(ui_keyboard_state_t *kb)
{
    if (kb == NULL) return;
    ui_keyboard_commit_pending(kb);
    kb->selected_key = kb->selected_key == 0u ? UI_KEYBOARD_KEY_COUNT - 1u : kb->selected_key - 1u;
}

void ui_keyboard_handle_select(ui_keyboard_state_t *kb, uint32_t now_ms)
{
    if (kb == NULL) return;
    const ui_key_def_t *key = &ui_keyboard_9key_defs[kb->selected_key];
    switch (key->kind) {
    case UI_KEY_KIND_CHAR: {
        if (kb->mode == UI_KEYBOARD_MODE_NUMERIC) {
            (void)ui_keyboard_append_char(kb, key->numeric_char);
            return;
        }
        const char *chars = ui_keyboard_chars_for_key(kb, key);
        if (chars == NULL || chars[0] == '\0') return;
        ui_keyboard_commit_expired_pending(kb, now_ms);
        if (kb->has_pending_cycle && kb->last_key == kb->selected_key && strlen(kb->text) > 0u) {
            kb->cycle_index = (uint8_t)((kb->cycle_index + 1u) % strlen(chars));
            kb->text[strlen(kb->text) - 1u] = chars[kb->cycle_index];
        } else {
            ui_keyboard_commit_pending(kb);
            if (ui_keyboard_append_char(kb, chars[0])) {
                kb->last_key = kb->selected_key;
                kb->cycle_index = 0u;
                kb->has_pending_cycle = true;
            }
        }
        kb->last_key_tick_ms = now_ms;
        break;
    }
    case UI_KEY_KIND_OK: ui_keyboard_commit_pending(kb); kb->result = UI_KEYBOARD_RESULT_OK; break;
    case UI_KEY_KIND_DELETE: ui_keyboard_backspace(kb); break;
    case UI_KEY_KIND_SPACE: ui_keyboard_commit_pending(kb); (void)ui_keyboard_append_char(kb, ' '); break;
    case UI_KEY_KIND_MODE:
        ui_keyboard_commit_pending(kb);
        if (kb->mode == UI_KEYBOARD_MODE_TEXT || kb->mode == UI_KEYBOARD_MODE_PASSWORD) kb->mode = UI_KEYBOARD_MODE_SYMBOL;
        else if (kb->mode == UI_KEYBOARD_MODE_SYMBOL) kb->mode = kb->secret ? UI_KEYBOARD_MODE_PASSWORD : UI_KEYBOARD_MODE_TEXT;
        break;
    case UI_KEY_KIND_CANCEL: kb->result = UI_KEYBOARD_RESULT_CANCEL; break;
    default: break;
    }
}
