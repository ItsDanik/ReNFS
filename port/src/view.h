/* The race's views (NOTES §4.7): the windows the 3D view is painted in, the camera's
   matrices, and the projection's centre and lens for the view being drawn. */
#ifndef RENFS_VIEW_H
#define RENFS_VIEW_H

#include "poly.h"
#include "car.h"

#define VIEW_WINDOWS 17        /* a resolution's, 0xe85c0 + 0x2a8 × resolution */

/* a window (40 bytes) */
typedef struct {
    int id;                    /* +0 */
    int squash;                /* +4: the picture's height for its width, 16.16 */
    int dx, dy;                /* +8, +0xc: the lens's centre moved (dy: the car's horizon) */
    int w, h;                  /* +0x10, +0x14: what the lens covers */
    int x, y, cw, ch;          /* +0x18 ..: the part of the screen painted */
    int tall;                  /* (the port's) one of the 320x240 races' (tall.h) */
} view_window;

/* a view of the camera (0xe8de8 the road ahead, 0xe8e28 the mirror) */
typedef struct {
    int fov;                   /* +0: degrees across */
    int near;                  /* +4: the near plane (road_transform's) */
    int mode;                  /* +0x10: its window, of the resolution's */
    const view_window *win;    /* +0x1c */
    int lens_h;                /* its layout's (+0x18, 0xe8b10 + 52 × +8) +0x20: the
                                  height the mirror's horizon is set by */
    int layout;                /* +8: its layout (0xe8b10 + 0x16c × resolution + 52 × it) */
    int current;               /* +0xc: the layout it has now (F1 takes the next) */
    int box[4];                /* the layout's +0x24 ..: the box round it on the screen, x y
                                  w h (the border of the views outside, 0x3081c) */
} view_rec;

/* what the frame is drawn from (the camera record [0x143df4], 0x129ecc) */
typedef struct {
    int mode;                  /* +0: 0, 1 the driver's eyes (camera.c) .. 13 */
    int wanted;                /* +4: the mode asked for (camera_set_car) */
    int frames;                /* +0xc: updates left of a move */
    vec3 pos;                  /* +0x10: in the world, 16.16 */
    vec3 off;                  /* +0x2c: the eye from the car, turned with it */
    int node;                  /* +0x38: its road node */
    int flags;                 /* +0x3c: 1 the node changed */
    int pitch, yaw, roll;      /* +0x40, +0x44, +0x48: 0x1000000 a turn */
    int tilt[3];               /* +0x5c ..: added to the car's pitch, yaw, roll */
    vec3 eye;                  /* +0x6c: the eye in the car */
    int mirror_on;             /* +0x68: the mirror is drawn */
    int yaw_follow;            /* +0x78: the yaw it turns towards */
    vec3 step;                 /* +0x7c: the eye's move each update */
    view_rec *ahead, *mirror;  /* +0x88, +0x8c */
    const race_car *car;       /* +0x90: the car followed */
    int m[9];                  /* +0x94: the world into the view ahead */
    int mm[9];                 /* +0xb8: the same for the mirror */
} race_camera;

extern view_window g_windows[2][VIEW_WINDOWS];  /* 0xe85c0: 320x200, 640x480 */
extern int g_view_res;                          /* 0x143e2c: 0 320x200, 1 640x480 */
/* (the port's) the 640x480 race shown with 240 or 200 of its rows (the MiSTer's 640x240,
   640x200; video_rows), the 320x200 one with 240 (320x240, tall.h), else 0 */
extern int g_race_rows;
/* (the port's) 320x240: the views ahead and outside drawn through tall windows (tall.h),
   set before view_init */
extern int g_view_tall;
/* a tall window's 320x200 one (any other itself) */
const view_window *view_window_lo(const view_window *w);
/* a number by the race's resolution: lo at 320x200, hi at 640x480 (0x12c308) */
#define H(lo, hi) (g_view_res ? (hi) : (lo))
extern view_rec g_view_ahead, g_view_mirror;    /* 0xe8de8, 0xe8e28 */
extern view_rec g_view_outside;                 /* 0xe8e08: the cameras outside the car */
extern int g_view_redraw;      /* 0x143e00: the view changed, the screen round it drawn
                                  again (0x14210) */
extern const view_rec *g_view_now;              /* 0x143dfc */
extern int g_view_detail;   /* 0x143e04: the views' pixels 1:1 (0), twice as wide (1),
                               twice as wide and high (2) */

/* the windows as the program has them, fitted to the player's car (0x14630's end,
   0x145d0): car its number (+0x48a), horizon its spec's +0x370 */
void view_init(int res, int car, int horizon);
/* a view in window `mode` (0x14154): the sky's tables, the lens, the reciprocals */
void view_setup(view_rec *v, int mode);
/* the view in its layout `layout` (0x14210): the layout's window for the detail
   g_view_detail, its box on the screen; the view changed (g_view_redraw) */
void view_layout(view_rec *v, int layout);
/* the same with the window of another detail (0x14568: the detail itself kept); 0 for
   no layout (-1) */
int view_set_mode(view_rec *v, int layout, int detail);
/* the view a camera mode's kind (its settings' +0) looks through: 0 the driver's, 1
   outside (0xe8de8 + 32 · kind) */
view_rec *view_of_kind(int kind);
/* the camera's two matrices from its angles (0x5868c), squashed by the current view's
   window and turned over for the mirrored tracks (g_road_mirror) */
void view_matrix(race_camera *c);
/* the view ahead (0) or the mirror (1) current (0x1486c): its centre and lens; the
   matrix to draw with returned */
const int *view_select(race_camera *c, int mirror);

/* a turn about x (0), y (1) or z (2) by angle, 0x1000000 a turn (0x58500, 0x5855c,
   0x585b8) */
void mat_turn(int axis, int angle, int m[9]);
/* 3 x 3 matrices of 16.16, each product >> 16 on its own (0x839f8): out = a b */
void mat_mul(const int a[9], const int b[9], int out[9]);

#endif
