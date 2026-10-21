/* Painting a textured four-cornered piece of the 3D view (NOTES §4.6): the road, the
   scenery, the cars. The game's filler 0x34ed8 hands a quad to 0x91360, or to its twin
   0x90b30 when the picture has see-through pixels (colour 255, shape type 0x45, marked at
   load by 0x5b270). Both are the same routine with a different row painter: 0xa7a4c,
   0xa7610.

   The corners come in order round the quad, the screen positions as whole pixels, the
   picture positions in 16.16. The picture is stepped straight across each row (no
   perspective: draw_quad 0x35b04 cuts the far pieces smaller instead). */
#include "raster.h"
#include <string.h>

/* the window (0xf88a4 x0, 0xf88a8 y0, 0xf88ac x1, 0xf88b0 y1: the ends outside) */
static unsigned char *g_base;
static int g_pitch, g_x0, g_y0, g_x1, g_y1;
const unsigned char *g_raster_shown;

/* the edge tables, a row each (0x300 at most): left and right x, and u, v at both ends.
   The original keeps them on its stack, so a row no edge reached keeps what was there */
#define ROWS 0x300
static int g_lx[ROWS], g_rx[ROWS], g_lu[ROWS], g_lv[ROWS], g_ru[ROWS], g_rv[ROWS];

void raster_window(unsigned char *base, int pitch, int x0, int y0, int x1, int y1)
{
    g_base = base;
    g_pitch = pitch;
    g_x0 = x0;
    g_y0 = y0;
    g_x1 = x1;
    g_y1 = y1 > ROWS ? ROWS : y1;
    g_raster_shown = 0;
}

void raster_rows(const unsigned char *shown)
{
    g_raster_shown = shown;
}

unsigned char *raster_row(int y)
{
    return g_base + y * g_pitch;
}

void raster_clip(int *x0, int *y0, int *x1, int *y1)
{
    *x0 = g_x0;
    *y0 = g_y0;
    *x1 = g_x1;
    *y1 = g_y1;
}

void raster_fill(int x, int y, int w, int h, int colour)
{
    int x1 = x + w, y1 = y + h;
    if (x < g_x0)
        x = g_x0;
    if (y < g_y0)
        y = g_y0;
    if (x1 > g_x1)
        x1 = g_x1;
    if (y1 > g_y1)
        y1 = g_y1;
    if (x1 > x)
        for (; y < y1; y++)
            if (RASTER_SHOWN(y))
                memset(g_base + y * g_pitch + x, colour, (size_t)(x1 - x));
}

static int shl16(int v)
{
    return (int)((unsigned)v << 16);
}

/* the rows of an edge from y, n of them, cut to the window: where its first and last row
   start. Returns 0 when none of it is inside */
static int edge_rows(int *y, int *n, int *start, int step)
{
    if (*y >= g_y1 || *y + *n < g_y0)
        return 0;
    if (*y < g_y0) {
        *start += step * (g_y0 - *y);
        *n -= g_y0 - *y;
        *y = g_y0;
    }
    if (*y + *n >= g_y1)
        *n = g_y1 - *y;
    return 1;
}

/* an edge's x into a table, from x0 to x1 over n rows: the left edge (0xa7790) starts half
   way along its first row when it slopes left, the right one (0xa7844) when it slopes
   right, so the quad takes in the pixels its edge passes through. Returns the slope */
static int edge_x(int *table, int y, int n, int x0, int x1, int right)
{
    int slope, x;
    if (y >= g_y1 || y + n < g_y0)
        return 0;
    slope = shl16(x1 - x0) / n;
    x = shl16(x0);
    if (right ? slope > 0 : slope < 0)
        x += slope;
    if (!edge_rows(&y, &n, &x, slope))
        return 0;
    for (; n > 0; n--, x += slope)
        table[y++] = x >> 16;
    return slope;
}

/* u or v along an edge (0xa78e4 left, 0xa79a4 right), half a step in where its x was */
static void edge_uv(int *table, int y, int n, int a0, int a1, int xslope, int right)
{
    int step, a = a0;
    if (y >= g_y1 || y + n < g_y0)
        return;
    step = (a1 - a0) / n;
    if (right ? xslope > 0 : xslope < 0)
        a += step;
    if (!edge_rows(&y, &n, &a, step))
        return;
    for (; n > 0; n--, a += step)
        table[y++] = a;
}

