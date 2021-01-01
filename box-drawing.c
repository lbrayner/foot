#include "box-drawing.h"

#include <stdio.h>
#include <errno.h>

#define LOG_MODULE "box-drawing"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "macros.h"
#include "stride.h"
#include "terminal.h"
#include "util.h"
#include "xmalloc.h"

#define LIGHT 1.0
#define HEAVY 2.0

#pragma GCC optimize("Os")

static int
thickness(float pts, int dpi)
{
    return max(pts * (float)dpi / 72.0, 1);
}

static void
_hline(uint8_t *buf, int x1, int x2, int y, int thick, int width, int height, int stride)
{
    x1 = min(max(x1, 0), width);
    x2 = min(max(x2, 0), width);

    for (size_t row = max(y, 0); row < min(y + thick, height); row++) {
        for (size_t col = x1; col < x2; col++) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

#define hline(x1, x2, y, thick) _hline(buf, x1, x2, y, thick, width, height, stride)

static void
_vline(uint8_t *buf, int y1, int y2, int x, int thick, int width, int height, int stride)
{
    y1 = min(max(y1, 0), height);
    y2 = min(max(y2, 0), height);

    for (size_t row = y1; row < y2; row++) {
        for (size_t col = max(x, 0); col < min(x + thick, width); col++) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

#define vline(y1, y2, x, thick) _vline(buf, y1, y2, x, thick, width, height, stride)

static void
_rect(uint8_t *buf, int x1, int y1, int x2, int y2, int width, int height, int stride)
{
    for (size_t row = max(y1, 0); row < min(y2, height); row++) {
        for (size_t col = max(x1, 0); col < min(x2, width); col++) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

#define rect(x1, y1, x2, y2) _rect(buf, x1, y1, x2, y2, width, height, stride)


#define _hline_middle_left(_vthick, _hthick)                            \
    do {                                                                \
        int vthick = thickness(_vthick, dpi);                           \
        int hthick = thickness(_hthick, dpi);                           \
        hline(0, (width + vthick) / 2, (height - hthick) / 2, hthick); \
    } while (0)

#define _hline_middle_right(_vthick, _hthick)                           \
    do {                                                                \
        int vthick = thickness(_vthick, dpi);                           \
        int hthick = thickness(_hthick, dpi);                           \
        hline((width - vthick) / 2, width, (height - hthick) / 2, hthick); \
    } while (0)

#define _vline_middle_up(_vthick, _hthick)                              \
    do {                                                                \
        int vthick = thickness(_vthick, dpi);                           \
        int hthick = thickness(_hthick, dpi);                           \
        vline(0, (height + hthick) / 2, (width - vthick) / 2, vthick); \
    } while (0)

#define _vline_middle_down(_vthick, _hthick)                            \
    do {                                                                \
        int vthick = thickness(_vthick, dpi);                           \
        int hthick = thickness(_hthick, dpi);                           \
        vline((height - hthick) / 2, height, (width - vthick) / 2, vthick); \
    } while (0)

#define hline_middle_left(thick) _hline_middle_left(thick, thick)
#define hline_middle_right(thick) _hline_middle_right(thick, thick)
#define vline_middle_up(thick) _vline_middle_up(thick, thick)
#define vline_middle_down(thick) _vline_middle_down(thick, thick)

#define quad_upper_left()  rect(0, 0, ceil(width / 2.), ceil(height / 2.))
#define quad_upper_right() rect(floor(width / 2.), 0, width, ceil(height / 2.))
#define quad_lower_left()  rect(0, floor(height / 2.), ceil(width / 2.), height)
#define quad_lower_right() rect(floor(width / 2.), floor(height / 2.), width, height)

static void
draw_box_drawings_light_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
}

static void
draw_box_drawings_heavy_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
}

static void
draw_box_drawings_light_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_heavy_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi, int count, int thick, int gap)
{
    assert(count >= 2 && count <= 4);
    const int gap_count = count - 1;

    int dash_width = (width - (gap_count * gap)) / count;
    while (dash_width <= 0 && gap > 1) {
        gap--;
        dash_width = (width - (gap_count * gap)) / count;
    }

    if (dash_width <= 0) {
        hline_middle_left(LIGHT);
        hline_middle_right(LIGHT);
        return;
    }

    assert(count * dash_width + gap_count * gap <= width);

    int remaining = width - count * dash_width - gap_count * gap;

    int x[4] = {0};
    int w[4] = {dash_width, dash_width, dash_width, dash_width};

    x[0] = 0;

    x[1] = x[0] + w[0] + gap;
    if (count == 2)
        w[1] = width - x[1];
    else if (count == 3)
        w[1] += remaining;
    else
        w[1] += remaining / 2;

    if (count >= 3) {
        x[2] = x[1] + w[1] + gap;
        if (count == 3)
            w[2] = width - x[2];
        else
            w[2] += remaining - remaining / 2;
    }

    if (count >= 4) {
        x[3] = x[2] + w[2] + gap;
        w[3] = width - x[3];
    }

    hline(x[0], x[0] + w[0], (height - thick) / 2, thick);
    hline(x[1], x[1] + w[1], (height - thick) / 2, thick);
    if (count >= 3)
        hline(x[2], x[2] + w[2], (height - thick) / 2, thick);
    if (count >= 4)
        hline(x[3], x[3] + w[3], (height - thick) / 2, thick);
}

static void
draw_box_drawings_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi, int count, int thick, int gap)
{
    assert(count >= 2 && count <= 4);
    const int gap_count = count - 1;

    int dash_height = (height - (gap_count * gap)) / count;
    while (dash_height <= 0 && gap > 1) {
        gap--;
        dash_height = (height - (gap_count * gap)) / count;
    }

    if (dash_height <= 0) {
        vline_middle_up(LIGHT);
        vline_middle_down(LIGHT);
        return;
    }

    assert(count * dash_height + gap_count * gap <= height);

    int remaining = height - count * dash_height - gap_count * gap;

    int y[4] = {0};
    int h[4] = {dash_height, dash_height, dash_height, dash_height};

    y[0] = 0;

    y[1] = y[0] + h[0] + gap;
    if (count == 2)
        h[1] = height - y[1];
    else if (count == 3)
        h[1] += remaining;
    else
        h[1] += remaining / 2;

    if (count >= 3) {
        y[2] = y[1] + h[1] + gap;
        if (count == 3)
            h[2] = height - y[2];
        else
            h[2] += remaining - remaining / 2;
    }

    if (count >= 4) {
        y[3] = y[2] + h[2] + gap;
        h[3] = height - y[3];
    }

    vline(y[0], y[0] + h[0], (width - thick) / 2, thick);
    vline(y[1], y[1] + h[1], (width - thick) / 2, thick);
    if (count >= 3)
        vline(y[2], y[2] + h[2], (width - thick) / 2, thick);
    if (count >= 4)
        vline(y[3], y[3] + h[3], (width - thick) / 2, thick);
}

static void
draw_box_drawings_light_triple_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 3, thickness(LIGHT, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_heavy_triple_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 3, thickness(HEAVY, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_light_triple_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 3, thickness(LIGHT, dpi), thickness(HEAVY, dpi));
}

static void
draw_box_drawings_heavy_triple_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 3, thickness(HEAVY, dpi), thickness(HEAVY, dpi));
}

