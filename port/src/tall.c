/* tall.c - see tall.h */
#include "tall.h"
#include "video.h"
#include <stdlib.h>
#include <string.h>

static unsigned char *g_pic;               /* the view at 240 rows */
static unsigned char *g_kept;              /* its 200-row copy as the lists found it */
static int *g_from;                        /* a screen pixel: the view picture's pixel the
                                              lists put there, */
static unsigned short *g_when;             /* in the frame of this number */
static unsigned short g_frame = 1;         /* this frame's, 0 when its view is not tall */
static int g_ty0, g_ty1;                   /* its window's rows */
static unsigned char g_row[TALL_W];

/* the view's row for a 200-row picture's row y (at its middle) */
static int tall_row(int y)
{
    return (y * 2 * TALL_H + TALL_H) / 400;
}

/* each row shown (video_rows' map): the view's pixels where the screen still has them,
   from the row as far from y as the lists moved the view, 6/5 of it */
static const unsigned char *out_row(int y, int row)
{
    const unsigned char *fb = g_fb + (long)row * TALL_W;
    const int *from = g_from + (long)row * TALL_W;
    const unsigned short *when = g_when + (long)row * TALL_W;
    int x;

    for (x = 0; x < TALL_W; x++) {
        int s = from[x], off, ty;
        if (!g_frame || when[x] != g_frame || fb[x] != g_kept[s]) {
            g_row[x] = fb[x];
            continue;
        }
        off = s / TALL_W - row;
        ty = y + (off * 6 + (off < 0 ? -2 : 2)) / 5;
        ty = ty < g_ty0 ? g_ty0 : ty >= g_ty1 ? g_ty1 - 1 : ty;
        g_row[x] = g_pic[(long)ty * TALL_W + s % TALL_W];
    }
    return g_row;
}

int tall_on(int on)
{
    if (on && !g_pic) {
        g_pic = calloc(TALL_W * TALL_H, 1);
        g_kept = calloc(TALL_W * 200, 1);
        g_from = calloc(TALL_W * 200, sizeof *g_from);
        g_when = calloc(TALL_W * 200, sizeof *g_when);
        if (!g_pic || !g_kept || !g_from || !g_when)
            on = 0;
    }
    if (!on) {
        free(g_pic);
        free(g_kept);
        free(g_from);
        free(g_when);
        g_pic = g_kept = 0;
        g_from = 0;
        g_when = 0;
    }
    video_row_source(on ? out_row : 0);
    return on;
}

unsigned char *tall_picture(void)
{
    return g_pic;
}

static unsigned short g_count;

void tall_frame(void)
{
    g_frame = 0;
}

void tall_view_done(const view_window *t, const view_window *lo, unsigned char *view)
{
    int y;

    /* (a new number each frame, never 0; one 65,536 frames old could pass, where the
       screen still has the colour the view had then) */
    if (!++g_count)
        g_count = 1;
    g_frame = g_count;
    g_ty0 = t->y;
    g_ty1 = t->y + t->ch;
    for (y = lo->y; y < lo->y + lo->ch && y < 200; y++) {
        int ty = tall_row(y);
        if (ty < t->y)
            ty = t->y;
        if (ty >= t->y + t->ch)
            ty = t->y + t->ch - 1;
        memcpy(view + (long)y * TALL_W + lo->x, g_pic + (long)ty * TALL_W + lo->x, (size_t)lo->cw);
        memcpy(g_kept + (long)y * TALL_W + lo->x, view + (long)y * TALL_W + lo->x, (size_t)lo->cw);
    }
}

void tall_copied(long to, long from, int n, unsigned rate)
{
    int j;

    if (!g_frame || to < 0 || from < 0 || to + n > TALL_W * 200)
        return;
    for (j = 0; j < n; j++) {
        long s = from + (long)((unsigned)j * rate >> 16);
        if (s < TALL_W * 200) {
            g_from[to + j] = (int)s;
            g_when[to + j] = g_frame;
        }
    }
}
