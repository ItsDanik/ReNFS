/* The main menu, "Control Central" (fe_main_menu 0x42c30, NOTES §4.2): the player's car,
   the opponent, the race type and the location, each a picture with arrows; the buttons
   Race, Options, Chat (network games only) and Cancel. The network game's parts are left
   out (Danik, 2026-10-18). */
#include "mainmenu.h"
#include "mister.h"
#include "config.h"
#include "fe.h"
#include "files.h"
#include "font.h"
#include "fsh.h"
#include "menu.h"
#include "options.h"
#include "race.h"
#include "session.h"
#include "showcase.h"
#include "trackrec.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

/* what can be chosen (the selection, 0x504 on the original's stack) */
enum { OPPONENT, PLAYER, LOCATION, RACE_TYPE, RACE, OPTIONS, CHAT, CANCEL, NOTHING };

/* the pointer is put on the one chosen by the keys (0xec418, 0xec42c) */
static const int sel_x[9] = { 482, 175, 444, 211, 45, 228, 400, 570, 0 };
static const int sel_y[9] = { 158, 197, 318, 349, 454, 454, 454, 454, 0 };

/* the pictures' names (0x41c80, 0x41ca8, 0x41ccc, 0x41cf0, 0x41d20, 0x41d50) */
static const char *const track_pic[3][10] = {
    { "city", "coast", "alpine", "springs", "avalley", "bsienna", "vridge", "tpolis", "lvegas", "moon" },
    { "citym", "coastm", "alpinem", "springsm", "avalleym", "bsiennam", "vridgem", "tpolism", "lvegasm", "moon" },
    { "cityr", "coastr", "alpiner", "springsr", "avalleyr", "bsiennar", "vridger", "tpolisr", "lvegasr", "moon" },
};
static const char *const player_pic[12] = { "rx71", "nsx1", "sup1", "9111", "vet1", "vip1",
                                            "5121", "dia1", "war1", "war1", "war1", "war1" };
static const char *const opponent_pic[12] = { "rx72", "nsx2", "sup2", "9112", "vet2", "vip2",
                                              "5122", "dia2", "slew", "watch2", "tourny", "war2" };
static const char *const race_type_pic[5] = { "watch1", "headhead", "single", "tourny", "nomercy" };
/* the tournament's car for each track (0xec440) */
static const int tournament_car[10] = { 0, 3, 6, 0, 3, 3, 6, 6, 7, 3 };

/* What stays from one visit to the next, and what the typed codes unlock */
int g_mirror, g_rally;                  /* 0x12dd28, 0x12dd2c: the track's versions */
int g_mirror_ok, g_rally_ok;            /* 0xec454, 0xec458 */
int g_vegas_ok;                         /* 0xec460 (track 8 offered) */
static int g_vegas_typed;               /* 0xec464 */
int g_warrior_ok;                       /* 0xf7494 */
static int g_no_mercy;                  /* 0xf7492: End held on head to head */
static int g_explode, g_nitro;          /* 0xf74a4, 0x12cca0 (the race's, g_race_codes) */
static int g_letter;                    /* 0x12ccaa: a letter held */
static int g_code_at[8];                /* 0xec4d8..: how far each code is typed */

static fsh_file g_art;                  /* control/central.qfs (0xec518) */
static struct {
    fsh_file f;
    int loaded, key[3];
} g_pic[4];                             /* track, player, opponent, race type */
enum { PIC_TRACK, PIC_PLAYER, PIC_OPPONENT, PIC_TYPE };

static int cw(int at) { return config_word(at); }
static void cset(int at, int v) { config_set_word(at, v); }

static void art(const char *name, int see_through)
{
    shape s;
    if (fsh_find(&g_art, name, &s))
        fe_draw_shape(&s, see_through);
}

/* a picture of its own file (0x83f18), its shape "0000", loaded again only when what it
   shows changes */
static void picture(int which, const char *name, int k0, int k1, int k2)
{
    shape s;
    if (!g_pic[which].loaded || g_pic[which].key[0] != k0 || g_pic[which].key[1] != k1 ||
        g_pic[which].key[2] != k2) {
        char path[256];
        if (g_pic[which].loaded)
            fsh_free(&g_pic[which].f);
        snprintf(path, sizeof path, "%s/CONTROL/%s.QFS", art_dir(), name);
        g_pic[which].loaded = fsh_load(&g_pic[which].f, path);
        if (!g_pic[which].loaded)
            fprintf(stderr, "renfs: %s is missing\n", path);
        g_pic[which].key[0] = k0;
        g_pic[which].key[1] = k1;
        g_pic[which].key[2] = k2;
    }
    if (g_pic[which].loaded && fsh_find(&g_pic[which].f, "0000", &s))
        fe_draw_shape(&s, 0);
}

