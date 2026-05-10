#include "es8311.h"
#include "fake_register_bus.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); exit(1); } } while (0)

static void test_baseline_init_writes_to_codec(void)
{
    fake_register_bus_reset();
    ASSERT_EQ(ESP_OK, es8311_init(I2C_NUM_0, 0x18, I2S_NUM_0, 16000));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x00, 0x1f));
}

static void test_unsupported_rate_emits_no_writes(void)
{
    fake_register_bus_reset();
    ASSERT_EQ(ESP_ERR_NOT_SUPPORTED, es8311_init(I2C_NUM_0, 0x18, I2S_NUM_0, 44100));
    ASSERT_EQ(0, fake_register_bus_op_count());
}

static void test_reset_write_failure_returns_error(void)
{
    fake_register_bus_reset();
    fake_register_bus_fail_next_write(ESP_FAIL);
    ASSERT_EQ(ESP_FAIL, es8311_init(I2C_NUM_0, 0x18, I2S_NUM_0, 16000));
}

static void test_adc_only_configures_capture_path_without_unmuting_dac(void)
{
    fake_register_bus_reset();
    ASSERT_EQ(ESP_OK, es8311_init_profile(I2C_NUM_0, 0x18, I2S_NUM_0, ES8311_PROFILE_ADC_ONLY, 16000));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x01, 0x3f));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x02, 0x40));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x0a, 0x0c));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x0d, 0x01));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x0e, 0x02));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x14, 0x1a));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x17, 0xc8));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x12, 0x02));
    ASSERT_TRUE(fake_register_bus_has_write(0x18, 0x31, 0x20));
    ASSERT_TRUE(!fake_register_bus_has_write(0x18, 0x12, 0x00));
    ASSERT_TRUE(!fake_register_bus_has_write(0x18, 0x31, 0x00));
}

static void test_invalid_profile_rejected(void)
{
    fake_register_bus_reset();
    ASSERT_EQ(ESP_ERR_INVALID_ARG, es8311_init_profile(I2C_NUM_0, 0x18, I2S_NUM_0, (es8311_profile_t)99, 16000));
}

int main(void)
{
    test_baseline_init_writes_to_codec();
    test_unsupported_rate_emits_no_writes();
    test_reset_write_failure_returns_error();
    test_adc_only_configures_capture_path_without_unmuting_dac();
    test_invalid_profile_rejected();
    puts("es8311_sequence tests passed");
    return 0;
}
