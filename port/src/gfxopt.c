/* The graphics screen (fe_graphics_options 0x32d08, NOTES §4.2): the races' resolution,
   the window's size, the image quality, the view distance, the cars' detail and the
   horizon, each a picture chosen by its setting. Enter or a click on a row steps its
   setting on, F1-F4 step the window, quality, distance and horizon from any row. From
   the options screen, the race menus' options and the pause menu (where the resolution
   cannot change). Done keeps it all, Cancel and Esc put the settings back. */
#include "gfxopt.h"
#include "mister.h"
#include "config.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

enum { RESOLUTION, WINDOW, QUALITY, DISTANCE, CAR_DETAIL, HORIZON, DONE, CANCEL };

/* the settings the rows show (CONFIG.DAT words) */
#define CFG_RESOLUTION 0x7f    /* 0 races at 640x480, 1 at 320x200 */
#define CFG_WINDOW 0x81        /* 0 full, 1 medium, 2 small: the outside view's size */
#define CFG_QUALITY 0x83       /* 0..3 (at 320x200 0..1): the view's pixels */
#define CFG_DISTANCE 0x85      /* 0 far, 1 medium, 2 close */
#define CFG_CAR_DETAIL 0x87    /* 0 high, 1 low */
#define CFG_HORIZON 0x89       /* 0 on, 1 off */

/* the mouse put on a row by the keys (0x32bf0, 0x32c00) */
static const int row_x[8] = { 230, 230, 230, 230, 230, 230, 60, 565 };
static const int row_y[8] = { 116, 169, 220, 270, 325, 377, 445, 445 };

static fsh_file g_art;

static void art(const char *name)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, 0);
}

static void value(const char *const *names, int n, int v)
{
    art(names[v >= 0 && v < n ? v : 0]);
}

/* a row: its label lit and its value's lit picture when chosen, else the value's dark
   one (the dark labels are the background's) */
static void row(int sel, int r, const char *label, const char *const *lit,
                const char *const *dark, int n, int at)
{
    if (sel == r)
        art(label);
    value(sel == r ? lit : dark, n, config_word(at));
}

static void draw(int sel, int where)
{
    /* 0x32c10 .. 0x32ca4 */
    static const char *const res_lit[2] = { "640l", "320l" }, *const res_dark[2] = { "640d", "320d" };
    static const char *const res_dim[2] = { "640m", "320m" };
    static const char *const win_lit[3] = { "full", "wmdl", "smal" };
    static const char *const win_dark[3] = { "fuld", "wmdd", "smad" };
    static const char *const ima_lit[6] = { "ihil", "imdl", "ilwl", "intl", "atml", "atll" };
    static const char *const ima_dark[6] = { "ihid", "imdd", "ilwd", "intd", "atmd", "atld" };
    static const char *const van_lit[3] = { "farl", "vmdl", "clsl" };
    static const char *const van_dark[3] = { "fard", "vmdd", "clsd" };
    static const char *const car_lit[2] = { "chil", "clwl" }, *const car_dark[2] = { "chid", "clwd" };
    static const char *const hor_lit[2] = { "honl", "offl" }, *const hor_dark[2] = { "hond", "offd" };

    art("bgnd");
    row(sel, RESOLUTION, "scrl", res_lit, res_dark, 2, CFG_RESOLUTION);
    row(sel, WINDOW, "winl", win_lit, win_dark, 3, CFG_WINDOW);
    row(sel, QUALITY, "imal", ima_lit, ima_dark, 6, CFG_QUALITY);
    row(sel, DISTANCE, "vanl", van_lit, van_dark, 3, CFG_DISTANCE);
    row(sel, CAR_DETAIL, "carl", car_lit, car_dark, 2, CFG_CAR_DETAIL);
    row(sel, HORIZON, "horl", hor_lit, hor_dark, 2, CFG_HORIZON);
    if (sel == DONE)
        art("donl");
    if (sel == CANCEL)
        art("canl");
    if (where == GFX_PAUSE) {
        art("scrm");
        value(res_dim, 2, config_word(CFG_RESOLUTION));
    }
}

static void place(int sel)
{
    mouse_place(row_x[sel], row_y[sel]);   /* 0x71ad4 */
    fe_mouse_save();
}

/* a setting stepped round its n values */
static void step(int at, int n)
{
    config_set_word(at, (config_word(at) + 1) % n);
}

/* the image quality's next: at 320x200 only the first two */
static void step_quality(void)
{
    step(CFG_QUALITY, 4);
    if (config_word(CFG_RESOLUTION) == 1 && config_word(CFG_QUALITY) > 1)
        config_set_word(CFG_QUALITY, 0);
}

