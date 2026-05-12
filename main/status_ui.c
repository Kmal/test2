#include "status_ui.h"

#include "board_sticks3.h"
#include "audio_metrics.h"
#include "app_wifi.h"
#include "ui_nav.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_APP_STATUS_UI_LCD
#include "freertos/queue.h"
#endif

#if CONFIG_APP_STATUS_UI_LCD
#include "board_i2c.h"
#include "m5pm1.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#endif

#define STATUS_UI_DEBOUNCE_MS 50
#define STATUS_UI_POLL_MS 25
#define STATUS_UI_TASK_STACK 2048
#define STATUS_UI_TASK_PRIORITY 5

#if CONFIG_APP_STATUS_UI_LCD
#define STATUS_UI_LCD_TASK_STACK 4096
#define STATUS_UI_LCD_TASK_PRIORITY 4
#ifdef CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#define STATUS_UI_LCD_REFRESH_MS CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#else
#define STATUS_UI_LCD_REFRESH_MS 100
#endif
#ifdef CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#define STATUS_UI_KEYBOARD_TIMEOUT_MS CONFIG_APP_WIFI_KEYBOARD_TIMEOUT_MS
#else
#define STATUS_UI_KEYBOARD_TIMEOUT_MS 0
#endif
#define STATUS_UI_LCD_TEXT_SCALE 2
#define STATUS_UI_LCD_BG 0x0000
#define STATUS_UI_LCD_HEADER_BG 0x001F
#define STATUS_UI_LCD_TEXT 0xFFFF
#define STATUS_UI_LCD_DIM 0x8410
#define STATUS_UI_LCD_OK 0x07E0
#define STATUS_UI_LCD_WARN 0xFFE0
#define STATUS_UI_LCD_ERR 0xF800
#define STATUS_UI_LCD_LINE_HEIGHT 16
#define STATUS_UI_LCD_LEFT_PAD 4
#define STATUS_UI_LCD_TOP_PAD 4
#define STATUS_UI_KEYBOARD_ROWS 4
#define STATUS_UI_KEYBOARD_COLS 10
#define STATUS_UI_KEYBOARD_CONTROLS 5
#define STATUS_UI_KEYBOARD_MAX_TEXT 64
#define STATUS_UI_KEYBOARD_LONG_MS 600
#endif

static const char *TAG = "STATUS_UI";

static status_ui_button_handlers_t s_handlers;
static status_ui_state_t s_state = STATUS_UI_STATE_BOOTING;
static bool s_monitoring_enabled = false;
static bool s_service_enabled = false;
static uint32_t s_key1_press_count = 0;
static uint32_t s_key2_press_count = 0;
static status_ui_sound_meter_snapshot_t s_sound_snapshot;
static app_display_mode_t s_display_mode = APP_DISPLAY_VU;
static ui_nav_state_t s_nav;
static bool s_menu_mode;
#if CONFIG_APP_STATUS_UI_LCD
static app_wifi_scan_results_t s_wifi_scan_results;
static size_t s_wifi_selected_network;
static char s_wifi_selected_ssid[33];
static char s_wifi_password[65];
static app_wifi_config_t s_wifi_draft_config;
#endif
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;


#if CONFIG_APP_STATUS_UI_LCD
typedef enum {
    STATUS_UI_KEYBOARD_EVENT_SELECT = 0,
    STATUS_UI_KEYBOARD_EVENT_NEXT,
    STATUS_UI_KEYBOARD_EVENT_PREV,
} status_ui_keyboard_event_t;

typedef struct {
    bool active;
    bool secret;
    bool caps;
    bool cancelled;
    bool done;
    size_t max_len;
    int selected;
    char title[24];
    char text[STATUS_UI_KEYBOARD_MAX_TEXT + 1];
} status_ui_keyboard_state_t;

static QueueHandle_t s_keyboard_queue;
static status_ui_keyboard_state_t s_keyboard;
#endif

#if CONFIG_APP_STATUS_UI_LCD
static esp_lcd_panel_io_handle_t s_panel_io;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_framebuffer;
#endif

typedef struct {
    gpio_num_t gpio;
    const char *name;
    void (*handler)(void *ctx);
    bool stable_pressed;
    bool last_sample_pressed;
    TickType_t last_change_tick;
#if CONFIG_APP_STATUS_UI_LCD
    TickType_t pressed_since_tick;
    bool long_sent;
#endif
} status_button_t;

static const char *bool_label(bool enabled)
{
    return enabled ? "on" : "off";
}

const char *status_ui_state_name(status_ui_state_t state)
{
    switch (state) {
    case STATUS_UI_STATE_BOOTING:
        return "booting";
    case STATUS_UI_STATE_NO_TRANSPORT:
        return "no transport selected";
    case STATUS_UI_STATE_READY:
        return "ready";
    case STATUS_UI_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void status_ui_set_state(status_ui_state_t state)
{
    bool changed = false;
    portENTER_CRITICAL(&s_state_mux);
    if (s_state != state) {
        s_state = state;
        changed = true;
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (changed) {
        ESP_LOGI(TAG, "status: %s", status_ui_state_name(state));
    }
}

status_ui_state_t status_ui_get_state(void)
{
    status_ui_state_t state;
    portENTER_CRITICAL(&s_state_mux);
    state = s_state;
    portEXIT_CRITICAL(&s_state_mux);
    return state;
}

void status_ui_set_monitoring_enabled(bool enabled)
{
    portENTER_CRITICAL(&s_state_mux);
    s_monitoring_enabled = enabled;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(enabled));
}

bool status_ui_get_monitoring_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_state_mux);
    enabled = s_monitoring_enabled;
    portEXIT_CRITICAL(&s_state_mux);
    return enabled;
}

void status_ui_set_service_enabled(bool enabled)
{
    portENTER_CRITICAL(&s_state_mux);
    s_service_enabled = enabled;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "transport service: %s", bool_label(enabled));
}

bool status_ui_get_service_enabled(void)
{
    bool enabled;
    portENTER_CRITICAL(&s_state_mux);
    enabled = s_service_enabled;
    portEXIT_CRITICAL(&s_state_mux);
    return enabled;
}

void status_ui_set_sound_meter_snapshot(const status_ui_sound_meter_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_mux);
    s_sound_snapshot = *snapshot;
    portEXIT_CRITICAL(&s_state_mux);
}

bool status_ui_get_sound_meter_snapshot(status_ui_sound_meter_snapshot_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_state_mux);
    *out = s_sound_snapshot;
    portEXIT_CRITICAL(&s_state_mux);
    return out->valid;
}

void status_ui_set_display_mode(app_display_mode_t mode)
{
    portENTER_CRITICAL(&s_state_mux);
    s_display_mode = mode;
    s_sound_snapshot.display_mode = (uint8_t)mode;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "display mode: %s", app_display_mode_name(mode));
}

app_display_mode_t status_ui_get_display_mode(void)
{
    app_display_mode_t mode;
    portENTER_CRITICAL(&s_state_mux);
    mode = s_display_mode;
    portEXIT_CRITICAL(&s_state_mux);
    return mode;
}


