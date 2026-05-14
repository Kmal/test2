#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAY_TEXT_MAX_REGIONS 32
#define DISPLAY_TEXT_MAX_TEXT    96
#define DISPLAY_TEXT_MAX_LINES   8

typedef enum {
    DISPLAY_TEXT_REGION_STATUS_TIME = 0,
    DISPLAY_TEXT_REGION_STATUS_WIFI,
    DISPLAY_TEXT_REGION_STATUS_BATTERY,
    DISPLAY_TEXT_REGION_TITLE,
    DISPLAY_TEXT_REGION_BODY_ROW_0,
    DISPLAY_TEXT_REGION_BODY_ROW_1,
    DISPLAY_TEXT_REGION_BODY_ROW_2,
    DISPLAY_TEXT_REGION_BODY_ROW_3,
    DISPLAY_TEXT_REGION_BODY_ROW_4,
    DISPLAY_TEXT_REGION_BODY_ROW_5,
    DISPLAY_TEXT_REGION_BODY_ROW_6,
    DISPLAY_TEXT_REGION_BODY_ROW_7,
    DISPLAY_TEXT_REGION_BODY_DYNAMIC,
    DISPLAY_TEXT_REGION_BOTTOM_HINT,
    DISPLAY_TEXT_REGION_TOAST,
    DISPLAY_TEXT_REGION_KEYBOARD_TITLE,
    DISPLAY_TEXT_REGION_KEYBOARD_INPUT,
    DISPLAY_TEXT_REGION_KEYBOARD_KEY,
} display_text_region_id_t;

typedef enum {
    DISPLAY_TEXT_FIT_ONE_LINE = 0,
    DISPLAY_TEXT_FIT_WRAP,
    DISPLAY_TEXT_FIT_MARQUEE,
    DISPLAY_TEXT_FIT_PAGE
} display_text_fit_t;

typedef enum {
    DISPLAY_TEXT_ALIGN_LEFT = 0,
    DISPLAY_TEXT_ALIGN_CENTER,
    DISPLAY_TEXT_ALIGN_RIGHT
} display_text_align_t;

typedef enum {
    DISPLAY_TEXT_COLLISION_REJECT = 0,
    DISPLAY_TEXT_COLLISION_OVERLAY,
    DISPLAY_TEXT_COLLISION_REPLACE_LOWER_PRIORITY
} display_text_collision_t;

typedef enum {
    DISPLAY_TEXT_PRIORITY_LOW = 0,
    DISPLAY_TEXT_PRIORITY_NORMAL,
    DISPLAY_TEXT_PRIORITY_HIGH,
    DISPLAY_TEXT_PRIORITY_OVERLAY
} display_text_priority_t;

typedef void (*display_text_fill_rect_fn)(
    int x,
    int y,
    int w,
    int h,
    uint16_t color,
    void *ctx
);

typedef struct {
    int x;
    int y;
    int width;
    int height;
    uint8_t scale;
    uint16_t color;
    uint16_t bg_color;
    bool clear_bg;

    display_text_region_id_t region_id;
    display_text_fit_t fit;
    display_text_align_t align;
    display_text_collision_t collision;
    display_text_priority_t priority;
} display_text_box_t;

typedef struct {
    display_text_region_id_t region_id;
    uint32_t text_hash;
    char text[DISPLAY_TEXT_MAX_TEXT];

    bool active;
    bool seen_this_frame;

    uint16_t scroll_px;
    uint8_t page_index;
    uint32_t last_step_ms;
} display_text_region_state_t;

typedef struct {
    display_text_region_id_t region_id;
    int x;
    int y;
    int width;
    int height;
    display_text_priority_t priority;
} display_text_drawn_region_t;

typedef struct {
    int screen_width;
    int screen_height;

    display_text_fill_rect_fn fill_rect;
    void *draw_ctx;

    uint32_t frame_tick_ms;

    display_text_region_state_t regions[DISPLAY_TEXT_MAX_REGIONS];
    size_t region_count;

    display_text_drawn_region_t drawn[DISPLAY_TEXT_MAX_REGIONS];
    size_t drawn_count;
} display_text_context_t;

typedef struct {
    bool drawn;
    bool all_visible_now;
    bool all_reachable;
    bool sanitized;
    bool wrapped;
    bool scrolled;
    bool paged;
    bool collision;
    size_t source_len;
    size_t sanitized_len;
    size_t visible_chars;
} display_text_result_t;

void display_text_context_init(
    display_text_context_t *ctx,
    int screen_width,
    int screen_height,
    display_text_fill_rect_fn fill_rect,
    void *draw_ctx
);

void display_text_begin_frame(
    display_text_context_t *ctx,
    uint32_t tick_ms
);

void display_text_end_frame(
    display_text_context_t *ctx
);

display_text_result_t display_text_put(
    display_text_context_t *ctx,
    const display_text_box_t *box,
    const char *text
);

int display_text_measure_width(
    const char *text,
    uint8_t scale
);

int display_text_char_width(
    uint8_t scale
);

int display_text_line_height(
    uint8_t scale
);

size_t display_text_max_chars_for_width(
    int width_px,
    uint8_t scale
);

size_t display_text_sanitize(
    const char *src,
    char *dst,
    size_t dst_len,
    bool *changed
);
