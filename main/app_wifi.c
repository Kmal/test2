#include "app_wifi.h"
#include "action_http.h"
#include "sdkconfig.h"
#include "status_ui.h"

#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#ifndef CONFIG_APP_WIFI_ENABLE
#define CONFIG_APP_WIFI_ENABLE 1
#endif
#ifndef CONFIG_APP_WIFI_AP_SSID
#define CONFIG_APP_WIFI_AP_SSID "M5StickS3-Setup"
#endif
#ifndef CONFIG_APP_WIFI_AP_PASSWORD
#define CONFIG_APP_WIFI_AP_PASSWORD ""
#endif
#ifndef CONFIG_APP_WIFI_AP_CHANNEL
#define CONFIG_APP_WIFI_AP_CHANNEL 6
#endif
#ifndef CONFIG_APP_WIFI_AP_MAX_CONNECTIONS
#define CONFIG_APP_WIFI_AP_MAX_CONNECTIONS 4
#endif
#ifndef CONFIG_APP_WIFI_CONNECT_TIMEOUT_MS
#define CONFIG_APP_WIFI_CONNECT_TIMEOUT_MS 8000
#endif
#ifndef CONFIG_APP_WIFI_MAX_SCAN_RESULTS
#define CONFIG_APP_WIFI_MAX_SCAN_RESULTS 12
#endif
#ifndef CONFIG_APP_WIFI_KEYBOARD_PROVISIONING
#define CONFIG_APP_WIFI_KEYBOARD_PROVISIONING 0
#endif
#ifndef CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#define CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS 60000
#endif

#define APP_WIFI_NVS_NAMESPACE "app_wifi"
#define APP_WIFI_NVS_SSID "ssid"
#define APP_WIFI_NVS_PASSWORD "password"
#define APP_WIFI_IP_LEN 16u

static const char *TAG = "APP_WIFI";

static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_ap_started;
static bool s_sta_connected;
static char s_sta_ssid[33];
static char s_sta_ip[APP_WIFI_IP_LEN] = "0.0.0.0";
static char s_ap_ip[APP_WIFI_IP_LEN] = "0.0.0.0";

static void copy_ip(char out[APP_WIFI_IP_LEN], esp_ip4_addr_t ip)
{
    (void)snprintf(out, APP_WIFI_IP_LEN, IPSTR, IP2STR(&ip));
}