void status_ui_open_screen(ui_screen_id_t screen)
{
    portENTER_CRITICAL(&s_state_mux);
    ui_nav_enter(&s_nav, screen);
    s_menu_mode = true;
    portEXIT_CRITICAL(&s_state_mux);
}

ui_screen_id_t status_ui_get_screen(void)
{
    ui_screen_id_t screen;
    portENTER_CRITICAL(&s_state_mux);
    screen = s_nav.current;
    portEXIT_CRITICAL(&s_state_mux);
    return screen;
}

#if CONFIG_APP_STATUS_UI_LCD
static bool status_ui_start_wifi_scan(void)
{
    memset(&s_wifi_scan_results, 0, sizeof(s_wifi_scan_results));
    s_wifi_selected_network = 0u;
    bool ok = app_wifi_scan(&s_wifi_scan_results);
    ESP_LOGI(TAG, "Wi-Fi scan from LCD menu: ok=%d count=%u error=%s",
             ok ? 1 : 0, (unsigned)s_wifi_scan_results.count, s_wifi_scan_results.error);
    return ok;
}

static bool status_ui_connect_selected_wifi(void)
{
    if (s_wifi_selected_ssid[0] == '\0' && s_wifi_scan_results.count > 0u &&
        s_wifi_selected_network < s_wifi_scan_results.count) {
        (void)snprintf(s_wifi_selected_ssid, sizeof(s_wifi_selected_ssid), "%s",
                       s_wifi_scan_results.items[s_wifi_selected_network].ssid);
    }
    if (s_wifi_selected_ssid[0] == '\0') {
        app_wifi_config_t config;
        if (app_wifi_get_config(&config) && config.sta_ssid[0] != '\0') {
            (void)snprintf(s_wifi_selected_ssid, sizeof(s_wifi_selected_ssid), "%s", config.sta_ssid);
            (void)snprintf(s_wifi_password, sizeof(s_wifi_password), "%s", config.sta_password);
        }
    }
    if (s_wifi_selected_ssid[0] == '\0') {
        ESP_LOGW(TAG, "LCD Wi-Fi connect requested without SSID");
        return false;
    }
    if (s_wifi_password[0] == '\0') {
        (void)status_ui_keyboard_read_line("WiFi PASSWORD", "", s_wifi_password,
                                           sizeof(s_wifi_password), 63, true,
                                           STATUS_UI_KEYBOARD_TIMEOUT_MS);
    }
    return app_wifi_connect(s_wifi_selected_ssid, s_wifi_password, true);
}

static bool status_ui_start_configured_ap(void)
{
    if (!app_wifi_get_config(&s_wifi_draft_config)) {
        return false;
    }
    if (s_wifi_draft_config.ap_ssid[0] == '\0') {
        (void)snprintf(s_wifi_draft_config.ap_ssid, sizeof(s_wifi_draft_config.ap_ssid), "StickS3-Setup");
    }
    return app_wifi_start_ap_configured(s_wifi_draft_config.ap_ssid,
                                        s_wifi_draft_config.ap_password,
                                        s_wifi_draft_config.ap_channel,
                                        true);
}

static void status_ui_open_keyboard_for_current_field(void)
{
    switch (s_nav.current) {
    case UI_SCREEN_NETWORK_WIFI:
    case UI_SCREEN_NETWORK_WIFI_MANUAL_SSID:
        if (status_ui_keyboard_read_line("WiFi SSID", s_wifi_selected_ssid,
                                         s_wifi_selected_ssid, sizeof(s_wifi_selected_ssid),
                                         32, false, STATUS_UI_KEYBOARD_TIMEOUT_MS)) {
            (void)status_ui_keyboard_read_line("WiFi PASSWORD", "", s_wifi_password,
                                               sizeof(s_wifi_password), 63, true,
                                               STATUS_UI_KEYBOARD_TIMEOUT_MS);
        }
        break;
    case UI_SCREEN_NETWORK_WIFI_SELECT:
    case UI_SCREEN_NETWORK_WIFI_PASSWORD:
        (void)status_ui_keyboard_read_line("WiFi PASSWORD", s_wifi_password,
                                           s_wifi_password, sizeof(s_wifi_password), 63,
                                           true, STATUS_UI_KEYBOARD_TIMEOUT_MS);
        break;
    case UI_SCREEN_NETWORK_AP:
    case UI_SCREEN_NETWORK_AP_NAME:
        if (app_wifi_get_config(&s_wifi_draft_config)) {
            (void)status_ui_keyboard_read_line("AP NAME", s_wifi_draft_config.ap_ssid,
                                               s_wifi_draft_config.ap_ssid,
                                               sizeof(s_wifi_draft_config.ap_ssid), 32,
                                               false, STATUS_UI_KEYBOARD_TIMEOUT_MS);
            (void)app_wifi_set_config(&s_wifi_draft_config, true);
        }
        break;
    case UI_SCREEN_NETWORK_AP_PASSWORD:
        if (app_wifi_get_config(&s_wifi_draft_config)) {
            (void)status_ui_keyboard_read_line("AP PASSWORD", s_wifi_draft_config.ap_password,
                                               s_wifi_draft_config.ap_password,
                                               sizeof(s_wifi_draft_config.ap_password), 63,
                                               true, STATUS_UI_KEYBOARD_TIMEOUT_MS);
            (void)app_wifi_set_config(&s_wifi_draft_config, true);
        }
        break;
    case UI_SCREEN_NETWORK_AP_CHANNEL:
        if (app_wifi_get_config(&s_wifi_draft_config)) {
            char channel[4];
            (void)snprintf(channel, sizeof(channel), "%u", (unsigned)s_wifi_draft_config.ap_channel);
            if (status_ui_keyboard_read_line("AP CHANNEL", channel, channel, sizeof(channel), 2,
                                             false, STATUS_UI_KEYBOARD_TIMEOUT_MS)) {
                unsigned parsed = 0;
                (void)sscanf(channel, "%u", &parsed);
                if (parsed >= 1u && parsed <= 13u) {
                    s_wifi_draft_config.ap_channel = (uint8_t)parsed;
                    (void)app_wifi_set_config(&s_wifi_draft_config, true);
                }
            }
        }
        break;
    default:
        break;
    }
}
#endif

static void status_ui_activate_selected_item(void)
{
    const ui_menu_item_t *item = NULL;
    if (!ui_nav_activate(&s_nav, &item) || item == NULL) {
        return;
    }
#if CONFIG_APP_STATUS_UI_LCD
    switch (item->action) {
    case UI_ITEM_ACTION_START_WIFI_SCAN:
        (void)status_ui_start_wifi_scan();
        (void)ui_nav_enter(&s_nav, UI_SCREEN_NETWORK_WIFI_SCAN);
        break;
    case UI_ITEM_ACTION_CONNECT_WIFI:
        (void)status_ui_connect_selected_wifi();
        break;
    case UI_ITEM_ACTION_START_AP:
        (void)status_ui_start_configured_ap();
        (void)ui_nav_enter(&s_nav, UI_SCREEN_NETWORK_AP_CONFIRM);
        break;
    case UI_ITEM_ACTION_SAVE_AP_CONFIG:
        (void)app_wifi_set_config(&s_wifi_draft_config, true);
        (void)ui_nav_enter(&s_nav, item->target);
        break;
    case UI_ITEM_ACTION_FORGET_WIFI:
        (void)app_wifi_forget_sta_credentials();
        break;
    case UI_ITEM_ACTION_OPEN_KEYBOARD:
        (void)ui_nav_enter(&s_nav, item->target);
        status_ui_open_keyboard_for_current_field();
        break;
    default:
        break;
    }
#endif
}

