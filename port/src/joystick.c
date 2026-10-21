/* The joystick: found at start-up, calibrated, read in the menus and in a race. */
#include "joystick.h"
#include "mister.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

joystick g_joy;

static void cal_path(char *path, size_t n)
{
    snprintf(path, n, "%s/joystick.cfg", files_home());
}

int joy_poll(void)
{
    int count[4], b;

    if (!joy_device(count, &b))
        return 0;
    g_joy.buttons = b & 0xf;
    g_joy.extra = b >> 4 & 3;
    /* (the game port's third and fourth axes: 0x12f280 the fourth, 0x12f284 the third;
       joyopt.cfg's remapping, 0x4e580 and 0x12f254, only with that file: never) */
    g_joy.x = count[0];
    g_joy.y = count[1];
    g_joy.throttle = count[3];
    g_joy.brake = count[2];
    return 1;
}

/* cfg_save_joystick 0x4d5ec */
static void cal_save(void)
{
    char path[1024];
    unsigned char b[JC_COUNT * 4];
    FILE *f;

    for (int i = 0; i < JC_COUNT; i++)
        for (int k = 0; k < 4; k++)
            b[i * 4 + k] = (unsigned char)((unsigned)g_joy.cal[i] >> 8 * k);
    cal_path(path, sizeof path);
    if ((f = fopen(path, "wb"))) {
        fwrite(b, 1, sizeof b, f);
        fclose(f);
    }
}

/* cfg_load_joystick 0x4d4a0: the calibration kept, if the stick let go is near its
   middle (75 counts) and inside the menus' thresholds; else the screen. A file of
   another size is removed */
static void cal_finish(void);

static void cal_load(void)
{
    char path[1024];
    unsigned char b[JC_COUNT * 4 + 1];
    const int *c = g_joy.cal;
    FILE *f;
    size_t n = 0;

    cal_path(path, sizeof path);
    if ((f = fopen(path, "rb"))) {
        n = fread(b, 1, sizeof b, f);
        fclose(f);
        if (n == JC_COUNT * 4) {
            g_joy.on = 1;
            for (int i = 0; i < JC_COUNT; i++)
                g_joy.cal[i] = (int)((unsigned)b[i * 4] | (unsigned)b[i * 4 + 1] << 8
                                     | (unsigned)b[i * 4 + 2] << 16 | (unsigned)b[i * 4 + 3] << 24);
            joy_poll();
            if (c[JC_X_MID] - 0x4b <= g_joy.x && g_joy.x <= c[JC_X_MID] + 0x4b
                && c[JC_Y_MID] - 0x4b <= g_joy.y && g_joy.y <= c[JC_Y_MID] + 0x4b
                && c[JC_Y_LO] <= g_joy.y && g_joy.y <= c[JC_Y_HI]
                && c[JC_X_LO] <= g_joy.x && g_joy.x <= c[JC_X_HI])
                return;
        } else
            remove(path);
    }
#ifdef MISTER_HYBRID
    if (mister_on()) {
        /* (the MiSTer's: the core's sticks and triggers are all alike, their whole range:
           no calibration screen; Options, Controls still has it) */
        int *k = g_joy.cal;
        k[JC_X_MIN] = k[JC_Y_MIN] = k[JC_T_MIN] = k[JC_B_MIN] = 0;
        k[JC_X_MID] = k[JC_Y_MID] = k[JC_T_MID] = 256;
        k[JC_X_MAX] = k[JC_Y_MAX] = k[JC_T_MAX] = k[JC_B_MAX] = 511;
        cal_finish();
        g_joy.on = 1;
        return;
    }
#endif
    fe_joystick_calibrate(0, 0);
    /* (0x4d670: the force feedback wheel, with joyopt.cfg's flag: not here) */
}

