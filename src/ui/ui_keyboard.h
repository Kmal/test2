#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_UI_KEYBOARD_MAX_TEXT UI_TEXT_WIFI_PASSWORD_MAX
#define UI_KEYBOARD_KEY_COUNT 16
#define UI_KEYBOARD_OVERLAY_H 108
#define UI_KEYBOARD_MULTI_TAP_TIMEOUT_MS 2000

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
    bool back_on_cancel;
    bool has_cancel_target;
    ui_screen_id_t cancel_target;
    ui_screen_id_t opened_screen;
} ui_keyboard_menu_edit_t;

extern const ui_key_def_t ui_keyboard_9key_defs[UI_KEYBOARD_KEY_COUNT];

ui_keyboard_state_t *ui_keyboard_session_state(void);
ui_keyboard_menu_edit_t *ui_keyboard_session_edit(void);

bool ui_keyboard_open(ui_keyboard_state_t *kb, const char *title, const char *initial, size_t max_len, ui_keyboard_mode_t mode, bool secret);
void ui_keyboard_close(ui_keyboard_state_t *kb);
bool ui_keyboard_is_active(const ui_keyboard_state_t *kb);
void ui_keyboard_handle_next(ui_keyboard_state_t *kb);
void ui_keyboard_handle_prev(ui_keyboard_state_t *kb);
void ui_keyboard_handle_select(ui_keyboard_state_t *kb, uint32_t now_ms);
void ui_keyboard_commit_pending(ui_keyboard_state_t *kb);
void ui_keyboard_commit_expired_pending(ui_keyboard_state_t *kb, uint32_t now_ms);
void ui_keyboard_cancel(ui_keyboard_state_t *kb);

#ifdef __cplusplus
}
#endif

#endif // UI_KEYBOARD_H