void status_ui_handle_input(status_ui_input_t input)
{
    bool activate = false;

    portENTER_CRITICAL(&s_state_mux);
    if (!s_menu_mode && input != STATUS_UI_INPUT_BACK) {
        s_menu_mode = true;
        (void)ui_nav_enter(&s_nav, UI_SCREEN_HOME);
        portEXIT_CRITICAL(&s_state_mux);
        return;
    }
    switch (input) {
    case STATUS_UI_INPUT_SELECT:
#if CONFIG_APP_STATUS_UI_LCD
        if (s_nav.current == UI_SCREEN_NETWORK_WIFI_SCAN && s_wifi_scan_results.count > 0u) {
            (void)snprintf(s_wifi_selected_ssid, sizeof(s_wifi_selected_ssid), "%s",
                           s_wifi_scan_results.items[s_wifi_selected_network].ssid);
            (void)ui_nav_enter(&s_nav, UI_SCREEN_NETWORK_WIFI_SELECT);
        } else
#endif
        {
            activate = true;
        }
        break;
    case STATUS_UI_INPUT_NEXT:
#if CONFIG_APP_STATUS_UI_LCD
        if (s_nav.current == UI_SCREEN_NETWORK_WIFI_SCAN && s_wifi_scan_results.count > 0u) {
            s_wifi_selected_network = (s_wifi_selected_network + 1u) % s_wifi_scan_results.count;
        } else
#endif
        {
            (void)ui_nav_next(&s_nav);
        }
        break;
    case STATUS_UI_INPUT_PREV:
#if CONFIG_APP_STATUS_UI_LCD
        if (s_nav.current == UI_SCREEN_NETWORK_WIFI_SCAN && s_wifi_scan_results.count > 0u) {
            s_wifi_selected_network = s_wifi_selected_network == 0u ?
                                      s_wifi_scan_results.count - 1u :
                                      s_wifi_selected_network - 1u;
        } else
#endif
        {
            (void)ui_nav_prev(&s_nav);
        }
        break;
    case STATUS_UI_INPUT_BACK:
        if (s_menu_mode && s_nav.current == UI_SCREEN_HOME) {
            s_menu_mode = false;
        } else if (s_menu_mode) {
            (void)ui_nav_back(&s_nav);
        }
        break;
    default:
        break;
    }
    portEXIT_CRITICAL(&s_state_mux);

    if (activate) {
        status_ui_activate_selected_item();
    }
}


static bool button_is_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == BOARD_BUTTON_ACTIVE_LEVEL;
}

static void record_button_press(gpio_num_t gpio)
{
    portENTER_CRITICAL(&s_state_mux);
    if (gpio == BOARD_BUTTON_KEY1_GPIO) {
        ++s_key1_press_count;
    } else if (gpio == BOARD_BUTTON_KEY2_GPIO) {
        ++s_key2_press_count;
    }
    portEXIT_CRITICAL(&s_state_mux);
}

#if CONFIG_APP_STATUS_UI_LCD
static bool keyboard_is_active(void)
{
    bool active;
    portENTER_CRITICAL(&s_state_mux);
    active = s_keyboard.active;
    portEXIT_CRITICAL(&s_state_mux);
    return active;
}

static void keyboard_queue_event(status_ui_keyboard_event_t event)
{
    if (s_keyboard_queue != NULL) {
        (void)xQueueSend(s_keyboard_queue, &event, 0);
    }
}
#endif

static void maybe_dispatch_button(status_button_t *button, TickType_t now)
{
    bool pressed = button_is_pressed(button->gpio);

    if (pressed != button->last_sample_pressed) {
        button->last_sample_pressed = pressed;
        button->last_change_tick = now;
        return;
    }

    if ((now - button->last_change_tick) < pdMS_TO_TICKS(STATUS_UI_DEBOUNCE_MS)) {
        return;
    }

#if CONFIG_APP_STATUS_UI_LCD
    if (pressed && button->stable_pressed && !button->long_sent &&
        (now - button->pressed_since_tick) >= pdMS_TO_TICKS(STATUS_UI_KEYBOARD_LONG_MS)) {
        if (keyboard_is_active() && button->gpio == BOARD_BUTTON_KEY2_GPIO) {
            button->long_sent = true;
            keyboard_queue_event(STATUS_UI_KEYBOARD_EVENT_PREV);
        } else if (s_menu_mode && button->gpio == BOARD_BUTTON_KEY2_GPIO) {
            button->long_sent = true;
            status_ui_handle_input(STATUS_UI_INPUT_BACK);
        } else if (!s_menu_mode && button->gpio == BOARD_BUTTON_KEY1_GPIO) {
            button->long_sent = true;
            status_ui_open_screen(UI_SCREEN_HOME);
        }
    }
#endif

    if (pressed == button->stable_pressed) {
        return;
    }

    button->stable_pressed = pressed;
#if CONFIG_APP_STATUS_UI_LCD
    if (keyboard_is_active()) {
        if (pressed) {
            record_button_press(button->gpio);
            button->pressed_since_tick = now;
            button->long_sent = false;
            ESP_LOGI(TAG, "keyboard button pressed: %s", button->name);
        } else if (!button->long_sent) {
            keyboard_queue_event(button->gpio == BOARD_BUTTON_KEY1_GPIO ?
                                 STATUS_UI_KEYBOARD_EVENT_SELECT : STATUS_UI_KEYBOARD_EVENT_NEXT);
        }
        return;
    }
#endif
#if CONFIG_APP_STATUS_UI_LCD
    if (s_menu_mode) {
        if (pressed) {
            record_button_press(button->gpio);
            button->pressed_since_tick = now;
            button->long_sent = false;
            ESP_LOGI(TAG, "menu button pressed: %s", button->name);
        } else if (!button->long_sent) {
            status_ui_handle_input(button->gpio == BOARD_BUTTON_KEY1_GPIO ?
                                   STATUS_UI_INPUT_NEXT : STATUS_UI_INPUT_SELECT);
        }
        return;
    }
#endif
    if (pressed) {
        record_button_press(button->gpio);
#if CONFIG_APP_STATUS_UI_LCD
        button->pressed_since_tick = now;
        button->long_sent = false;
#endif
        ESP_LOGI(TAG, "button pressed: %s", button->name);
        if (button->handler != NULL) {
            button->handler(s_handlers.ctx);
        }
    }
}

