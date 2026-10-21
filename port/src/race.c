/* A race (NOTES §4.3, §4.7): the loading of race_main 0x6356c and the frame of
   race_draw_frame 0x5bc40, as far as the port has them. */
#include "mister.h"
#include "race.h"
#include "ff.h"
#include "ffdev.h"
#include "replay.h"
#include "opponent.h"
#include "camera.h"
#include "collide.h"
#include "car.h"
#include "cardraw.h"
#include "config.h"
#include "crash.h"
#include "dash.h"
#include "drive.h"
#include "hud.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "fsh.h"
#include "mainmenu.h"
#include "racemenu.h"
#include "racepal.h"
#include "smoke.h"
#include "menu.h"
#include "pause.h"
#include "racekeys.h"
#include "racesnd.h"
#include "sound.h"
#include "sndlib.h"
#include "road.h"
#include "roaddraw.h"
#include "session.h"
#include "skid.h"
#include "sky.h"
#include "smooth.h"
#include "tall.h"
#include "standings.h"
#include "timing.h"
#include "track.h"
#include "trackfam.h"
#include "trackinfo.h"
#include "unpack.h"
#include "video.h"
#include "view.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* at most: two racers, the police car and six of the traffic, or eight racers */
#define RACE_CARS 9
#define MIRROR_SECTIONS 4      /* 0x13661c */

race_codes g_race_codes;

/* the tracks' files (0x36290): the open roads' by their segment, cy1 .. al3 */
static const char *const track_names[9] = { "cy", "cl", "al", "tr1", "tr2", "tr6", "tr3",
                                            "tr7", "tr4" };

/* 0x36c1c: a car as the menus number it (CONFIG.DAT +0x9b, +0x91) to the game's (the
   names 0xf6b10: 0 the Supra .. 7 the RX-7, 8 a mixed field, 11 the Warrior) */
/* RENFS_PERF=1, the port's: every 5 seconds of a race, how long its steps and pictures
   took (at 0 the frame begins, at 2 its steps are done, at 1 it is ready to be shown) */
static void perf(int at, unsigned steps)
{
    static int on = -1;
    static Uint64 t0, begun, work, worst, frames, stepping;
    static unsigned steps0;
    Uint64 now;
    if (on < 0)
        on = getenv("RENFS_PERF") && atoi(getenv("RENFS_PERF"));
    if (!on)
        return;
    now = SDL_GetPerformanceCounter();
    if (!at) {
        if (!t0 || now - t0 > 30 * SDL_GetPerformanceFrequency()) {   /* (a new race) */
            t0 = now;
            work = worst = frames = stepping = 0;
            steps0 = steps;
        }
        begun = now;
        return;
    }
    if (at == 2) {
        stepping += now - begun;
        return;
    }
    work += now - begun;
    worst = now - begun > worst ? now - begun : worst;
    if (++frames, now - t0 >= 5 * SDL_GetPerformanceFrequency()) {
        double f = (double)SDL_GetPerformanceFrequency(), s = (now - t0) / f;
        printf("perf: %dx%d smooth %d: %.1f pictures a second, %.1f steps; each %.1f ms, "
               "the slowest %.1f ms; the steps %.1f ms a picture\n", H(320, 640),
               g_race_rows ? g_race_rows : H(200, 480), g_smooth,
               frames / s, (steps - steps0) / s, work * 1000.0 / f / frames, worst * 1000.0 / f,
               stepping * 1000.0 / f / frames);
        fflush(stdout);
        t0 = now;
        work = worst = frames = stepping = 0;
        steps0 = steps;
    }
}

/* 0x653a4: the race's cars, by their ids (200 + the car the racers, or 100 + the car,
   the far models, for the computer's racers when the settings' +0x87 asks for less
   detail; 400 the police car; below 100 the traffic). A single race: seven of the
   opponent's car, or for the mixed field (8) one each (0xf6df8); a tournament: three
   cars by the player's class, in turn; a head to head: the opponent, the police car and
   six of the twelve traffic cars, drawn from the random numbers (0xeab70) without
   repeats. The count */
static int race_car_ids(int *ids, int type, int player, int opp, int field, int chase,
                        int traffic)
{
    int base = config_word(0x87) & 0xff ? 100 : 200, used[12] = { 0 }, n, i, k;

    ids[0] = player + 200;
    if (type == 2) {
        for (i = 1; i < field; i++)
            ids[i] = (opp == 8 ? *exe_at(0xf6df8 + i, 1) : opp) + base;
        return field;
    }
    if (type == 3) {
        static const int classes[3][3] = { { 0, 7, 6 }, { 4, 1, 4 }, { 5, 3, 2 } };
        int c = player == 1 || player == 4 ? 1 : player == 2 || player == 3 || player == 5 ? 2 : 0;

        for (i = 1; i < 8; i++)
            ids[i] = classes[c][i % 3] + base;
        return 8;
    }
    if (chase)
        ids[2] = 400;
    ids[1] = opp + 200;
    n = field + chase;
    for (i = 0; i < traffic; i++) {
        int tries = 0;

        do
            k = (int)(opponent_random() * 12 >> 16);
        while (used[k] && ++tries < 10000);
        /* (a small RENFS_SEED gives only 0 for long: the clock's seconds never do) */
        for (k = tries < 10000 ? k : 0; used[k]; k++)
            ;
        used[k] = 1;
        ids[n++] = k;
    }
    return n;
}

/* race_timer_tick 0x4c454: the race clock's count after t ticks of the 100 Hz clock, its
   countdown reloaded with a, a and b in turn (race_clock_rate 0x4c5f8: 3 3 3, 33.3 steps
   a second; with TURBO, 0x12da28 & 8, setting 3: 2 2 3, 42.9 a second) */
static int race_clock(unsigned long t)
{
    unsigned a = g_drive.options & 8 ? 2 : 3, per = 2 * a + 3;

    return (int)(t / per * 3 + (t % per >= a) + (t % per >= 2 * a));
}

/* the tick the race clock reaches n at (race_clock's other way round) */
static double race_due(unsigned n)
{
    unsigned a = g_drive.options & 8 ? 2 : 3, per = 2 * a + 3;

    return (double)(n / 3 * per + n % 3 * a);
}

/* the port's smoothing (smooth.c): the clock's count the last step was made at, and the
   counts until the next (more in a replay played slower) */
static unsigned g_step_at, g_step_span = 1;

/* 0x5d690, 0x5ee14: a replay begun while racing (the pause menu's) keeps the race's
   clock and its steps done; racing on after it they are as they were (the time watched
   does not count), the dashboard drawn again and the view's window set up again (0x2d278,
   0x14260) */
