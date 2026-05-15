#include "uac_device_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_EQ(e,a) do { if ((e)!=(a)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(e), (long)(a)); exit(1);} } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #x); exit(1);} } while(0)

static void test_mic_callback_reads_silence_on_underrun(void)
{
    uint8_t storage[8];
    uac_audio_buffer_t mic;
    ASSERT_EQ(ESP_OK, uac_audio_buffer_init(&mic, storage, sizeof(storage)));
    uint8_t pcm[] = {1,2};
    ASSERT_EQ(2, uac_audio_buffer_write(&mic, pcm, sizeof(pcm)));
    uac_device_adapter_t adapter;
    uac_audio_runtime_config_t rt = {.mode=UAC_AUDIO_MODE_MIC_ONLY,.sample_rate_hz=16000,.channels_per_direction=1,.bits_per_sample=16,.ring_buffer_bytes=8};
    ASSERT_EQ(ESP_OK, uac_device_adapter_init(&adapter, &rt, &mic, NULL));
    uac_device_descriptor_plan_t plan = uac_device_adapter_descriptor_plan(&adapter);
    ASSERT_TRUE(plan.input_enabled);
    ASSERT_TRUE(!plan.output_enabled);
    uint8_t out[4] = {9,9,9,9};
    size_t got = 0;
    ASSERT_EQ(ESP_OK, uac_device_adapter_input_cb(out, sizeof(out), &got, &adapter));
    ASSERT_EQ(4, got);
    ASSERT_TRUE(memcmp(out, (uint8_t[]){1,2,0,0}, 4) == 0);
}

static void test_speaker_callback_writes_and_clamps_volume(void)
{
    uint8_t storage[4];
    uac_audio_buffer_t spk;
    ASSERT_EQ(ESP_OK, uac_audio_buffer_init(&spk, storage, sizeof(storage)));
    uac_device_adapter_t adapter;
    uac_audio_runtime_config_t rt = {.mode=UAC_AUDIO_MODE_SPEAKER_ONLY,.sample_rate_hz=16000,.channels_per_direction=1,.bits_per_sample=16,.ring_buffer_bytes=4};
    ASSERT_EQ(ESP_OK, uac_device_adapter_init(&adapter, &rt, NULL, &spk));
    uac_device_descriptor_plan_t plan = uac_device_adapter_descriptor_plan(&adapter);
    ASSERT_TRUE(!plan.input_enabled);
    ASSERT_TRUE(plan.output_enabled);
    uint8_t in[] = {4,3,2};
    ASSERT_EQ(ESP_OK, uac_device_adapter_output_cb(in, sizeof(in), &adapter));
    ASSERT_EQ(3, uac_audio_buffer_get_stats(&spk).bytes_written);
    uac_device_adapter_set_volume_cb(120, &adapter);
    ASSERT_EQ(74, adapter.volume_percent);
    uac_device_adapter_set_mute_cb(1, &adapter);
    ASSERT_TRUE(adapter.muted);
}

int main(void)
{
    test_mic_callback_reads_silence_on_underrun();
    test_speaker_callback_writes_and_clamps_volume();
    puts("uac_device_adapter tests passed");
    return 0;
}