static void status_ui_button_task(void *arg)
{
    (void)arg;

    status_button_t buttons[] = {
        {
            .gpio = BOARD_BUTTON_KEY1_GPIO,
            .name = "KEY1",
            .handler = s_handlers.key1_pressed,
        },
        {
            .gpio = BOARD_BUTTON_KEY2_GPIO,
            .name = "KEY2",
            .handler = s_handlers.key2_pressed,
        },
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        buttons[i].stable_pressed = button_is_pressed(buttons[i].gpio);
        buttons[i].last_sample_pressed = buttons[i].stable_pressed;
        buttons[i].last_change_tick = xTaskGetTickCount();
    }

    while (true) {
        TickType_t now = xTaskGetTickCount();
        for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            maybe_dispatch_button(&buttons[i], now);
        }
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_POLL_MS));
    }
}

#if CONFIG_APP_STATUS_UI_LCD
static const uint8_t *glyph_rows(char c)
{
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t glyph_0[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t glyph_1[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t glyph_2[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t glyph_3[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t glyph_4[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t glyph_5[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t glyph_6[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t glyph_7[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t glyph_8[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t glyph_9[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t glyph_d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t glyph_f[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
    static const uint8_t glyph_h[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_i[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t glyph_j[7] = {7, 2, 2, 2, 18, 18, 12};
    static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t glyph_l[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t glyph_m[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t glyph_q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t glyph_w[7] = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t glyph_x[7] = {17, 17, 10, 4, 10, 17, 17};
    static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};
    static const uint8_t glyph_z[7] = {31, 1, 2, 4, 8, 16, 31};
    static const uint8_t glyph_colon[7] = {0, 4, 4, 0, 4, 4, 0};
    static const uint8_t glyph_dash[7] = {0, 0, 0, 14, 0, 0, 0};
    static const uint8_t glyph_dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t glyph_slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const uint8_t glyph_percent[7] = {24, 25, 2, 4, 8, 19, 3};
    static const uint8_t glyph_at[7] = {14, 17, 23, 21, 23, 16, 14};
    static const uint8_t glyph_underscore[7] = {0, 0, 0, 0, 0, 0, 31};
    static const uint8_t glyph_plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t glyph_question[7] = {14, 17, 1, 2, 4, 0, 4};
    static const uint8_t glyph_lparen[7] = {2, 4, 8, 8, 8, 4, 2};
    static const uint8_t glyph_rparen[7] = {8, 4, 2, 2, 2, 4, 8};
    static const uint8_t glyph_hash[7] = {10, 31, 10, 10, 31, 10, 0};
    static const uint8_t glyph_dollar[7] = {4, 15, 20, 14, 5, 30, 4};
    static const uint8_t glyph_amp[7] = {12, 18, 20, 8, 21, 18, 13};
    static const uint8_t glyph_equal[7] = {0, 0, 31, 0, 31, 0, 0};
    static const uint8_t glyph_bang[7] = {4, 4, 4, 4, 4, 0, 4};
    static const uint8_t glyph_caret[7] = {4, 10, 17, 0, 0, 0, 0};
    static const uint8_t glyph_star[7] = {0, 21, 14, 31, 14, 21, 0};
    static const uint8_t glyph_gt[7] = {16, 8, 4, 2, 4, 8, 16};

    switch ((char)toupper((unsigned char)c)) {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'A': return glyph_a;
    case 'B': return glyph_b;
    case 'C': return glyph_c;
    case 'D': return glyph_d;
    case 'E': return glyph_e;
    case 'F': return glyph_f;
    case 'G': return glyph_g;
    case 'H': return glyph_h;
    case 'I': return glyph_i;
    case 'J': return glyph_j;
    case 'K': return glyph_k;
    case 'L': return glyph_l;
    case 'M': return glyph_m;
    case 'N': return glyph_n;
    case 'O': return glyph_o;
    case 'P': return glyph_p;
    case 'Q': return glyph_q;
    case 'R': return glyph_r;
    case 'S': return glyph_s;
    case 'T': return glyph_t;
    case 'U': return glyph_u;
    case 'V': return glyph_v;
    case 'W': return glyph_w;
    case 'X': return glyph_x;
    case 'Y': return glyph_y;
    case 'Z': return glyph_z;
    case ':': return glyph_colon;
    case '-': return glyph_dash;
    case '.': return glyph_dot;
    case '/': return glyph_slash;
    case '%': return glyph_percent;
    case '@': return glyph_at;
    case '_': return glyph_underscore;
    case '+': return glyph_plus;
    case '?': return glyph_question;
    case '(': return glyph_lparen;
    case ')': return glyph_rparen;
    case '#': return glyph_hash;
    case '$': return glyph_dollar;
    case '&': return glyph_amp;
    case '=': return glyph_equal;
    case '!': return glyph_bang;
    case '^': return glyph_caret;
    case '*': return glyph_star;
    case '>': return glyph_gt;
    default: return blank;
    }
}

static void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int x_end = x + w;
    int y_end = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x_end > BOARD_LCD_H_RES) {
        x_end = BOARD_LCD_H_RES;
    }
    if (y_end > BOARD_LCD_V_RES) {
        y_end = BOARD_LCD_V_RES;
    }
    for (int row = y; row < y_end; ++row) {
        for (int col = x; col < x_end; ++col) {
            s_framebuffer[row * BOARD_LCD_H_RES + col] = color;
        }
    }
}

static void lcd_draw_char(int x, int y, char c, uint16_t color, uint8_t scale)
{
    const uint8_t *rows = glyph_rows(c);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1U << (4 - col))) != 0) {
                lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void lcd_draw_text(int x, int y, const char *text, uint16_t color, uint8_t scale)
{
    int cursor_x = x;
    while (*text != '\0' && cursor_x < BOARD_LCD_H_RES) {
        if (*text != ' ') {
            lcd_draw_char(cursor_x, y, *text, color, scale);
        }
        cursor_x += 6 * scale;
        ++text;
    }
}


static const char s_keyboard_keys[STATUS_UI_KEYBOARD_ROWS][STATUS_UI_KEYBOARD_COLS][2] = {
    {{'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}},
    {{'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}},
    {{'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'-', '_'}},
    {{'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {'.', '>'}, {'/', '?'}, {'@', ':'}},
};

static void lcd_draw_text_clipped(int x, int y, const char *text, uint16_t color, uint8_t scale, size_t max_chars)
{
    char buf[40];
    if (max_chars >= sizeof(buf)) {
        max_chars = sizeof(buf) - 1u;
    }
    size_t i = 0;
    while (i < max_chars && text != NULL && text[i] != '\0') {
        buf[i] = text[i];
        ++i;
    }
    buf[i] = '\0';
    lcd_draw_text(x, y, buf, color, scale);
}

static void keyboard_snapshot(status_ui_keyboard_state_t *out)
{
    portENTER_CRITICAL(&s_state_mux);
    *out = s_keyboard;
    portEXIT_CRITICAL(&s_state_mux);
}

static void status_ui_render_keyboard_lcd(void)
{
    status_ui_keyboard_state_t kb;
    keyboard_snapshot(&kb);
    if (!kb.active) {
        return;
    }

    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 16, STATUS_UI_LCD_HEADER_BG);
    lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 4, kb.title, STATUS_UI_LCD_TEXT, 1, 20);

    char display[STATUS_UI_KEYBOARD_MAX_TEXT + 1];
    size_t text_len = strlen(kb.text);
    for (size_t i = 0; i < text_len && i < sizeof(display) - 1u; ++i) {
        display[i] = kb.secret ? '*' : kb.text[i];
    }
    display[text_len < sizeof(display) ? text_len : sizeof(display) - 1u] = '\0';
    lcd_fill_rect(3, 19, BOARD_LCD_H_RES - 6, 18, 0x2104);
    lcd_draw_text_clipped(6, 24, display, STATUS_UI_LCD_TEXT, 1, 19);

    char count[16];
    snprintf(count, sizeof(count), "%u/%u", (unsigned)text_len, (unsigned)kb.max_len);
    lcd_draw_text(BOARD_LCD_H_RES - 38, 4, count, STATUS_UI_LCD_DIM, 1);

    static const char *controls[STATUS_UI_KEYBOARD_CONTROLS] = {"OK", "AA", "DEL", "SPC", "ESC"};
    const int control_w = BOARD_LCD_H_RES / STATUS_UI_KEYBOARD_CONTROLS;
    for (int i = 0; i < STATUS_UI_KEYBOARD_CONTROLS; ++i) {
        const bool selected = kb.selected == i;
        const int x = i * control_w;
        uint16_t bg = selected ? STATUS_UI_LCD_WARN : 0x3186;
        uint16_t fg = selected ? STATUS_UI_LCD_BG : STATUS_UI_LCD_TEXT;
        lcd_fill_rect(x + 1, 42, control_w - 2, 18, bg);
        lcd_draw_text(x + 5, 48, controls[i], fg, 1);
    }

    const int key_w = BOARD_LCD_H_RES / STATUS_UI_KEYBOARD_COLS;
    const int key_h = 34;
    const int y0 = 68;
    for (int row = 0; row < STATUS_UI_KEYBOARD_ROWS; ++row) {
        for (int col = 0; col < STATUS_UI_KEYBOARD_COLS; ++col) {
            const int index = STATUS_UI_KEYBOARD_CONTROLS + row * STATUS_UI_KEYBOARD_COLS + col;
            const bool selected = kb.selected == index;
            const int x = col * key_w;
            const int y = y0 + row * key_h;
            char label[2] = {s_keyboard_keys[row][col][kb.caps ? 1 : 0], '\0'};
            uint16_t bg = selected ? STATUS_UI_LCD_OK : 0x1082;
            uint16_t fg = selected ? STATUS_UI_LCD_BG : STATUS_UI_LCD_TEXT;
            lcd_fill_rect(x + 1, y + 1, key_w - 2, key_h - 2, bg);
            lcd_draw_text(x + 4, y + 11, label, fg, 1);
        }
    }

    lcd_draw_text(4, BOARD_LCD_V_RES - 13, "K1 SEL K2 NEXT HOLD PREV", STATUS_UI_LCD_DIM, 1);
}

static void keyboard_move(int delta)
{
    const int total = STATUS_UI_KEYBOARD_CONTROLS + STATUS_UI_KEYBOARD_ROWS * STATUS_UI_KEYBOARD_COLS;
    portENTER_CRITICAL(&s_state_mux);
    s_keyboard.selected = (s_keyboard.selected + delta + total) % total;
    portEXIT_CRITICAL(&s_state_mux);
}

static void keyboard_select(void)
{
    portENTER_CRITICAL(&s_state_mux);
    if (s_keyboard.selected < STATUS_UI_KEYBOARD_CONTROLS) {
        switch (s_keyboard.selected) {
        case 0:
            s_keyboard.done = true;
            break;
        case 1:
            s_keyboard.caps = !s_keyboard.caps;
            break;
        case 2: {
            size_t len = strlen(s_keyboard.text);
            if (len > 0) {
                s_keyboard.text[len - 1u] = '\0';
            }
            break;
        }
        case 3: {
            size_t len = strlen(s_keyboard.text);
            if (len < s_keyboard.max_len && len + 1u < sizeof(s_keyboard.text)) {
                s_keyboard.text[len] = ' ';
                s_keyboard.text[len + 1u] = '\0';
            }
            break;
        }
        case 4:
            s_keyboard.cancelled = true;
            break;
        default:
            break;
        }
    } else {
        const int key_index = s_keyboard.selected - STATUS_UI_KEYBOARD_CONTROLS;
        const int row = key_index / STATUS_UI_KEYBOARD_COLS;
        const int col = key_index % STATUS_UI_KEYBOARD_COLS;
        size_t len = strlen(s_keyboard.text);
        if (row >= 0 && row < STATUS_UI_KEYBOARD_ROWS && col >= 0 && col < STATUS_UI_KEYBOARD_COLS &&
            len < s_keyboard.max_len && len + 1u < sizeof(s_keyboard.text)) {
            s_keyboard.text[len] = s_keyboard_keys[row][col][s_keyboard.caps ? 1 : 0];
            s_keyboard.text[len + 1u] = '\0';
        }
    }
    portEXIT_CRITICAL(&s_state_mux);
}

bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms)
{
    if (out == NULL || out_len == 0 || max_len == 0 || s_keyboard_queue == NULL || s_panel == NULL || s_framebuffer == NULL) {
        return false;
    }
    if (max_len >= out_len) {
        max_len = out_len - 1u;
    }
    if (max_len > STATUS_UI_KEYBOARD_MAX_TEXT) {
        max_len = STATUS_UI_KEYBOARD_MAX_TEXT;
    }

    xQueueReset(s_keyboard_queue);
    portENTER_CRITICAL(&s_state_mux);
    memset(&s_keyboard, 0, sizeof(s_keyboard));
    s_keyboard.active = true;
    s_keyboard.secret = secret;
    s_keyboard.max_len = max_len;
    s_keyboard.selected = STATUS_UI_KEYBOARD_CONTROLS;
    snprintf(s_keyboard.title, sizeof(s_keyboard.title), "%s", title != NULL ? title : "Keyboard");
    snprintf(s_keyboard.text, sizeof(s_keyboard.text), "%s", initial != NULL ? initial : "");
    s_keyboard.text[max_len] = '\0';
    portEXIT_CRITICAL(&s_state_mux);

    ESP_LOGI(TAG, "virtual keyboard start: %s", title != NULL ? title : "Keyboard");
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    bool ok = false;
    while (true) {
        status_ui_keyboard_event_t event;
        TickType_t wait = pdMS_TO_TICKS(100);
        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            if (elapsed >= timeout) {
                break;
            }
            TickType_t remaining = timeout - elapsed;
            if (remaining < wait) {
                wait = remaining;
            }
        }
        if (xQueueReceive(s_keyboard_queue, &event, wait) == pdTRUE) {
            if (event == STATUS_UI_KEYBOARD_EVENT_SELECT) {
                keyboard_select();
            } else if (event == STATUS_UI_KEYBOARD_EVENT_NEXT) {
                keyboard_move(1);
            } else if (event == STATUS_UI_KEYBOARD_EVENT_PREV) {
                keyboard_move(-1);
            }
        }
        portENTER_CRITICAL(&s_state_mux);
        bool done = s_keyboard.done;
        bool cancelled = s_keyboard.cancelled;
        portEXIT_CRITICAL(&s_state_mux);
        if (done || cancelled) {
            ok = done && !cancelled;
            break;
        }
    }

    portENTER_CRITICAL(&s_state_mux);
    if (ok) {
        snprintf(out, out_len, "%s", s_keyboard.text);
    }
    s_keyboard.active = false;
    portEXIT_CRITICAL(&s_state_mux);
    ESP_LOGI(TAG, "virtual keyboard end: %s", ok ? "ok" : "cancel/timeout");
    return ok;
}

static uint16_t state_color(status_ui_state_t state)
{
    switch (state) {
    case STATUS_UI_STATE_READY:
        return STATUS_UI_LCD_OK;
    case STATUS_UI_STATE_BOOTING:
    case STATUS_UI_STATE_NO_TRANSPORT:
        return STATUS_UI_LCD_WARN;
    case STATUS_UI_STATE_ERROR:
        return STATUS_UI_LCD_ERR;
    default:
        return STATUS_UI_LCD_DIM;
    }
}


typedef struct {
    status_ui_state_t state;
    bool monitoring_enabled;
    bool service_enabled;
    uint32_t key1_count;
    uint32_t key2_count;
    status_ui_sound_meter_snapshot_t snapshot;
    app_display_mode_t display_mode;
} status_ui_lcd_context_t;

static const char *display_title(app_display_mode_t mode)
{
    switch (mode) {
    case APP_DISPLAY_VU:
        return "METER";
    case APP_DISPLAY_NUMERIC:
        return "NUMERIC";
    case APP_DISPLAY_BLE_STATUS:
        return "BLE";
    case APP_DISPLAY_DIAGNOSTICS:
        return "DIAG";
    default:
        return "APP";
    }
}

static void status_ui_render_app_shell(const char *title, const status_ui_lcd_context_t *ctx, uint16_t accent)
{
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, accent);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD, title, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

    char line[32];
    snprintf(line, sizeof(line), "K1 VIEW K2 APP");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, BOARD_LCD_V_RES - 18, line, STATUS_UI_LCD_DIM, 1);
    snprintf(line, sizeof(line), "%.10s", status_ui_state_name(ctx->state));
    lcd_draw_text(BOARD_LCD_H_RES - 64, BOARD_LCD_V_RES - 18, line, state_color(ctx->state), 1);
}

static void format_dbfs_q8(char *buf, size_t len, int32_t dbfs_q8)
{
    int32_t whole = dbfs_q8 / 256;
    int32_t frac = dbfs_q8 % 256;
    if (frac < 0) {
        frac = -frac;
    }
    frac = (frac * 10) / 256;
    snprintf(buf, len, "%ld.%ld", (long)whole, (long)frac);
}

static void lcd_draw_horizontal_bar(int x, int y, int w, int h, uint16_t percent, uint16_t fill, uint16_t bg)
{
    if (percent > 100) {
        percent = 100;
    }
    lcd_fill_rect(x, y, w, h, bg);
    lcd_fill_rect(x, y, (w * percent) / 100, h, fill);
}

static uint16_t vu_color(const status_ui_sound_meter_snapshot_t *snap)
{
    if ((snap->flags & AUDIO_METRICS_FLAG_CLIPPING) != 0 || snap->vu_percent >= 90) {
        return STATUS_UI_LCD_ERR;
    }
    if (snap->vu_percent >= 65) {
        return STATUS_UI_LCD_WARN;
    }
    return STATUS_UI_LCD_OK;
}

static void status_ui_render_vu_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    if (snap->calibration_active) {
        char line[32];
        lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 40, "KEEP QUIET", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
        snprintf(line, sizeof(line), "%lu/%lu", (unsigned long)snap->calibration_collected_windows,
                 (unsigned long)snap->calibration_required_windows);
        lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 68, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
        uint16_t percent = snap->calibration_required_windows == 0
                               ? 0
                               : (uint16_t)((snap->calibration_collected_windows * 100U) /
                                            snap->calibration_required_windows);
        lcd_draw_horizontal_bar(4, 100, BOARD_LCD_H_RES - 8, 24, percent, STATUS_UI_LCD_WARN, STATUS_UI_LCD_DIM);
        return;
    }
    char rms[16];
    char peak[16];
    char line[32];
    format_dbfs_q8(rms, sizeof(rms), snap->rms_dbfs_q8);
    format_dbfs_q8(peak, sizeof(peak), snap->peak_dbfs_q8);
    snprintf(line, sizeof(line), "RMS %sDB", rms);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 36, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PK %sDB", peak);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 56, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_horizontal_bar(4, 90, BOARD_LCD_H_RES - 8, 28, snap->vu_percent, vu_color(snap), STATUS_UI_LCD_DIM);
    snprintf(line, sizeof(line), "VU %u%%", (unsigned)snap->vu_percent);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 124, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "BLE %s", snap->ble_connected ? "ON" : "OFF");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 148, line, snap->ble_connected ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "MODE %.8s", app_mode_name((app_mode_t)snap->app_mode));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 168, line, STATUS_UI_LCD_DIM, 1);
    snprintf(line, sizeof(line), "CLIP %u", (unsigned)snap->clipped_samples);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 184, line, snap->clipped_samples ? STATUS_UI_LCD_ERR : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_numeric_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    char rms[16];
    char peak[16];
    char line[32];
    format_dbfs_q8(rms, sizeof(rms), snap->rms_dbfs_q8);
    format_dbfs_q8(peak, sizeof(peak), snap->peak_dbfs_q8);
    snprintf(line, sizeof(line), "RMS:%s", rms);
    lcd_draw_text(4, 34, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PK:%s", peak);
    lcd_draw_text(4, 54, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "VU:%u%%", (unsigned)snap->vu_percent);
    lcd_draw_text(4, 74, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "ZC:%lu", (unsigned long)snap->zero_crossings);
    lcd_draw_text(4, 94, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "FLG:%lx", (unsigned long)snap->flags);
    lcd_draw_text(4, 114, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "SEQ:%lu", (unsigned long)snap->sequence);
    lcd_draw_text(4, 134, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_ble_lcd(const status_ui_sound_meter_snapshot_t *snap)
{
    char line[32];
    snprintf(line, sizeof(line), "CONN %s", snap->ble_connected ? "YES" : "NO");
    lcd_draw_text(4, 36, line, snap->ble_connected ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "METR %s", snap->ble_metrics_notify_enabled ? "YES" : "NO");
    lcd_draw_text(4, 56, line, snap->ble_metrics_notify_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "PCM %s", snap->ble_pcm_notify_enabled ? "YES" : "NO");
    lcd_draw_text(4, 76, line, snap->ble_pcm_notify_enabled ? STATUS_UI_LCD_WARN : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    snprintf(line, sizeof(line), "SEQ %lu", (unsigned long)snap->sequence);
    lcd_draw_text(4, 96, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
}

static void status_ui_render_diagnostics_lcd(const status_ui_lcd_context_t *ctx)
{
    int y = 32;
    char line[32];
    snprintf(line, sizeof(line), "STATE");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "%.12s", status_ui_state_name(ctx->state));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, state_color(ctx->state), STATUS_UI_LCD_TEXT_SCALE);

    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    snprintf(line, sizeof(line), "BLE SVC: %s", bool_label(ctx->service_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, ctx->service_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "MON: %s", bool_label(ctx->monitoring_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, ctx->monitoring_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "PCM: %d HZ", BOARD_I2S_SAMPLE_RATE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY1:%lu", (unsigned long)ctx->key1_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY2:%lu", (unsigned long)ctx->key2_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "UP:%lus", (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000U));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

#ifdef CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME
    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME, STATUS_UI_LCD_DIM, 1);
#endif
}

static void status_ui_render_waiting_lcd(void)
{
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 46, "WAITING", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 70, "FOR PCM", STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 104, "CHECK I2S", STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 124, "CLOCK/CODEC", STATUS_UI_LCD_DIM, 1);
}


static void status_ui_render_menu_lcd(const ui_nav_state_t *nav)
{
    const ui_screen_def_t *screen = ui_nav_current(nav);
    if (screen == NULL) {
        return;
    }
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, STATUS_UI_LCD_HEADER_BG);
    lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD,
                          screen->title, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE, 15);

    if (screen->id == UI_SCREEN_NETWORK_STATUS) {
        char status_json[256];
        char line[48];
        app_wifi_status_t status;
        if (app_wifi_get_status(&status)) {
            snprintf(line, sizeof(line), "Mode:%s", app_wifi_mode_name(status.active_mode));
            lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 34, line, STATUS_UI_LCD_TEXT, 1);
            snprintf(line, sizeof(line), "STA:%s", status.sta_ssid[0] ? status.sta_ssid : "-");
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 50, line, STATUS_UI_LCD_TEXT, 1, 19);
            snprintf(line, sizeof(line), "IP:%s", status.sta_ip);
            lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 66, line, STATUS_UI_LCD_TEXT, 1);
            snprintf(line, sizeof(line), "AP:%s", status.ap_ssid);
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 82, line, STATUS_UI_LCD_TEXT, 1, 19);
            snprintf(line, sizeof(line), "AP IP:%s", status.ap_ip);
            lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 98, line, STATUS_UI_LCD_TEXT, 1);
            snprintf(line, sizeof(line), "CH:%u MAX:%u", (unsigned)status.ap_channel,
                     (unsigned)status.ap_max_connections);
            lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 114, line, STATUS_UI_LCD_TEXT, 1);
            snprintf(line, sizeof(line), "URL:%s", status.web_url);
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 130, line, STATUS_UI_LCD_TEXT, 1, 19);
        } else if (app_wifi_status_json(status_json, sizeof(status_json))) {
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 34, status_json, STATUS_UI_LCD_TEXT, 1, 19);
        }
    } else if (screen->id == UI_SCREEN_NETWORK_AP_CONFIRM) {
        char line[48];
        app_wifi_status_t status;
        if (app_wifi_get_status(&status)) {
            snprintf(line, sizeof(line), "AP:%s", status.ap_ssid);
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 34, line, STATUS_UI_LCD_TEXT, 1, 19);
            snprintf(line, sizeof(line), "IP:%s", status.ap_ip);
            lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, 50, line, STATUS_UI_LCD_TEXT, 1);
            snprintf(line, sizeof(line), "URL:%s", status.web_url);
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 66, line, STATUS_UI_LCD_OK, 1, 19);
        }
    } else if (screen->id == UI_SCREEN_NETWORK_WIFI_SAVED) {
        char line[48];
        app_wifi_config_t config;
        if (app_wifi_get_config(&config) && config.sta_ssid[0] != '\0') {
            snprintf(line, sizeof(line), "Saved:%s", config.sta_ssid);
        } else {
            snprintf(line, sizeof(line), "Saved:-");
        }
        lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 34, line, STATUS_UI_LCD_TEXT, 1, 19);
    } else if (screen->id == UI_SCREEN_NETWORK_WIFI_SCAN) {
        char line[48];
        if (!s_wifi_scan_results.ok) {
            snprintf(line, sizeof(line), "Scan: %s", s_wifi_scan_results.error[0] ? s_wifi_scan_results.error : "not run");
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 34, line, STATUS_UI_LCD_WARN, 1, 19);
        }
        const size_t visible_rows = s_wifi_scan_results.count < 5u ? s_wifi_scan_results.count : 5u;
        size_t first = 0u;
        if (visible_rows > 0u && s_wifi_selected_network >= visible_rows) {
            first = s_wifi_selected_network - visible_rows + 1u;
        }
        for (size_t row = 0; row < visible_rows; ++row) {
            const size_t i = first + row;
            snprintf(line, sizeof(line), "%c%s %ddBm ch%u",
                     i == s_wifi_selected_network ? '>' : ' ',
                     s_wifi_scan_results.items[i].ssid[0] ? s_wifi_scan_results.items[i].ssid : "(hidden)",
                     s_wifi_scan_results.items[i].rssi,
                     (unsigned)s_wifi_scan_results.items[i].channel);
            lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, 34 + (int)row * STATUS_UI_LCD_LINE_HEIGHT,
                                  line, i == s_wifi_selected_network ? STATUS_UI_LCD_OK : STATUS_UI_LCD_TEXT, 1, 19);
        }
    }

    int y = 118;
    if (screen->id != UI_SCREEN_NETWORK_WIFI_SCAN && screen->id != UI_SCREEN_NETWORK_STATUS &&
        screen->id != UI_SCREEN_NETWORK_WIFI_SAVED && screen->id != UI_SCREEN_NETWORK_AP_CONFIRM) {
        y = 34;
    }
    for (size_t i = 0; i < screen->item_count && y < BOARD_LCD_V_RES - 16; ++i) {
        const bool selected = i == nav->selected_index;
        char label[32];
        snprintf(label, sizeof(label), "%c %s", selected ? '>' : ' ', screen->items[i].label);
        lcd_draw_text_clipped(STATUS_UI_LCD_LEFT_PAD, y, label,
                              selected ? STATUS_UI_LCD_OK : STATUS_UI_LCD_TEXT, 1, 19);
        y += STATUS_UI_LCD_LINE_HEIGHT;
    }
    lcd_draw_text(4, BOARD_LCD_V_RES - 13, "K1 NEXT K2 SEL HOLD BACK", STATUS_UI_LCD_DIM, 1);
}

