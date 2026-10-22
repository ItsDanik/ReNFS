/* The sky behind the road (NOTES §4.7). */
#include "sky.h"
#include "exe.h"
#include "config.h"
#include "racepal.h"
#include "track.h"
#include "trackfam.h"
#include "trackinfo.h"
#include "view.h"
#include "polyfill.h"
#include <stdint.h>

int g_sky_steps[0x400];   /* 0x12df40: a column's x in the picture, from the middle */
unsigned char g_sky_rows[480];  /* 0x12dd60: a screen row's row of the picture */
static int g_columns;          /* 0x12efb8 */
static int g_squash;           /* 0x12efbc */

static int rd16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

/* a / b in 16.16, the whole part and the remainder divided apart (EA's way) */
static int fixdiv(int a, int b)
{
    int q = a / b, r = a % b;
    return (int)((unsigned)q << 16) + (int)((long long)r * 65536 / b);
}

/* a × b in 16.16, rounded by the bit below */
static int fixmul(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)(p >> 15 & 1);
}

void sky_tables(int width, int half_fov, int scale, int squash)
{
    int half = width / 2, angle = (int)((unsigned)half_fov << 16) / 360, t, focal, per, i;
    unsigned acc, step;

    t = ea_cos(angle);
    t = t ? fixdiv(ea_sin(angle), t) : 0x11;
    focal = fixdiv((int)((unsigned)half << 16), t);
    per = fixdiv(scale / 2, angle);
    if (half_fov <= 10)
        return;
    g_squash = squash;
    step = (unsigned)fixdiv(0x10000, squash);
    for (i = 0, acc = 0; i < 480; i++, acc += step)
        g_sky_rows[i] = (unsigned char)((int)acc >> 16);
    g_columns = width;
    /* each column from the middle by its angle, the left half the right's negated */
    for (i = 0; half + i <= width && half >= 0; i++) {
        int v = fixmul(ea_atan2((int)((unsigned)i << 16), focal), per);
        if (half + i < 0x400)
            g_sky_steps[half + i] = v;
        if (half - i >= 0)
            g_sky_steps[half - i] = -v;
    }
}

/* a pixel of the picture; outside the family's memory 0, where the original read on */
static unsigned char pixel(const unsigned char *pix, long at)
{
    const unsigned char *p = pix + at;
    return p >= g_fam.file && p < g_fam.file + g_fam.len ? *p : 0;
}

/* (the port's) whether pix + first .. pix + last all lie in the family's memory */
static int inside(const unsigned char *pix, long long first, long long last)
{
    long long at = (long long)((intptr_t)pix - (intptr_t)g_fam.file);
    return at + first >= 0 && at + last < (long long)g_fam.len;
}

/* the picture's rows from screen row `top` (0x49bdc), from picture column `off` at the
   middle: what falls off one end comes round from the other */
static void strip(const unsigned char *shape, int off, int x, int top)
{
    int w = rd16(shape + 4), h = rd16(shape + 6), n = fixmul(h - 1, g_squash), skip = 0;
    int x0, y0, x1, y1, k, rest, i, j, row = 1, lo_part[2], hi_part[2];
    const unsigned char *pix = shape + 16;

    raster_clip(&x0, &y0, &x1, &y1);
    if (top < y0) {
        skip = (int)((unsigned)y0 - (unsigned)top);
        top = y0;
        n -= skip;
    }
    if (top + n > y1)
        n = y1 - top;
    k = 0;
    if (g_sky_steps[0] + off < 0) {        /* the left end before the picture's start */
        row++;
        while (k < g_columns && g_sky_steps[k] + off < 0)
            k++;
    } else
        while (k < g_columns && g_sky_steps[k] + off < w)
            k++;
    rest = g_columns - k;
    /* (the port's) the least and greatest step of each part, so that a row inside the
       family's memory is painted without looking at each pixel */
    {
        int lo1 = 0, hi1 = -1, lo2 = 0, hi2 = -1;
        for (j = 0; j < k; j++) {
            lo1 = !j || g_sky_steps[j] < lo1 ? g_sky_steps[j] : lo1;
            hi1 = !j || g_sky_steps[j] > hi1 ? g_sky_steps[j] : hi1;
        }
        for (j = 0; j < rest; j++) {
            lo2 = !j || g_sky_steps[k + j] < lo2 ? g_sky_steps[k + j] : lo2;
            hi2 = !j || g_sky_steps[k + j] > hi2 ? g_sky_steps[k + j] : hi2;
        }
        lo2 -= w;
        hi2 -= w;
        lo_part[0] = lo1, hi_part[0] = hi1, lo_part[1] = lo2, hi_part[1] = hi2;
    }
    for (i = 0; i < n && skip + i >= 0 && skip + i < 480; i++) {
        unsigned char *dst;
        if (top + i < y0 || top + i >= y1)
            break;                     /* (a horizon far off the screen: the original
                                          runs past its table of rows) */
        if (!RASTER_SHOWN(top + i))
            continue;
        dst = raster_row(top + i) + x;
        long at = (long)(g_sky_rows[skip + i] + row) * w + off;
        if ((!k || inside(pix, at + lo_part[0], at + hi_part[0]))
            && (!rest || inside(pix, at + lo_part[1], at + hi_part[1]))) {
            const unsigned char *a = pix + at, *b = a - w;
            for (j = 0; j < k; j++)
                dst[j] = a[g_sky_steps[j]];
            for (j = 0; j < rest; j++)
                dst[k + j] = b[g_sky_steps[k + j]];
            continue;
        }
        for (j = 0; j < k; j++)
            dst[j] = pixel(pix, at + g_sky_steps[j]);
        for (j = 0; j < rest; j++)
            dst[k + j] = pixel(pix, at + g_sky_steps[k + j] - w);
    }
}

