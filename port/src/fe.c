/* The front end. */
#include "fe.h"
#include "joystick.h"
#include "session.h"
#include "mainmenu.h"
#include "font.h"
#include "config.h"
#include "files.h"
#include "fsh.h"
#include "menu.h"
#include "movie.h"
#include "race.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0x98abc: each channel x level / 32 */
static void scale(unsigned char *out, const unsigned char *in, int n, int level)
{
    for (int i = 0; i < n; i++)
        out[i] = (unsigned char)(in[i] * level >> 5);
}

void pal_fade_up(const unsigned char *pal, int first, int count)
{
    unsigned char p[768];
    for (int level = 0; level <= 32; level += 2) {
        scale(p, pal, 3 * count, level);
        video_retrace();
        video_palette(p, first, count);
    }
    inv_build();                           /* 0x8227c */
}

void pal_fade_down(int first, int count)
{
    unsigned char from[768], p[768];
    video_get_palette(from, first, count);
    for (int level = 32; level >= 0; level -= 2) {
        scale(p, from, 3 * count, level);
        video_retrace();
        video_palette(p, first, count);
    }
}

/* drawshapeatpos 0x8129c for an 8-bit picture: at its own x, y, colour 0 drawn too */
static void draw_picture(const shape *s)
{
    for (int y = 0; y < s->h && s->y + y < g_fb_h; y++) {
        int w = s->w;
        if (s->x + w > g_fb_w)
            w = g_fb_w - s->x;
        if (w > 0)
            memcpy(g_fb + (s->y + y) * g_fb_w + s->x, s->pixels + y * s->w, w);
    }
}