static void status_ui_render_lcd(void)
{
    status_ui_lcd_context_t ctx;

    portENTER_CRITICAL(&s_state_mux);
    ctx.state = s_state;
    ctx.monitoring_enabled = s_monitoring_enabled;
    ctx.service_enabled = s_service_enabled;
    ctx.key1_count = s_key1_press_count;
    ctx.key2_count = s_key2_press_count;
    ctx.snapshot = s_sound_snapshot;
    ctx.display_mode = s_display_mode;
    portEXIT_CRITICAL(&s_state_mux);

    uint16_t accent = STATUS_UI_LCD_HEADER_BG;
    if (ctx.snapshot.valid && (ctx.snapshot.flags & AUDIO_METRICS_FLAG_CLIPPING) != 0) {
        accent = STATUS_UI_LCD_ERR;
    }
    if (keyboard_is_active()) {
        status_ui_render_keyboard_lcd();
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        }
        return;
    }
    if (s_menu_mode) {
        status_ui_render_menu_lcd(&s_nav);
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        }
        return;
    }

    status_ui_render_app_shell(display_title(ctx.display_mode), &ctx, accent);

    if (!ctx.snapshot.valid && ctx.display_mode != APP_DISPLAY_DIAGNOSTICS) {
        status_ui_render_waiting_lcd();
    } else {
        ctx.snapshot.display_mode = (uint8_t)ctx.display_mode;
        switch (ctx.display_mode) {
        case APP_DISPLAY_VU:
            status_ui_render_vu_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_NUMERIC:
            status_ui_render_numeric_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_BLE_STATUS:
            status_ui_render_ble_lcd(&ctx.snapshot);
            break;
        case APP_DISPLAY_DIAGNOSTICS:
        default:
            status_ui_render_diagnostics_lcd(&ctx);
            break;
        }
    }

    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
    }
}