/* the showcase from a car's picture (0x45189, 0x4545c; not with the Warrior): the menu's
   music faded out, the showcase's. The opponent's car is shown as the player's for its
   time (the traffic's as the RX-7), and the car it ends on is the opponent's after */
static void showcase(int opponent)
{
    int car = cw(CFG_CAR), opp = cw(CFG_OPPONENT);

    music_fade_out();
    pal_fade_down(0, 256);
    music_wait_end();
    music_start("showcase.asf");
    if (g_pic[PIC_OPPONENT].loaded) {      /* (0xec4fc = 0x14: loaded again) */
        fsh_free(&g_pic[PIC_OPPONENT].f);
        g_pic[PIC_OPPONENT].loaded = 0;
    }
    if (opponent)
        cset(CFG_CAR, opp == 8 ? 0 : opp);
    fe_showcase();
    music_random();                        /* 0x63e2c */
    if (opponent) {
        cset(CFG_OPPONENT, cw(CFG_CAR));
        cset(CFG_CAR, car);
    }
}

/* the track's screen from its picture (0x456af; not over a link): the menu's music
   faded out, the showcase's */
static void track_screen(void)
{
    music_fade_out();
    pal_fade_down(0, 256);
    music_wait_end();
    music_start("showcase.asf");
    if (g_pic[PIC_OPPONENT].loaded) {      /* (0xec4fc = 0x14: loaded again) */
        fsh_free(&g_pic[PIC_OPPONENT].f);
        g_pic[PIC_OPPONENT].loaded = 0;
    }
    fe_track_screen();
    music_random();                        /* 0x63e2c */
}

/* 0x424c0: the pictures let go */
static void pictures_free(void)
{
    for (int i = 0; i < 4; i++)
        if (g_pic[i].loaded) {
            fsh_free(&g_pic[i].f);
            g_pic[i].loaded = 0;
        }
    fsh_free(&g_art);
}

