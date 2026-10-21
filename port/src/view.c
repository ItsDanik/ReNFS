/* The race's views (NOTES §4.7). */
#include "view.h"
#include "exe.h"
#include "sky.h"
#include "roaddraw.h"
#include <string.h>

view_window g_windows[2][VIEW_WINDOWS];
int g_view_res;
int g_race_rows;
int g_view_tall;
static view_window g_windows_tall[VIEW_WINDOWS];
view_rec g_view_ahead, g_view_mirror, g_view_outside;
const view_rec *g_view_now;
int g_view_detail;
int g_view_redraw;

static int layout(int rec, int res)
{
    return 0xe8b10 + 0x16c * res + 52 * exe_i32(rec + 8);
}

void view_init(int res, int car, int horizon)
{
    int r, i, k, h, d, d480;

    g_view_res = res;
    for (r = 0; r < 2; r++)
        for (i = 0; i < VIEW_WINDOWS; i++)
            memcpy(&g_windows[r][i], exe_at(0xe85c0 + 0x2a8 * r + 40 * i, 40), 40);
    g_view_ahead.fov = exe_i32(0xe8de8);
    g_view_outside.fov = exe_i32(0xe8e08);
    g_view_mirror.fov = exe_i32(0xe8e28);
    /* each its layout's (0x14630: 0xe8b10 + 0x16c × resolution + 52 × the record's +8):
       the window (+4, handed to 0x14154) and the lens's height (+0x20) */
    g_view_ahead.mode = exe_i32(layout(0xe8de8, res) + 4);
    g_view_outside.mode = exe_i32(layout(0xe8e08, res) + 4);
    g_view_mirror.mode = exe_i32(layout(0xe8e28, res) + 4);
    g_view_ahead.lens_h = exe_i32(layout(0xe8de8, res) + 0x20);
    g_view_outside.lens_h = exe_i32(layout(0xe8e08, res) + 0x20);
    g_view_mirror.lens_h = exe_i32(layout(0xe8e28, res) + 0x20);
    {
        view_rec *v[3] = { &g_view_ahead, &g_view_outside, &g_view_mirror };
        static const int rec[3] = { 0xe8de8, 0xe8e08, 0xe8e28 };

        for (k = 0; k < 3; k++) {
            v[k]->layout = exe_i32(rec[k] + 8);
            v[k]->current = v[k]->layout;
            for (i = 0; i < 4; i++)
                v[k]->box[i] = exe_i32(layout(rec[k], res) + 0x24 + 4 * i);
        }
    }

    /* the view's height above the dashboard, the car's own (0x145d0) */
    h = exe_i32(0xe8e4c + 0x90 * res + 12 * car);
    g_windows[res][0].ch = g_windows[res][1].ch = h;
    g_windows[res][2].ch = h / 2;

    /* the horizon where the car's driver sees it: the three windows of the view from
       the car (the first layout's), both resolutions, 108 the middle */
    d = horizon - 108;
    d480 = d * 480 / 200;
    for (k = 0; k < 3; k++) {
        view_window *w = &g_windows[0][exe_i32(0xe8b14 + 4 * k)];
        w->dy = w->h * d / 200;
        w = &g_windows[1][exe_i32(0xe8c80 + 4 * k)];
        w->dy = w->h * d480 / 480;
    }

    /* (the port's) 320x240: the 320x200 windows 6/5 as tall, the lens's picture as tall
       for its width (the same field of view in square pixels); not those of detail 2
       (the lists double their rows) nor the mirror's */
    for (i = 0; i < VIEW_WINDOWS; i++) {
        const view_window *lo = &g_windows[0][i];
        view_window *t = &g_windows_tall[i];
        int bottom = (lo->y + lo->ch) * 6 / 5;

        *t = *lo;
        t->tall = g_view_tall && lo->squash && i != g_view_mirror.mode;
        for (k = 0; k < 0x16c / 52 && t->tall; k++)    /* (the empty layouts' all 0) */
            if (exe_i32(0xe8b10 + 52 * k + 4 + 4 * 2) == i && exe_i32(0xe8b10 + 52 * k + 4) != i)
                t->tall = 0;
        if (!t->tall)
            continue;
        t->squash = lo->squash * 6 / 5;
        t->dy = lo->dy * 6 / 5;
        t->h = lo->h * 6 / 5;
        t->y = lo->y * 6 / 5;
        t->ch = bottom - t->y;
    }
}

/* the window `mode` of the views ahead and outside, or the mirror's */
static const view_window *window_of(const view_rec *v, int mode)
{
    if (g_view_tall && !g_view_res && v != &g_view_mirror)
        return &g_windows_tall[mode];
    return &g_windows[g_view_res][mode];
}

