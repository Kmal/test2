#include "audio_resample.h"

#include <string.h>

static int16_t clamp_i32_to_i16(int32_t sample)
{
    if (sample > INT16_MAX) {
        return INT16_MAX;
    }
    if (sample < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)sample;
}

void audio_resample_decimator_reset(audio_resample_decimator_t *decimator)
{
    memset(decimator, 0, sizeof(*decimator));
}

size_t audio_resample_decimate_2to1(audio_resample_decimator_t *decimator,
                                    const int16_t *input,
                                    size_t input_samples,
                                    int16_t *output,
                                    size_t output_capacity)
{
    size_t output_samples = 0;

    for (size_t i = 0; i < input_samples && output_samples < output_capacity; ++i) {
        const int16_t x0 = input[i];
        const int16_t x1 = decimator->history[0];
        const int16_t x2 = decimator->history[1];
        const int16_t x3 = decimator->history[2];

        /*
         * Four-tap low-pass FIR in fixed point:
         *     y = (x[n] + 3*x[n-1] + 3*x[n-2] + x[n-3]) / 8
         * Rounding is added before the arithmetic shift.  The largest
         * accumulator value is comfortably inside int32_t for 16-bit PCM.
         */
        if (decimator->phase == 1) {
            const int32_t acc = (int32_t)x0 + (3 * (int32_t)x1) +
                                (3 * (int32_t)x2) + (int32_t)x3;
            const int32_t rounded = acc + ((acc >= 0) ? 4 : -4);
            output[output_samples++] = clamp_i32_to_i16(rounded / 8);
        }

        decimator->phase ^= 1;
        decimator->history[2] = decimator->history[1];
        decimator->history[1] = decimator->history[0];
        decimator->history[0] = x0;
    }

    return output_samples;
}