/* both sides from the top corner to the bottom one: the left going round by -dir */
static void edges(const int uv[8], const int xy[8], int top, int bottom, int dir)
{
    int i, j, n, s;
    for (i = top; i != bottom; i = j) {
        j = (i - dir) & 3;
        n = xy[j * 2 + 1] - xy[i * 2 + 1] + 1;
        if (n > 0) {
            s = edge_x(g_lx, xy[i * 2 + 1], n, xy[i * 2], xy[j * 2], 0);
            edge_uv(g_lu, xy[i * 2 + 1], n, uv[i * 2], uv[j * 2], s, 0);
            edge_uv(g_lv, xy[i * 2 + 1], n, uv[i * 2 + 1], uv[j * 2 + 1], s, 0);
        }
    }
    for (i = top; i != bottom; i = j) {
        j = (i + dir) & 3;
        n = xy[j * 2 + 1] - xy[i * 2 + 1] + 1;
        if (n > 0) {
            s = edge_x(g_rx, xy[i * 2 + 1], n, xy[i * 2], xy[j * 2], 1);
            edge_uv(g_ru, xy[i * 2 + 1], n, uv[i * 2], uv[j * 2], s, 1);
            edge_uv(g_rv, xy[i * 2 + 1], n, uv[i * 2 + 1], uv[j * 2 + 1], s, 1);
        }
    }
}

/* one row (0xa7a4c, 0xa7610 skipping colour 255): the picture's whole part and fraction
   stepped apart, as the original's carries do */
static void span(const raster_tex *t, unsigned char *dst, int u, int du, int v, int dv,
                 int n, int see_through)
{
    const unsigned char *p = t->pixels + (v >> 16) * t->rowbytes + (u >> 16);
    int step = (dv >> 16) * t->rowbytes + (du >> 16);
    unsigned uf = u & 0xffff, vf = v & 0xffff;
    for (; n > 0; n--, dst++) {
        unsigned char c = p >= t->lo && p < t->hi ? *p : 0;
        if (!see_through || c != 255)
            *dst = c;
        uf += du & 0xffff;
        vf += dv & 0xffff;
        p += step + (int)(uf >> 16) + (vf >> 16 ? t->rowbytes : 0);
        uf &= 0xffff;
        vf &= 0xffff;
    }
}

void raster_quad(const raster_tex *t, const int uv[8], const int xy[8])
{
    int see_through = t->see_through;
    int top = 0, bottom = 2, ymin = xy[1], ymax = xy[5], i, y, dir, s_next, s_prev;
    int retried = 0;

    for (i = 0; i < 4; i++) {
        if (xy[i * 2 + 1] < ymin) {
            ymin = xy[i * 2 + 1];
            top = i;
        } else if (xy[i * 2 + 1] > ymax) {
            ymax = xy[i * 2 + 1];
            bottom = i;
        }
    }
    if (ymin >= g_y1 || ymax < g_y0)
        return;

    /* which way round the corners go: the edge after the top one further right */
    i = (top + 1) & 3;
    s_next = shl16(xy[i * 2] - xy[top * 2]) / (xy[i * 2 + 1] - xy[top * 2 + 1] + 1);
    i = (top - 1) & 3;
    s_prev = shl16(xy[i * 2] - xy[top * 2]) / (xy[i * 2 + 1] - xy[top * 2 + 1] + 1);
    dir = s_next >= s_prev ? 1 : -1;
    edges(uv, xy, top, bottom, dir);

    if (ymin < g_y0)
        ymin = g_y0;
    if (ymax >= g_y1)
        ymax = g_y1 - 1;
    for (y = ymin; y <= ymax; y++) {
        int l = g_lx[y], r = g_rx[y], n = r - l + 1, u, v, du, dv;
        if (n <= 0) {
            /* a row the wrong way round: once, the sides swapped and this row again;
               a second time ends the quad (0x91917) */
            if (retried)
                return;
            retried = 1;
            dir = -dir;
            edges(uv, xy, top, bottom, dir);
            y--;
            continue;
        }
        if (g_x1 <= l || g_x0 > r || !RASTER_SHOWN(y))
            continue;
        u = g_lu[y];
        v = g_lv[y];
        du = (g_ru[y] - u) / n;
        dv = (g_rv[y] - v) / n;
        if (l < g_x0) {
            u += du * (g_x0 - l);
            v += dv * (g_x0 - l);
            n -= g_x0 - l;
            l = g_x0;
        }
        if (r >= g_x1)
            n -= r - g_x1 + 1;
        span(t, g_base + y * g_pitch + l, u, du, v, dv, n, see_through);
    }
}

