/*
 * Basic ES8311 codec initialisation.  The purpose of this file is
 * to provide a small set of helper functions sufficient for
 * recording and playback on the M5Stack Stick S3.  It resets the
 * ES8311 and configures it for I2S slave mode, 16 bit samples and
 * the desired sampling rate.  The ESP32-S3 I2S peripheral supplies
 * MCLK/BCLK/LRCK, matching the Stick S3 ES8311 schematic pinout.
 */

#include "es8311.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stddef.h>

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#ifndef ES8311_VERIFY_REGISTERS
#define ES8311_VERIFY_REGISTERS 1
#endif

static const char *TAG_CODEC = "ES8311";

/* ES8311 register addresses used by this minimal driver. */
#define ES8311_REG_RESET        0x00
#define ES8311_REG_CLKMGR_1     0x01
#define ES8311_REG_CLKMGR_2     0x02
#define ES8311_REG_CLKMGR_3     0x03
#define ES8311_REG_CLKMGR_4     0x04
#define ES8311_REG_CLKMGR_5     0x05
#define ES8311_REG_CLKMGR_6     0x06
#define ES8311_REG_CLKMGR_7     0x07
#define ES8311_REG_CLKMGR_8     0x08
#define ES8311_REG_SDP_DAC      0x09
#define ES8311_REG_SDP_ADC      0x0A
#define ES8311_REG_POWER_1      0x0D
#define ES8311_REG_POWER_2      0x0E
#define ES8311_REG_DAC_POWER    0x12
#define ES8311_REG_OUTPUT       0x13
#define ES8311_REG_MIC_GAIN     0x14
#define ES8311_REG_ADC_RAM      0x16
#define ES8311_REG_ADC_VOLUME   0x17
#define ES8311_REG_ADC_EQ       0x1C
#define ES8311_REG_DAC_MUTE     0x31
#define ES8311_REG_DAC_VOLUME   0x32
#define ES8311_REG_DAC_EQ       0x37

/* Register values/bit fields used by the validated Stick S3 init sequence. */
#define ES8311_RESET_ASSERT             0x1F
#define ES8311_RESET_RELEASE            0x00
#define ES8311_RESET_CODEC_ON           0x80
#define ES8311_CLKMGR1_MCLK_PIN_ENABLE  0x3F
#define ES8311_I2S_16BIT                (3U << 2)
#define ES8311_POWER1_UP                0x01
#define ES8311_POWER1_DOWN              0xFC
#define ES8311_POWER2_ADC_PGA_UP        0x02
#define ES8311_POWER2_ADC_PGA_DOWN      0x70
#define ES8311_DAC_POWER_UP             0x00
#define ES8311_DAC_POWER_DOWN           0x08
#define ES8311_OUTPUT_HP_ENABLE         0x10
#define ES8311_MIC_SELECT_MIC1          0x10
#define ES8311_ADC_RAM_CLEAR            0x08
#define ES8311_ADC_VOLUME_0DB           0xBF
#define ES8311_ADC_EQ_BYPASS_HPF_ON     0x6A
#define ES8311_DAC_MUTE_BITS            (BIT(6) | BIT(5))
#define ES8311_DAC_VOLUME_0DB           0xBF
#define ES8311_DAC_EQ_BYPASS            0x08

#define ES8311_I2C_TIMEOUT_MS           100
#define ES8311_MCLK_MULTIPLE            256

typedef struct {
    int sample_rate;
    uint8_t pre_div;
    uint8_t pre_mult;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
} es8311_clock_config_t;

/* Coefficients for MCLK = Fs * 256, as used by the Stick S3 I2S setup. */
static const es8311_clock_config_t ES8311_CLOCK_CONFIGS[] = {
    {8000,  2, 1, 1, 1, 0, 0x00, 0xFF, 0x04, 0x10, 0x20},
    {16000, 1, 1, 1, 1, 0, 0x00, 0xFF, 0x04, 0x10, 0x20},
    {48000, 1, 1, 1, 1, 0, 0x00, 0xFF, 0x04, 0x10, 0x10},
};

static const char *es8311_mic_gain_name(es8311_mic_gain_t gain)
{
    switch (gain) {
    case ES8311_MIC_GAIN_0DB: return "0dB";
    case ES8311_MIC_GAIN_3DB: return "3dB";
    case ES8311_MIC_GAIN_6DB: return "6dB";
    case ES8311_MIC_GAIN_9DB: return "9dB";
    case ES8311_MIC_GAIN_12DB: return "12dB";
    case ES8311_MIC_GAIN_15DB: return "15dB";
    case ES8311_MIC_GAIN_18DB: return "18dB";
    case ES8311_MIC_GAIN_21DB: return "21dB";
    case ES8311_MIC_GAIN_24DB: return "24dB";
    case ES8311_MIC_GAIN_27DB: return "27dB";
    case ES8311_MIC_GAIN_30DB: return "30dB";
    default: return "invalid";
    }
}