static void replay_clocks(unsigned *done, unsigned long start, race_camera *cam)
{
    static unsigned kept_done;
    static unsigned long kept_ticks;

    if (g_replay.clock_kept[0] == 1) {
        g_replay.clock_kept[0] = 2;
        kept_done = *done;
        kept_ticks = clock_ticks();
    }
    if (g_replay.back) {
        g_replay.back = 0;
        g_replay.clock_kept[0] = 0;
        *done = kept_done;
        g_pause.held_ticks += clock_ticks() - kept_ticks;
        g_rkeys.race_clock = race_clock(clock_ticks() - start - g_pause.held_ticks);
        hud_redraw_due();
        if (cam->ahead->current > -1)
            view_layout(cam->ahead, cam->ahead->current);
        g_view_redraw = 1;
    }
}

/* load_slide 0x64d28: at 640x480 the player's car in one of its seven pictures
   (SIMDATA/SLIDES/<car>S<n>.QFS, "SCRN" with its palette, n from EA's random numbers),
   left on the screen while the race loads */
int g_race_slide = -1;

static void race_slide(int car)
{
    char path[80];
    fsh_file f;
    shape sh;
    unsigned char pal[768];
    const char *name = (const char *)exe_at((unsigned)exe_i32(0xf6b10 + 4 * (200 + car)), 1);
    int i;

    video_mode(640, 480);
    g_race_slide = collide_below(7);
    snprintf(path, sizeof path, "SIMDATA/%s/%sS%d.QFS", g_german ? "GSLIDES" : "SLIDES", name,
             g_race_slide);
    if (!fsh_load(&f, path))
        return;
    if (fsh_find(&f, "SCRN", &sh) && fsh_palette(&f, &sh, pal)) {
        video_palette(pal, 0, 256);
        fe_draw_shape(&sh, 0);
        /* (the loading in the original takes seconds: here it is shown for one) */
        for (i = 0; i < 60 && !g_quit; i++)
            video_retrace();
    }
    fsh_free(&f);
}

/* (the port's) the MiSTer's HUD (Bottom): its lines of a 240-line screen as the screen's
   rows; none while 200 rows are shown (RENFS_HUDBOTTOM=n for a test) */
static int hud_bottom_rows(void)
{
    int lines = 0, shown = g_race_rows ? g_race_rows : H(200, 480);
#ifdef MISTER_HYBRID
    if (mister_on())
        lines = mister_hud_bottom();
#endif
    if (getenv("RENFS_HUDBOTTOM"))
        lines = atoi(getenv("RENFS_HUDBOTTOM"));
    lines = lines < 0 ? 0 : lines > 24 ? 24 : lines;
    return shown >= 240 ? lines * H(200, 480) / 240 : 0;
}

static int quarter(int node)
{
    return node >= 0 ? node / 4 : -(-node / 4);
}

/* the mirror's view into its picture (0x5be3e ..): the road behind and its cars, turned
   round left for right */
static void draw_mirror(race_camera *cam, const int *m)
{
    const int *mm = view_select(cam, 1);
    const view_window *w = g_view_now->win;
    road_window win = { w->x, w->y, w->cw, w->ch };
    int near;

    g_camera_m = mm;
    near = road_transform_behind(&cam->pos, mm);
    raster_window(g_hud_mirror.pix, g_hud_mirror.w, 0, 0, g_hud_mirror.w, g_hud_mirror.h);
    sky_behind(cam->yaw, cam->pitch, cam->node, g_view_now->lens_h, w->dy, g_hud_mirror.w,
               g_hud_mirror.h);
    road_draw_behind(MIRROR_SECTIONS, cam->node, near, &win);
    hud_mirror_flip();
    view_select(cam, 0);
    g_camera_m = m;
}

static int S32(const unsigned char *p, int at)
{
    return (int)(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | (unsigned)p[at + 3] << 24);
}

/* RENFS_THROW="<step>:<x>,<y>,<z>": a test, the player's car given that speed in metres a
   second (east, up, north) at that step, as drive_emu's "thrown" runs do, to see a crash */
static void race_throw(race_car *player, int st)
{
    const char *e = getenv("RENFS_THROW");
    double x, y, z;
    int at;

    if (e && sscanf(e, "%d:%lf,%lf,%lf", &at, &x, &y, &z) == 4 && st == at * 2) {
        player->vel.x = (int)(x * 65536);
        player->vel.y = (int)(y * 65536);
        player->vel.z = (int)(z * 65536);
    }
}

/* RENFS_PLACE="<step>:<along>,<across>,<speed>": a test, the opponent put that many
   metres along and across the road from the player's car at that step, driving at that
   speed (m/s), as opponent_emu's placed runs do, to see the cars meet */
static void race_place(const race_car *player, int st)
{
    const char *e = getenv("RENFS_PLACE");
    race_car *o = g_ncars > 1 ? g_cars[1] : 0;
    double along, across, speed;
    int at;

    if (!o || !e || sscanf(e, "%d:%lf,%lf,%lf", &at, &along, &across, &speed) != 4 || st != at * 2)
        return;
    o->pos.x = player->pos.x + (int)(along * player->road_fwd.x) + (int)(across * player->road_side.x);
    o->pos.y = player->pos.y + (int)(along * player->road_fwd.y) + (int)(across * player->road_side.y);
    o->pos.z = player->pos.z + (int)(along * player->road_fwd.z) + (int)(across * player->road_side.z);
    o->yaw = player->yaw;
    o->node = player->node;
    o->road_side = player->road_side;
    o->road_up = player->road_up;
    o->road_fwd = player->road_fwd;
    o->road_at = player->road_at;
    o->road_off = o->across_to = player->road_off + (int)(across * 65536);
    o->road_speed = o->speed = o->speed_to = (int)(speed * 65536);
}

/* a race step for the player's car (race_step 0x6348c, race_update_cars 0x519d8): the
   flags from the step counter, the keyboard's word (controls_read 0x49248: both gear
   keys at once hold the engine at full revs), every second step the controls applied,
   then the driving; the counter on by 2 */
