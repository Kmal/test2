#include "display_text.h"

#include <string.h>

#define DISPLAY_TEXT_MARQUEE_STEP_MS 200u
#define DISPLAY_TEXT_PAGE_STEP_MS 1600u
#define DISPLAY_TEXT_MARQUEE_GAP_CHARS 3u

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
    static const uint8_t glyph_lower_a[7] = {0, 0, 14, 1, 15, 17, 15};
    static const uint8_t glyph_lower_b[7] = {16, 16, 22, 25, 17, 17, 30};
    static const uint8_t glyph_lower_c[7] = {0, 0, 14, 16, 16, 17, 14};
    static const uint8_t glyph_lower_d[7] = {1, 1, 13, 19, 17, 17, 15};
    static const uint8_t glyph_lower_e[7] = {0, 0, 14, 17, 31, 16, 14};
    static const uint8_t glyph_lower_f[7] = {6, 9, 8, 28, 8, 8, 8};
    static const uint8_t glyph_lower_g[7] = {0, 0, 15, 17, 15, 1, 14};
    static const uint8_t glyph_lower_h[7] = {16, 16, 22, 25, 17, 17, 17};
    static const uint8_t glyph_lower_i[7] = {4, 0, 12, 4, 4, 4, 14};
    static const uint8_t glyph_lower_j[7] = {2, 0, 6, 2, 2, 18, 12};
    static const uint8_t glyph_lower_k[7] = {16, 16, 18, 20, 24, 20, 18};
    static const uint8_t glyph_lower_l[7] = {12, 4, 4, 4, 4, 4, 14};
    static const uint8_t glyph_lower_m[7] = {0, 0, 26, 21, 21, 21, 21};
    static const uint8_t glyph_lower_n[7] = {0, 0, 22, 25, 17, 17, 17};
    static const uint8_t glyph_lower_o[7] = {0, 0, 14, 17, 17, 17, 14};
    static const uint8_t glyph_lower_p[7] = {0, 0, 30, 17, 30, 16, 16};
    static const uint8_t glyph_lower_q[7] = {0, 0, 13, 19, 15, 1, 1};
    static const uint8_t glyph_lower_r[7] = {0, 0, 22, 25, 16, 16, 16};
    static const uint8_t glyph_lower_s[7] = {0, 0, 15, 16, 14, 1, 30};
    static const uint8_t glyph_lower_t[7] = {8, 8, 28, 8, 8, 9, 6};
    static const uint8_t glyph_lower_u[7] = {0, 0, 17, 17, 17, 19, 13};
    static const uint8_t glyph_lower_v[7] = {0, 0, 17, 17, 17, 10, 4};
    static const uint8_t glyph_lower_w[7] = {0, 0, 17, 17, 21, 21, 10};
    static const uint8_t glyph_lower_x[7] = {0, 0, 17, 10, 4, 10, 17};
    static const uint8_t glyph_lower_y[7] = {0, 0, 17, 17, 15, 1, 14};
    static const uint8_t glyph_lower_z[7] = {0, 0, 31, 2, 4, 8, 31};
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
    static const uint8_t glyph_lt[7] = {1, 2, 4, 8, 4, 2, 1};
    static const uint8_t glyph_quote[7] = {10, 10, 0, 0, 0, 0, 0};
    static const uint8_t glyph_apos[7] = {4, 4, 0, 0, 0, 0, 0};
    static const uint8_t glyph_comma[7] = {0, 0, 0, 0, 0, 4, 8};

    /* Keep the rendered glyph case-sensitive. The 9-key input cycle stores
     * lowercase and uppercase letters distinctly (for example 2/a/b/c/A/B/C),
     * so case folding would make the input field display a/b/c as A/B/C
     * even though the backing text is lowercase. */
    switch (c) {
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
    case 'a': return glyph_lower_a;
    case 'b': return glyph_lower_b;
    case 'c': return glyph_lower_c;
    case 'd': return glyph_lower_d;
    case 'e': return glyph_lower_e;
    case 'f': return glyph_lower_f;
    case 'g': return glyph_lower_g;
    case 'h': return glyph_lower_h;
    case 'i': return glyph_lower_i;
    case 'j': return glyph_lower_j;
    case 'k': return glyph_lower_k;
    case 'l': return glyph_lower_l;
    case 'm': return glyph_lower_m;
    case 'n': return glyph_lower_n;
    case 'o': return glyph_lower_o;
    case 'p': return glyph_lower_p;
    case 'q': return glyph_lower_q;
    case 'r': return glyph_lower_r;
    case 's': return glyph_lower_s;
    case 't': return glyph_lower_t;
    case 'u': return glyph_lower_u;
    case 'v': return glyph_lower_v;
    case 'w': return glyph_lower_w;
    case 'x': return glyph_lower_x;
    case 'y': return glyph_lower_y;
    case 'z': return glyph_lower_z;
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
    case '<': return glyph_lt;
    case '"': return glyph_quote;
    case '\'': return glyph_apos;
    case ',': return glyph_comma;
    default: return blank;
    }
}

