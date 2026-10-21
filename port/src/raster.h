/* Painting textured quads into the 3D view (0x91360, 0x90b30; NOTES §4.6). */
#ifndef RENFS_RASTER_H
#define RENFS_RASTER_H

/* a picture to paint with: 8 bits a pixel, rowbytes a row, w x h; reads outside lo..hi
   (the memory the picture was loaded in) give 0 where the original read its neighbours.
   see_through: colour 255 is left out (shape type 0x45, painted by 0x90b30) */
typedef struct {
    const unsigned char *pixels, *lo, *hi;
    int rowbytes, w, h, see_through;
} raster_tex;

/* where the quads go: a picture of palette numbers, pitch bytes a row, and the window
   x0..x1, y0..y1 (the ends outside) */
void raster_window(unsigned char *base, int pitch, int x0, int y0, int x1, int y1);

/* (the port's) the rows of the window's picture that will be shown, a byte a row (the
   MiSTer's 640x240 and 640x200 races): the others are left as they are; 0 all of them.
   Until the next raster_window */
void raster_rows(const unsigned char *shown);
extern const unsigned char *g_raster_shown;
#define RASTER_SHOWN(y) (!g_raster_shown || g_raster_shown[y])

/* the window's picture: a row's start, and the window */
unsigned char *raster_row(int y);
void raster_clip(int *x0, int *y0, int *x1, int *y1);
/* a rectangle of one colour, cut to the window (0x8a2a0) */
void raster_fill(int x, int y, int w, int h, int colour);

/* the corners in order round the quad: uv in 16.16 picture pixels, xy in screen pixels */
void raster_quad(const raster_tex *t, const int uv[8], const int xy[8]);

/* the whole picture, less its outer row of pixels, stretched over a quad: its corners in
   order round it from the picture's top left, in screen pixels (0x90900, 0x906d0; the
   road's far sections). Painted as two triangles, from corners 0 1 2 and 2 3 0 */
void raster_quad_whole(const raster_tex *t, const int xy[8]);

/* a triangle, its corners and picture points in whole pixels (0x905e0, 0x8fad0): each made
   16.16 half way into its pixel */
void raster_tri(const raster_tex *t, const int uv[6], const int xy[6]);

/* where n points lie against the window, all of them together (0x91cee): 1 left, 2 right,
   4 above, 8 below. Compared unsigned, as the original: a point left of 0 counts as right */
int raster_outcode(int n, const int *xy);

#endif
