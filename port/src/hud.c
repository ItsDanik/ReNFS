/* The race's dashboard and HUD at 320x200 (NOTES §4.11). The game draws them with EA's
   shape library into windows: the screen, or an off-screen picture (the 3D view, the
   map). A picture is copied as it is, skipping colour 255 (see-through), or taken back
   from the window (0x9d8d0 with the records 0xf79a4, 0xf79cc, 0xf7a50), cut to the
   window's clip. */
#include "hud.h"
#include "joystick.h"
#include "cardraw.h"
#include "config.h"
#include "dash.h"
#include "drive.h"
#include "exe.h"
#include "files.h"
#include "font.h"
#include "fsh.h"
#include "polyfill.h"
#include "menu.h"
#include "racepal.h"
#include "replay.h"
#include "session.h"
#include "raster.h"
#include "timing.h"
#include "track.h"
#include "trackfam.h"
#include "video.h"
#include "view.h"
#include "tall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hud_status g_hud;
hud_picture g_hud_screen;
hud_picture g_hud_map_back;
hud_picture g_hud_view;
hud_picture g_hud_mirror;
hud_picture g_hud_map;

fsh_file g_mask;                     /* MASKLO.FSH (0x145604) */
static unsigned char *g_mask_pal;    /* its "!pal", 6 bits (0x145608) */
static int g_redraw;                 /* 0x12b8d4: the whole dashboard is due */
static int g_wheel_shown;            /* 0x12b8d0: the wheel's picture on the screen (0x63
                                        none), turned left < 0 */
static int g_radar_shown;            /* 0x12b938: the radar's level on the screen */
static int g_lever_shown;            /* 0xea99c: the lever's place as last drawn (kept from
                                        race to race, as the original's) */
static int g_bar_at;                 /* 0x12b944: the status bar drawn again after this
                                        step */
static int g_bar_black;              /* 0x12b94c: its strip to be blacked first */
static int g_map_back_taken;         /* 0x12ba0c: the map's background taken */
/* the status bar's numbers as last drawn, each drawn again only when it changes (-99:
   to be drawn, 0x2d150) */
static int g_speed_shown[3];         /* 0x12b8e4: the speed's digits, 10 blank */
static int g_gear_shown;             /* 0x12b92c */
static int g_place_drawn;            /* 0x12b918 */
static int g_time_shown[3];          /* 0x12b8fc, 0x12b8f8, 0x12b8f4: minutes, seconds,
                                        tenths */
static int g_split_drawn;            /* 0x12b8e0: the split (its place shared with the
                                        laps and miles) */
static int g_lap_shown;              /* 0x12b928 */
static int g_mile_shown[2];          /* 0x12b91c, 0x12b920: whole and tenths */
static int g_accel[6];               /* 0x12b968 ..: the acceleration test's figures: the
                                        steps to 60 and to 100 mph and to a quarter mile,
                                        the mph there, the feet from 80 and from 60 mph to
                                        standing (0 none) */
static int g_accel_shown[6];         /* 0x12b950 ..: those on the screen */
static int g_keys_up = 1;            /* 0xea9a0: no dashboard key held since the last */
static void hud_keys(void);
/* the views from outside: the HUD's lists of what of the view reaches the screen, by the
   view's size and the detail (MASKLO.MMB, 0x145a28; the .B<s><n> files leave out the map's
   box too, 0x145a58), and the small rev counter (0x145788 its centre, 0x145790 its
   needle's reach; the needle last drawn 0xeaa98: its end, then its start) */
static unsigned char *g_mmb, *g_mask_b[12];
static const unsigned char *g_lists[2][12];
static int g_tach_c[2], g_tach_r[2];
int g_hud_small_needle[4];
/* the replay's bar (0x5f240): its strip, each button's look (normal, lit, pressed), the
   cameras' and the speeds' labels (normal, lit) */
static const unsigned char *g_bar_strip, *g_bar_button[3][9], *g_bar_camera[2][8];
static const unsigned char *g_bar_speed[2][3];
/* the watching bar's (0x7ed30): its strip, the name's box, each button's look (normal,
   lit, pressed), the camera's labels (normal, lit), the letters and digits */
static const unsigned char *g_watch_strip, *g_watch_box, *g_watch_button[3][13];
static const unsigned char *g_watch_label[2][6], *g_watch_letter[36];

/* the window drawn into (0xf889c): a picture and its clip */
/* a times b, 16.16, rounded by the last bit out */
static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static hud_picture g_win;
static int g_cx0, g_cy0, g_cx1, g_cy1;

enum { COPY, SEE_THROUGH, TAKE };

static void window_set(const hud_picture *p)
{
    g_win = *p;
    g_cx0 = g_cy0 = 0;
    g_cx1 = p->w;
    g_cy1 = p->h;
    raster_window(p->pix, p->w, 0, 0, p->w, p->h);
}

/* the clip (0x812ec), inside the window */
static void clip_set(int x0, int x1, int y0, int y1)
{
    g_cx0 = x0 < 0 ? 0 : x0;
    g_cy0 = y0 < 0 ? 0 : y0;
    g_cx1 = x1 > g_win.w ? g_win.w : x1;
    g_cy1 = y1 > g_win.h ? g_win.h : y1;
    raster_window(g_win.pix, g_win.w, g_cx0, g_cy0, g_cx1, g_cy1);
}

