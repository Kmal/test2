#include "status_lcd.h"

#include "sdkconfig.h"

#if CONFIG_APP_STATUS_UI_LCD
#include <stdint.h>

#include "board_i2c.h"
#include "board_sticks3.h"
#include "m5pm1.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define STATUS_UI_LCD_TASK_STACK 4096
#define STATUS_UI_LCD_TASK_PRIORITY 4
#ifdef CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#define STATUS_UI_LCD_REFRESH_MS CONFIG_APP_STATUS_UI_LCD_REFRESH_MS
#else
#define STATUS_UI_LCD_REFRESH_MS 100
#endif
#define UI_COLOR_BG 0x0000

static const char *TAG = "STATUS_LCD";

static esp_lcd_panel_io_handle_t s_panel_io;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_framebuffer;
static display_text_context_t s_display_text;
static status_lcd_render_cb_t s_render_cb;
static void *s_render_ctx;

bool status_lcd_is_ready(void)
{
    return s_panel != NULL && s_framebuffer != NULL;
}

void status_lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (s_framebuffer == NULL) return;
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

static void status_lcd_display_text_fill_rect(int x, int y, int w, int h, uint16_t color, void *ctx)
{
    (void)ctx;
    status_lcd_fill_rect(x, y, w, h, color);
}

display_text_result_t status_lcd_text_put_box(
    display_text_region_id_t region_id,
    int x,
    int y,
    int w,
    int h,
    const char *text,
    uint16_t color,
    display_text_fit_t fit,
    display_text_align_t align,
    display_text_priority_t priority
)
{
    display_text_box_t box = {
        .x = x,
        .y = y,
        .width = w,
        .height = h,
        .scale = 1,
        .color = color,
        .bg_color = UI_COLOR_BG,
        .clear_bg = false,
        .region_id = region_id,
        .fit = fit,
        .align = align,
        .collision = priority == DISPLAY_TEXT_PRIORITY_OVERLAY ? DISPLAY_TEXT_COLLISION_OVERLAY : DISPLAY_TEXT_COLLISION_REJECT,
        .priority = priority,
    };
    return display_text_put(&s_display_text, &box, text != NULL ? text : "");
}

static void status_lcd_render_frame(void)
{
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    display_text_begin_frame(&s_display_text, now_ms);
    if (s_render_cb != NULL) {
        s_render_cb(s_render_ctx);
    }
    display_text_end_frame(&s_display_text);

    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BOARD_LCD_H_RES, BOARD_LCD_V_RES, s_framebuffer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD draw failed: %s", esp_err_to_name(err));
    }
}

static void status_lcd_task(void *arg)
{
    (void)arg;

    while (true) {
        status_lcd_render_frame();
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_LCD_REFRESH_MS));
    }
}

esp_err_t status_lcd_init(status_lcd_render_cb_t render_cb, void *ctx)
{
    s_render_cb = render_cb;
    s_render_ctx = ctx;
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

    display_text_context_init(
        &s_display_text,
        BOARD_LCD_H_RES,
        BOARD_LCD_V_RES,
        status_lcd_display_text_fill_rect,
        NULL
    );

    BaseType_t created = xTaskCreate(status_lcd_task, "status_ui_lcd",
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
bool status_lcd_is_ready(void) { return false; }
void status_lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
display_text_result_t status_lcd_text_put_box(display_text_region_id_t region_id, int x, int y, int w, int h, const char *text, uint16_t color, display_text_fit_t fit, display_text_align_t align, display_text_priority_t priority)
{
    (void)region_id; (void)x; (void)y; (void)w; (void)h; (void)text; (void)color; (void)fit; (void)align; (void)priority;
    return (display_text_result_t){0};
}
esp_err_t status_lcd_init(status_lcd_render_cb_t render_cb, void *ctx)
{
    (void)render_cb; (void)ctx;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
