/* The race held (NOTES §4.20). */
#include "pause.h"
#include "mister.h"
#include "ff.h"
#include "audioopt.h"
#include "gfxopt.h"
#include "camera.h"
#include "cardraw.h"
#include "config.h"
#include "drive.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "hud.h"
#include "menu.h"
#include "opponent.h"
#include "racekeys.h"
#include "racesnd.h"
#include "replay.h"
#include "session.h"
#include "sound.h"
#include "timing.h"
#include "video.h"
#include "view.h"
#include <stdio.h>
#include <string.h>

race_pause g_pause;

enum { K_ESC = 1, K_P = 0x19 };

/* the screen shown until P is down (or up) */
static void wait_key(int down)
{
    while (!g_quit && !key_down(K_P) != !down)
        video_retrace();
}

/* 0x63110: P: "paus" over the race, every sound quiet, until P is let go, pressed and
   let go again; then the dashboard drawn again, the camera's view set up again */
static void hold(race_camera *cam)
{
    unsigned long from = clock_ticks();

    hud_paused();
    rsnd_quiet();
    ff_forces(0);              /* (0x6312a) */
    wait_key(0);
    wait_key(1);
    hud_redraw_due();
    camera_set_mode(cam, cam->mode);
    wait_key(0);
    rsnd_music_back();         /* (0x52640) */
    g_pause.pending = 0;
    g_pause.last = g_drive.steps;
    g_pause.held_ticks += clock_ticks() - from;
    ff_forces(1);              /* (0x63238) */
}

#ifdef MISTER_HYBRID
/* (the MiSTer's) the OSD open: the race held as P holds it, until the OSD is closed */
static void hold_osd(race_camera *cam)
{
    unsigned long from = clock_ticks();

    hud_paused();
    rsnd_quiet();
    ff_forces(0);
    while (!g_quit && mister_osd_open())
        video_retrace();
    hud_redraw_due();
    camera_set_mode(cam, cam->mode);
    rsnd_music_back();
    g_pause.held_ticks += clock_ticks() - from;
    ff_forces(1);
}
#endif

/* race_pause_menu 0x56474: the race's pause menu, at 640x480 */

enum { CONTINUE, REPLAY, AUDIO, GRAPHICS, CHAT, QUIT, NONE };

static fsh_file g_art, g_map;

static void art(const fsh_file *f, const char *name)
{
    shape s;
    if (fsh_find(f, name, &s))
        fe_draw_shape(&s, 0);
}

/* drawshape_centred 0x81600: by the shape's centre, colour 255 left out */
static void marker(const char *name, int x, int y)
{
    shape s;
    if (fsh_find(&g_art, name, &s)) {
        s.x = x - s.cx;
        s.y = y - s.cy;
        fe_draw_shape(&s, 1);
    }
}

static int word(unsigned at)
{
    const unsigned char *p = exe_at(at, 2);
    return p ? (short)(p[0] | p[1] << 8) : 0;
}

/* a car's place on the map: the track's points (x and z in metres, a pair of words every
   4 nodes, the open roads' a segment each), x / 25 and -z / 25 on the open roads, / 6 on
   the circuits, moved by the map's place on the screen (0x56338, 0x5636e) */
static void map_place(int node, int *x, int *y)
{
    static const unsigned open[3][3] = { { 0xf1340, 0xf1ac0, 0xf21a0 },
                                         { 0xefd60, 0xf04e0, 0xf0c10 },
                                         { 0xee3c0, 0xeebe0, 0xef4e8 } };
    static const unsigned loop[6] = { 0xf2a38, 0xf2c38, 0xf3838, 0xf3038, 0xf3c38, 0xf3438 };
    int t = g_pause.track, sg = g_pause.segment, div = t < 3 ? 25 : 6, i = 3 * t + sg;
    unsigned at = (t < 3 ? open[t][sg] : loop[t - 3]) + 4 * (node / 4);

    *x = word(0x56338 + 2 * i) + word(at) / div;
    *y = word(0x5636e + 2 * i) + -word(at + 2) / div;
}

static void map_marker(const char *name, int car)
{
    int x, y;
    map_place(car < g_ncars ? g_cars[car]->node : 0, &x, &y);
    marker(name, x, y);
}

/* the background, the map, the chat dimmed (no network), the racers' markers and the row
   chosen lit */