static bool read_saved_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    if (ssid == NULL || password == NULL || ssid_len == 0 || password_len == 0) {
        return false;
    }
    ssid[0] = '\0';
    password[0] = '\0';
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "no saved Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }
    size_t required = ssid_len;
    err = nvs_get_str(handle, APP_WIFI_NVS_SSID, ssid, &required);
    if (err == ESP_OK) {
        required = password_len;
        esp_err_t pass_err = nvs_get_str(handle, APP_WIFI_NVS_PASSWORD, password, &required);
        if (pass_err == ESP_ERR_NVS_NOT_FOUND) {
            password[0] = '\0';
        } else if (pass_err != ESP_OK) {
            ESP_LOGW(TAG, "saved Wi-Fi password read failed: %s", esp_err_to_name(pass_err));
        }
    }
    nvs_close(handle);
    if (err != ESP_OK || ssid[0] == '\0') {
        ESP_LOGI(TAG, "saved Wi-Fi SSID unavailable: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open Wi-Fi NVS failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(handle, APP_WIFI_NVS_SSID, ssid != NULL ? ssid : "");
    if (err == ESP_OK) {
        err = nvs_set_str(handle, APP_WIFI_NVS_PASSWORD, password != NULL ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save Wi-Fi credentials failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_sta_connected = false;
            s_sta_ip[0] = '0';
            s_sta_ip[1] = '.';
            s_sta_ip[2] = '0';
            s_sta_ip[3] = '.';
            s_sta_ip[4] = '0';
            s_sta_ip[5] = '.';
            s_sta_ip[6] = '0';
            s_sta_ip[7] = '\0';
            action_http_set_network_ready(false);
            ESP_LOGW(TAG, "station disconnected");
        } else if (id == WIFI_EVENT_AP_START) {
            s_ap_started = true;
            esp_netif_ip_info_t ip_info;
            if (s_ap_netif != NULL && esp_netif_get_ip_info(s_ap_netif, &ip_info) == ESP_OK) {
                copy_ip(s_ap_ip, ip_info.ip);
            }
            ESP_LOGI(TAG, "setup AP started: ssid=%s ip=%s", CONFIG_APP_WIFI_AP_SSID, s_ap_ip);
        } else if (id == WIFI_EVENT_AP_STOP) {
            s_ap_started = false;
            (void)snprintf(s_ap_ip, sizeof(s_ap_ip), "0.0.0.0");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        s_sta_connected = true;
        copy_ip(s_sta_ip, event->ip_info.ip);
        action_http_set_network_ready(true);
        ESP_LOGI(TAG, "station connected: ssid=%s ip=%s", s_sta_ssid, s_sta_ip);
    }
}

static bool app_wifi_init_once(void)
{
    if (s_wifi_initialized) {
        return true;
    }
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_sta_netif == NULL || s_ap_netif == NULL) {
        ESP_LOGE(TAG, "failed to create default Wi-Fi netifs");
        return false;
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    s_wifi_initialized = true;
    return true;
}

static bool ensure_wifi_started(void)
{
    if (!s_wifi_initialized && !app_wifi_init_once()) {
        return false;
    }
    if (s_wifi_started) {
        return true;
    }
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
    }
    s_wifi_started = true;
    return true;
}

static bool set_mode(wifi_mode_t mode)
{
    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode(%d) failed: %s", (int)mode, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool app_wifi_start_ap(void)
{
#if !CONFIG_APP_WIFI_ENABLE
    return false;
#else
    if (!app_wifi_init_once()) {
        return false;
    }
    wifi_mode_t mode = s_sta_connected ? WIFI_MODE_APSTA : WIFI_MODE_AP;
    if (!set_mode(mode)) {
        return false;
    }
    wifi_config_t ap_config;
    memset(&ap_config, 0, sizeof(ap_config));
    (void)snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), "%s", CONFIG_APP_WIFI_AP_SSID);
    ap_config.ap.ssid_len = strlen(CONFIG_APP_WIFI_AP_SSID);
    (void)snprintf((char *)ap_config.ap.password, sizeof(ap_config.ap.password), "%s", CONFIG_APP_WIFI_AP_PASSWORD);
    ap_config.ap.channel = CONFIG_APP_WIFI_AP_CHANNEL;
    ap_config.ap.max_connection = CONFIG_APP_WIFI_AP_MAX_CONNECTIONS;
    ap_config.ap.authmode = strlen(CONFIG_APP_WIFI_AP_PASSWORD) >= 8u ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    if (ap_config.ap.authmode == WIFI_AUTH_OPEN) {
        ap_config.ap.password[0] = '\0';
    }
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set AP config failed: %s", esp_err_to_name(err));
        return false;
    }
    if (!ensure_wifi_started()) {
        return false;
    }
    esp_netif_ip_info_t ip_info;
    if (s_ap_netif != NULL && esp_netif_get_ip_info(s_ap_netif, &ip_info) == ESP_OK) {
        copy_ip(s_ap_ip, ip_info.ip);
    }
    s_ap_started = true;
    ESP_LOGI(TAG, "setup AP ready: ssid=%s password=%s url=http://%s/",
             CONFIG_APP_WIFI_AP_SSID,
             strlen(CONFIG_APP_WIFI_AP_PASSWORD) >= 8u ? CONFIG_APP_WIFI_AP_PASSWORD : "<open>",
             s_ap_ip);
    return true;
#endif
}

