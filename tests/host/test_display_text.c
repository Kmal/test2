#include "display_text.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int fills;
} fake_draw_t;

static void fake_fill_rect(int x, int y, int w, int h, uint16_t color, void *ctx)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
    ((fake_draw_t *)ctx)->fills++;
}

static void test_measurement(void)
{
    assert(display_text_char_width(1) == 6);
    assert(display_text_line_height(1) == 8);
    assert(display_text_measure_width("abc", 1) == 17);
    assert(display_text_max_chars_for_width(127, 1) == 21);
}

static void test_sanitize_utf8_and_invalid_bytes(void)
{
    char out[DISPLAY_TEXT_MAX_TEXT];
    bool changed = false;
    const char text[] = "smart \xe2\x80\x9cquote\xe2\x80\x9d \xe2\x80\x94 \xe2\x86\x92 \xe2\x9c\x93 \xc2\xb0 \xff";
    display_text_sanitize(text, out, sizeof(out), &changed);
    assert(changed);
    assert(strcmp(out, "smart \"quote\" - -> v deg ?") == 0);

    changed = false;
    const char incomplete[] = "bad \xe2\x80";
    size_t sanitized_len = display_text_sanitize(incomplete, out, sizeof(out), &changed);
    assert(changed);
    assert(sanitized_len == 6u);
    assert(strcmp(out, "bad ??") == 0);

    char small[5];
    changed = false;
    sanitized_len = display_text_sanitize("\xc2\xb0\xc2\xb0", small, sizeof(small), &changed);
    assert(changed);
    assert(sanitized_len == 6u);
    assert(strcmp(small, "degd") == 0);
}

static void test_overlay_draws_when_region_log_is_full(void)
{
    fake_draw_t draw = {0};
    display_text_context_t ctx;
    display_text_context_init(&ctx, 135, 240, fake_fill_rect, &draw);
    display_text_begin_frame(&ctx, 0);

    for (size_t i = 0; i < DISPLAY_TEXT_MAX_REGIONS; ++i) {
        display_text_box_t box = {
            .x = (int)i * 3,
            .y = 0,
            .width = 2,
            .height = 8,
            .scale = 1,
            .color = 0xffff,
            .region_id = DISPLAY_TEXT_REGION_BODY_DYNAMIC,
            .fit = DISPLAY_TEXT_FIT_ONE_LINE,
            .align = DISPLAY_TEXT_ALIGN_LEFT,
            .collision = DISPLAY_TEXT_COLLISION_REJECT,
            .priority = DISPLAY_TEXT_PRIORITY_NORMAL,
        };
        display_text_result_t result = display_text_put(&ctx, &box, "x");
        assert(result.drawn);
    }

    display_text_box_t overlay = {
        .x = 0,
        .y = 20,
        .width = 20,
        .height = 8,
        .scale = 1,
        .color = 0xffff,
        .region_id = DISPLAY_TEXT_REGION_TOAST,
        .fit = DISPLAY_TEXT_FIT_ONE_LINE,
        .align = DISPLAY_TEXT_ALIGN_LEFT,
        .collision = DISPLAY_TEXT_COLLISION_OVERLAY,
        .priority = DISPLAY_TEXT_PRIORITY_OVERLAY,
    };
    display_text_result_t overlay_result = display_text_put(&ctx, &overlay, "OK");
    assert(overlay_result.drawn);
    assert(!overlay_result.collision);
}

static void test_marquee_and_wrapping_results(void)
{
    fake_draw_t draw = {0};
    display_text_context_t ctx;
    display_text_context_init(&ctx, 135, 240, fake_fill_rect, &draw);
    display_text_begin_frame(&ctx, 0);

    display_text_box_t marquee = {
        .x = 4,
        .y = 10,
        .width = 127,
        .height = 16,
        .scale = 1,
        .color = 0xffff,
        .region_id = DISPLAY_TEXT_REGION_BODY_ROW_0,
        .fit = DISPLAY_TEXT_FIT_MARQUEE,
        .align = DISPLAY_TEXT_ALIGN_LEFT,
        .collision = DISPLAY_TEXT_COLLISION_REJECT,
        .priority = DISPLAY_TEXT_PRIORITY_NORMAL,
    };
    display_text_result_t result = display_text_put(&ctx, &marquee, "Long selected menu item with enough text to scroll");
    assert(result.drawn);
    assert(!result.all_visible_now);
    assert(result.all_reachable);
    assert(result.scrolled);

    display_text_box_t wrapped = marquee;
    wrapped.y = 40;
    wrapped.height = 16;
    wrapped.region_id = DISPLAY_TEXT_REGION_BODY_ROW_1;
    wrapped.fit = DISPLAY_TEXT_FIT_PAGE;
    result = display_text_put(&ctx, &wrapped, "http://example.local/path/with/a/very/long/url/value");
    assert(result.drawn);
    assert(!result.all_visible_now);
    assert(result.all_reachable);
    assert(result.wrapped);
    assert(result.paged);

    display_text_box_t ssid = marquee;
    ssid.y = 70;
    ssid.region_id = DISPLAY_TEXT_REGION_BODY_ROW_2;
    result = display_text_put(&ctx, &ssid, "VeryLongSSID_Name_With_More_Characters_Than_The_LCD_Row");
    assert(result.drawn);
    assert(result.scrolled);

    display_text_box_t bottom_hint = marquee;
    bottom_hint.y = 220;
    bottom_hint.region_id = DISPLAY_TEXT_REGION_BOTTOM_HINT;
    result = display_text_put(&ctx, &bottom_hint, "K1 SEL K2 NEXT 2x PREV HOLD BACK");
    assert(result.drawn);
    assert(result.scrolled);

    display_text_box_t toast = bottom_hint;
    toast.region_id = DISPLAY_TEXT_REGION_TOAST;
    toast.collision = DISPLAY_TEXT_COLLISION_OVERLAY;
    toast.priority = DISPLAY_TEXT_PRIORITY_OVERLAY;
    toast.fit = DISPLAY_TEXT_FIT_WRAP;
    result = display_text_put(&ctx, &toast, "Saved successfully with a longer toast message");
    assert(result.drawn);

    display_text_box_t narrow = marquee;
    narrow.x = 120;
    narrow.y = 130;
    narrow.width = 6;
    narrow.height = 16;
    narrow.region_id = DISPLAY_TEXT_REGION_BODY_ROW_3;
    narrow.fit = DISPLAY_TEXT_FIT_PAGE;
    result = display_text_put(&ctx, &narrow, "abcdefghijklmnopqrstuvwxyz");
    assert(result.drawn);
    assert(!result.all_reachable);
    assert(result.paged);

    display_text_box_t too_long = marquee;
    too_long.y = 100;
    too_long.region_id = DISPLAY_TEXT_REGION_BODY_ROW_4;
    result = display_text_put(&ctx, &too_long, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
    assert(result.drawn);
    assert(!result.all_reachable);
    assert(result.scrolled);

    display_text_box_t collide = marquee;
    collide.region_id = DISPLAY_TEXT_REGION_BODY_ROW_5;
    result = display_text_put(&ctx, &collide, "collision");
    assert(!result.drawn);
    assert(result.collision);

    display_text_end_frame(&ctx);
    assert(draw.fills > 0);
}

int main(void)
{
    test_measurement();
    test_sanitize_utf8_and_invalid_bytes();
    test_overlay_draws_when_region_log_is_full();
    test_marquee_and_wrapping_results();
    return 0;
}