static bool display_text_is_supported_ascii(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == ' ' || c == ':' || c == '-' ||
           c == '.' || c == '/' || c == '%' || c == '@' || c == '_' ||
           c == '+' || c == '?' || c == '(' || c == ')' || c == '#' ||
           c == '$' || c == '&' || c == '=' || c == '!' || c == '^' ||
           c == '*' || c == '>' || c == '<' || c == '"' || c == '\'' ||
           c == ',';
}

static uint32_t display_text_hash(const char *text)
{
    uint32_t hash = 2166136261u;
    if (text == NULL) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= 16777619u;
    }
    return hash;
}

static display_text_region_state_t *display_text_get_region(
    display_text_context_t *ctx,
    display_text_region_id_t region_id
)
{
    if (ctx == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < ctx->region_count; ++i) {
        if (ctx->regions[i].region_id == region_id) {
            return &ctx->regions[i];
        }
    }
    if (ctx->region_count >= DISPLAY_TEXT_MAX_REGIONS) {
        return NULL;
    }
    display_text_region_state_t *state = &ctx->regions[ctx->region_count++];
    memset(state, 0, sizeof(*state));
    state->region_id = region_id;
    return state;
}

static bool display_text_boxes_overlap(
    const display_text_box_t *a,
    const display_text_drawn_region_t *b
)
{
    if (a == NULL || b == NULL || a->width <= 0 || a->height <= 0 || b->width <= 0 || b->height <= 0) {
        return false;
    }
    return a->x < b->x + b->width && a->x + a->width > b->x &&
           a->y < b->y + b->height && a->y + a->height > b->y;
}

static bool display_text_register_drawn_region(
    display_text_context_t *ctx,
    const display_text_box_t *box
)
{
    if (ctx == NULL || box == NULL) {
        return false;
    }
    for (size_t i = 0; i < ctx->drawn_count; ++i) {
        display_text_drawn_region_t *drawn = &ctx->drawn[i];
        if (!display_text_boxes_overlap(box, drawn)) {
            continue;
        }
        if (box->collision == DISPLAY_TEXT_COLLISION_OVERLAY) {
            continue;
        }
        if (box->collision == DISPLAY_TEXT_COLLISION_REPLACE_LOWER_PRIORITY && box->priority > drawn->priority) {
            continue;
        }
        return false;
    }
    if (ctx->drawn_count >= DISPLAY_TEXT_MAX_REGIONS) {
        return box->collision == DISPLAY_TEXT_COLLISION_OVERLAY || box->priority == DISPLAY_TEXT_PRIORITY_OVERLAY;
    }
    display_text_drawn_region_t *drawn = &ctx->drawn[ctx->drawn_count++];
    drawn->region_id = box->region_id;
    drawn->x = box->x;
    drawn->y = box->y;
    drawn->width = box->width;
    drawn->height = box->height;
    drawn->priority = box->priority;
    return true;
}

