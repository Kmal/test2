#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_WIFI_MAX_SCAN_RESULTS 12u

typedef enum {
    APP_WIFI_MODE_OFF = 0,
    APP_WIFI_MODE_STA,
    APP_WIFI_MODE_AP,
    APP_WIFI_MODE_APSTA,
} app_wifi_mode_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;
    uint8_t channel;
    bool secure;
} app_wifi_scan_result_t;

typedef struct {
    char sta_ssid[33];
    char sta_password[65];
    bool sta_persist;
    char ap_ssid[33];
    char ap_password[65];
    uint8_t ap_channel;
    uint8_t ap_max_connections;
    app_wifi_mode_t preferred_mode;
} app_wifi_config_t;

typedef struct {
    bool enabled;
    app_wifi_mode_t active_mode;
    bool sta_connected;
    char sta_ssid[33];
    char sta_ip[16];
    bool ap_started;
    char ap_ssid[33];
    char ap_ip[16];
    uint8_t ap_channel;
    uint8_t ap_max_connections;
    char web_url[32];
} app_wifi_status_t;

typedef struct {
    app_wifi_scan_result_t items[APP_WIFI_MAX_SCAN_RESULTS];
    size_t count;
    bool ok;
    char error[32];
} app_wifi_scan_results_t;

bool app_wifi_start(void);
bool app_wifi_get_config(app_wifi_config_t *out);
bool app_wifi_set_config(const app_wifi_config_t *config, bool persist);
bool app_wifi_get_status(app_wifi_status_t *out);
bool app_wifi_set_mode(app_wifi_mode_t mode);
bool app_wifi_connect(const char *ssid, const char *password, bool persist);
bool app_wifi_last_connect_failed_due_to_password(void);
bool app_wifi_scan(app_wifi_scan_results_t *out);
bool app_wifi_start_ap_configured(const char *ap_ssid, const char *ap_password, uint8_t channel, bool persist);
bool app_wifi_forget_sta_credentials(void);
bool app_wifi_scan_json(char *out, size_t out_len);
bool app_wifi_status_json(char *out, size_t out_len);
bool app_wifi_is_sta_connected(void);
const char *app_wifi_mode_name(app_wifi_mode_t mode);
