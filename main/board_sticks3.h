/*
 * Board constants for the M5Stack StickS3.
 *
 * Verified against the official M5Stack StickS3 documentation, schematic, and
 * pin map. The StickS3 controller is ESP32-S3-PICO-1-N8R8, which does not
 * support Bluetooth Classic / BR/EDR; Classic Bluetooth HFP code must not be
 * enabled for this board.
 *
 * Audio codec pins (the default Bluetooth LE GATT PCM runtime uses the ADC/RX
 * path only; GPIO14 is physically available for DAC data but is not driven
 * until a full-duplex profile is selected):
 *
 *   ES8311 I2C address=0x18, MCLK=GPIO18, DAC data/DDAC=GPIO14,
 *   BCLK=GPIO17, LRCK=GPIO15, ADC data/DADC=GPIO16,
 *   I2C SCL=GPIO48, I2C SDA=GPIO47.
 *
 * Other documented I2C devices on the shared bus:
 *   BMI270=0x68, M5PM1=0x6e.
 *
 * User keys are active-low GPIO inputs with internal pull-ups:
 *   KEY1=GPIO11, KEY2=GPIO12. GPIO35, GPIO37, and GPIO39 are not status
 *   buttons; GPIO39 is LCD MOSI in the official pin map.
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
#define BOARD_I2S_BCLK_HZ      512000
#define BOARD_I2S_LRCK_HZ      BOARD_I2S_SAMPLE_RATE
#define BOARD_I2S_BCK_IO       17
#define BOARD_I2S_WS_IO        15
#define BOARD_I2S_DO_IO        14
#define BOARD_I2S_DI_IO        16
#define BOARD_I2S_MCLK_IO      18

#define BOARD_I2C_PORT         I2C_NUM_0
#define BOARD_I2C_SDA_IO       47
#define BOARD_I2C_SCL_IO       48
#define BOARD_I2C_CLK_HZ       400000
#define BOARD_ES8311_ADDR      0x18
#define BOARD_BMI270_ADDR      0x68
#define BOARD_M5PM1_ADDR       0x6e

#define BOARD_PCM_CHUNK_SIZE   320

#define BOARD_BUTTON_KEY1_GPIO     GPIO_NUM_11
#define BOARD_BUTTON_KEY2_GPIO     GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LEVEL  0
