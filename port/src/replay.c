/* The replay (NOTES §4.22): the recording while racing, REPLAY.RPL, and the race driven
   again from it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mister.h"
#include "replay.h"
#include "joystick.h"
#include "camera.h"
#include "cardraw.h"
#include "config.h"
#include "collide.h"
#include "crash.h"
#include "drive.h"
#include "exe.h"
#include "files.h"
#include "menu.h"
#include "opponent.h"
#include "racekeys.h"
#include "racesnd.h"
#include "skid.h"
#include "timing.h"
#include "track.h"
#include "video.h"

unsigned char g_rec[REC_SIZE];
replay_state g_replay;
replay_bar g_bar;
watch_bar g_watch;

/* the file's blocks (§3.15): the race record, then 0x9a4 bytes from 0x144ba8 (the
   highlights), the recording (UsrCtrlBuff 0xf6b98), 0xcc8 bytes from 0x143ed8 (the
   snapshots' other state), the racers' results (9 of 0x1d2 bytes from 0x14ea80) */
#define RPL_EXTRA 0x9a4
#define RPL_RING 0x15e00
#define RPL_SNAPS 0xcc8
#define RPL_RESULT 0x1d2
#define RPL_SIZE (REC_SIZE + RPL_EXTRA + RPL_RING + RPL_SNAPS + 9 * RPL_RESULT)

/* the recording: the control words (a racer's low bytes at k · 7200 + t, its high
   bytes at 14,400 + k · 7200 + t, t = the counter / 4), then the snapshots: the human
   racers' cars (0x1a4 bytes at 0x7080 + s · 0x1a4 + slot · 0x1a40), the other cars'
   (0x172 bytes at 0xa500 + s · 0xb90 + j · 0x172), s = the counter / 1800 */
#define RING_SLOTS 7200
#define RING_COUNTS 28800      /* 0x7080: the counter's ring (2 a step, 4 a slot) */
#define SNAP_COUNTS 1800       /* 0x708: 27 seconds */
#define SNAP_HUMANS 0x7080
#define SNAP_HUMAN 0x1a4
#define SNAP_OTHERS 0xa500
#define SNAP_OTHER 0x172
#define SNAPS 16
#define SNAP_STATE 0xa2        /* each snapshot's other state (0x143eec + s · 0xa2) */

static unsigned char *g_ring;
static unsigned char g_snap_state[SNAPS][SNAP_STATE];
static unsigned char g_snaps_head[0x14];       /* 0x143ed8 .. 0x143eeb */
static unsigned char g_results[9][RPL_RESULT];