static void
draw_box_drawings_light_quadruple_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 4, thickness(LIGHT, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_heavy_quadruple_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 4, thickness(HEAVY, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_light_quadruple_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 4, thickness(LIGHT, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_heavy_quadruple_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 4, thickness(HEAVY, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_light_down_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_light_and_right_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_right_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_heavy_down_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_down_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_light_and_left_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_left_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_heavy_down_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_up_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_light_and_right_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_right_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
}

static void
draw_box_drawings_heavy_up_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_light_up_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_light_and_left_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_left_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
}

static void
draw_box_drawings_heavy_up_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_light_vertical_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_vertical_light_and_right_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_right_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_right_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_vertical_heavy_and_right_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_down_light_and_right_up_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_light_and_right_down_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_heavy_vertical_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_vertical_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_vertical_light_and_left_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_left_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_left_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    vline_middle_up(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_vertical_heavy_and_left_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_down_light_and_left_up_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_light_and_left_down_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_heavy_vertical_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_down_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_left_heavy_and_right_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_right_heavy_and_left_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_light_and_horizontal_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_horizontal_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_right_light_and_left_down_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_left_light_and_right_down_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_heavy_down_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_up_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_left_heavy_and_right_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_right_heavy_and_left_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_light_and_horizontal_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_horizontal_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
}

static void
draw_box_drawings_right_light_and_left_up_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_left_light_and_right_up_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_heavy_up_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_light_vertical_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_left_heavy_and_right_vertical_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    _hline_middle_left(LIGHT, HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_right_heavy_and_left_vertical_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    _hline_middle_right(LIGHT, HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_vertical_light_and_horizontal_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_heavy_and_down_horizontal_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    _vline_middle_up(HEAVY, LIGHT);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_down_heavy_and_up_horizontal_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    _vline_middle_down(HEAVY, LIGHT);
}

static void
draw_box_drawings_vertical_heavy_and_horizontal_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_left_up_heavy_and_right_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_right_up_heavy_and_left_down_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_left_down_heavy_and_right_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_right_down_heavy_and_left_up_light(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_down_light_and_up_horizontal_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_up_light_and_down_horizontal_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_right_light_and_left_vertical_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_left_light_and_right_vertical_heavy(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_heavy_vertical_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(HEAVY);
    vline_middle_up(HEAVY);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_double_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 2, thickness(LIGHT, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_heavy_double_dash_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_horizontal(
        buf, width, height, stride, dpi, 2, thickness(HEAVY, dpi), thickness(LIGHT, dpi));
}

static void
draw_box_drawings_light_double_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 2, thickness(LIGHT, dpi), thickness(HEAVY, dpi));
}

static void
draw_box_drawings_heavy_double_dash_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_dash_vertical(
        buf, width, height, stride, dpi, 2, thickness(HEAVY, dpi), thickness(HEAVY, dpi));
}

static void
draw_box_drawings_double_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int mid = (height - thick * 3) / 2;

    hline(0, width, mid, thick);
    hline(0, width, mid + 2 * thick, thick);
}

static void
draw_box_drawings_double_vertical(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int mid = (width - thick * 3) / 2;

    vline(0, height, mid, thick);
    vline(0, height, mid + 2 * thick, thick);
}

static void
draw_box_drawings_down_single_and_right_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick) / 2;

    vline_middle_down(LIGHT);

    hline(vmid, width, hmid, thick);
    hline(vmid, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_down_double_and_right_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_right(LIGHT);

    vline(hmid, height, vmid, thick);
    vline(hmid, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_down_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(hmid, height, vmid, thick);
    vline(hmid + 2 * thick, height, vmid + 2 * thick, thick);

    hline(vmid, width, hmid, thick);
    hline(vmid + 2 * thick, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_down_single_and_left_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width + thick) / 2;

    vline_middle_down(LIGHT);

    hline(0, vmid, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_down_double_and_left_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_left(LIGHT);

    vline(hmid, height, vmid, thick);
    vline(hmid, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_down_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(hmid + 2 * thick, height, vmid, thick);
    vline(hmid, height, vmid + 2 * thick, thick);

    hline(0, vmid + 2 * thick, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_single_and_right_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick) / 2;

    vline_middle_up(LIGHT);

    hline(vmid, width, hmid, thick);
    hline(vmid, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_double_and_right_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height + thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_right(LIGHT);

    vline(0, hmid, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_up_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(0, hmid + 2 * thick, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);

    hline(vmid + 2 * thick, width, hmid, thick);
    hline(vmid, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_single_and_left_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width + thick) / 2;

    vline_middle_up(LIGHT);

    hline(0, vmid, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_double_and_left_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height + thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_left(LIGHT);

    vline(0, hmid, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_up_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(0, hmid + 0 * thick + thick, vmid, thick);
    vline(0, hmid + 2 * thick + thick, vmid + 2 * thick, thick);

    hline(0, vmid, hmid, thick);
    hline(0, vmid + 2 * thick, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_single_and_right_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick) / 2;

    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);

    hline(vmid, width, hmid, thick);
    hline(vmid, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_double_and_right_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int vmid = (width - thick * 3) / 2;

    hline(vmid + 2 * thick, width, (height - thick) / 2, thick);

    vline(0, height, vmid, thick);
    vline(0, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_vertical_and_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(0, height, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);
    vline(hmid + 2 * thick, height, vmid + 2 * thick, thick);

    hline(vmid + 2 * thick, width, hmid, thick);
    hline(vmid + 2 * thick, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_single_and_left_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width + thick) / 2;

    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);

    hline(0, vmid, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_double_and_left_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int vmid = (width - thick * 3) / 2;

    hline(0, vmid, (height - thick) / 2, thick);

    vline(0, height, vmid, thick);
    vline(0, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_vertical_and_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(0, height, vmid + 2 * thick, thick);
    vline(0, hmid, vmid, thick);
    vline(hmid + 2 * thick, height, vmid, thick);

    hline(0, vmid + thick, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_down_single_and_horizontal_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;

    vline(hmid + 2 * thick, height, (width - thick) / 2, thick);

    hline(0, width, hmid, thick);
    hline(0, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_down_double_and_horizontal_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);

    vline(hmid, height, vmid, thick);
    vline(hmid, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_down_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    hline(0, width, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
    hline(vmid + 2 * thick, width, hmid + 2 * thick, thick);

    vline(hmid + 2 * thick, height, vmid, thick);
    vline(hmid + 2 * thick, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_single_and_horizontal_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick) / 2;

    vline(0, hmid, vmid, thick);

    hline(0, width, hmid, thick);
    hline(0, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_up_double_and_horizontal_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick) / 2;
    int vmid = (width - thick * 3) / 2;

    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);

    vline(0, hmid, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_up_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    vline(0, hmid, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);

    hline(0, vmid + thick, hmid, thick);
    hline(vmid + 2 * thick, width, hmid, thick);
    hline(0, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_single_and_horizontal_double(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;

    vline_middle_up(LIGHT);
    vline_middle_down(LIGHT);

    hline(0, width, hmid, thick);
    hline(0, width, hmid + 2 * thick, thick);
}

static void
draw_box_drawings_vertical_double_and_horizontal_single(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int vmid = (width - thick * 3) / 2;

    hline_middle_left(LIGHT);
    hline_middle_right(LIGHT);

    vline(0, height, vmid, thick);
    vline(0, height, vmid + 2 * thick, thick);
}

static void
draw_box_drawings_double_vertical_and_horizontal(uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int hmid = (height - thick * 3) / 2;
    int vmid = (width - thick * 3) / 2;

    hline(0, vmid, hmid, thick);
    hline(vmid + 2 * thick, width, hmid, thick);
    hline(0, vmid, hmid + 2 * thick, thick);
    hline(vmid + 2 * thick, width, hmid + 2 * thick, thick);

    vline(0, hmid, vmid, thick);
    vline(0, hmid, vmid + 2 * thick, thick);
    vline(hmid + 2 * thick, height, vmid, thick);
    vline(hmid + 2 * thick, height, vmid + 2 * thick, thick);
}

#define cubic_bezier_x(t) __extension__ \
    ({                                                                  \
        double tm1 = 1 - t;                                             \
        double tm1_3 = tm1 * tm1 * tm1;                                 \
        double t_3 = t * t * t;                                         \
        tm1_3 * start_x + 3 * t * tm1 * (tm1 * c1_x + t * c2_x) + t_3 * end_x; \
    })

#define cubic_bezier_y(t) __extension__                                       \
    ({                                                                  \
        double tm1 = 1 - t;                                             \
        double tm1_3 = tm1 * tm1 * tm1;                                 \
        double t_3 = t * t * t;                                         \
        tm1_3 * start_y + 3 * t * tm1 * (tm1 * c1_y + t * c2_y) + t_3 * end_y; \
    })

static void
draw_box_drawings_light_arc(wchar_t wc, uint8_t *buf, int width, int height, int stride, int dpi)
{
    int thick = thickness(LIGHT, dpi);
    int delta = thick / 2;
    int extra = thick % 2;

    int hw = (width - thick) / 2;
    int hh = (height - thick) / 2;
    int start_x, start_y, end_x, end_y, c1_x, c1_y, c2_x, c2_y;

    if (wc == L'╭') {
        start_x = hw;
        start_y = 2 * hh;

        end_x = 2 * hw;
        end_y = hh;

        c1_x = hw;
        c1_y = 3 * height / 4;

        c2_x = hw;
        c2_y = hh;
    } else if (wc == L'╮') {
        start_x = hw;
        start_y = 2 * hh;

        end_x = 0;
        end_y = hh;

        c1_x = hw;
        c1_y = 3 * height / 4;

        c2_x = hw;
        c2_y = hh;
    } else if (wc == L'╯') {
        start_x = hw;
        start_y = 0;

        end_x = 0;
        end_y = hh;

        c1_x = hw;
        c1_y = height / 4;

        c2_x = hw;
        c2_y = hh;
    } else {
        assert(wc == L'╰');

        start_x = hw;
        start_y = 0;

        end_x = 2 * hw;
        end_y = hh;

        c1_x = hw;
        c1_y = height / 4;

        c2_x = hw;
        c2_y = hh;
    }

    int num_samples = height * 4;

    for (size_t i = 0; i < num_samples + 1; i++) {
        double t = (double)i / num_samples;
        int p_x = round(cubic_bezier_x(t));
        int p_y = round(cubic_bezier_y(t));

        for (int y = max(p_y - delta, 0); y < min(p_y + delta + extra, height); y++) {
            for (int x = max(p_x - delta, 0); x < min(p_x + delta + extra, width); x++) {
                size_t ofs = x / 8;
                size_t bit_no = x % 8;
                buf[y * stride + ofs] |= 1 << bit_no;
            }
        }
    }

    if (wc == L'╭' || wc == L'╰') {
        for (int x = 2 * hw; x < width; x++) {
            for (int y = max(hh - delta, 0); y < min(hh + delta + extra, height); y++) {
                size_t ofs = x / 8;
                size_t bit_no = x % 8;
                buf[y * stride + ofs] |= 1 << bit_no;
            }
        }
    }

    if (wc == L'╭' || wc == L'╮') {
        for (int y = 2 * hh; y < height; y++) {
            for (int x = max(hw - delta, 0); x < min(hw + delta + extra, width); x++) {
                size_t ofs = x / 8;
                size_t bit_no = x % 8;
                buf[y * stride + ofs] |= 1 << bit_no;
            }
        }
    }
}

static void
draw_box_drawings_light_diagonal(uint8_t *buf, int width, int height, int stride, int dpi, double k, double c)
{
#define linear_equation(x) (k * (x) + c)

    int num_samples = width * 16;

    for (int i = 0; i < num_samples; i++) {
        double x = i / 16.;
        int col = round(x);
        int row = round(linear_equation(x));

        if (row >= 0 && row < height) {
            size_t ofs = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + ofs] |= 1 << bit_no;
        }
    }

#undef linear_equation
}

static void
draw_box_drawings_light_diagonal_upper_right_to_lower_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    /* y = k * x + c */
    double c = height - 1;
    double k = (0 - (height - 1)) / (double)(width - 1 - 0);
    draw_box_drawings_light_diagonal(buf, width, height, stride, dpi, k, c);
}

static void
draw_box_drawings_light_diagonal_upper_left_to_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    /* y = k * x + c */
    double c = 0;
    double k = (height - 1 - 0) / (double)(width - 1 - 0);
    draw_box_drawings_light_diagonal(buf, width, height, stride, dpi, k, c);
}

static void
draw_box_drawings_light_diagonal_cross(uint8_t *buf, int width, int height, int stride, int dpi)
{
    draw_box_drawings_light_diagonal_upper_right_to_lower_left(buf, width, height, stride, dpi);
    draw_box_drawings_light_diagonal_upper_left_to_lower_right(buf, width, height, stride, dpi);
}

static void
draw_box_drawings_light_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
}

static void
draw_box_drawings_light_up(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(LIGHT);
}

static void
draw_box_drawings_light_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(LIGHT);
}

static void
draw_box_drawings_light_down(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_down(LIGHT);
}

static void
draw_box_drawings_heavy_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
}

static void
draw_box_drawings_heavy_up(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(HEAVY);
}

static void
draw_box_drawings_heavy_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_right(HEAVY);
}

static void
draw_box_drawings_heavy_down(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_light_left_and_heavy_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(LIGHT);
    hline_middle_right(HEAVY);
}

static void
draw_box_drawings_light_up_and_heavy_down(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(LIGHT);
    vline_middle_down(HEAVY);
}

static void
draw_box_drawings_heavy_left_and_light_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    hline_middle_left(HEAVY);
    hline_middle_right(LIGHT);
}

static void
draw_box_drawings_heavy_up_and_light_down(uint8_t *buf, int width, int height, int stride, int dpi)
{
    vline_middle_up(HEAVY);
    vline_middle_down(LIGHT);
}

static void
draw_upper_half_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, width, round(height / 2.));
}

static void
draw_lower_one_eighth_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(height / 8.), width, height);
}

static void
draw_lower_one_quarter_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(height / 4.), width, height);
}

