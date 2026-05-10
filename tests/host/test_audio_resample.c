#include "audio_resample.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        exit(1); \
    } \
} while (0)

static void test_zero_input_decimates_to_zero(void)
{
    audio_resample_decimator_t decimator;
    int16_t input[8] = {0};
    int16_t output[4] = {1, 1, 1, 1};
    audio_resample_decimator_reset(&decimator);
    ASSERT_EQ(4, audio_resample_decimate_2to1(&decimator, input, 8, output, 4));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(0, output[i]);
    }
}

static void test_constant_input_converges(void)
{
    audio_resample_decimator_t decimator;
    int16_t input[12];
    int16_t output[6] = {0};
    for (int i = 0; i < 12; ++i) {
        input[i] = 800;
    }
    audio_resample_decimator_reset(&decimator);
    ASSERT_EQ(6, audio_resample_decimate_2to1(&decimator, input, 12, output, 6));
    ASSERT_EQ(800, output[5]);
}

static void test_expander_interpolates(void)
{
    audio_resample_expander_t expander;
    int16_t input[2] = {0, 100};
    int16_t output[4] = {0};
    audio_resample_expander_reset(&expander);
    ASSERT_EQ(4, audio_resample_expand_2to1(&expander, input, 2, output, 4));
    ASSERT_EQ(0, output[0]);
    ASSERT_EQ(0, output[1]);
    ASSERT_EQ(50, output[2]);
    ASSERT_EQ(100, output[3]);
}

static void test_zero_length_inputs(void)
{
    audio_resample_decimator_t decimator;
    audio_resample_expander_t expander;
    int16_t output[2] = {0};
    audio_resample_decimator_reset(&decimator);
    audio_resample_expander_reset(&expander);
    ASSERT_EQ(0, audio_resample_decimate_2to1(&decimator, output, 0, output, 2));
    ASSERT_EQ(0, audio_resample_expand_2to1(&expander, output, 0, output, 2));
}


static void test_invalid_arguments_are_safe(void)
{
    audio_resample_decimator_t decimator;
    audio_resample_expander_t expander;
    int16_t input[2] = {1, 2};
    int16_t output[2] = {0};

    audio_resample_decimator_reset(NULL);
    audio_resample_expander_reset(NULL);
    audio_resample_decimator_reset(&decimator);
    audio_resample_expander_reset(&expander);

    ASSERT_EQ(0, audio_resample_decimate_2to1(NULL, input, 2, output, 2));
    ASSERT_EQ(0, audio_resample_decimate_2to1(&decimator, NULL, 2, output, 2));
    ASSERT_EQ(0, audio_resample_decimate_2to1(&decimator, input, 2, NULL, 2));
    ASSERT_EQ(0, audio_resample_expand_2to1(NULL, input, 2, output, 2));
    ASSERT_EQ(0, audio_resample_expand_2to1(&expander, NULL, 2, output, 2));
    ASSERT_EQ(0, audio_resample_expand_2to1(&expander, input, 2, NULL, 2));
}

int main(void)
{
    test_zero_input_decimates_to_zero();
    test_constant_input_converges();
    test_expander_interpolates();
    test_zero_length_inputs();
    test_invalid_arguments_are_safe();
    puts("audio_resample tests passed");
    return 0;
}