/* the circuits' ring (NOTES §4.7): 32 points round the track, 16 a turn twice over
   (sky_ring_setup 0x4a64c), and the horizon picture cut in eight panels, the second eight
   the same turned round */
static void ring_points(vec3 p[32])
{
    int k;

    for (k = 0; k < 32; k++) {
        int a = k << 12;

        p[k].x = fixmul(ea_sin(a), g_info.radius_far);
        p[k].y = g_info.vert_far;
        p[k].z = fixmul(ea_cos(a), g_info.radius_far) + g_info.z_far;
    }
}

static void ring_uv(int panel, int w, int h, int uv[8])
{
    int d = (int)((unsigned)(w / 8 - 1) << 16), i = panel & 7, u;

    if (panel < 8) {
        u = (int)((unsigned)(i * w) >> 3 << 16);
        uv[0] = uv[6] = u;
        uv[2] = uv[4] = u + d;
    } else {
        u = (int)((unsigned)((7 - i) * w) >> 3 << 16) + d;
        uv[0] = uv[6] = u;
        uv[2] = uv[4] = u - d;
    }
    uv[1] = uv[3] = 0x10000;
    uv[5] = uv[7] = (h - 2) << 16;
}

/* 0x4ab18: the sky's colour above the ring and the ground's below it, each in two parts
   left and right of the horizon's point, their edges moved by the camera's roll */
static void ring_fill(int roll, const scr2 *pt, int above, int below, const view_window *vw,
                      const road_window *win, int top, int bottom)
{
    int a = roll >> 8, c = ea_cos(a), up, down, right, dy, y_left, y_right, low_left, low_right;

    if (!c)
        return;
    up = fixdiv(above - 1, c) - 4;
    down = fixdiv(below - 1, c) - 4;
    right = win->x + win->w - pt->x;
    dy = fixmul(fixmul(right, fixdiv(ea_sin(a), c)), vw->squash);
    if (dy > 0) {
        y_left = pt->y - up;
        y_right = dy + pt->y - up;
        low_right = pt->y + down;
        low_left = pt->y + down - dy;
    } else {
        dy = -dy;
        y_left = pt->y + dy - up;
        y_right = pt->y - up;
        low_right = pt->y + down - dy;
        low_left = pt->y + down;
    }
    raster_fill(win->x, win->y, right + 4, y_left - win->y, top);
    raster_fill(win->x, low_left, right + 4, win->y + win->h - low_left, bottom);
    raster_fill(pt->x, win->y, right, y_right - win->y + 3, top);
    raster_fill(pt->x, low_right, right, win->y + win->h - low_right, bottom);
}

