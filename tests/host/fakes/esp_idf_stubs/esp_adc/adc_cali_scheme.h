#pragma once
#include "esp_adc/adc_cali.h"
#ifndef ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 0
#endif
typedef struct { int unit_id; int atten; int bitwidth; } adc_cali_curve_fitting_config_t;
static inline esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *cfg, adc_cali_handle_t *out) { (void)cfg; (void)out; return ESP_ERR_NOT_SUPPORTED; }
