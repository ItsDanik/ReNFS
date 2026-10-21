/* EA's polygon filler (NOTES §4.10): 0x8bc71 and its entries 0x8a04c, 0x8a094. The
   polygon's edges are stepped from its top corner down both ways round, into a left and a
   right x for every row, then the rows are painted.

   An edge (0xa631c, or 0xa632b cut to the window) is stepped along y when it is steep, the
   x in 32.32 rounded each row, and along x when it is shallow, the y in 32.32: each row
   then gets the run of x on it. The slope is the minor over the major length in 0.32, from
   EA's table of 16-bit ones for majors under 50 (0x127ff0, 0x127fec), else rounded.
   An edge cut by the window keeps, besides its part inside, how many rows above and below
   lie wholly left or right of it: those rows get the window's side. */
#include "polyfill.h"
#include "raster.h"
#include <stdint.h>

#define ROWS 0x500             /* the original's two tables on its stack, 0x1400 bytes each */

static int g_left[ROWS], g_right[ROWS];
unsigned char g_shade[256];

/* the window (0xf88a4 ..) while a polygon is filled */
static int g_wx0, g_wy0, g_wx1, g_wy1;

enum { FLAT_BACK, FLAT, VERTICAL, DIAG_LEFT, DIAG_RIGHT, STEEP_LEFT, STEEP_RIGHT,
       SHALLOW_LEFT, SHALLOW_RIGHT, POINT };

/* an edge (the original's 0x38 bytes at ebp-0x290c) */
typedef struct {
    int64_t x;                 /* +0, +4: the top end's x, 32.32 (steep kinds) */
    int64_t y;                 /* +8, +0xc: the top end's y, 32.32 (shallow kinds) */
    int x1, y1;                /* +0x10, +0x14: the bottom end */
    int count;                 /* +0x18: rows (steep) or x steps (shallow) */
    int top_left, bottom_left, top_right, bottom_right; /* +0x20, +0x24, +0x28, +0x2c */
    uint32_t slope;            /* +0x30 */
    int kind;                  /* +0x34: 0/1 flat (0 given right to left), 2 vertical,
                                  3/4 diagonal, 5/6 steep, 7/8 shallow (the odd ones going
                                  left as they go down), 9 a point */
} edge;

static int whole(int64_t v)
{
    return (int)(v >> 32);
}

/* v in 32.32 to the nearest whole, a half up */
static int round32(int64_t v)
{
    return (int)((v + 0x80000000LL) >> 32);
}

static int64_t fixed(int v)
{
    return (int64_t)v * 0x100000000LL;
}

/* n / slope (n 32.32, not below 0), rounded: a half down */
static uint32_t over_slope(uint64_t n, uint32_t slope)
{
    uint32_t q = (uint32_t)(n / slope), r = (uint32_t)(n % slope);
    return q + ((slope >> 1) < r);
}

static uint32_t slope_of(uint32_t minor, uint32_t major)
{
    if (major < 50)
        return ((minor << 16) / major) << 16;  /* the table's 16 bits */
    return over_slope((uint64_t)minor << 32, major);
}

static int64_t times_slope(uint32_t n, uint32_t slope)
{
    return (int64_t)((uint64_t)n * slope);
}

/* the edge wholly outside (0xa681d): code 4 above, 8 below, 2 left, 1 right */
static int outside(edge *e, int code)
{
    int last, top;

    e->count = 0;
    if (code & 4) {
        e->y = fixed(g_wy0);
        e->y1 = g_wy0 - 1;
    } else if (code & 8) {
        e->y = fixed(g_wy1);
    } else {
        last = e->y1 < g_wy1 ? e->y1 : g_wy1 - 1;
        top = round32(e->y);
        if (top < g_wy0)
            top = g_wy0;
        e->y = fixed(top);
        e->y1 = top - 1;
        if (code & 2)
            e->bottom_left += last - top + 1;
        else
            e->bottom_right += last - top + 1;
    }
    return code;
}