static void display_text_reset_region_if_changed(
    display_text_region_state_t *state,
    const char *sanitized_text,
    uint32_t now_ms
)
{
    if (state == NULL) {
        return;
    }
    uint32_t hash = display_text_hash(sanitized_text);
    if (!state->active || state->text_hash != hash || strncmp(state->text, sanitized_text, sizeof(state->text)) != 0) {
        state->text_hash = hash;
        state->scroll_px = 0;
        state->page_index = 0;
        state->last_step_ms = now_ms;
        strncpy(state->text, sanitized_text != NULL ? sanitized_text : "", sizeof(state->text) - 1u);
        state->text[sizeof(state->text) - 1u] = '\0';
    }
    state->active = true;
    state->seen_this_frame = true;
}

static size_t append_char(char *dst, size_t out, size_t dst_len, char c, bool *changed)
{
    if (out + 1u < dst_len) {
        dst[out] = c;
    } else if (changed != NULL) {
        *changed = true;
    }
    return out + 1u;
}

static size_t append_str(char *dst, size_t out, size_t dst_len, const char *s, bool *changed)
{
    while (s != NULL && *s != '\0') {
        out = append_char(dst, out, dst_len, *s++, changed);
    }
    return out;
}

size_t display_text_sanitize(const char *src, char *dst, size_t dst_len, bool *changed)
{
    size_t out = 0;
    bool did_change = false;
    if (dst_len == 0u) {
        if (changed != NULL) *changed = src != NULL && src[0] != '\0';
        return 0;
    }
    if (src == NULL) {
        dst[0] = '\0';
        if (changed != NULL) *changed = false;
        return 0;
    }

    const size_t src_len = strlen(src);
    for (size_t i = 0; i < src_len;) {
        const uint8_t c = (uint8_t)src[i];
        if (c >= 0x20u && c <= 0x7eu) {
            if (display_text_is_supported_ascii((char)c)) {
                out = append_char(dst, out, dst_len, (char)c, &did_change);
            } else {
                out = append_char(dst, out, dst_len, '?', &did_change);
                did_change = true;
            }
            ++i;
            continue;
        }
        if (c == '\n' || c == '\r' || c == '\t') {
            out = append_char(dst, out, dst_len, ' ', &did_change);
            did_change = true;
            ++i;
            continue;
        }

        const unsigned char *u = (const unsigned char *)&src[i];
        const size_t remaining = src_len - i;
        const char *map = NULL;
        size_t step = 0;
        if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x80 && (u[2] == 0x98 || u[2] == 0x99)) { map = "'"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x80 && (u[2] == 0x9C || u[2] == 0x9D)) { map = "\""; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x80 && (u[2] == 0x93 || u[2] == 0x94)) { map = "-"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x88 && u[2] == 0x92) { map = "-"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x80 && u[2] == 0xA6) { map = "..."; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x86 && u[2] == 0x90) { map = "<-"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x86 && u[2] == 0x92) { map = "->"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x86 && u[2] == 0x91) { map = "^"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x86 && u[2] == 0x93) { map = "v"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x9C && u[2] == 0x93) { map = "v"; step = 3; }
        else if (remaining >= 3u && u[0] == 0xE2 && u[1] == 0x9C && u[2] == 0x97) { map = "x"; step = 3; }
        else if (remaining >= 2u && u[0] == 0xC2 && u[1] == 0xB0) { map = "deg"; step = 2; }
        if (map != NULL) {
            out = append_str(dst, out, dst_len, map, &did_change);
            did_change = true;
            i += step;
        } else {
            out = append_char(dst, out, dst_len, '?', &did_change);
            did_change = true;
            ++i;
        }
    }
    const size_t terminator = out < dst_len ? out : dst_len - 1u;
    dst[terminator] = '\0';
    if (changed != NULL) {
        *changed = did_change || src_len != out;
    }
    return out;
}

int display_text_char_width(uint8_t scale)
{
    return 6 * (int)(scale == 0u ? 1u : scale);
}