/* The far road's painter (NOTES §4.6): a picture stretched whole over a quad, cut in two
   triangles (0x90900, and 0x906d0 for see-through pictures). Each triangle is stepped
   straight across from its long edge (0x8fbd8, 0x8f0c8), every row reading the picture
   through one table of steps worked out at its widest row (0xa6240). */

#define STEPS 0x604                    /* (0x6000000 >> 16) + 1, written in pairs (0x15ed1c) */
static int g_steps[STEPS];

/* where a row's pixels read, from where it starts: the picture stepped du, dv a pixel from
   half way into the first one (0xa6240). A width of a power of two (0 too) is shifted */
static void row_steps(int du, int dv, int w, int n)
{
    unsigned u = 0x8000, v = 0x8000, x;
    int i, shift = -1;
    if ((w & (w - 1)) == 0)
        for (shift = 0, x = (unsigned)w; x > 1; x >>= 1)
            shift++;
    n = (n + 1) >> 1 << 1;
    if (n > STEPS)
        n = STEPS;
    for (i = 0; i < n; i++, u += (unsigned)du, v += (unsigned)dv) {
        int row = (int)v >> 16;
        g_steps[i] = (shift >= 0 ? (int)((unsigned)row << shift) : row * w) + ((int)u >> 16);
    }
}

/* (a << 16) / b in 64 bits, as the original's shld and idiv; a itself when b is 0 */
static int div16(int a, int b)
{
    return b ? (int)((long long)a * 65536 / b) : a;
}

static int sub(int a, int b)
{
    return (int)((unsigned)a - (unsigned)b);
}

/* where a triangle's rows stand: both sides' x, the picture at the left one and its steps */
typedef struct {
    int row, lx, rx, u, v, sl, sr, su, sv;
} tri_rows;

/* rows up to `end`, cut to the window, each from the picture where its left side is plus
   the steps (0x94aed; 0x9473e leaves colour 255 out). A row cut on the left starts its
   steps from those of the pixels cut, added to the first pixel's: not the steps further
   along, as the original */
static void rows(const raster_tex *t, tri_rows *r, int end)
{
    for (; r->row < end; r->row++) {
        int l = r->lx >> 16, n = (r->rx >> 16) - l, over = l + n - g_x1, skip = g_x0 - l;
        if (over > 0)
            n -= over;
        if (skip > 0) {
            n -= skip;
            l += skip;
        } else
            skip = 0;
        if (n > 0 && r->row >= g_y0 && skip + n <= STEPS && RASTER_SHOWN(r->row)) {
            const unsigned char *src = t->pixels + (r->v >> 16) * t->rowbytes + (r->u >> 16) +
                                       g_steps[skip];
            unsigned char *dst = g_base + r->row * g_pitch + l;
            int i;
            for (i = 0; i < n; i++) {
                const unsigned char *p = src + g_steps[i];
                unsigned char c = p >= t->lo && p < t->hi ? *p : 0;
                if (!t->see_through || c != 255)
                    dst[i] = c;
            }
        }
        r->u += r->su;
        r->v += r->sv;
        r->lx += r->sl;
        r->rx += r->sr;
    }
}

/* one triangle, uv and xy in 16.16 (0x8fbd8). The original goes a quicker way when the
   corners lie inside the window, but it compares their 16.16 x with the window's pixels,
   so it only takes it for rows it would paint nothing in anyway */