static float es8311_volume_db(uint8_t volume)
{
    if (volume == 0) {
        return -95.5f;
    }
    return -95.5f + 5.0f + ((float)(volume - 1) * 0.5f);
}


static esp_err_t es8311_first_error(esp_err_t current, esp_err_t next)
{
    return current == ESP_OK ? next : current;
}

static const es8311_clock_config_t *es8311_get_clock_config(int sample_rate)
{
    for (size_t i = 0; i < sizeof(ES8311_CLOCK_CONFIGS) / sizeof(ES8311_CLOCK_CONFIGS[0]); ++i) {
        if (ES8311_CLOCK_CONFIGS[i].sample_rate == sample_rate) {
            return &ES8311_CLOCK_CONFIGS[i];
        }
    }
    return NULL;
}

// Write a single 8-bit value to a codec register.
static esp_err_t es8311_write_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(ES8311_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C write reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

// Read a single 8-bit value from a codec register.
static esp_err_t es8311_read_reg(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t reg, uint8_t *val)
{
    if (val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(ES8311_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "I2C read reg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t es8311_write_reg_checked(i2c_port_t i2c_num, uint8_t i2c_addr,
                                          uint8_t reg, uint8_t val, bool verify)
{
    esp_err_t err = es8311_write_reg(i2c_num, i2c_addr, reg, val);
    if (err != ESP_OK || !verify || !ES8311_VERIFY_REGISTERS) {
        return err;
    }

    uint8_t readback = 0;
    err = es8311_read_reg(i2c_num, i2c_addr, reg, &readback);
    if (err != ESP_OK) {
        return err;
    }

    if (readback != val) {
        ESP_LOGE(TAG_CODEC, "Register verify mismatch reg 0x%02x: wrote 0x%02x read 0x%02x",
                 reg, val, readback);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t es8311_update_reg_bits(i2c_port_t i2c_num, uint8_t i2c_addr,
                                        uint8_t reg, uint8_t mask, uint8_t val,
                                        bool verify)
{
    uint8_t current = 0;
    esp_err_t err = es8311_read_reg(i2c_num, i2c_addr, reg, &current);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t next = (current & ~mask) | (val & mask);
    return es8311_write_reg_checked(i2c_num, i2c_addr, reg, next, verify);
}

static esp_err_t es8311_configure_clock(i2c_port_t i2c_num, uint8_t i2c_addr, int sample_rate)
{
    const es8311_clock_config_t *clock = es8311_get_clock_config(sample_rate);
    if (clock == NULL) {
        ESP_LOGE(TAG_CODEC, "Unsupported ES8311 sample rate %d Hz for %dx MCLK", sample_rate, ES8311_MCLK_MULTIPLE);
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint8_t reg02 = ((clock->pre_div - 1) << 5) | (clock->pre_mult << 3);
    uint8_t reg03 = (clock->fs_mode << 6) | clock->adc_osr;
    uint8_t reg05 = ((clock->adc_div - 1) << 4) | (clock->dac_div - 1);
    uint8_t reg06 = (clock->bclk_div < 19) ? (clock->bclk_div - 1) : clock->bclk_div;

    esp_err_t ret = ESP_OK;
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_1, ES8311_CLKMGR1_MCLK_PIN_ENABLE, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_2, reg02, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_3, reg03, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_4, clock->dac_osr, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_5, reg05, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_6, reg06, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_7, clock->lrck_h, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_CLKMGR_8, clock->lrck_l, true));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "Clock configured: sample_rate=%d Hz, mclk=%d Hz", sample_rate,
                 sample_rate * ES8311_MCLK_MULTIPLE);
    }
    return ret;
}

/*
 * The sequence below follows the ES8311 user guide/datasheet register map:
 * reset, MCLK/LRCK/BCLK dividers, I2S 16-bit serial ports, analog Mic1 PGA,
 * ADC/DAC volume, mute controls, equalizer bypass and power-up registers.
 * It is for the Stick S3 schematic where the ESP32-S3 drives MCLK/BCLK/LRCK
 * and the ES8311 is at I2C address 0x18 with Mic1 and differential DAC output.
 */
esp_err_t es8311_init(i2c_port_t i2c_num, uint8_t i2c_addr, i2s_port_t i2s_port, int sample_rate)
{
    (void)i2s_port; // The codec is configured over I2C; I2S is configured by the caller.
    ESP_LOGI(TAG_CODEC, "Initialising ES8311 at I2C 0x%02x (sample_rate=%d)", i2c_addr, sample_rate);

    esp_err_t ret = es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_ASSERT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed during reset assert");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_RELEASE, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed during reset release");
        return ret;
    }

#define ES8311_INIT_CHECK(step, expr) do { \
        ret = (expr); \
        if (ret != ESP_OK) { \
            ESP_LOGE(TAG_CODEC, "ES8311 initialisation failed at %s: %s", step, esp_err_to_name(ret)); \
            return ret; \
        } \
    } while (0)

    ES8311_INIT_CHECK("clock configuration", es8311_configure_clock(i2c_num, i2c_addr, sample_rate));
    ES8311_INIT_CHECK("DAC serial format", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_SDP_DAC, ES8311_I2S_16BIT, true));
    ES8311_INIT_CHECK("ADC serial format", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_SDP_ADC, ES8311_I2S_16BIT, true));
    ES8311_INIT_CHECK("microphone gain", es8311_set_mic_gain(i2c_num, i2c_addr, ES8311_MIC_GAIN_30DB));
    ES8311_INIT_CHECK("ADC RAM clear", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_ADC_RAM, ES8311_ADC_RAM_CLEAR, true));
    ES8311_INIT_CHECK("ADC volume", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_ADC_VOLUME, ES8311_ADC_VOLUME_0DB, true));
    ES8311_INIT_CHECK("DAC volume", es8311_set_dac_volume(i2c_num, i2c_addr, ES8311_DAC_VOLUME_0DB));
    ES8311_INIT_CHECK("DAC unmute", es8311_mute(i2c_num, i2c_addr, false));
    ES8311_INIT_CHECK("analog power", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_1, ES8311_POWER1_UP, true));
    ES8311_INIT_CHECK("ADC/PGA power", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_2, ES8311_POWER2_ADC_PGA_UP, true));
    ES8311_INIT_CHECK("DAC power", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_UP, true));
    ES8311_INIT_CHECK("output enable", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_OUTPUT, ES8311_OUTPUT_HP_ENABLE, true));
    ES8311_INIT_CHECK("ADC EQ/HPF", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_ADC_EQ, ES8311_ADC_EQ_BYPASS_HPF_ON, true));
    ES8311_INIT_CHECK("DAC EQ", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_EQ, ES8311_DAC_EQ_BYPASS, true));
    ES8311_INIT_CHECK("codec state machine", es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_CODEC_ON, true));

