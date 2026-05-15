#include "board_audio.h"
#include "board_i2s.h"
#include "es8311.h"
#include "board_sticks3.h"

esp_err_t board_audio_init(const board_audio_config_t *config) { (void)config; return ESP_OK; }
esp_err_t board_audio_deinit(void) { return ESP_OK; }
esp_err_t board_i2s_read_mono_i16(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms) { (void)timeout_ms; if (dest && max_samples) { dest[0] = 0; } if (samples_read) { *samples_read = max_samples ? 1 : 0; } return ESP_OK; }
esp_err_t board_i2s_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms) { (void)src; (void)timeout_ms; if (bytes_written) { *bytes_written = size; } return ESP_OK; }
esp_err_t es8311_set_dac_volume(i2c_port_t port, uint8_t addr, uint8_t volume) { (void)port; (void)addr; (void)volume; return ESP_OK; }
esp_err_t es8311_mute(i2c_port_t port, uint8_t addr, bool mute) { (void)port; (void)addr; (void)mute; return ESP_OK; }
