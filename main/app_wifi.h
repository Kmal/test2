#pragma once

#include <stdbool.h>
#include <stddef.h>

bool app_wifi_start(void);
bool app_wifi_start_ap(void);
bool app_wifi_connect(const char *ssid, const char *password, bool persist);
bool app_wifi_scan_json(char *out, size_t out_len);
bool app_wifi_status_json(char *out, size_t out_len);
bool app_wifi_is_sta_connected(void);
