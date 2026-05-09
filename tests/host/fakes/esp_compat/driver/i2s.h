#pragma once
typedef int i2s_port_t;
typedef int i2s_mode_t;
#define I2S_NUM_0 0
#define I2S_MODE_MASTER 0x1
#define I2S_MODE_TX 0x2
#define I2S_MODE_RX 0x4
#define I2S_BITS_PER_SAMPLE_16BIT 16
#define I2S_CHANNEL_FMT_ONLY_LEFT 1
#define I2S_COMM_FORMAT_STAND_I2S 1
#define I2S_MCLK_MULTIPLE_256 256
#define I2S_PIN_NO_CHANGE (-1)
typedef struct { i2s_mode_t mode; int sample_rate; int bits_per_sample; int channel_format; int communication_format; int intr_alloc_flags; int dma_buf_count; int dma_buf_len; int use_apll; int tx_desc_auto_clear; int fixed_mclk; int mclk_multiple; } i2s_config_t;
typedef struct { int bck_io_num; int ws_io_num; int data_out_num; int data_in_num; int mck_io_num; } i2s_pin_config_t;