static void cut_top(edge *e)
{
    int rows = g_wy0 - whole(e->y);
    uint32_t steps;

    e->y = fixed(g_wy0);
    switch (e->kind) {
    case VERTICAL: e->count -= rows; break;
    case DIAG_LEFT: e->x -= fixed(rows); e->count -= rows; break;
    case DIAG_RIGHT: e->x += fixed(rows); e->count -= rows; break;
    case STEEP_LEFT: e->x -= times_slope((uint32_t)rows, e->slope); e->count -= rows; break;
    case STEEP_RIGHT: e->x += times_slope((uint32_t)rows, e->slope); e->count -= rows; break;
    case SHALLOW_LEFT:
    case SHALLOW_RIGHT:
        e->y = fixed(g_wy0 - rows);
        steps = over_slope((uint64_t)(uint32_t)rows << 32, e->slope);
        e->x += e->kind == SHALLOW_LEFT ? -fixed((int)steps) : fixed((int)steps);
        e->count -= (int)steps;
        if (e->count <= 0) {
            e->count = 1;
            e->y = fixed(g_wy0);
            e->x = fixed(e->x1);
        } else
            e->y += times_slope(steps, e->slope);
        break;
    }
}

static void cut_bottom(edge *e)
{
    int rows = e->y1 - (g_wy1 - 1);
    int64_t d;
    uint32_t steps;

    e->y1 = g_wy1 - 1;
    switch (e->kind) {
    case VERTICAL: e->count -= rows; break;
    case DIAG_LEFT: e->x1 += rows; e->count -= rows; break;
    case DIAG_RIGHT: e->x1 -= rows; e->count -= rows; break;
    case STEEP_LEFT:
        e->count -= rows;
        e->x1 = round32(e->x - times_slope((uint32_t)(e->count - 1), e->slope));
        break;
    case STEEP_RIGHT:
        e->count -= rows;
        e->x1 = round32(e->x + times_slope((uint32_t)(e->count - 1), e->slope));
        break;
    case SHALLOW_LEFT:
    case SHALLOW_RIGHT:
        d = fixed(g_wy1 - 1) - e->y;
        steps = d < 0 ? 0 : over_slope((uint64_t)d, e->slope);
        e->x1 = whole(e->x) + (e->kind == SHALLOW_LEFT ? -(int)steps : (int)steps);
        e->count = (int)steps + 1;
        break;
    }
}

/* the part left of the window cut off; nonzero if none is left */
static int cut_left(edge *e)
{
    int c, old;
    int64_t d;
    uint32_t q;

    switch (e->kind) {
    case VERTICAL:
        return outside(e, 2);
    case DIAG_LEFT:
        c = g_wx0 - e->x1;
        e->x1 = g_wx0;
        e->bottom_left += c;
        e->count -= c;
        e->y1 -= c;
        break;
    case DIAG_RIGHT:
        c = g_wx0 - whole(e->x);
        e->x = fixed(g_wx0);
        e->top_left += c;
        e->y += fixed(c);
        e->count -= c;
        break;
    case STEEP_LEFT:
        e->x1 = g_wx0;
        d = e->x - fixed(g_wx0);
        q = whole(d) < 0 ? 1 : over_slope((uint64_t)d, e->slope) + 1;
        c = e->count - (int)q;
        e->count = (int)q;
        e->bottom_left += c;
        e->y1 = (int)q - 1 + whole(e->y);
        break;
    case STEEP_RIGHT:
        d = fixed(g_wx0) - e->x;
        if (d < 0)
            return outside(e, 2);
        q = over_slope((uint64_t)d, e->slope);
        e->y += fixed((int)q);
        e->top_left += (int)q;
        e->count -= (int)q;
        if (e->count <= 0)
            return outside(e, 2);
        e->x += times_slope(q, e->slope);
        break;
    case SHALLOW_LEFT:
        c = whole(e->x) - g_wx0;
        e->x1 = g_wx0;
        e->count = c + 1;
        old = round32(times_slope((uint32_t)c, e->slope) + e->y);
        e->bottom_left += e->y1 - old;
        e->y1 = old;
        break;
    case SHALLOW_RIGHT:
        c = g_wx0 - whole(e->x);
        e->x = fixed(g_wx0);
        e->count -= c;
        old = round32(e->y);
        e->y += times_slope((uint32_t)c, e->slope);
        e->top_left += round32(e->y) - old;
        break;
    }
    return 0;
}

