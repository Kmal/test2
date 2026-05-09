#include "board_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); exit(1); } } while (0)

typedef struct {
    char log[256];
    esp_err_t fail_i2c;
    esp_err_t fail_probe;
    esp_err_t fail_power;
    esp_err_t fail_i2s;
    esp_err_t fail_codec;
} fake_audio_t;

static void append(fake_audio_t *fake, const char *text)
{
    strncat(fake->log, text, sizeof(fake->log) - strlen(fake->log) - 1);
}

static esp_err_t fake_i2c(void *ctx) { fake_audio_t *f = ctx; append(f, "i2c>"); return f->fail_i2c; }
static esp_err_t fake_probe(void *ctx) { fake_audio_t *f = ctx; append(f, "probe>"); return f->fail_probe; }
static esp_err_t fake_power(void *ctx) { fake_audio_t *f = ctx; append(f, "power>"); return f->fail_power; }
static esp_err_t fake_i2s(board_audio_profile_t profile, void *ctx) { fake_audio_t *f = ctx; append(f, profile == BOARD_AUDIO_PROFILE_CAPTURE_ONLY ? "i2s-rx>" : "i2s-fd>"); return f->fail_i2s; }
static esp_err_t fake_codec(board_audio_profile_t profile, void *ctx) { fake_audio_t *f = ctx; append(f, profile == BOARD_AUDIO_PROFILE_CAPTURE_ONLY ? "codec-adc>" : "codec-fd>"); return f->fail_codec; }
static esp_err_t fake_cleanup(esp_err_t cause, void *ctx) { fake_audio_t *f = ctx; (void)cause; append(f, "cleanup>"); return ESP_OK; }

static board_audio_ops_t ops(fake_audio_t *fake)
{
    return (board_audio_ops_t){
        .i2c_init = fake_i2c,
        .m5pm1_probe = fake_probe,
        .audio_power_enable = fake_power,
        .i2s_init_profile = fake_i2s,
        .es8311_init_profile = fake_codec,
        .cleanup_on_failure = fake_cleanup,
        .ctx = fake,
    };
}

static void test_capture_only_order(void)
{
    fake_audio_t fake = {0};
    board_audio_ops_t o = ops(&fake);
    board_audio_config_t cfg = {.profile = BOARD_AUDIO_PROFILE_CAPTURE_ONLY, .probe_m5pm1 = true, .require_audio_power_enable = false};
    ASSERT_EQ(ESP_OK, board_audio_init_with_ops(&cfg, &o));
    ASSERT_TRUE(strcmp(fake.log, "i2c>probe>i2s-rx>codec-adc>") == 0);
}

static void test_power_gate_order_when_required(void)
{
    fake_audio_t fake = {0};
    board_audio_ops_t o = ops(&fake);
    board_audio_config_t cfg = {.profile = BOARD_AUDIO_PROFILE_FULL_DUPLEX, .probe_m5pm1 = true, .require_audio_power_enable = true};
    ASSERT_EQ(ESP_OK, board_audio_init_with_ops(&cfg, &o));
    ASSERT_TRUE(strcmp(fake.log, "i2c>probe>power>i2s-fd>codec-fd>") == 0);
}

static void test_failure_aborts_later_steps_and_cleans_up(void)
{
    fake_audio_t fake = {.fail_probe = ESP_FAIL};
    board_audio_ops_t o = ops(&fake);
    board_audio_config_t cfg = {.profile = BOARD_AUDIO_PROFILE_CAPTURE_ONLY, .probe_m5pm1 = true, .require_audio_power_enable = false};
    ASSERT_EQ(ESP_FAIL, board_audio_init_with_ops(&cfg, &o));
    ASSERT_TRUE(strcmp(fake.log, "i2c>probe>cleanup>") == 0);
}

int main(void)
{
    test_capture_only_order();
    test_power_gate_order_when_required();
    test_failure_aborts_later_steps_and_cleans_up();
    puts("board_audio tests passed");
    return 0;
}