static void race_step(race_car *player, race_camera *cam, int done)
{
    int st = g_drive.steps, i;
    /* (0x48bec) given up: straight on, no pedals, the handbrake */
    unsigned w = g_drive.gave_up[g_player] ? 0x87a0
               : g_drive.state == 1 ? drive_keyboard(g_road_mirror) : 0;

    smooth_keep(cam);
    g_step_at = (unsigned)done;
    g_step_span = g_drive.state == 2 && g_replay.speed > 0 && g_replay.speed < 4
                  && g_replay.wait > 1 ? (unsigned)g_replay.wait : 1;
    g_drive.flags = (st & 1 ? 0 : 1) | (st & 3 ? 0 : 4) | (st & 7 ? 0 : 0x10);
    /* (the keyboard read for the recording: played back, the controls are the ring's) */
    if (g_drive.state == 1) {
        g_drive.revving[g_player] = (w & 0x3000) == 0x3000;
        if (g_drive.revving[g_player])
            w &= 0xcfff;
        g_drive.controls[g_player] = w;
    }
    /* NITRO's keys: N with a shift or B */
    g_drive.nitro_keys = key_down(0x31) && (key_down(0x2a) || key_down(0x36) || key_down(0x30));
#ifdef MISTER_HYBRID
    g_drive.nitro_keys |= mister_nitro_held();     /* (the MiSTer's: R3) */
#endif
    g_drive.clock = (int)clock_ticks();
    /* race_step 0x6348c: the calls due and the pause's keys, then the race's other keys */
    race_pause_step(cam, done);
    race_keys(cam);
    race_throw(player, st);
    race_place(player, st);
    /* race_update_cars 0x519d8: the replay first (once the race has begun), then the
       first 120 counts nothing pressed; while playing held or running back the cars only
       kept where they are drawn */
    replay_step();
    if (g_drive.steps < 0x78)
        for (i = 0; i < g_humans; i++)
            g_drive.controls[i] = 0x7a0;
    if (g_drive.state == 4 || (g_drive.state == 2 && g_replay.speed == 0 && g_replay.hold < 1)) {
        for (i = 0; i < g_ncars; i++) {
            race_car *c = g_cars[i];

            c->draw_node = c->mirror_node = c->node;
            c->yaw_drawn = c->yaw;
        }
        return;
    }
    st = g_drive.steps;        /* (where the replay put the race) */
    if (g_drive.flags & 4)
        drive_controls(player);
    /* (race_update_cars 0x519d8) the finish, the driving, the cars sorted round the
       world's checks, the splits */
    timing_step();
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]];

        if ((c->part & 4) && c->state)
            drive_step(c);
    }
    timing_sort();
    crash_world();
    timing_sort();
    timing_splits();
    /* the camera: outside while the car watched is upside down or turned across the road
       (0x6324c), then moved (0x136b0) */
    camera_check(cam, g_cars[g_drive.viewed]);
    camera_update(cam);
    /* the sounds, heard from where the camera now is (the voices that played to their
       end freed first, as the mixer frees them) */
    snd_time(clock_ticks());
    /* (0x51f22) the force feedback's road shake and steady push, the player's car */
    if (g_drive.state != 3)
        ff_step(g_cars[g_player]);
    rsnd_step();
    /* the cars to draw (0x51fa3): each one's heading as drawn its heading now (its pitch
       and roll too, read by nothing); those turned by their own matrix take it from that
       as they are drawn */
    for (i = 0; i < g_ncars; i++) {
        race_car *c = g_cars[g_timing.by_road[i]];

        if ((c->part & 4) && c->state)
            c->yaw_drawn = c->yaw;
    }
    if (g_replay.hold > 0)
        g_replay.hold--;
    g_drive.steps = st + 2;
}

