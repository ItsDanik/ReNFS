/* The race's motion smoothed between its steps (the port's own; smooth.h). */
#include "smooth.h"
#include "mister.h"
#include "car.h"
#include "cardraw.h"
#include "files.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_smooth = 1;

typedef struct {
    vec3 pos;
    int pitch, yaw, roll, yaw_drawn, body_pitch, body_roll;
    int own_turn, turn[9];
    const race_car *car;
} car_kept;

typedef struct {
    vec3 pos;
    int pitch, yaw, roll, mode;
    const race_car *car;
} cam_kept;

static car_kept g_before[RACE_CARS_MAX], g_after[RACE_CARS_MAX];
static cam_kept g_cam_before, g_cam_after;
static int g_rpm_before, g_steer_before, g_t, g_have, g_applied;

/* a step's moves further than these are jumps, not motion (16 m; an eighth of a turn;
   a quarter of a matrix's unit) */
#define JUMP_POS 0x100000
#define JUMP_ANGLE 0x200000
#define JUMP_TURN 0x4000

static int lerp(int a, int b, int t)
{
    return a + (int)(((long long)b - a) * t >> 16);
}

/* an angle (0x1000000 a turn) the short way round; its value at t = 1 is b's own */
static int lerp_angle(int a, int b, int t)
{
    int d = (int)((unsigned)(b - a) << 8) >> 8;
    return b - (int)((long long)d * (0x10000 - t) >> 16);
}

static int angle_jump(int a, int b)
{
    int d = (int)((unsigned)(b - a) << 8) >> 8;
    return d > JUMP_ANGLE || d < -JUMP_ANGLE;
}

static int pos_jump(const vec3 *a, const vec3 *b)
{
    long long dx = (long long)b->x - a->x, dy = (long long)b->y - a->y,
              dz = (long long)b->z - a->z;
    return llabs(dx) > JUMP_POS || llabs(dy) > JUMP_POS || llabs(dz) > JUMP_POS;
}

void smooth_load(void)
{
    const char *e = getenv("RENFS_SMOOTH");

    g_smooth = e ? atoi(e) != 0 : files_cfg_get("smooth", 1) != 0;
}

static void smooth_save(void)
{
    if (!getenv("RENFS_SMOOTH"))
        files_cfg_set("smooth", g_smooth);
}

void smooth_keys(void)
{
    static int held;
    int down = key_down(0x57);

#ifdef MISTER_HYBRID
    if (mister_smooth() >= 0 && !getenv("RENFS_SMOOTH")) {
        g_smooth = mister_smooth();    /* (the MiSTer's: the OSD's Smooth Motion, not F11) */
        return;
    }
#endif
    if (down && !held) {
        g_smooth = !g_smooth;
        smooth_save();
    }
    held = down;
}

static void car_take(car_kept *k, const race_car *c)
{
    k->pos = c->pos;
    k->pitch = c->pitch;
    k->yaw = c->yaw;
    k->roll = c->roll;
    k->yaw_drawn = c->yaw_drawn;
    k->body_pitch = c->body_pitch;
    k->body_roll = c->body_roll;
    k->own_turn = c->own_turn;
    memcpy(k->turn, c->turn, sizeof k->turn);
    k->car = c;
}

static void car_put(race_car *c, const car_kept *k)
{
    c->pos = k->pos;
    c->pitch = k->pitch;
    c->yaw = k->yaw;
    c->roll = k->roll;
    c->yaw_drawn = k->yaw_drawn;
    c->body_pitch = k->body_pitch;
    c->body_roll = k->body_roll;
    c->own_turn = k->own_turn;
    memcpy(c->turn, k->turn, sizeof c->turn);
}

static void cam_take(cam_kept *k, const race_camera *cam)
{
    k->pos = cam->pos;
    k->pitch = cam->pitch;
    k->yaw = cam->yaw;
    k->roll = cam->roll;
    k->mode = cam->mode;
    k->car = cam->car;
}

