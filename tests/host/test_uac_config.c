#include "uac_config.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(e,a) do { if ((e)!=(a)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(e), (long)(a)); exit(1);} } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #x); exit(1);} } while(0)

static void test_modes_and_rates(void)
{
    uac_audio_runtime_config_t out;
    ASSERT_EQ(ESP_OK, uac_audio_config_resolve(&(uac_audio_config_t){0}, &out));
    ASSERT_EQ(UAC_AUDIO_MODE_DISABLED, out.mode);
    ASSERT_EQ(ESP_OK, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true}, &out));
    ASSERT_EQ(UAC_AUDIO_MODE_MIC_ONLY, out.mode);
    ASSERT_EQ(16000, out.sample_rate_hz);
    ASSERT_EQ(ESP_OK, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.speaker_enabled=true,.sample_rate_hz=16000,.ring_buffer_bytes=8192}, &out));
    ASSERT_EQ(UAC_AUDIO_MODE_SPEAKER_ONLY, out.mode);
    ASSERT_EQ(16000, out.sample_rate_hz);
    ASSERT_EQ(8192, out.ring_buffer_bytes);
    ASSERT_EQ(ESP_ERR_NOT_SUPPORTED, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.speaker_enabled=true,.sample_rate_hz=48000,.ring_buffer_bytes=8192}, &out));
    ASSERT_EQ(ESP_ERR_INVALID_STATE, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true,.speaker_enabled=true}, &out));
    ASSERT_EQ(ESP_ERR_INVALID_STATE, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true,.speaker_enabled=true,.simultaneous_mic_speaker_enabled=true}, &out));
    ASSERT_EQ(ESP_OK, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true,.speaker_enabled=true,.combined_experimental=true}, &out));
    ASSERT_EQ(UAC_AUDIO_MODE_COMBINED_SERIALIZED, out.mode);
    ASSERT_EQ(ESP_OK, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true,.speaker_enabled=true,.combined_experimental=true,.simultaneous_mic_speaker_enabled=true}, &out));
    ASSERT_EQ(UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER, out.mode);
    ASSERT_EQ(ESP_ERR_NOT_SUPPORTED, uac_audio_config_resolve(&(uac_audio_config_t){.uac_enabled=true,.mic_enabled=true,.sample_rate_hz=44100}, &out));
    ASSERT_EQ(74, uac_audio_clamp_volume_percent(100));
}

int main(void)
{
    test_modes_and_rates();
    puts("uac_config tests passed");
    return 0;
}