/* the part right of the window cut off */
static int cut_right(edge *e)
{
    int c, old, last = g_wx1 - 1;
    int64_t d;
    uint32_t q;

    switch (e->kind) {
    case VERTICAL:
        return outside(e, 1);
    case DIAG_LEFT:
        c = whole(e->x) - last;
        e->x = fixed(last);
        e->y += fixed(c);
        e->count -= c;
        e->top_right += c;
        break;
    case DIAG_RIGHT:
        c = e->x1 - last;
        e->x1 = last;
        e->bottom_right += c;
        e->count -= c;
        e->y1 -= c;
        break;
    case STEEP_LEFT:
        q = over_slope((uint64_t)(e->x - fixed(g_wx1 - 1)), e->slope);
        e->count -= (int)q;
        if (e->count <= 0)
            return outside(e, 1);
        e->y += fixed((int)q);
        e->top_right += (int)q;
        e->x -= times_slope(q, e->slope);
        break;
    case STEEP_RIGHT:
        e->x1 = last;
        d = fixed(last) - e->x;
        if (d < 0)
            return outside(e, 1);
        q = over_slope((uint64_t)d, e->slope) + 1;
        c = e->count - (int)q;
        e->count = (int)q;
        e->bottom_right += c;
        e->y1 = (int)q - 1 + whole(e->y);
        break;
    case SHALLOW_LEFT:
        c = whole(e->x) - last;
        e->x = fixed(last);
        e->count -= c;
        old = round32(e->y);
        e->y += times_slope((uint32_t)c, e->slope);
        e->top_right += round32(e->y) - old;
        break;
    case SHALLOW_RIGHT:
        e->x1 = last;
        c = g_wx1 - whole(e->x);
        e->count = c;
        old = round32(times_slope((uint32_t)(c - 1), e->slope) + e->y);
        e->bottom_right += e->y1 - old;
        e->y1 = old;
        break;
    }
    return 0;
}

/* where the edge's ends lie left (2) or right (1) of the window, once cut above and below
   (0xa6a0d); nonzero if it is wholly outside */
static int x_code(edge *e, int *code)
{
    int top = 0, bottom = 0;

    if (whole(e->x) < g_wx0)
        top |= 2;
    if (round32(e->x) >= g_wx1)
        top |= 1;
    if (e->x1 < g_wx0)
        bottom |= 2;
    if (e->x1 >= g_wx1)
        bottom |= 1;
    if (top & bottom) {
        outside(e, top & bottom);
        return 1;
    }
    *code = top | bottom;
    return 0;
}

/* a flat edge, one row (0xa6a79): its run of x cut to the window */
static int edge_flat(edge *e, int x0, int x1, int y, int cut)
{
    int t;

    e->kind = x0 == x1 ? POINT : FLAT;
    if (x0 > x1) {
        e->kind = FLAT_BACK;
        t = x0;
        x0 = x1;
        x1 = t;
        e->x = fixed(x0);
        e->x1 = x1;
    }
    if (!cut) {
        e->count = x1 - x0 + 1;
        return 0;
    }
    if (y < g_wy0 || y >= g_wy1) {
        e->y = fixed(y < g_wy0 ? g_wy0 : g_wy1);
        e->y1 = whole(e->y);
        e->count = 0;
        return y < g_wy0 ? 4 : 8;
    }
    e->count = x1 - x0 + 1;
    if (x1 < g_wx0 || x0 >= g_wx1) {
        e->y1--;
        if (x1 < g_wx0)
            e->bottom_left = 1;
        else
            e->bottom_right = 1;
        e->count = 0;
        return x1 < g_wx0 ? 2 : 1;
    }
    if (g_wx0 > x0) {
        e->x = fixed(g_wx0);
        e->count -= g_wx0 - x0;
    }
    if (x1 > g_wx1 - 1) {
        e->count -= x1 - (g_wx1 - 1);
        e->x1 = g_wx1 - 1;
    }
    return 0;
}

/* an edge from (x0, y0) to (x1, y1), from its top end (0xa631c; cut to the window:
   0xa632b); the code where it lies if wholly outside, else 0 */
