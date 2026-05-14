#ifndef STATUS_LCD_H
#define STATUS_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "display_text.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*status_lcd_render_cb_t)(void *ctx);

esp_err_t status_lcd_init(status_lcd_render_cb_t render_cb, void *ctx);
bool status_lcd_is_ready(void);
void status_lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
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
);

#ifdef __cplusplus
}
#endif

#endif // STATUS_LCD_H