bool app_wifi_connect(const char *ssid, const char *password, bool persist)
{
#if !CONFIG_APP_WIFI_ENABLE
    (void)ssid;
    (void)password;
    (void)persist;
    return false;
#else
    if (ssid == NULL || ssid[0] == '\0' || strlen(ssid) > 32u) {
        ESP_LOGW(TAG, "invalid SSID");
        return false;
    }
    if (password != NULL && strlen(password) > 63u) {
        ESP_LOGW(TAG, "invalid password length");
        return false;
    }
    if (!app_wifi_init_once()) {
        return false;
    }
    if (!set_mode(s_ap_started ? WIFI_MODE_APSTA : WIFI_MODE_STA)) {
        return false;
    }
    wifi_config_t sta_config;
    memset(&sta_config, 0, sizeof(sta_config));
    (void)snprintf((char *)sta_config.sta.ssid, sizeof(sta_config.sta.ssid), "%s", ssid);
    (void)snprintf((char *)sta_config.sta.password, sizeof(sta_config.sta.password), "%s", password != NULL ? password : "");
    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set STA config failed: %s", esp_err_to_name(err));
        return false;
    }
    if (!ensure_wifi_started()) {
        return false;
    }
    (void)snprintf(s_sta_ssid, sizeof(s_sta_ssid), "%s", ssid);
    s_sta_connected = false;
    action_http_set_network_ready(false);
    (void)esp_wifi_disconnect();
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "connect start failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "connecting to Wi-Fi SSID %s", ssid);
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CONFIG_APP_WIFI_CONNECT_TIMEOUT_MS);
    while (!s_sta_connected && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!s_sta_connected) {
        ESP_LOGW(TAG, "connection timed out for SSID %s", ssid);
        return false;
    }
    if (persist && !save_credentials(ssid, password != NULL ? password : "")) {
        ESP_LOGW(TAG, "connected but failed to persist Wi-Fi credentials");
    }
    ESP_LOGI(TAG, "web UI available on station network: http://%s/", s_sta_ip);
    return true;
#endif
}


static bool prompt_credentials_and_connect(void)
{
#if CONFIG_APP_WIFI_KEYBOARD_PROVISIONING
    char ssid[33];
    char password[65];
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    ESP_LOGI(TAG, "starting LCD virtual keyboard Wi-Fi provisioning");
    if (!status_ui_keyboard_read_line("WiFi SSID", "", ssid, sizeof(ssid), 32,
                                      false, CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS) || ssid[0] == '\0') {
        ESP_LOGI(TAG, "LCD Wi-Fi SSID entry cancelled or timed out");
        return false;
    }
    if (!status_ui_keyboard_read_line("WiFi PASSWORD", "", password, sizeof(password), 63,
                                      true, CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS)) {
        ESP_LOGI(TAG, "LCD Wi-Fi password entry cancelled or timed out");
        return false;
    }
    return app_wifi_connect(ssid, password, true);
#else
    return false;
#endif
}

bool app_wifi_start(void)
{
#if !CONFIG_APP_WIFI_ENABLE
    ESP_LOGI(TAG, "Wi-Fi manager disabled");
    action_http_set_network_ready(false);
    return false;
#else
    char ssid[33];
    char password[65];
    if (!app_wifi_init_once()) {
        return false;
    }
    if (read_saved_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "trying saved Wi-Fi SSID %s", ssid);
        if (app_wifi_connect(ssid, password, false)) {
            return true;
        }
    }
    if (prompt_credentials_and_connect()) {
        return true;
    }
    ESP_LOGI(TAG, "starting setup AP for Wi-Fi selection and web UI");
    return app_wifi_start_ap();
#endif
}

static bool append_json_string(char **cursor, size_t *remaining, const char *value)
{
    int written = snprintf(*cursor, *remaining, "\"");
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }
    *cursor += written;
    *remaining -= (size_t)written;
    for (const unsigned char *ch = (const unsigned char *)(value != NULL ? value : ""); *ch != '\0'; ++ch) {
        const char *escape = NULL;
        char escape_buf[7];
        switch (*ch) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default:
            if (*ch < 0x20u) {
                (void)snprintf(escape_buf, sizeof(escape_buf), "\\u%04x", (unsigned)*ch);
                escape = escape_buf;
            }
            break;
        }
        written = snprintf(*cursor, *remaining, "%s", escape != NULL ? escape : (char[]){(char)*ch, '\0'});
        if (written < 0 || (size_t)written >= *remaining) {
            return false;
        }
        *cursor += written;
        *remaining -= (size_t)written;
    }
    written = snprintf(*cursor, *remaining, "\"");
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

