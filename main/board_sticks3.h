/*
 * Board constants for the M5Stack Stick S3.
 *
 * Verified against the official M5Stack StickS3 schematic/pin map:
 *   MCLK=GPIO18, DAC data/DDAC=GPIO14, BCLK=GPIO17, LRCK=GPIO15,
 *   ADC data/DADC=GPIO16, I2C SCL=GPIO48, I2C SDA=GPIO47.
 *
 * Status buttons are active-low GPIO inputs with internal pull-ups.
 *
 * Note that this differs from an earlier working assumption that used
 * BCLK=GPIO47, LRCLK=GPIO0, DAC data=GPIO2, ADC data=GPIO1,
 * MCLK=GPIO48, I2C SDA=GPIO8, and I2C SCL=GPIO18.
 */

#pragma once

#include "driver/i2c.h"
#include "driver/i2s.h"
#include "driver/gpio.h"

#define BOARD_I2S_PORT         I2S_NUM_0
#define BOARD_I2S_SAMPLE_RATE  16000
#define BOARD_I2S_BITS         I2S_BITS_PER_SAMPLE_16BIT
#define BOARD_I2S_CHANNEL_FMT  I2S_CHANNEL_FMT_ONLY_LEFT

#define BOARD_I2S_MCLK_HZ      12288000
#define BOARD_I2S_BCK_IO       17
#define BOARD_I2S_WS_IO        15
#define BOARD_I2S_DO_IO        14
#define BOARD_I2S_DI_IO        16
#define BOARD_I2S_MCLK_IO      18

#define BOARD_I2C_PORT         I2C_NUM_0
#define BOARD_I2C_SDA_IO       47
#define BOARD_I2C_SCL_IO       48
#define BOARD_ES8311_ADDR      0x18

#define BOARD_PCM_CHUNK_SIZE   320

#define BOARD_BUTTON_CLEAR_PAIRING_GPIO       GPIO_NUM_37
#define BOARD_BUTTON_TOGGLE_MONITORING_GPIO   GPIO_NUM_39
#define BOARD_BUTTON_TOGGLE_DISCOVERABLE_GPIO GPIO_NUM_35
#define BOARD_BUTTON_ACTIVE_LEVEL             0