static int edge_make(edge *e, int x0, int y0, int x1, int y1, int cut)
{
    int top = 0, bottom = 0, code, dx, dy, t;

    if (y0 > y1) {
        t = x0, x0 = x1, x1 = t;
        t = y0, y0 = y1, y1 = t;
    }
    e->kind = 0xff;
    e->x = fixed(x0);
    e->y = fixed(y0);
    e->x1 = x1;
    e->y1 = y1;
    e->top_left = e->bottom_left = e->top_right = e->bottom_right = 0;
    e->slope = 0;
    if (y0 == y1)
        return edge_flat(e, x0, x1, y0, cut);
    if (cut) {
        if (y0 >= g_wy1)
            return outside(e, 8);
        if (y0 < g_wy0)
            top |= 4;
        if (y1 < g_wy0)
            return outside(e, 4);
        if (y1 >= g_wy1)
            bottom |= 8;
        if (x0 < g_wx0)
            top |= 2;
        if (x0 >= g_wx1)
            top |= 1;
        if (x1 < g_wx0)
            bottom |= 2;
        if (x1 >= g_wx1)
            bottom |= 1;
        if (top & bottom)
            return outside(e, top & bottom);
    }
    code = top | bottom;
    /* (lengths past 31 bits, halved until they fit: 0xa6b39, not reached by the game's
       polygons) */
    dy = y1 - y0;
    dx = x1 - x0;
    if (dx == 0) {
        e->kind = VERTICAL;
        e->count = dy + 1;
    } else if (dx == dy || -dx == dy) {
        e->kind = dx > 0 ? DIAG_RIGHT : DIAG_LEFT;
        e->count = dy + 1;
    } else if ((dx > 0 ? dx : -dx) < dy) {
        e->kind = dx > 0 ? STEEP_RIGHT : STEEP_LEFT;
        e->slope = slope_of((uint32_t)(dx > 0 ? dx : -dx), (uint32_t)dy);
        e->count = dy + 1;
    } else {
        e->kind = dx > 0 ? SHALLOW_RIGHT : SHALLOW_LEFT;
        dx = dx > 0 ? dx : -dx;
        e->slope = slope_of((uint32_t)dy, (uint32_t)dx);
        e->count = dx + 1;
    }
    if (code & 0xc) {
        if (code & 4)
            cut_top(e);
        if (code & 8)
            cut_bottom(e);
        if (x_code(e, &code))
            return 1;
    }
    if (code & 2) {
        if (cut_left(e))
            return 2;
    }
    if (code & 1)
        return cut_right(e);
    return 0;
}

static void put(int *t, int row, int v)
{
    if (row >= 0 && row < ROWS)
        t[row] = v;
}

static void fill(int *t, int row, int n, int v)
{
    for (; n > 0; n--, row++)
        put(t, row, v);
}

/* the rows wholly outside, as the edge on the first way round has them (0x8be7e): left
   and right one past each other, so nothing of them is painted unless the other way
   round widens them */
static void sides_first(const edge *e)
{
    int top = round32(e->y);

    if (e->top_left > 0) {
        fill(g_left, top - e->top_left, e->top_left, g_wx0);
        fill(g_right, top - e->top_left, e->top_left, g_wx0 - 1);
    }
    if (e->top_right > 0) {
        fill(g_left, top - e->top_right, e->top_right, g_wx1);
        fill(g_right, top - e->top_right, e->top_right, g_wx1 - 1);
    }
    if (e->bottom_left > 0) {
        fill(g_left, e->y1 + 1, e->bottom_left, g_wx0);
        fill(g_right, e->y1 + 1, e->bottom_left, g_wx0 - 1);
    }
    if (e->bottom_right > 0) {
        fill(g_left, e->y1 + 1, e->bottom_right, g_wx1);
        fill(g_right, e->y1 + 1, e->bottom_right, g_wx1 - 1);
    }
}

/* and as the other way round has them, after its edge (0x8c4ac): only its own side */
static void sides_second(const edge *e)
{
    int top = round32(e->y);

    if (e->top_left > 0)
        fill(g_left, top - e->top_left, e->top_left, g_wx0);
    if (e->top_right > 0)
        fill(g_right, top - e->top_right, e->top_right, g_wx1 - 1);
    if (e->bottom_left > 0)
        fill(g_left, e->y1 + 1, e->bottom_left, g_wx0);
    if (e->bottom_right > 0)
        fill(g_right, e->y1 + 1, e->bottom_right, g_wx1 - 1);
}

/* the shallow kinds start on the row their y rounds to */
static int first_row(const edge *e, uint32_t *step)
{
    *step = (uint32_t)e->y + 0x80000000u;
    return whole(e->y) + (*step < 0x80000000u);
}

/* the edge on the first way round, from the top corner on through the corners (0x8bf3c):
   both tables get its x on each row, the shallow kinds their run's two ends */
