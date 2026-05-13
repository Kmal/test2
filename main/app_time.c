#define _POSIX_C_SOURCE 200809L
#include "app_time.h"
#include "sdkconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CONFIG_APP_TIMEZONE
#define CONFIG_APP_TIMEZONE "UTC"
#endif

#ifdef ESP_PLATFORM
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define APP_TIME_NVS_NAMESPACE "app_time"
#define APP_TIME_NVS_TIMEZONE "timezone"
static const char *TAG = "APP_TIME";
#endif

static app_time_config_t s_config;
static bool s_config_loaded;

static size_t bounded_strlen(const char *value, size_t max_len)
{
    size_t len = 0u;
    if (value == NULL) {
        return 0u;
    }
    while (len < max_len && value[len] != '\0') {
        ++len;
    }
    return len;
}

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0u) {
        return;
    }
    const char *value = src != NULL ? src : "";
    size_t len = bounded_strlen(value, dst_size - 1u);
    memcpy(dst, value, len);
    dst[len] = '\0';
}

static bool timezone_is_valid(const char *timezone)
{
    const size_t len = bounded_strlen(timezone, APP_TIME_TIMEZONE_MAX_LEN + 1u);
    if (len == 0u || len > APP_TIME_TIMEZONE_MAX_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        const unsigned char ch = (unsigned char)timezone[i];
        if (ch < 0x21u || ch > 0x7eu || ch == '"' || ch == '\'' || ch == '\\') {
            return false;
        }
    }
    return true;
}

static const char *timezone_to_env(const char *timezone)
{
    return (timezone != NULL && strcmp(timezone, "UTC") == 0) ? "UTC0" : timezone;
}

static void config_defaults(app_time_config_t *config)
{
    memset(config, 0, sizeof(*config));
    copy_string(config->timezone, sizeof(config->timezone), CONFIG_APP_TIMEZONE);
    if (!timezone_is_valid(config->timezone)) {
        copy_string(config->timezone, sizeof(config->timezone), "UTC");
    }
}

static void apply_timezone(void)
{
    const char *tz = timezone_to_env(s_config.timezone[0] != '\0' ? s_config.timezone : "UTC");
    (void)setenv("TZ", tz, 1);
    tzset();
}

static void load_config_once(void)
{
    if (s_config_loaded) {
        return;
    }
    config_defaults(&s_config);
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_TIME_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        char timezone[APP_TIME_TIMEZONE_MAX_LEN + 1u];
        size_t required = sizeof(timezone);
        if (nvs_get_str(handle, APP_TIME_NVS_TIMEZONE, timezone, &required) == ESP_OK && timezone_is_valid(timezone)) {
            copy_string(s_config.timezone, sizeof(s_config.timezone), timezone);
        }
        nvs_close(handle);
    } else {
        ESP_LOGI(TAG, "no saved time config: %s", esp_err_to_name(err));
    }
#endif
    s_config_loaded = true;
    apply_timezone();
}

static bool persist_config(void)
{
#ifdef ESP_PLATFORM
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_TIME_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open time NVS failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(handle, APP_TIME_NVS_TIMEZONE, s_config.timezone);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save time config failed: %s", esp_err_to_name(err));
        return false;
    }
#endif
    return true;
}

bool app_time_init(void)
{
    load_config_once();
    return true;
}

bool app_time_get_config(app_time_config_t *out)
{
    if (out == NULL) {
        return false;
    }
    load_config_once();
    *out = s_config;
    return true;
}

bool app_time_set_timezone(const char *timezone, bool persist)
{
    if (!timezone_is_valid(timezone)) {
        return false;
    }
    load_config_once();
    copy_string(s_config.timezone, sizeof(s_config.timezone), timezone);
    apply_timezone();
    return !persist || persist_config();
}

bool app_time_format_hhmm_24h(char out[6])
{
    if (out == NULL) {
        return false;
    }
    load_config_once();
    time_t now = time(NULL);
    struct tm local;
    if (now > 0 && localtime_r(&now, &local) != NULL && local.tm_year >= 120) {
        snprintf(out, 6, "%02d:%02d", local.tm_hour, local.tm_min);
        return true;
    }
    snprintf(out, 6, "--:--");
    return false;
}

static bool append_json_string(char **cursor, size_t *remaining, const char *value)
{
    int written = snprintf(*cursor, *remaining, "\"");
    if (written < 0 || (size_t)written >= *remaining) return false;
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
        if (written < 0 || (size_t)written >= *remaining) return false;
        *cursor += written;
        *remaining -= (size_t)written;
    }
    written = snprintf(*cursor, *remaining, "\"");
    if (written < 0 || (size_t)written >= *remaining) return false;
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

bool app_time_config_json(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0u) {
        return false;
    }
    load_config_once();
    char hhmm[6];
    bool valid = app_time_format_hhmm_24h(hhmm);
    char *cursor = out;
    size_t remaining = out_len;
    int written = snprintf(cursor, remaining, "{\"timezone\":");
    if (written < 0 || (size_t)written >= remaining) return false;
    cursor += written;
    remaining -= (size_t)written;
    if (!append_json_string(&cursor, &remaining, s_config.timezone)) return false;
    written = snprintf(cursor, remaining, ",\"time_24h\":\"%s\",\"time_valid\":%s}", hhmm, valid ? "true" : "false");
    return written > 0 && (size_t)written < remaining;
}
