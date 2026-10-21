/* The player's car driven (NOTES §4.6). 16.16 fixed point as the original: its
   multiplies round by the last bit shifted out (mulr), its divisions by powers of two
   either round towards zero (the compiler's signed division: / here) or down (sar:
   >> here), each as the machine code does. */
#include "drive.h"
#include "ff.h"
#include "config.h"
#include "replay.h"
#include "hud.h"
#include "sound.h"
#include "cardraw.h"
#include "crash.h"
#include "opponent.h"
#include "racesnd.h"
#include "exe.h"
#include "track.h"
#include "files.h"
#include "unpack.h"
#include "video.h"
#include "joystick.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

drive_race g_drive;

/* 0x12cc00: grip (8.8), drag, rolling drag, kind; a row a surface */
int g_surfaces[4][4];

/* a 32-bit word of the car's physics (its PBS, +0x479) */
static int S(const race_car *c, int at)
{
    const unsigned char *p = c->spec + at;
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

static int mulr(int a, int b)
{
    long long p = (long long)a * b;
    return (int)(p >> 16) + (int)((p >> 15) & 1);
}

static int iabs(int v)
{
    return v < 0 ? (int)(0u - (unsigned)v) : v;
}

/* a road node's heading, 0x1000000 a turn (its +0x16 word's high half << 10) */
static int node_heading(int node)
{
    return (int)((unsigned)(node_i32(node, 0x16) >> 16) << 10);
}

/* ... and the same between -half and +half a turn */
static int node_heading_signed(int node)
{
    int h = node_heading(node);
    return h > 0x800000 ? h - 0x1000000 : h;
}

static int wrap_half(int a)
{
    if (a > 0x800000)
        return a - 0x1000000;
    if (a < -0x800000)
        return a + 0x1000000;
    return a;
}

/* rotate2d 0x3e918: (x, y) turned by a (0x1000000 a turn) */
static void turn2(int x, int y, int a, int *out_x, int *out_y)
{
    int c = ea_cos(a >> 8), s = ea_sin(a >> 8);

    *out_x = mulr(c, x) - mulr(s, y);
    *out_y = mulr(s, x) + mulr(c, y);
}

void drive_surfaces(void)
{
    static const int base[4][4] = {
        { 0x100, 0x100, 0x3333, 0 }, { 0xcc, 0xf00, 0x5999, 1 }, { 0xcc, 0xf00, 0x5999, 1 },
        { 0, 0, 0, 0 } };
    int i;

    if (g_drive.kind == 2 && g_drive.segment == 2) {
        /* (the third row's kind kept from before) */
        int keep = g_surfaces[2][3];
        static const int t[3][4] = { { 0x100, 0x100, 0x3333, 0 }, { 0x4c, 0x100, 0x1999, 1 },
                                     { 0x4c, 0x100, 0x1999, 0 } };
        memcpy(g_surfaces, t, sizeof t);
        g_surfaces[2][3] = keep;
    } else if (g_drive.kind >= 4) {
        static const int t[3][4] = { { 0x100, 0x100, 0x3333, 0 }, { 0x133, 0x100, 0x3333, 1 },
                                     { 0x133, 0x100, 0x3333, 1 } };
        memcpy(g_surfaces, t, sizeof t);
    } else if (g_drive.kind == 3) {
        static const int t[3][4] = { { 0x100, 0x100, 0x3333, 0 }, { 0x100, 0x1400, 0x3333, 1 },
                                     { 0x100, 0x1400, 0x3333, 1 } };
        memcpy(g_surfaces, t, sizeof t);
    } else
        memcpy(g_surfaces, base, 3 * sizeof base[0]);
    if (g_drive.rally)
        for (i = 0; i < 3; i++) {
            g_surfaces[i][0] = 0x180;
            g_surfaces[i][1] = 0x80;
            g_surfaces[i][2] = 0x1999;
            g_surfaces[i][3] = 1;
        }
}

/* ---- the controls ---------------------------------------------------------------- */

void drive_default_keys(void)
{
    static const int keys[8] = { 0x4b, 0x4d, 0x48, 0x50, 0x1e, 0x2c, 0x39, 0x23 };

    memcpy(g_drive.keys, keys, sizeof keys);
    g_drive.map_key = 0x32;    /* M */
}

/* key_scan_code 0x36758: in the German game the keys were typed through the German
   keyboard's layout; each becomes the US character of the same key (Y and Z change
   places), 0 if it has none */
static int us_char(int c)
{
    static const struct { unsigned char de, us; } t[] = {
        { 0x21, 0x30 }, { 0x22, 0x32 }, { 0x23, 0x5c }, { 0x24, 0x34 }, { 0x25, 0x35 },
        { 0x26, 0x36 }, { 0x27, 0x5c }, { 0x28, 0x38 }, { 0x29, 0x39 }, { 0x2a, 0x5d },
        { 0x2b, 0x5d }, { 0x2c, 0x2c }, { 0x2d, 0x2f }, { 0x2e, 0x2e }, { 0x2f, 0x37 },
        { 0x30, 0x3d }, { 0x31, 0x31 }, { 0x32, 0x32 }, { 0x33, 0x33 }, { 0x34, 0x34 },
        { 0x35, 0x35 }, { 0x36, 0x36 }, { 0x37, 0x37 }, { 0x38, 0x38 }, { 0x39, 0x39 },
        { 0x3a, 0x2e }, { 0x3b, 0x2c }, { 0x3d, 0x3d }, { 0x3f, 0x2d }, { 0x40, 0x51 },
        { 0x5b, 0x38 }, { 0x5c, 0x2d }, { 0x5d, 0x39 }, { 0x5f, 0x2f }, { 0x7b, 0x37 },
        { 0x7d, 0x3d }, { 0x7e, 0x5d }, { 0x81, 0x5b }, { 0x84, 0x27 }, { 0x8e, 0x27 },
        { 0x94, 0x3b }, { 0x99, 0x3b }, { 0x9a, 0x5b }, { 0xe1, 0x2d },
    };

    if ((c >= 0x21 && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x9a)
        || c == 0xe1) {
        for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
            if (t[i].de == c)
                return t[i].us;
        return 0;
    }
    return c == 'Y' ? 'Z' : c == 'Z' ? 'Y' : c;
}

int drive_key_scan(int code)
{
    /* key_scancode 0x91d60: the first place of the character in EA's five tables of the
       keys' characters (0xf7b74, 0x5b keys each), a key of the extended block by its
       code's high byte with bit 7 */
    const unsigned char *t = exe_at(0xf7b74, 0x1c7);
    int c;

    if (g_german)
        code = us_char(code);
    c = code & 0xff00 ? (code >> 8 & 0xff) | 0x80 : code & 0xff;
    for (int i = 0; t && i < 0x1c7; i++)
        if (t[i] == c)
            return i % 0x5b;
    return 0;
}

void drive_keys_from_config(void)
{
    /* 0x36a0c: left, right, faster, brake, gear up, gear down, handbrake (+0x5f3c ..
       +0x5f48), the horn (+0x5f4c); the map's (+0x5f4e) */
    static const int at[8] = { 0x5f3c, 0x5f3e, 0x5f40, 0x5f42, 0x5f44, 0x5f46, 0x5f48, 0x5f4c };

    for (int i = 0; i < 8; i++)
        g_drive.keys[i] = drive_key_scan(config_word(at[i]) & 0xffff);
    g_drive.map_key = drive_key_scan(config_word(0x5f4e) & 0xffff);
}

/* 0x47b78: the mouse across (kept in 0..256 by 0..64) as the steering, 0..0x3c with 0x20
   from 0x1e to 0x21, while the player's own car is watched and races (else straight on,
   the buttons as last read) */
static int g_mouse_btn;                    /* 0x12d9e8 */

static int mouse_steer(int mirrored)
{
    int x, s;

    if (g_drive.viewed != g_player || g_timing.stopped)
        return 0x20;
    mouse_relative(1);                     /* (the port: its movement, not its place) */
    mouse_limits(0, 0, 256, 64);
    g_mouse_btn = mouse_read(&x, 0);
    s = x >> 2;
    if (mirrored)
        s = 0x40 - s;
    return s > 0x1d && s < 0x22 ? 0x20 : s > 0x3c ? 0x3c : s < 0 ? 0 : s;
}

unsigned drive_keyboard(int mirrored)
{
    int p = g_player, sk = g_drive.steer_kind[p], pk = g_drive.pedal_kind[p];
    int gk = g_drive.gear_kind[p], held, mouse = 0;
    unsigned steer = 0x20, pedal = 0x1e, b = 0;

    joy_race_read();                       /* 0x4e0b8 */
    held = g_joy.held;
    /* (the port's: a control left on the keys takes a calibrated joystick too, when no
       key is held, as its own kind would read it; car_steer and car_pedals then treat
       it as keys: past half way, full) */
    if (sk == 0) {
        int left = key_down(g_drive.keys[0]), right = key_down(g_drive.keys[1]);
        if (left != right)
            steer = left != !mirrored ? 0x3c : 0;
        else if (!left && g_joy.on)
            steer = (unsigned)(mirrored ? 0x20 - g_joy.steer : g_joy.steer + 0x20);
    } else if (sk == 1) {
        int v = g_joy.steer;
#ifdef MISTER_HYBRID
        /* (the MiSTer's: the keyboard's arrows full lock, as the stick all the way) */
        int left = key_down(g_drive.keys[0]), right = key_down(g_drive.keys[1]);
        if (left != right)
            v = g_joy.steer = left ? -0x20 : 0x1f;
#endif
        if (mirrored && (v == 0x1f || v == -0x20))
            v = ++g_joy.steer;
        steer = (unsigned)(mirrored ? 0x20 - v : v + 0x20);
    } else {
        steer = (unsigned)mouse_steer(mirrored);
        mouse = 1;
    }
    if (pk == 0) {
        if (key_down(g_drive.keys[2]))
            pedal = 5;
        if (key_down(g_drive.keys[3]))
            pedal = 0x3c;
        if (pedal == 0x1e && g_joy.on)
            pedal = (unsigned)(g_joy.pedal + 0x1e);
    } else if (pk == 1) {
        pedal = (unsigned)(g_joy.pedal + 0x1e);
#ifdef MISTER_HYBRID
        if (key_down(g_drive.keys[2]))     /* (the MiSTer's: the keys as the pedals all the way) */
            pedal = 5;
        if (key_down(g_drive.keys[3]))
            pedal = 0x3c;
#endif
    }
    else if (pk == 3) {
        if (held & 1)
            pedal = 5;
        if (held & 2)
            pedal = 0x3c;
    } else {
        if (!mouse)
            mouse_steer(mirrored);
        mouse = 1;
        if (g_mouse_btn == 1)
            pedal = 5;
        if (g_mouse_btn == 2)
            pedal = 0x3c;
    }
    if (gk == 0) {
        if (key_down(g_drive.keys[4]))
            b = 1;
        else if (key_down(g_drive.keys[5]))
            b = 2;
    } else if (gk == 3) {
        if (held & 1)
            b = 1;
        else if (held & 2)
            b = 2;
    } else {
        if (!mouse)
            mouse_steer(mirrored);
        if (g_mouse_btn == 1)
            b |= 1;
        else if (g_mouse_btn == 2)
            b |= 2;
    }
    if (!b && g_joy.on)                    /* (the port's: a pad's shoulders) */
        b = g_joy.extra & 1 ? 1 : g_joy.extra & 2 ? 2 : 0;
    if (key_down(0x30) && key_down(0x13) && key_down(0x11) && g_cars[g_player]
        && g_cars[g_player]->gearbox)
        b |= 3;
    if (key_down(g_drive.keys[6]) || (g_joy.on && (held & 8)))
        b |= 8;
    if (key_down(g_drive.keys[7]) || (g_joy.on && pk != 3 && gk != 3 && (held & 1)))
        b |= 4;
    return b << 12 | pedal << 6 | steer;
}

/* 0x10010: the turn towards the road six nodes ahead, a lane's width (2 m) to the side
   the car is of its node's middle when it is more than 0.7 m off it */
static int toward_road(const race_car *c)
{
    int a = node_heading(c->node) >> 8, off, side, a6, x, z;
    int n6 = c->node + 6;

    off = mulr(ea_sin(a), node_i32(c->node, 0x10) - c->pos.z)
        - mulr(ea_cos(a), node_i32(c->node, 8) - c->pos.x);
    side = off < -0xb333 ? -0x20000 : off > 0xb333 ? 0x20000 : 0;
    a6 = node_heading(n6) >> 8;
    x = node_i32(n6, 8) + mulr(ea_cos(a6), side);
    z = node_i32(n6, 0x10) - mulr(ea_sin(a6), side);
    return wrap_half(angle_of(z - c->pos.z, x - c->pos.x) - c->yaw);
}

/* car_steer 0x483e4: the keys' steering, full lock either way or none; with none the
   wheels follow `toward` when it is small for the speed */
static void steer_keys(race_car *c, unsigned word, int toward)
{
    int in = (int)(word & 0x3f) - 0x20, target, rate, d;

    if (in >= 1)
        in++;
    target = in > 15 ? 0x1b0000 : in < -15 ? -0x1b0000 : 0;
    if (c->speed >= 0 && target == 0 && iabs(toward) < S(c, 0x2ec) * (c->speed >> 16))
        target = toward;
    d = (iabs(c->speed) >> 16) * S(c, 0x310);
    if (d > 0x18000)
        d = 0x18000;
    rate = S(c, 0x30c) - d;
    if (rate > 0x19999)
        rate = 0x19999;
    rate *= S(c, 0x300);
    if (c->speed > 0 && iabs(toward) < 0x400000) {
        if ((toward > c->steer && target > c->steer) || (toward < c->steer && target < c->steer)) {
            rate <<= S(c, 0x314) & 31;
            if (!target)
                rate *= 2;
        } else {
            rate >>= S(c, 0x318) & 31;
            if (!target)
                rate /= 2;
        }
    }
    d = iabs(c->steer - target);
    if (d < rate)
        rate = d;
    if (target > c->steer)
        c->steer += rate;
    else if (target < c->steer)
        c->steer -= rate;
    else
        c->steer = target;
}

/* car_steer_2 0x4853c: a joystick's, the turn from the table 0xec538 by how far over */
static void steer_slope(race_car *c, unsigned word, int toward)
{
    int in = (int)(word & 0x3f) - 0x20, target, slow, want, other, step;

    if (in >= 1)
        in++;
    target = exe_i32(0xec538 + 4 * iabs(in));
    if (in < 0)
        target = -target;
    if (c->speed < 0)
        toward = 0;
    slow = c->speed > 0x120000 ? (c->speed - 0x120000) / 128 : 0;
    want = target - c->steer;
    other = toward - c->steer;
    if (want > 0 && other > 0)
        step = want > other ? other : want;
    else if (want < 0 && other < 0)
        step = want < other ? other : want;
    else if (want > 0) {
        step = (want > 0xc0000 ? 0x40000 : 0x18000) - slow;
        if (step > want)
            step = want;
    } else {
        step = -((want < -0xc0000 ? 0x40000 : 0x18000) - slow);
        if (step < want)
            step = want;
    }
    c->steer += step;
}

/* car_throttle 0x48600: towards `to`, by the car's rates up (+0x33c) and down (+0x340) */
static void throttle_to(race_car *c, int to)
{
    if (to > c->throttle && to - c->throttle > S(c, 0x33c))
        c->throttle += S(c, 0x33c);
    else if (to < c->throttle && c->throttle - to > S(c, 0x340))
        c->throttle -= S(c, 0x340);
    else
        c->throttle = to;
}

/* car_brake 0x48658: the same for the brake, its rates a quarter more on (below 0x90
   +0x344, else +0x348) and +0x34c / +0x350 off */
static void brake_to(race_car *c, int to)
{
    int r;

    if (to > c->braking) {
        r = S(c, c->braking < 0x90 ? 0x344 : 0x348);
        r += r / 4;
        if (to - c->braking > r) {
            c->braking += r;
            return;
        }
    } else if (to < c->braking) {
        r = S(c, c->braking < 0x90 ? 0x34c : 0x350);
        if (c->braking - to > r) {
            c->braking -= r;
            return;
        }
    } else
        return;
    c->braking = to;
}

/* car_shift 0x48714: req 1 a gear down, 2 up, 0 none; acted on as it changes. The
   automatic's lever: 1 reverse, 2 neutral, 3 drive */
static void shift(race_car *c, int req)
{
    if (g_drive.steps < 300) {
        c->shift_last = req;
        return;
    }
    if (req) {
        if (c->gearbox >= 1) {
            if (c->shift_last != 1 && req == 1 && c->gearbox >= 2) {
                c->gearbox -= req;
                if (c->gearbox == 2) {
                    c->gear = -1;
                    c->clutch = 0;
                } else {
                    c->gear = 0;
                    c->clutch = req;
                    c->shifting = 0;
                }
            } else if (c->shift_last != 2 && req == 2 && c->gearbox <= req) {
                c->gearbox++;
                if (c->gearbox == req) {
                    c->clutch = 0;
                    c->gear = -1;
                } else {
                    c->gear = 0;
                    c->clutch = 0;
                    c->shifting = 1;
                    c->shift_wait = S(c, 0x354) + 1;
                    c->throttle_kept = c->throttle;
                }
            }
        } else if (c->shift_last != 1 && req == 1 && c->gear >= -1) {
            c->clutch = 0;
            c->gear -= req;
            c->shifting = req;
            c->shift_wait = S(c, 0x354) / 2;
            c->throttle_kept = c->throttle;
        } else if (c->shift_last != 2 && req == 2 && S(c, 0x58) - 3 > c->gear) {
            c->clutch = 0;
            c->gear++;
            /* (the 512TR now and then a little slower) */
            c->shift_wait = S(c, 0x354)
                + (c->number == 4 && (g_drive.steps & 0x31) == 0x10 ? 3 : 0);
            c->shifting = 1;
            c->throttle_kept = c->throttle;
        }
    }
    c->shift_last = req;
    if (c->gearbox == 0 && c->shift_wait > -1) {
        if (c->shift_wait == 0) {
            c->shifting = 0;
            c->throttle = c->throttle_kept;
            if (c->gear != -1)
                c->clutch = 1;
        }
        c->shift_wait--;
    }
}

/* the gear request and the two buttons in a control word's bits 12..15 */
static void buttons(race_car *c, unsigned word)
{
    unsigned b = word >> 12 & 0xf;

    shift(c, b & 2 ? 1 : b & 1 ? 2 : 0);
    c->handbrake = (b & 8) != 0;
    c->horn = (b & 4) != 0;
}

/* car_pedals 0x48990: the keys' pedals, all or nothing */
static void pedals_keys(race_car *c, unsigned word)
{
    int in = (int)(word >> 6 & 0x3f) - 0x1e;

    buttons(c, word);
    if (in >= 1)
        in++;
    if (in < -15) {
        c->braking = 0;
        throttle_to(c, 0xff);
    } else if (in > 15) {
        brake_to(c, 0xff);
        throttle_to(c, 0);
    } else {
        if (c->braking > 100)
            c->braking = 100;
        if (c->throttle)
            throttle_to(c, 0);
        else if (c->braking)
            brake_to(c, 0);
    }
}

/* car_pedals_2 0x48a80: a joystick's, by how far */
static void pedals_slope(race_car *c, unsigned word)
{
    int in = (int)(word >> 6 & 0x3f) - 0x1e, to = 0, up = 0x4c, down;

    buttons(c, word);
    if (in >= 1)
        in++;
    if (in <= 0) {
        to = -in * 0x889 / 256;
        c->braking = 0;
    } else {
        c->throttle = 0;
        if (c->abs)
            c->braking = in * 0x889 / 256;
        else
            brake_to(c, in * 0x889 / 256);
    }
    down = c->clutch ? 0x33 : 0xc;
    if (to > c->throttle && to - c->throttle > up)
        c->throttle += up;
    else if (to < c->throttle && c->throttle - to > down)
        c->throttle -= down;
    else if (to != c->throttle)
        c->throttle = to;
    if (c->throttle > 0xff)
        c->throttle = 0xff;
    if (c->braking > 0xff)
        c->braking = 0xff;
}

/* car_race_progress 0x5162c: 1 before the start (an open road's car kept past its
   first 1.1 m), 2 past the finish, else 0. (The network's check first: not here) */
int drive_race_progress(race_car *c)
{
    if (g_track.loop) {
        if (c->lap < 1)
            return 1;
        return c->lap > g_drive.laps ? 2 : 0;
    }
    if (c->node < 12) {
        if (c->pos.z < 0x120000)
            c->pos.z = 0x120000;
        return 1;
    }
    return c->node > g_drive.last_node ? 2 : 0;
}

/* 0x10180: the car drives itself (before the start, past the finish): turned towards
   the road, slowed, its wheels at full lock until it faces along it */
static void drive_itself(race_car *c)
{
    int d = iabs(node_heading(c->node) - c->yaw);

    if (d > 0x800000)
        d = 0x1000000 - d;
    if (iabs(d - 0x400000) < 0x80000 || c->auto_mode == 2) {
        int slow = c->yaw_rate / 8;

        c->handbrake = 0;
        c->braking = 0xfa;
        c->throttle = 0;
        c->speed = 0;
        c->yaw_rate -= slow;
        if (c->side_speed > 0) {
            if (c->auto_steer > -0x190000)
                c->auto_steer -= 0x30000;
        } else if (c->auto_steer < 0x190000)
            c->auto_steer += 0x30000;
        c->steer = c->auto_steer;
        c->auto_mode = 2;
    } else {
        if (c->auto_mode == 0) {
            int a = node_heading(c->node) >> 8;

            c->turn_count = 0;
            c->auto_steer = 0;
            c->turn_target = ((node_i32(c->node, 0x10) - c->pos.z) >> 8) * (ea_sin(a) >> 8)
                - ((node_i32(c->node, 8) - c->pos.x) >> 8) * (ea_cos(a) >> 8);
        }
        c->throttle = 0;
        c->handbrake = 1;
        c->auto_mode = 1;
        {
            const unsigned char *nd = node_ptr(c->node);
            int left = iabs((int)((unsigned)nd[2] << 13) + c->turn_target);
            int right = iabs(c->turn_target - (int)((unsigned)nd[3] << 13));

            if (left < right ? c->turn_count >= 6 : c->turn_count < 6)
                c->steer = 0x1e0000;
            else
                c->steer = -0x1e0000;
        }
    }
    c->turn_count++;
    c->side_speed -= c->side_speed / 8;
    c->speed -= c->speed / 8;
}

void drive_controls(race_car *c)
{
    unsigned word = g_drive.controls[c->player];
    int toward = toward_road(c);

    if (iabs(toward) > 0x40)
        toward = 0;
    if (g_drive.steer_kind[c->player] == 0)
        steer_keys(c, word, toward);
    else
        steer_slope(c, word, toward);
    if (g_drive.pedal_kind[c->player] == 1)
        pedals_slope(c, word);
    else
        pedals_keys(c, word);
    if (drive_race_progress(c))
        drive_itself(c);
    else
        c->auto_mode = 0;
}

/* ---- the gearbox and the engine -------------------------------------------------- */

/* the engine's turns a minute at `speed` in gear g (the final drive +0x54, the gear's
   ratio +0x70 + 4g) */
static int rpm_at(const race_car *c, int speed, int g)
{
    return ((speed >> 8) * (S(c, 0x54) >> 8) >> 8) * (S(c, 0x70 + 4 * g) >> 8) >> 16;
}

/* ... and the speed at rpm r: r << 16 over that */
static int speed_at(const race_car *c, int r)
{
    return (int)((unsigned)r << 16) / ((S(c, 0x54) >> 8) * (S(c, 0x70 + 4 * c->gear) >> 8) >> 16);
}

/* 0x3ea98, every second step: the engine's revs follow the throttle (out of gear) or
   the wheels (in gear) */
static void engine_revs(race_car *c)
{
    int target;

    if (c->shifting)
        c->throttle = 0;
    c->rpm_road = rpm_at(c, c->wheel_speed, c->gear);
    if (c->rpm_road < 700)
        c->rpm_road = 700;
    target = S(c, 0xac) * c->throttle >> 8;
    if (!c->clutch) {
        if (c->throttle < 13) {
            int d = c->gear == -1 ? S(c, 0x358) / 2 : S(c, 0x358);

            if (c->rpm > S(c, 0xb0)) {
                c->rpm -= d;
                if (c->rpm < S(c, 0xb0))
                    c->rpm = S(c, 0xb0);
            } else {
                c->rpm += d;
                if (c->rpm > S(c, 0xb0))
                    c->rpm = S(c, 0xb0);
            }
        } else {
            int p = c->throttle * S(c, 0x35c);

            c->rpm += c->gear == -1 ? p / 2 >> 8 : p >> 8;
            if (target < c->rpm)
                c->rpm = target;
        }
    } else if (c->rpm > c->rpm_road) {
        c->rpm -= (c->tyres[1] & 2) && c->throttle > 0xdc ? S(c, 0x360) / 8 : S(c, 0x360);
        if (c->rpm < c->rpm_road)
            c->rpm = c->rpm_road;
    } else {
        if (c->rpm_road + 500 > c->rpm)
            c->rpm += S(c, 0x364);
        else
            c->rpm += S(c, 0x364) * exe_i32(0xec270 + 4 * c->gear) / 256;
        if (c->rpm > c->rpm_road)
            c->rpm = c->rpm_road;
    }
    if (c->gearbox && c->gear == 0 && c->rpm < S(c, 0xb0))
        c->rpm = S(c, 0xb0);
    if (c->rpm > 11000) {
        c->rpm = 11000;
        c->handbrake = 1;
    }
    if (c->rpm_road > 11000)
        c->rpm_road = 11000;
}

/* 0x188a0: the automatic's choice, a gear up past +0x294's revs, down below 15/16 of
   +0x290's */
static void auto_shift(race_car *c)
{
    if (c->gear + 3 != S(c, 0x58) && rpm_at(c, c->speed, c->gear) > S(c, 0x294 + 4 * c->gear)) {
        c->rpm_road = rpm_at(c, c->speed, c->gear + 1);
        c->gear++;
    } else if (c->gear != 0) {
        int r = rpm_at(c, c->speed, c->gear - 1);

        if (r < S(c, 0x290 + 4 * c->gear) * 15 / 16) {
            c->rpm_road = r;
            c->gear--;
        }
    }
}

/* 0x3e9c4, the automatic in drive: the clutch bites when the wait is over, then the
   next shift is chosen */
static void automatic(race_car *c)
{
    int was;

    if (c->shift_wait > 0)
        c->shift_wait--;
    if (c->shift_wait == 0) {
        c->clutch = 1;
        c->shift_wait = -1;
        c->throttle = c->throttle_kept;
        c->shifting = 0;
    }
    if (c->shift_wait >= 0)
        return;
    was = c->gear;
    auto_shift(c);
    if (was != c->gear) {
        c->shifting = 1;
        c->throttle_kept = c->throttle;
        c->clutch = 0;
        c->shift_wait = S(c, 0x354)
            + (c->number == 4 && (g_drive.steps & 0x31) == 0x10 ? 3 : 0);
    }
}

/* 0x18a04: the torque curve (+0x88 points from +0xb4, every 200 rpm) at r */
static int torque_at(const race_car *c, int r)
{
    int i = (r - (r + 100) % 200 + 100 - S(c, 0xb4)) / 200;

    if (i <= 0)
        i = 0;
    if (i >= S(c, 0x88))
        i = S(c, 0x88) - 1;
    return S(c, 0xb8 + 8 * i);
}

/* 0x3ed68: the force the engine puts through the driven wheels, and their speed */
static int engine(race_car *c)
{
    int f = 0, boost = 0, spin = 0, ratio = S(c, 0x70 + 4 * c->gear);

    if (c->clutch) {
        if (g_player == c->player && g_drive.nitro) {
            /* NITRO (not in a network game, 0x15d136): N with a shift or B, two seconds at
               full revs, then two to wait */
            if (g_drive.nitro_keys && !g_drive.nitro_on) {
                g_drive.nitro_on = 1;
                g_drive.nitro_until = g_drive.clock + 200;
                c->rpm = S(c, 0xac) + 1200;
                g_drive.nitro_off = g_drive.clock + 400;
            }
            if (g_drive.nitro_on) {
                if (g_drive.clock < 100)
                    g_drive.nitro_on = 0;
                if (g_drive.clock <= g_drive.nitro_until) {
                    boost = 1;
                    c->throttle = 0xff;
                } else if (g_drive.clock > g_drive.nitro_off)
                    g_drive.nitro_on = 0;
            }
        } else if (c->player < g_humans && g_drive.revving[c->player]) {
            c->throttle = 0xff;
            boost = 1;
            c->rpm = S(c, 0xac) + 1200;
        }
        if (c->rpm_road > c->rpm)
            f = (-((c->rpm_road - c->rpm) << 3) * S(c, 0x368) >> 8) * (ratio >> 8);
        else {
            int target = S(c, 0xac) * c->throttle >> 8;

            if (c->rpm > target && !boost) {
                f = iabs(((iabs((c->rpm - target) * S(c, 0x368)) >> 8) * (ratio >> 8)));
                if (f > iabs(c->speed) << 4)
                    f = -(c->speed << 4);
                else if (c->speed > 0)
                    f = -f;
                else if (c->speed == 0)
                    f = 0;
            } else {
                int d;

                f = torque_at(c, c->rpm) / 256 * (ratio / 256);
                f = S(c, 0x2b8 + 4 * c->gear) * f / 256;
                if (f > 0x70000)
                    f = 0x70000;
                d = speed_at(c, c->rpm) - c->speed;
                if (iabs(d) > 0x30000 && c->throttle > 0xfa && c->gear < 1) {
                    f = d + d / 4;
                    g_drive.spinning = 1;
                } else
                    f = f * c->throttle >> 8;
                if (boost)
                    f <<= 2;
            }
        }
    }
    f = f * (S(c, 0x328) / 64) / 4;
    if (g_drive.rally && g_surfaces[c->surface][3] && iabs(f) > c->grip_rear
        && c->rpm < S(c, 0xac) && c->throttle > 0xf0) {
        spin = 1;
        f <<= 2;
    }
    if ((c->throttle > 0xf0 && iabs(f) > c->grip_rear - c->wheelspin
         && S(c, 0x298) - 500 > c->rpm && S(c, 0x18) == 0) || spin) {
        /* the driven wheels spin up with the engine */
        c->wheel_speed = speed_at(c, c->rpm);
        if (spin && c->rpm + 200 < S(c, 0xac)) {
            c->rpm += 150;
            c->wheel_speed = speed_at(c, c->rpm - 200);
        }
        if (c->speed > 0 ? c->speed > c->wheel_speed : c->speed < c->wheel_speed)
            c->wheel_speed = c->speed;
    } else if (c->wheel_speed > c->speed && c->wheel_speed > 0) {
        c->wheel_speed -= exe_i32(0xec270 + 4 * c->gear) * 15 * 0x20000 / 256;
        if (c->wheel_speed < c->speed)
            c->wheel_speed = c->speed;
    } else
        c->wheel_speed = c->speed;
    if (g_drive.rally && c->throttle > 0xfa && c->gear > 0 && c->speed < 0x190000)
        f *= 2;
    return f;
}

/* ---- the tyres ------------------------------------------------------------------- */

/* 0x3f260: the drag against speed v across (rear 0) or along (1) the car: the air, the
   rolling, the road's surface; rolled over on a circuit, an eighth */
static int drag(race_car *c, int v, int rear)
{
    const int *sf = g_surfaces[c->surface];
    int q = (v >> 16) * (v >> 16), d, r, lim;

    c->surface_flag = sf[3] ? 4 : 0;
    d = S(c, 0x34) * sf[1] / 256 * q;
    if (rear)
        d += (c->downforce[2] + c->downforce[3]) * q;
    d += sf[2];
    r = c->roll;
    if (r > 0x800000)
        r -= 0x1000000;
    if (g_drive.kind > 2 && iabs(r) > 0xa0000)
        d /= 8;
    lim = iabs(c->rate * v);
    if (d > lim)
        d = lim;
    return v >= 0 ? -d : d;
}

/* the larger of a, b and a quarter of the other */
static int mag(int a, int b)
{
    return a > b ? a + (b >> 2) : b + (a >> 2);
}

/* 0x3f56c: locked wheels: the force cut back to the grip, the rest slides */
static void locked(int *side, int *fwd, int grip, int *slide)
{
    int m = mag(iabs(*side), iabs(*fwd)), g, k;

    if (m <= grip || m / 256 == 0)
        return;
    g = grip - grip * 3 / 8;
    k = g / (m / 256);
    *side = *side * k / 256;
    *fwd = *fwd * k / 256;
    *slide = m - g;
}

/* 0x3f60c: rolling wheels: past the grip the force is scaled back (the traction
   control first takes off what spins); the rear's spin kept in +0x40d */
static void rolling(race_car *c, int *side, int *fwd, int grip, int *slide, int rear)
{
    int a = iabs(*side), b = iabs(*fwd), m = mag(a, b), over = 0;

    if (c->tcs_now && m > grip) {
        int cut = m - grip;

        if (b > cut) {
            b -= cut;
            *fwd = *fwd > 0 ? b : -b;
        } else {
            *fwd = 0;
            b = 0;
        }
        m = mag(a, b);
    }
    if (m > grip) {
        int cap = grip * 18 / 16, g = grip, k;

        *slide = m - grip;
        over = (m > cap ? cap : m) - grip;
        if (over > 0)
            g -= over;
        k = m / 256 ? g / (m / 256) : 0;
        if ((c->braking > 0x96 && !c->abs) || c->handbrake) {
            *side = *side * k / 256;
            *fwd = *fwd * k / 256;
        } else {
            if (grip < *side)
                *side = grip;
            else if (-grip > *side)
                *side = -grip;
            if (c->throttle < 0x32) {
                if (grip < *fwd)
                    *fwd = grip;
                else if (-grip > *fwd)
                    *fwd = -grip;
            } else
                *fwd = *fwd * k / 256;
        }
    }
    if (rear && ((*fwd > 0 && c->speed > 0) || (*fwd < 0 && c->speed < 0)))
        c->wheelspin = over;
}

/* 0x3f7b8: an axle's forces (1 the front, turned by the steering a; 2 the rear) from
   the road's push across and along it, the engine's drive and the brakes: out the
   forces the tyres pass on, across and along the car */
static void axle(race_car *c, int *out_side, int *out_fwd, int side_in, int fwd_in, int a,
                 int drive, int brake, int which)
{
    int side, fwd, grip, slip, slipc, skid = 0, *slide;
    unsigned char *flags;

    if (!c->on_ground) {
        *out_fwd = 0;
        *out_side = 0;
        return;
    }
    if (which == 1) {
        turn2(side_in, fwd_in, -a, &side, &fwd);
        flags = &c->tyres[0];
        grip = c->grip_front;
        slide = &c->slide_front;
    } else {
        side = side_in;
        fwd = fwd_in;
        flags = &c->tyres[1];
        grip = c->grip_rear;
        slide = &c->slide_rear;
    }
    *flags = 0;
    slip = slipc = angle_of(iabs(fwd), iabs(side));
    if (slip > S(c, 0xa4)) {
        slipc = S(c, 0xa4);
        *flags |= 1;
    }
    if ((c->braking > 0xf5 && !c->abs)
        || (grip * 5 / 4 < iabs(brake) && (!c->abs || c->handbrake))) {
        if (iabs(fwd) > grip)
            *flags |= 1;
        locked(&side, &fwd, grip, slide);
    } else {
        /* the grip across by the slip angle, the car's curve (+0x374, 0x200 an axle) */
        int lim = mulr((int)c->spec[0x374 + 0x200 * (which - 1)
                                    + ((slipc > 0x1ffffe ? 0x1ffffe : slipc) >> 12)] << 9, grip);
        int both;

        if (iabs(side) > lim) {
            if (iabs(side) > lim * 8 && slipc > 0xf0000) {
                if (slipc > 0x1e0000) {
                    *flags |= 1;
                    skid = iabs(side) - lim * 8;
                } else {
                    skid = (iabs(side) - lim * 8) / 4;
                    *flags |= 4;
                }
            }
            side = side > 0 ? lim : -lim;
        }
        if (fwd <= 0)
            brake = -brake;
        both = drive + brake;
        if (iabs(brake) < iabs(drive)) {
            fwd = both;
            if (iabs(side) + iabs(fwd) > grip) {
                *flags = 2;
                if (iabs(fwd) > grip && g_drive.spinning && !c->tcs)
                    side /= 8;
            }
            rolling(c, &side, &fwd, grip, slide, which == 2);
            if (*slide)
                *slide *= 8;
            if (skid > *slide)
                *slide = skid;
        } else {
            if (iabs(both) < iabs(fwd))
                fwd = fwd > 0 ? iabs(both) : -iabs(both);
            rolling(c, &side, &fwd, grip, slide, which == 2);
            *slide |= skid;
            if (c->abs_now)
                *slide = 0;
        }
    }
    if (*slide > 0x960000)
        *slide = 0x960000;
    if (a)
        turn2(side, fwd, a, out_side, out_fwd);
    else {
        *out_side = side;
        *out_fwd = fwd;
    }
}

/* a road node's slope (its +0x12 word's high half << 10), between -half and +half a
   turn */
static int node_slope(int node)
{
    int a = (int)((unsigned)(node_i32(node, 0x12) >> 16) << 10);
    return a > 0x800000 ? a - 0x1000000 : a;
}

/* 0x4071c: the grip changed by the road's slope changing to the next node (more in a
   dip, less over a crest) and the car's roll, and the downforce */
static void grip_change(race_car *c)
{
    int bend = (node_slope(c->node + 1) - node_slope(c->node)) >> 14;
    int g = mulr(fix_sin(bend), c->speed) * 4, r = c->roll;

    if (g > 0)
        g += g >> 1;
    if (r > 0x800000)
        r -= 0x1000000;
    g += (mulr(iabs(r), iabs(c->speed)) >> 7) * 2;
    c->grip_extra = g;
    c->grip_front += g;
    if (c->grip_front < 0)
        c->grip_front = 0;
    c->grip_rear += c->grip_extra;
    if (c->grip_rear < 0)
        c->grip_rear = 0;
    if (c->speed > 0) {
        int q = (c->speed >> 16) * (c->speed >> 16);
        int front = c->downforce[0] * q, rear = q * c->downforce[1], sum = front + rear;

        c->grip_front += front;
        c->grip_rear += rear;
        c->grip_extra += sum / 4 + sum / 8;
    }
}

/* 0x4008c: what the node's kind says (+7): tunnels and the like */
static void node_kind(race_car *c)
{
    int t = node_ptr(c->node)[7];

    c->node_flags[0] = t == 4 || t == 7 || t == 9 || t == 12 || t == 13;
    c->node_flags[1] = c->node_flags[0] || t == 8;
    c->node_flags[2] = t == 5 || t == 0x12;
    c->node_flags[3] = t == 0xe || t == 0x11;
    c->node_flags[4] = t == 0xf || t == 0x12;
}

/* ---- the road's edges ------------------------------------------------------------ */

/* 0x401d8: how far the car reaches to the side of a road heading h: between half its
   width (square to it) and half its length (along) */
static int reach(const race_car *c, int h)
{
    int d = iabs(h - c->yaw) >> 16;

    if (d > 0xc0)
        d = 0x100 - d;
    else if (d > 0x80)
        d -= 0x80;
    else if (d > 0x40)
        d = 0x80 - d;
    return (((c->half_len - c->half_wid) * d >> 6) + c->half_wid) >> 1;
}

/* 0x3fed4: off a wall at road heading h, side + the right: a glancing touch turns the car
   along it; a hard one (across at v) starts a crash */
static void wall_hit(race_car *c, int h, int v, int side, int kind, int bounce)
{
    int a = (side > 0 ? h - bounce : h + bounce) - c->yaw, b;

    if (a < 0)
        a += 0x1000000;
    b = a > 0x800000 ? 0x1000000 - a : a;
    if (b > 0x400000) {
        a += 0x800000;
        b = 0x1000000 - a;
    }
    if (a > 0x1000000)
        a -= 0x1000000;
    if ((!g_drive.rally && v >= 0x180000 && c->speed_abs > 0x230000)
        || (g_drive.rally && v >= 0x220000 && c->speed_abs > 0x2d0000)) {
        /* the crash: the hit's push into the body's turn, by how square the car met the
           wall (a) and how much faster than 6 m/s (up to 14), the wall's kind deciding
           which way it rolls */
        int s = v - 0x60000, t, q;

        t = a < 0x800000 ? a / 8 : 0x1000000 - (0x1000000 - a) / 8;
        if (t > 0x800000)
            t -= 0x1000000;
        if (s > 0xe0000)
            s = 0xe0000;
        q = (b >> 16) * s / 64;
        if (t > 0)
            t = s - q;
        else {
            t = q - s;
            s = -s;
        }
        if (kind == 0) {
            t = -t;
        } else
            q = -q;
        if (s)
            crash_hard_hit(c, s / 4, t, q * 3 / 4);
        return;
    }
    if (iabs(b) < 0x130000 && c->speed > 0
        && (side > 0 ? a > 0xed0000 : a < 0x130000))
        c->yaw += a;
}

/* 0x40230: the road's edges: past a lane's edge the surface is the verge's (the node's
   +6 nibbles), past the wall (+2, +3) the car is put back and bounced off it; the
   watched car's scrape heard off a wall of kind 0 (from the seat, to that side) */
static void road_edges(race_car *c)
{
    const unsigned char *n = node_ptr(c->node);
    int h = node_heading(c->node), side = 0, in = 0, kind = 0, off;

    off = ((node_i32(c->node, 0x10) - c->pos.z) >> 8) * (ea_sin(h >> 8) >> 8)
        - (ea_cos(h >> 8) >> 8) * ((node_i32(c->node, 8) - c->pos.x) >> 8);
    if (-(n[0] << 13) > off) {
        int w;

        c->surface = n[6] >> 4;
        w = reach(c, h) - (n[2] << 13);
        if (w > off) {
            side = -0x100;
            in = w + 0x4ccc - off;
            kind = n[5] >> 4;
            if (g_drive.viewed == c->player)
                ff_wall(0);    /* (0x4037f) */
        }
    } else if (n[1] << 13 < off) {
        int w;

        c->surface = n[6] & 15;
        w = (n[3] << 13) - reach(c, h);
        if (w < off) {
            side = 0x100;
            in = off - (w - 0x4ccc);
            kind = n[5] & 15;
            if (g_drive.viewed == c->player)
                ff_wall(1);    /* (0x4042a) */
        }
    } else
        c->surface = exe_i32(0xeab08);
    if (!side)
        return;
    {
        int across, along, dx, dz, v, s;

        across = side * iabs(in) / 256;
        turn2(across, 0, h, &dx, &dz);
        c->pos.x -= dx;
        c->pos.z += dz;
        turn2(c->vel.x, c->vel.z, -h, &across, &along);
        v = iabs(across);
        if (!kind && v > 0x60000 && g_drive.viewed == c->player) {
            int angle, dist;
            if (g_camera->mode <= 1) {
                angle = off > 0 ? 0xd70000 : 0x280000;
                dist = 1;
            } else
                rsnd_where(g_cars[g_drive.viewed], &angle, &dist);
            rsnd_hit(2, 9, v * 20, dist, angle);
        }
        if (!kind && v > 0x140000)
            replay_mark(0x32);
        if (v < 0x180000)
            across = across > 0x20000 ? 0x20000 : across < -0x20000 ? -0x20000 : across;
        else
            across /= 4;
        if (along > 0) {
            if (along > 0x30000) {
                along -= g_drive.rally ? v / 2 + v / 8 : v + v / 4;
                if (along < 0)
                    along = 0;
            }
        } else if (along < -0x30000) {
            along += g_drive.rally ? v / 2 : v + v / 4;
            if (along > 0)
                along = 0;
        }
        s = v / 4 > 0xa0000 ? 0xa0000 : v / 4;
        if (side > 0) {
            c->roll_shake = s;
            c->pitch_shake = -s;
            h -= 0x20000;
        } else {
            c->roll_shake = -s;
            c->pitch_shake = -s;
            h += 0x20000;
        }
        c->roll_phase = 0x1e;
        c->pitch_phase = 0x1e;
        turn2(across, along, h, &c->vel.x, &c->vel.z);
        wall_hit(c, h, v, side, kind, kind ? 0x18000 : 0x30000);
        if (c->vel.y > 0)
            c->vel.y = 0;
    }
}

/* ---- a step's driving: car_physics_player 0x408e0 -------------------------------- */

/* 0x417a2: the time trial on Rusty Springs measures the car, near the start and finish
   (nodes up to 0x61 and from 0x1d1): standing (below 0x3333) starts the clock; with the
   throttle down, the steps to 60 and 100 mph and to a quarter mile (0x53 of the road on)
   with the speed then; braking, the steps from 60 or 80 mph to standing. The best of the
   race goes into its statistics, and each onto the status bar as it is made (0x2d71c,
   0x2d778). The clocks are kept from race to race. */
static void rusty_springs_test(race_car *c)
{
    static int braked80, braked60;                   /* 0xec3c4, 0xec3c8 */
    static int best60 = 99999, best100 = 99999, bestq = 99999; /* 0xec3cc, d0, d4 */
    static int start, start_at;                      /* 0xec3d8, 0xec3dc */
    int *st, dt;

    if (c->player >= DRIVE_PLAYERS)
        return;
    st = g_drive.stats[c->player];
    if (c->node > 0x61 && c->node < 0x1d1)
        return;
    if (c->speed < 0x3333) {
        start = g_drive.steps;
        start_at = c->progress;
        best60 = best100 = bestq = 99999;
    }
    dt = g_drive.steps - start;
    if (c->throttle > 0x32 && dt < 0x5dc && dt > 0x64) {
        if (c->speed > 0x1ac937 && dt < best60) {
            best60 = dt;
            hud_accel_times(dt, 0, 0, 0);
            if (dt < st[0])
                st[0] = dt;
        }
        if (c->speed > 0x2ca666 && dt < best100) {
            best100 = dt;
            hud_accel_times(0, dt, 0, 0);
            if (dt < st[1])
                st[1] = dt;
        }
    }
    if (c->progress - start_at > 0x53) {
        dt = g_drive.steps - start;
        start_at = 999999;
        if (dt < bestq && dt < 0x3e8) {
            if (dt < st[5]) {
                st[5] = dt;
                st[4] = c->speed;
            }
            hud_accel_times(0, 0, dt, c->speed);
            bestq = dt;
        }
    }
    if (c->braking > 0x32) {
        if (c->speed > 0x23b5c2)
            braked80 = g_drive.steps;
        if (c->speed > 0x1ac937)
            braked60 = g_drive.steps;
        if (c->speed < 0x1999 && braked60 > 0) {
            dt = g_drive.steps - braked60;
            hud_braking(dt, 0);
            braked60 = 0;
            if (dt < st[3])
                st[3] = dt;
        }
        if (c->speed < 0x1999 && braked80 > 0) {
            dt = g_drive.steps - braked80;
            hud_braking(0, dt);
            braked80 = 0;
            if (dt < st[2])
                st[2] = dt;
        }
    }
}

/* x held to within -lim..lim */
static int clampi(int x, int lim)
{
    if (iabs(x) > lim)
        return x >= 0 ? lim : -lim;
    return x;
}

static void physics(race_car *c)
{
    int drive, drive_front, drive_rear, brake_front, brake_rear, drag_side_f, drag_fwd;
    int side_rear, fwd_rear, side_front, fwd_front, a, m1, m2, y, h;
    int out[4], side, fwd, shift;
    int x, z, sx, sz;

    g_drive.spinning = 0;
    c->speed_abs = mag(iabs(c->vel.x), iabs(c->vel.z));
    if (c->handbrake == 1 && g_drive.rally) {
        c->handbrake = 0;
        c->braking = 0xfa;
    }
    /* a step 1/30 s; with two racers, the leader's slowed (fewer steps a second) by
       how far ahead it is, up to catch_up */
    c->dt = 0x888;
    c->rate = 30;
    if (g_humans == 2 && g_drive.catch_up) {
        int d = (g_cars[c->player == 0]->progress - c->progress) / 2;

        if (d > 0) {
            if (d > g_drive.catch_up)
                d = g_drive.catch_up;
            c->rate = 30 - d;
            c->dt = 0x10000 / c->rate;
        }
    }
    if ((g_drive.flags & 0x10) && !c->handbrake) {
        c->abs_now = c->abs;
        c->tcs_now = c->tcs;
    } else {
        c->abs_now = 0;
        c->tcs_now = 0;
    }
    if ((g_drive.flags & 4) || c->gear == -1) {
        engine_revs(c);
        if (c->gearbox == 3)
            automatic(c);
        else if (c->gearbox == 2)
            c->gear = -1;
        else if (c->gearbox == 1)
            c->gear = -2;
    }

    /* the engine's push, shared between the axles (+0x18 the front's part); the
       brakes' by the pedal and the grip (+0x1c the front's part) */
    c->engine_force = drive = engine(c);
    if (c->tcs_now && c->gear == 0)
        drive -= c->engine_force / 4;
    c->wheelspin = 0;
    drive_front = (drive >> 8) * (S(c, 0x18) >> 8);
    drive_rear = drive - drive_front;
    if (c->braking > 0xf0)
        brake_front = brake_rear = 0x140000;
    else {
        int b = (c->braking * 330 >> 8) * ((c->grip_front + c->grip_rear) >> 8);

        brake_front = (b >> 8) * (S(c, 0x1c) >> 8);
        brake_rear = b - brake_front;
    }
    if (c->handbrake == 1) {
        /* (0x78c30 would push the cars around here with 0xf74f8 = 4, never written;
           EXPLODE's push is the horn's, opponent.c) */
        drive_rear = 0;
        brake_rear = 0x280000;
    }
    /* moving against the gear chosen, nearly straight: the driven wheels brake */
    if (iabs(c->side_speed) < 0x1999
        && ((c->speed > 0x10000 && c->gear == -2) || (c->speed < -0x10000 && c->gear >= 0))) {
        if (drive_front && drive_front < c->grip_front) {
            drive_front = 0;
            brake_front = S(c, 0x28);
        }
        if (drive_rear && drive_rear < c->grip_rear) {
            drive_rear = 0;
            brake_rear = S(c, 0x28);
        }
    }
    if (c->throttle < 0x28)
        c->tcs_now = 0;
    else if (c->tcs_now && iabs(drive) > 0x70000 && c->throttle > 0x53)
        c->throttle -= 12;
    if (c->braking < 0x28)
        c->abs_now = 0;
    if (c->gear != c->gear_now) {
        if (g_drive.viewed == c->player)
            ff_gear(c);        /* (0x40c26) */
        c->gear_shown = 0x10;
        c->gear_before = c->gear_now;
        c->gear_now = c->gear;
    }
    if (c->gear_shown > 0) {
        if (c->gear_shown > 12 || c->shift_wait < 1)
            c->gear_shown--;
        /* the gear's clunk, for the watched car from the driver's seat as the clutch
           bites */
        if (g_drive.viewed == c->player && c->gear_shown == 0xb && g_camera->mode == 0
            && c->shift_wait < 1)
            rsnd_hit(0xd, 0, 0, 1, 0xf00000);
    }

    drag_side_f = drag(c, c->side_speed, 0);
    drag_fwd = drag(c, c->speed, 1);
    if (c->speed > S(c, 0x38) && g_drive.kind != 6) {
        /* at the top speed the drag at least holds the engine back */
        if (drag_fwd > 0 && drag_fwd < drive)
            drag_fwd = iabs(drive);
        else if (drag_fwd < 0 && iabs(drag_fwd) < iabs(drive))
            drag_fwd = -iabs(drive);
    }

    /* the speeds along and across the car; the road's pushes on each axle */
    turn2(c->vel.x, c->vel.z, -c->yaw, &c->side_speed, &c->speed);
    m1 = mulr(c->side_speed * c->rate, c->mass_front);
    y = mulr(c->yaw_rate * c->rate, c->yaw_damp);
    side_rear = m1 - c->side_speed * c->rate - drag_side_f / 2 - y;
    side_front = -m1 - drag_side_f / 2 + y;
    m2 = mulr(c->speed * c->rate, c->mass_front);
    fwd_rear = m2 - c->speed * c->rate - drag_fwd / 2;
    fwd_front = -m2 - drag_fwd / 2;
    /* the front wheels' angle: more when slow, less when fast */
    a = c->steer;
    if (c->speed_abs < 0xc0000) {
        if (c->braking < 10)
            a += c->steer / 2;
    } else if (c->braking > 200)
        a = c->steer / 2;
    else
        a -= c->steer * (c->speed / 256) / 32768;
    c->slide_front = 0;
    c->slide_rear = 0;
    axle(c, &out[0], &out[1], side_front, fwd_front, a, drive_front, brake_front, 1);
    axle(c, &out[2], &out[3], side_rear, fwd_rear, 0, drive_rear, brake_rear, 2);
    side = out[0] + out[2];
    fwd = out[1] + out[3];
    if (c->braking > 100 || c->handbrake) {
        int lim;

        if (c->speed_abs < 0x1ac7ae)
            lim = S(c, 0x28);
        else if (c->speed_abs < 0x280000)
            lim = S(c, 0x2c);
        else
            lim = S(c, 0x2c) > S(c, 0x28) ? S(c, 0x2c) : S(c, 0x28);
        fwd = clampi(fwd, lim);
        /* the acceleration test's strip, before and after the measured stretch */
        if (g_drive.kind == 3 && (c->node <= 0x61 || c->node >= 0x1d1))
            side = clampi(side, lim);
    }
    {
        int lim = S(c, 0x31c) + c->grip_extra;

        side = clampi(side, lim < 0 ? 0 : lim);
    }
    /* the weight moves with the braking and the push: the grips, by the surface */
    shift = (c->weight_shift >> 8) * (fwd >> 8);
    c->grip_front = (c->grip_front_base - shift) * g_surfaces[c->surface][0] / 256;
    c->grip_rear = g_surfaces[c->surface][0] * (shift + c->grip_rear_base) / 256;
    grip_change(c);
    fwd = S(c, 0x32c) * fwd / 256;
    c->force_side = side;
    c->force_fwd = fwd;

    /* the speeds moved on a step */
    if (iabs(c->speed) + iabs(c->side_speed) < 0x4ccc) {
        c->side_speed += mulr(side, c->dt);
        c->speed += mulr(fwd, c->dt);
        if (c->gear == -1 || c->throttle == 0) {
            c->speed = 0;
            c->side_speed = 0;
        }
    } else {
        c->side_speed += mulr(drag_side_f + side + c->push_side, c->dt);
        c->speed += mulr(drag_fwd + fwd + c->push_fwd, c->dt);
        if (iabs(c->speed) < 0xa0000 && (c->gear == -1 || c->throttle == 0)) {
            c->speed -= c->speed / 64;
            c->side_speed -= c->side_speed / 64;
        }
    }
    turn2(c->side_speed, c->speed, c->yaw, &c->vel.x, &c->vel.z);

    /* the car moved: along the road, some of its slide across is turned into speed
       along (by a shift from 0xec320, 0xec36c on rally tracks, per 8 m/s) */
    h = node_heading(c->node);
    turn2(c->vel.x, c->vel.z, -h, &x, &z);
    {
        int ax = iabs(x), k = exe_i32((g_drive.rally ? 0xec36c : 0xec320)
                                      + 4 * ((ax + iabs(z)) >> 19)) & 31;

        if (z > 0) {
            z += ax >> k;
            x -= x >> k;
        }
    }
    turn2(x, z, h, &sx, &sz);
    c->pos.z += mulr(sz / 4 + sz, c->dt);
    c->pos.x -= mulr(sx / 4 + sx, c->dt);

    /* the body leans with the forces */
    if (c->speed + c->side_speed > 0x1999) {
        c->body_roll += (-((c->force_side >> 8) * (S(c, 0x2d4) >> 8)) - c->body_roll) / 2;
        c->body_pitch += (-((c->force_fwd >> 8) * (S(c, 0x2d8) >> 8)) - c->body_pitch) / 2;
    } else {
        c->body_roll += -c->body_roll / 2;
        c->body_pitch += -c->body_pitch / 2;
    }

    /* the turn: the axles' pushes across about the middle; on rally tracks, the
       steering's turn followed */
    {
        int t = mulr(out[2], c->turn_rear) - mulr(out[0], c->turn_front);

        if (g_drive.rally && c->on_ground) {
            int v = c->speed, target;

            if (v > 0x10000)
                v = 0x10000;
            else if (v < -0x10000)
                v = -0x10000;
            if (v < 0x8000 && v > 0 && c->rpm > 3000 && c->gear > 0)
                v = 0x8000;
            target = c->steer / 4 + c->steer;
            if (iabs(c->speed) < iabs(c->side_speed))
                target += c->throttle < 10 ? c->yaw_rate / 2 : c->yaw_rate / 8;
            target = (v >> 10) * target / 64;
            if (g_drive.spinning)
                target *= 2;
            target = target / 256 * ((0x280000 - c->force_fwd) / 256) / 40;
            t = 0;
            if (c->yaw_rate > target) {
                t = (target - c->yaw_rate) << 4;
                if (t < -0x4b0000)
                    t = -0x4b0000;
            } else if (c->yaw_rate < target) {
                t = (target - c->yaw_rate) << 4;
                if (t > 0x4b0000)
                    t = 0x4b0000;
            }
        }
        c->yaw_rate = clampi(c->yaw_rate + mulr(t, c->dt), 0x960000);
    }
    {
        int d = mulr(c->yaw_rate, c->dt);

        c->yaw += d + d / 4;
    }
    /* rolled over on a bend: the bend turns it */
    if (iabs(c->roll) > 0x30000) {
        int bend = node_heading_signed(c->node + 1) - node_heading_signed(c->node);
        int b = c->speed / 256 * bend / 256;

        c->yaw -= mulr(fix_sin(c->roll >> 14), b) / 128;
    }
    c->yaw = (c->yaw < 0 ? c->yaw + 0x1000000 : c->yaw) & 0xffffff;
    road_edges(c);
    if (g_player == c->player)
        node_kind(c);
    if ((g_drive.flags & 4) && g_drive.state == 1) {
        /* the replay's highlights (0x5c6d4): sliding sideways, the tyres skidding fast,
           the start */
        if (iabs(c->side_speed) > iabs(c->speed / 2) && c->side_speed > 0x8000)
            replay_mark(0x57);
        if ((c->slide_front || c->slide_rear) && c->speed_abs > 0x140000
            && ((c->tyres[1] & 1) || (c->tyres[0] & 1)))
            replay_mark(0x28);
        if (!g_drive.first_move && iabs(c->speed) > 0xccc) {
            replay_mark(0x78);
            g_drive.first_move = g_drive.steps + 1;
        }
    }
    if (c->player < DRIVE_RESULTS && iabs(c->speed) > g_drive.top_speed[c->player])
        g_drive.top_speed[c->player] = iabs(c->speed);
    if (g_race_type == 0 && g_drive.kind == 3)
        rusty_springs_test(c);
    /* (a highlight: spinning with the rear sliding) */
    if (c->tyres[1] && iabs(c->yaw_rate) > 0x320000)
        replay_mark(0x57);
    if (c->roll_phase > 1)
        c->roll_phase -= 2;
    if (c->pitch_phase > 0)
        c->pitch_phase--;
}

/* ---- the car on the road: 0x38808 ------------------------------------------------ */

/* 0x3871c: the car tilted a quarter of the way to the road's slope under it (pitch dp
   and roll dr more) */
static void tilt_to_road(race_car *c, int dp, int dr)
{
    int tp = (int)((unsigned)ea_atan_1024(dp - c->along.y, c->half_len) << 14);
    int tr = (int)((unsigned)ea_atan_1024(-c->across.y - dr, c->half_wid) << 14);
    int sp = iabs((tp - c->pitch) / 4), sr = iabs((tr - c->roll) / 4);

    if (c->pitch - tp > sp)
        c->pitch -= sp;
    else if (c->pitch - tp < -sp)
        c->pitch += sp;
    if (c->roll - tr > sr)
        c->roll -= sr;
    else if (c->roll - tr < -sr)
        c->roll += sr;
}

/* 0x38808: gravity; on the road the car lies on its slope and its speed up follows it;
   off it, in the air, it noses over; a landing after a long jump is hard. The slope's
   pull along the car (+0x3e1) for the next step */
void drive_on_road(race_car *c, int place)
{
    int g = (c->dt >> 2) * 0x9cf5c >> 14, y, keep;

    car_road_plane(c, c->yaw, &c->ground);
    c->vel.y -= g;
    if (!(g_drive.options & 0x20))
        c->vel.y -= g / 2;
    y = ((c->vel.y / 4 + c->vel.y) >> 12) * (c->dt >> 4) + c->pos.y;
    if (!place) {
        c->pos.y = c->ground.y;
        c->vel.y = 0;
        c->push_fwd = 0;
        c->push_side = 0;
        return;
    }
    if (y - 0x40 > c->ground.y) {
        keep = y;
        if (y - c->ground.y < 0x4000) {
            tilt_to_road(c, 0, 0);
            if (!c->on_ground && c->gear != -1)
                c->clutch = 1;
            c->on_ground = 1;
        } else {
            /* in the air: the clutch out, the nose and a wing dropping */
            int n = ++c->air_steps;

            c->clutch = 0;
            c->on_ground = 0;
            if (c->roll < 0)
                tilt_to_road(c, 0x3d7 * n, 0x28f * n);
            else
                tilt_to_road(c, 0x3d7 * n, -0x28f * n);
        }
    } else {
        int pitch, roll;

        keep = c->ground.y;
        tilt_to_road(c, 0, 0);
        pitch = (int)((unsigned)ea_atan_1024(-c->along.y, c->half_len) << 14);
        roll = (int)((unsigned)ea_atan_1024(-c->across.y, c->half_wid) << 14);
        c->vel.y = -((fix_sin(pitch >> 14) >> 8) * (c->speed >> 8))
                 - (fix_sin(roll >> 14) >> 8) * (c->side_speed >> 8);
        if (c->air_steps > 20)
            c->vel.y += (g_drive.options & 0x20) && strcmp(g_drive.track, "tr6")
                ? c->air_steps / 6 * g : c->air_steps / 4 * g;
        if (c->air_steps > 10) {
            /* a long jump's landing: laid flat on the road, the view shaken */
            int s;

            replay_mark(0x54);

            c->air_steps = -2;
            c->pitch = pitch;
            c->roll = roll;
            if (c->player == g_player)
                g_drive.air_shake = 5;
            s = c->air_steps - 7;
            if (s > 7)
                s = 7;
            c->pitch_phase = 0x1d;
            c->pitch_shake = -(int)((unsigned)s << 16);
        }
        if (c->air_steps < 0)
            c->air_steps++;
        else
            c->air_steps = 0;
        /* -1: the player's landing heard (0x52ea8), by how fast it came down */
        if (c->air_steps == -1 && c->player == g_player)
            rsnd_hit(4, 1, iabs(c->vel.y), 1, 0);
        if (!c->on_ground && c->gear != -1)
            c->clutch = 1;
        c->on_ground = 1;
    }
    c->push_fwd = (fix_sin(c->pitch >> 14) >> 8) * 2511;
    c->push_up = 0;
    c->push_side = 0;
    if (c->vel.y > 0)
        c->push_fwd = mulr(c->push_fwd, 0x2666);
    else if (c->vel.y < 0)
        c->push_fwd = mulr(c->push_fwd, 0x10000);
    if (g_drive.air_shake)
        g_drive.air_shake--;
    c->pos.y = keep;
}

/* car_check_lap 0x5056c: the node the car is on followed; on a circuit, a lap counted
   crossing the line forwards (its time kept), taken back crossing it backwards; how
   far along the race it is (+0x4c) */
int drive_check_lap(race_car *c)
{
    int was = c->node, n = c->node, moved;

    moved = track_follow(&c->pos, &n);
    c->node = n;
    if (!g_track.loop) {
        c->progress = n;
        return moved;
    }
    if (was > g_drive.last_node - 2 && n < 2) {
        int p = c->player;

        if (p >= 0 && p < DRIVE_PLAYERS && c->lap >= 0 && c->lap < DRIVE_LAPS)
            g_drive.lap_times[p][c->lap] = g_drive.steps - 300;
        c->lap++;
        /* the player's lap: its time against the best (in a race played; a replay's is
           the viewed car's) */
        if (p == g_player && p < DRIVE_PLAYERS && c->lap >= 2 && c->lap <= DRIVE_LAPS) {
            int t = g_drive.lap_times[p][c->lap - 1] - g_drive.lap_times[p][c->lap - 2];

            /* said: a best lap, a best lap and the last to come, or the last lap */
            if (t < g_drive.best_lap) {
                g_drive.best_lap = t;
                g_drive.new_best = 1;
                if (c->lap < g_drive.laps)
                    speech_say("BESTTIME.EAS");
                else if (c->lap == g_drive.laps)
                    speech_say("BESTLAST.EAS");
            } else if (c->lap == g_drive.laps)
                speech_say("FINALLAP.EAS");
        }
    } else if (was < 2 && g_drive.last_node - 2 < n)
        c->lap--;
    c->progress = (c->lap - 1) * g_track.length * 4 + c->node;
    return moved;
}

/* car_road_offset 0x51598: how far the car is to the side of its node's point */
static void road_offset(race_car *c)
{
    int a = node_heading(c->node) >> 8;

    c->dist = -mulr(ea_sin(a), node_i32(c->node, 8) - c->pos.x)
            - mulr(ea_cos(a), node_i32(c->node, 0x10) - c->pos.z);
}

void drive_step(race_car *c)
{
    if (c->state == 4)
        crash_step(c);
    else if (c->state == 3)
        opponent_drive(c);
    else if (c->state == 5)
        opponent_recover(c);
    else if (c->state == 6)
        opponent_wait(c);
    else if (c->state == 1)
        opponent_player_wait(c);
    else {
        physics(c);
        drive_check_lap(c);
        drive_on_road(c, 1);
    }
    road_offset(c);
}

/* ---- a racer's car made ready: load_car_pbs 0x50c24, car_reset 0x50830 ----------- */

/* (a << 16) / b, in the original's two divisions (the whole part, then the rest's) */
static int fixdiv(int a, int b)
{
    return (int)((unsigned)(a / b) << 16) + (int)(((long long)(a % b) << 16) / b);
}

static void S_set(race_car *c, int at, int v)
{
    unsigned char *p = c->spec + at;

    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* 0x3fb90: the weight on each axle and the grips it gives, the turn's resistance and
   each axle's lever on it (+0x445, +0x449, +0x45d are made but never read) */
static void physics_setup(race_car *c)
{
    int mass_rear, k;

    drive_surfaces();
    c->mass_front = mulr(S(c, 0), S(c, 0x14));
    mass_rear = mulr(S(c, 4), S(c, 0x14));
    S_set(c, 0x34, mulr(S(c, 0x34), S(c, 0x14)));
    c->weight_shift = mulr(S(c, 0x24), S(c, 0x44));
    c->grip_front_base = mulr(mulr(c->mass_front, 0x9cf5c), S(c, 0x2dc));
    c->grip_rear_base = mulr(mulr(mass_rear, 0x9cf5c), S(c, 0x2e0));
    c->grip_front = c->grip_front_base;
    c->grip_rear = c->grip_rear_base;
    c->downforce[2] = (short)(c->downforce[0] * 2);
    c->downforce[3] = (short)(c->downforce[1] * 2);
    c->downforce[0] = (short)(S(c, 0x2dc) / 256 * (c->downforce[0] * 10) / 256);
    c->downforce[1] = (short)(S(c, 0x2e0) / 256 * (c->downforce[1] * 10) / 256);
    k = mulr(0x324, mulr(S(c, 0x40), S(c, 0x40)));
    c->yaw_damp = fixdiv(k, S(c, 0x40));
    c->turn_front = fixdiv(mulr(mass_rear, S(c, 0x40)), k);
    c->turn_rear = fixdiv(mulr(c->mass_front, S(c, 0x40)), k);
}

/* car_reset 0x50830: everything moving stopped, in neutral, on the ground */
static void reset(race_car *c)
{
    c->speed_abs = 0;
    c->pos.x = 0x10000;
    c->pos.y = 0;
    c->lap = 1;
    c->vel.x = c->vel.y = c->vel.z = 0;
    c->side_speed = c->up_speed = c->speed = c->wheel_speed = c->yaw_rate = 0;
    c->slide_front = c->slide_rear = c->wheelspin = 0;
    c->rpm_road = c->rpm = 1;
    c->engine_force = c->gear_now = c->gear_shown = 0;
    c->steer = 0;
    c->throttle = c->throttle_kept = c->braking = c->handbrake = 0;
    c->gear = -1;
    c->shifting = c->clutch = 0;
    c->shift_wait = -1;
    c->on_ground = 1;
    c->air_steps = 0;
    c->own_turn = 0;
    memset(c->turn, 0, sizeof c->turn);
    c->shift_last = 0;
    c->pitch = c->roll = c->yaw = 0;
    c->body_roll = c->body_pitch = 0;
    c->push_side = c->push_up = c->push_fwd = 0;
    c->surface = c->surface_flag = 0;
    memset(c->node_flags, 0, sizeof c->node_flags);
    c->tcs_now = c->abs_now = 0;
    c->roll_phase = c->pitch_phase = 0;
    c->roll_shake = c->pitch_shake = 0;
    c->turn_target = c->turn_count = c->auto_mode = c->auto_steer = 0;
    c->dist = c->slot * 10;
}

int drive_load(race_car *c, int automatic, int tcs, int abs)
{
    char path[64];
    const char *name = (const char *)exe_at((unsigned)exe_i32(0xede84 + 4 * c->number), 1);
    unsigned char *d, *u;
    long len, n;
    unsigned sum = 0;
    int i;

    snprintf(path, sizeof path, "SIMDATA/CARSPECS/%s.PBS", name);
    d = file_load(path, &len);
    if (!d) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    u = is_packed(d, len) ? unpack(d, len, &n) : (n = len, d);
    if (u != d)
        free(d);
    if (!u || n < 0x778) {
        fprintf(stderr, "renfs: %s is not a car's physics\n", path);
        free(u);
        return 0;
    }
    free(c->spec);
    c->spec = calloc(1, 0x800);
    if (!c->spec) {
        free(u);
        return 0;
    }
    memcpy(c->spec, u, (size_t)(n < 0x800 ? n : 0x800));
    free(u);
    c->state = 2;
    c->pos.z = 0x600000;
    c->node = 0x10;
    c->progress = 0x10;
    c->lap = 1;
    /* a file changed by hand gives a car without power */
    for (i = 0; i < 0x758; i++)
        sum += c->spec[i];
    if ((int)sum != S(c, 0x774))
        S_set(c, 0x3c, 0);
    if (g_drive.rally) {
        /* EARALLYE: the rear's grip curve the front's, half the grips, the engine's
           and the brakes' (the turn's half as much again) */
        memcpy(c->spec + 0x574, c->spec + 0x374, 0x1ff);
        S_set(c, 0x320, S(c, 0x320) / 2);
        S_set(c, 0x324, 0x10000 / S(c, 0x320));
        S_set(c, 0x328, S(c, 0x328) / 2);
        S_set(c, 0x32c, 0x10000 / S(c, 0x328));
        S_set(c, 0x2dc, S(c, 0x2dc) / 2);
        S_set(c, 0x2e0, S(c, 0x2e0) / 2);
        S_set(c, 0x28, S(c, 0x28) / 2);
        S_set(c, 0x2c, S(c, 0x2c) / 2);
        S_set(c, 0x24, S(c, 0x24) * 3 / 2);
        S_set(c, 0x3c, S(c, 0x3c) * (g_drive.kind == 3 ? 0x14c : 0x133) / 256);
    }
    /* car_torque_load 0x18990: the power less with the automatic, the torque points
       made 16.16 by the power, the final drive and two factors */
    S_set(c, 0x3c, S(c, 0x3c) * (automatic ? 0xf8 : 0x100) / 256);
    for (i = 0; i < S(c, 0x88); i++) {
        int at = 0xb8 + 8 * i, t = (int)((unsigned)S(c, at) << 16);

        t = mulr(mulr(t, S(c, 0x5c)), mulr(S(c, 0x3c), S(c, 0x64)));
        S_set(c, at, mulr(t, S(c, 0x14)));
    }
    c->abs = abs && S(c, 0x334) > 0;
    c->tcs = tcs && S(c, 0x338) > 0;
    c->downforce[0] = c->downforce[1] = 0;
    c->gearbox = automatic ? 2 : 0;
    physics_setup(c);
    reset(c);
    return 1;
}