static void walk_first(const edge *e)
{
    int n = e->count, row = whole(e->y), x = whole(e->x);
    int64_t v = e->x + 0x80000000LL;
    uint32_t step;

    if (n <= 0)
        return;
    switch (e->kind) {
    case VERTICAL:
    case DIAG_LEFT:
    case DIAG_RIGHT:
        for (; n > 0; n--, row++) {
            put(g_right, row, x);
            put(g_left, row, x);
            x += e->kind == DIAG_LEFT ? -1 : e->kind == DIAG_RIGHT;
        }
        break;
    case STEEP_LEFT:
    case STEEP_RIGHT:
        for (; n > 0; n--, row++) {
            put(g_right, row, whole(v));
            put(g_left, row, whole(v));
            v += e->kind == STEEP_LEFT ? -times_slope(1, e->slope) : times_slope(1, e->slope);
        }
        break;
    case SHALLOW_LEFT:
        /* each row's right end first, its left end where the row changes (0x8c002) */
        row = first_row(e, &step);
        put(g_right, row, x);
        for (;;) {
            uint32_t was = step;
            step += e->slope;
            if (step < was) {
                put(g_left, row++, x--);
                if (--n == 0)
                    break;
                put(g_right, row, x);
            } else {
                x--;
                if (--n == 0) {
                    put(g_left, row, x + 1);
                    break;
                }
            }
        }
        break;
    case SHALLOW_RIGHT:
        row = first_row(e, &step);
        put(g_left, row, x);
        for (;;) {
            uint32_t was = step;
            step += e->slope;
            if (step < was) {
                put(g_right, row++, x++);
                if (--n == 0)
                    break;
                put(g_left, row, x);
            } else {
                x++;
                if (--n == 0) {
                    put(g_right, row, x - 1);
                    break;
                }
            }
        }
        break;
    }
}

static int get(const int *t, int row)
{
    return row >= 0 && row < ROWS ? t[row] : 0;
}

/* the edge on the other way round (0x8c06e, the convex case 0x8c276): row by row while it
   lies between the two, then, once it is left of the left x or right of the right one,
   every row after it on that side */
static void walk_second(const edge *e)
{
    int n = e->count, row = whole(e->y), x = whole(e->x);
    int64_t v = e->x + 0x80000000LL;
    uint32_t step, was;
    int *side = 0;

    if (n <= 0)
        return;
    switch (e->kind) {
    case VERTICAL:
    case DIAG_LEFT:
    case DIAG_RIGHT:
    case STEEP_LEFT:
    case STEEP_RIGHT:
        for (; n > 0; n--, row++) {
            int at = e->kind >= STEEP_LEFT ? whole(v) : x;
            if (!side) {
                if (get(g_left, row) > at)
                    side = g_left;
                else if (get(g_right, row) < at)
                    side = g_right;
            }
            if (side)
                put(side, row, at);
            if (e->kind == DIAG_LEFT)
                x--;
            else if (e->kind == DIAG_RIGHT)
                x++;
            else if (e->kind == STEEP_LEFT)
                v -= times_slope(1, e->slope);
            else if (e->kind == STEEP_RIGHT)
                v += times_slope(1, e->slope);
        }
        break;
    case SHALLOW_LEFT:
        row = first_row(e, &step);
        for (;;) {
            if (get(g_right, row) < x) {
                /* 0x8c41d: the right x of each row from here */
                for (;;) {
                    put(g_right, row++, x);
                    for (;;) {
                        x--;
                        was = step;
                        step += e->slope;
                        if (--n == 0)
                            return;
                        if (step < was)
                            break;
                    }
                }
            }
            was = step;
            step += e->slope;
            if (step < was) {
                if (get(g_left, row) > x) {
                    /* 0x8c437: the left x of each row from here */
                    for (;;) {
                        put(g_left, row++, x--);
                        if (--n == 0)
                            return;
                        for (;;) {
                            was = step;
                            step += e->slope;
                            if (step < was)
                                break;
                            x--;
                            if (--n == 0) {
                                put(g_left, row, x + 1);
                                return;
                            }
                        }
                    }
                }
                row++;
            }
            x--;
            if (--n == 0) {
                x++;
                if (get(g_left, row) > x)
                    put(g_left, row, x);
                return;
            }
        }
    case SHALLOW_RIGHT:
        row = first_row(e, &step);
        for (;;) {
            if (get(g_left, row) > x) {
                /* 0x8c49e: the left x of each row from here */
                for (;;) {
                    put(g_left, row++, x);
                    for (;;) {
                        x++;
                        if (--n == 0)
                            return;
                        was = step;
                        step += e->slope;
                        if (step < was)
                            break;
                    }
                }
            }
            was = step;
            step += e->slope;
            if (step < was) {
                if (get(g_right, row) < x) {
                    /* 0x8c477: the right x of each row from here */
                    for (;;) {
                        put(g_right, row, x);
                        for (;;) {
                            x++;
                            was = step;
                            step += e->slope;
                            if (step < was)
                                break;
                            if (--n == 0) {
                                put(g_right, row + 1, x - 1);
                                return;
                            }
                        }
                        row++;
                        if (--n == 0) {
                            put(g_right, row, x - 1);
                            return;
                        }
                    }
                }
                row++;
            }
            x++;
            if (--n == 0)
                return;
        }
    }
}