int race_run(void)
{
    static race_car cars[RACE_CARS];
    race_car *list[RACE_CARS];
    unsigned char marks[RACE_CARS] = { 0 };
    int ids[RACE_CARS];
    race_camera cam;
    road_window win;
    const view_window *w;
    int i, k, eye, horizon, redline, near, ncars, type, field, chase, traffic, player, opp;
    int grid[8] = { 7, 6, 2, 3, 4, 5, 1, 0 };
    char name[8];
    hud_car driver;
    hud_map_state map;
    unsigned start, now;
    int mirror_skip = 0, redraw, in_seat = 1, seat, settle = 0, family, fam_kind;
    unsigned done = 0;
#ifdef MISTER_HYBRID
    unsigned drawn = ~0u;      /* the step the last picture was drawn at */
#endif
    const char *car;

    /* race_setup_record 0x36c5c: the race as the settings ask: the track (0x12da12,
       CONFIG.DAT +0x95) and segment (0x12da0e, +0x93), the race type (0x12da20, +0x6d),
       the racers (0x12da2c: 1 in a time trial, 2 head to head, else 8), on the open roads
       head to head the police car and six of the traffic (0x12da34, 0x12da3c) unless "No
       mercy" (the codes' 4), which also stops the leader waiting (0x12dd10) */
    memset(&g_drive, 0, sizeof g_drive);
    g_drive.state = 1;
    replay_setup_record();
    /* (race_run 0x3724f) at 640x480 when the graphics screen's first row says so (+0x7f
       0; 0x12c308, 0x143e2c by 0x5b984), else 320x200; RENFS_RES=320 or 640 for a test */
    g_view_res = config_word(0x7f) == 0;
    g_race_rows = 0;
#ifdef MISTER_HYBRID
    if (mister_on()) {         /* (the MiSTer's: the OSD's Resolution, the settings follow) */
        const int r = mister_resolution();
        g_view_res = r == MR_640x480 || r == MR_640x240 || r == MR_640x200;
        g_race_rows = r == MR_640x240 || r == MR_320x240 ? 240 : r == MR_640x200 ? 200 : 0;
        config_set_word(0x7f, !g_view_res);
    }
#endif
    if (getenv("RENFS_RES")) {
        const char *r = getenv("RENFS_RES");
        g_view_res = atoi(r) == 640;
        g_race_rows = strchr(r, 'x') ? atoi(strchr(r, 'x') + 1) : 0;
        g_race_rows = g_race_rows == 240 || (g_race_rows == 200 && g_view_res) ? g_race_rows : 0;
    }
    g_view_tall = !g_view_res && g_race_rows == 240;
    /* (0x5e4b8, 0x5e4c4) a race, or a replay (CONFIG.DAT +0xaf: 1 the whole of
       REPLAY.RPL, 2 its highlights, 3 the attract mode's): the race record from the file
       (0x5c340), the rest of it read at the first step */
    g_replay.from_menu = 0;
    g_replay.mode = 0;
    g_replay.end = 0;
    if (config_word(0xaf)) {
        g_replay.mode = config_word(0xaf) == 1 ? 4 : config_word(0xaf) == 2 ? 5 : 8;
        g_replay.from_menu = 1;
        if (!replay_settings())
            return 0x20;
    }
    g_drive.kind = rec_get(REC_TRACK);
    if (g_drive.kind < 0 || g_drive.kind > 8)
        g_drive.kind = 0;
    g_drive.segment = g_drive.kind < 3 ? rec_get(REC_SEGMENT) : 0;
    if (g_drive.segment < 0 || g_drive.segment > 2)
        g_drive.segment = 0;
    type = rec_get(REC_TYPE);
    if (type < 0 || type > 3)
        type = 1;
    field = rec_get(REC_RACERS);
    if (field != 1 && field != 2 && field != 8)
        field = type == 0 ? 1 : type == 1 ? 2 : 8;
    chase = rec_get(REC_POLICE) != 0;
    traffic = rec_get(REC_TRAFFIC);
    if (traffic < 0 || traffic > 6)
        traffic = 0;
    player = rec_get(REC_ENTRY + ENT_CAR);
    opp = rec_get(REC_ENTRY + REC_ENTRY_SIZE + ENT_CAR);
    g_drive.catch_up = rec_get(REC_CATCH_UP);
    g_drive.options = rec_get(REC_FLAGS);
    if (!g_replay.mode) {
        rec_put(REC_RALLY, g_race_codes.rally);
        rec_put(REC_MIRROR, g_race_codes.mirror);
    }
    g_drive.rally = rec_get(REC_RALLY);
    g_drive.nitro = g_race_codes.nitro;
    g_opp.explode = g_race_codes.explode;
    g_road_mirror = rec_get(REC_MIRROR);
    /* the track's files: an open road's by its segment, Rusty Springs' rally tr5 */
    if (g_drive.kind < 3)
        snprintf(name, sizeof name, "%s%d", track_names[g_drive.kind], g_drive.segment + 1);
    else
        snprintf(name, sizeof name, "%s", g_drive.rally && g_drive.kind == 3 ? "tr5"
                                          : track_names[g_drive.kind]);
    snprintf((char *)g_rec + REC_STEM, 10, "%s", name);
    /* race_run 0x37228: EA's random numbers and the race's own from the clock's
       seconds (0x91d9b, kept in the record: a replay's are the race's), then the cars
       chosen (0x653a4) and their grid places (0x370bc: in a single race or a
       tournament's first segment 7 6 2 3 4 5 1 0) */
    if (!g_replay.mode)
        rec_put(REC_TIME, getenv("RENFS_SEED") ? (int)strtoul(getenv("RENFS_SEED"), 0, 0)
                                                : (int)time(0));
    now = (unsigned)rec_get(REC_TIME);
    collide_seed(now);
    opponent_seed(now);
    ncars = race_car_ids(ids, type, player, opp, field, chase, traffic);

    /* race_main 0x6356c: the loading picture, then the loading */
    race_slide(player);
    /* the family (0x12c340, race_run 0x374ed): the mirrored and the rally's 1, else the
       track's time of day chosen on its screen (0x12dd30) */
    family = g_road_mirror || g_drive.rally ? 1 : rec_get(REC_TOD + 4 * g_drive.kind);
    fam_kind = g_road_mirror ? 2 : g_drive.rally && g_drive.kind != 3 ? 3 : 0;
    if (!track_load(name) || !trackinfo_load(name, family, g_view_res)
        || !trackfam_load(name, family, fam_kind)) {
        fprintf(stderr, "renfs: the track %s could not be loaded\n", name);
        return 0;
    }
    g_model_wide = g_drive.kind == 0 && g_drive.segment == 2;
    /* the laps: an open road's one; a circuit's as Control Central set them (+0xb1,
       0x12dd14) (or RENFS_LAPS for a test) */
    g_drive.laps = 1;
    if (g_track.loop) {
        if (getenv("RENFS_LAPS") && !g_replay.mode)
            rec_put(REC_LAPS, atoi(getenv("RENFS_LAPS")));
        g_drive.laps = rec_get(REC_LAPS);
        if (g_drive.laps < 1 || g_drive.laps > DRIVE_LAPS - 1)
            g_drive.laps = 1;
    }
    /* where the race ends (race_main before 0x506dc): a circuit's laps, an open road's
       last section less 45 nodes */
    g_drive.last_node = g_track.loop ? g_track.length * 4
        : (short)(g_track.head[6] | g_track.head[7] << 8) * 4 - 0xb5;
    snprintf(g_drive.track, sizeof g_drive.track, "%s", name);
    /* the best lap to beat: the track's lap record (load_dash 0x2e8d4) */
    g_drive.best_lap = S32(g_config, 0xd0 + g_drive.kind * 0xa6b);
    g_drive.chase = chase;     /* (0x12dd08, 0x12da34) */
    g_drive.police = chase;
    memset(&g_crash, 0, sizeof g_crash);
    if (!smoke_load(&g_crash.smoke))       /* (after cars_create 0x51388) */
        return 0;
    crash_start();
    drive_keys_from_config();      /* (race_run: 0x36a0c) */
    g_view.min_z = exe_i32(0xf7938);
    g_view.far_shift = *exe_at(0xf6aa4, 1);
    g_view.max_level = exe_i32(0xeaad8);

    /* the cars and the palette, in race_main's order */
    for (i = 0; i < ncars; i++) {
        memset(&cars[i], 0, sizeof cars[i]);
        cars[i].player = i;
        cars[i].police = ids[i] >= 400;
        if (!car_load(&cars[i], ids[i]))
            return 0;
        list[i] = &cars[i];
    }
    /* the player's car ready to drive (cars_create 0x51388: load_car_pbs, car_reset),
       with the options' gearbox, traction control and ABS */
    cars[0].slot = 0;
    if (!drive_load(&cars[0], rec_get(REC_ENTRY + ENT_AUTO), rec_get(REC_ENTRY + ENT_TCS),
                    rec_get(REC_ENTRY + ENT_ABS)))
        return 0;
    /* (race_run) the controls' kinds from the entry: a replay's those it was driven with */
    g_drive.steer_kind[0] = rec_get(REC_ENTRY + ENT_STEER);
    g_drive.pedal_kind[0] = rec_get(REC_ENTRY + ENT_PEDALS);
    g_drive.gear_kind[0] = rec_get(REC_ENTRY + ENT_PEDALS + 4);
    /* the computer's cars (car_setup 0x51200; their size the PDN's or the model's,
       0x512e8, as car_load left it): driven by the computer (kind 3), a metre along
       until their first step: the opponent's puts it on the road from the grid, the
       traffic's sends each car off the road to wait (kind 6), the police car's (car 2)
       puts it behind the player's, off the race (part without 4) until it comes out
       ahead (0x79500) */
    for (i = 1; i < ncars; i++) {
        cars[i].slot = i;
        cars[i].state = 3;
        cars[i].pos.z = 0x10000;
        cars[i].lap = 1;
        if (i < field)
            cars[i].bits[0] |= 4;
    }
    if (chase)
        cars[field].bits[0] |= 8;
    eye = S32(cars[0].spec, 0x36c);         /* the driver's eyes above the road */
    horizon = S32(cars[0].spec, 0x370);     /* where the horizon is in the view */
    redline = S32(cars[0].spec, 0xac);      /* the engine's red line */
    cars_queue_colours(list, ncars, marks);
    car = (const char *)exe_at((unsigned)exe_i32(0xf6b10 + 4 * (200 + player)), 1);
    if (!dash_load(car, cars[0].number, g_view_res))
        return 0;
    dash_queue_colours();
    if (!hud_load())
        return 0;
    /* (0x5ba1c: at 640x480 the menus' mode kept) */
    video_mode(H(320, 640), H(200, 480));
    /* (the MiSTer's 640x240 and 640x200 of the 640x480 picture, its 320x240 of the 320x200
       one with the view at 240 rows) */
    video_rows(g_race_rows);
    if (g_view_tall && !tall_on(1))
        return 0;
    pal_apply_pieces();
    race_colours(marks, ncars, type);
    needle_colours(cars[0].number);     /* (0x8a390, in race_colours) */
    /* (race_colours 0x2e74c goes on with the dashboard's pictures, then tints its palette)
       then the cars' pictures; in these races one racer against the player with the police
       on (0x12da30 = 1, 0x12da34), the chase of 0x6577c */
    pal_remap(g_dash.file.d, g_dash.pal);
    pal_tint(0, 0x100, g_dash.pal);
    cars_remap(list, ncars, marks, chase);
    hud_colours();             /* 0x55abc: the HUD's colours, then the shade table */
    shade_make(0x9999);
    pal_apply_remaps();
    smoke_table();             /* (0x55fa8's end) */

    /* the views and the camera in the driver's seat; the racers on the grid, the player
       in slot 7 (0x12dca0) */
    view_init(g_view_res, cars[0].number, horizon);
    camera_init(eye);
    view_setup(&g_view_ahead, g_view_ahead.mode);
    view_setup(&g_view_mirror, g_view_mirror.mode);
    view_setup(&g_view_ahead, g_view_ahead.mode);
    memset(&g_pause, 0, sizeof g_pause);
    g_pause.track = g_drive.kind;      /* (the pause menu's map) */
    g_pause.segment = g_drive.segment;
    race_keys_start();         /* (the settings' detail, 0x5ba1c's 0x1440c) */
    /* (the opponent keeps the grid's node until its first step: the road's axes it
       drives by are worked out as that step moves it to the nearest, drive_opponent) */
    for (i = 0; i < 8; i++) {
        if (g_replay.mode)
            grid[i] = rec_get(REC_GRID + 4 * i);
        else
            rec_put(REC_GRID + 4 * i, grid[i]);
    }
    for (i = 0; i < field; i++)
        car_grid_place(&cars[i], grid[i], field, g_drive.kind);
    track_follow(&cars[0].pos, &cars[0].node);
    memset(&cam, 0, sizeof cam);
    camera_set_car(&cam, &cars[0], 0);

    /* the cars: the racers standing on the grid (their turn the one the grid gives,
       +0x351), the traffic and the police after them */
    for (i = 0; i < ncars; i++) {
        cars[i].slot = i;
        cars[i].yaw_drawn = cars[i].yaw;
        cars[i].routine = i ? 2 : 1;
        g_cars[i] = &cars[i];
    }
    rec_put(REC_CARS, ncars);
    rec_put(REC_HUMANS, 1);
    g_ncars = ncars;
    g_player = 0;
    if (getenv("RENFS_VIEW"))  /* a test: the race watched from camera mode n */
        camera_set_car(&cam, &cars[0], atoi(getenv("RENFS_VIEW")));
    g_racers = field;
    g_race_type = type;
    g_field = field;
    g_humans = 1;
    memcpy(g_car_marks, marks, RACE_CARS);
    for (i = 0; i < ncars; i++)
        cars[i].part = 7;      /* (+0x51a: driven, their finish checked, meeting the others) */
    /* the race's timing (race_start 0x506dc): the settings' laps (CONFIG.DAT +0xb1,
       0x12dd14, as Control Central left them), the race length's times (+0xb3, 0x12dd24) */
    g_timing.lap_limit = g_drive.laps;
    g_timing.table = rec_get(REC_TABLE) < 0 || rec_get(REC_TABLE) > 2 ? 1 : rec_get(REC_TABLE);
    timing_cars();
    timing_start();
    /* the computer's driving (load_traffic_cfg 0x7de84): the drivers by the grid places,
       the random numbers from the clock (0x38c10 with time(), 0x91d9b) */
    for (i = 0; i < field; i++)
        g_opp.grid[i] = grid[i];
    g_opp.traffic = traffic;                   /* (0x12da3c: the cars after the police) */
    g_opp.rival_car = opp;                     /* (0x12da9e: the opponent's record) */
    /* the sounds (race_main 0x6363b, before the random numbers are seeded): the settings'
       volumes and stereo (0x12da53's record; none without a sound card, 0x529f4), the
       banks (0x52774), the engine started silent (0x52670) and heard (0x53214); voice 0
       held for the race's music */
    rsnd_init();
    g_rsnd.present = !getenv("RENFS_NOSOUND");
    g_rsnd.sfx = g_rsnd.present ? config_word(CFG_SFX_VOLUME) : 0;
    g_rsnd.music = g_rsnd.present ? config_word(0x75) : 0;
    g_rsnd.mono = config_word(0x71) == 2;
    snd_init(g_rsnd.present);
    if (g_rsnd.sfx && g_rsnd.music)
        snd_hold(0, 101, g_rsnd.music);
    snd_time(clock_ticks());
    rsnd_load(cars[0].number, opp, g_drive.kind);
    rsnd_engine_on();
    opponent_start();
    g_camera = &cam;
    g_road_paint.cars = cars_at_node;
    g_road_paint.cars_behind = cars_behind_at;
    g_road_paint.car_row = skid_paint_row;
    skid_reset();              /* (0x650c8) */

    /* the dashboard as the race starts: the status bar still black (its first 360
       steps), standing with the engine idling (+0x3c1 700 at the start) */
    memset(&g_hud, 0, sizeof g_hud);
    g_hud.track = g_drive.kind;
    g_hud.table = g_timing.table;
    g_hud.shown = 1;
    g_hud.race_type = g_race_type;
    g_hud.running = 1;
    g_hud.map = 1;             /* (0x12b988: on as a race starts) */
    g_hud.laps = g_drive.laps;
    g_hud.lap_limit = g_timing.lap_limit;
    g_hud.finish_node = g_drive.last_node;
    g_hud.circuit = g_track.loop;
    g_hud.segment = g_drive.segment;
    /* (kilometres an hour: the settings' 0x15d140, not read yet; miles) */
    memset(&map, 0, sizeof map);
    map.place = 1;
    for (i = 0; i < field; i++)
        map.places[i] = (unsigned char)(i + 1);
    map.driver = 1;
    memset(&driver, 0, sizeof driver);
    driver.steer = 0;
    driver.number = cars[0].number;
    driver.rpm = 700;
    driver.redline = redline;
    driver.speed = 0;
    driver.viewed = 0;
    driver.human = 1;
    driver.player = 0;
    hud_redraw_due();
    /* the race's music, the loading's last (race_main 0x6356c: 0x63d88, with 0xf749a) */
    music_level_from(-1);      /* (heard at the stream's own volume) */
    race_music_start();
    /* (0x63628) the force feedback's forces on, every effect stopped (a pad looked for
       again: one plugged in since is felt too) */
    ff_open();
    ff_forces(1);
    ff_stop();
    start = clock_ticks();
    /* (race_main 0x6368a, 0x639b0) the recording ready, its first step a snapshot */
    replay_start();
    replay_race_begins();
    g_replay.cam = &cam;
    smooth_reset();
#ifdef MISTER_HYBRID
    mister_racing(1);
#endif

    while (!g_quit && !g_pause.quit && !g_timing.result && !g_replay.end) {
        const int *m;

        perf(0, done);

        /* the race's steps: one each three ticks of the clock (the race clock's
           three-cycle, race_timer_tick 0x4c454), the player's car driven */
        g_rkeys.race_clock = race_clock(clock_ticks() - start - g_pause.held_ticks);
        while (done < (unsigned)g_rkeys.race_clock && !g_replay.end) {
            /* (race_main 0x6356c) a replay played slower: a step each wait + 1 due */
            if (g_drive.state == 2 && g_replay.speed > 0 && g_replay.speed < 4) {
                if (g_bar.wait_left < 1) {
                    g_bar.wait_left = g_replay.wait;
                    race_step(&cars[0], &cam, done);
                    replay_clocks(&done, start, &cam);
                }
                g_bar.wait_left--;
                done++;
                continue;
            }
            if (g_drive.state == 2)
                g_bar.wait_left = 0;
            race_step(&cars[0], &cam, done);
            replay_clocks(&done, start, &cam);
            done++;
            /* running fast to a point of the replay: no frames between */
            while (g_drive.state == 3 && !g_replay.end && !g_quit) {
                race_step(&cars[0], &cam, done);
                replay_clocks(&done, start, &cam);
            }
        }
        perf(2, done);
#ifdef MISTER_HYBRID
        /* the port's, on the MiSTer: without smooth motion a picture only for a new step,
           the one before shown again meanwhile (a 640x480 picture takes longer than a
           field there; the original, too, draws nothing new between steps) */
        if (!g_smooth && done == drawn && !g_view_redraw
            && (g_drive.state == 1 || (g_drive.state == 2 && g_replay.speed == 4))) {
            ffdev_tick();
            video_retrace();
            continue;
        }
        drawn = done;
#endif
        /* the port's: this frame drawn between the last two steps, by the time since
           the last (smooth.c; F11) */
        smooth_keys();
        smooth_apply(&cam, (int)((clock_fine() - start - g_pause.held_ticks
                                  - race_due(g_step_at + 1)) * 65536.0
                                 / (race_due(g_step_at + 1 + g_step_span)
                                    - race_due(g_step_at + 1))));
        /* the dials the player's car's, the status bar's numbers the car watched's
           (Alt+Ctrl and a number) */
        driver.steer = smooth_steer(cars[0].steer);
        driver.rpm = smooth_rpm(cars[0].rpm);
        driver.lever_shown = cars[0].gear_shown;
        driver.lever_before = cars[0].gear_before;
        driver.lever_gear = cars[0].gear;
        driver.lever_wait = cars[0].shift_wait;
        {
            const race_car *v = g_cars[g_drive.viewed];

            driver.viewed = g_drive.viewed;
            driver.human = g_drive.viewed < g_humans;
            driver.player = v->player;
            driver.speed = v->speed;
            driver.road_speed = v->road_speed;
            driver.gear = v->gear;
            driver.lap = v->lap;
            driver.node = v->node;
        }

        /* the road's points (0x50070): its sections kept, gathered again when the
           camera is on another quarter (or an object was knocked over) */
        /* (race_draw_frame 0x5bcf5: the road drawn as far as F3 asked, gathered again) */
        if (race_keys_frame())
            road_refresh();
        g_hud.detail = g_rkeys.window == 3 ? 3 : g_view_detail;
        track_keep_sections(quarter(cam.node));
        if (g_track.last_node < 0 || quarter(g_track.last_node) != quarter(cam.node))
            road_gather(cam.node, quarter(cam.node), g_rkeys.drawn);
        /* (the road behind for the mirror: 0x50070's end, when the camera's node changed
           or an object was knocked over: 0x5004c sets the camera's flag) */
        if (cam.flags || g_track.last_kinds < 0)
            road_gather_behind(cam.node);
        cam.flags = 0;

        cars_list();           /* (the race's step does this) */

        m = view_select(&cam, 0);
        /* the view changed (0x5bdaa): set up again with its 1:1 window for this frame,
           and with its detail's at the next (0x5bd71); this also makes the projection's
           table the view ahead's again, whoever's view was set up last */
        if (settle) {
            int keep = g_view_redraw;
            view_set_mode(cam.ahead, cam.ahead->current, g_view_detail);
            g_view_redraw = keep;
            settle = 0;
        }
        if (g_view_redraw) {
            view_set_mode(cam.ahead, cam.ahead->current, 0);
            settle = 1;
        }
        view_matrix(&cam);
        g_camera_m = m;
        skid_rows(cam.node);
        cars_seen(cam.node);
        w = g_view_now->win;
        near = road_transform(&cam.pos, m, cam.node);
        /* the mirror (race_draw_frame 0x5be17), every other frame with the settings'
           +0x87, shown each frame while the camera has one */
        g_hud.mirror = cam.mirror_on;
        if (cam.mirror_on && (!config_word(0x87) || (mirror_skip ^= 1)))
            draw_mirror(&cam, m);
        win.x = w->x;
        win.y = w->y;
        win.w = w->cw;
        win.h = w->ch;
        /* the view changed: the screen round it into the view's picture first (0x3081c,
           whatever the view; from the driver's seat the dashboard covers it) */
        redraw = g_view_redraw;
        g_view_redraw = 0;
        if (redraw)
            hud_outside_screen(cam.ahead->box);
        /* the view painted off the screen (0xf4578), then the dashboard puts it on the
           screen through its windows (0x31684), or from outside the HUD's lists (0x314e4) */
        tall_frame();
        if (w->tall)               /* (the port's 320x240: the view at 240 rows) */
            raster_window(tall_picture(), TALL_W, win.x, win.y, win.x + win.w, win.y + win.h);
        else
            raster_window(g_hud_view.pix, g_hud_view.w, win.x, win.y, win.x + win.w, win.y + win.h);
        /* (the port's) with fewer rows shown, only those painted; not when the lists
           double the view's rows */
        if (g_hud.detail != 2)
            raster_rows(video_rows_shown());
        sky_draw(cam.yaw, cam.roll, &cam.pos, m, w, &win, g_rkeys.plain_sky);
        g_car_ticket = g_opp.ticket_up || g_opp.warning_up;
        g_road_steps = g_drive.steps;
        g_cars_held = g_drive.state == 4;
        road_objects_start();
        road_draw(g_rkeys.drawn, cam.node, near, &win);
        if (w->tall)
            tall_view_done(w, view_window_lo(w), g_hud_view.pix);
        g_hud.steps = g_drive.steps;
        g_hud.ahead = g_timing.split;
        g_hud.splits = g_timing.split_shown;
        g_hud.best_lap = g_drive.best_lap;
        map.node = (short)g_cars[g_drive.viewed]->node;
        for (i = 0; i < field; i++)
            map.nodes[i] = (short)cars[i].node;
        map.place = g_timing.place_shown;   /* (the last frame's, as the map comes first) */
        memcpy(map.places, g_timing.places, sizeof map.places);
        map.driver = cam.mode == 0 && !g_hud.no_dash;
        g_hud.cam_mode = cam.mode;
        g_hud.running = g_drive.state == 1;
        g_hud.stopped = g_timing.stopped;
        hud_map(&map);
        k = hud_ticket(g_opp.ticket_up, g_opp.warning_up, g_timing.result);
        /* (the seat this frame is drawn from: F9 inside the dashboard's frame changes it
           for the next) */
        seat = cam.mode == 0 && !g_hud.no_dash;
        /* detail 3, every other line: the screen's view blacked again after a change
           (0x5bfef); from outside, this frame's 1:1 view gets its odd lines black in the
           view's window (0x5c053) as the lists leave them out later */
        if (redraw && g_hud.detail == 3) {
            g_hud.clear = 1;
            if (!seat)
                for (i = win.y | 1; i < win.y + win.h; i += 2)
                    memset(g_hud_view.pix + (long)i * g_hud_view.w + win.x, g_marker_line,
                           (size_t)win.w);
        }
        if (k == 1)
            ;                  /* the police's ticket or warning instead */
        else if (seat) {
            /* back in the driver's seat: the whole dashboard again (0x143e68, 0x2d284) */
            if (!in_seat)
                hud_redraw_due();
            hud_frame(&driver);
        } else
            hud_outside_frame(&driver, redraw, cam.ahead->current);
        in_seat = seat;
        hud_lift(hud_bottom_rows());
        smooth_restore(&cam);
        /* F9 (0x2f644): the driver's view with the dashboard or without, mode 0's view
           (0xe80f0) and the camera's mode taken up again, the screen round it redrawn */
        if (g_hud.view_changed) {
            g_hud.view_changed = 0;
            g_cam_modes[0].kind = g_hud.no_dash;
            camera_set_mode(&cam, cam.mode);
            g_view_redraw = 999;
        }
        /* (after the ticket, the screen round the view drawn again: 0xeaaa8) */
        if (k == 2)
            g_view_redraw = 1;
        ffdev_tick();
        perf(1, done);
        video_retrace();
    }
    ff_stop();                 /* (0x63b61) */
    ff_forces(0);
#ifdef MISTER_HYBRID
    mister_racing(0);
#endif

    mouse_show(0);             /* (the replay's pointer: the films and menus after show
                                  their own) */
    mouse_relative(0);         /* (a mouse steering: the pointer back) */
    replay_end();              /* (0x5de88: REPLAY.RPL, not after a replay) */
    rsnd_free();               /* (0x52a70) */
    /* race_run 0x37228 after race_main: the detail settings as the keys left them back
       into the settings and their copy (0x144cc: the outside view's size; on 320x200
       the view's pixels at most doubled across), how the race ended, the results
       (0x68ab0), the music faded out */
    {
        int d[5], at;

        d[0] = g_view_outside.current - g_view_outside.layout;
        d[1] = g_rkeys.window > 1 && !g_view_res ? 1 : g_rkeys.window;
        d[2] = g_rkeys.road;
        d[3] = config_word(0x87);
        d[4] = g_rkeys.plain_sky;
        for (i = 0, at = 0x81; i < 5; i++, at += 2) {
            config_set_word(at, d[i]);
            g_session.kept[at - 9] = (unsigned char)d[i];
            g_session.kept[at - 8] = (unsigned char)(d[i] >> 8);
        }
    }
    /* (left from the pause menu, 0x13c17c = 2, the result 0x13c178 stays 0) */
    g_session.result = g_timing.result;
    session_race_end();
    music_fade_out();
    music_wait_end();
    music_level_from(CFG_MENU_VOLUME);
    g_road_paint.cars = g_road_paint.cars_behind = 0;
    g_road_paint.car_row = 0;
    g_ncars = 0;
    for (i = 0; i < ncars; i++)
        car_free(&cars[i]);
    video_lift(0, 0, 0, 0, 0, -1);
    hud_free();
    tall_on(0);
    smoke_free();
    dash_free();
    trackfam_free();
    track_free();
    if (g_replay.end)
        return g_replay.end;
    return g_pause.quit ? 2 : g_timing.result;
}