static void cam_put(race_camera *cam, const cam_kept *k)
{
    cam->pos = k->pos;
    cam->pitch = k->pitch;
    cam->yaw = k->yaw;
    cam->roll = k->roll;
}

void smooth_reset(void)
{
    g_have = 0;
}

void smooth_keep(const race_camera *cam)
{
    int i;

    for (i = 0; i < g_ncars; i++)
        car_take(&g_before[i], g_cars[i]);
    cam_take(&g_cam_before, cam);
    g_rpm_before = g_ncars ? g_cars[0]->rpm : 0;
    g_steer_before = g_ncars ? g_cars[0]->steer : 0;
    g_have = 1;
}

/* car c between k (before the step) and now */
static void car_between(race_car *c, const car_kept *k, int t)
{
    int i;

    if (k->car != c || pos_jump(&k->pos, &c->pos) || k->own_turn != c->own_turn
        || angle_jump(k->yaw, c->yaw) || angle_jump(k->pitch, c->pitch)
        || angle_jump(k->roll, c->roll) || angle_jump(k->yaw_drawn, c->yaw_drawn))
        return;
    for (i = 0; i < 9; i++)
        if (abs(c->turn[i] - k->turn[i]) > JUMP_TURN)
            return;
    c->pos.x = lerp(k->pos.x, c->pos.x, t);
    c->pos.y = lerp(k->pos.y, c->pos.y, t);
    c->pos.z = lerp(k->pos.z, c->pos.z, t);
    c->pitch = lerp_angle(k->pitch, c->pitch, t);
    c->yaw = lerp_angle(k->yaw, c->yaw, t);
    c->roll = lerp_angle(k->roll, c->roll, t);
    c->yaw_drawn = lerp_angle(k->yaw_drawn, c->yaw_drawn, t);
    c->body_pitch = lerp(k->body_pitch, c->body_pitch, t);
    c->body_roll = lerp(k->body_roll, c->body_roll, t);
    for (i = 0; i < 9; i++)
        c->turn[i] = lerp(k->turn[i], c->turn[i], t);
}

void smooth_apply(race_camera *cam, int t)
{
    const cam_kept *k = &g_cam_before;
    int i;

    g_applied = 0;
    if (!g_smooth || !g_have)
        return;
    if (t < 0)
        t = 0;
    if (t > 0x10000)
        t = 0x10000;
    g_t = t;
    for (i = 0; i < g_ncars; i++) {
        car_take(&g_after[i], g_cars[i]);
        car_between(g_cars[i], &g_before[i], t);
    }
    cam_take(&g_cam_after, cam);
    if (k->mode == cam->mode && k->car == cam->car && !pos_jump(&k->pos, &cam->pos)
        && !angle_jump(k->yaw, cam->yaw) && !angle_jump(k->pitch, cam->pitch)
        && !angle_jump(k->roll, cam->roll)) {
        cam->pos.x = lerp(k->pos.x, cam->pos.x, t);
        cam->pos.y = lerp(k->pos.y, cam->pos.y, t);
        cam->pos.z = lerp(k->pos.z, cam->pos.z, t);
        cam->pitch = lerp_angle(k->pitch, cam->pitch, t);
        cam->yaw = lerp_angle(k->yaw, cam->yaw, t);
        cam->roll = lerp_angle(k->roll, cam->roll, t);
    }
    g_applied = 1;
}

void smooth_restore(race_camera *cam)
{
    int i;

    if (!g_applied)
        return;
    for (i = 0; i < g_ncars; i++)
        car_put(g_cars[i], &g_after[i]);
    cam_put(cam, &g_cam_after);
    g_applied = 0;
}

int smooth_rpm(int now)
{
    return g_applied ? lerp(g_rpm_before, now, g_t) : now;
}

int smooth_steer(int now)
{
    return g_applied ? lerp_angle(g_steer_before, now, g_t) : now;
}