static int s16(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

/* a picture of w x h at (x, y) in the window, cut to the clip (0x9d8d0, 8 bits to 8) */
static void blit_pixels(unsigned char *pix, int w, int h, int x, int y, int how)
{
    int pitch = w, sx = 0, sy = 0, i, r;

    if (g_cy0 - y > 0) {
        sy = g_cy0 - y;
        h -= sy;
        y = g_cy0;
    }
    if (y + h - g_cy1 > 0)
        h -= y + h - g_cy1;
    if (g_cx0 - x > 0) {
        sx = g_cx0 - x;
        w -= sx;
        x = g_cx0;
    }
    if (x + w - g_cx1 > 0)
        w -= x + w - g_cx1;
    if (w <= 0 || h <= 0)
        return;
    for (r = 0; r < h; r++) {
        unsigned char *src = pix + (long)(sy + r) * pitch + sx;
        unsigned char *dst = g_win.pix + (long)(y + r) * g_win.w + x;
        if (how == COPY)
            memmove(dst, src, w);
        else if (how == TAKE)
            memmove(src, dst, w);
        else
            for (i = 0; i < w; i++)
                if (src[i] != 0xff)
                    dst[i] = src[i];
    }
}

static void blit(const unsigned char *head, int x, int y, int how)
{
    blit_pixels((unsigned char *)head + 16, s16(head + 4), s16(head + 6), x, y, how);
}

/* a picture at its own place (0x81230 copied, 0x81580 see-through) */
static void blit_own(const unsigned char *head, int how)
{
    blit(head, s16(head + 0xc), s16(head + 0xe), how);
}

static const unsigned char *named(const fsh_file *f, const char *name)
{
    shape s;

    if (!fsh_find(f, name, &s)) {
        fprintf(stderr, "renfs: the HUD has no picture %.4s\n", name);
        return 0;
    }
    return s.head;
}

/* a picture by the name in one of the program's tables of names */
/* 0x2d314: the screen drawn again whole while racing, above the replay's bar (its 20
   rows at the bottom) while playing back */
static void redraw_clip(void)
{
    clip_set(0, g_win.w, 0, g_hud.running ? g_win.h : H(0xb4, 0x1c0));
}

static const unsigned char *table_named(unsigned table, int i)
{
    return named(&g_mask, (const char *)exe_at((unsigned)exe_i32(table + 4 * i), 4));
}

int hud_load(void)
{
    shape s;
    char path[64];
    int i;

    hud_free();
    /* (0x2e8d4 as the player's car is loaded: the acceleration test's figures none) */
    memset(g_accel, 0, sizeof g_accel);
    memset(g_accel_shown, 0, sizeof g_accel_shown);
    /* (load_cockpit_mask 0x5f7c8: MASKHI at 640x480, MASKHIG / MASKLOG the German) */
    snprintf(path, sizeof path, "SIMDATA/MISC/MASK%s%s.FSH", H("LO", "HI"), g_german ? "G" : "");
    if (!fsh_load(&g_mask, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (!fsh_find(&g_mask, "!pal", &s)) {
        hud_free();
        return 0;
    }
    g_mask_pal = (unsigned char *)s.head + 0x10;
    if (s.type == 0x24)
        for (i = 0; i < 0x300; i++)
            g_mask_pal[i] >>= 2;
    pal_queue_remap(g_mask.d, g_mask_pal);
    cockpit_queue_colours();
    g_hud_screen.w = H(320, 640);
    g_hud_screen.h = H(200, 480);
    g_hud_screen.pix = g_fb;
    /* (race_load_track 0x5ba1c's windows: the map's background 0xf4580, the map 0xf4584,
       the view 0xf4578) */
    g_hud_map_back.w = H(0x39, 0x71);
    g_hud_map_back.h = H(0x34, 0x8c);
    g_hud_map_back.pix = calloc((size_t)(g_hud_map_back.w * g_hud_map_back.h), 1);
    g_hud_view.w = H(320, 640);            /* 0x143e30, 0x143e34 */
    g_hud_view.h = H(200, 480);
    g_hud_view.pix = calloc((size_t)(g_hud_view.w * g_hud_view.h), 1);
    g_hud_mirror.w = g_dash.place[1][0] - g_dash.place[0][0] + 1;
    g_hud_mirror.h = g_dash.place[1][1] - g_dash.place[0][1] + 1;
    g_hud_mirror.pix = calloc((size_t)(g_hud_mirror.w * g_hud_mirror.h), 1);
    g_hud_map.w = H(0x3a, 0x74);
    g_hud_map.h = H(0x36, 0x8e);
    g_hud_map.pix = calloc((size_t)(g_hud_map.w * g_hud_map.h), 1);
    g_redraw = 1;
    g_bar_black = 1;
    g_map_back_taken = 0;

    /* the views from outside (load_cockpit_mask 0x5f7c8 and 0x5f240) */
    {
        const unsigned char *t = named(&g_mask, "tach");
        int hw, hh, k;
        long len;

        if (!t)
            return 0;
        hw = s16(t + 4) / 2;
        hh = s16(t + 6) / 2;
        g_tach_c[0] = hw + s16(t + 0xc);
        g_tach_c[1] = hh + s16(t + 0xe);
        g_tach_r[0] = hw - hw / 4 - H(hw / 16, 0);
        g_tach_r[1] = hh - hh / 4 - H(hh / 16, 0);
        for (k = 0; k < 4; k++)
            g_hud_small_needle[k] = exe_i32(0xeaa98 + 4 * k);
        /* the replay's bar: the names from the program's tables (0xf6bc0 dond .., 0xf6c08
           donl .., 0xf6be4 doni ..; 0xf6c2c card .., 0xf6c4c carl ..; 0xf6c6c fuld ..,
           0xf6c78 full ..); each button's box its pressed picture's, inclusive */
        if (!(g_bar_strip = named(&g_mask, "repl")))
            return 0;
        for (k = 1; k <= 8; k++) {
            const unsigned char *in;

            g_bar_button[0][k] = table_named(0xf6bc0, k);
            g_bar_button[1][k] = table_named(0xf6c08, k);
            g_bar_button[2][k] = in = table_named(0xf6be4, k);
            if (!g_bar_button[0][k] || !g_bar_button[1][k] || !in)
                return 0;
            g_bar.rect[k][0] = s16(in + 0xc);
            g_bar.rect[k][2] = s16(in + 4) + s16(in + 0xc);
            g_bar.rect[k][1] = s16(in + 0xe);
            g_bar.rect[k][3] = s16(in + 6) + s16(in + 0xe);
            g_bar.lit_size[k][0] = s16(g_bar_button[1][k] + 4);
            g_bar.lit_size[k][1] = s16(g_bar_button[1][k] + 6);
        }
        for (k = 0; k < 8; k++)
            if (!(g_bar_camera[0][k] = table_named(0xf6c2c, k))
                || !(g_bar_camera[1][k] = table_named(0xf6c4c, k)))
                return 0;
        for (k = 0; k < 3; k++)
            if (!(g_bar_speed[0][k] = table_named(0xf6c6c, k))
                || !(g_bar_speed[1][k] = table_named(0xf6c78, k)))
                return 0;
        /* the watching bar (0x7ed30): bard, namd; the buttons from the tables 0xf7604
           (normal; Done's donm, greyed, but in a network game), 0xf766c (lit), 0xf7638
           (pressed: their boxes the buttons'); the labels 0xf76a0, 0xf76b8 by the
           camera's mode 1..5; 0-9 a-z from 0xf76d0 */
        if (!(g_watch_strip = named(&g_mask, "bard")) || !(g_watch_box = named(&g_mask, "namd")))
            return 0;
        for (k = 1; k <= WATCH_CAMERA; k++) {
            const unsigned char *in;

            g_watch_button[0][k] = k == WATCH_DONE ? named(&g_mask, "donm") : table_named(0xf7604, k);
            g_watch_button[1][k] = table_named(0xf766c, k);
            g_watch_button[2][k] = in = table_named(0xf7638, k);
            if (!g_watch_button[0][k] || !g_watch_button[1][k] || !in)
                return 0;
            g_watch.rect[k][0] = s16(in + 0xc);
            g_watch.rect[k][2] = s16(in + 4) + s16(in + 0xc);
            g_watch.rect[k][1] = s16(in + 0xe);
            g_watch.rect[k][3] = s16(in + 6) + s16(in + 0xe);
            g_watch.size[k][0] = s16(g_watch_button[0][k] + 4);
            g_watch.size[k][1] = s16(g_watch_button[0][k] + 6);
        }
        for (k = 1; k <= 5; k++)
            if (!(g_watch_label[0][k] = table_named(0xf76a0, k))
                || !(g_watch_label[1][k] = table_named(0xf76b8, k)))
                return 0;
        for (k = 0; k < 36; k++)
            if (!(g_watch_letter[k] = table_named(0xf76d0, k)))
                return 0;
        g_watch.label = 1;
        /* (load_cockpit_mask 0x5f7c8) none pressed or lit, the camera's label 1, full
           speed */
        g_bar.pressed = g_bar.lit = 0;
        g_bar.cam = 1;
        g_bar.speed = 0;
        g_bar.redraw = 0;
        g_bar.speeds = 1;
        snprintf(path, sizeof path, "SIMDATA/MISC/MASK%s.MMB", H("LO", "HI"));
        g_mmb = file_load(path, &len);
        if (ww_count(g_mmb) < 12) {
            fprintf(stderr, "renfs: %s is missing\n", path);
            return 0;
        }
        for (k = 0; k < 12; k++) {
            g_lists[0][k] = ww_entry(g_mmb, k);
            snprintf(path, sizeof path, "SIMDATA/MISC/MASK%s.B%d%d", H("LO", "HI"), k / 4, k % 4);
            if (!(g_mask_b[k] = file_load(path, &len))) {
                fprintf(stderr, "renfs: %s is missing\n", path);
                return 0;
            }
            g_lists[1][k] = g_mask_b[k];
        }
    }
    return g_hud_map_back.pix && g_hud_view.pix && g_hud_map.pix && g_hud_mirror.pix;
}

/* the police's ticket or warning shown (0x12b4d0), below */
static fsh_file g_ticket;
static int g_ticket_on;
static unsigned char g_ticket_saved[768];      /* 0x12ba30: the race's palette */

void hud_free(void)
{
    int i;

    fsh_free(&g_mask);
    /* (0x31bf4) the ticket too: an arrest ends the race with it still up */
    fsh_free(&g_ticket);
    g_ticket_on = 0;
    free(g_hud_map_back.pix);
    free(g_hud_view.pix);
    free(g_hud_map.pix);
    free(g_hud_mirror.pix);
    g_hud_map_back.pix = g_hud_view.pix = g_hud_map.pix = g_hud_mirror.pix = 0;
    free(g_mmb);
    g_mmb = 0;
    for (i = 0; i < 12; i++) {
        free(g_mask_b[i]);
        g_mask_b[i] = 0;
    }
    /* (0x31bf4) the race's colours 0x5e, 0x5f and 0xfa .. 0xfd for the menus again;
       0x47 and the racers' marks stay out */
    g_colour_off[0x5e] = g_colour_off[0x5f] = 0;
    for (i = 0xfa; i < 0xfe; i++)
        g_colour_off[i] = 0;
}

/* the HUD's digits by the program's tables of names: grey (0xf6c84: gry0 .. gry9, gryb,
   kept at 0x1456b8), yellow (0xf6cb0: yel0 .. yel9, yelr, yeln, plus, mnus, yelb, at
   0x1456e8) and red (0xf6cec: red0 .. red9, redb, at 0x145728) */
#define GREY 0xf6c84
#define YELLOW 0xf6cb0
#define RED 0xf6cec
#define YELLOW_R 10
#define YELLOW_N 11
#define YELLOW_PLUS 12
#define YELLOW_MINUS 13
#define YELLOW_BLANK 14

/* n digits of v, the most significant first, step apart (0x2d47c) */
static void digits_in(unsigned table, int v, int x, int y, int step, int n)
{
    int d[8], i;

    for (i = 0; i < n; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = n - 1; i >= 0; i--, x += step)
        blit(table_named(table, d[i]), x, y, COPY);
}

/* a time (1/60 s) as minutes, seconds and tenths in grey digits (0x2d6b4 with 0x2d3ac:
   300 taken off first, never below 0; 0x2d47c each group) */
static void digits(int v, int x, int y, int step, int n)
{
    digits_in(GREY, v, x, y, step, n);
}

static void time_draw(int t, int x, int y, int step, int gap)
{
    int left = t > 300 ? t - 300 : 0, min, sec;

    min = left / 3600;
    left -= min * 3600;
    sec = left / 60;
    left -= sec * 60;
    digits(min, x, y, step, 2);
    x += 2 * step + gap;
    digits(sec, x, y, step, 2);
    digits(left / 6, x + 2 * step + gap, y, step, 1);
}

/* a record's time of the CONFIG.DAT's records (the track's 9 blocks of 37) */
static int record_time(int track, int k)
{
    const unsigned char *r = g_config + 0xb5 + track * 0xa6b + k * 0x27 + 0x1b;

    return (int)(r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
}

/* the status bar (0x2f2a4): its picture, what is timed, the record to beat, laps or
   miles, the kind of race; a black strip for the first 360 steps */
/* every number shown marked for drawing again (0x2d150) */
static void numbers_reset(void)
{
    g_radar_shown = -99;
    g_speed_shown[0] = g_speed_shown[1] = g_speed_shown[2] = -99;
    g_gear_shown = g_place_drawn = g_split_drawn = g_lap_shown = -99;
    g_time_shown[0] = g_time_shown[1] = g_time_shown[2] = -99;
    g_mile_shown[0] = g_mile_shown[1] = -99;
    memset(g_accel_shown, 0, sizeof g_accel_shown);
    g_bar_at = 0x7d000;
}

/* n yellow digits of v, the leading noughts blank but the last (0x2d50c) */
static void yellow_digits(int v, int x, int y, int step, int n)
{
    int d[8], i, leading = 1;

    for (i = 0; i < n; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = n - 1; i >= 0; i--, x += step)
        if (d[i] == 0 && i > 0 && leading)
            blit(table_named(YELLOW, YELLOW_BLANK), x, y, COPY);
        else {
            blit(table_named(YELLOW, d[i]), x, y, COPY);
            leading = 0;
        }
}

/* steps as seconds and tenths (0x2d7f0: more than 1,200 shown as none); at 640x480 the
   places its registers give, else those pushed */
static void accel_time(int v, int x)
{
    int sec, y = H(1, 6), step = H(8, 0x13);

    if (v > 0x4b0)
        v = 0;
    sec = v / 60;
    yellow_digits(sec, x, y, step, 2);
    yellow_digits((v - sec * 60) / 6, x + 2 * step + H(2, 8), y, step, 1);
}

/* the acceleration test's figures on the status bar, each when it changes (0x2d8c0) */
static void accel_figures(void)
{
    static const int time_x[2][3] = { { 0x14, 0x45, 0x76 }, { 0x25, 0x8d, 0xf5 } };
    int i, y = H(1, 6), step = H(8, 0x13);

    for (i = 0; i < 3; i++)
        if (g_accel_shown[i] != g_accel[i]) {
            accel_time(g_accel[i], time_x[g_view_res][i]);
            g_accel_shown[i] = g_accel[i];
        }
    if (g_accel_shown[3] != g_accel[3]) {
        yellow_digits(g_accel[3], H(0xaa, 0x14f), y, step, 3);
        g_accel_shown[3] = g_accel[3];
    }
    if (g_accel_shown[5] != g_accel[5]) {
        yellow_digits(g_accel[5], H(0x11f, 0x234), y, step, 3);
        g_accel_shown[5] = g_accel[5];
    }
    if (g_accel_shown[4] != g_accel[4]) {
        yellow_digits(g_accel[4], H(0xee, 0x1cc), y, step, 3);
        g_accel_shown[4] = g_accel[4];
    }
}

void hud_accel_times(int t60, int t100, int quarter, int speed)
{
    if (t60 > 0x4b0)
        t60 = 0;
    if (t100 > 0x4b0)
        t100 = 0;
    if (quarter > 0x4b0)
        quarter = 0;
    if (t60 > 0)
        g_accel[0] = t60;
    if (t100 > 0)
        g_accel[1] = t100;
    if (quarter > 0)
        g_accel[2] = quarter;
    if (speed > 0)
        g_accel[3] = speed * 224 / 0x640000;
}

void hud_braking(int from60, int from80)
{
    if (from60 > 0x1f4)
        from60 = 0;
    if (from80 > 0x1f4)
        from80 = 0;
    if (from60 > 0)
        g_accel[5] = from60 * 0xa76 / 12000 * 33 / 10;
    if (from80 > 0)
        g_accel[4] = from80 * 0xdf3 / 12000 * 33 / 10;
}

static void status_bar(void)
{
    if (!g_hud.shown || g_hud.steps < 360) {
        raster_fill(0, 0, H(320, 640), H(16, 32), g_marker_line);
        return;
    }
    blit_own(named(&g_mask, "stat"), COPY);
    if (g_hud.mode == 1) {
        blit_own(named(&g_mask, "rtet"), COPY);
        time_draw(record_time(g_hud.track, 6 + (g_hud.circuit ? 10 * g_hud.table : 0)) + 300,
                  H(0x11, 0x1f), H(3, 8), H(6, 0xd), H(2, 7));
        blit_own(named(&g_mask, "splt"), COPY);
        blit_own(table_named(YELLOW, g_hud.ahead >= 0 ? YELLOW_PLUS : YELLOW_MINUS), COPY);
    } else if (g_hud.mode == 0) {
        blit_own(named(&g_mask, "segt"), COPY);
        if (g_hud.splits > 0) {
            blit_own(named(&g_mask, "splt"), COPY);
            blit_own(table_named(YELLOW, g_hud.ahead >= 0 ? YELLOW_PLUS : YELLOW_MINUS), COPY);
        } else
            blit_own(named(&g_mask, g_hud.circuit ? "lap#" : "mile"), COPY);
        time_draw((g_hud.circuit ? g_hud.best_lap : record_time(g_hud.track, g_hud.segment)) + 300,
                  H(0x11, 0x1f), H(3, 8), H(6, 0xd), H(2, 7));
    }
    else {
        blit_own(named(&g_mask, "acct"), COPY);
        accel_figures();
    }
    if (g_hud.mode < 2)
        blit_own(named(&g_mask, g_hud.race_type <= 1 ? "head" : "sing"), COPY);
    numbers_reset();
}

void hud_redraw(void)
{
    static const int corners[2][8] = { { 0, 0, 0x38, 0, 0x38, 0x3b, 0, 0x3b },
                                       { 0, 0, 0x70, 0, 0x70, 0x8b, 0, 0x8b } };
    shape dash;

    window_set(&g_hud_screen);
    redraw_clip();
    if (fsh_find(&g_dash.file, "dash", &dash))
        blit(dash.head, 0, 0, SEE_THROUGH);
    /* the map's box: the dashboard under it, taken and darkened twice (0x84248) */
    blit_pixels(g_hud_map_back.pix, g_hud_map_back.w, g_hud_map_back.h, H(0x104, 0x208),
                H(0x7f, 0x132), TAKE);
    window_set(&g_hud_map_back);
    poly_fill(4, corners[g_view_res], poly_shade_rows, 0);
    poly_fill(4, corners[g_view_res], poly_shade_rows, 0);
    g_map_back_taken = 1;
    window_set(&g_hud_screen);
    status_bar();
    g_wheel_shown = 0x63;
    g_redraw = 0;
    numbers_reset();
}

void hud_redraw_due(void)
{
    g_redraw = 1;
}

void hud_paused(void)
{
    window_set(&g_hud_screen);
    clip_set(0, g_hud_screen.w, 0, g_hud_screen.h);
    blit_own(named(&g_mask, "paus"), COPY);
}

/* ---- the police's ticket or warning (0x30424, 0x306e8) -------------------------------- */

int hud_ticket(int ticket, int warning, int over)
{
    shape s;

    if (!ticket && !warning) {
        if (!g_ticket_on)
            return 0;
        /* 0x306e8: the picture let go, the race's colours back (unless the race is over)
           and the dashboard drawn again */
        fsh_free(&g_ticket);
        g_ticket_on = 0;
        if (!over)
            video_palette(g_ticket_saved, 0, 0xfe);
        hud_redraw_due();
        if (!g_hud.running)
            g_bar.redraw = 1;
        else
            g_watch.whole = 1;
        return 2;
    }
    if (!g_ticket_on) {
        char path[48];

        /* 0x30424: SIMDATA/MISC's TICKET or WARN (TICKEG, GWARN in kilometres), D
           racing or R the replay, L 320x200 or H 640x480 */
        snprintf(path, sizeof path, "SIMDATA/MISC/%s%c%c.QFS",
                 ticket ? (g_hud.kmh ? "TICKEG" : "TICKET") : (g_hud.kmh ? "GWARN" : "WARN"),
                 g_hud.running ? 'D' : 'R', H('L', 'H'));
        if (!fsh_load(&g_ticket, path)) {
            fprintf(stderr, "renfs: %s is missing\n", path);
            return 0;
        }
        g_ticket_on = 1;
        video_get_palette(g_ticket_saved, 0, 256);
        /* racing, its colours over the race's but the last two (setpalette 0, 0xfe); in
           a replay the race's colours kept and its pictures re-coloured to them (0x55ee0) */
        if (fsh_find(&g_ticket, "!pal", &s)) {
            if (g_hud.running) {
                unsigned char p8[768];
                int k;

                for (k = 0; k < 3 * 0xfe; k++)
                    p8[k] = (unsigned char)((s.head[0x10 + k] & 0x3f) << 2);
                video_palette(p8, 0, 0xfe);
            } else
                pal_remap(g_ticket.d, s.head + 0x10);
        }
    }
    window_set(&g_hud_screen);
    redraw_clip();
    if (fsh_find(&g_ticket, "scrn", &s))
        blit_own(s.head, COPY);
    return 1;
}

static int u16(const unsigned char *p)
{
    return p[0] | p[1] << 8;
}

/* a picture through a BAMF list into the window (0x8b8f0): each span from its place in
   the picture to its place in the window (by 64 KB bank), going on at the start of the
   next row of both where the window's row ends. The list's +0xc is the rate the picture
   is read at across (0x10000 1:1, 0x8000 each pixel twice): the window's i-th pixel of a
   piece from the picture's (i · rate) >> 16-th (the step table 0x15ed1c, 0xa6240; the
   rows doubled are the list's own spans) */
static void bamf_copy(const hud_picture *from, const unsigned char *list)
{
    const unsigned char *p = list + 0x14;
    int banks, b, n, i, sr, sc, dr, dc, len, k, j;
    unsigned so, d, rate;

    if (!list || memcmp(list, "BAMF", 4) != 0)
        return;
    rate = (unsigned)u16(list + 0xc) | (unsigned)u16(list + 0xe) << 16;
    banks = u16(list + 8);
    for (b = 0; b < banks; b++) {
        n = u16(p);
        p += 2;
        for (i = 0; i < n; i++, p += 8) {
            so = (unsigned)u16(p) | (unsigned)u16(p + 2) << 16;
            d = (unsigned)b << 16 | (unsigned)u16(p + 4);
            len = u16(p + 6);
            sr = (int)(so / (unsigned)from->w);
            sc = (int)(so % (unsigned)from->w);
            dr = (int)(d / (unsigned)g_win.w);
            dc = (int)(d % (unsigned)g_win.w);
            while (len > 0 && sr < from->h && dr < g_win.h) {
                const unsigned char *src = from->pix + (long)sr * from->w + sc;
                unsigned char *dst = g_win.pix + (long)dr * g_win.w + dc;

                k = dc + len > g_win.w ? g_win.w - dc : len;
                if (rate == 0x10000)
                    memmove(dst, src, k);
                else
                    for (j = 0; j < k; j++)
                        dst[j] = src[(unsigned)j * rate >> 16];
                /* (the port's 320x240: where the view went on the screen) */
                if (g_view_tall && from == &g_hud_view && g_win.pix == g_fb)
                    tall_copied((long)dr * g_win.w + dc, (long)sr * from->w + sc, k, rate);
                len -= k;
                sr++, dr++;
                sc = dc = 0;
            }
        }
    }
}

/* how far the wheel is turned, 0 .. 6 (5 limits at 320x200, 3 never; at 640x480 0 .. 5) */
static int wheel_turn(int steer)
{
    int a = steer < 0 ? -steer : steer;

    if (g_view_res)
        return a < 0x21999 ? 0 : a < 0x70000 ? 1 : a < 0xc8000 ? 2 : a < 0x133333 ? 3
            : a < 0x1b0000 ? 4 : 5;
    if (a < 0x74ccc)
        return a < 0x26666 ? 0 : a < 0x66666 ? 1 : 2;
    return a < 0x154ccc ? 4 : a < 0x1c6666 ? 5 : 6;
}

/* a gauge's needle: from its centre (the last point) to t/n of the way from one point
   to the next, n 1024 or 10 (20 for km/h), the needle EA's smoothed line in the car's
   colour (0x8b195) as wide as the car's (0xea71c, 0xea720; less one at 320x200) */
static void needle(int (*pt)[2], int count, int i, int t, int n, int number)
{
    int cx = pt[count - 1][0], cy = pt[count - 1][1], ex, ey;

    if (n == 1024) {
        ex = pt[i][0] + ((pt[i + 1][0] - pt[i][0]) * t >> 10);
        ey = pt[i][1] + ((pt[i + 1][1] - pt[i][1]) * t >> 10);
    } else {
        ex = pt[i][0] + (pt[i + 1][0] - pt[i][0]) * t / n;
        ey = pt[i][1] + (pt[i + 1][1] - pt[i][1]) * t / n;
    }
    poly_wide_line(cx << 16, cy << 16, ex << 16, ey << 16, g_needle_colour,
                   (const unsigned char (*)[256])g_needle_blend,
                   exe_i32(0xea71c + 20 * number) - H(0x10000, 0),
                   exe_i32(0xea720 + 20 * number) - H(0x10000, 0), g_hud_view.w, g_hud_view.h);
}

/* the speed in three digits, the leading noughts blank (cars 3 and 11) */
static void speed_digits(int v, int number)
{
    int d[3], i, x = number == 3 ? H(0x99, 0x131) : H(0x58, 0xb0);
    int y = number == 3 ? H(0x98, 0x16d) : H(0x62, 0xeb);
    int step = number == 3 ? H(4, 9) : H(7, 0xe), leading = 1;

    for (i = 0; i < 3; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = 2; i >= 0; i--, x += step) {
        const unsigned char *pic;

        if (d[i] == 0 && i > 0 && leading)
            pic = g_dash.digit[10];
        else {
            pic = g_dash.digit[d[i]];
            leading = 0;
        }
        blit(pic, x, y, number == 0xb ? SEE_THROUGH : COPY);
    }
}

/* the gauges' needles (0x2fc40): the engine's turns, the speed */
static void needles(const hud_car *c)
{
    int rpm = c->rpm, v;
    long long p;

    if (rpm > c->redline + 0x3d4)
        rpm = c->redline + 0x3d4;
    needle(g_dash.tach_pt, g_dash.tach_n, rpm >> 10, rpm & 0x3ff, 1024, c->number);

    p = (long long)(c->speed < 0 ? -c->speed : c->speed) * (g_hud.kmh ? 0x39999 : 0x23d70);
    v = ((int)(p >> 16) + (int)((p >> 15) & 1)) >> 16;
    if (g_dash.digital)
        speed_digits(v, c->number);
    else if (v > 0)
        needle(g_dash.sped_pt, g_dash.sped_n, v / (g_hud.kmh ? 20 : 10), v % (g_hud.kmh ? 20 : 10),
               g_hud.kmh ? 20 : 10, c->number);
    else
        needle(g_dash.sped_pt, g_dash.sped_n, 0, 0, 10, c->number);
}

/* the radar detector's lights (0x30214): the big one at place 2 while it hears
   anything, lit or dark as it flashes; the five small ones at places 3..7, as many lit as
   the level, drawn when it changes */
static void radar(void)
{
    int n = g_hud.radar > 5 ? 5 : g_hud.radar, i;
    const unsigned char *big = named(&g_dash.file, g_hud.radar_flash ? "bglt" : "bgdk");
    const unsigned char *lit = named(&g_dash.file, "smlt"), *dark = named(&g_dash.file, "smdk");

    if (g_hud.radar)
        blit(big, g_dash.place[2][0], g_dash.place[2][1], COPY);
    if (g_hud.radar != g_radar_shown) {
        if (!g_hud.radar)
            blit(named(&g_dash.file, "bgdk"), g_dash.place[2][0], g_dash.place[2][1], COPY);
        for (i = 0; i < 5; i++)
            if (g_hud.radar && i < n)
                blit(lit, g_dash.place[3 + i][0], g_dash.place[3 + i][1], COPY);
            else if (!g_hud.radar || n <= 4)
                blit(dark, g_dash.place[3 + i][0], g_dash.place[3 + i][1], COPY);
    }
    g_radar_shown = g_hud.radar;
}

/* the start's lights across the status bar (0x2dbbc), a second (60 steps) each: three
   red then two yellow lit in turn, all five green from step 300 */
static void start_lights(void)
{
    int k = g_hud.steps / 60, i;

    if (g_bar_black) {
        raster_fill(0, 0, H(320, 640), H(16, 32), g_marker_line);
        g_bar_black = 0;
    }
    for (i = 0; i < 5; i++) {
        const char *name = g_hud.steps >= 300 ? "sgrn"
                         : i < 3 ? (i > k ? "srdd" : "srdl") : (i > k ? "syld" : "syll");

        /* (0x5257c, each light's sound as it comes on, is an empty routine) */
        blit(named(&g_mask, name), H(0x41, 0x82) * i, H(0, 1), COPY);   /* (0x2dc13) */
    }
    g_bar_at = g_hud.steps + 10;
}

/* the speed in red digits, the leading noughts blank (0x2ddc0): a player's own speed, a
   computer's car's along the road; miles or kilometres an hour */
static void speed_number(const hud_car *c, int x, int y, int step)
{
    int v = c->human ? c->speed : c->road_speed, d[3], i, leading = 1;

    v = mulr(v > 0 ? v : -v, g_hud.kmh ? 0x39999 : 0x23d70) >> 16;
    for (i = 0; i < 3; i++) {
        d[i] = v >= 10 ? v % 10 : v;
        v = v >= 10 ? v / 10 : 0;
    }
    for (i = 0; i < 3; i++, x += step) {
        int k = d[2 - i];

        if (k == 0 && i < 2 && leading) {
            if (g_speed_shown[i] != 10) {
                blit(table_named(RED, 10), x, y, COPY);
                g_speed_shown[i] = 10;
            }
        } else {
            leading = 0;
            if (g_speed_shown[i] != k) {
                blit(table_named(RED, k), x, y, COPY);
                g_speed_shown[i] = k;
            }
        }
    }
}

/* the gear (0x2dee8): R, N or its number. (A computer's car's, worked out from its speed
   against its gears' speeds +0x308, comes with them) */
static void gear_number(const hud_car *c, int x, int y)
{
    int g = c->gear;

    if (g == g_gear_shown)
        return;
    blit(table_named(YELLOW, g >= 0 ? g + 1 : g == -1 ? YELLOW_N : YELLOW_R), x, y, COPY);
    g_gear_shown = g;
}

/* the place (0x2e368), worked out by the race's timing */
static void place_number(const hud_car *c, int x, int y, int step)
{
    int place = timing_place(c->viewed);

    if (place != g_place_drawn) {
        g_timing.place_shown = place;
        digits_in(YELLOW, place, x, y, step, 1);
        g_place_drawn = place;
    }
    g_timing.place_shown = place;
}

/* minutes, seconds and tenths of t less 300 (0x2d3ac) */
static void split_time(int t, int *min, int *sec, int *tenths)
{
    int left = t > 300 ? t - 300 : 0;

    *min = left / 3600;
    left -= *min * 3600;
    *sec = left / 60;
    *tenths = (left - *sec * 60) / 6;
}

/* the time in yellow (0x2d5dc), from step 390; each part drawn when it changes; a new best
   lap brings the whole bar back (its record time) */
static void time_number(int t, int x, int y, int step, int gap)
{
    int v[3], i;

    if (g_hud.steps < 0x186)
        return;
    split_time(t, &v[0], &v[1], &v[2]);
    for (i = 0; i < 3; i++) {
        if (v[i] != g_time_shown[i]) {
            digits_in(YELLOW, v[i], x, y, step, i < 2 ? 2 : 1);
            g_time_shown[i] = v[i];
        }
        x += 2 * step + gap;
    }
    if (g_drive.new_best) {
        g_hud.shown = 1;
        status_bar();
        g_drive.new_best = 0;
    }
}

/* the split against the rival (0x2e1dc): "splt", plus behind or minus ahead, minutes,
   seconds and tenths of the gap; the laps' and miles' numbers in the same place drawn
   again after it */
static void split_number(int x, int y, int step, int gap)
{
    int v, min, sec, tenths;

    if (g_split_drawn == -99) {
        blit_own(named(&g_mask, "splt"), COPY);
        blit_own(table_named(YELLOW, g_hud.ahead < 0 ? YELLOW_PLUS : YELLOW_MINUS), COPY);
        g_split_drawn = -99;
    }
    if (g_hud.ahead == g_split_drawn)
        return;
    if ((g_hud.ahead >= 0) != (g_split_drawn >= 0))
        blit_own(table_named(YELLOW, g_hud.ahead < 0 ? YELLOW_PLUS : YELLOW_MINUS), COPY);
    g_split_drawn = g_hud.ahead;
    v = g_hud.ahead > 0 ? g_hud.ahead : -g_hud.ahead;
    split_time(v + 300, &min, &sec, &tenths);
    digits_in(YELLOW, min, x, y, step, 1);
    digits_in(YELLOW, sec, x + step + gap, y, step, 2);
    digits_in(YELLOW, tenths, x + 3 * step + 2 * gap, y, step, 1);
    g_lap_shown = g_mile_shown[0] = g_mile_shown[1] = -99;
}

/* the lap of the laps on a circuit (0x2e100): "lap#", the lap (at most the settings'
   laps) and the race's laps at (x2, y2) */
static void laps_number(const hud_car *c, int x, int y, int step, int x2, int y2)
{
    int lap = c->lap < g_hud.lap_limit ? c->lap : g_hud.lap_limit;

    if (g_lap_shown == -99)
        blit_own(named(&g_mask, "lap#"), COPY);
    if (lap != g_lap_shown) {
        g_lap_shown = lap;
        if (lap >= 10)
            digits_in(YELLOW, lap, x, y, step, 2);
        else {
            digits_in(YELLOW, lap, x + step, y, step, 1);
            blit(table_named(YELLOW, YELLOW_BLANK), x, y, COPY);
        }
        digits_in(YELLOW, g_hud.laps, x2, y2, step, g_hud.laps >= 10 ? 2 : 1);
    }
    g_split_drawn = -99;
}

/* the distance to an open road's finish (0x2df78): "mile", whole and tenths, 48 units a
   node over 1600 a mile (1000 a kilometre, the whole then in two places) */
static void miles_number(const hud_car *c, int x, int y, int step, int gap)
{
    int left = g_hud.finish_node - c->node, v, whole, tenths, at = x;

    if (g_mile_shown[0] == -99)
        blit_own(named(&g_mask, "mile"), COPY);
    v = left * 48 / (g_hud.kmh ? 1000 : 1600);
    v = g_hud.finish_node > c->node ? v + 1 : 0;
    whole = v / 10;
    tenths = v - whole * 10;
    if (whole != g_mile_shown[0]) {
        g_mile_shown[0] = whole;
        if (g_hud.kmh) {
            if (whole > 9) {
                blit(table_named(YELLOW, 1), x, y, COPY);
                blit(table_named(YELLOW, 0), x + step, y, COPY);
            } else {
                blit(table_named(YELLOW, YELLOW_BLANK), x, y, COPY);
                blit(table_named(YELLOW, whole), x + step, y, COPY);
            }
            at = x + step;
        } else
            blit(table_named(YELLOW, whole), x, y, COPY);
    } else if (g_hud.kmh)
        at = x + step;
    at += step + gap;
    if (tenths != g_mile_shown[1]) {
        blit(table_named(YELLOW, tenths), at, y, COPY);
        g_mile_shown[1] = tenths;
    }
    g_split_drawn = -99;
}

/* the time the bar shows (0x30d8c): the race's steps; an open road's finishing time once
   past it; on a circuit from the second lap the time the lap began for four seconds (in
   the segment's mode from the third lap the last lap's time), then the lap's so far (the
   race's so far in the race's mode); in the race's mode the segments before added */
static int shown_time(const hud_car *c)
{
    int t = g_hud.steps, p = c->player, i;

    if (p < 0 || p >= DRIVE_PLAYERS)
        p = 0;
    if (!g_hud.circuit) {
        if (g_hud.finish_node < c->node)
            t = g_timing.finish[p] + 300;
    } else if (c->lap > 1 && c->lap <= DRIVE_LAPS) {
        int began = g_drive.lap_times[p][c->lap - 1] + 300;

        if (g_hud.steps - began < 0xf0) {
            t = began;
            if (g_hud.mode == 0 && c->lap > 2)
                t = began - g_drive.lap_times[p][c->lap - 2];
        } else if (g_hud.mode == 0)
            t = g_hud.steps - g_drive.lap_times[p][c->lap - 1];
    }
    if (g_hud.mode == 1)
        for (i = 0; i < g_hud.segment && i < 3; i++)
            t += g_timing.segment_times[i];
    return t;
}

/* the status bar's numbers (0x30d8c): before the start its lights; then the speed, the
   gear, the place, the time, and the split or the laps or the miles (the segment's
   mode) or the split (the race's) */
static void bar_numbers(const hud_car *c)
{
    int t = shown_time(c);

    if (!g_hud.shown || g_hud.steps <= 360) {
        if (g_hud.steps < 360)
            start_lights();
        return;
    }
    /* (the same routines in other places at 640x480) */
    int y = H(1, 5), step = H(8, 0x13), gap = H(3, 8);

    if (g_hud.mode < 2) {
        speed_number(c, H(0x7a, 0x106), H(1, 2), H(0xe, 0x1b));
        gear_number(c, H(0xba, 0x17a), y);
        place_number(c, H(0x121, 0x244), y, H(7, 0x13));
    }
    if (g_hud.mode == 1) {
        time_number(t, H(0x48, 0x90), y, step, gap);
        split_number(H(0xe0, 0x1c0), y, step, gap);
    } else if (g_hud.mode == 0) {
        time_number(t, H(0x48, 0x90), y, step, gap);
        if (g_hud.splits > 0)
            split_number(H(0xe0, 0x1c0), y, step, gap);
        else if (g_hud.circuit)
            laps_number(c, H(0xde, 0x1c0), y, step, H(0xf5, 0x1f2), y);
        else
            miles_number(c, H(0xd9, 0x1b8), y, step, H(2, 7));
    }
    if (g_hud.mode == 2)
        accel_figures();
    /* (a network race's "lost" 0x12b93c: not here) */
    g_bar_black = 1;
}

/* 0x5fa54: the bar's strip, the button pressed (its pressed look) or lit (its lit look,
   the mouse on it but for Done), the camera's label and the speed's (lit while the mouse
   is on their buttons, pressed a pixel down and right), Done's normal look unless it is
   lit. On the English bar at 320x200 Done's pictures 10 to the right */
static void bar_buttons(void)
{
    int mx, my, k = g_bar.lit, over;
    const unsigned char *p;

    mouse_read(&mx, &my);
    mx = mx * H(320, 640) / 640;
    my = my * H(200, 480) / 480;
    blit_own(g_bar_strip, COPY);
    if (g_bar.pressed) {
        p = g_bar_button[2][g_bar.pressed];
        if (g_bar.pressed == BAR_DONE && !g_hud.kmh && !g_view_res)
            blit(p, s16(p + 0xc) + 10, s16(p + 0xe) + 1, COPY);
        else
            blit_own(p, COPY);
    } else if (k == BAR_DONE) {
        p = g_bar_button[1][k];
        if (!g_hud.kmh && !g_view_res)
            blit(p, s16(p + 0xc) + 10, s16(p + 0xe), COPY);
        else
            blit_own(p, COPY);
    } else if (k) {
        const int *r = g_bar.rect[k];

        if (r[0] <= mx && mx <= r[2] && r[1] <= my && my <= r[3])
            blit_own(g_bar_button[1][k], COPY);
    }
    over = g_bar.lit == BAR_CAMERA && g_bar.rect[BAR_CAMERA][0] <= mx
           && mx <= g_bar.rect[BAR_CAMERA][2] && g_bar.rect[BAR_CAMERA][1] <= my
           && my <= g_bar.rect[BAR_CAMERA][3];
    p = g_bar_camera[g_bar.pressed == BAR_CAMERA || over][g_bar.cam];
    if (g_bar.pressed == BAR_CAMERA)
        blit(p, s16(p + 0xc) + 1, s16(p + 0xe) + 1, COPY);
    else
        blit_own(p, COPY);
    over = g_bar.lit == BAR_PLAY && g_bar.rect[BAR_PLAY][0] <= mx
           && mx <= g_bar.rect[BAR_PLAY][2] && g_bar.rect[BAR_PLAY][1] <= my
           && my <= g_bar.rect[BAR_PLAY][3];
    p = g_bar_speed[g_bar.pressed == BAR_PLAY || over][g_bar.speed];
    if (g_bar.pressed == BAR_PLAY)
        blit(p, s16(p + 0xc) + 1, s16(p + 0xe) + 1, COPY);
    else
        blit_own(p, COPY);
    if (g_bar.lit != BAR_DONE && g_view_res)
        blit_own(g_bar_button[0][BAR_DONE], COPY);
    else if (g_bar.lit != BAR_DONE && !g_hud.kmh) {
        p = g_bar_button[0][BAR_DONE];
        blit(p, s16(p + 0xc) + 10, s16(p + 0xe) + 1, COPY);
    }
}

/* 0x7f310: the driver's name of the car watched (from the session: up to 8 letters and
   digits, anything else a space), centred in its box (drawn again unless the whole bar
   is) */
static void watch_name(void)
{
    const char *name = g_session.racer[g_drive.viewed].name;
    const char *end = memchr(name, 0, sizeof g_session.racer[0].name);
    int pick[8], n = end ? (int)(end - name) : 9, w = 0, x, i;

    if (!g_watch.whole)
        blit_own(g_watch_box, COPY);
    if (n < 1 || n > 8)
        return;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)name[i];

        if (c >= '0' && c <= '9')
            pick[i] = c - '0';
        else {
            pick[i] = c - 'a';
            if (pick[i] < 0)
                pick[i] += 0x20;
            pick[i] += 10;
        }
        if (pick[i] >= 0 && pick[i] < 36)
            w += s16(g_watch_letter[pick[i]] + 4);
        else {
            pick[i] = -1;
            w += H(5, 10);
        }
    }
    x = s16(g_watch_box + 0xc) + (s16(g_watch_box + 4) >> 1) - (w >> 1);
    for (i = 0; i < n; i++)
        if (pick[i] < 0)
            x += H(5, 10);
        else {
            const unsigned char *p = g_watch_letter[pick[i]];

            blit(p, x, s16(p + 0xe), COPY);
            x += s16(p + 4);
        }
}

/* 0x7f1d8: the bar's strip, its own and Done's greyed (all of it asked for); the button
   pressed, or the one lit when that changed; under the cars' numbers their markers'
   colours (0x7f998: the pressed one a pixel right, at 640x480 down too); the camera's
   label (lit when its button is, pressed a pixel down and right); the name when another
   car is watched */
static void watch_buttons(void)
{
    const unsigned char *p;
    int i, y;

    if (g_watch.whole) {
        g_watch.redraw = 1;
        blit_own(g_bar_strip, COPY);
        blit_own(g_watch_strip, COPY);
        blit_own(g_watch_button[0][WATCH_DONE], COPY);
    }
    if (g_watch.pressed) {
        blit_own(g_watch_button[2][g_watch.pressed], COPY);
        g_watch.lit = g_watch.pressed;
    } else if (g_watch.lit != g_watch.lit_drawn || g_watch.redraw) {
        if (g_watch.lit_drawn)
            blit_own(g_watch_button[0][g_watch.lit_drawn], COPY);
        g_watch.lit_drawn = g_watch.lit;
        if (g_watch.lit)
            blit_own(g_watch_button[1][g_watch.lit], COPY);
    }
    y = s16(g_watch_button[1][WATCH_CAR1] + 0xe) + 3;
    for (i = 0; i < 8; i++) {
        int x = s16(g_watch_button[1][WATCH_CAR1 + i] + 0xc) + H(3, 4), yy = y;

        if (WATCH_CAR1 + i == g_watch.pressed) {
            x++;
            yy += H(0, 1);
        }
        raster_fill(x, yy, H(2, 5), H(1, 6), g_marker_colour[i]);
    }
    if (g_watch.pressed == WATCH_CAMERA) {
        p = g_watch_label[1][g_watch.label];
        blit(p, s16(p + 0xc) + 1, s16(p + 0xe) + 1, COPY);
    } else
        blit_own(g_watch_label[g_watch.lit == WATCH_CAMERA][g_watch.label], COPY);
    if (g_watch.name_for != g_drive.viewed || g_watch.whole) {
        g_watch.name_for = g_drive.viewed;
        watch_name();
    }
}

/* 0x5c636: racing, another car watched or the player stopped: the camera's label from
   the camera's mode 1..5 (the crash's view 13 as 3, else as it was), then 0x7ef14: the
   bar drawn again when asked, the pointer, pressed while a button is */
static void watch_draw(void)
{
    int m = g_hud.cam_mode;

    if (m >= 1 && m <= 5 && m != g_watch.label) {
        g_watch.label = m;
        g_watch.redraw = 1;
    } else if (m == 13) {
        g_watch.label = 3;
        g_watch.redraw = 1;
    } else if (!g_watch.label) {
        g_watch.label = 1;
        g_watch.redraw = 1;
    }
    window_set(&g_hud_screen);
    if (g_watch.redraw || g_watch.whole) {
        clip_set(0, H(0x140, 0x280), H(0xb4, 0x1c0), H(0xc8, 0x1e0));
        watch_buttons();
        g_watch.redraw = g_watch.whole = 0;
    }
    fe_mouse_look(g_watch.pressed > 0);
}

/* 0x5c5d0 when not racing, 0x5fcf0: the camera's label from the camera's mode (the
   crash's view 13 as 3); the whole bar when asked, else the time and how far through
   the recording (a dark green line 122 long over black) when the counter moved; the
   mouse's pointer, pressed while a button is */
void hud_bar_draw(void)
{
    int w;

    if (g_drive.state == 1) {
        if (g_drive.viewed != g_player || g_hud.stopped)
            watch_draw();
        else
            mouse_show(0);
        return;
    }
    if (g_hud.cam_mode >= 0 && g_hud.cam_mode < 8 && g_bar.cam != g_hud.cam_mode) {
        g_bar.cam = g_hud.cam_mode;
        g_bar.redraw = 1;
    } else if (g_hud.cam_mode == 13) {
        g_bar.cam = 3;
        g_bar.redraw = 1;
    }
    window_set(&g_hud_screen);
    if (!g_bar.redraw && g_bar.count_shown == g_drive.steps)
        goto pointer;
    clip_set(0, H(0x140, 0x280), H(0xb4, 0x1c0), H(0xc8, 0x1e0));
    if (g_bar.redraw)
        bar_buttons();
    g_bar.count_shown = g_drive.steps;
    g_bar.redraw = 0;
    time_draw(g_drive.steps, H(0x2d, 0x41), H(0xba, 0x1c5), H(6, 0xd), H(2, 7));
    w = g_drive.steps * H(0x7a, 0xf1) / (g_replay.last + 1);
    raster_fill(H(0x2d, 0x41), H(0xc3, 0x1d8), w, H(3, 6), g_hud_colour[3]);
    raster_fill(H(0x2d, 0x41) + w, H(0xc3, 0x1d8), H(0x7a, 0xf1) - w, H(3, 6),
                g_hud_colour[HUD_BLACK]);
pointer:
    fe_mouse_look(g_bar.pressed > 0);
}

/* detail 3, every other line (0x31832, 0x30bff): the first time, the view's picture
   blacked and put on the screen whole through the list of the pixels doubled, so that
   the lines left out are black */
static void lines_clear(const unsigned char *list)
{
    if (!g_hud.clear)
        return;
    memset(g_hud_view.pix, g_marker_line, (size_t)(g_hud_view.w * g_hud_view.h));
    bamf_copy(&g_hud_view, list);
    g_hud.clear = 0;
}

/* 0x2e4f0: with the map off, in its box, the gear lever for a while after each shift
   (+0x421 counting down from 16): at the gear before (16 .. 14), at its place across
   from neutral (13), the new gear's (12, until the clutch bites), the new gear (11 ..
   1); then the gate without it (0). Drawn again only when its place changes: the gate,
   then the knob by its middle, or with the stick's four parts the stick from neutral
   towards it, a quarter of the way further each part (lth1, lth2, lth3, nob1) */
static void gear_lever(const hud_car *c)
{
    int s = c->lever_shown, wait = c->lever_wait, at = 0, x = 0, y = 0, i;
    int before = c->lever_before + 2, now = c->lever_gear + 2;
    const int (*pt)[2] = g_dash.gate_pt;

    if (g_hud.map)
        return;
    redraw_clip();             /* 0x2d314 */
    if (s <= 0) {              /* (0xea800, 0 and read by nothing, would keep it shown) */
        if (g_lever_shown)
            blit_own(g_dash.gate_back, COPY);
        g_lever_shown = 0;
        return;
    }
    if (before < 0 || before > 8 || now < 0 || now > 8)
        return;
    if (s < 11 && wait < 1) {
        at = 1;
        x = pt[now][0], y = pt[now][1];
    } else if (s >= 14) {
        at = 2;
        x = pt[before][0], y = pt[before][1];
    } else if (s == 13) {
        at = 3;
        x = pt[before][0], y = pt[1][1];
    } else if (s == 12) {
        at = 4;
        x = pt[now][0], y = pt[1][1];
    } else if (s == 11 && wait < 1) {
        at = 5;
        x = pt[now][0], y = pt[now][1];
    }
    if (at == g_lever_shown)
        return;
    g_lever_shown = at;
    blit_own(g_dash.gate, COPY);
    if (g_dash.knob_n == 4 && g_dash.knob[3]) {
        int dx = (x - pt[1][0]) / 4, dy = (y - pt[1][1]) / 4, ox = 0, oy = 0;

        for (i = 3; i >= 0; i--, ox += dx, oy += dy)
            blit(g_dash.knob[i], pt[1][0] - s16(g_dash.knob[i] + 4) / 2 + ox,
                 pt[1][1] - s16(g_dash.knob[i] + 6) / 2 + oy, SEE_THROUGH);
    } else
        blit(g_dash.knob[0], x - s16(g_dash.knob[0] + 4) / 2, y - s16(g_dash.knob[0] + 6) / 2,
             SEE_THROUGH);
}

void hud_frame(const hud_car *c)
{
    int turn = wheel_turn(c->steer), shown = c->steer < 0 ? -turn : turn;
    /* the wheel's pictures turned left: steering left, or right on a mirrored track */
    int left = g_hud.mirrored ? c->steer >= 0 : c->steer < 0;

    /* (0x13c17c: nothing; the police's ticket or warning instead: hud_ticket) */
    if (g_redraw)
        hud_redraw();
    window_set(&g_hud_screen);
    if (g_hud.detail == 3)
        lines_clear(g_dash.list[2]);
    bamf_copy(&g_hud_view, g_dash.list[g_hud.detail & 3]);
    if (g_hud.mirror && g_hud_mirror.pix)          /* 0x5b040 */
        blit_pixels(g_hud_mirror.pix, g_hud_mirror.w, g_hud_mirror.h,
                    g_dash.place[0][0], g_dash.place[0][1], COPY);
    clip_set(0, H(0x102, 0x206), 0, g_hud.running ? H(0xc8, 0x1e0) : H(0xb4, 0x1c0));
    if (shown != g_wheel_shown)
        blit_own(g_dash.wheel[!left][turn], SEE_THROUGH);
    g_wheel_shown = shown;

    /* the gauges in the view's picture, then their boxes to the screen */
    window_set(&g_hud_view);
    blit_own(g_dash.tach, COPY);
    blit_own(g_dash.sped, c->number == 0xb ? SEE_THROUGH : COPY);
    needles(c);
    blit_own(g_dash.over_tach[!left][turn], SEE_THROUGH);
    blit_own(g_dash.over_sped[!left][turn], SEE_THROUGH);
    window_set(&g_hud_screen);
    clip_set(g_dash.tach_box[0], g_dash.tach_box[0] + g_dash.tach_box[2],
             g_dash.tach_box[1], g_dash.tach_box[1] + g_dash.tach_box[3]);
    blit_pixels(g_hud_view.pix, g_hud_view.w, g_hud_view.h, 0, 0, COPY);
    clip_set(g_dash.sped_box[0], g_dash.sped_box[0] + g_dash.sped_box[2],
             g_dash.sped_box[1], g_dash.sped_box[1] + g_dash.sped_box[3]);
    blit_pixels(g_hud_view.pix, g_hud_view.w, g_hud_view.h, 0, 0, COPY);

    window_set(&g_hud_screen);
    gear_lever(c);
    radar();
    hud_keys();
    bar_numbers(c);
    if (g_hud.map && g_hud_map.pix)
        blit_pixels(g_hud_map.pix, g_hud_map.w, g_hud_map.h, H(0x103, 0x206), H(0x7e, 0x130),
                    COPY);
    hud_bar_draw();            /* 0x5c5d0 */
}

/* hud_keys 0x2f644: the status bar again some steps after the start's lights; the
   dashboard's keys, one at a time until all are let go: F8 what the bar shows (the race
   or the segment; on Rusty Springs' time trial the acceleration test too), F7 the bar
   on or off, M the map (off: the dashboard drawn again; from outside the map's corner
   0x2f84c, here the whole screen round the view), F9 the driver's view without the
   dashboard (not after the race or in a replay, only from the driver's seat) */
static void hud_keys(void)
{
    if (g_hud.steps > g_bar_at)
        status_bar();
    if (key_down(0x42)) {                  /* F8 */
        if (g_keys_up) {
            g_hud.mode = g_hud.track == 3 && g_hud.race_type == 0 ? (g_hud.mode + 1) % 3
                                                                 : 1 - g_hud.mode;
            g_hud.shown = 1;
            status_bar();
            g_keys_up = 0;
        }
    } else if (key_down(0x41)) {           /* F7 */
        if (g_keys_up) {
            g_hud.shown = 1 - g_hud.shown;
            status_bar();
            g_keys_up = 0;
        }
    } else if (key_down(g_drive.map_key)   /* M: the settings' +0x5f4e, or the
                                              joystick's second button when the pedals
                                              and gears are not on its buttons */
               || (g_joy.on && g_drive.pedal_kind[g_player] != 3
                   && g_drive.gear_kind[g_player] != 3 && (g_joy.held & 2))) {
        if (g_keys_up) {
            g_hud.map = 1 - g_hud.map;
            if (!g_hud.map) {
                g_redraw = 1;
                if (g_hud.cam_mode != 0 || g_hud.no_dash)
                    g_view_redraw = 1;
                if (g_hud.detail == 3)     /* (0x2f7a3) */
                    g_hud.clear = 1;
            }
            g_keys_up = 0;
        }
    } else if (key_down(0x43) && !g_hud.stopped && g_hud.cam_mode <= 1) {
        if (g_keys_up) {                   /* F9 (0x15d13a, a replay: none yet) */
            g_hud.no_dash = !g_hud.no_dash;
            g_hud.view_changed = 1;
            g_keys_up = 0;
        }
    } else
        g_keys_up = 1;
}

/* a picture by its centre (0x812bc: less its +8, +0xa), copied */
static void blit_centred(const unsigned char *head, int x, int y)
{
    blit(head, x - s16(head + 8), y - s16(head + 0xa), COPY);
}

void hud_outside_screen(const int box[4])
{
    const unsigned char *tile = named(&g_mask, "0000"), *top = named(&g_mask, "b00t");
    const unsigned char *left = named(&g_mask, "b00l"), *right = named(&g_mask, "b00r");
    const unsigned char *bottom = named(&g_mask, "b00b");
    int w = s16(tile + 4), h = s16(tile + 6), tw = s16(top + 4), lh = s16(left + 6);
    int x0 = box[0], y0 = box[1], x1 = box[2] + x0 - 1, y1 = y0 + box[3] - 1, i, j, x, y;

    window_set(&g_hud_view);
    /* the tiles, two across in turn, from row 13 (640x480: 32) under the status bar
       (their names 0xeaa20: 0000 .. 0003) */
    for (i = 0; i < g_hud_view.w / w; i++)
        for (j = 0; j < g_hud_view.h / h; j++)
            blit(named(&g_mask, (const char *)exe_at((unsigned)exe_i32(0xeaa20 + 4 * ((i & 1) + 2 * j)), 4)),
                 i * w, H(0xd, 0x20) + j * h, COPY);
    status_bar();
    /* the border: its top and bottom, its sides, the sides' last pieces, the corners */
    for (x = x0; x < x1; x += tw) {
        blit_centred(top, x, y0);
        blit_centred(bottom, x, y1);
    }
    for (y = y0; y < y1 - lh; y += lh) {
        blit_centred(left, x0, y);
        blit_centred(right, x1, y);
    }
    y = y1 + 1 - lh;
    blit_centred(left, x0, s16(left + 0xa) + y);
    blit_centred(right, x1, s16(right + 0xa) + y);
    blit_centred(named(&g_mask, "b000"), x0, y0);
    blit_centred(named(&g_mask, "b001"), x1, y0);
    blit_centred(named(&g_mask, "b002"), x1, y1);
    blit_centred(named(&g_mask, "b003"), x0, y1);
    blit_own(named(&g_mask, "tach"), SEE_THROUGH);
    numbers_reset();
}

/* the small rev counter's needle (0x31168): a quarter of the way out from its centre to
   its end, on the 1024-a-turn table from 0x1a9 round by 600 at the red line; the dial
   put back under the old one first, in a box round each of its halves */
static void small_tach(const hud_car *c)
{
    const unsigned char *dial = named(&g_mask, "tach");
    int r = c->rpm < c->redline + 0x3d4 ? c->rpm : c->redline + 0x3d4;
    int a = r * 600 / c->redline + 0x1a9;
    int ex = g_tach_c[0] + mulr(g_tach_r[0], fix_cos(a));
    int ey = g_tach_c[1] + mulr(g_tach_r[1], fix_sin(a));
    int sx = (g_tach_c[0] + (g_tach_c[0] + ex) / 2) / 2;
    int sy = (g_tach_c[1] + (g_tach_c[1] + ey) / 2) / 2;
    int *o = g_hud_small_needle, mx = (o[0] + o[2]) / 2, my = (o[1] + o[3]) / 2;

    window_set(&g_hud_screen);
    clip_set((mx < o[0] ? mx : o[0]) - 2, (mx > o[0] ? mx : o[0]) + 2,
             (my < o[1] ? my : o[1]) - 2, (my > o[1] ? my : o[1]) + 2);
    blit_own(dial, SEE_THROUGH);
    clip_set((mx < o[2] ? mx : o[2]) - 2, (mx > o[2] ? mx : o[2]) + 2,
             (my < o[3] ? my : o[3]) - 2, (my > o[3] ? my : o[3]) + 2);
    blit_own(dial, SEE_THROUGH);
    clip_set(0, g_win.w, 0, g_win.h);
    if (g_view_res) {
        /* at 640x480 three of EA's lines a pixel apart in the HUD's first colour */
        poly_line(ex, ey, sx, sy, g_hud_colour[0]);
        poly_line(ex + 1, ey, sx + 1, sy, g_hud_colour[0]);
        poly_line(ex, ey + 1, sx, sy + 1, g_hud_colour[0]);
    } else
        poly_wide_line(sx << 16, sy << 16, ex << 16, ey << 16, g_needle_colour,
                       (const unsigned char (*)[256])g_needle_blend, 0x10000, 0x10000,
                       g_hud_screen.w, g_hud_screen.h);
    o[0] = ex;
    o[1] = ey;
    o[2] = sx;
    o[3] = sy;
}

void hud_outside_frame(const hud_car *c, int redraw, int layout)
{
    /* (0x13c17c: nothing; the police's ticket or warning instead, and the background
       again after them, 0xeaaa8: hud_ticket) */
    if (redraw) {
        /* the dial into the view's picture, then all of it to the screen */
        window_set(&g_hud_view);
        blit_own(named(&g_mask, "tach"), SEE_THROUGH);
        window_set(&g_hud_screen);
        redraw_clip();
        blit_pixels(g_hud_view.pix, g_hud_view.w, g_hud_view.h, 0, 0, COPY);
    } else {
        const unsigned char **lists = g_lists[g_hud.map != 0] + (layout > 1 ? layout - 1 : 0) * 4;

        window_set(&g_hud_screen);
        if (g_hud.detail == 3)
            lines_clear(lists[2]);
        bamf_copy(&g_hud_view, lists[g_hud.detail & 3]);
    }
    hud_keys();
    bar_numbers(c);
    small_tach(c);
    if (g_hud.map && g_hud_map.pix)
        blit_pixels(g_hud_map.pix, g_hud_map.w, g_hud_map.h, H(0x103, 0x206), H(0x7e, 0x130), COPY);
    hud_bar_draw();            /* 0x5c5d0 */
}

/* the map (0x31cb4), 58 x 54 at 320x200: positions in 1/2^20 of the world's units round
   the viewed car's node, turned by its heading, x the other way on a mirrored track */
/* the map's corner (1, 1; 640x480: 2, 2), its scale (a node's place >> 20; 19) and its
   centre: the road's (+0x1c, +0x1c; +0x38, +0x45), the dots' (+0x1c, +0x19; +0x38, +0x46) */
#define MAP_O H(1, 2)
#define MAP_SHIFT H(20, 19)
#define MAP_X (MAP_O + H(0x1c, 0x38))
#define MAP_Y (MAP_O + H(0x1c, 0x45))
#define MAP_DOT_Y (MAP_O + H(0x19, 0x46))

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static const unsigned char *map_node(int node)
{
    return g_track.nodes + (long)(node & (int)g_track.node_mask) * 0x24;
}

typedef struct {
    int px, pz, c, s, mir;
} map_view;

static void map_point(const map_view *v, int node, int *x, int *y)
{
    const unsigned char *r = map_node(node);
    int dx = (rd32(r + 8) >> MAP_SHIFT) - v->px, dz = (rd32(r + 0x10) >> MAP_SHIFT) - v->pz;

    *x = (mulr(v->c, dx) - mulr(v->s, dz)) * v->mir;
    *y = mulr(v->s, dx) + mulr(v->c, dz);
}

/* a racer's dot (0x32ac0): a square of 4 in its colour under the "pos1" ring, kept off
   the map's top and bottom rows; 640x480: 7 by 5 and a line above and below, a round dot */
static void map_dot(int x, int y, int colour)
{
    if (g_view_res) {
        clip_set(2, 0x72, 9, 0x85);
        raster_fill(x - 3, y - 3, 7, 5, colour);
        poly_line(x - 1, y - 4, x + 1, y - 4, colour);
        poly_line(x - 1, y + 2, x + 1, y + 2, colour);
        blit(named(&g_mask, "pos1"), x - 4, y - 5, SEE_THROUGH);
        clip_set(2, 0x72, 2, 0x8c);
        return;
    }
    clip_set(1, 0x39, 6, 0x30);
    raster_fill(x - 2, y + 1, 4, 4, colour);
    blit(named(&g_mask, "pos1"), x - 3, y, SEE_THROUGH);
    clip_set(1, 0x39, 1, 0x35);
}

/* the nodes between two racers, the shorter way (round a circuit too) */
static int map_apart(int d)
{
    int a = abs(d), b = abs(d + g_track.nodes_len), c = abs(g_track.nodes_len - d);

    return a < b && a < c ? a : b < c ? b : c;
}

/* the map's frame (0x8a0f0): a rectangle's outline from (o, o) to (x1, y1) */
static void map_frame(int o, int x1, int y1)
{
    raster_fill(o, o, x1 - o + 1, 1, g_map_frame);
    raster_fill(x1, o + 1, 1, y1 - o - 1, g_map_frame);
    raster_fill(o, o + 1, 1, y1 - o - 1, g_map_frame);
    raster_fill(o, y1, x1 - o + 1, 1, g_map_frame);
}

void hud_map(const hud_map_state *m)
{
    int track = g_hud.track, range = (short)exe_i32(0xea694 + 4 * track);
    int count = g_hud.race_type > 1 ? 8 : g_hud.race_type == 0 ? 1 : 2;
    int i, k, node, x, y, x0, y0, x1, y1, a, c4, s4, t;
    int seg_end = track < 3 ? (short)(exe_at(0xeaab4 + 2 * (track * 3 + g_hud.segment), 2)[0]
                                      | exe_at(0xeaab4 + 2 * (track * 3 + g_hud.segment), 2)[1] << 8)
                            : 0;
    const unsigned char *r = map_node(m->node);
    map_view v;

    if (!g_hud.map)
        return;
    v.mir = g_hud.mirrored ? -1 : 1;
    v.px = rd32(r + 8) >> MAP_SHIFT;
    v.pz = rd32(r + 0x10) >> MAP_SHIFT;
    a = (int)((unsigned)(short)(r[0x18] | r[0x19] << 8) << 10) >> 14;
    v.c = fix_cos(a);
    v.s = fix_sin(a);

    window_set(&g_hud_map);
    if (m->driver) {
        if (g_map_back_taken)
            blit_pixels(g_hud_map_back.pix, g_hud_map_back.w, g_hud_map_back.h, MAP_O, MAP_O, COPY);
    } else
        blit(named(&g_mask, "mpbd"), MAP_O, MAP_O, SEE_THROUGH);
    /* the frame (0x8a0f0); 640x480 twice, the second one pixel further out */
    map_frame(MAP_O - 1, MAP_O + H(0x38, 0x70), MAP_O + H(0x34, 0x8a));
    if (g_view_res)
        map_frame(MAP_O - 2, MAP_O + 0x71, MAP_O + 0x8b);
    clip_set(MAP_O, MAP_O + H(0x38, 0x70), MAP_O, MAP_O + H(0x34, 0x8a));

    /* the road: from `range` nodes behind, then 16 apart (City-like circuit: 13.5),
       open roads kept to their first node and 60 past the segment's end */
    node = (short)(m->node - range);
    if (node < 0)
        node = track < 3 ? 0 : (short)(node + g_track.nodes_len);
    map_point(&v, node, &x0, &y0);
    for (i = (short)exe_i32(0xea6b8 + 4 * track); i <= exe_i32(0xea6dc + 4 * track); i++) {
        node = (short)(track == 3 ? (i * 0x6c >> 3) + (short)m->node : m->node + i * 16);
        if (track < 3) {
            if (node < 0)
                node = 0;
            else if (node > seg_end + 0x3c)
                node = (short)(seg_end + 0x3c);
        } else if (node < 0)
            node = (short)(node + g_track.nodes_len);
        map_point(&v, node, &x, &y);
        poly_line(MAP_X + x0, MAP_Y - y0, MAP_X + x, MAP_Y - y, g_hud_colour[HUD_WHITE]);
        x0 = x;
        y0 = y;
    }

    /* the segment's end (the circuits' start): a short red bar across the road */
    map_point(&v, seg_end, &x0, &y0);
    map_point(&v, seg_end + 8, &x1, &y1);
    t = y1 - y0;
    a = ea_atan_1024(x1 - x0, t + (t >> 2));
    c4 = mulr(4, -fix_cos(a));
    s4 = mulr(4, fix_sin(a));
    t = s4 - (s4 >> 2) + (s4 >> 4);
    poly_line(x0 + MAP_X + (c4 >> 1), MAP_Y - (y0 + (t >> 1)),
              x0 + MAP_X - (c4 - (c4 >> 1)), MAP_Y - (y0 - (t - (t >> 1))),
              g_hud_colour[HUD_RED]);

    /* the racers, the last first: a dot on the map, else an arrow at its edge, above
       when ahead of the player (behind: below) */
    for (k = count - 1; k >= 0; k--) {
        int apart = (short)map_apart((short)(m->node - m->nodes[k]));
        int colour = g_marker_colour[k], bx, ahead;

        if (apart <= range) {
            map_point(&v, m->nodes[k], &x, &y);
            if ((x < 0 ? -x : x) < H(0x1c, 0x38) && (y < 0 ? -y : y) < H(0x17, 0x3d)) {
                map_dot(MAP_X + x, MAP_DOT_Y - y, colour);
                continue;
            }
        }
        bx = g_hud.race_type == 1 ? MAP_O + H(0x2e, 0x5b) : MAP_O + H(0x31 - 7 * k, 0x60 - 0xd * k);
        ahead = g_hud.race_type == 1 ? m->place == 1 : m->places[k] > m->place;
        if (ahead) {
            int y0 = MAP_O + H(0x2f, 0x83), w = H(2, 4);
            int tri[6] = { bx + 1, y0 + 1, bx + 1 + 2 * w, y0 + 1, bx + 1 + w, y0 + 1 + w };
            poly_fill(3, tri, poly_solid_rows, colour);
            blit(named(&g_mask, "aro1"), bx, y0, SEE_THROUGH);
        } else {
            int y0 = MAP_O + H(3, 5), w = H(2, 4);
            int tri[6] = { bx + 1, y0, bx + 1 + 2 * w, y0, bx + 1 + w, y0 - w };
            poly_fill(3, tri, poly_solid_rows, colour);
            blit(named(&g_mask, "aru1"), bx, MAP_O - H(1, 0), SEE_THROUGH);
        }
    }

    /* the police, near enough: its dot flashing */
    if (m->cop) {
        int apart = (short)(m->node - m->cop_node);

        apart = (short)(apart < 0 ? -apart : apart);
        if (apart < 0x140) {
            map_point(&v, m->cop_node, &x, &y);
            if ((x < 0 ? -x : x) < H(0x1c, 0x38) && (y < 0 ? -y : y) < H(0x1d, 0x44)) {
                if (++g_hud.cop_flash > 10)
                    g_hud.cop_flash = 0;
                map_dot(MAP_X + x, MAP_DOT_Y - y, g_marker_colour[g_hud.cop_flash > 5 ? 0 : 6]);
            }
        }
    }
}

void hud_mirror_flip(void)
{
    int y, i;

    for (y = 0; y < g_hud_mirror.h; y++) {
        unsigned char *r = g_hud_mirror.pix + (long)y * g_hud_mirror.w, t;
        for (i = 0; i < g_hud_mirror.w / 2; i++) {
            t = r[i];
            r[i] = r[g_hud_mirror.w - 1 - i];
            r[g_hud_mirror.w - 1 - i] = t;
        }
    }
}
