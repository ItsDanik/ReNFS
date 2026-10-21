/* The force feedback's device (ffdev.h; NOTES §4.26). The original sent its commands to
   an I-Force device on a serial port; here they are played on SDL's haptics (a wheel or
   a stick with force feedback) or on a pad's rumble, or written down. None of it is the
   original's: only what each command asks for is. */
#include "ffdev.h"
#include "mister.h"
#include "drive.h"
#include "files.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *(*g_ffdev_joystick)(void);
unsigned char *g_ffdev_rec;
int g_ffdev_rec_len, g_ffdev_rec_cap;

static FILE *g_log;
static int g_strength = 100;   /* percent */
static int g_open;

/* a wheel or stick with force feedback */
static SDL_Haptic *g_hap;
static unsigned g_hap_can;
enum { E_KICK, E_SHAKE_A, E_SHAKE_B, E_RUMBLE, E_STEADY, E_N };
static int g_effect[E_N];

#ifdef MISTER_HYBRID
/* or the MiSTer's: a wheel's forces through the core's library (MH_Force*), the I-Force's
   effects in its slots */
static int g_mforce;
#endif

/* else a pad's two motors */
static SDL_Joystick *g_rumble_joy;
static int g_on = 1;           /* the forces on (0x83) or held off (0x84) */
static int g_shake[2], g_rumble_size, g_kick_size;
static Uint32 g_kick_end, g_sent_at;
static int g_sent_low = -1, g_sent_high = -1;

static int sbyte(unsigned char b)
{
    return (signed char)b;
}

/* renfs.cfg's "forcefeedback <0..100>", or RENFS_FF */
static void strength_load(void)
{
    const char *e = getenv("RENFS_FF");

    g_strength = e ? atoi(e) : files_cfg_get("forcefeedback", 100);
    if (g_strength < 0)
        g_strength = 0;
    if (g_strength > 100)
        g_strength = 100;
#ifdef MISTER_HYBRID
    if (!e && mister_rumble_strength() >= 0)   /* (the OSD's Rumble: On, Strong 160, Off) */
        g_strength = mister_rumble_strength();
#endif
}

static void haptic_close(void)
{
    if (g_hap) {
        SDL_HapticStopAll(g_hap);
        SDL_HapticClose(g_hap);
        g_hap = 0;
    }
}

/* a haptic device that can push (a constant force): the joystick's own, else the first
   one SDL lists (on the MiSTer the wheel is not the joystick the game reads) */
static SDL_Haptic *haptic_find(SDL_Joystick *joy)
{
    SDL_Haptic *h;
    int i;

    if (SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0)
        return 0;
    if (joy && SDL_JoystickIsHaptic(joy) == 1 && (h = SDL_HapticOpenFromJoystick(joy))) {
        if (SDL_HapticQuery(h) & SDL_HAPTIC_CONSTANT)
            return h;
        SDL_HapticClose(h);
    }
    for (i = 0; i < SDL_NumHaptics(); i++)
        if ((h = SDL_HapticOpen(i))) {
            if (SDL_HapticQuery(h) & SDL_HAPTIC_CONSTANT)
                return h;
            SDL_HapticClose(h);
        }
    return 0;
}

int ffdev_open(void)
{
    const char *log = getenv("RENFS_FFLOG");
    SDL_Joystick *joy;

    if (g_ffdev_rec)
        return 1;
    if (log && !g_log)
        g_log = fopen(log, "w");
    strength_load();
    g_rumble_joy = 0;
    joy = g_strength > 0 && g_ffdev_joystick ? g_ffdev_joystick() : 0;
    if (!g_hap && g_strength > 0 && (g_hap = haptic_find(joy))) {
        g_hap_can = SDL_HapticQuery(g_hap);
        for (int i = 0; i < E_N; i++)
            g_effect[i] = -1;
        SDL_HapticSetGain(g_hap, 100);
    }
#ifdef MISTER_HYBRID
    g_mforce = !g_hap && g_strength > 0 && mister_force_available();
    if (g_mforce)
        joy = 0;
#endif
    /* (a rumble of nothing tried, not SDL_JoystickHasRumble: SDL 2.0.18's) */
    if (!g_hap && joy && SDL_JoystickRumble(joy, 0, 0, 0) == 0)
        g_rumble_joy = joy;
    g_open = g_hap || g_rumble_joy || g_log;
#ifdef MISTER_HYBRID
    g_open |= g_mforce;
#endif
    return g_open;
}

