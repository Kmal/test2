#include "button_state.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        exit(1); \
    } \
} while (0)

static void test_bounce_emits_nothing(void)
{
    button_state_t state;
    button_state_init(&state, 50, false, false, 0);
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, false, 10));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 20));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 80));
}

static void test_key1_short_emits_once_on_release(void)
{
    button_state_t state;
    button_state_init(&state, 50, false, false, 0);
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, false, 1));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, false, 60));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, false, 120));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 121));
    ASSERT_EQ(BUTTON_STATE_EVENT_KEY1_SHORT, button_state_update(&state, false, false, 180));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 240));
}

static void test_key2_short_emits_once_on_release(void)
{
    button_state_t state;
    button_state_init(&state, 50, false, false, 0);
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, true, 1));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, true, 60));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 61));
    ASSERT_EQ(BUTTON_STATE_EVENT_KEY2_SHORT, button_state_update(&state, false, false, 120));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 180));
}

static void test_both_short_is_deterministic(void)
{
    button_state_t state;
    button_state_init(&state, 50, false, false, 0);
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, true, 1));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, true, true, 60));
    ASSERT_EQ(BUTTON_STATE_EVENT_NONE, button_state_update(&state, false, false, 61));
    ASSERT_EQ(BUTTON_STATE_EVENT_BOTH_SHORT, button_state_update(&state, false, false, 120));
}

int main(void)
{
    test_bounce_emits_nothing();
    test_key1_short_emits_once_on_release();
    test_key2_short_emits_once_on_release();
    test_both_short_is_deterministic();
    puts("button_state tests passed");
    return 0;
}
