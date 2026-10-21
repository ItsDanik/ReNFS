/* The race's dashboard and HUD at 320x200 and 640x480 (NOTES §4.11): the dashboard's picture, the
   status bar on top, the map's box, and what changes each frame over them. */
#ifndef RENFS_HUD_H
#define RENFS_HUD_H

#include "fsh.h"

/* an off-screen picture the game draws into, a window of its own (0xf4578 the view,
   0xf4580 the map's background, 0xf4584 the map) */
typedef struct {
    int w, h;
    unsigned char *pix;        /* w a row */
} hud_picture;

/* what the status bar shows (the race's state) */
typedef struct {
    int shown;                 /* 0x12b940 */
    int steps;                 /* 0x145c94: the race's steps; the bar from 360 on */
    int mode;                  /* 0x12b8cc: 0 a segment, 1 the race, 2 the acceleration
                                  test */
    int table;                 /* 0x12dd24: the race length's table of times */
    int track, segment;        /* 0x12da12, 0x12da0e */
    int circuit;               /* 0x13bb80: laps, not miles */
    int ahead;                 /* 0x14ea6c: the split, >= 0 ahead (plus) */
    int splits;                /* 0x14ea70: splits shown */
    int best_lap;              /* 0x13c20c */
    int race_type;             /* 0x12da20 */
    int detail;                /* 0x143e04: which of the dashboard's lists (0: 1:1); 3 (F2's
                                  fourth at 640x480, 0x12d9d9): every other line */
    int mirror;                /* 0x143e08: the mirror shown */
    int map;                   /* 0x12b988: the map shown */
    int mirrored;              /* 0x12dd28: the track the other way round (the wheel too) */
    int running;               /* 0xf6b74 = 1: the race running (else the clip to row
                                  180 for the wheel) */
    int kmh;                   /* 0x15d140: kilometres an hour, not miles */
    int radar;                 /* 0xea814: the radar detector's level, 0 quiet */
    int radar_flash;           /* 0x12b9f8: its big light lit */
    int cop_flash;             /* 0x12ba04: the police's dot on the map, 0..10 */
    int laps;                  /* 0x13c160: the race's laps */
    int lap_limit;             /* 0x12dd14: the most the bar shows */
    int finish_node;           /* 0x13c164: an open road's finish */
    int no_dash;               /* 0x12ba2c: F9, the driver's view without the dashboard
                                  (camera mode 0's view 1, 0xe80f0) */
    int view_changed;          /* F9 pressed: the camera's mode to be taken up again */
    int cam_mode;              /* 0x129ecc: the camera's mode */
    int stopped;               /* 0x145c90: the player finished and stopped */
    int clear;                 /* 0x12d9da: at detail 3 the view's place on the screen
                                  blacked first, once */
} hud_status;

/* what the map shows (0x31cb4) */
typedef struct {
    int node;                  /* the viewed car's road node (+0x48 of 0x13c24c[0x12dd54]) */
    int nodes[8];              /* each racer's */
    int place;                 /* 0x12b8c8: the player's place */
    unsigned char places[8];   /* 0x12bd33: each racer's */
    int cop;                   /* the police car on the map (0x13c254 +0x51a & 4, the
                                  police on 0x12dd08, head to head) */
    int cop_node;
    int driver;                /* in the driver's seat (camera mode 0, 0x12ba2c = 0) */
} hud_map_state;

/* what the dashboard shows of the player's car */
typedef struct {
    int steer;                 /* +0x18: the wheel, 16.16, < 0 left */
    int number;                /* +0x48a: which car */
    int rpm;                   /* +0x3c1: the engine's turns a minute */
    int redline;               /* its physics' +0xac: the most the needle shows, less
                                  980 */
    int speed;                 /* +0x68: metres a second, 16.16 */
    /* and what the status bar's numbers show of the viewed car (0x12dd54) */
    int viewed;                /* 0x12dd54: which car */
    int human;                 /* a player's (0x12dd54 < 0x12da30): its own speed and
                                  gear, else its speed along the road */
    int road_speed;            /* +0x15c */
    int gear;                  /* +0x3ed: -2 reverse, -1 neutral, 0 first .. */
    int player;                /* +0x47d: its place in the race's tables */
    int lap;                   /* +0x50 */
    int node;                  /* +0x48 */
    /* the gear lever, of the dashboard's car (0x2e4f0) */
    int lever_shown;           /* +0x421: counts down after a shift */
    int lever_before;          /* +0x41d: the gear before it */
    int lever_gear;            /* +0x3ed: the gear now */
    int lever_wait;            /* +0x3bd: steps until the clutch bites */
} hud_car;

