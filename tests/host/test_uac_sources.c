#include "uac_mic_source.h"
#include "uac_speaker_sink.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(e,a) do { if ((e)!=(a)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(e), (long)(a)); exit(1);} } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #x); exit(1);} } while(0)

typedef struct { int init; int read; int write; int deinit; int vol; int mute; } fake_t;
static esp_err_t fake_init(void *ctx) { ((fake_t*)ctx)->init++; return ESP_OK; }
static esp_err_t fake_deinit(void *ctx) { ((fake_t*)ctx)->deinit++; return ESP_OK; }
static esp_err_t fake_read(int16_t *d, size_t max, size_t *read, uint32_t t, void *ctx) { (void)t; ((fake_t*)ctx)->read++; d[0]=42; *read=max > 0 ? 1 : 0; return ESP_OK; }
static esp_err_t fake_write(const int16_t *s, size_t n, size_t *written, uint32_t t, void *ctx) { (void)s; (void)t; ((fake_t*)ctx)->write++; *written=n; return ESP_OK; }
static esp_err_t fake_vol(uint8_t v, void *ctx) { ((fake_t*)ctx)->vol = v; return ESP_OK; }
static esp_err_t fake_mute(bool m, void *ctx) { ((fake_t*)ctx)->mute = m ? 1 : 0; return ESP_OK; }

static void test_mic_source_ops(void)
{
    fake_t fake = {0};
    uac_mic_source_reset_for_test();
    ASSERT_EQ(ESP_OK, uac_mic_source_init(&(uac_mic_source_ops_t){.init_capture=fake_init,.read_i16=fake_read,.deinit=fake_deinit,.ctx=&fake}));
    ASSERT_EQ(ESP_OK, uac_mic_source_start());
    int16_t sample = 0;
    size_t read = 0;
    ASSERT_EQ(ESP_OK, uac_mic_source_read_i16(&sample, 1, &read, 20));
    ASSERT_EQ(42, sample);
    ASSERT_EQ(1, read);
    ASSERT_EQ(ESP_OK, uac_mic_source_stop());
    ASSERT_EQ(1, fake.init);
    ASSERT_EQ(1, fake.read);
    ASSERT_EQ(1, fake.deinit);
}

static void test_speaker_sink_ops_and_volume_clamp(void)
{
    fake_t fake = {0};
    uac_speaker_sink_reset_for_test();
    ASSERT_EQ(ESP_OK, uac_speaker_sink_init(&(uac_speaker_sink_ops_t){.init_playback=fake_init,.write_i16=fake_write,.set_volume_percent=fake_vol,.set_mute=fake_mute,.deinit=fake_deinit,.ctx=&fake}));
    ASSERT_EQ(ESP_OK, uac_speaker_sink_start());
    int16_t samples[] = {1,2,3};
    size_t written = 0;
    ASSERT_EQ(ESP_OK, uac_speaker_sink_write_i16(samples, 3, &written, 20));
    ASSERT_EQ(3, written);
    ASSERT_EQ(ESP_OK, uac_speaker_sink_set_volume_percent(99));
    ASSERT_EQ(74, fake.vol);
    ASSERT_EQ(ESP_OK, uac_speaker_sink_set_mute(true));
    ASSERT_EQ(1, fake.mute);
    ASSERT_EQ(ESP_OK, uac_speaker_sink_stop());
    ASSERT_EQ(1, fake.init);
    ASSERT_EQ(1, fake.write);
    ASSERT_EQ(1, fake.deinit);
}

int main(void)
{
    test_mic_source_ops();
    test_speaker_sink_ops_and_volume_clamp();
    puts("uac_sources tests passed");
    return 0;
}