const view_window *view_window_lo(const view_window *w)
{
    return w >= g_windows_tall && w < g_windows_tall + VIEW_WINDOWS
        ? &g_windows[0][w - g_windows_tall] : w;
}

/* the lens (0x14284: 0x83c18, 0x83c5b): the centre in the middle of what it covers, the
   focal length from half the field of view on the 1024-a-turn table */
static void lens(int x, int y, int w, int h, int fov)
{
    unsigned a = ((unsigned)(fov / 2) << 10) / 360;

    g_view.cx = x + (int)((unsigned)w >> 1);
    g_view.cy = y + (int)((unsigned)h >> 1);
    g_view.focal = (int)((unsigned)fix_cos(a) * ((unsigned)w >> 1) / (unsigned)fix_sin(a));
}

void view_setup(view_rec *v, int mode)
{
    const view_window *w = window_of(v, mode);

    v->mode = mode;
    v->win = w;
    sky_tables(w->w, v->fov / 2, (short)((v->fov << 8) / 360 * 5), w->squash * w->w / 320);
    lens(w->x + w->dx, w->y + w->dy, w->w, w->h, v->fov);
    view_set_focal(g_view.focal);
}

int view_set_mode(view_rec *v, int n, int detail)
{
    int at = 0xe8b10 + 0x16c * g_view_res + 52 * n, i;

    if (n < 0)
        return 0;
    v->current = n;
    v->lens_h = exe_i32(at + 0x20);
    for (i = 0; i < 4; i++)
        v->box[i] = exe_i32(at + 0x24 + 4 * i);
    view_setup(v, exe_i32(at + 4 + 4 * detail));
    g_view_redraw = 1;
    return 1;
}

void view_layout(view_rec *v, int n)
{
    view_set_mode(v, n, g_view_detail);
}

view_rec *view_of_kind(int kind)
{
    return kind ? &g_view_outside : &g_view_ahead;
}

/* the turns about each axis (0x58500 x, 0x5855c y, 0x585b8 z), 0x1000000 a turn */
void mat_turn(int axis, int angle, int m[9])
{
    int s = ea_sin(angle >> 8), c = ea_cos(angle >> 8);

    memset(m, 0, 9 * sizeof *m);
    m[axis * 4] = 0x10000;
    switch (axis) {
    case 0: m[4] = c; m[5] = -s; m[7] = s; m[8] = c; break;
    case 1: m[0] = c; m[2] = s; m[6] = -s; m[8] = c; break;
    default: m[0] = c; m[1] = -s; m[3] = s; m[4] = c; break;
    }
}

/* a matrix scaling one axis (0x58614 x, 0x58650 y) */
static void scale(int axis, int by, int m[9])
{
    memset(m, 0, 9 * sizeof *m);
    m[0] = m[4] = m[8] = 0x10000;
    m[axis * 4] = by;
}

void mat_mul(const int a[9], const int b[9], int out[9])
{
    int r, c;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            out[3 * r + c] = (int)((long long)a[3 * r] * b[c] >> 16)
                           + (int)((long long)a[3 * r + 1] * b[3 + c] >> 16)
                           + (int)((long long)a[3 * r + 2] * b[6 + c] >> 16);
}

void view_matrix(race_camera *c)
{
    int x[9], y[9], z[9], s[9], f[9], half[9], yz[9], yzx[9], t[9];

    mat_turn(0, c->pitch, x);
    mat_turn(2, c->roll, z);
    mat_turn(1, c->yaw, y);
    scale(1, g_view_now->win->squash, s);
    scale(0, g_road_mirror ? -0x10000 : 0x10000, f);
    mat_mul(y, z, yz);
    mat_mul(yz, x, yzx);
    mat_mul(yzx, s, t);
    mat_mul(t, f, c->m);
    /* the mirror looks back: half a turn more, neither squashed nor turned over */
    mat_turn(1, 0x800000, half);
    mat_mul(yzx, half, c->mm);
}

const int *view_select(race_camera *c, int mirror)
{
    const view_rec *v = mirror ? c->mirror : c->ahead;
    const view_window *w = v->win;

    g_view_now = v;
    /* the original also adds a shake to the centre ahead (0x129ec0, set only by 0x14860,
       which nothing calls) and shifts it by 0xf456c (never set): both always 0 */
    lens(w->x + w->dx, w->y + w->dy, w->w, w->h, v->fov);
    return mirror ? c->mm : c->m;
}
