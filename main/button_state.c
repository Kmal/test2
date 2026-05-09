#include "button_state.h"

void button_state_init(button_state_t *state, uint32_t debounce_ms,
                       bool key1_pressed, bool key2_pressed, uint32_t now_ms)
{
    state->stable_key1 = key1_pressed;
    state->stable_key2 = key2_pressed;
    state->last_key1 = key1_pressed;
    state->last_key2 = key2_pressed;
    state->press_key1 = false;
    state->press_key2 = false;
    state->last_change_ms = now_ms;
    state->debounce_ms = debounce_ms;
}

button_state_event_t button_state_update(button_state_t *state,
                                         bool key1_pressed, bool key2_pressed,
                                         uint32_t now_ms)
{
    if (key1_pressed != state->last_key1 || key2_pressed != state->last_key2) {
        state->last_key1 = key1_pressed;
        state->last_key2 = key2_pressed;
        state->last_change_ms = now_ms;
        return BUTTON_STATE_EVENT_NONE;
    }

    if ((now_ms - state->last_change_ms) < state->debounce_ms) {
        return BUTTON_STATE_EVENT_NONE;
    }

    if (key1_pressed == state->stable_key1 && key2_pressed == state->stable_key2) {
        return BUTTON_STATE_EVENT_NONE;
    }

    const bool was_key1_pressed = state->press_key1;
    const bool was_key2_pressed = state->press_key2;
    state->stable_key1 = key1_pressed;
    state->stable_key2 = key2_pressed;

    if (key1_pressed || key2_pressed) {
        state->press_key1 = key1_pressed;
        state->press_key2 = key2_pressed;
        return BUTTON_STATE_EVENT_NONE;
    }

    state->press_key1 = false;
    state->press_key2 = false;
    if (was_key1_pressed && was_key2_pressed) {
        return BUTTON_STATE_EVENT_BOTH_SHORT;
    }
    if (was_key1_pressed) {
        return BUTTON_STATE_EVENT_KEY1_SHORT;
    }
    if (was_key2_pressed) {
        return BUTTON_STATE_EVENT_KEY2_SHORT;
    }
    return BUTTON_STATE_EVENT_NONE;
}