int fe_graphics_options(int where)
{
    unsigned char pal[768], keep[0xac];
    char path[256];
    int sel = where == GFX_PAUSE ? WINDOW : RESOLUTION;
    int redraw = 1, first = 1, changed = 0, done = 0, ready = 1, released = 1, k;
    unsigned held = 0;

    memcpy(keep, g_config + 9, sizeof keep);
    g_fkeys = 1;                   /* (0x12bd3c: F1-F4 read) */
    place(sel);
    if (where == GFX_OPTIONS)
        snprintf(path, sizeof path, "%s/OPTION/GRAPHICS.QFS", art_dir());
    else
        snprintf(path, sizeof path, "%s/CHECK/GRAPHICS.FSH", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        g_fkeys = 0;
        return 0;
    }
    if (g_mouse_active)
        fe_mouse_read();
    fe_mouse_save();

    while (!done) {
        int mx, my, over = -1;

#ifdef MISTER_HYBRID
        /* (the MiSTer's: the races' resolution is the OSD's; this row shows it) */
        if (mister_on() && config_word(CFG_RESOLUTION) != (mister_resolution() == MR_640x480 ? 0 : 1)) {
            config_set_word(CFG_RESOLUTION, mister_resolution() == MR_640x480 ? 0 : 1);
            if (config_word(CFG_RESOLUTION) == 1 && config_word(CFG_QUALITY) > 1)
                config_set_word(CFG_QUALITY, 1);
            redraw = 1;
        }
#endif
        /* the mouse picks a row by its height (the resolution's not in the pause menu) */
        mouse_read(&mx, &my);
        if (my <= 0x8c && where != GFX_PAUSE)
            over = RESOLUTION;
        else if (my >= 0x91 && my <= 0xc1)
            over = WINDOW;
        else if (my >= 0xc6 && my <= 0xf2)
            over = QUALITY;
        else if (my >= 0xf7 && my <= 0x127)
            over = DISTANCE;
        else if (my >= 300 && my <= 0x15c)
            over = CAR_DETAIL;
        else if (my >= 0x161 && my <= 400)
            over = HORIZON;
        else if (mx <= 0x140 && my >= 0x1a4 && my <= 0x1e0)
            over = DONE;
        else if (mx >= 0x145 && my >= 0x1a4 && my <= 0x1df)
            over = CANCEL;
        if (over >= 0 && over != sel) {
            sel = over;
            changed = 1;
        }

        if (redraw) {
            draw(sel, where);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            g_mouse_active = 1;
            if (first) {
                if (g_mouse_active)
                    fe_mouse_read();
                fe_styles_init();
                pal_fade_up(pal, 0, 256);
                first = 0;
            }
            settimeout(12000);
            redraw = 0;
        }
        if (changed) {                     /* the rows drawn again, the whole screen after */
            fe_play_sound(0x47);
            redraw = 1;
            changed = 0;
        }

        if (!ready) {
            held += ticksince();
            if (held > 25)
                ready = 1;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();

        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                switch (sel) {
                case RESOLUTION:           /* (at 320x200 the quality at most the second) */
                    g_config[CFG_RESOLUTION] ^= 1;
                    if (config_word(CFG_RESOLUTION) == 1 && config_word(CFG_QUALITY) > 1)
                        config_set_word(CFG_QUALITY, 1);
                    break;
                case WINDOW:
                    step(CFG_WINDOW, 3);
                    break;
                case QUALITY:
                    step_quality();
                    break;
                case DISTANCE:
                    step(CFG_DISTANCE, 3);
                    break;
                case CAR_DETAIL:
                case HORIZON:
                    g_config[sel == CAR_DETAIL ? CFG_CAR_DETAIL : CFG_HORIZON] ^= 1;
                    break;
                case CANCEL:
                    memcpy(g_config + 9, keep, sizeof keep);
                    /* fall through */
                case DONE:
                    done = 1;
                    break;
                }
                if (sel <= HORIZON) {
                    if (!g_mouse_active)
                        place(sel);
                    changed = 1;
                }
            }
            ticksince();
            released = ready = 0;
            held = 0;
        } else if (k && ready) {
            int to = -1;

            if (k >= 0x3b00 && k <= 0x3e00) {
                /* F1 the window, F2 the quality, F3 the distance, F4 the horizon: the
                   mouse put on that row */
                fe_play_sound(0x48);
                if (k == 0x3b00) {
                    step(CFG_WINDOW, 3);
                    to = WINDOW;
                } else if (k == 0x3c00) {
                    step_quality();
                    to = QUALITY;
                } else if (k == 0x3d00) {
                    step(CFG_DISTANCE, 3);
                    to = DISTANCE;
                } else {
                    g_config[CFG_HORIZON] ^= 1;
                    to = HORIZON;
                }
                place(to);
                changed = 1;
                released = 0;
            } else if (k == 0x1b) {        /* as Cancel */
                memcpy(g_config + 9, keep, sizeof keep);
                fe_play_sound(0x48);
                done = 1;
            } else if (k == 0x4800) {      /* up: from Cancel as from Done, round to Done */
                fe_mouse_hide();
                sel = (sel == CANCEL ? DONE : sel) - 1;
                if (sel < 0 || (where == GFX_PAUSE && sel == RESOLUTION))
                    sel = DONE;
                place(sel);
                changed = 1;
            } else if (k == 0x5000) {      /* down: from the buttons to the top */
                fe_mouse_hide();
                sel = sel == CANCEL ? RESOLUTION : (sel + 1) % 7;
                if (where == GFX_PAUSE && sel == RESOLUTION)
                    sel = WINDOW;
                place(sel);
                changed = 1;
            } else if ((k == 0x4b00 || k == 0x4d00) && (sel == DONE || sel == CANCEL)) {
                fe_mouse_hide();           /* between Done and Cancel */
                sel = sel == DONE ? CANCEL : DONE;
                place(sel);
                changed = 1;
            }
            ticksince();
            ready = 0;
            held = 0;
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
        if (!done && !first && (timedout() || (g_mouse_buttons == 2 && released))) {
            fe_play_sound(0x48);           /* 2 minutes, or the right button: as Done */
            done = 1;
        }
    }
    g_fkeys = 0;
    pal_fade_down(0, 256);
    fsh_free(&g_art);
    fe_mouse_save();
    return 0;
}