static void draw(int sel)
{
    static const char *const lit[6] = { "conl", "repl", "audl", "gral", "chal", "quil" };
    static const char *const racer[8] = { "p1  ", "p2  ", "p3  ", "p4  ", "p5  ", "p6  ",
                                          "p7  ", "p8  " };
    int i;

    art(&g_art, "bgnd");
    art(&g_map, "0000");
    art(&g_art, "cham");
    if (g_race_type == 2 || g_race_type == 3) {
        /* everyone, you, and the leader (the last in the race's order) */
        for (i = 0; i < 8; i++)
            map_marker(racer[i], i);
        map_marker("youl", g_player);
        map_marker("lear", g_timing.by_race[7]);
    } else {
        if (g_race_type == 1)
            map_marker(racer[1 - g_player], 1 - g_player);
        map_marker(racer[g_player], g_player);
        map_marker("youl", g_player);
    }
    if (sel < NONE)
        art(&g_art, lit[sel]);
}

/* the mouse put on a row by the keys (0x56320, 0x5632c) */
static void place(int sel)
{
    static const int y[6] = { 120, 170, 217, 265, 311, 360 };
    mouse_place(100, y[sel]);
    fe_mouse_save();
}

static void menu(void)
{
    static const char *const names[9] = { "CY", "CL", "AL", "LOOP1", "LOOP2", "LOOP6",
                                          "LOOP3", "LOOP7", "LOOPX" };
    unsigned char race_pal[768], pal[768];
    char path[256];
    int sel = CONTINUE, changed = 0, redraw = 1, first = 1, done = 0;
    int ready = 1, released = 1, esc_up = 0, k;
    unsigned held = 0;

    /* the settings as before the race (0x565c1), with the detail as the keys left it
       (0x56b48); the race's palette kept, its sounds stopped, the screen faded down and
       640x480 (a 640x480 race keeps its mode, 0x56ce7) */
    mouse_relative(0);                     /* (a mouse steering: the pointer back) */
    ff_forces(0);                          /* (0x56572) */
    memcpy(g_config + 9, g_session.kept, sizeof g_session.kept);
    race_keys_to_config();
    video_get_palette(race_pal, 0, 256);
    rsnd_stop_all();
    pal_fade_down(0, 256);
    if (!g_view_res)
        video_mode(640, 480);
    video_rows(0);                         /* (the port's 640x240 and 640x200: all of it) */
    place(CONTINUE);
    fe_font_load();
    fe_mouse_load();
    fe_sound_bank_load();
    music_wait_end();          /* (0x7211c, 0x72150: the race's music stopped) */
    music_level_from(CFG_MENU_VOLUME);
    music_start("PAUSE.ASF");
    if (g_pause.track < 3)
        snprintf(path, sizeof path, "%s/CHECK/%s%d.QFS", art_dir(), names[g_pause.track],
                 g_pause.segment + 1);
    else
        snprintf(path, sizeof path, "%s/CHECK/%s.QFS", art_dir(), names[g_pause.track]);
    fsh_load(&g_map, path);
    snprintf(path, sizeof path, "%s/CHECK/PAUSE.FSH", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        done = 1;
    }
    fe_mouse_save();

    while (!done) {
        int mx, my;

        /* (0x1f3e8: the network's messages) Esc closes the menu once let go */
        if (!key_down(1))
            esc_up = 1;
        /* the mouse picks a row by its height; between the rows none (the replay and the
           chat in a network race the other way round) */
        mouse_read(&mx, &my);
        k = my >= 0 && my <= 0x8c ? CONTINUE : my >= 0x98 && my <= 0xbe ? REPLAY
            : my >= 0xc5 && my <= 0xec ? AUDIO : my >= 0xf7 && my <= 0x11b ? GRAPHICS
            : my >= 0x153 && my <= 0x1e0 ? QUIT : NONE;
        if (k != sel) {
            sel = k;
            changed = 1;
        }

        if (redraw) {
            draw(sel);
            mouse_show(1);                 /* mouse_draw 0x52288 */
            if (first) {
                fe_mouse_save();
                if (g_mouse_active)
                    fe_mouse_read();
                fe_styles_init();
                pal_fade_up(pal, 0, 256);
                first = changed = 0;
            }
            redraw = 0;
        }
        if (changed) {                     /* the old row dark, the new one lit */
            esc_up = 1;
            if (sel != NONE)
                fe_play_sound(0x47);
            draw(sel);
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
        /* the right button: as Quit chosen */
        if (g_mouse_buttons == 2 && released) {
            sel = QUIT;
            k = 0x0d;
            ready = 1;
        }

        if (k == 0x20 || k == 0x0d || (g_mouse_buttons == 1 && released)) {
            fe_mouse_press();
            if (ready) {
                fe_play_sound(0x48);
                switch (sel) {
                case CONTINUE:
                    done = 1;
                    break;
                case REPLAY:               /* 0x5e4c4(1): the last 600 counts again
                                              (Done: racing on), as Continue */
                    g_replay.mode = 1;
                    ff_stop();             /* (0x5780e) */
                    done = 1;
                    break;
                case AUDIO:                /* fe_audio_options 0x105dc(2) */
                    pal_fade_down(0, 256);
                    fe_audio_options(AUDIO_PAUSE);
                    place(sel);
                    first = redraw = 1;
                    break;
                case GRAPHICS:             /* fe_graphics_options 0x32d08(1) */
                    pal_fade_down(0, 256);
                    fe_graphics_options(GFX_PAUSE);
                    place(sel);
                    first = redraw = 1;
                    break;
                case QUIT:
                    if (fe_yes_no_dialog(g_german ? "Spiel beenden ?" : "End game ?")) {
                        g_pause.quit = 1;  /* (0x13c17c = 2) */
                        done = 1;
                    }
                    redraw = 1;
                    break;
                }
            }
            released = ready = 0;
            held = 0;
            ticksince();
        } else if (k && ready) {
            if (k == 0x1b) {
                if (esc_up) {              /* (once Esc is let go) */
                    while (key_down(1) && !g_quit)
                        video_retrace();
                    done = 1;
                }
            } else if (k == 0x4800) {      /* up, round to Quit, past the chat */
                fe_mouse_hide();
                sel = sel == CONTINUE ? QUIT : sel - 1;
                if (sel == CHAT)
                    sel = GRAPHICS;
                place(sel);
                changed = 1;
            } else if (k == 0x5000) {      /* down, round to Continue, past the chat */
                fe_mouse_hide();
                sel = (sel + 1) % 6;
                if (sel == CHAT)
                    sel = QUIT;
                place(sel);
                changed = 1;
            }
            ready = 0;
            held = 0;
            ticksince();
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1;
    }

    /* back to the race: its screen, its palette as it was, its dashboard drawn again, the
       camera's view set up again, the engine heard again */
    pal_fade_down(0, 256);
    mouse_show(0);
    fsh_free(&g_art);
    fsh_free(&g_map);
    music_fade_out();
    fe_sound_bank_free();
    music_wait_end();
    if (!g_view_res)                       /* (0x580a9) */
        video_mode(320, 200);
    video_rows(g_race_rows);
    video_palette(race_pal, 0, 256);
    memcpy(g_session.kept, g_config + 9, sizeof g_session.kept);   /* (0x5809d) */
    fe_sound_unmute();                     /* (0x58107: F5's silence ends here) */
    inv_build();                           /* (make_inverse_table 0x8227c) */
    hud_redraw_due();
    g_view_redraw = 1;
    /* (0x58358) the forces on again to race on, or every effect stopped */
    if (!g_pause.quit)
        ff_forces(1);
    else
        ff_stop();
    if (!g_pause.quit) {
        rsnd_engine_restart();
        music_level_from(-1);
        race_music_start();    /* (0x63d88: another one chosen) */
    }
    g_pause.pending = 0;
    g_pause.last = g_drive.steps;          /* (0x632f8) */
}

void race_pause_step(race_camera *cam, int done)
{
    static const int drawn[3] = { 0x1c, 0xe, 9 };
    int k;

    /* 0x1fa1c: the calls due */
    for (k = 0; k < 2; k++)
        if (done && g_pause.at[k] && g_pause.at[k] <= done) {
            g_pause.at[k] = 0;
            if (k == 0) {
                unsigned from = clock_ticks();
                menu();
                camera_set_mode(cam, cam->mode);
                race_keys_from_config();
                g_pause.held_ticks += clock_ticks() - from;
            }
            else
                hold(cam);
        }
#ifdef MISTER_HYBRID
    if (mister_osd_open() && g_drive.state != 3)
        hold_osd(cam);
#endif
    /* 0x63304: Esc after the first 120 steps' counts (60 steps), once in 90 steps; P the
       same once in 60, not while the police's ticket is shown (no network races here:
       there Esc with a Shift ends the race) */
    if (g_drive.steps < 0x3c) {
        g_pause.pending = 0;
        g_pause.last = 0;
    }
    if (key_down(K_ESC) && g_drive.state == 1 && g_drive.steps > 0x78 && !g_pause.pending) {
        if (g_pause.last + 0xb4 <= g_drive.steps) {
            g_pause.last = g_drive.steps;
            g_pause.at[0] = done + 1;
            g_pause.pending = 1;
            /* (the road's distance asked again, as F3 left it) */
            if (g_rkeys.road < 3)
                g_rkeys.drawn_next = drawn[g_rkeys.road];
        }
    } else if (key_down(K_P) && g_drive.steps > 0x78 && !g_pause.pending
               && !g_opp.ticket_up && !g_opp.warning_up && g_pause.last + 0x78 <= g_drive.steps) {
        g_pause.last = g_drive.steps;
        g_pause.at[1] = done + 1;
        g_pause.pending = 1;
    }
}
