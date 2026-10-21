/* (the port's) races at 320x240 (the MiSTer's Resolution line, RENFS_RES=320x240): the
   game's 320x200 screen stretched to 240 rows, with the 3D view drawn at 240 rows of its
   own. The view is painted into a 320x240 picture through windows 6/5 as tall (view.c's
   tall windows, the same field of view in square pixels); a 320x200 copy of it goes where
   the game's view picture goes, so that everything the game draws is drawn as at
   320x200; then each row shown takes the 240-row view's pixels wherever the screen still
   has what the dashboard's lists copied there from the view, and the screen's own pixels
   (stretched) elsewhere. */
#ifndef RENFS_TALL_H
#define RENFS_TALL_H

#include "view.h"

#define TALL_W 320
#define TALL_H 240

/* on for the race (the picture made), or off */
int tall_on(int on);
/* the 240-row view's picture */
unsigned char *tall_picture(void);
/* a frame begins: nothing of the view on the screen yet */
void tall_frame(void);
/* the view painted (tall window t, its 200-row window lo): its rows into the game's view
   picture `view` (320 a row) */
void tall_view_done(const view_window *t, const view_window *lo, unsigned char *view);
/* the dashboard's list copied n pixels of the view picture from pixel `from` (its row's
   start + column) on, read at `rate` (0x10000 1:1), to the screen's from pixel `to` on */
void tall_copied(long to, long from, int n, unsigned rate);

#endif