static void
draw_lower_three_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(3. * height / 8.), width, height);
}

static void
draw_lower_half_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(height / 2.), width, height);
}

static void
draw_lower_five_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(5. * height / 8.), width, height);
}

static void
draw_lower_three_quarters_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(3. * height / 4.), width, height);
}

static void
draw_lower_seven_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, height - round(7. * height / 8.), width, height);
}

static void
draw_full_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, width, height);
}

static void
draw_left_seven_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(7. * width / 8.), height);
}

static void
draw_left_three_quarters_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(3. * width / 4.), height);
}

static void
draw_left_five_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(5. * width / 8.), height);
}

static void
draw_left_half_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(width / 2.), height);
}

static void
draw_left_three_eighths_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(3. * width / 8.), height);
}

static void
draw_left_one_quarter_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(width / 4.), height);
}

static void
draw_left_one_eighth_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, round(width / 8.), height);
}

static void
draw_right_half_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(round(width / 2.), 0, width, height);
}

static void
draw_light_shade(uint8_t *buf, int width, int height, int stride, int dpi)
{
    for (size_t row = 0; row < height; row += 2) {
        for (size_t col = 0; col < width; col += 2) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

static void
draw_medium_shade(uint8_t *buf, int width, int height, int stride, int dpi)
{
    for (size_t row = 0; row < height; row++) {
        for (size_t col = row % 2; col < width; col += 2) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

static void
draw_dark_shade(uint8_t *buf, int width, int height, int stride, int dpi)
{
    for (size_t row = 0; row < height; row++) {
        for (size_t col = 0; col < width; col += 1 + row % 2) {
            size_t idx = col / 8;
            size_t bit_no = col % 8;
            buf[row * stride + idx] |= 1 << bit_no;
        }
    }
}

static void
draw_upper_one_eighth_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(0, 0, width, round(height / 8.));
}

static void
draw_right_one_eighth_block(uint8_t *buf, int width, int height, int stride, int dpi)
{
    rect(width - round(width / 8.), 0, width, height);
}

static void
draw_quadrant_lower_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_lower_left();
}

static void
draw_quadrant_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_lower_right();
}

static void
draw_quadrant_upper_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_left();
}