void joy_startup(void)
{
    int count[4], b, mask;

    memset(&g_joy, 0, sizeof g_joy);
    mask = joy_device(count, &b);          /* (EA's 0x94fa8: the axes that answer) */
    if (mask == 0)
        return;
    g_joy.present = 1;
    if (mask == 3)                         /* the stick alone */
        g_joy.has_y = 1;
    else if (mask == 0xc)                  /* the second stick's axes alone */
        g_joy.has_throttle = g_joy.has_pedals = 1;
    else {
        g_joy.has_y = 1;
        g_joy.has_throttle = count[3] > 0;
        g_joy.has_pedals = count[2] > 0;
    }
    g_joy.has_x = g_joy.has_y;
    /* (0x4dda4: joyopt.cfg's axes and buttons; without it, every one there) */
    cal_load();
}

/* the counts from the middle as -lo..hi: (d * scale >> 16) less a dead zone of `dead`
   counts on each side */
static int scaled(int d, int neg, int pos, int dead_neg, int dead_pos, int lo, int hi)
{
    if (d < 0) {
        d = (d * neg >> 16) + dead_neg;
        return d > 0 ? 0 : d < -lo ? -lo : d;
    }
    d = (d * pos >> 16) - dead_pos;
    return d < 0 ? 0 : d > hi ? hi : d;
}

void joy_race_read(void)
{
    const int *c = g_joy.cal;
    int x, y, t, b;

    if (!g_joy.on || !joy_poll()) {
        g_joy.held = g_joy.steer = g_joy.pedal = 0;
        return;
    }
    g_joy.held = g_joy.buttons;
    x = g_joy.x - c[JC_X_MID];
    y = g_joy.y - c[JC_Y_MID];
    t = g_joy.throttle;
    b = g_joy.brake;
    /* (0x4e680, an axis turned round: joyopt.cfg's, never) */
    if (!g_joy.has_pedals)
        t -= c[JC_T_MID];
    x = scaled(x, c[JC_X_NEG], c[JC_X_POS], 3, 3, 0x20, 0x1f);
    y = scaled(y, c[JC_Y_NEG], c[JC_Y_POS], 3, 3, 0x1e, 0x1e);
    if (g_joy.has_pedals) {
        /* two pedals: each 0x1f let go less its travel, the brake's less the throttle's */
        t = 0x1f - (t * c[JC_T_SCALE] >> 16);
        b = 0x1f - (b * c[JC_B_SCALE] >> 16);
        if (t < 0)
            t = 0;
        if (b < 0)
            b = 0;
        y = b - t;
        y = y < -0x1e ? -0x1e : y > 0x1e ? 0x1e : y;
    } else if (g_joy.has_throttle)
        y = scaled(t, c[JC_T_NEG], c[JC_T_POS], 3, 4, 0x1e, 0x1e);
    g_joy.steer = x;
    g_joy.pedal = y;
}

int joy_menu_key(void)
{
    const int *c = g_joy.cal;
    int dir;

    if (!g_joy.on || !joy_poll())
        return 0;
#ifdef MISTER_HYBRID
    if (mister_on()) {                     /* (the MiSTer menu's OK and Back) */
        int k = mister_menu_key();
        if (k)
            return k;
    } else
#endif
    {
        if (g_joy.buttons & 1)
            return 0x20;
        if (g_joy.buttons & 2)
            return 0x0d;
    }
    dir = g_joy.y < c[JC_Y_LO] ? 0x4800 : g_joy.y > c[JC_Y_HI] ? 0x5000
        : g_joy.x < c[JC_X_LO] ? 0x4b00 : g_joy.x > c[JC_X_HI] ? 0x4d00 : 0;
    return dir;
}

/* the calibration screen */

static int g_cx0, g_cx1, g_cy0, g_cy1;     /* the clip (0x812ec) */

