#include "board_audio_clock.h"

#include "board_sticks3.h"

/*
 * Current StickS3 board-support profile: 16 kHz mono samples with a fixed
 * 12.288 MHz MCLK. For the ESP-IDF standard I2S channel API used by this
 * project, the 768x MCLK multiple produces the documented 12.288 MHz MCLK
 * for 16 kHz audio. Hardware acceptance must measure GPIO18, GPIO17, and
 * GPIO15 before claiming physical audio success.
 */
static const board_audio_clock_profile_t s_profile = {
    .sample_rate_hz = BOARD_I2S_SAMPLE_RATE,
    .mclk_hz = BOARD_I2S_MCLK_HZ,
    .bclk_hz = BOARD_I2S_BCLK_HZ,
    .lrck_hz = BOARD_I2S_LRCK_HZ,
    .bits_per_sample = 16,
    .channels = 1,
    .fixed_mclk_authoritative = true,
    .mclk_multiple_for_driver = 768,
    .es8311_clk_reg_value = 0x1B,
    .source_note = "StickS3 pin map + current project ES8311 16 kHz sequence; measure clocks on hardware",
};

const board_audio_clock_profile_t *board_audio_clock_get_profile(void)
{
    return &s_profile;
}