static int rd32(const unsigned char *p)
{
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static void wr32(unsigned char *p, int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

int rec_get(int off)
{
    return rd32(g_rec + off);
}

void rec_put(int off, int v)
{
    wr32(g_rec + off, v);
}

/* the flags of a step from the counter (0x5ca84) */
static void step_flags(void)
{
    int st = g_drive.steps;

    g_drive.flags = (st & 1 ? 0 : 1) | (st & 3 ? 0 : 4) | (st & 7 ? 0 : 0x10);
}

void replay_setup_record(void)
{
    int menu_car = config_word(CFG_CAR), auto_box = config_word(CFG_AUTO);
    unsigned char *e0 = g_rec + REC_ENTRY, *e1 = e0 + REC_ENTRY_SIZE;
    const unsigned char *p;
    int v;

    /* (a race over a link and the two-player game are not ported: one player, recorded) */
    rec_put(REC_SEGMENT, config_word(CFG_SEGMENT));
    rec_put(REC_TRACK, config_word(CFG_TRACK));
    rec_put(REC_TYPE, config_word(CFG_RACE_TYPE));
    rec_put(REC_GEARBOX, config_word(CFG_TRANSMISSION));
    rec_put(REC_LAPS, config_word(CFG_LAPS));
    rec_put(REC_CLOCK, 1);
    rec_put(REC_TABLE, config_word(0xb3));
    rec_put(REC_FLAGS, (int)g_cheat_flags);
    rec_put(REC_HUMANS, 1);
    rec_put(REC_PLAYER, 0);
    rec_put(REC_LINKED, 0);
    switch (config_word(CFG_RACE_TYPE)) {
    case 2:
    case 3:
        rec_put(REC_RACERS, 8);
        rec_put(REC_POLICE, 0);
        rec_put(REC_TRAFFIC, 0);
        break;
    case 0:
        rec_put(REC_RACERS, 1);
        rec_put(REC_POLICE, 0);
        rec_put(REC_TRAFFIC, 0);
        break;
    default:
        /* head to head: on the open roads the police and six of the traffic, unless
           "No mercy" (the codes' 4) */
        rec_put(REC_RACERS, 2);
        if (!(g_cheat_flags & 4) && config_word(CFG_TRACK) < 3) {
            rec_put(REC_POLICE, 1);
            rec_put(REC_TRAFFIC, 6);
        } else {
            rec_put(REC_POLICE, 0);
            rec_put(REC_TRAFFIC, 0);
        }
        break;
    }
    /* the player's entry: the name, the car (the game's number), the gearbox, traction
       control and ABS on; the rival's a copy with its car */
    memset(e0, 0, REC_ENTRY_SIZE);
    memcpy(e0 + ENT_NAME, g_config, 9);
    e0[ENT_NAME + 8] = 0;
    p = exe_at(0x36250 + 2 * (menu_car & 15), 2);
    wr32(e0 + ENT_CAR, p ? (short)(p[0] | p[1] << 8) : 0);
    wr32(e0 + ENT_AUTO, auto_box);
    wr32(e0 + ENT_TCS, config_word(CFG_TCS) == 0);
    wr32(e0 + ENT_ABS, config_word(CFG_ABS) == 0);
    /* (0x36a0c) the steering's, the pedals' and the third control's kinds from the
       controls chosen (CONFIG.DAT +0x5f36, +0x5f38, +0x5f3a): 0 the keys */
    v = config_word(0x5f36);
#ifdef MISTER_HYBRID
    /* (the MiSTer's: the pad's stick and triggers analogue whatever the controls say,
       the keys too, drive_keyboard; Danik, 2026-10-21) */
    if (v == 0 && g_joy.on && mister_on())
        v = 1;
#endif
    wr32(e0 + ENT_STEER, v == 0 ? 0 : v < 4 ? 1 : 2);
    v = config_word(0x5f38);
#ifdef MISTER_HYBRID
    if (v == 0 && g_joy.on && mister_on())
        v = 1;
#endif
    wr32(e0 + ENT_PEDALS, v == 0 ? 0 : v == 3 ? 3 : v < 4 ? 1 : 2);
    v = config_word(0x5f3a);
    wr32(e0 + ENT_PEDALS + 4, v == 0 ? 0 : v == 3 ? 3 : v < 4 ? 1 : 2);
    memcpy(e1, e0, REC_ENTRY_SIZE);
    wr32(e1 + 0x3f, 0);
    wr32(e1 + 0x43, 0);
    p = exe_at(0x36250 + 2 * (config_word(CFG_OPPONENT) & 15), 2);
    wr32(e1 + ENT_CAR, p ? (short)(p[0] | p[1] << 8) : 0);
    rec_put(REC_CATCH_UP, !(g_cheat_flags & 4));
}

/* the file to play: CONFIG.DAT +0x3b's name, the port's own in the home folder (the last
   race's REPLAY.RPL, those saved), else the CD's (REPLAY1..3.RPL) */
static FILE *replay_open(void)
{
    char name[0x40], path[512];
    FILE *f;
    int i;

    memcpy(name, g_config + 0x3b, 0x20);
    name[0x20] = 0;
    for (i = 0; name[i]; i++)
        if (name[i] >= 'A' && name[i] <= 'Z')
            name[i] = (char)(name[i] - 'A' + 'a');
    snprintf(path, sizeof path, "%s/%s", files_home(), name);
    if ((f = fopen(path, "rb")))
        return f;
    snprintf(path, sizeof path, "GAMEDATA/REPLAY/%s", name);
    return file_open(path);
}

int replay_settings(void)
{
    FILE *f = replay_open();
    int ok = f && fread(g_rec, REC_SIZE, 1, f) == 1;

    if (f)
        fclose(f);
    if (!ok) {
        g_replay.mode = 6;
        g_replay.end = 0x20;
        return 0;
    }
    return 1;
}

/* the 0x9a4 bytes from 0x144ba8 as the file keeps them */
static void extra_out(unsigned char *b)
{
    int i;

    memset(b, 0, RPL_EXTRA);
    for (i = 0; i < 30; i++) {
        wr32(b + 12 * i, g_replay.hl[i][0]);
        wr32(b + 12 * i + 4, g_replay.hl[i][1]);
        wr32(b + 12 * i + 8, g_replay.hl[i][2]);
    }
    wr32(b + 0x1e0, g_replay.nhighlights);
    wr32(b + 0x1e4, g_replay.highlight);
    wr32(b + 0x1ec, g_replay.camera_kept);
    wr32(b + 0x1f0, g_replay.start_count);
    for (i = 0; i < 480; i++)
        wr32(b + 0x200 + 4 * i, g_replay.events[i]);
    wr32(b + 0x988, g_replay.auto_camera);
    wr32(b + 0x98c, g_replay.stop);
    wr32(b + 0x990, g_replay.target);
}

static void extra_in(const unsigned char *b)
{
    int i;

    for (i = 0; i < 30; i++) {
        g_replay.hl[i][0] = rd32(b + 12 * i);
        g_replay.hl[i][1] = rd32(b + 12 * i + 4);
        g_replay.hl[i][2] = rd32(b + 12 * i + 8);
    }
    g_replay.nhighlights = rd32(b + 0x1e0);
    g_replay.highlight = rd32(b + 0x1e4);
    g_replay.camera_kept = rd32(b + 0x1ec);
    g_replay.start_count = rd32(b + 0x1f0);
    for (i = 0; i < 480; i++)
        g_replay.events[i] = rd32(b + 0x200 + 4 * i);
    g_replay.auto_camera = rd32(b + 0x988);
    g_replay.stop = rd32(b + 0x98c);
    g_replay.target = rd32(b + 0x990);
}

/* the racers' results as the file keeps them (the game's records 4 bytes late) */
static void results_out(void)
{
    int k, i;

    memset(g_results, 0, sizeof g_results);
    for (k = 0; k < DRIVE_RESULTS; k++) {
        unsigned char *r = g_results[k];

        if (k < DRIVE_PLAYERS) {
            for (i = 0; i < DRIVE_LAPS; i++)
                wr32(r + 4 * i, g_drive.lap_times[k][i]);
            for (i = 0; i < 6; i++)
                wr32(r + 0x198 + 4 * i, g_drive.stats[k][i]);
            wr32(r + 0x1b0, g_timing.tickets[k]);
            wr32(r + 0x1b4, g_timing.warnings[k]);
            wr32(r + 0x1bc, g_timing.finish[k]);
            r[0x1ca] = (unsigned char)g_timing.place[k];
            r[0x1cb] = (unsigned char)(g_timing.place[k] >> 8);
        }
        wr32(r + 0x1c0, g_drive.distance[k]);
        wr32(r + 0x1c4, g_drive.top_speed[k]);
    }
}

/* replay_load 0x5c4a4: all of the file (the player's own three settings kept in its
   entry: the link's, not ported) */
static void replay_load(void)
{
    unsigned char extra[RPL_EXTRA], snaps[RPL_SNAPS];
    FILE *f = replay_open();
    int s;

    if (!f)
        return;
    if (fread(g_rec, REC_SIZE, 1, f) == 1 && fread(extra, RPL_EXTRA, 1, f) == 1
        && fread(g_ring, RPL_RING, 1, f) == 1 && fread(snaps, RPL_SNAPS, 1, f) == 1
        && fread(g_results, sizeof g_results, 1, f) == 1) {
        extra_in(extra);
        memcpy(g_snaps_head, snaps, sizeof g_snaps_head);
        g_replay.last = rd32(snaps + 4);
        g_replay.countdown = rd32(snaps + 0x10);
        for (s = 0; s < SNAPS; s++)
            memcpy(g_snap_state[s], snaps + 0x14 + s * SNAP_STATE, SNAP_STATE);
    }
    fclose(f);
}

void replay_end(void)
{
    unsigned char extra[RPL_EXTRA], snaps[RPL_SNAPS];
    char path[512];
    FILE *f;
    int s;

    if (g_replay.end != 0x40 && g_replay.end != 0x20 && g_ring) {
        snprintf(path, sizeof path, "%s/replay.rpl", files_home());
        if ((f = fopen(path, "wb"))) {
            rec_put(REC_PLAYER, g_player);
            extra_out(extra);
            memset(snaps, 0, sizeof snaps);
            memcpy(snaps, g_snaps_head, sizeof g_snaps_head);
            wr32(snaps + 4, g_replay.last);
            wr32(snaps + 0x10, g_replay.countdown);
            for (s = 0; s < SNAPS; s++)
                memcpy(snaps + 0x14 + s * SNAP_STATE, g_snap_state[s], SNAP_STATE);
            results_out();
            fwrite(g_rec, REC_SIZE, 1, f);
            fwrite(extra, RPL_EXTRA, 1, f);
            fwrite(g_ring, RPL_RING, 1, f);
            fwrite(snaps, RPL_SNAPS, 1, f);
            fwrite(g_results, sizeof g_results, 1, f);
            fclose(f);
        }
    }
    free(g_ring);
    g_ring = 0;
}

void replay_start(void)
{
    free(g_ring);
    g_ring = calloc(RPL_RING, 1);
    memset(g_snap_state, 0, sizeof g_snap_state);
    memset(g_snaps_head, 0, sizeof g_snaps_head);
    g_replay.countdown = 450;
    g_replay.highlight = -2;
    g_replay.last = 0;
    g_replay.restart = 0;
    g_replay.put_back = 0;
    memset(g_replay.events, 0, sizeof g_replay.events);
    g_replay.auto_camera = 0;
    g_replay.attract = 0;
    g_replay.end = 0;
    g_replay.plays = 1;
    /* (the watching bar: drawn whole, the first car's button pressed) */
    g_watch.whole = 1;
    g_watch.pressed = g_watch.lit = g_watch.last_pressed = WATCH_CAR1;
    g_watch.lit_drawn = 1;
    g_watch.name_for = 0;
    /* (the bar: play pressed, full speed, drawn again) */
    g_bar.redraw = 1;
    g_bar.speed = 0;
    g_bar.shown = 6;
    g_bar.pressed = BAR_PLAY;
}

void replay_race_begins(void)
{
    g_replay.countdown = 0;
}

/* --- a car's snapshot (replay_pack_car 0x5cbd0, back 0x5cd2c) --- */

static void vec_out(unsigned char *p, const vec3 *v)
{
    wr32(p, v->x);
    wr32(p + 4, v->y);
    wr32(p + 8, v->z);
}

static void vec_in(const unsigned char *p, vec3 *v)
{
    v->x = rd32(p);
    v->y = rd32(p + 4);
    v->z = rd32(p + 8);
}

/* the body (+0xc4, 0x94 bytes): the port's fields at their places */
static void body_out(unsigned char *p, const car_body *b)
{
    int i;

    for (i = 0; i < 9; i++)
        wr32(p + 4 * i, b->m[i]);
    vec_out(p + 0x24, &b->pos);
    vec_out(p + 0x30, &b->vel);
    vec_out(p + 0x3c, &b->spin);
    wr32(p + 0x60, b->mass);
    wr32(p + 0x64, b->inertia);
    wr32(p + 0x68, b->inv_mass);
    wr32(p + 0x6c, b->inv_inertia);
    wr32(p + 0x70, b->radius);
    for (i = 0; i < 3; i++)
        wr32(p + 0x74 + 4 * i, b->half[i]);
    wr32(p + 0x80, b->wait);
    wr32(p + 0x84, b->honks);
    wr32(p + 0x88, b->steady);
    wr32(p + 0x8c, b->shift);
    wr32(p + 0x90, b->grip);
}

static void body_in(const unsigned char *p, car_body *b)
{
    int i;

    for (i = 0; i < 9; i++)
        b->m[i] = rd32(p + 4 * i);
    vec_in(p + 0x24, &b->pos);
    vec_in(p + 0x30, &b->vel);
    vec_in(p + 0x3c, &b->spin);
    b->mass = rd32(p + 0x60);
    b->inertia = rd32(p + 0x64);
    b->inv_mass = rd32(p + 0x68);
    b->inv_inertia = rd32(p + 0x6c);
    b->radius = rd32(p + 0x70);
    for (i = 0; i < 3; i++)
        b->half[i] = rd32(p + 0x74 + 4 * i);
    b->wait = rd32(p + 0x80);
    b->honks = rd32(p + 0x84);
    b->steady = rd32(p + 0x88);
    b->shift = rd32(p + 0x8c);
    b->grip = rd32(p + 0x90);
}

static void car_out(unsigned char *p, const race_car *c)
{
    int i;

    wr32(p, c->routine);
    wr32(p + 4, c->state);
    wr32(p + 8, (int)((unsigned)c->slot << 24 | (unsigned)c->part << 8));
    vec_out(p + 0xc, &c->pos);
    wr32(p + 0x18, c->pitch);
    wr32(p + 0x1c, c->yaw);
    wr32(p + 0x20, c->roll);
    wr32(p + 0x24, c->steer);
    wr32(p + 0x28, c->steer_to);
    wr32(p + 0x2c, c->own_turn);
    for (i = 0; i < 9; i++)
        wr32(p + 0x30 + 4 * i, c->turn[i]);
    wr32(p + 0x54, c->node);
    wr32(p + 0x58, c->lap);
    vec_out(p + 0x5c, &c->vel);
    wr32(p + 0x68, c->side_speed);
    wr32(p + 0x6c, c->up_speed);
    wr32(p + 0x70, c->speed);
    wr32(p + 0x74, c->speed_abs);
    wr32(p + 0x78, c->yaw_rate);
    wr32(p + 0x7c, c->half_len);
    wr32(p + 0x80, c->half_wid);
    wr32(p + 0x84, c->road_off);
    wr32(p + 0x88, c->reach);
    vec_out(p + 0x8c, &c->road_side);
    vec_out(p + 0x98, &c->road_up);
    vec_out(p + 0xa4, &c->road_fwd);
    vec_out(p + 0xb0, &c->road_at);
    p[0xbc] = c->bits[0];
    p[0xbd] = c->bits[1];
    p[0xbe] = c->bits[2];
    p[0xbf] = c->bits[3];
    wr32(p + 0xc0, c->height);
    body_out(p + 0xc4, &c->body);
    wr32(p + 0x158, c->road_speed);
    wr32(p + 0x15c, c->zone);
    wr32(p + 0x160, c->lane_offset);
    wr32(p + 0x164, c->crashing);
}

static void car_in(const unsigned char *p, race_car *c)
{
    int i;

    c->routine = rd32(p);
    c->state = rd32(p + 4);
    c->slot = rd32(p + 8) >> 24;
    c->part = (unsigned char)(rd32(p + 8) >> 8);
    vec_in(p + 0xc, &c->pos);
    c->pitch = rd32(p + 0x18);
    c->yaw = rd32(p + 0x1c);
    c->roll = rd32(p + 0x20);
    c->steer = rd32(p + 0x24);
    c->steer_to = rd32(p + 0x28);
    c->own_turn = rd32(p + 0x2c);
    for (i = 0; i < 9; i++)
        c->turn[i] = rd32(p + 0x30 + 4 * i);
    c->node = rd32(p + 0x54);
    c->lap = rd32(p + 0x58);
    vec_in(p + 0x5c, &c->vel);
    c->side_speed = rd32(p + 0x68);
    c->up_speed = rd32(p + 0x6c);
    c->speed = rd32(p + 0x70);
    c->speed_abs = rd32(p + 0x74);
    c->yaw_rate = rd32(p + 0x78);
    c->half_len = rd32(p + 0x7c);
    c->half_wid = rd32(p + 0x80);
    c->road_off = rd32(p + 0x84);
    c->reach = rd32(p + 0x88);
    vec_in(p + 0x8c, &c->road_side);
    vec_in(p + 0x98, &c->road_up);
    vec_in(p + 0xa4, &c->road_fwd);
    vec_in(p + 0xb0, &c->road_at);
    c->bits[0] = p[0xbc];
    c->bits[1] = p[0xbd];
    c->bits[2] = p[0xbe];
    c->bits[3] = p[0xbf];
    c->height = rd32(p + 0xc0);
    body_in(p + 0xc4, &c->body);
    c->road_speed = rd32(p + 0x158);
    c->zone = rd32(p + 0x15c);
    c->lane_offset = rd32(p + 0x160);
    c->crashing = rd32(p + 0x164);
    /* (0x5cd2c's end) the sliding cleared, how far along from the lap and the node */
    c->slide_front = c->slide_rear = 0;
    c->progress = (c->lap - 1) * g_track.length * 4 + c->node;
}

/* replay_record_car 0x5d0e4: a human racer's car with its engine and gearbox, another's
   with its targets */
static unsigned char *human_snap(int s, int slot)
{
    return g_ring + SNAP_HUMANS + s * SNAP_HUMAN + slot * 0x1a40;
}

static unsigned char *other_snap(int s, int j)
{
    return g_ring + SNAP_OTHERS + s * 0xb90 + j * SNAP_OTHER;
}

/* a car's snapshot into p: 0x1a4 bytes a human's, 0x172 the others' */
static void pack_car(unsigned char *p, const race_car *c)
{
    if (c->slot < g_humans) {
        car_out(p, c);
        p[0x168] = (unsigned char)c->throttle;
        p[0x169] = (unsigned char)c->throttle_kept;
        p[0x16a] = (unsigned char)c->braking;
        p[0x16b] = (unsigned char)(c->shift_wait + 100);
        wr32(p + 0x16c, (int)((unsigned)c->rpm_road << 16 | (unsigned)c->rpm));
        wr32(p + 0x170, c->grip_extra);
        wr32(p + 0x174, c->grip_rear);
        wr32(p + 0x178, c->grip_front);
        wr32(p + 0x17c, c->wheel_speed);
        wr32(p + 0x180, c->wheelspin);
        p[0x184] = (unsigned char)c->gearbox;
        p[0x185] = (unsigned char)(c->gear + 2);
        p[0x186] = (unsigned char)(c->shifting * 2 | c->handbrake << 2 | c->clutch << 3
                                   | c->on_ground | (c->tyres[1] & 2 ? 0x20 : 0));
        wr32(p + 0x188, c->air_steps);
        wr32(p + 0x18c, c->shift_last);
        wr32(p + 0x190, c->push_side);
        wr32(p + 0x194, c->push_up);
        wr32(p + 0x198, c->push_fwd);
        wr32(p + 0x19c, c->engine_force);
        wr32(p + 0x1a0, c->surface);
    } else {
        car_out(p, c);
        wr32(p + 0x168, c->speed_to);
        wr32(p + 0x16c, c->across_to);
        p[0x170] = (unsigned char)c->on_ground;
        p[0x171] = 0;          /* (+0x340: not in the port) */
    }
}

static void record_car(const race_car *c, int s, int j)
{
    pack_car(c->slot < g_humans ? human_snap(s, c->slot) : other_snap(s, j), c);
}

/* replay_restore_car 0x5ceb0 */
static void restore_human(int s, int slot)
{
    const unsigned char *p = human_snap(s, slot);
    race_car *c = g_cars[slot];

    car_in(p, c);
    c->throttle = p[0x168];
    c->throttle_kept = p[0x169];
    c->braking = p[0x16a];
    c->grip_extra = rd32(p + 0x170);
    c->grip_rear = rd32(p + 0x174);
    c->grip_front = rd32(p + 0x178);
    c->push_side = rd32(p + 0x190);
    c->push_up = rd32(p + 0x194);
    c->push_fwd = rd32(p + 0x198);
    c->engine_force = rd32(p + 0x19c);
    c->shift_last = rd32(p + 0x18c);
    c->wheelspin = rd32(p + 0x180);
    c->wheel_speed = rd32(p + 0x17c);
    c->gearbox = p[0x184];
    c->shift_wait = p[0x16b] - 100;
    c->gear = p[0x185] - 2;
    c->rpm_road = rd32(p + 0x16c) >> 16;
    c->rpm = rd32(p + 0x16c) & 0xffff;
    c->air_steps = rd32(p + 0x188);
    c->tyres[1] = p[0x186] & 0x20 ? 2 : 0;
    c->clutch = p[0x186] >> 3 & 1;
    c->handbrake = p[0x186] >> 2 & 1;
    c->shifting = p[0x186] >> 1 & 1;
    c->on_ground = p[0x186] & 1;
    g_rsnd.skid[0] = g_rsnd.skid[1] = -1;
    c->tyres[0] = 0;
    c->surface = rd32(p + 0x1a0);
}

static void restore_other(int s, int j)
{
    const unsigned char *p = other_snap(s, j);
    race_car *c = g_cars[rd32(p + 8) >> 24];

    car_in(p, c);
    c->speed_to = rd32(p + 0x168);
    c->across_to = rd32(p + 0x16c);
    c->on_ground = p[0x170];
}

/* --- the snapshots (replay_record_step 0x5d888's, put back by 0x5d2e4) --- */

/* the fallen objects (2 bytes each, 0xea5d8): 0x5f18c keeps the first in the second as a
   snapshot is taken, 0x5f1e0 gives it back, 0x5f138(s) stands again those that fell at
   or after snapshot s (their byte is the snapshot they fell in, + 1) */
static int objects(void)
{
    return g_track.fallen ? g_track.nfallen : 0;
}

static void objects_keep(void)
{
    int i;

    for (i = 0; i < objects(); i++)
        g_track.fallen[2 * i + 1] = g_track.fallen[2 * i];
}

static void objects_back(int s)
{
    int i;

    for (i = 0; i < objects(); i++) {
        g_track.fallen[2 * i] = g_track.fallen[2 * i + 1];
        if (s <= g_track.fallen[2 * i] - 1)
            g_track.fallen[2 * i] = 0;
    }
}

static void state_out(unsigned char *e, int others)
{
    int i;

    memset(e, 0, SNAP_STATE);
    for (i = 0; i < g_ncars && i < 9; i++) {
        wr32(e + 4 * i, g_timing.by_road[i]);
        wr32(e + 0x24 + 4 * i, g_timing.by_race[i]);
    }
    wr32(e + 0x50, others);
    wr32(e + 0x54, g_crash.refresh);
    wr32(e + 0x58, g_opp.chasing);
    for (i = 0; i < 2; i++) {
        unsigned char *z = e + 0x5c + 12 * i;
        int k;

        for (k = 0; k < 6; k++) {
            z[2 * k] = (unsigned char)g_opp.zone[i][k];
            z[2 * k + 1] = (unsigned char)(g_opp.zone[i][k] >> 8);
        }
    }
    wr32(e + 0x74, g_opp.ticks);
    wr32(e + 0x78, g_rsnd.echo_now);
    wr32(e + 0x7c, (int)g_opp.random);
    wr32(e + 0x80, g_opp.come_node);
    wr32(e + 0x84, g_opp.sides[0]);
    wr32(e + 0x88, g_opp.sides[1]);
    wr32(e + 0x8c, g_opp.waiting);
    /* (+0x90 0x13c21c and +0x98 0xeab6c: read by nothing else) */
    e[0x99] = (unsigned char)g_opp.long_chases;
    wr32(e + 0x9a, g_timing.passed);
    wr32(e + 0x9e, g_timing.rival_passed);
}

static int state_in(const unsigned char *e)
{
    int i;

    for (i = 0; i < g_ncars && i < 9; i++) {
        g_timing.by_road[i] = rd32(e + 4 * i);
        g_timing.by_race[i] = rd32(e + 0x24 + 4 * i);
    }
    g_opp.random = (unsigned)rd32(e + 0x7c);
    g_opp.ticks = rd32(e + 0x74);
    g_opp.long_chases = e[0x99];
    g_rsnd.echo_now = rd32(e + 0x78);
    g_opp.chasing = rd32(e + 0x58);
    g_opp.come_node = rd32(e + 0x80);
    g_opp.sides[0] = rd32(e + 0x84);
    g_opp.sides[1] = rd32(e + 0x88);
    g_opp.waiting = rd32(e + 0x8c);
    for (i = 0; i < 2; i++) {
        const unsigned char *z = e + 0x5c + 12 * i;
        int k;

        for (k = 0; k < 6; k++)
            g_opp.zone[i][k] = (short)(z[2 * k] | z[2 * k + 1] << 8);
    }
    g_crash.refresh = rd32(e + 0x54);
    g_timing.passed = rd32(e + 0x9a);
    g_timing.rival_passed = rd32(e + 0x9e);
    return rd32(e + 0x50);
}

/* 0x5d2e4(pos, -1): the snapshot pos / 1800 put back: the fallen objects, the race's
   state, the cars (the humans', then the others'); the camera on its car again when not
   racing; the engine's sounds again when asked (0x145548) */
static void snapshot_back(int pos)
{
    int s = pos / SNAP_COUNTS, others, i;

    objects_back(g_drive.steps / SNAP_COUNTS);
    others = state_in(g_snap_state[s]);
    for (i = 0; i < g_ncars; i++)
        g_cars[i]->obj_index = g_cars[i]->obj_node = 0;
    for (i = 0; i < g_humans; i++)
        restore_human(s, i);
    for (i = 0; i < others; i++)
        restore_other(s, i);
    g_replay.put_back = 0;
    if (g_drive.state != 1 && g_replay.cam) {
        /* the cameras on the police and the other racer (7, 6) back to 5 when it is not
           near */
        race_camera *cam = g_replay.cam;
        const race_car *c = g_cars[g_player];

        if (cam->wanted == 7) {
            if (camera_police_close(0, 5))
                c = g_cars[2];
            else
                cam->wanted = 5;
        } else if (cam->wanted == 6) {
            if (camera_rival_close(0, 5))
                c = g_cars[g_player == 0];
            else
                cam->wanted = 5;
        }
        camera_set_car(cam, c, cam->wanted);
    }
    if (g_replay.engine_again) {
        g_replay.engine_again = 0;
        rsnd_engine_restart();
    }
}

/* replay_record_controls 0x5cac4, replay_play_controls 0x5cb58 */
static void controls_out(void)
{
    int t = g_replay.pos / 4, k;

    for (k = 0; k < g_humans; k++) {
        g_ring[k * RING_SLOTS + t] = (unsigned char)g_drive.controls[k];
        g_ring[2 * RING_SLOTS + k * RING_SLOTS + t] = (unsigned char)(g_drive.controls[k] >> 8);
    }
}

static void controls_in(void)
{
    int t = g_replay.pos / 4, k;

    for (k = 0; k < g_humans; k++)
        g_drive.controls[k] = g_ring[k * RING_SLOTS + t]
                              | (unsigned)g_ring[2 * RING_SLOTS + k * RING_SLOTS + t] << 8;
}

/* replay_record_step 0x5d888 (the keyboard's word is read each step, race.c): the first
   120 counts nothing pressed; every 450th call a snapshot (and put back at once), else
   the controls only */
static void record_step(void)
{
    int s = g_replay.pos / SNAP_COUNTS, i, j;

    if (g_drive.steps < 0x78)
        for (i = 0; i < g_humans; i++)
            g_drive.controls[i] = 0x7a0;
    if (g_replay.countdown == 0) {
        objects_keep();
        g_replay.countdown = 450;
        controls_out();
        for (i = 0, j = 0; i < g_ncars; i++) {
            record_car(g_cars[i], s, j);
            if (g_cars[i]->slot >= g_humans)
                j++;
        }
        state_out(g_snap_state[s], j);
        snapshot_back(g_replay.pos);
        g_replay.countdown--;
        return;
    }
    controls_out();
    g_replay.countdown--;
}

void replay_mark(int event)
{
    int slot, now;

    if (g_drive.state != 1)
        return;
    slot = g_replay.pos / 60;
    now = g_drive.steps / 60;
    if (g_replay.events[slot] >> 8 < now)
        g_replay.events[slot] = 0;
    if ((g_replay.events[slot] & 0xff) < event)
        g_replay.events[slot] = now << 8 | (event + 0x80);
}

/* --- playing back --- */

/* 0x5d690(to, end): the race put at counter `to` (a snapshot's: shown from there; else
   from the snapshot before, run fast to it), played until `end`. Leaving the race: the
   fallen objects and the clocks kept to come back to */
static void jump(int to, int end)
{
    if (g_replay.last < end)
        end = g_replay.last;
    g_replay.pos = to % RING_COUNTS;
    g_replay.stop = end;
    if (g_drive.state == 1) {
        objects_keep();
        g_replay.clock_kept[0] = 1;            /* (race.c keeps the clocks) */
    }
    if (g_replay.pos % SNAP_COUNTS == 0) {
        g_drive.steps = to;
        g_drive.state = 2;
        rsnd_stop_all();
        g_replay.engine_again = 1;
        g_replay.restart = 1;
    } else {
        g_drive.steps = to / SNAP_COUNTS * SNAP_COUNTS;
        g_replay.target = to;
        g_drive.state = 3;
        rsnd_stop_all();
        g_replay.restart = 0;
    }
    g_replay.pos = g_drive.steps % RING_COUNTS;
    step_flags();
    g_replay.put_back = 1;
    crash_smoke_reset();
    skid_reset();
    /* (0x2d2e4) the police's picture taken down */
    if (g_opp.ticket_up || g_opp.warning_up)
        g_opp.msg_time = g_drive.steps;
    g_opp.ticket_up = g_opp.warning_up = 0;
    g_crash.restart = 0;
    g_opp.msg_time = -1000;
    g_replay.engine_again = 1;
    g_timing.split = 0;
    g_timing.split_shown = 0;
}

/* 0x5c768: the highlights from the events: thirty times the greatest event left (each
   slot's mark taken down by 0x80 once taken), in time order, two slots before to five
   after, joined where they meet; none: the whole race */
static void highlights(void)
{
    int slots = g_replay.last / 60, first = (g_replay.last - RING_COUNTS) / 60;
    int at[30], n = 0, i, k, best, bi = 0, swapped;

    if (slots > 480)
        slots = 480;
    first = first < -2 ? -2 : first + 60;
    for (k = 0; k < 30; k++) {
        best = 0x80;
        for (i = 0; i < slots; i++) {
            int e = g_replay.events[i] & 0xff;

            if (best <= e && first <= g_replay.events[i] >> 8) {
                at[n] = g_replay.events[i] >> 8;
                best = e;
                bi = i;
            }
        }
        if (best > 0x80) {
            n++;
            g_replay.events[bi] -= 0x80;
        }
    }
    for (i = 0; i < slots; i++) {
        int e = g_replay.events[i] & 0xff;

        if (e && e < 0x80)
            g_replay.events[i] += 0x80;
    }
    for (k = 0; k < n; k++) {
        swapped = 0;
        for (i = 1; i < n; i++)
            if (at[i] < at[i - 1]) {
                int t = at[i];

                at[i] = at[i - 1];
                at[i - 1] = t;
                swapped = 1;
            }
        if (!swapped)
            break;
    }
    g_replay.nhighlights = 0;
    for (k = 0; k < n; k++) {
        int c = g_replay.nhighlights, s = at[k] - 2;

        if (s < first)
            continue;
        g_replay.hl[c][0] = s * 60 - s * 60 % 4;
        g_replay.hl[c][1] = (at[k] + 5) * 60 - (at[k] + 5) * 60 % 4;
        g_replay.hl[c][2] = g_replay.events[at[k]] & 0xff;
        if (g_replay.hl[c][0] < 0)
            g_replay.hl[c][0] = 0;
        if (g_replay.hl[c][1] > g_replay.last)
            g_replay.hl[c][1] = g_replay.last;
        if (c > 0 && g_replay.hl[c][0] < g_replay.hl[c - 1][1]) {
            g_replay.hl[c - 1][1] = g_replay.hl[c][1];
            if (g_replay.hl[c - 1][2] < g_replay.hl[c][2])
                g_replay.hl[c - 1][2] = g_replay.hl[c][2];
        } else
            g_replay.nhighlights = c + 1;
    }
    if (g_replay.nhighlights == 0) {
        g_replay.hl[0][0] = 0;
        g_replay.hl[0][1] = g_replay.last;
        g_replay.hl[0][2] = 200;
        g_replay.nhighlights = 1;
    }
    if (getenv("RENFS_REPLAYCHECK"))
        for (k = 0; k < g_replay.nhighlights; k++)
            fprintf(stderr, "highlight %d: %d..%d event 0x%x\n", k, g_replay.hl[k][0],
                    g_replay.hl[k][1], g_replay.hl[k][2]);
}

/* 0x5dac4: a highlight's camera: the other racer or the police near, theirs (6, 7);
   else by the event: a big one sometimes from above (2), else one of the cameras behind
   (1 .. 4, or 0 .. 4 for 0xdb .. 0xe3) */
static void director(void)
{
    race_camera *cam = g_replay.cam;
    int police = camera_police_close(0, 5), rival = camera_rival_close(0, 5);
    int e = g_replay.hl[g_replay.highlight < 0 ? 0 : g_replay.highlight][2];
    const race_car *c;

    if (police || rival) {
        if (police && rival ? collide_below(10) < 6 : !police) {
            cam->wanted = 6;
            c = g_cars[g_player == 0];
        } else {
            cam->wanted = 7;
            c = g_cars[2];
        }
    } else {
        if (e >= 0xd6 && e <= 0xf7 && collide_below(10) >= 8)
            cam->wanted = 2;
        else if (e < 0xdb || e > 0xe3)
            cam->wanted = collide_below(4) + 1;
        else
            cam->wanted = collide_below(5);
        c = g_cars[g_player];
    }
    camera_set_car(cam, c, cam->wanted);
    if (g_replay.highlight >= 0 && cam->wanted > 0)
        collide_below(10);
}

/* 0x5dc54: a step played: at a snapshot's count the race put back there, the controls
   from the ring; on starting, the camera and the sounds again */
/* RENFS_REPLAYCHECK: the race driven from the controls alone, each of the file's
   snapshots after the first compared with it instead of put back */
static void check(int s)
{
    int i, j, others = rd32(g_snap_state[s] + 0x50), bad = 0;

    for (i = 0; i < g_humans + others; i++) {
        const unsigned char *p = i < g_humans ? human_snap(s, i) : other_snap(s, i - g_humans);
        const race_car *c = g_cars[rd32(p + 8) >> 24];
        unsigned char now[SNAP_HUMAN];
        int n = i < g_humans ? SNAP_HUMAN : SNAP_OTHER, k, first = -1, words = 0;

        /* every byte of the car's snapshot but those nothing writes (the body's +0x48 ..
           +0x5f, the human's +0x187) and the one the port has not (+0x171) */
        memset(now, 0, sizeof now);
        pack_car(now, c);
        for (j = 0; j < n; j += 4)
            for (k = j; k < j + 4 && k < n; k++)
                if (now[k] != p[k] && !(k >= 0x10c && k < 0x124)
                    && !(i < g_humans ? k == 0x187 : k == 0x171)) {
                    if (first < 0)
                        first = j;
                    if (getenv("RENFS_REPLAYCHECK")[0] == '2')
                        fprintf(stderr, "  +0x%x %d (file %d)\n", j, rd32(now + j), rd32(p + j));
                    words++;
                    break;
                }
        if (words) {
            bad++;
            fprintf(stderr, "replay check %d: car %d differs in %d words, the first +0x%x: %d"
                    " (file %d); node %d (file %d)\n", g_drive.steps, rd32(p + 8) >> 24, words,
                    first, rd32(now + first), rd32(p + first), c->node, rd32(p + 0x54));
        }
    }
    {
        /* the race's other state: every byte it keeps but +0x90 and +0x98 (read by
           nothing) and +0x99 (the long chases, counted only while racing) */
        unsigned char e[SNAP_STATE];

        state_out(e, others);
        for (j = 0; j < SNAP_STATE; j++)
            if (e[j] != g_snap_state[s][j] && !(j >= 0x90 && j <= 0x99)) {
                bad++;
                fprintf(stderr, "replay check %d: the race's state differs at +0x%x\n",
                        g_drive.steps, j);
                break;
            }
    }
    fprintf(stderr, "replay check %d (snapshot %d): %d of %d cars differ\n", g_drive.steps, s,
            bad, g_humans + others);
}

static void play_step(void)
{
    static int came = -100;

    /* (RENFS_REPLAYCHECK: a snapshot reached by playing on is compared, not put back) */
    if (g_replay.pos % SNAP_COUNTS == 0) {
        if (getenv("RENFS_REPLAYCHECK") && came < g_drive.steps && came >= g_drive.steps - 4)
            check(g_replay.pos / SNAP_COUNTS);
        else
            snapshot_back(g_replay.pos);
    }
    came = g_drive.steps;
    controls_in();
    if (g_replay.restart) {
        g_crash.respawned = 0;
        camera_set_car(g_replay.cam, g_cars[g_player], g_replay.cam->wanted);
        if (g_replay.highlight > -2)
            director();
        g_replay.restart = 0;
        if (g_replay.mark_start) {
            g_replay.mark_start = 0;
            g_replay.start_count = g_drive.steps;
        }
        rsnd_engine_restart();
    }
    if (g_replay.auto_camera && (g_drive.steps & 0xff) == 0xfc)
        camera_next(g_replay.cam);
}

/* 0x5e118: the replay begins: 2 the highlights from the first, 3 the whole recording
   (back 27,000 counts at most), 1 the pause menu's (back 600) */
static void begin(int mode)
{
    int from;

    if (mode == 2) {
        if (g_drive.state == 1)
            g_replay.camera_kept = g_replay.cam->wanted;
        highlights();
        if (g_replay.nhighlights > 0) {
            g_replay.highlight = 0;
            jump(g_replay.hl[0][0], g_replay.hl[0][1]);
            g_replay.speed = 4;
            g_replay.wait = 0;
            g_bar.shown = 6;
            g_bar.pressed = BAR_PLAY;
            g_bar.redraw = 1;
            g_bar.speed = 0;
        }
        return;
    }
    g_replay.highlight = -2;
    g_replay.speed = 4;
    g_replay.wait = 0;
    g_bar.shown = 6;
    g_bar.pressed = BAR_PLAY;
    g_bar.speed = 0;
    g_bar.redraw = 1;
    from = g_replay.last - (mode == 3 ? 27000 : 600);
    if (from < 0)
        from = 0;
    g_drive.steps = from;
    g_replay.mark_start = 1;
    g_replay.resume = 4;
    jump(from - from % 4, g_replay.last);
    if (mode == 3)
        g_replay.auto_camera = 1;
    g_replay.highlight = -1;
}

/* 0x5e020: Done in the pause menu's replay: run fast to the last step recorded (less 8)
   and race on from there */
static void back_to_race(void)
{
    int to = g_replay.last - 8;

    if (to < 0)
        to = 0;
    jump(to - to % 4, g_replay.last);
    g_replay.speed = 4;
    g_bar.shown = 6;
    g_replay.resume = 1;
    g_replay.wait = 0;
    g_bar.pressed = 0;
    g_bar.speed = 0;
    g_bar.redraw = 1;
    g_replay.cam->wanted = g_replay.camera_kept;
    /* the driver's seat asked for while the player's car lies rolled over: the camera
       above it at once */
    if (g_replay.camera_kept == 0 && g_cars[0]->roll > 0x1e0000 && g_cars[0]->roll < 0xdc0000)
        g_replay.cam->mode = 4;
    g_replay.highlight = -2;
    rsnd_mute(0);
    rsnd_engine_restart();
    g_watch.whole = 1;         /* (0x5e020) */
}

/* 0x5ec04: the next highlight (after the last, the first again; the attract mode's
   ends) */
static void next_highlight(void)
{
    int e, near = 0, i;

    g_replay.speed = 4;
    g_replay.wait = 0;
    g_bar.shown = 6;
    g_bar.pressed = BAR_PLAY;
    g_bar.speed = 0;
    g_replay.highlight++;
    g_bar.redraw = 1;
    if (g_replay.highlight >= g_replay.nhighlights) {
        if (!g_replay.attract) {
            g_replay.highlight = 0;
            jump(g_replay.hl[0][0], g_replay.hl[0][1]);
            play_step();
            return;
        }
        g_replay.end = 0x20;
        return;
    }
    i = collide_below(10);
    e = g_replay.hl[g_replay.highlight][2];
    if (e == 0xe4) {
        int k;

        for (k = 0; k < g_humans; k++)
            if (k != g_player) {
                int d = g_cars[g_player]->node - g_cars[k]->node;

                if ((d < 0 ? -d : d) < 3) {
                    near = 1;
                    break;
                }
            }
    } else
        near = 1;
    if (e < 0xdb || e > 0xf7 || i < 5 || g_replay.slowed || !near)
        g_replay.slowed = 0;
    else {
        /* a big one played at half speed */
        g_replay.speed = 3;
        g_replay.wait = 2;
        g_bar.shown = 8;
        g_bar.pressed = BAR_PLAY;
        g_bar.speed = 1;
        g_bar.redraw = 1;
        g_replay.slowed = 1;
    }
    jump(g_replay.hl[g_replay.highlight][0], g_replay.hl[g_replay.highlight][1]);
    play_step();
}

/* 0x5ee14: a step while playing: held, only the flags; else played to the end; at the
   end of the recording (not the highlights) racing on, or held at the start */
static void playing(void)
{
    if (g_replay.speed == 0 && g_replay.hold < 1) {
        step_flags();
        return;
    }
    if (g_drive.steps < g_replay.stop && (!g_crash.respawned || g_replay.highlight < 0)) {
        play_step();
        return;
    }
    if (g_replay.stop == g_replay.last && g_replay.highlight < 0) {
        g_drive.state = g_replay.resume;
        if (g_drive.state == 1) {
            g_replay.back = 1;     /* (race.c: the clocks as they were) */
        } else {
            g_drive.steps = g_replay.start_count;
            step_flags();
            g_replay.pos = g_drive.steps % RING_COUNTS;
            jump(g_drive.steps, g_replay.last);
            play_step();
        }
        rsnd_engine_restart();
        return;
    }
    if (g_replay.highlight >= 0)
        next_highlight();
}

/* 0x5decc: |<, >|, << and >> (held: by more the longer, a sixteenth of the steps held,
   in fours): the counter moved, nothing played (state 4) until a speed is chosen; at
   most 27,900 counts back from the end, at least 300 before it */
static void seek(int button, int repeats)
{
    int by = repeats ? repeats / 16 * 4 : 4, first = g_replay.last - 27900;
    int end = g_replay.last - 300, *t = &g_drive.steps;

    if (*t > end)
        *t = end;
    switch (button) {
    case 1:
        *t = first < 0 ? 0 : first;
        g_bar.shown = 4;
        break;
    case 2:
        *t = end < first ? first : end;
        if (*t < 0)
            *t = 0;
        g_bar.shown = 3;
        break;
    case 3:
        if (by < *t)
            *t -= by;
        if (*t < first)
            *t = first;
        g_bar.shown = 4;
        break;
    case 4:
        if (*t < end - by)
            *t += by;
        if (*t < 0)
            *t = 0;
        g_bar.shown = 3;
        break;
    }
    g_replay.auto_camera = 0;
    *t -= *t % 4;
    if (g_drive.state == 1)
        objects_keep();
    g_drive.state = 4;
    g_replay.highlight = -2;
    g_replay.speed = 4;
    g_replay.wait = 0;
    rsnd_mute(1);
}

/* 0x5e308: play at a speed (6 full, 7 a half, 8 a quarter, 9 an eighth), or pause (5,
   once a press: two more steps shown, then held); after seeking, played from there */
static void speed_set(int action, int repeats)
{
    static const int speed[4] = { 4, 3, 2, 1 }, wait[4] = { 0, 2, 4, 8 };

    if (g_drive.state == 1)
        return;
    if (action >= 6 && action <= 9) {
        int k = action - 6;

        g_replay.speed = speed[k];
        g_replay.wait = wait[k];
        g_bar.speed = k;
        g_bar.shown = k ? 7 + k : 6;
        g_bar.pressed = BAR_PLAY;
        g_bar.redraw = 1;
        if (action != 9)
            rsnd_mute(0);
    } else if (action == 5) {
        if (repeats)
            return;
        if (g_drive.state != 3 && g_replay.hold == 0) {
            rsnd_mute(1);
            g_replay.speed = 0;
            g_replay.hold = 2;
            g_bar.shown = 5;
        }
    } else
        return;
    if (g_drive.state == 4) {
        g_replay.resume = 4;
        g_replay.mark_start = 1;
        jump(g_drive.steps - g_drive.steps % 4, g_replay.last);
        g_replay.auto_camera = 0;
    }
}

/* the mouse in the race's picture (the port keeps it in 640x480's) */
static void bar_mouse(int *x, int *y)
{
    mouse_read(x, y);
    *x = *x * H(320, 640) / 640;
    *y = *y * H(200, 480) / 480;
}

/* the mouse put at a pixel of the race's screen (0x84848): rounded up into the 640x480
   picture's, so that bar_mouse reads that pixel back */
static void mouse_place_at(int x, int y)
{
    mouse_place((x * 640 + H(319, 639)) / H(320, 640), (y * 480 + H(199, 479)) / H(200, 480));
}

static int bar_in(int k, int x, int y)
{
    const int *r = g_bar.rect[k];

    return r[0] <= x && x <= r[2] && r[1] <= y && y <= r[3];
}

/* 0x72f28: the key held, in this order: Space, Enter, Esc, Tab, up, down, left, right */
static int held_key(void)
{
    static const int scan[8] = { 0x39, 0x1c, 0x01, 0x0f, 0x48, 0x50, 0x4b, 0x4d };
    static const int code[8] = { 0x20, 0x0d, 0x1b, 0x09, 0x4800, 0x5000, 0x4b00, 0x4d00 };
    int i;

    for (i = 0; i < 8; i++)
        if (key_down(scan[i]))
            return code[i];
    return joy_menu_key();                 /* (0x4dfe0: the joystick's) */
}

/* 0x5e4ec: the buttons by the mouse (moved or pressed; the play button's look kept up
   to date) or the arrow keys (the mouse put on the lit one), Space or Enter pressing
   the lit one, Esc pressing Done; let go, the pressed one stays lit. Then what the
   pressed button asks */
static void bar_input(void)
{
    int over = 0, arrows = 0, x, y, k, key;

    /* the mouse kept on the bar's rows (0x5e4f7: from 180 of 200, 450 of 480) */
    mouse_limits(0, H(0xb4 * 480 / 200, 0x1c2), 640, 480);
    /* (the buttons read once the mouse has been used, 0x156604) */
    fe_mouse_moved();
    if (g_mouse_active)
        fe_mouse_read();
    bar_mouse(&x, &y);
    if (x != g_bar.mouse_was[0] || y != g_bar.mouse_was[1] || g_mouse_buttons
        || g_bar.lit == BAR_PLAY)
        for (k = 1; k <= 8; k++) {
            if ((k == BAR_CAMERA && g_bar.shown == 5) || !bar_in(k, x, y))
                continue;
            if (!g_mouse_buttons) {
                if (k != g_bar.lit) {
                    g_bar.pressed = 0;
                    g_bar.redraw = 1;
                    g_bar.lit = k;
                }
            } else if (k != g_bar.pressed) {
                g_bar.lit = 0;
                g_bar.redraw = 1;
                g_bar.pressed = k;
            }
            over = 1;
        }
    g_bar.mouse_was[0] = x;
    g_bar.mouse_was[1] = y;
    key = held_key();
    if (key) {
        if (key != g_bar.last_key && key == 0x4b00) {
            if (--g_bar.lit < 1)
                g_bar.lit = 8;
            if (g_bar.shown == 5 && g_bar.lit == BAR_CAMERA)
                g_bar.lit = 7;
            g_bar.redraw = 1;
            over = arrows = 1;
        }
        if (key != g_bar.last_key && key == 0x4d00) {
            if (++g_bar.lit > 8)
                g_bar.lit = 1;
            if (g_bar.shown == 5 && g_bar.lit == BAR_CAMERA)
                g_bar.lit = 1;
            g_bar.redraw = 1;
            over = arrows = 1;
        }
        if (arrows) {
            /* the mouse to the middle of the lit picture */
            const int *r = g_bar.rect[g_bar.lit];

            mouse_place_at((g_bar.lit_size[g_bar.lit][0] / 2 + r[0]),
                           (g_bar.lit_size[g_bar.lit][1] / 2 + r[1]));
        }
        if (key == 0x20 || key == 0x0d) {
            g_bar.pressed = g_bar.lit;
            g_bar.redraw = 1;
            over = 1;
        }
    }
    if (key_down(0x01) && g_rkeys.race_clock > 15) {
        g_bar.pressed = BAR_DONE;
        over = 1;
        g_bar.redraw = 1;
    }
    g_bar.last_key = key;
    if (!over && g_bar.pressed) {
        g_bar.lit = g_bar.pressed;
        g_bar.redraw = 1;
        g_bar.pressed = 0;
    }
    switch (g_bar.pressed) {
    case 0:
        break;
    case BAR_DONE:
        g_replay.mode = 7;
        g_bar.speeds = 0;
        break;
    case BAR_START:
        g_bar.action = 1;
        g_bar.speeds = 0;
        break;
    case BAR_BACK:
        g_bar.action = 3;
        g_bar.speeds = 0;
        break;
    case BAR_ON:
        g_bar.action = 4;
        g_bar.speeds = 0;
        break;
    case BAR_END:
        g_bar.action = 2;
        g_bar.speeds = 0;
        break;
    case BAR_PLAY:
        /* the speeds in turn: full, a half, a quarter (while playing) */
        if (g_bar.last_pressed != BAR_PLAY) {
            if (g_drive.state == 2 && ++g_bar.speeds > 3)
                g_bar.speeds = 1;
            g_bar.action = exe_i32(0xf6b58 + 4 * g_bar.speeds);
        }
        break;
    case BAR_PAUSE:
        if (g_bar.last_pressed != BAR_PAUSE) {
            g_bar.action = 5;
            g_bar.speeds = 0;
        }
        break;
    case BAR_CAMERA:
        if (g_bar.last_pressed != BAR_CAMERA)
            g_bar.action = 10;
        break;
    }
    g_bar.last_pressed = g_bar.pressed;
}

static int watch_in(int k, int x, int y)
{
    const int *r = g_watch.rect[k];

    return r[0] <= x && x <= r[2] && r[1] <= y && y <= r[3];
}

/* 0x7f470: the watching bar's buttons by the mouse, once it moved or is pressed: those
   of the cars racing, the camera's while a player's car is watched, never Done (a
   network game's); 1 while it is on one */
static int watch_mouse(void)
{
    int over = 0, x, y, k;

    mouse_limits(0, H(0xb4 * 480 / 200, 0x1c2), 640, 480);
    fe_mouse_moved();
    if (g_mouse_active)
        fe_mouse_read();
    bar_mouse(&x, &y);
    if (x != g_watch.mouse_was[0] || y != g_watch.mouse_was[1] || g_mouse_buttons)
        for (k = 1; k <= WATCH_CAMERA; k++) {
            if (k > WATCH_CAR1 && k < WATCH_AFTER && k - WATCH_CAR1 >= g_field) {
                k = WATCH_AFTER - 1;
                continue;
            }
            if ((k == WATCH_CAMERA && g_drive.viewed >= g_humans) || k == WATCH_DONE
                || !watch_in(k, x, y))
                continue;
            if (!g_mouse_buttons) {
                if (k != g_watch.lit) {
                    g_watch.redraw = 1;
                    g_watch.pressed = 0;
                    g_watch.lit = k;
                }
            } else if (k != g_watch.pressed) {
                g_watch.redraw = 1;
                g_watch.lit = 0;
                g_watch.pressed = k;
            }
            over = 1;
        }
    g_watch.mouse_was[0] = x;
    g_watch.mouse_was[1] = y;
    return over;
}

/* 0x7f5e8: with no menu key held, ',' the car before, '.' the car after, 1..8 a car
   (the first held, if it races); the arrows move the light along (up and down between
   the cars' row and the buttons each side), Space or Enter press the lit one */
static int watch_keys(void)
{
    static const unsigned char scan[10] = { 0x33, 0x34, 2, 3, 4, 5, 6, 7, 8, 9 };
    int key = held_key(), i, done = 0;

    if (!key) {
        for (i = 0; i < 10; i++)
            if (key_down(scan[i])) {
                if (i >= 2 && g_field < i - 1)
                    break;
                g_watch.pressed = i == 0 ? WATCH_BEFORE : i == 1 ? WATCH_AFTER : WATCH_CAR1 + i - 2;
                g_watch.redraw = done = 1;
                break;
            }
        g_watch.last_key = 0;
        return done;
    }
    if (key != g_watch.last_key) {
        if (key == 0x4b00) {
            /* (Done passed over: a network game's) */
            int k = --g_watch.lit == WATCH_DONE ? 0 : g_watch.lit;

            if (k < 1)
                g_watch.lit = g_drive.viewed < g_humans ? WATCH_CAMERA : WATCH_AFTER;
            else if (k == WATCH_AFTER - 1)
                g_watch.lit = g_field + 2;
            g_watch.redraw = done = 1;
        } else if (key == 0x4d00) {
            ++g_watch.lit;
            if (g_watch.lit <= WATCH_CAMERA
                && (g_watch.lit != WATCH_CAMERA || g_drive.viewed < g_humans)) {
                if (g_watch.lit > WATCH_CAR1 && g_watch.lit < WATCH_AFTER
                    && g_watch.lit > g_field + 2)
                    g_watch.lit = WATCH_AFTER;
            } else
                g_watch.lit = WATCH_BEFORE;
            g_watch.redraw = done = 1;
        } else if (key == 0x4800) {
            if (g_watch.lit == WATCH_BEFORE) {
                g_watch.lit = WATCH_CAR1;
                g_watch.redraw = done = 1;
            } else if (g_watch.lit == WATCH_AFTER) {
                g_watch.lit = g_field + 2;
                g_watch.redraw = done = 1;
            }
        } else if (key == 0x5000) {
            if (g_watch.lit == WATCH_CAR1) {
                g_watch.lit = WATCH_BEFORE;
                g_watch.redraw = done = 1;
            } else if (g_watch.lit == g_field + 2) {
                g_watch.lit = WATCH_AFTER;
                g_watch.redraw = done = 1;
            }
        }
    }
    g_watch.last_key = key;
    if (key == 0x20 || key == 0x0d) {
        g_watch.pressed = g_watch.lit;
        g_watch.redraw = done = 1;
    }
    return done;
}

static void watch_car(int n, int engine)
{
    g_drive.viewed = n;
    camera_set_car(g_replay.cam, g_cars[n], 4);
    if (engine)
        rsnd_engine_on();
}

/* 0x7f010, each step while the watching bar is up: the mouse, else the keys (the mouse
   then put on the lit button); let go, the pressed one stays lit. The car before or
   after (once a press, round the racers), a car (its engine heard), the camera's next
   mode */
static void watch_step(void)
{
    int over = watch_mouse(), keyed = 0;

    if (!over && (over = watch_keys()))
        keyed = 1;
    if (!over) {
        if (g_watch.pressed) {
            g_watch.lit = g_watch.pressed;
            g_watch.redraw = 1;
            g_watch.pressed = 0;
        }
    } else if (g_watch.pressed == WATCH_BEFORE) {
        if (g_watch.pressed != g_watch.last_pressed)
            watch_car(g_drive.viewed - 1 < 0 ? g_field - 1 : g_drive.viewed - 1, 0);
    } else if (g_watch.pressed >= WATCH_CAR1 && g_watch.pressed < WATCH_AFTER) {
        if (g_watch.pressed - WATCH_CAR1 != g_drive.viewed)
            watch_car(g_watch.pressed - WATCH_CAR1, 1);
    } else if (g_watch.pressed == WATCH_AFTER) {
        if (g_watch.pressed != g_watch.last_pressed)
            watch_car(g_drive.viewed + 1 >= g_field ? 0 : g_drive.viewed + 1, 0);
    } else if (g_watch.pressed == WATCH_CAMERA) {
        if (g_watch.pressed != g_watch.last_pressed && g_drive.viewed < g_humans)
            camera_next(g_replay.cam);
    }
    /* (Done, 1: a network game's leaving, 0x15d138) */
    g_watch.last_pressed = g_watch.pressed;
    if (g_watch.pressed)
        g_watch.lit = g_watch.pressed;
    if (keyed) {
        /* the mouse to the middle of the lit button's normal picture */
        const int *r = g_watch.rect[g_watch.lit];

        mouse_place_at((g_watch.size[g_watch.lit][0] >> 1) + r[0],
                       (g_watch.size[g_watch.lit][1] >> 1) + r[1]);
    }
}

/* 0x5e948's second half: the bar read, its action done (a new one: the bar drawn again,
   its look let go) */
static void bar_step(void)
{
    g_bar.action = 0;
    bar_input();
    if (!g_bar.action) {
        g_bar.last_action = g_bar.repeats = 0;
        return;
    }
    if (g_bar.action == g_bar.last_action)
        g_bar.repeats++;
    else {
        g_bar.redraw = 1;
        g_bar.repeats = 0;
        g_bar.pressed = 0;
    }
    if (g_bar.action >= 1 && g_bar.action <= 4) {
        seek(g_bar.action, g_bar.repeats);
        g_replay.auto_camera = 0;
    } else if (g_bar.repeats == 0 && g_bar.action == 10 && g_drive.state != 4) {
        camera_next(g_replay.cam);
        /* (held: a frame from the new camera) */
        if (g_drive.state == 2 && g_replay.speed == 0 && g_replay.hold < 1)
            g_replay.hold = 2;
        g_replay.auto_camera = 0;
    } else if (g_bar.action >= 5 && g_bar.action <= 9)
        speed_set(g_bar.action, g_bar.repeats);
    g_bar.last_action = g_bar.action;
}

void replay_step(void)
{
    /* 0x5e948: the mode asked for */
    if (g_replay.mode) {
        if (g_replay.mode == 4) {
            replay_load();
            g_replay.mode = 3;
        }
        if (g_replay.mode == 5 || g_replay.mode == 8) {
            if (g_replay.mode == 8)
                g_replay.attract = 1;
            replay_load();
            g_replay.mode = 2;
        }
        if (g_replay.mode >= 1 && g_replay.mode <= 3) {
            begin(g_replay.mode);
            g_replay.mode = 0;
        } else if (g_replay.mode == 7) {
            /* (the original sends Done again each step Done stays held, each time
               running fast from the start again to the same end; the port reads the keys
               and the mouse only between frames, none of which come while running fast:
               once on the way is enough) */
            if (!g_replay.from_menu) {
                if (!(g_drive.state == 3 && g_replay.resume == 1))
                    back_to_race();
            } else
                g_replay.end = g_replay.attract ? 0x40 : 0x20;
            g_replay.auto_camera = 0;
            g_replay.mode = 0;
        } else if (g_replay.mode == 6)
            g_replay.end = 0x20;
    }
    if (g_drive.state != 1)
        bar_step();
    else if (g_drive.viewed != g_player || g_timing.stopped)
        watch_step();
    if (!(g_drive.flags & 4))
        return;
    g_replay.pos = g_drive.steps % RING_COUNTS;
    switch (g_drive.state) {
    case 1:
        record_step();
        g_replay.last = g_drive.steps;
        break;
    case 2:
        playing();
        g_replay.plays++;
        break;
    case 3:
        if (g_replay.target <= g_drive.steps) {
            g_drive.state = 2;
            rsnd_engine_restart();
            g_replay.plays++;
            g_replay.restart = 1;
        }
        if (g_drive.steps < g_replay.stop)
            play_step();
        break;
    case 4:
        step_flags();
        break;
    }
}