/* car_setup 0x73a80: ABS and traction control as the player's car has them */
void fe_car_setup(void)
{
    static const int a[12] = { 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0 };
    static const int b[12] = { 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
    int car = cw(CFG_CAR);
    if (car < 0 || car > 11)
        car = 0;
    cset(CFG_ABS, a[car] ? 0 : 2);         /* on, or none on that car */
    cset(CFG_TCS, b[car] ? 1 : 2);         /* off, or none */
}

/* the laps from the race length; the open roads (4-7 are circuits) run half as long */
static void set_laps(void)
{
    int track = cw(CFG_TRACK), circuit = track >= 4 && track <= 7;
    int len = cw(CFG_RACE_LENGTH);
    cset(CFG_LAPS, len == 0 ? (circuit ? 2 : 4) : len == 1 ? (circuit ? 6 : 8) : (circuit ? 12 : 16));
}

/* 0x425b0: a new race type sets the opponent */
static void race_type_changed(void)
{
    int opp = cw(CFG_OPPONENT);
    int unlocked = g_vegas_typed || cw(CFG_UNLOCKED);
    switch (cw(CFG_RACE_TYPE)) {
    case 0:                                /* time trial: the clock */
        g_no_mercy = 0;
        g_vegas_ok = unlocked;
        cset(CFG_OPPONENT, 9);
        break;
    case 1:                                /* head to head: a car, the player's if none */
        g_vegas_ok = unlocked;
        if (opp >= 8 && opp <= 10)
            cset(CFG_OPPONENT, cw(CFG_CAR));
        break;
    case 2:                                /* single race: traffic */
        g_vegas_ok = unlocked;
        g_no_mercy = 0;
        cset(CFG_OPPONENT, 8);
        break;
    case 3: {                              /* tournament: its car class by track */
        int track;
        g_no_mercy = 0;
        g_mirror = g_rally = 0;
        if (!cw(CFG_UNLOCKED))
            g_vegas_ok = 0;
        if ((!g_vegas_ok && cw(CFG_TRACK) == 8) || cw(CFG_TRACK) == 9)
            cset(CFG_TRACK, 0);
        track = cw(CFG_TRACK);
        cset(CFG_CAR, tournament_car[track < 0 || track > 9 ? 0 : track]);
        fe_car_setup();
        cset(CFG_OPPONENT, 10);
        cset(CFG_RACE_LENGTH, 1);
        cset(CFG_LAPS, track < 4 || track > 7 ? 8 : 6);
        break;
    }
    }
}

/* "Quit tournament ?" while a game is on (0x15d134: after any race from here, or a
   loaded tournament); 1 to go on */
static int quit_tournament(void)
{
    return !g_game_on || fe_yes_no_dialog(g_german ? "Wettkampf beenden ?" : "Quit tournament ?");
}

/* 0x4235c: the race type left or right; while a game is on only when it is quit: then
   head to head and a new session */
static void race_type_step(int right)
{
    if (g_game_on) {
        if (!quit_tournament())
            return;
        cset(CFG_RACE_TYPE, 1);
        session_new();             /* 0x7469c (and 0x15d134 = 0) */
    } else
        cset(CFG_RACE_TYPE, (cw(CFG_RACE_TYPE) + (right ? 1 : 3)) % 4);
    race_type_changed();
}

/* 0x4287c: the next track; its mirrored and rally versions first when unlocked */
static void track_next(void)
{
    int type3 = cw(CFG_RACE_TYPE) == 3;
    if (!g_mirror) {
        if (!g_rally) {
            if (g_mirror_ok && !type3) {
                g_mirror = 1;
                return;
            }
            if (g_rally_ok && !type3) {
                g_rally = 1;
                return;
            }
        } else
            g_rally = 0;
    } else {
        g_mirror = 0;
        if (g_rally_ok && !type3) {
            g_rally = 1;
            return;
        }
    }
    /* 0x42910 */
    if (cw(CFG_TRACK) == 9)
        cset(CFG_TRACK, 0);
    else
        cset(CFG_TRACK, (cw(CFG_TRACK) + 1) % (g_vegas_ok ? 9 : 8));
}

/* 0x42984: the track before (the same versions, the other way) */
static void track_previous(void)
{
    int type3 = cw(CFG_RACE_TYPE) == 3, track;
    if (g_rally) {
        g_rally = 0;
        if (!g_mirror_ok)
            return;
        g_mirror = 1;
        return;
    }
    if (g_mirror) {
        g_mirror = 0;
        return;
    }
    track = cw(CFG_TRACK);
    track = track == 9 ? (g_vegas_ok ? 8 : 7) : track - 1;
    if (track < 0)
        track = g_vegas_ok ? 8 : 7;
    cset(CFG_TRACK, track);
    if (g_rally_ok && !type3) {
        g_rally = 1;
        return;
    }
    if (g_mirror_ok && !type3)
        g_mirror = 1;
}

/* the player's car: in a tournament within its class (by the track: 0-2, 3-5, 6-7);
   the Warrior after 7 once unlocked */
static void car_step(int right)
{
    int car = cw(CFG_CAR), track = cw(CFG_TRACK);
    if (cw(CFG_RACE_TYPE) == 3) {
        int lo = track == 0 || track == 3 ? 0 : track == 1 || track == 4 || track == 5 ? 3 : 6;
        int hi = lo == 0 ? 2 : lo == 3 ? 5 : 7;
        car = car == 11 ? (right ? 0 : 7) : car + (right ? 1 : -1);
        if (right && car > hi)
            car = lo;
        if (!right && car < lo)
            car = hi;
    } else if (right) {
        car = g_warrior_ok && car == 7 ? 11 : car == 11 ? 0 : car + 1 > 7 ? 0 : car + 1;
    } else {
        car = car == 11 ? 7 : car - 1;
        if (car < 0)
            car = g_warrior_ok ? 11 : 7;
    }
    cset(CFG_CAR, car);
    fe_car_setup();
}

/* the opponent: the cars, and in a single race traffic (8) too */
static void opponent_step(int right)
{
    int opp = cw(CFG_OPPONENT), last = cw(CFG_RACE_TYPE) == 2 ? 8 : 7;
    if (right)
        opp = opp == 11 || opp + 1 > last ? 0 : opp + 1;
    else {
        opp = opp == 11 ? last : opp - 1;
        if (opp < 0)
            opp = last;
    }
    cset(CFG_OPPONENT, opp);
}

/* 0x91d60 for the letters: their PC scan codes */
static int letter_scan(int c)
{
    static const char rows[] = "QWERTYUIOP\0\0\0\0ASDFGHJKL\0\0\0\0\0ZXCVBNM";
    const char *p = memchr(rows, c, sizeof rows - 1);
    return p ? 0x10 + (int)(p - rows) : 0;
}

/* the first letter held, 'A'..'Z', or 0 */
static int letter_held(void)
{
    for (int c = 'A'; c <= 'Z'; c++)
        if (key_down(letter_scan(c)))
            return c;
    return 0;
}

/* 0x46e94: one more letter of a code; 1 when it is complete */
static int code_typed(int n, const char *code, int letter)
{
    /* left at the end once complete: the next letter only brings it back to the start */
    if (letter == code[g_code_at[n]]) {
        if (!code[++g_code_at[n]])
            return 1;
    } else
        g_code_at[n] = 0;
    return 0;
}

/* the codes typed on this screen (stored as letter + 99 at 0xec48c..) */
static int codes(void)
{
    int letter = letter_held(), redraw = 0, type3 = cw(CFG_RACE_TYPE) == 3;
    if (g_letter) {                        /* one letter at a time: until let go */
        g_letter = letter;
        return 0;
    }
    if (!letter)
        return 0;
    g_letter = 1;
    if (!g_mirror_ok && code_typed(0, "SKCART", letter)) {        /* tracks mirrored */
        g_mirror_ok = 1;
        fe_play_sound(0x48);
        if (!type3) {
            g_mirror = 1;
            g_rally = 0;
            redraw = 1;
        }
    }
    if (!g_rally_ok && code_typed(1, "EARALLYE", letter)) {       /* rally tracks */
        g_rally_ok = 1;
        fe_play_sound(0x48);
        if (!type3) {
            g_rally = 1;
            g_mirror = 0;
            redraw = 1;
        }
    }
    if (!g_explode && !type3 && code_typed(2, "EXPLODE", letter)) {
        g_explode = 1;
        fe_play_sound(0x48);
    }
    if (!g_nitro && !type3 && code_typed(6, "NITRO", letter)) {
        g_nitro = 1;
        fe_play_sound(0x48);
    }
    if (!g_vegas_typed && !type3 && code_typed(3, "CASINOVILLE", letter))
        g_vegas_typed = g_vegas_ok = 1;    /* Lost Vegas (no sound) */
    if (!type3 && code_typed(4, "CONCEPTCAR", letter)) {          /* the Warrior */
        g_warrior_ok = 1;
        redraw = 1;
        cset(CFG_CAR, 11);
        fe_play_sound(0x48);
    }
    if (!type3 && code_typed(5, "TURBO", letter)) {
        g_cheat_flags |= 0x08;
        fe_play_sound(0x48);
    }
    if (!type3 && code_typed(7, "ROCKYROAD", letter)) {          /* all of it, kept */
        int level = (cw(CFG_UNLOCKED) == 1) + 1;
        cset(CFG_UNLOCKED, level);
        if (!g_vegas_ok)
            g_vegas_ok = level;
        g_warrior_ok = g_mirror_ok = 1;
        if (level > 1)
            g_rally_ok = 1;
    }
    return redraw;
}

#ifdef MISTER_HYBRID
/* (the MiSTer's) the OSD's Cheats page: while a line is on, its code is as typed (not in
   a tournament, as the codes are not taken there); turned on, what typing it chooses is
   chosen (the mirrored or rally track, the Warrior), turned off it is taken back. Unlock
   All (ROCKYROAD) is kept in the records once done, as typed twice */
static int codes_osd(void)
{
    static int had;
    int on = mister_cheats(), type3 = cw(CFG_RACE_TYPE) == 3, redraw = 0;
    int up = on & ~had, down = had & ~on;

    had = on;
    if (on & MC_MIRROR)
        g_mirror_ok = 1;
    if (on & MC_RALLY)
        g_rally_ok = 1;
    if ((up & MC_MIRROR) && !type3) {
        g_mirror = 1;
        g_rally = 0;
        redraw = 1;
    }
    if ((up & MC_RALLY) && !type3) {
        g_rally = 1;
        g_mirror = 0;
        redraw = 1;
    }
    if (down & MC_MIRROR) {
        g_mirror_ok = 0;
        redraw |= g_mirror;
        g_mirror = 0;
    }
    if (down & MC_RALLY) {
        g_rally_ok = 0;
        redraw |= g_rally;
        g_rally = 0;
    }
    if ((on & MC_VEGAS) && !type3)
        g_vegas_typed = g_vegas_ok = 1;
    if (down & MC_VEGAS) {
        g_vegas_typed = 0;
        g_vegas_ok = cw(CFG_UNLOCKED) != 0;
        if (!g_vegas_ok && cw(CFG_TRACK) == 8) {
            cset(CFG_TRACK, 0);
            redraw = 1;
        }
    }
    if ((on & MC_WARRIOR) && !type3)
        g_warrior_ok = 1;
    if ((up & MC_WARRIOR) && !type3) {
        cset(CFG_CAR, 11);
        fe_car_setup();
        redraw = 1;
    }
    if (down & MC_WARRIOR) {
        g_warrior_ok = 0;
        if (cw(CFG_CAR) == 11) {
            cset(CFG_CAR, 0);
            fe_car_setup();
            redraw = 1;
        }
    }
    if ((up & MC_UNLOCK) && !type3) {
        cset(CFG_UNLOCKED, 2);
        g_vegas_ok = g_warrior_ok = g_mirror_ok = g_rally_ok = 1;
        redraw = 1;
    }
    if (!type3) {
        if (on & MC_TURBO)
            g_cheat_flags |= 0x08;
        if (on & MC_NITRO)
            g_nitro = 1;
        if (on & MC_EXPLODE)
            g_explode = 1;
    }
    if (down & MC_TURBO)
        g_cheat_flags &= ~0x08;
    if (down & MC_NITRO)
        g_nitro = 0;
    if (down & MC_EXPLODE)
        g_explode = 0;
    return redraw;
}
#endif

static int in_rect(int x0, int x1, int y0, int y1)
{
    int x, y;
    mouse_read(&x, &y);
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static void place_mouse(int sel)
{
    mouse_place(sel_x[sel], sel_y[sel]);   /* 0x71ad4: placed and remembered */
    fe_mouse_save();
}

/* the screen drawn again (0x43911..0x44452) */
static void draw(int sel, int arrow)
{
    static const char *const frame[4] = { "Trb3", "Tlb1", "Brb4", "Blb2" };
    static const char *const arrows[4][2] = { { "Aro5", "Aro6" }, { "Aro1", "Aro2" },
                                              { "Aro7", "Aro8" }, { "Aro3", "Aro4" } };
    static const char *const button[4] = { "Ra1l", "optl", "chal", "canl" };
    int track = cw(CFG_TRACK), car = cw(CFG_CAR), opp = cw(CFG_OPPONENT), type = cw(CFG_RACE_TYPE);
    int version = g_mirror ? 1 : g_rally ? 2 : 0;
    const char *opp_name;

    art("bgnd", 0);
    art("cham", 1);                        /* Chat, greyed */
    if (track >= 0 && track < 10)
        picture(PIC_TRACK, track_pic[version][track], track, g_mirror, g_rally);
    if (type == 3 && track >= 0 && track < 9 && g_session.raced[track])
        art("flad", 1);                    /* the tournament's track raced (0x43e3c) */
    if (car >= 0 && car < 12)
        picture(PIC_PLAYER, player_pic[car], car, 0, 0);
    opp_name = opp >= 0 && opp < 12 ? opponent_pic[opp] : "slew";
    if (type == 3)                         /* the tournament's class */
        opp_name = car < 3 ? "tourny3" : car < 6 ? "tourny2" : "tourny1";
    picture(PIC_OPPONENT, opp_name, opp, type == 3 ? car : -1, 0);
    picture(PIC_TYPE, race_type_pic[type == 1 && g_no_mercy ? 4 : type < 0 || type > 3 ? 0 : type],
            type, g_no_mercy, 0);
    if (sel <= RACE_TYPE) {
        art(frame[sel], 1);
        if (arrow)
            art(arrows[sel][arrow - 1], 1);
    } else if (sel <= CANCEL)
        art(button[sel - RACE], 0);
}

int fe_main_menu(void)
{
    unsigned char pal[768];
    shape pal_shape;
    int sel = PLAYER, arrow = 0, redraw = 1, first = 1, changed = 0, done = 0, result = 0;
    int ready = 1, released = 1, prev_track = -1, prev_type = -1, k;
    unsigned held = 0;
    char path[256];

    place_mouse(PLAYER);
    /* the codes' unlocks kept in CONFIG.DAT; the Warrior is not kept as the car */
    g_vegas_ok = cw(CFG_UNLOCKED);
    g_vegas_typed = 0;
    g_mirror_ok = g_mirror = g_rally = 0;
    g_rally_ok = 0;
    if (cw(CFG_UNLOCKED) >= 1)
        g_warrior_ok = g_mirror_ok = 1;
    if (cw(CFG_UNLOCKED) >= 2)
        g_rally_ok = 1;
    if (!g_vegas_ok && cw(CFG_TRACK) == 8)
        cset(CFG_TRACK, 0);
    if (cw(CFG_CAR) == 11)
        cset(CFG_CAR, 0);

    snprintf(path, sizeof path, "%s/CONTROL/CENTRAL.QFS", art_dir());
    if (!fsh_load(&g_art, path) || !fsh_find(&g_art, "!pal", &pal_shape) ||
        !fsh_palette(&g_art, 0, pal)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    fe_sound_bank_load();
    fe_mouse_load();
    fe_mouse_save();

    while (!done) {
        /* the mouse over a panel, an arrow or a button picks it */
        int over = NOTHING, over_arrow = 0;
        int type = cw(CFG_RACE_TYPE);
        if (in_rect(0x153, 0x271, 0x61, 0xda) && (type == 1 || type == 2)) {
            over = OPPONENT;
            over_arrow = in_rect(0x155, 0x16a, 0x91, 0xa8) ? 1 : in_rect(0x25d, 0x26f, 0x91, 0xa8) ? 2 : 0;
        } else if (in_rect(0x13, 0x14a, 0x70, 0x11a)) {
            over = PLAYER;
            over_arrow = in_rect(0x15, 0x28, 0xba, 0xd1) ? 1 : in_rect(0x136, 0x148, 0xba, 0xd1) ? 2 : 0;
        } else if (in_rect(0x153, 0x225, 0xfc, 0x17f)) {
            over = LOCATION;
            over_arrow = in_rect(0x155, 0x168, 0x131, 0x14e) ? 1 : in_rect(0x20f, 0x223, 0x131, 0x14e) ? 2 : 0;
        } else if (in_rect(0x5a, 0x14c, 0x13a, 0x180)) {
            over = RACE_TYPE;
            over_arrow = in_rect(0x5c, 0x6b, 0x154, 0x16d) ? 1 : in_rect(0x136, 0x145, 0x154, 0x16d) ? 2 : 0;
        } else if (in_rect(0xa5, 0x125, 0x1ba, 0x1d1))
            over = OPTIONS;
        else if (in_rect(0x1f8, 0x27f, 0x1ba, 0x1d1))
            over = CANCEL;
        else if (in_rect(0, 0x70, 0x1ba, 0x1d1))
            over = RACE;
        if (over != sel) {
            sel = over;
            changed = 1;
        }
        if (over <= RACE_TYPE && over_arrow != arrow) {
            arrow = over_arrow;
            changed = 1;
        }

        if (prev_track != cw(CFG_TRACK)) {  /* a new track: its laps */
            prev_track = cw(CFG_TRACK);
            changed = 1;
            if (cw(CFG_RACE_TYPE) == 3) {
                int t = cw(CFG_TRACK);
                cset(CFG_CAR, tournament_car[t < 0 || t > 9 ? 0 : t]);
                fe_car_setup();
            }
            set_laps();
        }
        if (prev_type != cw(CFG_RACE_TYPE)) {
            race_type_changed();
            prev_type = cw(CFG_RACE_TYPE);
            changed = 1;
        }

        if (redraw) {
            redraw = 0;
            draw(sel, arrow);
            mouse_show(1);                 /* 0x52288 */
            g_mouse_active = 1;
            if (first) {
                redraw = 1;                /* once more */
                if (g_mouse_active)
                    fe_mouse_read();
                pal_fade_up(pal, 0, 256);
                inv_build();               /* 0x8227c again */
                fe_styles_init();
                first = 0;
                settimeout(12000);         /* 2 minutes */
            }
        }
        if (changed) {                     /* (the buttons drawn here are drawn over by the
                                              redraw before they are seen) */
            if (sel != NOTHING)
                fe_play_sound(0x47);
            redraw = 1;
            changed = 0;
        }

        k = fe_read_keys();
        if (!ready && k != 0x1b && k != 0x20 && k != 0x0d) {
            held += ticksince();
            if (held > 25)
                ready = 1;
        }
        if (codes())
            redraw = 1;
#ifdef MISTER_HYBRID
        if (codes_osd())
            redraw = 1;
        if (cw(CFG_RACE_TYPE) == 1 && (key_down(0x4f) || (mister_cheats() & MC_NO_MERCY))) {
#else
        if (cw(CFG_RACE_TYPE) == 1 && key_down(0x4f)) {   /* End held: "No mercy" */
#endif
            if (!g_no_mercy) {
                g_no_mercy = 1;
                g_cheat_flags |= 0x04;
                redraw = 1;
            }
        } else if (g_no_mercy) {
            g_no_mercy = 0;
            g_cheat_flags &= ~0x04;
            redraw = 1;
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
                case OPPONENT:
                    if (in_rect(0x25d, 0x270, 0x91, 0xa8))
                        opponent_step(1), redraw = 1;
                    else if (in_rect(0x155, 0x16a, 0x91, 0xa8))
                        opponent_step(0), redraw = 1;
                    else if (cw(CFG_CAR) != 11) {
                        showcase(1);
                        place_mouse(sel);
                        redraw = first = 1;
                    }
                    break;
                case PLAYER:
                    if (in_rect(0x136, 0x149, 0xba, 0xd1))
                        car_step(1), redraw = 1;
                    else if (in_rect(0x15, 0x28, 0xba, 0xd1))
                        car_step(0), redraw = 1;
                    else if (cw(CFG_CAR) != 11) {
                        showcase(0);
                        place_mouse(sel);
                        redraw = first = 1;
                    }
                    break;
                case LOCATION:
                    if (in_rect(0x20f, 0x224, 0x131, 0x14e))
                        track_next(), redraw = 1;
                    else if (in_rect(0x155, 0x168, 0x131, 0x14e))
                        track_previous(), redraw = 1;
                    else if (cw(CFG_TRACK) != 9) {
                        track_screen();
                        place_mouse(sel);
                        redraw = first = 1;
                    }
                    break;
                case RACE_TYPE:
                    if (in_rect(0x136, 0x145, 0x154, 0x16d))
                        race_type_step(1), redraw = 1;
                    else if (in_rect(0x5c, 0x6b, 0x154, 0x16d))
                        race_type_step(0), redraw = 1;
                    break;
                case RACE:
                    if (cw(CFG_RACE_TYPE) == 3 && g_session.raced[cw(CFG_TRACK) % 9]) {
                        /* a tournament's track raced already (0x45a32) */
                        fe_message_box(g_german ? "Strecke bereits fertiggestellt"
                                                : "Track already completed");
                        settimeout(100);
                        while (!timedout() && !g_quit) {
                            video_retrace();
                            if (g_mouse_active)
                                fe_mouse_read();
                        }
                        settimeout(12000);
                        redraw = 1;
                        break;
                    }
                    g_game_on = 1;         /* (0x45ab7) */
                    if (cw(CFG_RACE_TYPE) == 3) {
                        /* (the codes not taken into a tournament) */
                        g_explode = 0;
                        g_rally = 0;
                        g_cheat_flags &= ~8ul;
                        g_warrior_ok = 0;
                    }
                    mouse_show(0);
                    pictures_free();
                    music_fade_out();
                    fe_sound_bank_free();
                    pal_fade_down(0, 256);
                    music_wait_end();
                    fe_sound_unmute();     /* (0x45b21: F5's silence ends) */
                    g_race_codes.mirror = g_mirror;
                    g_race_codes.rally = g_rally;
                    g_race_codes.nitro = g_nitro;
                    g_race_codes.explode = g_explode;
                    race_from_menu();
                    /* back: "No mercy" over, the menu loaded and faded in again */
                    g_no_mercy = 0;
                    g_cheat_flags &= ~4ul;
                    if (g_quit || !fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
                        done = 1;
                        result = 1;
                        break;
                    }
                    fe_sound_bank_load();
                    fe_mouse_load();
                    place_mouse(sel);
                    fe_mouse_save();
                    redraw = first = 1;
                    break;
                case OPTIONS:              /* back with everything loaded and faded in
                                              again (0x4577a) */
                    pal_fade_down(0, 256);
                    pictures_free();
                    fe_options_screen();
                    if (!fsh_load(&g_art, path) || !fsh_palette(&g_art, 0, pal)) {
                        done = 1;
                        break;
                    }
                    place_mouse(sel);
                    redraw = first = 1;
                    break;
                case CANCEL:
                    if (!quit_tournament())
                        break;
                    fe_flush_keys();
                    done = 1;
                    result = 0;
                    break;
                }
            }
            released = ready = 0;
            ticksince();
            held = 0;
        } else if (k && ready) {
            int left = k == 0x4b00, right = k == 0x4d00;
            if (k == 0x1b) {
                fe_play_sound(0x48);
                if (quit_tournament()) {
                    fe_flush_keys();
                    done = 1;
                    result = 0;
                }
            } else if (k == 0x4800 || k == 0x5000) {
                static const int up[9] = { RACE, 0, LOCATION - 1, RACE_TYPE - 1, RACE - 1,
                                           RACE_TYPE, RACE_TYPE, RACE_TYPE, PLAYER };
                static const int down[9] = { PLAYER, LOCATION, RACE_TYPE, RACE, 0, 0, 0, 0, PLAYER };
                int t = cw(CFG_RACE_TYPE), opp_off = t == 3 || t == 0;
                fe_mouse_hide();
                if (k == 0x4800)
                    sel = sel == PLAYER ? (opp_off ? RACE : OPPONENT) : up[sel];
                else
                    sel = sel >= RACE && sel <= CANCEL ? (opp_off ? PLAYER : OPPONENT) : down[sel];
                changed = 1;
                place_mouse(sel);
            } else if (left || right) {
                fe_mouse_hide();
                switch (sel) {
                case OPPONENT:
                    fe_play_sound(0x48);
                    opponent_step(right);
                    redraw = 1;
                    place_mouse(sel);
                    break;
                case PLAYER:
                    fe_play_sound(0x48);
                    car_step(right);
                    redraw = 1;
                    place_mouse(sel);
                    break;
                case LOCATION:
                    fe_play_sound(0x48);
                    if (right)
                        track_next();
                    else
                        track_previous();
                    redraw = 1;           /* the laps, and a tournament's car, follow below */
                    place_mouse(sel);
                    break;
                case RACE_TYPE:
                    fe_play_sound(0x48);
                    race_type_step(right);
                    redraw = 1;
                    place_mouse(sel);
                    break;
                default:                   /* along the buttons, Chat left out */
                    sel = right ? (sel == RACE ? OPTIONS : sel == OPTIONS ? CANCEL : sel == CANCEL ? RACE : CANCEL)
                                : (sel == RACE ? CANCEL : sel == OPTIONS ? RACE : sel == CANCEL ? OPTIONS : RACE);
                    changed = 1;
                    place_mouse(sel);
                    break;
                }
            }
            settimeout(12000);
            ticksince();
            ready = 0;
            held = 0;
        } else if (!k)
            ready = 1;

        video_retrace();
        if (g_quit)
            done = 1, result = 1;
        if (!done && !first) {
            if (timedout() && cw(CFG_RACE_TYPE) != 3) {   /* 2 minutes: back to the title */
                fe_play_sound(0x48);
                done = 1;
                result = 2;
            } else if (g_mouse_buttons == 2 && released) {
                if (fe_yes_no_dialog(g_german ? "Spiel verlassen ?" : "Exit game ?")) {
                    done = 1;
                    result = 0;
                }
                fe_flush_keys();
                do {
                    if (g_mouse_active)
                        fe_mouse_read();
                    video_retrace();
                } while ((fe_read_keys() || g_mouse_buttons) && !g_quit);
            }
        }
    }
    fe_sound_bank_free();
    mouse_show(0);
    pal_fade_down(0, 256);
    pictures_free();
    return result;
}

void fe_unlock(void)
{
    int level = config_word(CFG_UNLOCKED);

    if (!g_vegas_ok)
        g_vegas_ok = level;
    if (level > 0)
        g_warrior_ok = g_mirror_ok = 1;
    if (level > 1)
        g_rally_ok = 1;
}
