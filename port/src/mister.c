/* mister.c - see mister.h */
#include "mister.h"
#include "files.h"
#include <SDL.h>
#include <mister_hybrid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define EXIT_TO_MENU  0
#define EXIT_RESTART  42    /* danik_hybrid_launch.sh starts the game again: the core's list */
#define EXIT_RELOADED 43    /* the same, once the core has settled after being loaded again */

#define TITLE "ReNFS"

int mister_on(void)
{
    static int tried;
    if (!tried) {                      /* (once: without the core it would look each time) */
        tried = 1;
        return MH_Open();
    }
    return MH_IsOpen();
}

/* the OSD's lines greyed: HUD (Bottom) (bit 0) while the races have 200 rows, Smooth
   Motion (bit 1) while they are 640x480, HUD (Top) always (bit 2: the status bar stays
   where the game has it) */
static void menu_mask(void)
{
    static int was = -1;
    const int r = mister_resolution();
    const int mask = (r == MR_320x200 || r == MR_640x200 ? 1 : 0) | (r == MR_640x480 ? 2 : 0) | 4;
    if (mask != was) {
        MH_SetMenuMask(mask);
        was = mask;
    }
}

static uint32_t g_field;           /* the field of the last retrace */
static int g_field_ok;

void mister_present_at(void)
{
    if (MH_IsOpen() && g_field_ok)
        MH_PresentAtField(g_field + 2);
}

void mister_retrace_wait(void)
{
    if (!MH_IsOpen())
        return;
    menu_mask();
    if (!g_field_ok) {
        g_field = MH_FieldCounter();
        g_field_ok = 1;
    }
    /* the next field after the last retrace's; one that took longer than a field goes on
       at once from the field it is in */
    if (MH_FieldCounter() == g_field)
        MH_WaitField(g_field);
    g_field = MH_FieldCounter();
}

/*
 * THE GAME'S FILES. The CD in NFSSE/ in the game's folder; the files the game opens
 * there (English: the German ones, the G folders, are never opened on the MiSTer) are
 * looked for, so that one missing is said at the start and not found out in a race.
 */
static const char *const g_found[] = {   /* the game is there (MH_PickGame) */
    "NFSSE/NFS_DOS.EXE", "NFSSE/FRONTEND/ART/TITLE.QFS", "NFSSE/SIMDATA/MISC/*", NULL
};

#define NO_GAME "The Need for Speed SE was not found. Copy everything on its CD, all the " \
    "files and folders as they are, to %s/NFSSE/."

static int g_listed;

/* `want` (a path from the CD's top, any case) is in `dir` */
static int has_file(const char *dir, const char *want)
{
    char at[1100], part[256];
    const char *p = want;
    snprintf(at, sizeof at, "%s", dir);
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : strlen(p);
        DIR *d;
        struct dirent *e;
        int found = 0;
        if (n >= sizeof part)
            return 0;
        memcpy(part, p, n);
        part[n] = 0;
        if (!(d = opendir(at)))
            return 0;
        while ((e = readdir(d)))
            if (!strcasecmp(e->d_name, part)) {
                size_t l = strlen(at);
                snprintf(at + l, sizeof at - l, "/%s", e->d_name);
                found = 1;
                break;
            }
        closedir(d);
        if (!found)
            return 0;
        p += n + (slash ? 1 : 0);
    }
    return 1;
}

int mister_start(void)
{
    static const mh_game game = { "SE", "Need for Speed SE", g_found, NULL, NO_GAME };
    if (!MH_Open())
        return 1;
    if (MH_PickGame(&game, 1, NULL, NO_GAME, &g_listed) < 0)
        return 0;
    if (g_listed)
        setenv("RENFS_MISTER_LISTED", "1", 1);
    {
        /* every file the game opens, the first missing ones named */
        extern const char *const g_mister_files[];
        char text[600] = "", path[600];
        int missing = 0;
        for (int i = 0; g_mister_files[i]; i++) {
            if (has_file("NFSSE", g_mister_files[i]))
                continue;
            if (missing++ < 3) {
                size_t l = strlen(text);
                snprintf(text + l, sizeof text - l, "%s%s", l ? ", " : "", g_mister_files[i]);
            }
        }
        if (missing) {
            char msg[1400];
            if (missing > 3) {
                size_t l = strlen(text);
                snprintf(text + l, sizeof text - l, " and %d more", missing - 3);
            }
            snprintf(path, sizeof path, "%s/NFSSE", MH_GamePath());
            snprintf(msg, sizeof msg, "Files of the CD are missing in %s: %s. Copy everything on "
                     "the CD, all the files and folders as they are.", path, text);
            mister_fatal(msg);
            return 0;
        }
    }
    MH_UI_Message(TITLE, "Loading Need for Speed SE...", 0);
    return 1;
}

void mister_fatal(const char *text)
{
    printf("mister: %s\n", text);
    fflush(stdout);
    if (MH_Open())
        MH_UI_Message(TITLE, text, MH_UI_WAIT | MH_UI_ERROR);
}

int mister_exit_code(void)
{
    if (MH_CoreReloaded())
        return EXIT_RELOADED;
    /* a game picked from the core's list: the list again (not when another core was loaded) */
    if (getenv("RENFS_MISTER_LISTED") && MH_Open())
        return EXIT_RESTART;
    return EXIT_TO_MENU;
}

/*
 * THE PAD. The core's joystick 1, as SDL's mister driver has it: the buttons of the
 * core's list ("J1" in core/ReNFS.sv) in its order, 28 and 29 the MiSTer menu's OK and
 * Back, 30 and 31 the sticks' clicks; the left stick axes 0 and 1, the triggers 4 and 5
 * (0..32767), the d-pad hat 0. Given to the game as the original's joystick: four axes
 * (the stick, the brake, the throttle) and its four buttons (1 handbrake, 2 map,
 * 4 camera, 8 horn), and the port's two (1 gear up, 2 gear down).
 */