static int display_text_visible_char_width(uint8_t scale)
{
    return 5 * (int)(scale == 0u ? 1u : scale);
}

int display_text_line_height(uint8_t scale)
{
    return 8 * (int)(scale == 0u ? 1u : scale);
}

int display_text_measure_width(const char *text, uint8_t scale)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    return (int)len * display_text_char_width(scale) - (display_text_char_width(scale) - display_text_visible_char_width(scale));
}

size_t display_text_max_chars_for_width(int width_px, uint8_t scale)
{
    if (width_px <= 0) {
        return 0;
    }
    int cw = display_text_char_width(scale);
    int visible = display_text_visible_char_width(scale);
    return (size_t)((width_px + cw - visible) / cw);
}

static size_t copy_span(char *dst, size_t dst_len, const char *src, size_t start, size_t len)
{
    if (dst_len == 0u) return 0;
    if (len >= dst_len) len = dst_len - 1u;
    memcpy(dst, src + start, len);
    dst[len] = '\0';
    return len;
}

static size_t display_text_build_wrapped_lines(
    const char *text,
    int box_width,
    uint8_t scale,
    char lines[DISPLAY_TEXT_MAX_LINES][DISPLAY_TEXT_MAX_TEXT],
    size_t line_capacity
)
{
    if (line_capacity > DISPLAY_TEXT_MAX_LINES) line_capacity = DISPLAY_TEXT_MAX_LINES;
    for (size_t i = 0; i < line_capacity; ++i) lines[i][0] = '\0';
    if (text == NULL) return 0;
    size_t max_chars = display_text_max_chars_for_width(box_width, scale);
    if (max_chars == 0u) max_chars = 1u;
    size_t stored_count = 0;
    size_t total_count = 0;
    size_t pos = 0;
    size_t len = strlen(text);
    while (pos < len) {
        while (pos < len && text[pos] == ' ') ++pos;
        if (pos >= len) break;
        size_t remaining = len - pos;
        size_t take = remaining < max_chars ? remaining : max_chars;
        if (take < remaining) {
            size_t break_at = take;
            while (break_at > 0u && text[pos + break_at] != ' ' && text[pos + break_at - 1u] != ' ') --break_at;
            if (break_at > 0u) {
                take = break_at;
            }
        }
        while (take > 0u && text[pos + take - 1u] == ' ') --take;
        if (take == 0u) take = remaining < max_chars ? remaining : max_chars;
        if (stored_count < line_capacity) {
            copy_span(lines[stored_count++], DISPLAY_TEXT_MAX_TEXT, text, pos, take);
        }
        ++total_count;
        pos += take;
    }
    return total_count;
}

static void display_text_draw_char(
    display_text_context_t *ctx,
    int x,
    int y,
    char c,
    uint16_t color,
    uint8_t scale
)
{
    /* Raw glyph primitive. Do not use for user-visible UI text.
     * Use display_text_put() with an explicit display_text_box_t instead.
     */
    if (ctx == NULL || ctx->fill_rect == NULL) return;
    if (scale == 0u) scale = 1u;
    const uint8_t *rows = glyph_rows(c);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1U << (4 - col))) != 0) {
                ctx->fill_rect(x + col * scale, y + row * scale, scale, scale, color, ctx->draw_ctx);
            }
        }
    }
}

static void display_text_draw_raw_line(
    display_text_context_t *ctx,
    int x,
    int y,
    const char *text,
    uint16_t color,
    uint8_t scale
)
{
    /* Raw glyph primitive. Do not use for user-visible UI text.
     * Use display_text_put() with an explicit display_text_box_t instead.
     */
    int cursor_x = x;
    int cw = display_text_char_width(scale);
    while (ctx != NULL && text != NULL && *text != '\0' && cursor_x < ctx->screen_width) {
        if (*text != ' ') {
            display_text_draw_char(ctx, cursor_x, y, *text, color, scale);
        }
        cursor_x += cw;
        ++text;
    }
}