void poly_fill(int n, const int *xy, poly_rows rows, int colour)
{
    int top = 0, bottom = 0, min_y, max_y, min_x, max_x, cut, i, k, y0, y1;
    edge e;

    raster_clip(&g_wx0, &g_wy0, &g_wx1, &g_wy1);
    if (n == 1) {
        poly_line(xy[0], xy[1], xy[0], xy[1], colour);   /* a point: 0x8bb3c */
        return;
    }
    min_y = max_y = xy[1];
    min_x = max_x = xy[0];
    for (i = 1; i < n; i++) {
        if (xy[2 * i + 1] <= min_y) {
            min_y = xy[2 * i + 1];
            top = i;
        }
        if (xy[2 * i + 1] > max_y) {
            max_y = xy[2 * i + 1];
            bottom = i;
        }
        if (xy[2 * i] < min_x)
            min_x = xy[2 * i];
        if (xy[2 * i] > max_x)
            max_x = xy[2 * i];
    }
    if (max_y - min_y <= 0 || max_x < g_wx0 || min_x >= g_wx1 - 1 || max_y < g_wy0
        || min_y >= g_wy1)
        return;
    cut = max_x > g_wx1 - 1 || min_x < g_wx0 || max_y >= g_wy1 || min_y < g_wy0;
    if (max_x == min_x) {
        poly_line(min_x, min_y, max_x, max_y, colour);   /* one column: 0x8bb3c */
        return;
    }

    /* on from the top corner to the bottom one, then back the other way */
    for (i = top; i != bottom; i = k) {
        k = i == n - 1 ? 0 : i + 1;
        if (xy[2 * k + 1] > xy[2 * i + 1]) {
            edge_make(&e, xy[2 * i], xy[2 * i + 1], xy[2 * k], xy[2 * k + 1], cut);
            if (cut)
                sides_first(&e);
            walk_first(&e);
        }
    }
    for (i = top; i != bottom; i = k) {
        k = i == 0 ? n - 1 : i - 1;
        if (xy[2 * k + 1] > xy[2 * i + 1]) {
            edge_make(&e, xy[2 * i], xy[2 * i + 1], xy[2 * k], xy[2 * k + 1], cut);
            walk_second(&e);
            if (cut)
                sides_second(&e);
        }
    }
    y1 = max_y < g_wy1 ? max_y : g_wy1 - 1;
    y0 = min_y < g_wy0 ? g_wy0 : min_y;
    if (y1 - y0 <= 0)
        return;
    rows(g_left + y0, g_right + y0, y0, y1 - y0 + 1, colour);
}

/* a pixel, if in the window (0xa6060) */
static void plot(int x, int y, int colour)
{
    if (x >= g_wx0 && x < g_wx1 && y >= g_wy0 && y < g_wy1)
        raster_row(y)[x] = (unsigned char)colour;
}

/* the edge's pixels (0xa6c38): its ends rounded to 16.16 and only the slope's top 16 bits
   stepped, so a long line drifts from the polygons' edges */
static void line_paint(const edge *e, int colour)
{
    uint32_t xf = (uint32_t)(e->x >> 16) + 0x8000, yf = (uint32_t)(e->y >> 16) + 0x8000;
    uint32_t step = e->slope >> 16;
    int x = (int)(xf >> 16), y = (int)(yf >> 16), row = whole(e->y), n = e->count;

    xf &= 0xffff;
    yf &= 0xffff;
    switch (e->kind) {
    case FLAT_BACK:
    case FLAT:
        if (n)
            raster_fill(x, y, n, 1, colour);
        break;
    case VERTICAL:
        if (n)
            raster_fill(x, row, 1, n, colour);
        break;
    case DIAG_LEFT:
    case DIAG_RIGHT:
        for (; n > 0; n--, row++, x += e->kind == DIAG_LEFT ? -1 : 1)
            plot(x, row, colour);
        break;
    case STEEP_LEFT:
    case STEEP_RIGHT:
        for (; n > 0; n--, row++) {
            plot(x, row, colour);
            xf += step;
            if (xf & 0xffff0000u) {
                xf &= 0xffff;
                x += e->kind == STEEP_LEFT ? -1 : 1;
            }
        }
        break;
    case SHALLOW_LEFT:
    case SHALLOW_RIGHT:
        for (; n > 0; n--) {
            yf += step;
            plot(x, y, colour);
            x += e->kind == SHALLOW_LEFT ? -1 : 1;
            if (yf & 0xffff0000u) {
                y++;
                yf &= 0xffff;
            }
        }
        break;
    case POINT:
        plot(x, y, colour);
        break;
    }
}

