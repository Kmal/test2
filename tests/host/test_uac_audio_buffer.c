#include "uac_audio_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_EQ(e,a) do { if ((e)!=(a)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(e), (long)(a)); exit(1);} } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #x); exit(1);} } while(0)

static void test_write_read_wrap_and_stats(void)
{
    uint8_t storage[4];
    uac_audio_buffer_t b;
    ASSERT_EQ(ESP_OK, uac_audio_buffer_init(&b, storage, sizeof(storage)));
    uint8_t in[] = {1,2,3};
    ASSERT_EQ(3, uac_audio_buffer_write(&b, in, sizeof(in)));
    uint8_t out[2] = {0};
    ASSERT_EQ(2, uac_audio_buffer_read(&b, out, sizeof(out)));
    ASSERT_EQ(1, out[0]);
    ASSERT_EQ(2, out[1]);
    uint8_t more[] = {4,5,6};
    ASSERT_EQ(3, uac_audio_buffer_write(&b, more, sizeof(more)));
    uint8_t all[4] = {0};
    ASSERT_EQ(4, uac_audio_buffer_read(&b, all, sizeof(all)));
    ASSERT_TRUE(memcmp(all, (uint8_t[]){3,4,5,6}, 4) == 0);
    uac_audio_buffer_stats_t st = uac_audio_buffer_get_stats(&b);
    ASSERT_EQ(6, st.bytes_written);
    ASSERT_EQ(6, st.bytes_read);
}

static void test_overrun_and_silence_underrun(void)
{
    uint8_t storage[2];
    uac_audio_buffer_t b;
    ASSERT_EQ(ESP_OK, uac_audio_buffer_init(&b, storage, sizeof(storage)));
    uint8_t in[] = {9,8,7};
    ASSERT_EQ(2, uac_audio_buffer_write(&b, in, sizeof(in)));
    uint8_t out[4] = {1,1,1,1};
    ASSERT_EQ(4, uac_audio_buffer_read_or_silence(&b, out, sizeof(out)));
    ASSERT_TRUE(memcmp(out, (uint8_t[]){9,8,0,0}, 4) == 0);
    uac_audio_buffer_stats_t st = uac_audio_buffer_get_stats(&b);
    ASSERT_EQ(1, st.overruns);
    ASSERT_EQ(1, st.underruns);
}

int main(void)
{
    test_write_read_wrap_and_stats();
    test_overrun_and_silence_underrun();
    puts("uac_audio_buffer tests passed");
    return 0;
}