enum { J_ACCEL, J_BRAKE, J_GEAR_UP, J_GEAR_DOWN, J_CAMERA, J_HANDBRAKE, J_MAP, J_PAUSE,
       J_MENU_OK, J_MENU_BACK, J_OK = 28, J_BACK = 29, J_L3 = 30, J_R3 = 31 };

static SDL_Joystick *g_pad;

static SDL_Joystick *pad(void)
{
    if (g_pad && !SDL_JoystickGetAttached(g_pad)) {
        SDL_JoystickClose(g_pad);
        g_pad = 0;
    }
    if (!g_pad && SDL_NumJoysticks() > 0)
        g_pad = SDL_JoystickOpen(0);
    return g_pad;
}

static int button(int n)
{
    SDL_Joystick *j = pad();
    return j && n < SDL_JoystickNumButtons(j) && SDL_JoystickGetButton(j, n);
}

static int axis(int n)
{
    SDL_Joystick *j = pad();
    return j && n < SDL_JoystickNumAxes(j) ? SDL_JoystickGetAxis(j, n) : 0;
}

void *mister_pad_sdl(void)
{
    return pad();
}

int mister_joy(int count[4], int *buttons)
{
    int hat, t, b;
    SDL_Joystick *j = pad();
    if (!j)
        return 0;
    count[0] = (axis(0) + 32768) >> 7;
    count[1] = (axis(1) + 32768) >> 7;
    hat = SDL_JoystickNumHats(j) > 0 ? SDL_JoystickGetHat(j, 0) : 0;
    if (hat & SDL_HAT_LEFT)
        count[0] = 0;
    if (hat & SDL_HAT_RIGHT)
        count[0] = 511;
    if (hat & SDL_HAT_UP)
        count[1] = 0;
    if (hat & SDL_HAT_DOWN)
        count[1] = 511;
    /* the pedals: the triggers, or the buttons all the way; pressed reads low, as the
       game port's */
    t = button(J_ACCEL) ? 511 : axis(5) >> 6;
    b = button(J_BRAKE) ? 511 : axis(4) >> 6;
    count[3] = 511 - (t > 511 ? 511 : t);
    count[2] = 511 - (b > 511 ? 511 : b);
    *buttons = (button(J_HANDBRAKE) ? 1 : 0) | (button(J_MAP) ? 2 : 0)
             | (button(J_CAMERA) ? 4 : 0) | (button(J_L3) ? 8 : 0)
             | (button(J_GEAR_UP) ? 16 : 0) | (button(J_GEAR_DOWN) ? 32 : 0);
    return 0xf;
}

int mister_menu_key(void)
{
    if (getenv("RENFS_PADLOG")) {
        static unsigned last;
        unsigned m = 0;
        for (int i = 0; i < 32; i++)
            m |= (unsigned)button(i) << i;
        if (m != last)
            printf("pad: buttons %08x\n", m);
        last = m;
    }
    if (button(J_OK) || button(J_MENU_OK))
        return 0x0d;
    if (button(J_BACK) || button(J_MENU_BACK))
        return 0x1b;
    return 0;
}

int mister_pause_held(void)
{
    return button(J_PAUSE);
}

int mister_nitro_held(void)
{
    return button(J_R3);
}

static int g_racing;

void mister_racing(int on)
{
    g_racing = on;
}

int mister_is_racing(void)
{
    return g_racing;
}

int mister_osd_open(void)
{
    return MH_IsOpen() && MH_OSDOpen();
}

/* "O[26:24],Resolution,320x200,320x240,640x200,640x240,640x480" */
int mister_resolution(void)
{
    int r;
    if (!MH_IsOpen())
        return MR_320x200;
    r = MH_OSD_GAME_BITS(MH_OSDStatus(), 24, 3);
    return r > MR_640x480 ? MR_320x200 : r;
}

/* the CRT Options' "HUD (Bottom)" (status bits 82..86): the core's entries 0..24 in order,
   0 the default (the framework's lines start at 10) */
int mister_hud_bottom(void)
{
    int n;
    if (!MH_IsOpen())
        return 0;
    n = (int)(MH_OSDStatusHigh() >> (82 - 64) & 31);
    return n > 24 ? 24 : n;
}

/* "D1O[27],Smooth Motion,On,Off": off at 640x480 whatever the line says */
int mister_smooth(void)
{
    if (!MH_IsOpen())
        return -1;
    return mister_resolution() != MR_640x480 && !MH_OSD_GAME_BITS(MH_OSDStatus(), 27, 1);
}

/* "O[29:28],Rumble,On,Strong,Off" */
int mister_rumble_strength(void)
{
    static const int strength[4] = { 100, 160, 0, 0 };
    return MH_IsOpen() ? strength[MH_OSD_GAME_BITS(MH_OSDStatus(), 28, 2)] : -1;
}

/* "P2O[30],Mirrored Tracks" .. "P2O[38],No Mercy" */
int mister_cheats(void)
{
    return MH_IsOpen() ? (int)(MH_OSDStatus() >> 30 & 0x1ff) : 0;
}

int mister_force_available(void)
{
    return MH_IsOpen() && MH_ForceAvailable();
}

void mister_force_constant(int slot, int level, int ms)
{
    MH_ForceConstant(slot, level, ms);
}

void mister_force_sine(int slot, int magnitude, int period_ms)
{
    MH_ForceSine(slot, magnitude, period_ms, 0);
}

void mister_force_stop(int slot)
{
    MH_ForceStop(slot);
}

void mister_force_keep(void)
{
    MH_ForceKeep();
}
