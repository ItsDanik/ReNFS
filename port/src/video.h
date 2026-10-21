/* The screen, the clock and the keys: what the game had from the VGA card, the PC's
   timer and the keyboard. */
#ifndef RENFS_VIDEO_H
#define RENFS_VIDEO_H

#define FB_MAX_W 640
#define FB_MAX_H 480

extern unsigned char g_fb[FB_MAX_W * FB_MAX_H];  /* palette numbers, g_fb_w a row */
extern int g_fb_w, g_fb_h;
extern int g_quit;                                /* the window was closed */

int video_init(const char *shot, int shot_frames);
void video_done(void);
/* initgraphics: 640x480 (the front end), 320x200 (a race in low detail), 320x240 (the
   movies, VGA mode X) */
void video_mode(int w, int h);
/* (the port's) the picture shown with `rows` rows (the MiSTer's 640x240 and 640x200 races
   of a 640x480 picture, each at the middle of those it stands for; its 320x240 of a
   320x200 one, one row in five twice); 0 its own. A video_mode() sets its own again */
void video_rows(int rows);
const unsigned char *video_rows_shown(void);   /* a byte a row, 1 shown; 0 all are */
/* with more rows shown than the picture has: row y's pixels from f (the game's row `row`
   it stands for, its pixels `pix` as shown; tall.c), 0 the picture's own */
void video_row_source(const unsigned char *(*f)(int y, int row, const unsigned char *pix));
/* (the port's) the bottom HUD lifted `rows` of the picture's rows as it is shown (the
   MiSTer's HUD (Bottom) margin, hud_lift): each piece's own pixels moved up, where they
   were the picture `behind` (behind_w a row: what the HUD covers) or a colour; the
   pointer lifted too from the picture's row pointer_row down (-1 never), the mouse's
   rows there the bar's own. 0 rows or pieces: none. A video_mode() ends it */
typedef struct {
    int x, y, w, h;                /* its box on the screen */
    const unsigned char *pix;      /* its pixels, w x h, 255 not its own; 0 all of the box */
    int fill;                      /* where it was: -1 the picture behind, else this colour */
    const unsigned char *back;     /* or this picture of its box (w x h) */
} video_piece;
void video_lift(int rows, const video_piece *p, int n, const unsigned char *behind, int behind_w,
                int from_row);
/* setpalette 0x84080: colours first.. from an 8-bit palette; the card keeps 6 bits */
void video_palette(const unsigned char *pal, int first, int count);
/* the palette read back from the card (0x98aec): its 6 bits widened, v << 2 | v >> 4 */
void video_get_palette(unsigned char *pal, int first, int count);
/* the palette as last given, 8 bits a channel (what nearestcolour 0x80c80 searches) */
const unsigned char *video_palette8(void);
/* wait for the next vertical retrace (0xba9c2): the frame is shown then */
void video_retrace(void);

/* the system clock, 100 a second (the game's timer, NOTES §4.3) */
unsigned clock_ticks(void);
/* the same clock between its ticks (the port's: a frame drawn between race steps) */
double clock_fine(void);
unsigned long timer_counter(void);           /* 0xed0fc, the timer's free count */
void timer_counter_set(unsigned long v);
/* settimeout 0x8136b / timedout 0x81389: a wait of n hundredths */
void settimeout(unsigned n);
int timedout(void);

/* the next key pressed, as the front end reads them (0x727a0): 0x1b Esc, 0x0d Enter,
   0x20 space, letters and digits as they are; 0 if none */
int key_get(void);

/* the next key pressed, by its PC scan code (set 1), whatever the keyboard's layout;
   0 if none */
int scan_get(void);

/* the keyboard's state (0x84742): is the key of that PC scan code (set 1) held? */
int key_down(int scan);

/* the mouse (0x848af): its place in the 640x480 picture; the buttons held (1 left,
   2 right) */
int mouse_read(int *x, int *y);
/* 0x84885: the mouse kept inside x0..x1, y0..y1 (the whole screen: 0, 0, 640, 480) */
void mouse_limits(int x0, int y0, int x1, int y1);
/* mouse_set 0x71ad4: the mouse put there */
void mouse_place(int x, int y);
/* the mouse driven by its movement (on: a race steered with it), not its place */
void mouse_relative(int on);
/* the pointer's picture (palette numbers, 255 see-through), drawn over the screen at
   the mouse while shown */
void mouse_pointer(const unsigned char *pixels, int w, int h);
void mouse_show(int on);

/* the joystick as the PC's game port gave it (EA's 0x94fa8): the four axes' counts
   (0..511: the stick's middle 256; the third axis a brake pedal, the fourth a throttle,
   at rest 511, pressed 0; 0 an axis it lacks), the buttons held (1, 2, 4, 8); returns
   the axes it has (bits 0..3), 0 without a joystick */
int joy_device(int count[4], int *buttons);
/* the SDL_Joystick read there (a pad's own), 0 none or a test's; for the force feedback */
void *joy_sdl(void);

/* called at every retrace (the music is fed from it) */
extern void (*g_retrace_hook)(void);

#endif