int fe_show_picture(const char *name, int wait)
{
    char path[256];
    unsigned char pal[768], pal_black[768];
    fsh_file f;
    shape bg;

    snprintf(path, sizeof path, "%s/%s", art_dir(), name);
    if (!fsh_load(&f, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    if (!fsh_find(&f, "bgnd", &bg) || bg.type != 0x7b || !fsh_palette(&f, 0, pal)) {
        fprintf(stderr, "renfs: %s has no picture\n", path);
        fsh_free(&f);
        return 0;
    }
    memset(pal_black, 0, sizeof pal_black);   /* the screen is black before it comes up */
    video_palette(pal_black, 0, 256);
    memset(g_fb, 0, (size_t)g_fb_w * g_fb_h);
    draw_picture(&bg);
    fsh_free(&f);
    pal_fade_up(pal, 0, 256);
    if (wait) {
        /* 0x74b20 with mode 0: until Space, Enter or Esc, or the time set before */
        while (!g_quit && !timedout()) {
            int k = key_get();
            if (k == 0x20 || k == 0x0d || k == 0x1b)
                break;
            video_retrace();
        }
        pal_fade_down(0, 256);
    }
    return 1;
}

void fe_title_screen(void)
{
    settimeout(500);                       /* 5 seconds */
    if (!fe_show_picture("TITLE.QFS", 0))
        return;
    while (!g_quit && !timedout() && !key_get())
        video_retrace();
    pal_fade_down(0, 256);
}

int fe_attract_mode(void)
{
    static int round, attract_n, replay_n;     /* 0xeaaca: a round played, the replay */
    unsigned char black[768] = { 0 };
    int stopped = 0;

    /* from the second round on a replay first (0x342b7): REPLAY1..3.RPL in turn from
       REPLAY2, its highlights (+0xaf 3, race_run in replay mode); Done (Esc) there skips
       the films. (The original plays it only on a machine its speed test finds fast,
       0xec414 > 5,400,000: the port's machine is) */
    if (round) {
        char name[32], path[64];
        FILE *f;

        replay_n = (replay_n + 1) % 3;
        snprintf(name, sizeof name, "REPLAY%d.RPL", replay_n + 1);
        memset(g_config + 0x3b, 0, 0x20);
        memcpy(g_config + 0x3b, name, strlen(name));
        config_set_word(0xaf, 3);
        snprintf(path, sizeof path, "GAMEDATA/REPLAY/%s", name);
        if ((f = file_open(path))) {
            fclose(f);
            stopped = race_run() == 0x40;      /* (race_run: 1 for the attract mode's
                                                  0x40) */
            mouse_limits(0, 0, 640, 480);      /* (0x3437e: the replay bar's taken off) */
        }
        config_set_word(0xaf, 0);
    }
    if (round++ == 0) {
        int r = (rand() & 0x7fff) / 0x2aaa;   /* 0x74aec: Watcom's rand is 15 bits; 3 = 0 */
        attract_n = r > 2 ? 0 : r;
    } else
        attract_n = (attract_n + 1) % 3;
    if (!stopped && !g_quit)
        stopped = movie_play("EA.TGV") > 0;
    if (!stopped && !g_quit) {
        char name[32];
        snprintf(name, sizeof name, "ATTRACT%d.TGV", attract_n + 1);
        stopped = movie_play(name) > 0;
    }
    if (!stopped && !g_quit)
        movie_play("TITLE.TGV");
    video_palette(black, 0, 256);
    video_mode(640, 480);
    video_retrace();
    video_palette(black, 0, 256);
    return 1;
}

int fe_game_mode_screen(void)
{
    /* the three buttons: lit, dark; the mouse put in their middle (0x41d64, 0x41d6a) */
    static const char *const lit[3] = { "sinl", "modl", "exil" };
    static const char *const dark[3] = { "sind", "modd", "exid" };
    static const int button_x[3] = { 320, 320, 320 }, button_y[3] = { 167, 242, 315 };
    char path[256];
    unsigned char pal[768];
    fsh_file f;
    shape bg, sh;
    int sel = 0, prev = 0, changed = 0, redraw = 1, first = 1, done = 0, result = 0;
    int ready = 1, held = 0, released = 1;
    int mx, my;

    mouse_place(button_x[0], button_y[0]);
    fe_sound_bank_load();
    snprintf(path, sizeof path, "%s/CONTROL/GAMEMOD2.QFS", art_dir());
    if (!fsh_load(&f, path) || !fsh_find(&f, "bgnd", &bg) || !fsh_palette(&f, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        fe_sound_bank_free();
        return 1;
    }
    fe_mouse_load();
    fe_mouse_save();
    for (;;) {
        /* the mouse over a button picks it */
        mouse_read(&mx, &my);
        {
            int over = my <= 0xcd ? 0 : my <= 0x118 ? 1 : 2;
            if (over != sel && over != 1) {     /* (the modem game: never chosen) */
                prev = sel;
                sel = over;
                changed = 1;
            }
        }
        if (redraw) {
            draw_picture(&bg);
            /* (the port's own: the modem game greyed out, never chosen: no network in
               the port. The file's "modm", that button dimmed, which the program never
               shows) */
            if (fsh_find(&f, "modm", &sh) || fsh_find(&f, dark[1], &sh))
                draw_picture(&sh);
            if (fsh_find(&f, lit[sel], &sh))
                draw_picture(&sh);
            video_retrace();
            mouse_show(1);                 /* 0x52288 */
            g_mouse_active = 1;
            if (first) {
                pal_fade_up(pal, 0, 256);
                inv_build();               /* 0x8227c again */
                first = 0;
            }
            settimeout(4500);              /* 45 seconds */
            changed = redraw = 0;
        }
        if (changed) {
            redraw = 1;
            if (prev != sel)
                fe_play_sound(0x47);
            if (prev != 1 && fsh_find(&f, dark[prev], &sh))
                draw_picture(&sh);
            if (fsh_find(&f, lit[sel], &sh))
                draw_picture(&sh);
            changed = 0;
        }
        /* a key acts once, then again only after a quarter of a second held (the arrows)
           or once let go */
        {
            int k = fe_read_keys();
            if (!ready && k != 0x1b && k != 0x20 && k != 0x0d) {
                held += (int)ticksince();
                if (held > 25)
                    ready = 1;
            }
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        {
            int k = fe_read_keys();
            if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
                fe_mouse_press();
                if (ready) {
                    fe_play_sound(0x48);
                    if (sel == 0) {
                        if (fe_enter_name((char *)g_config)) {
                            done = 1;
                            result = 0;
                        }
                    } else if (sel == 2) {
                        done = 1;
                        result = 1;
                    }
                    /* (sel 1, the modem game: greyed out, never chosen; no network in
                       the port) */
                }
                ticksince();
                released = ready = held = 0;
            } else if (k == 0 || !ready) {
                if (k == 0)
                    ready = 1;
            } else {
                if (k == 0x1b) {
                    fe_play_sound(0x48);
                    if (fe_ask_exit()) {
                        done = 1;
                        result = 1;
                    }
                    fe_flush_keys();
                } else if (k == 0x4800 || k == 0x5000) {
                    prev = sel;
                    sel = k == 0x4800 ? (sel + 2) % 3 : (sel + 1) % 3;
                    if (sel == 1)          /* (the modem game skipped) */
                        sel = k == 0x4800 ? 0 : 2;
                    mouse_place(button_x[sel], button_y[sel]);
                    fe_mouse_save();
                    changed = 1;
                }
                settimeout(4500);
                ticksince();
                ready = held = 0;
            }
        }
        video_retrace();                   /* the original runs on; the port waits here */
        if (g_quit) {
            done = 1;
            result = 1;
        }
        if (!first && !done) {
            if (timedout()) {              /* nothing chosen in 45 seconds: the movies */
                fe_play_sound(0x48);
                done = 1;
                result = 2;
            } else if (g_mouse_buttons == 2 && released) {
                if (fe_ask_exit()) {
                    done = 1;
                    result = 1;
                }
                fe_flush_keys();
                do {                       /* until the button is let go */
                    if (g_mouse_active)
                        fe_mouse_read();
                    video_retrace();
                } while ((fe_read_keys() || g_mouse_buttons) && !g_quit);
            }
        }
        if (done)
            break;
    }
    mouse_show(0);
    fe_sound_bank_free();
    pal_fade_down(0, 256);
    fsh_free(&f);
    return result;
}

void fe_outer_loop(void)
{
    int state = 2, again = 1, first = 1;
    fe_font_load();                        /* 0x42250, at start-up */
    joy_startup();                         /* 0x4d1fc: a joystick, calibrated */
    do {
        if (again) {
            music_fade_out();
            music_wait_end();
            fe_attract_mode();
            again = 0;
            if (first) {
                first = 0;
                if (!config_load())
                    state = 3;
            }
            music_choose();
        }
        if (state == 2 || state == 3)
            fe_title_screen();
        state = fe_game_mode_screen();
        if (state == 2)
            again = 1;
        if (state == 0) {
            session_new();                 /* 0x7469c */
            state = fe_main_menu();        /* 2: the title, then the game mode again */
            config_save();
        }
    } while (state != 1 && !g_quit);
    music_fade_out();
    music_wait_end();
}