void poly_line(int x0, int y0, int x1, int y1, int colour)
{
    edge e;

    raster_clip(&g_wx0, &g_wy0, &g_wx1, &g_wy1);
    if (!edge_make(&e, x0, y0, x1, y1, 1) && e.count > 0)
        line_paint(&e, colour);
}

void poly_solid_rows(const int *left, const int *right, int y, int n, int colour)
{
    for (; n > 0; n--, y++, left++, right++)
        raster_fill(*left, y, *right - *left + 1, 1, colour);
}

void poly_shade_rows(const int *left, const int *right, int y, int n, int colour)
{
    unsigned char *p;
    int x;

    (void)colour;
    for (; n > 0; n--, y++, left++, right++) {
        if (!RASTER_SHOWN(y))
            continue;
        p = raster_row(y);
        for (x = *left; x <= *right; x++)
            p[x] = g_shade[p[x]];
    }
}

/* EA's smoothed wide line (0x8b195): stepped a row at a time when it is steep, a column
   at a time when it is shallow; on each the span from one side to the other, the end
   pixels blended into what is there by how much of them it covers (in 16ths), the ones
   between in the colour. A cut of it into the bounds first (0x8a507, 0x8a76f); then the
   steps that may leave the window are painted pixel by pixel through the window
   (0x8a9d7, 0x8adb2), the others straight into the rows (0x8ac1c, 0x8aff7). */
typedef struct {
    int steep;
    int a, a_end;              /* along: y (steep) or x, 16.16 */
    int lo, hi;                /* the sides across, 16.16 */
    int s_lo, s_hi;            /* their steps */
    int amin, amax, cmin, cmax;    /* the bounds along and across, 16.16 */
    int xmin, xmax;            /* the bounds in x, 16.16 (the cut's checks along) */
    int wc0, wc1;              /* the window across */
    int colour;
    const unsigned char (*blend)[256];
} wide;

/* a times b, 16.16, rounded by the last bit out */
static int fmul(int a, int b)
{
    int64_t p = (int64_t)a * b;
    return (int)(uint32_t)(p >> 16) + (int)((p >> 15) & 1);
}

static int fdiv(int a, int b)
{
    return (int)(((int64_t)a << 16) / b);
}

/* 0 nothing to draw; else 1, or 2 when it may leave the bounds across */
static int wide_cut(wide *w)
{
    int d, partial;

    if (w->a_end < w->amin || w->a >= w->amax)
        return 0;
    if (w->a < w->amin) {
        d = w->amin - w->a;
        w->lo += fmul(w->s_lo, d);
        w->hi += fmul(w->s_hi, d);
        w->a = w->amin;
    }
    if (w->a_end > w->amax)
        w->a_end = w->amax;
    partial = w->lo < w->cmin || w->hi >= w->cmax;
    if (w->hi < w->cmin) {
        if (w->s_hi <= 0)
            return 0;
        d = fdiv(w->cmin - w->hi, w->s_hi);
        w->a += d;
        if (w->a >= w->xmax || w->a < w->xmin)
            return 0;
        w->hi = w->cmin;
        w->lo += fmul(w->s_lo, d);
        partial = 1;
    }
    if (w->lo >= w->cmax) {
        if (w->s_lo >= 0)
            return 0;
        d = fdiv(w->cmax - w->lo + 1, w->s_lo);
        w->a += d;
        if (w->a >= w->xmax || w->a < w->xmin)
            return 0;
        w->hi += fmul(w->s_hi, d);
        w->lo = w->cmax - 1;
        partial = 1;
    }
    return 1 + partial;
}

/* the pixel at (along, across) */
static int wide_get(const wide *w, int a, int c)
{
    int x = w->steep ? c : a, y = w->steep ? a : c;

    return x >= g_wx0 && x < g_wx1 && y >= g_wy0 && y < g_wy1 ? raster_row(y)[x] : 0;
}

static void wide_put(const wide *w, int a, int c, int colour)
{
    plot(w->steep ? c : a, w->steep ? a : c, colour);
}

static void wide_blend(const wide *w, int a, int c, int level)
{
    wide_put(w, a, c, w->blend[level - 1][wide_get(w, a, c)]);
}