/* the settings +9 .. +0xb4 and the keys kept while a race runs, and put back */
static void keep_settings(void)
{
    memcpy(g_session.kept, g_config + 9, sizeof g_session.kept);
    memcpy(g_session.kept_keys, g_config + 0x5f36, sizeof g_session.kept_keys);
}

static void put_settings_back(void)
{
    memcpy(g_config + 9, g_session.kept, sizeof g_session.kept);
    memcpy(g_config + 0x5f36, g_session.kept_keys, sizeof g_session.kept_keys);
}

void race_from_menu(void)
{
    static const unsigned char black[768];
    int over = 0, seg, track;
    session_racer *me;

    g_session.race_type = config_word(CFG_RACE_TYPE);
    memset(g_session.seg_total, 0, sizeof g_session.seg_total);
    memset(g_session.seg_top, 0, sizeof g_session.seg_top);
    while (!over && !g_quit) {
        /* (the track's record splits copied out to 0x12c0e8 and back unchanged: read by
           nothing in between) */
        keep_settings();
        race_run();
        video_palette(black, 0, 256);
        video_mode(640, 480);
        video_palette(black, 0, 256);
        mouse_limits(0, 0, 640, 480);          /* (0x37941: the replay bar's taken off) */
        if (g_quit)
            break;
        /* one segment on in the copy; the player's total and top speed for it; the
           distance driven added up (a circuit's: the laps'); the best top speed */
        seg = (short)(g_session.kept[CFG_SEGMENT - 9] | g_session.kept[CFG_SEGMENT - 8] << 8) + 1;
        g_session.kept[CFG_SEGMENT - 9] = (unsigned char)seg;
        g_session.kept[CFG_SEGMENT - 8] = (unsigned char)(seg >> 8);
        me = &g_session.racer[g_player];
        if (seg >= 1 && seg <= 3) {
            g_session.seg_total[seg - 1] = me->total;
            g_session.seg_top[seg - 1] = me->top;
        }
        track = config_word(CFG_TRACK);
        if (me->distance < 1)
            me->total_distance = 0;
        else if (track < 3)
            me->total_distance += me->distance;
        else
            me->total_distance = config_word(CFG_LAPS) * me->distance;
        if (me->best_top < me->top)
            me->best_top = me->top;
        put_settings_back();
        if (g_session.result == 4)                 /* arrested: the police's film */
            race_film_police();
        if (g_session.result == 1) {
            if (track > 2 || seg > 2) {
                /* (the record splits back when the race's total beat the best: the same
                   words) the next race from the first segment; the winner's film */
                config_set_word(CFG_SEGMENT, 0);
                race_film_winner();
            }
            fe_sound_bank_load();
            music_random();
            if (track < 3 && seg < 3) {
                /* between the segments: the standings, then the checkpoint's menu */
                standings_after(1, 0);
                if (!race_menu_checkpoint()) {
                    config_set_word(CFG_SEGMENT, 0);
                    over = 1;
                }
            } else {
                /* the last segment or a circuit: the standings (a tournament's points
                   given), then the finish menu */
                standings_after(1, 1);
                race_menu_finish();
                over = 1;
            }
        } else {
            /* given up, left or turned back: the standings, the finish menu, and the
               next race from the first segment */
            music_random();
            fe_sound_bank_load();
            standings_after(0, track > 2 || seg > 2);
            race_menu_finish();
            config_set_word(CFG_SEGMENT, 0);
            over = 1;
        }
    }
    if (g_quit)
        return;
    /* a tournament goes on from race to race (the racers' results cleared, the points
       and the tracks raced kept) until every track is raced: then, won (the player's
       points the most), +0x5f50 opens Lost Vegas, the Warrior and the mirrored tracks
       (eight tracks), the rally too (nine) (0x689b0, 0x42a84) */
    if (g_session.race_type == 3) {
        int raced = 0, i;

        session_clear_results();
        for (i = 0; i < 9; i++)
            raced += g_session.raced[i];
        if (g_vegas_ok && raced == 9) {
            if (session_player_leads()) {
                config_set_word(CFG_UNLOCKED, 2);
                fe_unlock();
            }
        } else if (!g_vegas_ok && raced == 8) {
            if (session_player_leads()) {
                if (config_word(CFG_UNLOCKED) < 2)
                    config_set_word(CFG_UNLOCKED, 1);
                fe_unlock();
            }
        } else
            return;
    }
    session_new();
}