void ffdev_close(void)
{
#ifdef MISTER_HYBRID
    if (g_mforce)
        mister_force_stop(-1);
    g_mforce = 0;
#endif
    haptic_close();
    if (g_rumble_joy)
        SDL_JoystickRumble(g_rumble_joy, 0, 0, 0);
    g_rumble_joy = 0;
    if (g_log)
        fclose(g_log);
    g_log = 0;
    g_open = 0;
}

/* ---- on a haptic device ---------------------------------------------------------------- */

static Sint16 level(int pct)
{
    int v = pct < 0 ? -pct : pct;
    v = (v > 100 ? 100 : v) * 32767 / 100 * g_strength / 100;
    return (Sint16)(v > 32767 ? 32767 : v);
}

/* SDL's direction is where the force comes from: a push to the right (+across) comes
   from the left */
static void direction(SDL_HapticDirection *d, int across, int along)
{
    memset(d, 0, sizeof *d);
    d->type = SDL_HAPTIC_CARTESIAN;
    d->dir[0] = across || along ? -across : -1;
    d->dir[1] = -along;
}

static void play(int which, SDL_HapticEffect *e)
{
    if (g_effect[which] < 0)
        g_effect[which] = SDL_HapticNewEffect(g_hap, e);
    else if (SDL_HapticUpdateEffect(g_hap, g_effect[which], e) < 0) {
        SDL_HapticDestroyEffect(g_hap, g_effect[which]);
        g_effect[which] = SDL_HapticNewEffect(g_hap, e);
    }
    if (g_effect[which] >= 0)
        SDL_HapticRunEffect(g_hap, g_effect[which], 1);
}

static void stop(int which)
{
    if (g_effect[which] >= 0)
        SDL_HapticStopEffect(g_hap, g_effect[which]);
}

static void constant(int which, int across, int along, Uint32 length)
{
    SDL_HapticEffect e;
    int m = abs(across) > abs(along) ? across : along;

    memset(&e, 0, sizeof e);
    e.type = SDL_HAPTIC_CONSTANT;
    direction(&e.constant.direction, across, along);
    e.constant.length = length;
    e.constant.level = level(m);
    play(which, &e);
}

static void sine(int which, int across, int along, int size, int rate)
{
    SDL_HapticEffect e;

    if (!(g_hap_can & SDL_HAPTIC_SINE))
        return;
    memset(&e, 0, sizeof e);
    e.type = SDL_HAPTIC_SINE;
    direction(&e.periodic.direction, across, along);
    e.periodic.length = SDL_HAPTIC_INFINITY;
    e.periodic.period = (Uint16)(1000 / (rate > 0 ? rate : 1));
    e.periodic.magnitude = level(size);
    play(which, &e);
}

static void haptic_send(int cmd, const unsigned char *b)
{
    switch (cmd) {
    case 0xa1:
        constant(E_KICK, sbyte(b[0]), sbyte(b[1]), b[2] * 10u);
        break;
    case 0xa4:
    case 0xa5: {
        int w = cmd == 0xa4 ? E_SHAKE_A : E_SHAKE_B, x = sbyte(b[0]), y = sbyte(b[1]);
        if (!x && !y && !b[2])
            stop(w);
        else
            sine(w, x, y, abs(x) > abs(y) ? x : y, b[2]);
        break;
    }
    case 0xa6:
        sine(E_RUMBLE, 1, 0, sbyte(b[0]), 20);       /* (the I-Force's rate unknown) */
        break;
    case 0xa7:
        if (!b[0] && !b[1])
            stop(E_STEADY);
        else
            constant(E_STEADY, sbyte(b[0]), sbyte(b[1]), SDL_HAPTIC_INFINITY);
        break;
    case 0x82:
        SDL_HapticStopAll(g_hap);
        break;
    case 0x83:
        if (g_hap_can & SDL_HAPTIC_PAUSE)
            SDL_HapticUnpause(g_hap);
        break;
    case 0x84:
        if (g_hap_can & SDL_HAPTIC_PAUSE)
            SDL_HapticPause(g_hap);
        else
            SDL_HapticStopAll(g_hap);
        break;
    }
}

#ifdef MISTER_HYBRID
/* ---- on the MiSTer's wheel ---------------------------------------------------------------- */

/* a percent of the I-Force's as the library's -255..255, by the strength */
static int mlevel(int pct)
{
    int v = (pct < -100 ? -100 : pct > 100 ? 100 : pct) * 255 / 100 * g_strength / 100;
    return v < -255 ? -255 : v > 255 ? 255 : v;
}

