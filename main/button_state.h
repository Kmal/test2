#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_STATE_EVENT_NONE = 0,
    BUTTON_STATE_EVENT_KEY1_SHORT,
    BUTTON_STATE_EVENT_KEY2_SHORT,
    BUTTON_STATE_EVENT_BOTH_SHORT,
} button_state_event_t;

typedef struct {
    bool stable_key1;
    bool stable_key2;
    bool last_key1;
    bool last_key2;
    bool press_key1;
    bool press_key2;
    uint32_t last_change_ms;
    uint32_t debounce_ms;
} button_state_t;

void button_state_init(button_state_t *state, uint32_t debounce_ms,
                       bool key1_pressed, bool key2_pressed, uint32_t now_ms);
button_state_event_t button_state_update(button_state_t *state,
                                         bool key1_pressed, bool key2_pressed,
                                         uint32_t now_ms);