static void clip(int x0, int x1, int y0, int y1)
{
    g_cx0 = x0 < 0 ? 0 : x0;
    g_cx1 = x1 > g_fb_w ? g_fb_w : x1;
    g_cy0 = y0 < 0 ? 0 : y0;
    g_cy1 = y1 > g_fb_h ? g_fb_h : y1;
}

/* drawshape 0x81280 (a copy) or drawshape_see_through_at 0x815c8 (255 left), cut to
   the clip */
static void blit(const shape *s, int x, int y, int see_through)
{
    for (int r = 0; r < s->h; r++) {
        int sy = y + r;
        if (sy < g_cy0 || sy >= g_cy1)
            continue;
        for (int i = 0; i < s->w; i++) {
            int sx = x + i;
            unsigned char p = s->pixels[r * s->w + i];
            if (sx >= g_cx0 && sx < g_cx1 && !(see_through && p == 255))
                g_fb[sy * g_fb_w + sx] = p;
        }
    }
}

/* 35.0 (or n) / (range + 1) in 16.16, as the original divides */
static int scale_of(int n, int range)
{
    return (int)(((long long)n << 32) / ((long long)(range + 1) << 16));
}

/* the end: the menus' thresholds half way to each end, the scales from the ranges
   (at most 0x7ffa; ranges of 0 or 1 kept as they are) */
static void cal_finish(void)
{
    int *c = g_joy.cal;

    c[JC_Y_NEG] = c[JC_Y_MID] - c[JC_Y_MIN];
    c[JC_Y_POS] = c[JC_Y_MAX] - c[JC_Y_MID];
    c[JC_Y_LO] = (c[JC_Y_NEG] >> 1) + c[JC_Y_MIN];
    c[JC_X_NEG] = c[JC_X_MID] - c[JC_X_MIN];
    c[JC_Y_HI] = (c[JC_Y_POS] >> 1) + c[JC_Y_MID];
    c[JC_X_LO] = (c[JC_X_NEG] >> 1) + c[JC_X_MIN];
    c[JC_X_POS] = c[JC_X_MAX] - c[JC_X_MID];
    c[JC_X_HI] = (c[JC_X_POS] >> 1) + c[JC_X_MID];
    c[JC_T_POS] = c[JC_T_MAX] - c[JC_T_MID];
    c[JC_T_NEG] = c[JC_T_MID] - c[JC_T_MIN];
    if (c[JC_X_NEG] > 0x7ffa)
        c[JC_X_NEG] = 0x7ffa;
    if (c[JC_Y_NEG] > 0x7ffa)
        c[JC_Y_NEG] = 0x7ffa;
    if (c[JC_T_NEG] > 0x7ffa)
        c[JC_T_NEG] = 0x7ffa;
    if (c[JC_X_POS] > 0x7ffa)
        c[JC_X_POS] = 0x7ffa;
    if (c[JC_Y_POS] > 0x7ffa)
        c[JC_Y_POS] = 0x7ffa;
    c[JC_T_SCALE] = c[JC_T_MAX] - c[JC_T_MIN];
    c[JC_B_SCALE] = c[JC_B_MAX] - c[JC_B_MIN];
    if (c[JC_X_NEG] > 1)
        c[JC_X_NEG] = scale_of(0x23, c[JC_X_NEG]);
    if (c[JC_X_POS] > 1)
        c[JC_X_POS] = scale_of(0x23, c[JC_X_POS]);
    if (c[JC_Y_NEG] > 1)
        c[JC_Y_NEG] = scale_of(0x23, c[JC_Y_NEG]);
    if (c[JC_Y_POS] > 1)
        c[JC_Y_POS] = scale_of(0x23, c[JC_Y_POS]);
    if (c[JC_T_NEG] > 1)
        c[JC_T_NEG] = scale_of(0x23, c[JC_T_NEG]);
    if (c[JC_T_POS] > 1)
        c[JC_T_POS] = scale_of(0x26, c[JC_T_POS]);
    if (g_joy.has_pedals || g_joy.has_throttle) {
        if (c[JC_T_SCALE] > 1)
            c[JC_T_SCALE] = scale_of(0x24, c[JC_T_SCALE]);
        if (c[JC_B_SCALE] > 1)
            c[JC_B_SCALE] = scale_of(0x24, c[JC_B_SCALE]);
    }
}