/* 0x34f1c: a panel's picture on its quad, as two triangles (0 1 2, 2 3 0) */
static void ring_panel(const raster_tex *t, const int uv[8], const int q[8])
{
    int a[6], b[6], xy[6];

    a[0] = uv[0] >> 16; a[1] = uv[1] >> 16; a[2] = uv[2] >> 16;
    a[3] = uv[3] >> 16; a[4] = uv[4] >> 16; a[5] = uv[5] >> 16;
    raster_tri(t, a, q);
    b[0] = uv[4] >> 16; b[1] = uv[5] >> 16; b[2] = uv[6] >> 16;
    b[3] = uv[7] >> 16; b[4] = uv[0] >> 16; b[5] = uv[1] >> 16;
    xy[0] = q[4]; xy[1] = q[5]; xy[2] = q[6]; xy[3] = q[7]; xy[4] = q[0]; xy[5] = q[1];
    raster_tri(t, b, xy);
}

/* sky_ring_draw 0x4ace8: the seven panels ahead between eight of the ring's points, from
   the one 70 degrees left of the heading; the ring's height and its place on the horizon
   from the INFO (scaled to the view), tilted with the camera's roll */
static void ring_draw(int yaw, int roll, const vec3 *pos, const int m[9], const scr2 *pt,
                      const view_window *vw, const road_window *win, int plain)
{
    const unsigned char *shape = g_fam.horizon ? shpi_find(g_fam.horizon, "horz") : 0;
    int start, scale, y_off, height, w, h, top_x, top_y, low_x, low_y, i, screen;
    vec3 ring[32], p[8], t;
    scr2 s[8];
    raster_tex tex;

    if (!shape)
        return;
    w = rd16(shape + 4);
    h = rd16(shape + 6);
    start = (int)((unsigned)yaw - 0x320000) / 0x100000 % 16;
    if (start < 0)
        start += 16;
    ring_points(ring);
    t.x = (-pos->x >> 10) >> 8;
    t.y = (-pos->y >> 10) >> 8;
    t.z = (-pos->z >> 10) >> 8;
    for (i = 0; i < 8; i++) {
        vec3 q;

        q.x = (ring[start + i].x >> 8) + t.x;
        q.y = (ring[start + i].y >> 8) + t.y;
        q.z = (ring[start + i].z >> 8) + t.z;
        rotate(&q, m, &p[i]);
    }
    project_points(8, p, s, 16);

    screen = g_view_res ? 640 : 320;       /* 0x143e30, 0x143e38; 0x143e40 the left, 0 */
    scale = vw->squash * vw->w / screen;
    y_off = fixmul(scale, g_info.ring_y);
    height = fixmul(scale, g_info.ring_height);
    if (g_road_mirror)
        roll = -roll;
    {
        int a = roll >> 8, up = height - y_off, low = height + y_off;

        top_x = ea_sin(a) * up >> 16;
        top_y = -(ea_cos(a) * up >> 16);
        low_x = ea_sin(a) * low >> 16;
        low_y = -(ea_cos(a) * low >> 16);
        if (!plain)
            ring_fill(roll, pt, up, low, vw, win, shape[16], shape[16 + w * h - 1]);
    }
    shape_tex(shape, &tex);
    for (i = 0; i < 7; i++) {
        int panel = start + i, q[8], uv[8];

        q[0] = s[i].x + top_x;      q[1] = s[i].y + top_y;
        q[2] = s[i + 1].x + top_x;  q[3] = s[i + 1].y + top_y;
        q[4] = s[i + 1].x - low_x;  q[5] = s[i + 1].y - low_y;
        q[6] = s[i].x - low_x;      q[7] = s[i].y - low_y;
        /* off the screen to one side, facing away, or too far out */
        if (!g_road_mirror) {
            if ((q[0] > screen && q[2] > screen) || (q[4] < 0 && q[6] < 0)
                || q[0] > q[2] || q[6] > q[4])
                continue;
        } else if ((q[4] > screen && q[6] > screen) || (q[0] < 0 && q[2] < 0)
                   || q[0] < q[2] || q[6] < q[4])
            continue;
        if (q[0] <= -0x500 || q[2] <= -0x500 || q[4] <= -0x500 || q[6] <= -0x500
            || q[0] >= 0x500 || q[2] >= 0x500 || q[4] >= 0x500 || q[6] >= 0x500)
            continue;
        if (!plain) {
            ring_uv(g_info.last_used && (panel >> 3 & 1) ? 8 + (panel & 7) : panel & 7, w, h, uv);
            ring_panel(&tex, uv, q);
        } else {
            /* the F4 sky: above the ring's line the sky's colour, below it the ground's */
            int xy[8];

            xy[0] = s[i].x;     xy[1] = win->y;
            xy[2] = s[i + 1].x; xy[3] = win->y;
            xy[4] = s[i + 1].x; xy[5] = s[i + 1].y;
            xy[6] = s[i].x;     xy[7] = s[i].y;
            poly_fill(4, xy, poly_solid_rows, shape[16]);
            xy[1] = xy[3] = win->y + win->h;
            poly_fill(4, xy, poly_solid_rows, shape[16 + w * h - 1]);
        }
    }
}