extern hud_status g_hud;
extern fsh_file g_mask;              /* the HUD's pictures (0x145604) */
extern hud_picture g_hud_screen;     /* the screen (0xa0000 at 320x200) */
extern hud_picture g_hud_map_back;   /* 0xf4580 */
extern hud_picture g_hud_view;       /* 0xf4578: the 3D view, painted off the screen */
extern hud_picture g_hud_mirror;     /* 0xf457c: the mirror's view, between the
                                        dashboard's places 0 and 1 (0x5ad44) */
extern int g_hud_small_needle[4];  /* 0xeaa98: the small rev counter's needle as last drawn,
                                       its end then its start */
extern hud_picture g_hud_map;        /* 0xf4584: the map */

/* the HUD's pictures (load_cockpit_mask 0x5f7c8: SIMDATA/MISC/MASKLO.FSH, MASKLOG.FSH
   in German), their colours queued for re-colouring with the program's 13 at 0xf0 */
int hud_load(void);
void hud_free(void);
/* race_stats_max 0x2d71c: the acceleration test's times (steps, 1,200 at most; 0 left
   as it was) and the speed at the quarter mile; 0x2d778: the steps from 60 and from 80
   mph to standing (500 at most), as feet */
void hud_accel_times(int t60, int t100, int quarter, int speed);
void hud_braking(int from60, int from80);
/* the whole dashboard drawn again, as a view starts (0x30094): the dashboard's picture,
   the map's dim background taken from it, the status bar */
void hud_redraw(void);
/* the whole dashboard drawn again at the next frame (0x2d284) */
void hud_redraw_due(void);
/* a frame in the driver's seat (0x31684): the view through the windows, the mirror, the
   wheel, the gauges and the status bar's numbers over it */
void hud_frame(const hud_car *c);
/* the map's picture (0x31cb4, before the frame): the road round the viewed car turned
   to its heading, the segment's end, the racers and the police */
void hud_map(const hud_map_state *m);
/* the mirror's picture turned round, left for right (0x96240), once it is painted */
void hud_mirror_flip(void);

/* the views from outside the car (camera modes 2 ..): the background of tiles, the
   status bar, a border round the view's box (x, y, w, h) and the small rev counter,
   drawn into the view's picture when the view changes (0x3081c) */
void hud_outside_screen(const int box[4]);
/* each frame from outside (0x314e4): the view's picture to the screen, whole after a
   change, else through the HUD's list for the view's layout (0x30b64); the status bar's
   numbers, the small rev counter's needle (0x31168), the map */
void hud_outside_frame(const hud_car *c, int redraw, int layout);

/* the police's ticket or warning over the whole screen (0x30424): loaded as it comes,
   its colours over the race's; 1 while shown (the race's view and dashboard not
   drawn). Once both are 0 again (0x306e8) the race's colours come back (unless `over`)
   and the dashboard is drawn again: 2 that frame, else 0 */
int hud_ticket(int ticket, int warning, int over);
/* 0x5c5d0, at the end of both frames: playing back the replay's bar (0x5fcf0), racing
   with another car watched or the player stopped the watching bar (0x7ef14) */
void hud_bar_draw(void);
/* (the port's) the bottom HUD lifted `rows` of the screen's rows as it is shown (the
   MiSTer's HUD (Bottom)): the replay's or the watching bar, and from outside the small
   rev counter and the map, as this frame drew them; once a frame drawn, after the HUD */
void hud_lift(int rows);
/* 0x2dac4: the race held by P: "paus" on the screen at its own place */
void hud_paused(void);

#endif
