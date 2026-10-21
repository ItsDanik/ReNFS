/* The smoke of a crash drawn (NOTES §4.10): see smoke.h. */
#include <stdio.h>
#include <string.h>

#include "smoke.h"
#include "cardraw.h"
#include "font.h"
#include "poly.h"
#include "racepal.h"
#include "raster.h"
#include "video.h"

smoke_art g_smoke;

int smoke_load(crash_smoke *s)
{
    shape pal;
    int i;

    smoke_free();
    if (!fsh_load(&g_smoke.file, "SIMDATA/MISC/SMOKE.QFS")
        || !fsh_find(&g_smoke.file, "smok", &g_smoke.puff)
        || !fsh_find(&g_smoke.file, "!PAL", &pal)) {
        fprintf(stderr, "renfs: SIMDATA/MISC/SMOKE.QFS is missing\n");
        smoke_free();
        return 0;
    }
    /* the tyres' dust (0xf6db8, 0xf6dc8) */
    for (i = 0; i < 4; i++) {
        char name[5];

        snprintf(name, sizeof name, "smo%d", i + 1);
        fsh_find(&g_smoke.file, name, &g_smoke.dust[i]);
        snprintf(name, sizeof name, "dir%d", i + 1);
        fsh_find(&g_smoke.file, name, &g_smoke.dirt[i]);
    }
    pal_tint(0, 0x100, (unsigned char *)pal.head + 0x10);
    pal_queue_remap(g_smoke.file.d, pal.head + 0x10);
    for (i = 0; i < SMOKE_PUFFS; i++) {
        s->order[i] = i;
        s->age[i] = -1;
    }
    s->timer = s->next = 0;
    return 1;
}

void smoke_free(void)
{
    fsh_free(&g_smoke.file);
    memset(&g_smoke.puff, 0, sizeof g_smoke.puff);
    memset(g_smoke.dust, 0, sizeof g_smoke.dust);
    memset(g_smoke.dirt, 0, sizeof g_smoke.dirt);
}

void smoke_table(void)
{
    const unsigned char *p8 = video_palette8();
    int rgb[256][3], i, k, c;

    for (i = 0; i < 256; i++)
        for (k = 0; k < 3; k++)
            rgb[i][k] = (p8[3 * i + k] >> 2) * 0xff / 0x3f;
    for (i = 0; i < 256; i++) {
        int add[3];

        for (k = 0; k < 3; k++)
            add[k] = rgb[i][k] - rgb[i][k] / 4;
        for (c = 0; c < 256; c++) {
            int r = rgb[c][0] + add[0], g = rgb[c][1] + add[1], b = rgb[c][2] + add[2];

            if (r > 0xff)
                r = 0xff;
            if (g > 0xff)
                g = 0xff;
            if (b > 0xff)
                b = 0xff;
            g_smoke.add[i][c] = (unsigned char)nearest_colour((unsigned long)r << 16 | g << 8 | b);
        }
    }
}