void display_text_context_init(
    display_text_context_t *ctx,
    int screen_width,
    int screen_height,
    display_text_fill_rect_fn fill_rect,
    void *draw_ctx
)
{
    if (ctx == NULL) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->screen_width = screen_width;
    ctx->screen_height = screen_height;
    ctx->fill_rect = fill_rect;
    ctx->draw_ctx = draw_ctx;
}

void display_text_begin_frame(display_text_context_t *ctx, uint32_t tick_ms)
{
    if (ctx == NULL) return;
    ctx->frame_tick_ms = tick_ms;
    ctx->drawn_count = 0;
    for (size_t i = 0; i < ctx->region_count; ++i) {
        ctx->regions[i].seen_this_frame = false;
    }
}

void display_text_end_frame(display_text_context_t *ctx)
{
    if (ctx == NULL) return;
    for (size_t i = 0; i < ctx->region_count; ++i) {
        if (!ctx->regions[i].seen_this_frame) {
            ctx->regions[i].active = false;
        }
    }
}

static int aligned_x(const display_text_box_t *box, int text_width)
{
    if (box->align == DISPLAY_TEXT_ALIGN_RIGHT) {
        return box->x + box->width - text_width;
    }
    if (box->align == DISPLAY_TEXT_ALIGN_CENTER) {
        return box->x + (box->width - text_width) / 2;
    }
    return box->x;
}

static void draw_line_clipped(display_text_context_t *ctx, const display_text_box_t *box, int y, const char *text, size_t start_chars, size_t max_chars)
{
    char clipped[DISPLAY_TEXT_MAX_TEXT];
    size_t len = text != NULL ? strlen(text) : 0u;
    if (start_chars > len) start_chars = len;
    size_t take = len - start_chars;
    if (take > max_chars) take = max_chars;
    copy_span(clipped, sizeof(clipped), text != NULL ? text : "", start_chars, take);
    int x = aligned_x(box, display_text_measure_width(clipped, box->scale));
    if (box->align == DISPLAY_TEXT_ALIGN_LEFT) x = box->x;
    display_text_draw_raw_line(ctx, x, y, clipped, box->color, box->scale);
}