static void
draw_quadrant_upper_left_and_lower_left_and_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_left();
    quad_lower_left();
    quad_lower_right();
}

static void
draw_quadrant_upper_left_and_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_left();
    quad_lower_right();
}

static void
draw_quadrant_upper_left_and_upper_right_and_lower_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_left();
    quad_upper_right();
    quad_lower_left();
}

static void
draw_quadrant_upper_left_and_upper_right_and_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_left();
    quad_upper_right();
    quad_lower_right();
}

static void
draw_quadrant_upper_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_right();
}

static void
draw_quadrant_upper_right_and_lower_left(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_right();
    quad_lower_left();
}

static void
draw_quadrant_upper_right_and_lower_left_and_lower_right(uint8_t *buf, int width, int height, int stride, int dpi)
{
    quad_upper_right();
    quad_lower_left();
    quad_lower_right();
}

#define sextant_upper_left() rect(0, 0, round(width / 2.), round(height / 3.))
#define sextant_middle_left() rect(0, height / 3, round(width / 2.), round(2. * height / 3.))
#define sextant_lower_left() rect(0, 2 * height / 3, round(width / 2.), height)
#define sextant_upper_right() rect(width / 2, 0, width, round(height / 3.))
#define sextant_middle_right() rect(width / 2, height / 3, width, round(2. * height / 3.))
#define sextant_lower_right() rect(width / 2, 2 * height / 3, width, height)