/* the slots: 0 the kicks, 1 and 2 the road's shakes, 3 the crash's rumble, 4 the steady
   push; across is the wheel's (a wheel has no along) */
static void mforce_send(int cmd, const unsigned char *b)
{
    switch (cmd) {
    case 0xa1:
        mister_force_constant(0, mlevel(sbyte(b[0])), b[2] * 10);
        break;
    case 0xa4:
    case 0xa5: {
        int x = sbyte(b[0]), y = sbyte(b[1]);
        if (!x && !y && !b[2])
            mister_force_stop(cmd - 0xa3);
        else
            mister_force_sine(cmd - 0xa3, mlevel(abs(x) > abs(y) ? x : y), 1000 / (b[2] ? b[2] : 1));
        break;
    }
    case 0xa6:
        mister_force_sine(3, mlevel(sbyte(b[0])), 50);    /* (the I-Force's rate unknown) */
        break;
    case 0xa7:
        if (!b[0] && !b[1])
            mister_force_stop(4);
        else
            mister_force_constant(4, mlevel(sbyte(b[0])), 0);
        break;
    case 0x82:
    case 0x84:
        mister_force_stop(-1);
        break;
    }
}
#endif

/* ---- on a pad's rumble -------------------------------------------------------------------- */

static void rumble_send(int cmd, const unsigned char *b)
{
    switch (cmd) {
    case 0xa1: {
        int x = abs(sbyte(b[0])), y = abs(sbyte(b[1]));
        g_kick_size = x > y ? x : y;
        g_kick_end = SDL_GetTicks() + b[2] * 10u;
        break;
    }
    case 0xa4:
    case 0xa5: {
        int x = abs(sbyte(b[0])), y = abs(sbyte(b[1]));
        g_shake[cmd - 0xa4] = b[2] ? (x > y ? x : y) : 0;
        break;
    }
    case 0xa6:
        g_rumble_size = b[0];
        break;
    case 0x82:
        g_shake[0] = g_shake[1] = g_rumble_size = g_kick_size = 0;
        break;
    case 0x83:
    case 0x84:
        g_on = cmd == 0x83;
        break;
    }
    ffdev_tick();
}

void ffdev_tick(void)
{
    Uint32 now;
    int low, high;

#ifdef MISTER_HYBRID
    if (g_mforce)
        mister_force_keep();
#endif
    if (!g_rumble_joy)
        return;
    now = SDL_GetTicks();
    if (g_kick_size && (Sint32)(now - g_kick_end) >= 0)
        g_kick_size = 0;
    /* the strong motor the kicks and the crash's rumble, the weak one the road */
    low = g_rumble_size > g_kick_size ? g_rumble_size : g_kick_size;
    high = g_shake[0] > g_shake[1] ? g_shake[0] : g_shake[1];
    if (g_kick_size / 2 > high)
        high = g_kick_size / 2;
    if (!g_on)
        low = high = 0;
    low = low * 655 * g_strength / 100;
    high = high * 655 * g_strength / 100;
    if (low != g_sent_low || high != g_sent_high || now - g_sent_at > 100) {
        SDL_JoystickRumble(g_rumble_joy, (Uint16)(low > 0xffff ? 0xffff : low),
                           (Uint16)(high > 0xffff ? 0xffff : high), 300);
        g_sent_low = low;
        g_sent_high = high;
        g_sent_at = now;
    }
}

/* ---- every command ------------------------------------------------------------------------ */

void ffdev_send(int cmd, const unsigned char *b, int n)
{
    if (g_ffdev_rec) {
        if (g_ffdev_rec_len + 2 + n <= g_ffdev_rec_cap) {
            g_ffdev_rec[g_ffdev_rec_len++] = (unsigned char)cmd;
            g_ffdev_rec[g_ffdev_rec_len++] = (unsigned char)n;
            memcpy(g_ffdev_rec + g_ffdev_rec_len, b, n);
            g_ffdev_rec_len += n;
        }
        return;
    }
    if (g_log) {
        fprintf(g_log, "%d %02x", g_drive.steps / 2, cmd);
        for (int i = 0; i < n; i++)
            fprintf(g_log, " %d", sbyte(b[i]));
        fputc('\n', g_log);
        fflush(g_log);
    }
    if (g_hap)
        haptic_send(cmd, b);
#ifdef MISTER_HYBRID
    else if (g_mforce)
        mforce_send(cmd, b);
#endif
    else if (g_rumble_joy)
        rumble_send(cmd, b);
}