display_text_result_t display_text_put(
    display_text_context_t *ctx,
    const display_text_box_t *box,
    const char *text
)
{
    display_text_result_t result = { .all_visible_now = true, .all_reachable = true };
    if (ctx == NULL || box == NULL || ctx->fill_rect == NULL || box->width <= 0 || box->height <= 0) {
        return result;
    }
    result.source_len = text != NULL ? strlen(text) : 0u;
    char sanitized[DISPLAY_TEXT_MAX_TEXT];
    bool changed = false;
    result.sanitized_len = display_text_sanitize(text != NULL ? text : "", sanitized, sizeof(sanitized), &changed);
    result.sanitized = changed;
    const size_t draw_len = strlen(sanitized);
    const bool content_truncated = result.sanitized_len != draw_len;

    display_text_region_state_t *state = display_text_get_region(ctx, box->region_id);
    display_text_reset_region_if_changed(state, sanitized, ctx->frame_tick_ms);

    if (!display_text_register_drawn_region(ctx, box)) {
        result.collision = true;
        return result;
    }
    if (box->clear_bg) {
        ctx->fill_rect(box->x, box->y, box->width, box->height, box->bg_color, ctx->draw_ctx);
    }

    int text_width = display_text_measure_width(sanitized, box->scale);
    size_t max_chars = display_text_max_chars_for_width(box->width, box->scale);
    if (text_width <= box->width || box->fit == DISPLAY_TEXT_FIT_ONE_LINE) {
        result.all_visible_now = text_width <= box->width && !content_truncated;
        result.all_reachable = (text_width <= box->width || box->fit != DISPLAY_TEXT_FIT_ONE_LINE) && !content_truncated;
        result.visible_chars = text_width <= box->width ? draw_len : max_chars;
        draw_line_clipped(ctx, box, box->y, sanitized, 0, max_chars);
        result.drawn = true;
        return result;
    }

    if (box->fit == DISPLAY_TEXT_FIT_MARQUEE) {
        if (state != NULL && ctx->frame_tick_ms - state->last_step_ms >= DISPLAY_TEXT_MARQUEE_STEP_MS) {
            uint32_t steps = (ctx->frame_tick_ms - state->last_step_ms) / DISPLAY_TEXT_MARQUEE_STEP_MS;
            state->scroll_px = (uint16_t)(state->scroll_px + steps * (uint32_t)display_text_char_width(box->scale));
            state->last_step_ms += steps * DISPLAY_TEXT_MARQUEE_STEP_MS;
        }
        size_t gap = DISPLAY_TEXT_MARQUEE_GAP_CHARS;
        size_t cycle_chars = draw_len + gap;
        size_t start = cycle_chars > 0u && state != NULL ? (state->scroll_px / display_text_char_width(box->scale)) % cycle_chars : 0u;
        if (box->region_id == DISPLAY_TEXT_REGION_KEYBOARD_INPUT && state != NULL &&
            state->scroll_px == 0u && state->last_step_ms == ctx->frame_tick_ms &&
            draw_len > max_chars) {
            start = draw_len - max_chars;
        }
        char loop[DISPLAY_TEXT_MAX_TEXT];
        size_t out = 0;
        for (size_t i = 0; i < max_chars && out + 1u < sizeof(loop); ++i) {
            size_t idx = (start + i) % cycle_chars;
            loop[out++] = idx < draw_len ? sanitized[idx] : ' ';
        }
        loop[out] = '\0';
        display_text_draw_raw_line(ctx, box->x, box->y, loop, box->color, box->scale);
        result.drawn = true;
        result.all_visible_now = false;
        result.all_reachable = !content_truncated;
        result.scrolled = true;
        result.visible_chars = out;
        return result;
    }

    char lines[DISPLAY_TEXT_MAX_LINES][DISPLAY_TEXT_MAX_TEXT];
    size_t line_count = display_text_build_wrapped_lines(sanitized, box->width, box->scale, lines, DISPLAY_TEXT_MAX_LINES);
    size_t stored_line_count = line_count < DISPLAY_TEXT_MAX_LINES ? line_count : DISPLAY_TEXT_MAX_LINES;
    size_t lines_per_page = (size_t)(box->height / display_text_line_height(box->scale));
    if (lines_per_page == 0u) lines_per_page = 1u;
    result.wrapped = true;
    result.visible_chars = 0;
    size_t first_line = 0;
    if (line_count > lines_per_page) {
        result.all_visible_now = false;
        result.all_reachable = !content_truncated && line_count <= DISPLAY_TEXT_MAX_LINES;
        result.paged = true;
        if (state != NULL && ctx->frame_tick_ms - state->last_step_ms >= DISPLAY_TEXT_PAGE_STEP_MS) {
            uint32_t steps = (ctx->frame_tick_ms - state->last_step_ms) / DISPLAY_TEXT_PAGE_STEP_MS;
            size_t pages = (stored_line_count + lines_per_page - 1u) / lines_per_page;
            state->page_index = (uint8_t)((state->page_index + steps) % (pages == 0u ? 1u : pages));
            state->last_step_ms += steps * DISPLAY_TEXT_PAGE_STEP_MS;
        }
        first_line = state != NULL ? (size_t)state->page_index * lines_per_page : 0u;
        if (first_line >= stored_line_count) first_line = 0;
    } else if (content_truncated || line_count > DISPLAY_TEXT_MAX_LINES) {
        result.all_visible_now = false;
        result.all_reachable = false;
    }
    for (size_t i = 0; i < lines_per_page && first_line + i < stored_line_count; ++i) {
        int y = box->y + (int)i * display_text_line_height(box->scale);
        int x = aligned_x(box, display_text_measure_width(lines[first_line + i], box->scale));
        display_text_draw_raw_line(ctx, x, y, lines[first_line + i], box->color, box->scale);
        result.visible_chars += strlen(lines[first_line + i]);
    }
    result.drawn = true;
    return result;
}