/* 16.16 times a whole number, rounded by the last bit out (imul, shrd 16, adc) */
static int mulr(int a, int b)
{
    long long p = (long long)a * b;

    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

/* half, towards 0 */
static int half(int v)
{
    return (v - (v >> 31)) >> 1;
}

/* 0x96aa8: the picture stretched over w x h pixels from (x, y) (w < 0 mirrored, h < 0
   upside down), cut to the window; the source's pixels picked by steps in 16.16 from half
   a step in, each added to the screen's (0x9498d) or, solid, put on it unless 0xff
   (0x97038's 0x9473e) */
static void stretch_box(int w, int h, const shape *sh, int x, int y, int solid)
{
    int x0, y0, x1, y1, du, dv, u, v, i, cols[640];

    if (!w || !h || !sh->pixels)
        return;
    raster_clip(&x0, &y0, &x1, &y1);
    if (x >= x1 || y >= y1 || x + (w > 0 ? w : -w) <= x0 || y + (h > 0 ? h : -h) <= y0)
        return;
    du = (sh->w << 16) / w;
    dv = (sh->h << 16) / h;
    v = half(dv);
    if (h < 0) {
        h = -h;
        v += sh->h << 16;
    }
    u = half(du);
    if (w < 0) {
        u += sh->w << 16;
        w = -w;
    }
    if (y < y0) {
        v += dv * (y0 - y);
        h -= y0 - y;
        y = y0;
    }
    if (y + h > y1)
        h = y1 - y;
    if (x < x0) {
        u += du * (x0 - x);
        w -= x0 - x;
        x = x0;
    }
    if (x + w > x1)
        w = x1 - x;
    if (w > (int)(sizeof cols / sizeof *cols))
        w = sizeof cols / sizeof *cols;
    for (i = 0; i < w; i++)
        cols[i] = (u + i * du) >> 16;
    for (; h > 0; h--, y++, v += dv) {
        const unsigned char *src = sh->pixels + (v >> 16) * sh->w;
        unsigned char *dst = raster_row(y) + x;

        if (!RASTER_SHOWN(y))
            continue;
        if (solid) {
            for (i = 0; i < w; i++)
                if (src[cols[i]] != 0xff)
                    dst[i] = src[cols[i]];
        } else
            for (i = 0; i < w; i++)
                dst[i] = g_smoke.add[src[cols[i]]][dst[i]];
    }
}

static void stretch(int scale, const shape *sh, int x, int y, int solid)
{
    int a = scale < 0 ? -scale : scale;
    int w = (int)((unsigned)scale * (unsigned)sh->w) >> 16;
    int h = (int)((unsigned)a * (unsigned)sh->h) >> 16;

    if (scale >= 0)
        x -= mulr(scale, sh->cx);
    else
        x += mulr(scale, sh->w - sh->cx);
    stretch_box(w, h, sh, x, y - mulr(a, sh->cy), solid);
}

void smoke_stretch(int scale, const shape *sh, int x, int y)
{
    stretch(scale, sh, x, y, 0);
}

void smoke_stretch_solid(int scale, const shape *sh, int x, int y)
{
    stretch(scale, sh, x, y, 1);
}

void smoke_draw(crash_smoke *s, int dx, int dy)
{
    int shift = g_view.far_shift, i, j;
    vec3 t, seen[SMOKE_PUFFS];
    scr2 at[SMOKE_PUFFS];

    if (!g_smoke.puff.pixels)
        return;
    /* into the view: (place >> 11) less the camera's, turned (0x94db8, 0x94e10) */
    t.x = -g_camera->pos.x >> shift;
    t.y = -g_camera->pos.y >> shift;
    t.z = -g_camera->pos.z >> shift;
    for (i = 0; i < SMOKE_PUFFS; i++) {
        vec3 p;

        p.x = (s->pos[i].x >> shift) + t.x;
        p.y = (s->pos[i].y >> shift) + t.y;
        p.z = (s->pos[i].z >> shift) + t.z;
        rotate(&p, g_camera_m, &seen[i]);
        if (seen[i].z < 1)
            seen[i].z = 1;
        else if (seen[i].z > 0x7fff)
            seen[i].z = 0x7fff;
    }
    /* the order sorted again, far first: each moved back past the nearer ones */
    for (i = 1; i < SMOKE_PUFFS; i++)
        for (j = i; j > 0 && seen[s->order[j]].z > seen[s->order[j - 1]].z; j--) {
            int k = s->order[j];

            s->order[j] = s->order[j - 1];
            s->order[j - 1] = k;
        }
    project_points(SMOKE_PUFFS, seen, at, 16);
    for (i = 0; i < SMOKE_PUFFS; i++) {
        int k = s->order[i], size, fade;

        if (s->age[k] < 0 || seen[k].z <= 0x4ccc >> shift)
            continue;
        /* the lens over the distance, shrinking with age's square, then by 3/16 */
        size = (int)(((long long)g_view.focal << (16 - shift) << 16) / seen[k].z);
        fade = (0x10000 - s->age[k]) >> 8;
        fade = (0x10000 - fade * fade + 0xccc) >> 8;
        size = (int)((unsigned)(size >> 9) * (unsigned)fade) >> 8;
        size = (int)((unsigned)size * 0x300u);
        smoke_stretch(size >> 4, &g_smoke.puff, at[k].x + dx, at[k].y + dy);
    }
}