static void tri(const raster_tex *t, const int uv[6], const int xy[6])
{
    int hi = 0, lo = 0, m, xb, yb, xt, yt, xm, ym, ub, vb, ut, vt, um, vm;
    int dy, xlm, w, wabs, left, ulm, vlm, end;
    tri_rows r;

    if (xy[3] > xy[1])
        hi = 1;
    if (xy[1] > xy[3])
        lo = 1;
    if (xy[hi * 2 + 1] < xy[5])
        hi = 2;
    if (xy[lo * 2 + 1] > xy[5])
        lo = 2;
    xb = xy[lo * 2], yb = xy[lo * 2 + 1], ub = uv[lo * 2], vb = uv[lo * 2 + 1];
    xt = xy[hi * 2], yt = xy[hi * 2 + 1], ut = uv[hi * 2], vt = uv[hi * 2 + 1];
    dy = sub(yt, yb);
    if (dy < 0x10000)
        return;                        /* also when all three are on one row */
    m = 3 - (hi | lo);
    xm = xy[m * 2], ym = xy[m * 2 + 1], um = uv[m * 2], vm = uv[m * 2 + 1];

    /* the long edge where the middle corner's row crosses it, and the width there */
    xlm = xb + (int)((long long)sub(xt, xb) * sub(ym, yb) / dy);
    w = sub(xm, xlm);
    left = w < 0;                      /* the middle corner on the left */
    wabs = left ? (int)(0u - (unsigned)w) : w;
    if (wabs < 0x10000 || wabs >= 0x6000000)
        return;
    ulm = ut + (int)((long long)sub(ub, ut) * sub(yt, ym) / dy);
    vlm = vt + (int)((long long)sub(vb, vt) * sub(yt, ym) / dy);
    row_steps(div16(sub(um, ulm), w), div16(sub(vm, vlm), w), t->rowbytes, (wabs >> 16) + 1);

    /* from the top corner down to the middle one's row: the picture follows the left side */
    r.row = yb >> 16;
    r.u = ub;
    r.v = vb;
    r.lx = r.rx = xb;
    if (left) {
        r.su = div16(sub(ub, um), sub(yb, ym));
        r.sv = div16(sub(vb, vm), sub(yb, ym));
    } else {
        r.su = div16(sub(ub, ut), sub(yb, yt));
        r.sv = div16(sub(vt, vb), sub(yt, yb));
    }
    if (r.row < ym >> 16) {
        if (left) {
            r.sl = div16(sub(xb, xm), sub(yb, ym));
            r.sr = div16(sub(xt, xb), dy);
        } else {
            r.sr = div16(sub(xb, xm), sub(yb, ym));
            r.sl = div16(sub(xt, xb), dy);
        }
        end = g_y1 < ym >> 16 ? g_y1 : ym >> 16;
        rows(t, &r, end);
    }

    /* and on to the bottom corner: the short side starts again at the middle corner */
    if (r.row >= yt >> 16)
        return;
    if (left) {
        r.sl = div16(sub(xm, xt), sub(ym, yt));
        r.sr = div16(sub(xt, xb), dy);
        r.lx = xm;
        r.u = um;
        r.v = vm;
        r.su = div16(sub(um, ut), sub(ym, yt));
        r.sv = div16(sub(vm, vt), sub(ym, yt));
    } else {
        r.sr = div16(sub(xm, xt), sub(ym, yt));
        r.sl = div16(sub(xt, xb), dy);
        r.rx = xm;
    }
    end = yt >> 16 < g_y1 ? yt >> 16 : g_y1;
    rows(t, &r, end);
}

/* corners and picture points as whole pixels into 16.16, half way into each (0x905e0) */
static void tri_pixels(const raster_tex *t, const int uv[6], const int *p0, const int *p1,
                       const int *p2)
{
    int uv16[6], xy16[6], i;
    const int *p[3] = {p0, p1, p2};
    for (i = 0; i < 6; i++) {
        uv16[i] = (int)(((unsigned)uv[i] << 16) + 0x8000);
        xy16[i] = (int)(((unsigned)p[i / 2][i % 2] << 16) + 0x8000);
    }
    tri(t, uv16, xy16);
}

void raster_tri(const raster_tex *t, const int uv[6], const int xy[6])
{
    tri_pixels(t, uv, xy, xy + 2, xy + 4);
}

void raster_quad_whole(const raster_tex *t, const int xy[8])
{
    int w = t->w - 2, h = t->h - 2;
    const int a[6] = {1, 1, w, 1, w, h}, b[6] = {w, h, 1, h, 1, 1};
    tri_pixels(t, a, xy, xy + 2, xy + 4);
    tri_pixels(t, b, xy + 4, xy + 6, xy);
}

int raster_outcode(int n, const int *xy)
{
    int all = 15;
    for (; n > 0 && all; n--, xy += 2) {
        unsigned x = (unsigned)xy[0], y = (unsigned)xy[1], code = 0;
        if (x < (unsigned)g_x0)
            code |= 1;
        if ((unsigned)g_x1 < x + 1)
            code |= 2;
        if (y < (unsigned)g_y0)
            code |= 4;
        if ((unsigned)g_y1 < y + 1)
            code |= 8;
        all &= code;
    }
    return all;
}