/* an axis's least and most widened to the count, its middle half way between them */
static void widen(int v, int *min, int *mid, int *max)
{
    if (v > *max) {
        *max = v;
        *mid = *min + ((v - *min) >> 1);
    } else if (v < *min) {
        *min = v;
        *mid = ((*max - v) >> 1) + v;
    }
}

void fe_joystick_calibrate(int from_menu, int race_menu)
{
    static const unsigned char black[768];
    unsigned char pal[768];
    char path[256];
    int *c = g_joy.cal;
    int x0 = 0x10e, y0 = 0xdc, x = x0, y = y0, done = 0, redraw = 0;
    int bp = 0, tp = 0;                    /* the pedals' bars, in hundredths (esi, edi) */
    fsh_file art;
    shape bgnd, knob, pedl, sldr;

    if (!g_joy.present)
        return;
    video_palette(black, 0, 256);          /* (pal_clear, setpalette) */
    snprintf(path, sizeof path, "%s/%s", art_dir(), race_menu ? "JOYCHK.QFS" : "JOYCAL.QFS");
    if (!fsh_load(&art, path) || !fsh_palette(&art, 0, pal) || !fsh_find(&art, "bgnd", &bgnd)
        || !fsh_find(&art, "knob", &knob)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        g_joy.on = 0;
        return;
    }
    if (g_joy.has_throttle && (!fsh_find(&art, "pedl", &pedl) || !fsh_find(&art, "sldr", &sldr)))
        g_joy.has_throttle = g_joy.has_pedals = 0;
    if (!joy_poll()) {
        fsh_free(&art);
        g_joy.on = 0;
        return;
    }
    /* the ranges start a count either side of where the stick is */
    g_joy.on = 1;
    c[JC_X_MID] = g_joy.x;
    c[JC_Y_MID] = g_joy.y;
    c[JC_T_MID] = g_joy.throttle;
    c[JC_X_MAX] = c[JC_X_MID] + 1;
    c[JC_X_MIN] = c[JC_X_MID] - 1;
    c[JC_Y_MAX] = c[JC_Y_MID] + 1;
    c[JC_Y_MIN] = c[JC_Y_MID] - 1;
    c[JC_T_MAX] = c[JC_T_MID] + 1;
    c[JC_T_MIN] = c[JC_T_MID] - 1;
    c[JC_B_MAX] = g_joy.brake + 1;
    c[JC_B_MIN] = g_joy.brake - 1;
    settimeout(12000);
    clip(0, g_fb_w, 0, g_fb_h);
    blit(&bgnd, bgnd.x, bgnd.y, 0);
    blit(&knob, x, y, 1);
    if (g_joy.has_throttle)
        blit(&pedl, 0x217, 0x60, 0);
    pal_fade_up(pal, 0, 256);
    fe_styles_init();
    fe_style(1);
    while (!done) {
        int v;

        if (redraw) {
            clip(0, g_fb_w, 0, g_fb_h);
            if (g_joy.has_throttle)
                blit(&pedl, 0x217, 0x60, 0);
            clip(0x9b, 0x1db, 0x8c, 0x190);
            blit(&bgnd, bgnd.x, bgnd.y, 0);
            blit(&knob, x, y, 1);
            if (g_joy.has_throttle) {
                /* the bars filled from the bottom as far as the pedal is pressed: the
                   brake (a throttle back) left, the throttle right */
                clip(0x218, 0x229, 0x93 + (100 - bp) * 0xba / 100, 0x14d);
                blit(&sldr, 0x218, 0x93, 0);
                clip(0x242, 0x253, 0x93 + (100 - tp) * 0xba / 100, 0x14d);
                blit(&sldr, 0x242, 0x93, 0);
            }
        }
        video_retrace();                   /* (the window shown, 0x8378c .. 0x83740) */
        if (from_menu)
            fe_read_keys();                /* (and the music, 0x71c20) */
        joy_poll();
        widen(g_joy.x, &c[JC_X_MIN], &c[JC_X_MID], &c[JC_X_MAX]);
        widen(g_joy.y, &c[JC_Y_MIN], &c[JC_Y_MID], &c[JC_Y_MAX]);
        widen(g_joy.throttle, &c[JC_T_MIN], &c[JC_T_MID], &c[JC_T_MAX]);
        if (g_joy.brake > c[JC_B_MAX])
            c[JC_B_MAX] = g_joy.brake;
        else if (g_joy.brake < c[JC_B_MIN])
            c[JC_B_MIN] = g_joy.brake;
        /* the knob: 105 pixels to the right, 108 to the left, 77 up or down, held in
           the middle within 5 across and 3 up and down */
        v = g_joy.x - c[JC_X_MID];
        x = v > 0 ? x0 + v * 0x69 / (c[JC_X_MAX] - c[JC_X_MID])
                  : x0 - v * 0x6c / (c[JC_X_MIN] - c[JC_X_MID]);
        v = g_joy.y - c[JC_Y_MID];
        y = v > 0 ? y0 + v * 0x4d / (c[JC_Y_MAX] - c[JC_Y_MID])
                  : y0 - v * 0x4d / (c[JC_Y_MIN] - c[JC_Y_MID]);
        x = (short)x;
        y = (short)y;
        if (x0 - 5 < x && x < x0 + 5)
            x = x0;
        if (y0 - 3 < y && y < y0 + 3)
            y = y0;
        if (g_joy.has_pedals) {
            bp = 100 - (g_joy.brake - c[JC_B_MIN]) * 100 / (c[JC_B_MAX] - c[JC_B_MIN]);
            if (bp < 5)
                bp = 0;
            tp = 100 - (g_joy.throttle - c[JC_T_MIN]) * 100 / (c[JC_T_MAX] - c[JC_T_MIN]);
            if (tp < 5)
                tp = 0;
        } else if (g_joy.has_throttle) {
            /* a throttle: forward of its middle one bar, back of it the other */
            bp = g_joy.throttle <= c[JC_T_MID] ? 0
                : (g_joy.throttle - c[JC_T_MID]) * 100 / (c[JC_T_MAX] - c[JC_T_MID]);
            tp = g_joy.throttle >= c[JC_T_MID] ? 0
                : (c[JC_T_MID] - g_joy.throttle) * 100 / (c[JC_T_MID] - c[JC_T_MIN]);
        }
        /* a button with the stick back within a quarter of its middle (left Shift not
           held): the middle taken there */
        if ((g_joy.buttons & 1) && !key_down(0x2a)
            && g_joy.x < ((c[JC_X_MAX] - c[JC_X_MID]) >> 2) + c[JC_X_MID]
            && c[JC_X_MID] - ((c[JC_X_MID] - c[JC_X_MIN]) >> 2) < g_joy.x
            && g_joy.y < ((c[JC_Y_MAX] - c[JC_Y_MID]) >> 2) + c[JC_Y_MID]
            && c[JC_Y_MID] - ((c[JC_Y_MID] - c[JC_Y_MIN]) >> 2) < g_joy.y) {
            c[JC_Y_MID] = g_joy.y;
            c[JC_X_MID] = g_joy.x;
            done = 1;
        }
        if (key_get() == 0x1b || g_quit) { /* Esc: no joystick */
            done = 1;
            g_joy.on = 0;
        }
        redraw = 1;
    }
    cal_finish();
    fsh_free(&art);
    pal_fade_down(0, 256);
    cal_save();
}