int race_test(const char *name)
{
    int i, len;

    if (!exe_load())
        return -1;
    config_load();             /* the player's settings, the records the status bar shows */
    session_new();
    /* the track (+0x95) and segment (+0x93) by the name asked for, tr5 Rusty Springs'
       rally; a circuit's laps by the race length (+0xb3) as Control Central sets them
       (0x437fc): 4, 8 or 16, or 2, 6 or 12 on tracks 4..7 */
    memset(&g_race_codes, 0, sizeof g_race_codes);
    g_race_codes.mirror = getenv("RENFS_MIRROR") != 0;
    g_race_codes.rally = getenv("RENFS_RALLY") != 0;
    g_race_codes.explode = getenv("RENFS_EXPLODE") != 0;
    if (!strncasecmp(name, "tr5", 3)) {
        i = 3;
        g_race_codes.rally = 1;
    } else {
        for (i = 8; i >= 0; i--)
            if (!strncasecmp(name, track_names[i], strlen(track_names[i])))
                break;
        if (i < 0) {
            fprintf(stderr, "renfs: no track %s (cy1 .. al3, tr1 .. tr7)\n", name);
            return -1;
        }
    }
    config_set_word(CFG_TRACK, i);
    if (getenv("RENFS_TOD"))   /* the time of day as the track's screen sets it */
        rec_put(REC_TOD + 4 * i, atoi(getenv("RENFS_TOD")));
    config_set_word(CFG_SEGMENT, i < 3 && name[2] >= '1' && name[2] <= '3' ? name[2] - '1' : 0);
    len = config_word(CFG_RACE_LENGTH);
    len = len < 0 || len > 2 ? 2 : len;
    config_set_word(CFG_LAPS, (i >= 4 && i <= 7 ? (int[]){ 2, 6, 12 } : (int[]){ 4, 8, 16 })[len]);
    keep_settings();
    return race_run();
}

int race_replay_test(const char *name, int kind)
{
    int r;

    if (!exe_load())
        return -1;
    config_load();
    session_new();
    memset(g_config + 0x3b, 0, 0x20);
    snprintf((char *)g_config + 0x3b, 0x20, "%s", name);
    config_set_word(0xaf, kind);
    keep_settings();
    r = race_run();
    config_set_word(0xaf, 0);
    return r;
}