/* the steps that may leave the window, until one lies inside it (0x8a9d7, 0x8adb2) */
static void wide_cut_steps(wide *w)
{
    int at = w->a >> 16, l, r, fl, fr;

    for (; w->a < w->a_end; w->lo += w->s_lo, w->hi += w->s_hi, w->a += 0x10000, at++) {
        r = w->hi >> 16;
        l = w->lo >> 16;
        if ((w->wc0 <= l && w->wc1 > r) || (w->wc0 > l && w->wc0 > r)
            || (w->wc1 <= l && w->wc1 <= r))
            return;
        fl = (0x10000 - (w->lo & 0xffff)) >> 12;
        fr = (w->hi & 0xffff) >> 12;
        if (r == l) {
            fr += fl - 16;
            if (fr > 0)
                wide_blend(w, at, r, fr);
            continue;
        }
        if (fl && w->wc0 < l)
            wide_blend(w, at, l, fl);
        if (l + 1 < r) {
            if (w->steep)
                raster_fill(l + 1, at, r - l - 1, 1, w->colour);
            else
                raster_fill(at, l + 1, 1, r - l - 1, w->colour);
        }
        if (fr && w->wc1 > r)
            wide_blend(w, at, r, fr);
    }
}

/* the steps inside the bounds, straight into the rows (0x8ac1c, 0x8aff7) */
static void wide_steps(wide *w)
{
    int at = w->a >> 16, l, n, fl, fr, lvl;
    unsigned char *p;
    long step;

    for (; w->a < w->a_end && w->hi < w->cmax && w->lo >= w->cmin;
         w->lo += w->s_lo, w->hi += w->s_hi, w->a += 0x10000, at++) {
        fr = (w->hi & 0xffff) >> 12;
        l = w->lo >> 16;
        n = (w->hi >> 16) - l;
        fl = (0x10000 - (w->lo & 0xffff)) >> 12;
        if (w->steep) {
            p = raster_row(at) + l;
            step = 1;
        } else {
            p = raster_row(l) + at;
            step = raster_row(1) - raster_row(0);
        }
        if (n == 0) {
            lvl = fr + fl - 16;
            if (lvl > 0)       /* (below 1 the original reads before its table) */
                *p = w->blend[lvl - 1][*p];
            continue;
        }
        if (fl)
            *p = w->blend[fl - 1][*p];
        for (p += step, n--; n; p += step, n--)
            *p = (unsigned char)w->colour;
        if (fr)
            *p = w->blend[fr - 1][*p];
    }
}

void poly_wide_line(int x0, int y0, int x1, int y1, int colour,
                    const unsigned char (*blend)[256], int w0, int w1, int width, int height)
{
    int dx = x1 - x0, dy = y1 - y0, dw, h;
    wide w;

    if ((dx < 0 ? -dx : dx) < 0x10000 && (dy < 0 ? -dy : dy) < 0x10000)
        return;
    raster_clip(&g_wx0, &g_wy0, &g_wx1, &g_wy1);
    w.colour = colour;
    w.blend = blend;
    w.xmin = 0;
    w.xmax = width << 16;
    w.steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx);
    if (w.steep) {
        dw = (w1 - w0) / (2 * (dy >> 16));
        if (dy < 0) {
            w.a = y1, w.a_end = y0, h = w1 / 2;
            w.lo = x1 - h, w.hi = x1 + h;
        } else {
            w.a = y0, w.a_end = y1, h = w0 / 2;
            w.lo = x0 - h, w.hi = x0 + h;
        }
        dx /= dy >> 16;
        w.s_lo = dx - dw;
        w.s_hi = dx + dw;
        w.amin = 0, w.amax = height << 16;
        w.cmin = 0, w.cmax = width << 16;
        w.wc0 = g_wx0, w.wc1 = g_wx1;
    } else {
        dw = (w1 - w0) / (2 * (dx >> 16));
        if (dx < 0) {
            w.a = x1, w.a_end = x0, h = w1 / 2;
            w.lo = y1 - h, w.hi = y1 + h;
        } else {
            w.a = x0, w.a_end = x1, h = w0 / 2;
            w.lo = y0 - h, w.hi = y0 + h;
        }
        dy /= dx >> 16;
        w.s_lo = dy - dw;
        w.s_hi = dy + dw;
        w.amin = 0, w.amax = width << 16;
        w.cmin = 0, w.cmax = height << 16;
        w.wc0 = g_wy0, w.wc1 = g_wy1;
    }
    switch (wide_cut(&w)) {
    case 0:
        return;
    case 2:
        wide_cut_steps(&w);
    }
    if (w.a < w.a_end) {
        wide_steps(&w);
        if (w.a < w.a_end)
            wide_cut_steps(&w);
    }
}
