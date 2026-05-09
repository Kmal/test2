#include "status_ui.h"

#include "board_sticks3.h"
#include "audio_metrics.h"

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
static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;

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

    if (pressed == button->stable_pressed) {
        return;
    }

    button->stable_pressed = pressed;
    if (pressed) {
        record_button_press(button->gpio);
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
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    uint16_t header = ((snap->flags & AUDIO_METRICS_FLAG_CLIPPING) != 0) ? STATUS_UI_LCD_ERR : STATUS_UI_LCD_HEADER_BG;
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, header);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD,
                  snap->calibration_active ? "CALIBRATE" : "M5S3 LEVEL",
                  STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

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
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, STATUS_UI_LCD_HEADER_BG);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD, "METER NUM", STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
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
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, STATUS_UI_LCD_HEADER_BG);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD, "BLE STATUS", STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
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

static void status_ui_render_lcd(void)
{
    status_ui_state_t state;
    bool monitoring_enabled;
    bool service_enabled;
    uint32_t key1_count;
    uint32_t key2_count;

    portENTER_CRITICAL(&s_state_mux);
    state = s_state;
    monitoring_enabled = s_monitoring_enabled;
    service_enabled = s_service_enabled;
    key1_count = s_key1_press_count;
    key2_count = s_key2_press_count;
    status_ui_sound_meter_snapshot_t snapshot = s_sound_snapshot;
    app_display_mode_t display_mode = s_display_mode;
    portEXIT_CRITICAL(&s_state_mux);

    if (snapshot.valid && display_mode != APP_DISPLAY_DIAGNOSTICS) {
        snapshot.display_mode = (uint8_t)display_mode;
        switch (display_mode) {
        case APP_DISPLAY_VU:
            status_ui_render_vu_lcd(&snapshot);
            break;
        case APP_DISPLAY_NUMERIC:
            status_ui_render_numeric_lcd(&snapshot);
            break;
        case APP_DISPLAY_BLE_STATUS:
            status_ui_render_ble_lcd(&snapshot);
            break;
        case APP_DISPLAY_DIAGNOSTICS:
        default:
            break;
        }
        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        }
        return;
    }

    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, STATUS_UI_LCD_BG);
    lcd_fill_rect(0, 0, BOARD_LCD_H_RES, 24, STATUS_UI_LCD_HEADER_BG);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, STATUS_UI_LCD_TOP_PAD, "STICKS3 DBG", STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

    int y = 32;
    char line[32];
    snprintf(line, sizeof(line), "STATE");
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "%.12s", status_ui_state_name(state));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, state_color(state), STATUS_UI_LCD_TEXT_SCALE);

    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    snprintf(line, sizeof(line), "BLE SVC: %s", bool_label(service_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, service_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_WARN, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "MON: %s", bool_label(monitoring_enabled));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, monitoring_enabled ? STATUS_UI_LCD_OK : STATUS_UI_LCD_DIM, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "PCM: %d HZ", BOARD_I2S_SAMPLE_RATE);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY1:%lu", (unsigned long)key1_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "KEY2:%lu", (unsigned long)key2_count);
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);
    y += STATUS_UI_LCD_LINE_HEIGHT;
    snprintf(line, sizeof(line), "UP:%lus", (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000U));
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, line, STATUS_UI_LCD_TEXT, STATUS_UI_LCD_TEXT_SCALE);

#ifdef CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME
    y += STATUS_UI_LCD_LINE_HEIGHT + 2;
    lcd_draw_text(STATUS_UI_LCD_LEFT_PAD, y, CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME, STATUS_UI_LCD_DIM, 1);
#endif

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
    esp_err_t err = board_i2c_init();
    if (err != ESP_OK) {
        return err;
    }
    err = m5pm1_enable_lcd_power(BOARD_I2C_PORT, BOARD_M5PM1_ADDR);
    if (err != ESP_OK) {
        return err;
    }

    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << BOARD_LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&bl_conf);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(BOARD_LCD_BL_GPIO, !BOARD_LCD_BL_ON_LEVEL);

    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SCLK_GPIO,
        .mosi_io_num = BOARD_LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t),
    };
    err = spi_bus_initialize(BOARD_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
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
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(s_panel_io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    err = esp_lcd_panel_reset(s_panel);
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(s_panel);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_invert_color(s_panel, true);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_set_gap(s_panel, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_disp_on_off(s_panel, true);
    }
    if (err != ESP_OK) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BOARD_LCD_HOST);
        return err;
    }

    s_framebuffer = heap_caps_malloc(BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_framebuffer == NULL) {
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
    ESP_LOGI(TAG, "LCD debug UI ready: ST7789P3 %dx%d gap=%d,%d MOSI=%d SCLK=%d DC=%d CS=%d RST=%d BL=%d",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP, BOARD_LCD_MOSI_GPIO, BOARD_LCD_SCLK_GPIO,
             BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO, BOARD_LCD_RST_GPIO, BOARD_LCD_BL_GPIO);
    return ESP_OK;
}
#endif

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers)
{
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
