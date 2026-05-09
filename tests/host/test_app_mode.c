#include "app_mode.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        exit(1); \
    } \
} while (0)

#define ASSERT_STR(value) do { \
    if ((value) == NULL || *(value) == '\0') { \
        fprintf(stderr, "%s:%d expected non-empty string\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static void test_defaults_and_cycles(void)
{
    app_runtime_state_t state;
    app_runtime_state_init(&state);
    ASSERT_EQ(APP_MODE_SOUND_METER, state.app_mode);
    ASSERT_EQ(APP_DISPLAY_VU, state.display_mode);
    ASSERT_EQ(1, state.ble_telemetry_enabled);
    ASSERT_EQ(0, state.ble_pcm_debug_enabled);

    app_runtime_next_display_mode(&state);
    ASSERT_EQ(APP_DISPLAY_NUMERIC, state.display_mode);
    app_runtime_next_display_mode(&state);
    ASSERT_EQ(APP_DISPLAY_BLE_STATUS, state.display_mode);
    app_runtime_next_display_mode(&state);
    ASSERT_EQ(APP_DISPLAY_DIAGNOSTICS, state.display_mode);
    app_runtime_next_display_mode(&state);
    ASSERT_EQ(APP_DISPLAY_VU, state.display_mode);

    app_runtime_next_app_mode(&state);
    ASSERT_EQ(APP_MODE_PCM_DEBUG_STREAM, state.app_mode);
    ASSERT_EQ(1, state.ble_pcm_debug_enabled);
    app_runtime_next_app_mode(&state);
    ASSERT_EQ(APP_MODE_CALIBRATION, state.app_mode);
    app_runtime_next_app_mode(&state);
    ASSERT_EQ(APP_MODE_PAUSED, state.app_mode);
    app_runtime_next_app_mode(&state);
    ASSERT_EQ(APP_MODE_SOUND_METER, state.app_mode);
}

int main(void)
{
    test_defaults_and_cycles();
    ASSERT_STR(app_mode_name(APP_MODE_SOUND_METER));
    ASSERT_STR(app_display_mode_name(APP_DISPLAY_VU));
    puts("app_mode tests passed");
    return 0;
}
