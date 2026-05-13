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

static void test_defaults(void)
{
    app_runtime_state_t state;
    app_runtime_state_init(&state);
    ASSERT_EQ(APP_MODE_CONTROL, state.app_mode);
    ASSERT_EQ(0, state.mode_change_count);
}

int main(void)
{
    test_defaults();
    ASSERT_STR(app_mode_name(APP_MODE_CONTROL));
    puts("app_mode tests passed");
    return 0;
}