static void status_ui_lcd_task(void *arg)
{
    (void)arg;

    while (true) {
        status_ui_render_lcd();
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_LCD_REFRESH_MS));
    }
}

static esp_err_t status_ui_lcd_init(void)
{
    ESP_LOGI(TAG, "LCD init start: ST7789P3 %dx%d host=%d pixel_clock=%d Hz",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, BOARD_LCD_HOST, BOARD_LCD_PIXEL_CLOCK_HZ);
    esp_err_t err = board_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during shared I2C setup: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: shared I2C bus ready");

    err = m5pm1_enable_lcd_power(BOARD_I2C_PORT, BOARD_M5PM1_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during M5PM1 LCD/L3B power enable: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: M5PM1 LCD/L3B power enabled");

    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << BOARD_LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&bl_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed configuring backlight GPIO%d: %s",
                 BOARD_LCD_BL_GPIO, esp_err_to_name(err));
        return err;
    }
    gpio_set_level(BOARD_LCD_BL_GPIO, !BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD init step ok: backlight GPIO%d configured off_level=%d on_level=%d",
             BOARD_LCD_BL_GPIO, !BOARD_LCD_BL_ON_LEVEL, BOARD_LCD_BL_ON_LEVEL);

    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SCLK_GPIO,
        .mosi_io_num = BOARD_LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_LOGI(TAG, "LCD init step: SPI bus host=%d MOSI=GPIO%d SCLK=GPIO%d max_transfer=%u",
             BOARD_LCD_HOST, BOARD_LCD_MOSI_GPIO, BOARD_LCD_SCLK_GPIO,
             (unsigned)buscfg.max_transfer_sz);
    err = spi_bus_initialize(BOARD_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed during SPI bus init: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .cs_gpio_num = BOARD_LCD_CS_GPIO,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = BOARD_LCD_CMD_BITS,
        .lcd_param_bits = BOARD_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST, &io_config, &s_panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed creating panel IO: DC=GPIO%d CS=GPIO%d err=%s",
                 BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, esp_err_to_name(err));
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }
    ESP_LOGI(TAG, "LCD init step ok: panel IO DC=GPIO%d CS=GPIO%d queue_depth=%d",
             BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, io_config.trans_queue_depth);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(s_panel_io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed creating ST7789 panel: RST=GPIO%d err=%s",
                 BOARD_LCD_RST_GPIO, esp_err_to_name(err));
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    err = esp_lcd_panel_reset(s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed resetting panel: %s", esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(s_panel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed initialising panel: %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_invert_color(s_panel, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed setting color inversion: %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_set_gap(s_panel, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed setting panel gap %d,%d: %s",
                     BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP, esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_disp_on_off(s_panel, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD init failed enabling panel display: %s", esp_err_to_name(err));
        }
    }
    if (err != ESP_OK) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    ESP_LOGI(TAG, "LCD init step ok: panel configured gap=%d,%d inversion=on",
             BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP);

    s_framebuffer = heap_caps_malloc(BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "LCD init failed allocating framebuffer: %u bytes DMA/internal",
                 (unsigned)(BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t)));
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t created = xTaskCreate(status_ui_lcd_task, "status_ui_lcd",
                                     STATUS_UI_LCD_TASK_STACK, NULL,
                                     STATUS_UI_LCD_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "LCD init failed starting refresh task");
        heap_caps_free(s_framebuffer);
        s_framebuffer = NULL;
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return ESP_ERR_NO_MEM;
    }

    gpio_set_level(BOARD_LCD_BL_GPIO, BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD init step ok: backlight enabled GPIO%d level=%d",
             BOARD_LCD_BL_GPIO, BOARD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "LCD debug UI ready: ST7789P3 %dx%d gap=%d,%d MOSI=%d SCLK=%d DC=%d CS=%d RST=%d BL=%d",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP, BOARD_LCD_MOSI_GPIO, BOARD_LCD_SCLK_GPIO,
             BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, BOARD_LCD_RST_GPIO, BOARD_LCD_BL_GPIO);
    return ESP_OK;
}
#else
bool status_ui_keyboard_read_line(const char *title, const char *initial, char *out, size_t out_len, size_t max_len, bool secret, uint32_t timeout_ms)
{
    (void)title;
    (void)initial;
    (void)out;
    (void)out_len;
    (void)max_len;
    (void)secret;
    (void)timeout_ms;
    return false;
}
#endif

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers)
{
#if CONFIG_APP_STATUS_UI_LCD
    if (s_keyboard_queue == NULL) {
        s_keyboard_queue = xQueueCreate(8, sizeof(status_ui_keyboard_event_t));
        if (s_keyboard_queue == NULL) {
            ESP_LOGE(TAG, "failed to create virtual keyboard queue");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    ui_nav_init(&s_nav);
#if CONFIG_APP_STATUS_UI_LCD
    (void)app_wifi_get_config(&s_wifi_draft_config);
#endif

    if (handlers != NULL) {
        memcpy(&s_handlers, handlers, sizeof(s_handlers));
    } else {
        memset(&s_handlers, 0, sizeof(s_handlers));
    }

    uint64_t pin_mask = (1ULL << BOARD_BUTTON_KEY1_GPIO) |
                        (1ULL << BOARD_BUTTON_KEY2_GPIO);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure StickS3 keys: %s", esp_err_to_name(err));
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return err;
    }

    BaseType_t created = xTaskCreate(status_ui_button_task, "status_ui_buttons",
                                     STATUS_UI_TASK_STACK, NULL,
                                     STATUS_UI_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to start button task");
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_APP_STATUS_UI_LCD
    err = status_ui_lcd_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD debug UI disabled: %s", esp_err_to_name(err));
    }
#endif

    ESP_LOGI(TAG, "status UI ready; StickS3 keys KEY1=%d KEY2=%d",
             BOARD_BUTTON_KEY1_GPIO, BOARD_BUTTON_KEY2_GPIO);
    ESP_LOGI(TAG, "status: %s", status_ui_state_name(status_ui_get_state()));
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(status_ui_get_monitoring_enabled()));
    ESP_LOGI(TAG, "transport service: %s", bool_label(status_ui_get_service_enabled()));
    return ESP_OK;
}