#undef ES8311_INIT_CHECK

    ESP_LOGI(TAG_CODEC, "ES8311 initialisation succeeded: mic_gain=%s, adc_volume=0x%02x (%.1fdB), dac_volume=0x%02x (%.1fdB)",
             es8311_mic_gain_name(ES8311_MIC_GAIN_30DB), ES8311_ADC_VOLUME_0DB,
             es8311_volume_db(ES8311_ADC_VOLUME_0DB), ES8311_DAC_VOLUME_0DB,
             es8311_volume_db(ES8311_DAC_VOLUME_0DB));
    return ESP_OK;
}

esp_err_t es8311_set_mic_gain(i2c_port_t i2c_num, uint8_t i2c_addr, es8311_mic_gain_t gain)
{
    if (gain < ES8311_MIC_GAIN_0DB || gain > ES8311_MIC_GAIN_30DB) {
        ESP_LOGE(TAG_CODEC, "Invalid microphone PGA gain value: %d", gain);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg14 = ES8311_MIC_SELECT_MIC1 | ((uint8_t)gain & 0x0F);
    esp_err_t ret = es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_MIC_GAIN, reg14, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "Microphone PGA gain set to %s (reg 0x%02x=0x%02x)",
                 es8311_mic_gain_name(gain), ES8311_REG_MIC_GAIN, reg14);
    }
    return ret;
}

esp_err_t es8311_set_dac_volume(i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t volume)
{
    esp_err_t ret = es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_VOLUME, volume, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "DAC volume set to 0x%02x (%.1fdB)", volume, es8311_volume_db(volume));
    }
    return ret;
}

esp_err_t es8311_mute(i2c_port_t i2c_num, uint8_t i2c_addr, bool mute)
{
    esp_err_t ret = es8311_update_reg_bits(i2c_num, i2c_addr, ES8311_REG_DAC_MUTE,
                                           ES8311_DAC_MUTE_BITS,
                                           mute ? ES8311_DAC_MUTE_BITS : 0x00,
                                           true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "DAC output %s", mute ? "muted" : "unmuted");
    }
    return ret;
}

esp_err_t es8311_power_down(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    esp_err_t ret = es8311_mute(i2c_num, i2c_addr, true);
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_2, ES8311_POWER2_ADC_PGA_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_1, ES8311_POWER1_DOWN, true));
    ret = es8311_first_error(ret, es8311_write_reg(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_ASSERT));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered down");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power down failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t es8311_power_up(i2c_port_t i2c_num, uint8_t i2c_addr)
{
    esp_err_t ret = es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_RELEASE, true);
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_1, ES8311_POWER1_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_POWER_2, ES8311_POWER2_ADC_PGA_UP, true));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_DAC_POWER, ES8311_DAC_POWER_UP, true));
    ret = es8311_first_error(ret, es8311_mute(i2c_num, i2c_addr, false));
    ret = es8311_first_error(ret, es8311_write_reg_checked(i2c_num, i2c_addr, ES8311_REG_RESET, ES8311_RESET_CODEC_ON, true));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_CODEC, "ES8311 powered up");
    } else {
        ESP_LOGE(TAG_CODEC, "ES8311 power up failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