bool app_wifi_scan_json(char *out, size_t out_len)
{
#if !CONFIG_APP_WIFI_ENABLE
    (void)out;
    (void)out_len;
    return false;
#else
    if (out == NULL || out_len == 0 || !app_wifi_init_once()) {
        return false;
    }
    if (!set_mode(s_ap_started ? WIFI_MODE_APSTA : WIFI_MODE_STA) || !ensure_wifi_started()) {
        return false;
    }
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return snprintf(out, out_len, "{\"ok\":false,\"error\":\"scan_failed\"}") > 0;
    }
    uint16_t count = CONFIG_APP_WIFI_MAX_SCAN_RESULTS;
    wifi_ap_record_t records[CONFIG_APP_WIFI_MAX_SCAN_RESULTS];
    memset(records, 0, sizeof(records));
    err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan records failed: %s", esp_err_to_name(err));
        return snprintf(out, out_len, "{\"ok\":false,\"error\":\"scan_records_failed\"}") > 0;
    }
    char *cursor = out;
    size_t remaining = out_len;
    int written = snprintf(cursor, remaining, "{\"ok\":true,\"networks\":[");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    cursor += written;
    remaining -= (size_t)written;
    for (uint16_t i = 0; i < count; ++i) {
        char ssid[33];
        memcpy(ssid, records[i].ssid, 32);
        ssid[32] = '\0';
        written = snprintf(cursor, remaining, "%s{\"ssid\":", i == 0 ? "" : ",");
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        cursor += written;
        remaining -= (size_t)written;
        if (!append_json_string(&cursor, &remaining, ssid)) {
            return false;
        }
        written = snprintf(cursor, remaining, ",\"rssi\":%d,\"auth\":%d,\"channel\":%u}",
                           records[i].rssi, (int)records[i].authmode, (unsigned)records[i].primary);
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        cursor += written;
        remaining -= (size_t)written;
    }
    written = snprintf(cursor, remaining, "]}");
    return written > 0 && (size_t)written < remaining;
#endif
}

bool app_wifi_status_json(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return false;
    }
#if !CONFIG_APP_WIFI_ENABLE
    return snprintf(out, out_len, "{\"enabled\":false}") > 0;
#else
    const char *mode = s_sta_connected ? (s_ap_started ? "apsta" : "sta") : (s_ap_started ? "ap" : "off");
    char *cursor = out;
    size_t remaining = out_len;
    int written = snprintf(cursor, remaining,
                           "{\"enabled\":true,\"mode\":\"%s\",\"sta_connected\":%s,\"sta_ssid\":",
                           mode, s_sta_connected ? "true" : "false");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    cursor += written;
    remaining -= (size_t)written;
    if (!append_json_string(&cursor, &remaining, s_sta_ssid)) {
        return false;
    }
    written = snprintf(cursor, remaining, ",\"sta_ip\":\"%s\",\"ap_started\":%s,\"ap_ssid\":",
                       s_sta_ip, s_ap_started ? "true" : "false");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    cursor += written;
    remaining -= (size_t)written;
    if (!append_json_string(&cursor, &remaining, CONFIG_APP_WIFI_AP_SSID)) {
        return false;
    }
    written = snprintf(cursor, remaining, ",\"ap_ip\":\"%s\",\"web_url\":\"http://%s/\"}",
                       s_ap_ip, s_sta_connected ? s_sta_ip : s_ap_ip);
    return written > 0 && (size_t)written < remaining;
#endif
}

bool app_wifi_is_sta_connected(void)
{
    return s_sta_connected;
}

#else

bool app_wifi_start(void)
{
    return false;
}

bool app_wifi_start_ap(void)
{
    return false;
}

bool app_wifi_connect(const char *ssid, const char *password, bool persist)
{
    (void)ssid;
    (void)password;
    (void)persist;
    return false;
}

bool app_wifi_scan_json(char *out, size_t out_len)
{
    return out != NULL && snprintf(out, out_len, "{\"ok\":false,\"error\":\"wifi_unavailable\"}") > 0;
}

bool app_wifi_status_json(char *out, size_t out_len)
{
    return out != NULL && snprintf(out, out_len, "{\"enabled\":false}") > 0;
}

bool app_wifi_is_sta_connected(void)
{
    return false;
}

#endif