static void
draw_sextant(wchar_t wc, uint8_t *buf, int width, int height, int stride, int dpi)
{
    /*
     * Each byte encodes one sextant:
     *
     * Bit     sextant
     *   0      upper left
     *   1      middle left
     *   2      lower left
     *   3      upper right
     *   4      middle right
     *   5      lower right
     */
#define UPPER_LEFT (1 << 0)
#define MIDDLE_LEFT (1 << 1)
#define LOWER_LEFT (1 << 2)
#define UPPER_RIGHT (1 << 3)
#define MIDDLE_RIGHT (1 << 4)
#define LOWER_RIGHT (1 << 5)

    static const uint8_t matrix[60] = {
        /* U+1fb00 - U+1fb0f */
        UPPER_LEFT,
        UPPER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT,
        MIDDLE_LEFT,
        UPPER_LEFT | MIDDLE_LEFT,
        UPPER_RIGHT | MIDDLE_LEFT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT,
        MIDDLE_RIGHT,
        UPPER_LEFT | MIDDLE_RIGHT,
        UPPER_RIGHT | MIDDLE_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_RIGHT,
        MIDDLE_LEFT | MIDDLE_RIGHT,
        UPPER_LEFT | MIDDLE_LEFT | MIDDLE_RIGHT,
        UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT,
        LOWER_LEFT,

        /* U+1fb10 - U+1fb1f */
        UPPER_LEFT | LOWER_LEFT,
        UPPER_RIGHT | LOWER_LEFT,
        UPPER_LEFT | UPPER_RIGHT | LOWER_LEFT,
        MIDDLE_LEFT | LOWER_LEFT,
        UPPER_RIGHT | MIDDLE_LEFT | LOWER_LEFT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | LOWER_LEFT,
        MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_LEFT | MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_RIGHT | MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_RIGHT | LOWER_LEFT,
        MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_LEFT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT,
        LOWER_RIGHT,
        UPPER_LEFT | LOWER_RIGHT,

        /* U+1fb20 - U+1fb2f */
        UPPER_RIGHT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | LOWER_RIGHT,
        MIDDLE_LEFT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_LEFT | LOWER_RIGHT,
        UPPER_RIGHT | MIDDLE_LEFT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | LOWER_RIGHT,
        MIDDLE_RIGHT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_RIGHT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_RIGHT | LOWER_RIGHT,
        MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_RIGHT,
        UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_RIGHT,
        LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_RIGHT | LOWER_LEFT | LOWER_RIGHT,

        /* U+1fb30 - U+1fb3b */
        UPPER_LEFT | UPPER_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        MIDDLE_LEFT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_LEFT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_RIGHT | MIDDLE_LEFT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_LEFT | LOWER_LEFT | LOWER_RIGHT,
        MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_RIGHT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | UPPER_RIGHT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_LEFT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
        UPPER_RIGHT | MIDDLE_LEFT | MIDDLE_RIGHT | LOWER_LEFT | LOWER_RIGHT,
    };

    assert(wc >= 0x1fb00 && wc <= 0x1fb3b);
    const size_t idx = wc - 0x1fb00;

    assert(idx < ALEN(matrix));
    uint8_t encoded = matrix[idx];

    if (encoded & UPPER_LEFT)
        sextant_upper_left();

    if (encoded & MIDDLE_LEFT)
        sextant_middle_left();

    if (encoded & LOWER_LEFT)
        sextant_lower_left();

    if (encoded & UPPER_RIGHT)
        sextant_upper_right();

    if (encoded & MIDDLE_RIGHT)
        sextant_middle_right();

    if (encoded & LOWER_RIGHT)
        sextant_lower_right();
}