void sky_draw(int yaw, int roll, const vec3 *pos, const int m[9], const view_window *vw,
              const road_window *win, int plain)
{
    const unsigned char *shape;
    int a = yaw >> 14, w, h, cy, top, bottom, off;
    vec3 d, v;
    scr2 s;

    /* where the horizon is: a point far ahead along the heading, on the screen */
    d.x = (int)((unsigned)fix_sin(a) << 8) >> g_view.far_shift;
    d.y = 0;
    d.z = (int)((unsigned)fix_cos(a) << 8) >> g_view.far_shift;
    rotate(&d, m, &v);
    project_points(1, &v, &s, 16);

    /* the circuits (and CL3, AL3): the ring, if it has a height (0x4a9d0) */
    if (g_info.ring_type) {
        if (g_info.height_far > 0)
            ring_draw(yaw, roll, pos, m, &s, vw, win, plain);
        return;
    }
    shape = g_fam.horizon ? shpi_find(g_fam.horizon, "horz") : 0;
    if (!shape)
        return;
    w = rd16(shape + 4);
    h = rd16(shape + 6);
    cy = rd16(shape + 0xa);
    off = (fixmul(g_road_mirror ? -yaw : yaw, 0x50000) >> 16) % w;
    if (plain)
        h = cy = 0;
    top = s.y - (cy * g_squash >> 16);
    bottom = top + ((h - 1) * g_squash >> 16);
    raster_fill(win->x, win->y, win->w, top, shape[16]);
    if (!plain)
        strip(shape, off, win->x, top);
    raster_fill(win->x, bottom, win->w, win->y + win->h - bottom, shape[16 + rd16(shape + 4) * rd16(shape + 6) - 1]);
}

void sky_behind(int yaw, int pitch, int node, int lens_h, int dy, int width, int height)
{
    int type = node_ptr((int)((unsigned)(node - 3) & g_track.node_mask))[7];
    int across, row, step, w, tile, bottom, flip = 0, xy[8];
    const unsigned char *shape;
    raster_tex t;

    if (type == 4 || type == 7 || type == 9) {
        raster_fill(0, 0, width, height, g_hud_colour[12]);
        return;
    }
    if (type == 8) {
        raster_fill(0, 0, width, height, g_hud_colour[3]);
        return;
    }
    across = -(((int)((unsigned)yaw * 2) >> 16) + 0x100) & 0x1ff;
    row = (int)((unsigned)pitch << 2) >> 16;
    if (row > 0x200)
        row -= 0x400;
    if (g_view_res)
        row = row * 480 / 200;
    row += lens_h / 2 + dy;
    shape = g_fam.horizon ? shpi_find(g_fam.horizon, "horz") : 0;
    if (!shape)
        return;
    w = rd16(shape + 4);
    step = 0x1000000 / w;
    if (!g_view_res) {
        step /= 2;
        across /= 2;
    }
    tile = fixmul(step, w);
    bottom = fixmul(step, rd16(shape + 6) - 1);
    row -= fixmul(step, rd16(shape + 0xa));
    if (config_word(CFG_TRACK) == 5)
        row = -17;
    raster_fill(0, 0, width, row, shape[16]);
    while (across > width) {
        across -= tile;
        flip ^= 1;
    }
    shape_tex(shape, &t);
    for (; across > -tile; across -= tile, flip ^= 1) {
        int turned = g_info.last_used && flip;
        xy[0] = xy[6] = turned ? across + tile : across;
        xy[2] = xy[4] = turned ? across : across + tile;
        xy[1] = xy[3] = row;
        xy[5] = xy[7] = row + bottom;
        raster_quad_whole(&t, xy);
    }
    raster_fill(0, row + bottom, width, height - row + bottom,
                shape[16 + rd16(shape + 4) * rd16(shape + 6) - 1]);
}
