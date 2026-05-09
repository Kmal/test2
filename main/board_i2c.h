#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the single shared StickS3 I2C bus used by ES8311, M5PM1, and BMI270. */
esp_err_t board_i2c_init(void);

#ifdef __cplusplus
}
#endif