static void
draw_glyph(wchar_t wc, uint8_t *buf, int width, int height, int stride, int dpi)
{
#if defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wpedantic"
#endif

    switch (wc) {
    case 0x2500: draw_box_drawings_light_horizontal(buf, width, height, stride, dpi); break;
    case 0x2501: draw_box_drawings_heavy_horizontal(buf, width, height, stride, dpi); break;
    case 0x2502: draw_box_drawings_light_vertical(buf, width, height, stride, dpi); break;
    case 0x2503: draw_box_drawings_heavy_vertical(buf, width, height, stride, dpi); break;
    case 0x2504: draw_box_drawings_light_triple_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x2505: draw_box_drawings_heavy_triple_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x2506: draw_box_drawings_light_triple_dash_vertical(buf, width, height, stride, dpi); break;
    case 0x2507: draw_box_drawings_heavy_triple_dash_vertical(buf, width, height, stride, dpi); break;
    case 0x2508: draw_box_drawings_light_quadruple_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x2509: draw_box_drawings_heavy_quadruple_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x250a: draw_box_drawings_light_quadruple_dash_vertical(buf, width, height, stride, dpi); break;
    case 0x250b: draw_box_drawings_heavy_quadruple_dash_vertical(buf, width, height, stride, dpi); break;
    case 0x250c: draw_box_drawings_light_down_and_right(buf, width, height, stride, dpi); break;
    case 0x250d: draw_box_drawings_down_light_and_right_heavy(buf, width, height, stride, dpi); break;
    case 0x250e: draw_box_drawings_down_heavy_and_right_light(buf, width, height, stride, dpi); break;
    case 0x250f: draw_box_drawings_heavy_down_and_right(buf, width, height, stride, dpi); break;

    case 0x2510: draw_box_drawings_light_down_and_left(buf, width, height, stride, dpi); break;
    case 0x2511: draw_box_drawings_down_light_and_left_heavy(buf, width, height, stride, dpi); break;
    case 0x2512: draw_box_drawings_down_heavy_and_left_light(buf, width, height, stride, dpi); break;
    case 0x2513: draw_box_drawings_heavy_down_and_left(buf, width, height, stride, dpi); break;
    case 0x2514: draw_box_drawings_light_up_and_right(buf, width, height, stride, dpi); break;
    case 0x2515: draw_box_drawings_up_light_and_right_heavy(buf, width, height, stride, dpi); break;
    case 0x2516: draw_box_drawings_up_heavy_and_right_light(buf, width, height, stride, dpi); break;
    case 0x2517: draw_box_drawings_heavy_up_and_right(buf, width, height, stride, dpi); break;
    case 0x2518: draw_box_drawings_light_up_and_left(buf, width, height, stride, dpi); break;
    case 0x2519: draw_box_drawings_up_light_and_left_heavy(buf, width, height, stride, dpi); break;
    case 0x251a: draw_box_drawings_up_heavy_and_left_light(buf, width, height, stride, dpi); break;
    case 0x251b: draw_box_drawings_heavy_up_and_left(buf, width, height, stride, dpi); break;
    case 0x251c: draw_box_drawings_light_vertical_and_right(buf, width, height, stride, dpi); break;
    case 0x251d: draw_box_drawings_vertical_light_and_right_heavy(buf, width, height, stride, dpi); break;
    case 0x251e: draw_box_drawings_up_heavy_and_right_down_light(buf, width, height, stride, dpi); break;
    case 0x251f: draw_box_drawings_down_heavy_and_right_up_light(buf, width, height, stride, dpi); break;

    case 0x2520: draw_box_drawings_vertical_heavy_and_right_light(buf, width, height, stride, dpi); break;
    case 0x2521: draw_box_drawings_down_light_and_right_up_heavy(buf, width, height, stride, dpi); break;
    case 0x2522: draw_box_drawings_up_light_and_right_down_heavy(buf, width, height, stride, dpi); break;
    case 0x2523: draw_box_drawings_heavy_vertical_and_right(buf, width, height, stride, dpi); break;
    case 0x2524: draw_box_drawings_light_vertical_and_left(buf, width, height, stride, dpi); break;
    case 0x2525: draw_box_drawings_vertical_light_and_left_heavy(buf, width, height, stride, dpi); break;
    case 0x2526: draw_box_drawings_up_heavy_and_left_down_light(buf, width, height, stride, dpi); break;
    case 0x2527: draw_box_drawings_down_heavy_and_left_up_light(buf, width, height, stride, dpi); break;
    case 0x2528: draw_box_drawings_vertical_heavy_and_left_light(buf, width, height, stride, dpi); break;
    case 0x2529: draw_box_drawings_down_light_and_left_up_heavy(buf, width, height, stride, dpi); break;
    case 0x252a: draw_box_drawings_up_light_and_left_down_heavy(buf, width, height, stride, dpi); break;
    case 0x252b: draw_box_drawings_heavy_vertical_and_left(buf, width, height, stride, dpi); break;
    case 0x252c: draw_box_drawings_light_down_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x252d: draw_box_drawings_left_heavy_and_right_down_light(buf, width, height, stride, dpi); break;
    case 0x252e: draw_box_drawings_right_heavy_and_left_down_light(buf, width, height, stride, dpi); break;
    case 0x252f: draw_box_drawings_down_light_and_horizontal_heavy(buf, width, height, stride, dpi); break;

    case 0x2530: draw_box_drawings_down_heavy_and_horizontal_light(buf, width, height, stride, dpi); break;
    case 0x2531: draw_box_drawings_right_light_and_left_down_heavy(buf, width, height, stride, dpi); break;
    case 0x2532: draw_box_drawings_left_light_and_right_down_heavy(buf, width, height, stride, dpi); break;
    case 0x2533: draw_box_drawings_heavy_down_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x2534: draw_box_drawings_light_up_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x2535: draw_box_drawings_left_heavy_and_right_up_light(buf, width, height, stride, dpi); break;
    case 0x2536: draw_box_drawings_right_heavy_and_left_up_light(buf, width, height, stride, dpi); break;
    case 0x2537: draw_box_drawings_up_light_and_horizontal_heavy(buf, width, height, stride, dpi); break;
    case 0x2538: draw_box_drawings_up_heavy_and_horizontal_light(buf, width, height, stride, dpi); break;
    case 0x2539: draw_box_drawings_right_light_and_left_up_heavy(buf, width, height, stride, dpi); break;
    case 0x253a: draw_box_drawings_left_light_and_right_up_heavy(buf, width, height, stride, dpi); break;
    case 0x253b: draw_box_drawings_heavy_up_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x253c: draw_box_drawings_light_vertical_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x253d: draw_box_drawings_left_heavy_and_right_vertical_light(buf, width, height, stride, dpi); break;
    case 0x253e: draw_box_drawings_right_heavy_and_left_vertical_light(buf, width, height, stride, dpi); break;
    case 0x253f: draw_box_drawings_vertical_light_and_horizontal_heavy(buf, width, height, stride, dpi); break;

    case 0x2540: draw_box_drawings_up_heavy_and_down_horizontal_light(buf, width, height, stride, dpi); break;
    case 0x2541: draw_box_drawings_down_heavy_and_up_horizontal_light(buf, width, height, stride, dpi); break;
    case 0x2542: draw_box_drawings_vertical_heavy_and_horizontal_light(buf, width, height, stride, dpi); break;
    case 0x2543: draw_box_drawings_left_up_heavy_and_right_down_light(buf, width, height, stride, dpi); break;
    case 0x2544: draw_box_drawings_right_up_heavy_and_left_down_light(buf, width, height, stride, dpi); break;
    case 0x2545: draw_box_drawings_left_down_heavy_and_right_up_light(buf, width, height, stride, dpi); break;
    case 0x2546: draw_box_drawings_right_down_heavy_and_left_up_light(buf, width, height, stride, dpi); break;
    case 0x2547: draw_box_drawings_down_light_and_up_horizontal_heavy(buf, width, height, stride, dpi); break;
    case 0x2548: draw_box_drawings_up_light_and_down_horizontal_heavy(buf, width, height, stride, dpi); break;
    case 0x2549: draw_box_drawings_right_light_and_left_vertical_heavy(buf, width, height, stride, dpi); break;
    case 0x254a: draw_box_drawings_left_light_and_right_vertical_heavy(buf, width, height, stride, dpi); break;
    case 0x254b: draw_box_drawings_heavy_vertical_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x254c: draw_box_drawings_light_double_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x254d: draw_box_drawings_heavy_double_dash_horizontal(buf, width, height, stride, dpi); break;
    case 0x254e: draw_box_drawings_light_double_dash_vertical(buf, width, height, stride, dpi); break;
    case 0x254f: draw_box_drawings_heavy_double_dash_vertical(buf, width, height, stride, dpi); break;

    case 0x2550: draw_box_drawings_double_horizontal(buf, width, height, stride, dpi); break;
    case 0x2551: draw_box_drawings_double_vertical(buf, width, height, stride, dpi); break;
    case 0x2552: draw_box_drawings_down_single_and_right_double(buf, width, height, stride, dpi); break;
    case 0x2553: draw_box_drawings_down_double_and_right_single(buf, width, height, stride, dpi); break;
    case 0x2554: draw_box_drawings_double_down_and_right(buf, width, height, stride, dpi); break;
    case 0x2555: draw_box_drawings_down_single_and_left_double(buf, width, height, stride, dpi); break;
    case 0x2556: draw_box_drawings_down_double_and_left_single(buf, width, height, stride, dpi); break;
    case 0x2557: draw_box_drawings_double_down_and_left(buf, width, height, stride, dpi); break;
    case 0x2558: draw_box_drawings_up_single_and_right_double(buf, width, height, stride, dpi); break;
    case 0x2559: draw_box_drawings_up_double_and_right_single(buf, width, height, stride, dpi); break;
    case 0x255a: draw_box_drawings_double_up_and_right(buf, width, height, stride, dpi); break;
    case 0x255b: draw_box_drawings_up_single_and_left_double(buf, width, height, stride, dpi); break;
    case 0x255c: draw_box_drawings_up_double_and_left_single(buf, width, height, stride, dpi); break;
    case 0x255d: draw_box_drawings_double_up_and_left(buf, width, height, stride, dpi); break;
    case 0x255e: draw_box_drawings_vertical_single_and_right_double(buf, width, height, stride, dpi); break;
    case 0x255f: draw_box_drawings_vertical_double_and_right_single(buf, width, height, stride, dpi); break;

    case 0x2560: draw_box_drawings_double_vertical_and_right(buf, width, height, stride, dpi); break;
    case 0x2561: draw_box_drawings_vertical_single_and_left_double(buf, width, height, stride, dpi); break;
    case 0x2562: draw_box_drawings_vertical_double_and_left_single(buf, width, height, stride, dpi); break;
    case 0x2563: draw_box_drawings_double_vertical_and_left(buf, width, height, stride, dpi); break;
    case 0x2564: draw_box_drawings_down_single_and_horizontal_double(buf, width, height, stride, dpi); break;
    case 0x2565: draw_box_drawings_down_double_and_horizontal_single(buf, width, height, stride, dpi); break;
    case 0x2566: draw_box_drawings_double_down_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x2567: draw_box_drawings_up_single_and_horizontal_double(buf, width, height, stride, dpi); break;
    case 0x2568: draw_box_drawings_up_double_and_horizontal_single(buf, width, height, stride, dpi); break;
    case 0x2569: draw_box_drawings_double_up_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x256a: draw_box_drawings_vertical_single_and_horizontal_double(buf, width, height, stride, dpi); break;
    case 0x256b: draw_box_drawings_vertical_double_and_horizontal_single(buf, width, height, stride, dpi); break;
    case 0x256c: draw_box_drawings_double_vertical_and_horizontal(buf, width, height, stride, dpi); break;
    case 0x256d ... 0x2570: draw_box_drawings_light_arc(wc, buf, width, height, stride, dpi); break;

    case 0x2571: draw_box_drawings_light_diagonal_upper_right_to_lower_left(buf, width, height, stride, dpi); break;
    case 0x2572: draw_box_drawings_light_diagonal_upper_left_to_lower_right(buf, width, height, stride, dpi); break;
    case 0x2573: draw_box_drawings_light_diagonal_cross(buf, width, height, stride, dpi); break;
    case 0x2574: draw_box_drawings_light_left(buf, width, height, stride, dpi); break;
    case 0x2575: draw_box_drawings_light_up(buf, width, height, stride, dpi); break;
    case 0x2576: draw_box_drawings_light_right(buf, width, height, stride, dpi); break;
    case 0x2577: draw_box_drawings_light_down(buf, width, height, stride, dpi); break;
    case 0x2578: draw_box_drawings_heavy_left(buf, width, height, stride, dpi); break;
    case 0x2579: draw_box_drawings_heavy_up(buf, width, height, stride, dpi); break;
    case 0x257a: draw_box_drawings_heavy_right(buf, width, height, stride, dpi); break;
    case 0x257b: draw_box_drawings_heavy_down(buf, width, height, stride, dpi); break;
    case 0x257c: draw_box_drawings_light_left_and_heavy_right(buf, width, height, stride, dpi); break;
    case 0x257d: draw_box_drawings_light_up_and_heavy_down(buf, width, height, stride, dpi); break;
    case 0x257e: draw_box_drawings_heavy_left_and_light_right(buf, width, height, stride, dpi); break;
    case 0x257f: draw_box_drawings_heavy_up_and_light_down(buf, width, height, stride, dpi); break;

    case 0x2580: draw_upper_half_block(buf, width, height, stride, dpi); break;
    case 0x2581: draw_lower_one_eighth_block(buf, width, height, stride, dpi); break;
    case 0x2582: draw_lower_one_quarter_block(buf, width, height, stride, dpi); break;
    case 0x2583: draw_lower_three_eighths_block(buf, width, height, stride, dpi); break;
    case 0x2584: draw_lower_half_block(buf, width, height, stride, dpi); break;
    case 0x2585: draw_lower_five_eighths_block(buf, width, height, stride, dpi); break;
    case 0x2586: draw_lower_three_quarters_block(buf, width, height, stride, dpi); break;
    case 0x2587: draw_lower_seven_eighths_block(buf, width, height, stride, dpi); break;
    case 0x2588: draw_full_block(buf, width, height, stride, dpi); break;
    case 0x2589: draw_left_seven_eighths_block(buf, width, height, stride, dpi); break;
    case 0x258a: draw_left_three_quarters_block(buf, width, height, stride, dpi); break;
    case 0x258b: draw_left_five_eighths_block(buf, width, height, stride, dpi); break;
    case 0x258c: draw_left_half_block(buf, width, height, stride, dpi); break;
    case 0x258d: draw_left_three_eighths_block(buf, width, height, stride, dpi); break;
    case 0x258e: draw_left_one_quarter_block(buf, width, height, stride, dpi); break;
    case 0x258f: draw_left_one_eighth_block(buf, width, height, stride, dpi); break;

    case 0x2590: draw_right_half_block(buf, width, height, stride, dpi); break;
    case 0x2591: draw_light_shade(buf, width, height, stride, dpi); break;
    case 0x2592: draw_medium_shade(buf, width, height, stride, dpi); break;
    case 0x2593: draw_dark_shade(buf, width, height, stride, dpi); break;
    case 0x2594: draw_upper_one_eighth_block(buf, width, height, stride, dpi); break;
    case 0x2595: draw_right_one_eighth_block(buf, width, height, stride, dpi); break;
    case 0x2596: draw_quadrant_lower_left(buf, width, height, stride, dpi); break;
    case 0x2597: draw_quadrant_lower_right(buf, width, height, stride, dpi); break;
    case 0x2598: draw_quadrant_upper_left(buf, width, height, stride, dpi); break;
    case 0x2599: draw_quadrant_upper_left_and_lower_left_and_lower_right(buf, width, height, stride, dpi); break;
    case 0x259a: draw_quadrant_upper_left_and_lower_right(buf, width, height, stride, dpi); break;
    case 0x259b: draw_quadrant_upper_left_and_upper_right_and_lower_left(buf, width, height, stride, dpi); break;
    case 0x259c: draw_quadrant_upper_left_and_upper_right_and_lower_right(buf, width, height, stride, dpi); break;
    case 0x259d: draw_quadrant_upper_right(buf, width, height, stride, dpi); break;
    case 0x259e: draw_quadrant_upper_right_and_lower_left(buf, width, height, stride, dpi); break;
    case 0x259f: draw_quadrant_upper_right_and_lower_left_and_lower_right(buf, width, height, stride, dpi); break;

    case 0x1fb00 ... 0x1fb3b: draw_sextant(wc, buf, width, height, stride, dpi); break;
    }

#if defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif
}

struct fcft_glyph * COLD
box_drawing(const struct terminal *term, wchar_t wc)
{
    int width = term->cell_width;
    int height = term->cell_height;
    int stride = stride_for_format_and_width(PIXMAN_a1, width);
    uint8_t *data = xcalloc(height * stride, 1);

    pixman_image_t *pix = pixman_image_create_bits_no_clear(
        PIXMAN_a1, width, height, (uint32_t*)data, stride);

    if (pix == NULL) {
        errno = ENOMEM;
        perror(__func__);
        abort();
    }

    draw_glyph(wc, data, width, height, stride, term->font_dpi);

    struct fcft_glyph *glyph = xmalloc(sizeof(*glyph));
    *glyph = (struct fcft_glyph){
        .wc = wc,
        .cols = 1,
        .pix = pix,
        .x = 0,
        .y = term->fonts[0]->ascent,
        .width = width,
        .height = height,
        .advance = {
            .x = width,
            .y = height,
        },
    };
    return glyph;
}
