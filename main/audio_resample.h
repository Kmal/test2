/*
 * Lightweight fixed-point audio resampling helpers for HFP voice audio.
 *
 * The ES8311 codec runs at 16 kHz, 16-bit, mono PCM.  Narrow-band HFP
 * CVSD audio is 8 kHz, so microphone samples must be low-pass filtered
 * before 2:1 decimation and incoming monitoring audio must be expanded
 * back to 16 kHz before it is written to the codec stream.
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Compile-time audio format and decimator constants. */
#define AUDIO_RESAMPLE_INPUT_RATE_HZ      16000
#define AUDIO_RESAMPLE_OUTPUT_RATE_HZ     8000
#define AUDIO_RESAMPLE_DECIMATION_FACTOR  2
#define AUDIO_RESAMPLE_FILTER_LENGTH      4

#if AUDIO_RESAMPLE_INPUT_RATE_HZ != (AUDIO_RESAMPLE_OUTPUT_RATE_HZ * AUDIO_RESAMPLE_DECIMATION_FACTOR)
#error "The current HFP resampler only supports exact 2:1 decimation"
#endif

/**
 * @brief Persistent state for the low-pass decimator.
 *
 * The filter is a 4-tap fixed-point FIR with coefficients [1 3 3 1] / 8.
 * It needs only the last three samples plus a decimation phase bit, keeping
 * CPU and memory use low enough for the Bluetooth HFP data callback.
 */
typedef struct {
    int16_t history[AUDIO_RESAMPLE_FILTER_LENGTH - 1];
    uint8_t phase;
} audio_resample_decimator_t;

/**
 * @brief Persistent state for the 2:1 narrow-band monitoring expander.
 *
 * The expander inserts one linearly interpolated sample between each pair of
 * adjacent 8 kHz samples so the stream can be written to the 16 kHz codec.
 */
typedef struct {
    int16_t previous_sample;
    bool has_previous_sample;
} audio_resample_expander_t;

/**
 * @brief Reset the decimator history and phase.
 */
void audio_resample_decimator_reset(audio_resample_decimator_t *decimator);

/**
 * @brief Low-pass filter and decimate 16-bit mono PCM by 2:1.
 *
 * @param decimator Persistent decimator state across callback invocations.
 * @param input Source PCM samples at AUDIO_RESAMPLE_INPUT_RATE_HZ.
 * @param input_samples Number of int16_t samples in @p input.
 * @param output Destination PCM buffer at AUDIO_RESAMPLE_OUTPUT_RATE_HZ.
 * @param output_capacity Capacity of @p output in int16_t samples.
 * @return Number of output samples written.
 */
size_t audio_resample_decimate_2to1(audio_resample_decimator_t *decimator,
                                    const int16_t *input,
                                    size_t input_samples,
                                    int16_t *output,
                                    size_t output_capacity);

/**
 * @brief Reset the expander history.
 */
void audio_resample_expander_reset(audio_resample_expander_t *expander);

/**
 * @brief Expand 16-bit mono PCM by 2:1 using linear interpolation.
 *
 * @param expander Persistent expander state across callback invocations.
 * @param input Source PCM samples at AUDIO_RESAMPLE_OUTPUT_RATE_HZ.
 * @param input_samples Number of int16_t samples in @p input.
 * @param output Destination PCM buffer at AUDIO_RESAMPLE_INPUT_RATE_HZ.
 * @param output_capacity Capacity of @p output in int16_t samples.
 * @return Number of output samples written.
 */
size_t audio_resample_expand_2to1(audio_resample_expander_t *expander,
                                  const int16_t *input,
                                  size_t input_samples,
                                  int16_t *output,
                                  size_t output_capacity);
