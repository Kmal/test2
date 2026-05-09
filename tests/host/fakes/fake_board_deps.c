#include "board_i2c.h"
#include "board_i2s.h"
#include "board_audio_power.h"
#include "es8311.h"
#include "m5pm1.h"

esp_err_t board_i2c_init(void) { return ESP_OK; }
esp_err_t board_i2s_init_profile(board_audio_profile_t profile) { (void)profile; return ESP_OK; }
esp_err_t board_audio_power_enable(i2c_port_t port) { (void)port; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t board_speaker_amp_pulse(i2c_port_t port, uint8_t pulse_count) { (void)port; (void)pulse_count; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t es8311_init_profile(i2c_port_t port, uint8_t addr, i2s_port_t i2s_port, es8311_profile_t profile, int sample_rate) { (void)port; (void)addr; (void)i2s_port; (void)profile; (void)sample_rate; return ESP_OK; }
esp_err_t m5pm1_probe(i2c_port_t port, uint8_t addr, m5pm1_identity_t *identity) { (void)port; (void)addr; if (identity) { identity->device_id = 0; identity->device_model = 0; } return ESP_OK; }
