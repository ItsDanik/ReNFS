/* The screen, the clock and the keys. The picture is palette numbers, as the game's;
   it is shown through the palette at each vertical retrace, so a fade is a palette fade
   as on the VGA card. The VGA's DAC kept 6 bits a channel: so does this one.

   With --shot <file> <n> the program runs without a window and its clock is the
   retraces counted (70 a second in the 200-line modes, else 60): the n-th retrace's
   picture is written to the file (PPM) and the program ends. */
#include "video.h"
#include "files.h"
#include "ffdev.h"
#include "mister.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char g_fb[FB_MAX_W * FB_MAX_H];
int g_fb_w = 640, g_fb_h = 480;
int g_quit;

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static unsigned char g_dac[768];        /* 6 bits a channel, as the card's */
static unsigned char g_pal8[768];       /* as given, 8 bits (0x1607a4, kept by 0x83fe4) */
static unsigned g_rgb[256];

#ifdef MISTER_HYBRID
/* the MiSTer's core is there: the window's surface is the core's picture, 8 bits a pixel
   through the palette (SDL's "mister" driver; its size picks the core's mode) */
static int g_mister;
#endif

static const char *g_shot;
static int g_shot_at;
static unsigned long g_retraces;        /* counted, for the headless clock */
static unsigned long g_virtual_ms;      /* the headless clock, in ms x 70 x 60 */
static unsigned g_start_ms;
static Uint64 g_start_pc;           /* the same moment on the fine counter */

static int g_keys[16], g_nkeys;  /* the keys typed, as the BIOS gives them (0x84304) */

static void key_put(int c)
{
    if (c && g_nkeys < 16)
        g_keys[g_nkeys++] = c;
}

static int g_scans[16], g_nscans;    /* the keys pressed, by their PC scan codes */

static void scan_put(int scan)
{
    if (scan > 0 && g_nscans < 16)
        g_scans[g_nscans++] = scan;
}

/* the mouse, in the 640x480 picture's pixels, and its pointer drawn over the picture */
static int g_mouse_x = 320, g_mouse_y = 240, g_mouse_buttons;
static int g_lim_x0 = 0, g_lim_y0 = 0, g_lim_x1 = 639, g_lim_y1 = 479;

static void mouse_clamp(void)
{
    g_mouse_x = g_mouse_x < g_lim_x0 ? g_lim_x0 : g_mouse_x > g_lim_x1 ? g_lim_x1 : g_mouse_x;
    g_mouse_y = g_mouse_y < g_lim_y0 ? g_lim_y0 : g_mouse_y > g_lim_y1 ? g_lim_y1 : g_mouse_y;
}
static int g_mouse_rel;
static const unsigned char *g_cursor;
static int g_cursor_w, g_cursor_h, g_cursor_on;

void (*g_retrace_hook)(void);

/* RENFS_INPUT: keys and the mouse played in at given retraces, for pictures without a
   window: "<retrace>:<what>" or "<from>-<to>:<what>" (held from .. until) separated by
   spaces; <what> is a key (up down left right enter esc space y n j tab f1..f6 bksp
   del ctrl alt lshift rshift end), "click", "rclick" (held 6 retraces unless a span is given), "m<x>,<y>",
   "t<text>": those letters typed, or "k<letter>": that letter's (or digit's) key held;
   the test joystick (RENFS_JOY): jl jr ju jd the stick to that side, jg / jk the fourth /
   third axis down (the throttle / the brake), jb1..jb4 its buttons (jb5 / jb6 a pad's right / left
   shoulder), jx<n> jy<n> jz<n>
   jw<n> the four axes' counts */
typedef struct {
    unsigned long at, until;
    int scan, buttons, x, y, joy, jc[4];
    char text[16];
} scripted;
static scripted g_script[64];
static int g_nscript;
static int g_script_buttons;

static unsigned char g_script_held[0x59];

static const struct { const char *name; int scan; } g_script_keys[] = {
    { "esc", 0x01 }, { "enter", 0x1c }, { "space", 0x39 }, { "up", 0x48 }, { "down", 0x50 },
    { "left", 0x4b }, { "right", 0x4d }, { "tab", 0x0f }, { "y", 0x15 }, { "n", 0x31 },
    { "j", 0x24 }, { "f1", 0x3b }, { "f2", 0x3c }, { "f3", 0x3d }, { "f4", 0x3e },
    { "bksp", 0x0e }, { "del", 0x53 }, { "f5", 0x3f }, { "f6", 0x40 }, { "f7", 0x41 },
    { "f8", 0x42 }, { "f9", 0x43 }, { "f10", 0x44 }, { "f11", 0x57 }, { "f12", 0x58 }, { "ctrl", 0x1d }, { "alt", 0x38 }, { "lshift", 0x2a }, { "rshift", 0x36 }, { "end", 0x4f },
};

static const SDL_Scancode g_pc_keys[0x59][2];

static void script_parse(const char *s)
{
    while (s && *s && g_nscript < 64) {
        char what[16];
        unsigned long at;
        int n = 0;
        scripted *e = &g_script[g_nscript];
        unsigned long until = 0;

        if (sscanf(s, " %lu-%lu:%15s%n", &at, &until, what, &n) != 3) {
            until = 0;
            if (sscanf(s, " %lu:%15s%n", &at, what, &n) != 2)
                break;
        }
        s += n;
        memset(e, 0, sizeof *e);
        e->at = at;
        e->until = until > at ? until : at + 6;
        e->x = e->jc[0] = e->jc[1] = e->jc[2] = e->jc[3] = -1;
        if (what[0] == 'j' && what[1] && !what[2] && strchr("lrudgk", what[1]))
            e->joy = 1 << (strchr("lrudgk", what[1]) - "lrudgk");
        else if (what[0] == 'j' && what[1] == 'b' && what[2] >= '1' && what[2] <= '6' && !what[3])
            e->joy = 0x100 << (what[2] - '1');
        else if (what[0] == 'j' && what[1] && strchr("xyzw", what[1]) && what[2])
            e->jc[strchr("xyzw", what[1]) - "xyzw"] = atoi(what + 2);
        else if (!strcmp(what, "click"))
            e->buttons = 1;
        else if (!strcmp(what, "rclick"))
            e->buttons = 2;
        else if (what[0] == 'm' && sscanf(what + 1, "%d,%d", &e->x, &e->y) == 2)
            ;
        else if (what[0] == 't' && what[1])
            snprintf(e->text, sizeof e->text, "%s", what + 1);
        else if (what[0] == 'k' && what[1] >= '1' && what[1] <= '9' && !what[2])
            e->scan = 2 + (what[1] - '1');
        else if (what[0] == 'k' && what[1] >= 'a' && what[1] <= 'z' && !what[2]) {
            for (int k = 0; k < 0x59; k++)
                if ((int)g_pc_keys[k][0] == SDL_SCANCODE_A + (what[1] - 'a'))
                    e->scan = k;
        }
        else {
            for (size_t k = 0; k < sizeof g_script_keys / sizeof g_script_keys[0]; k++)
                if (!strcmp(what, g_script_keys[k].name))
                    e->scan = g_script_keys[k].scan;
            if (!e->scan) {
                fprintf(stderr, "RENFS_INPUT: what is \"%s\"?\n", what);
                continue;
            }
        }
        g_nscript++;
    }
}

static void script_step(unsigned long retrace)
{
    g_script_buttons = 0;
    memset(g_script_held, 0, sizeof g_script_held);
    for (int i = 0; i < g_nscript; i++) {
        scripted *e = &g_script[i];
        if (e->x >= 0 && retrace == e->at) {
            g_mouse_x = e->x;
            g_mouse_y = e->y;
            mouse_clamp();
        }
        if (retrace == e->at) {            /* a press, for the key queue too */
            scan_put(e->scan);
            key_put(e->scan == 0x01 ? 0x1b : e->scan == 0x1c ? 0x0d : e->scan == 0x39 ? 0x20 :
                    e->scan == 0x0e ? 8 : e->scan == 0x53 ? 0x5300 : 0);
            for (const char *t = e->text; *t; t++)
                key_put((unsigned char)*t);
        }
        if (retrace >= e->at && retrace < e->until) {
            if (e->scan > 0 && e->scan < 0x59)
                g_script_held[e->scan] = 1;
            g_script_buttons |= e->buttons;
        }
    }
}

int video_init(const char *shot, int shot_frames)
{
    g_shot = shot;
    g_shot_at = shot_frames;
    g_ffdev_joystick = joy_sdl;    /* (the force feedback's device) */
    if (shot)
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
#ifdef MISTER_HYBRID
    if (!shot && mister_on()) {
        g_mister = 1;
        SDL_SetHint("SDL_MISTER_VIDEO_FORMAT", "INDEX8");
        SDL_SetHint("SDL_MISTER_DIRECT", "1");     /* the surface is the core's next picture */
    }
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
#ifdef MISTER_HYBRID
    if (g_mister) {
        if (!(g_win = SDL_CreateWindow("ReNFS", 0, 0, 640, 480, 0))) {
            fprintf(stderr, "SDL: %s\n", SDL_GetError());
            return 0;
        }
        video_mode(640, 480);
        script_parse(getenv("RENFS_INPUT"));
        g_start_ms = SDL_GetTicks();
        g_start_pc = SDL_GetPerformanceCounter();
        return 1;
    }
#endif
    g_win = SDL_CreateWindow("ReNFS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             1280, 960, SDL_WINDOW_RESIZABLE);
    if (!g_win) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
    if (!shot && files_cfg_get("fullscreen", 0))
        SDL_SetWindowFullscreen(g_win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_StartTextInput();                  /* the characters typed, for the names */
    g_ren = SDL_CreateRenderer(g_win, -1, shot ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren)
        g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    video_mode(640, 480);
    script_parse(getenv("RENFS_INPUT"));
    g_start_ms = SDL_GetTicks();
    g_start_pc = SDL_GetPerformanceCounter();
    return 1;
}

void video_done(void)
{
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    if (g_ren)
        SDL_DestroyRenderer(g_ren);
    if (g_win)
        SDL_DestroyWindow(g_win);
    SDL_Quit();
}

void video_mode(int w, int h)
{
    g_fb_w = w;
    g_fb_h = h;
    memset(g_fb, 0, sizeof g_fb);
#ifdef MISTER_HYBRID
    if (g_mister) {
        SDL_SetWindowSize(g_win, w, h);
        return;
    }
#endif
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    /* always 4:3, whatever the mode: 320x200 had tall pixels on a 4:3 screen */
    SDL_RenderSetLogicalSize(g_ren, 640, 480);
}

void video_palette(const unsigned char *pal, int first, int count)
{
    for (int i = first; i < first + count && i < 256; i++) {
        for (int c = 0; c < 3; c++) {
            g_pal8[3 * i + c] = pal[3 * (i - first) + c];
            g_dac[3 * i + c] = pal[3 * (i - first) + c] >> 2;
        }
        /* 6 bits to 8 as the screen saw them: the top bits repeated below */
        unsigned r = g_dac[3 * i], g = g_dac[3 * i + 1], b = g_dac[3 * i + 2];
        g_rgb[i] = 0xff000000u | (r << 2 | r >> 4) << 16 | (g << 2 | g >> 4) << 8 | (b << 2 | b >> 4);
    }
}

void video_get_palette(unsigned char *pal, int first, int count)
{
    for (int i = 0; i < 3 * count && 3 * first + i < 768; i++) {
        unsigned v = g_dac[3 * first + i];
        pal[i] = (unsigned char)(v << 2 | v >> 4);
    }
}

const unsigned char *video_palette8(void)
{
    return g_pal8;
}

/* the VGA's rates: 70 a second in the 200-line modes, 60 in the others */
static int retrace_rate(void)
{
    return g_fb_h == 200 ? 70 : 60;
}

static void write_shot(void)
{
    FILE *f = fopen(g_shot, "wb");
    if (!f) {
        perror(g_shot);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", g_fb_w, g_fb_h);
    for (int i = 0; i < g_fb_w * g_fb_h; i++) {
        int x = i % g_fb_w - g_mouse_x * g_fb_w / 640, y = i / g_fb_w - g_mouse_y * g_fb_h / 480;
        unsigned char v = g_fb[i];
        if (g_cursor && g_cursor_on && x >= 0 && y >= 0 && x < g_cursor_w &&
            y < g_cursor_h && g_cursor[y * g_cursor_w + x] != 255)
            v = g_cursor[y * g_cursor_w + x];  /* the pointer, as on the screen */
        unsigned c = g_rgb[v];
        unsigned char px[3] = { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    if (getenv("RENFS_SHOTRAW") && (f = fopen(getenv("RENFS_SHOTRAW"), "wb"))) {
        fwrite(g_fb, 1, (size_t)g_fb_w * g_fb_h, f);   /* the palette numbers, for tests */
        fclose(f);
    }
    printf("%s: frame %lu\n", g_shot, g_retraces);
}

/* (the port's) a pad's Start held: Esc, which the original's joystick has not (the pause
   menu in a race, back in the menus) */
static int g_pad_esc;

static void poll(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            g_quit = 1;
        else if (e.type == SDL_MOUSEMOTION && g_mouse_rel) {
            /* driving with the mouse: its movement counted, as the mouse driver's mickeys */
            g_mouse_x += e.motion.xrel;
            g_mouse_y += e.motion.yrel;
            mouse_clamp();
        } else if (e.type == SDL_MOUSEMOTION) {
            /* the renderer's logical size: already the 640x480 picture's pixels */
            int mx = e.motion.x, my = e.motion.y;
#ifdef MISTER_HYBRID
            if (g_mister) {                    /* (the window is the picture's size) */
                mx = mx * 640 / g_fb_w;
                my = my * 480 / g_fb_h;
            }
#endif
            g_mouse_x = mx < 0 ? 0 : mx > 639 ? 639 : mx;
            g_mouse_y = my < 0 ? 0 : my > 479 ? 479 : my;
            mouse_clamp();
        } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
            int bit = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
            if (e.type == SDL_MOUSEBUTTONDOWN)
                g_mouse_buttons |= bit;
            else
                g_mouse_buttons &= ~bit;
        } else if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
                   && e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            g_pad_esc = e.type == SDL_CONTROLLERBUTTONDOWN;
            if (g_pad_esc) {
                scan_put(1);
                key_put(0x1b);
            }
        } else if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_ALT)
                   && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)) {
            /* (the port's) Alt+Enter: the whole screen or a window, kept in renfs.cfg; the
               game sees no Enter */
            int full = !(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP);
            if (!e.key.repeat) {
                SDL_SetWindowFullscreen(g_win, full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                files_cfg_set("fullscreen", full);
            }
        } else if (e.type == SDL_KEYDOWN) {
            for (int k = 1; k < 0x59; k++)
                if (e.key.keysym.scancode == g_pc_keys[k][0] || (g_pc_keys[k][1]
                    && e.key.keysym.scancode == g_pc_keys[k][1]))
                    scan_put(k);
            /* the keys that type no character; held, they repeat as the BIOS's do */
            static const struct { SDL_Keycode k; int c; } typed[] = {
                { SDLK_ESCAPE, 0x1b }, { SDLK_RETURN, 0x0d }, { SDLK_KP_ENTER, 0x0d },
                { SDLK_BACKSPACE, 8 }, { SDLK_TAB, 9 }, { SDLK_DELETE, 0x5300 },
                { SDLK_UP, 0x4800 }, { SDLK_DOWN, 0x5000 }, { SDLK_LEFT, 0x4b00 },
                { SDLK_RIGHT, 0x4d00 }, { SDLK_INSERT, 0x5200 },
            };
            for (size_t i = 0; i < sizeof typed / sizeof typed[0]; i++)
                if (e.key.keysym.sym == typed[i].k)
                    key_put(typed[i].c);
        } else if (e.type == SDL_TEXTINPUT) {
            /* the characters, in the DOS code page (437): ASCII, and the German letters
               the game takes (ü ä ö ß µ Ä Ü Ö ° §) */
            static const struct { unsigned u; int c; } cp437[] = {
                { 0xfc, 0x81 }, { 0xe4, 0x84 }, { 0xf6, 0x94 }, { 0xdf, 0xe1 }, { 0xb5, 0xe6 },
                { 0xc4, 0x8e }, { 0xdc, 0x9a }, { 0xd6, 0x99 }, { 0xb0, 0xf8 }, { 0xa7, 0x15 },
            };
            for (const unsigned char *t = (const unsigned char *)e.text.text; *t;) {
                unsigned u = *t++;
                if (u >= 0xc0 && u < 0xe0 && (*t & 0xc0) == 0x80)
                    u = (u & 0x1f) << 6 | (*t++ & 0x3f);
                else if (u >= 0x80) {          /* longer: none the game knows */
                    while ((*t & 0xc0) == 0x80)
                        t++;
                    continue;
                }
                if (u >= 0x20 && u < 0x7f)
                    key_put((int)u);
                for (size_t i = 0; i < sizeof cp437 / sizeof cp437[0]; i++)
                    if (u == cp437[i].u)
                        key_put(cp437[i].c);
            }
        }
    }
}

#ifdef MISTER_HYBRID
/* the picture to the core: the palette numbers as they are, the pointer over them, the
   palette when it changed; then the next field */
static void retrace_mister(void)
{
    SDL_Surface *s = SDL_GetWindowSurface(g_win);
    if (s && s->format->BytesPerPixel == 1 && s->format->palette && s->w == g_fb_w && s->h == g_fb_h) {
        static unsigned shown[256];
        static int shown_ok;
        unsigned char *px = s->pixels;
        for (int y = 0; y < g_fb_h; y++)
            memcpy(px + y * s->pitch, g_fb + y * g_fb_w, (size_t)g_fb_w);
        if (g_cursor && g_cursor_on) {
            int mx = g_mouse_x * g_fb_w / 640, my = g_mouse_y * g_fb_h / 480;
            for (int y = 0; y < g_cursor_h && my + y < g_fb_h; y++)
                for (int x = 0; x < g_cursor_w && mx + x < g_fb_w; x++) {
                    unsigned char c = g_cursor[y * g_cursor_w + x];
                    if (c != 255)
                        px[(my + y) * s->pitch + mx + x] = c;
                }
        }
        if (!shown_ok || memcmp(shown, g_rgb, sizeof shown)) {
            SDL_Color c[256];
            for (int i = 0; i < 256; i++) {
                c[i].r = (Uint8)(g_rgb[i] >> 16);
                c[i].g = (Uint8)(g_rgb[i] >> 8);
                c[i].b = (Uint8)g_rgb[i];
                c[i].a = 255;
            }
            SDL_SetPaletteColors(s->format->palette, c, 0, 256);
            memcpy(shown, g_rgb, sizeof shown);
            shown_ok = 1;
        }
        SDL_UpdateWindowSurface(g_win);
    }
    mister_retrace_wait();
}
#endif

void video_retrace(void)
{
    void *pixels;
    int pitch;
    poll();
#ifdef MISTER_HYBRID
    if (g_mister) {
        retrace_mister();
        g_retraces++;
        g_virtual_ms += 4200 / retrace_rate();
        if (g_retrace_hook)
            g_retrace_hook();
        return;
    }
#endif
    if (SDL_LockTexture(g_tex, 0, &pixels, &pitch) == 0) {
        for (int y = 0; y < g_fb_h; y++) {
            unsigned *row = (unsigned *)((char *)pixels + y * pitch);
            const unsigned char *src = g_fb + y * g_fb_w;
            for (int x = 0; x < g_fb_w; x++)
                row[x] = g_rgb[src[x]];
        }
        /* the pointer (0x81600: its top left corner at the mouse; 255 is see-through) */
        /* (the mouse is kept in 640x480's pixels: at 320x200 in the race's) */
        if (g_cursor && g_cursor_on) {
            int mx = g_mouse_x * g_fb_w / 640, my = g_mouse_y * g_fb_h / 480;
            for (int y = 0; y < g_cursor_h && my + y < g_fb_h; y++)
                for (int x = 0; x < g_cursor_w && mx + x < g_fb_w; x++) {
                    unsigned char c = g_cursor[y * g_cursor_w + x];
                    if (c != 255)
                        ((unsigned *)((char *)pixels + (my + y) * pitch))[mx + x] = g_rgb[c];
                }
        }
        SDL_UnlockTexture(g_tex);
    }
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, 0, 0);
    SDL_RenderPresent(g_ren);
    g_retraces++;
    g_virtual_ms += 4200 / retrace_rate();
    if (g_nscript)
        script_step(g_retraces);
    if (g_retrace_hook)
        g_retrace_hook();
    if (g_shot) {
        if ((int)g_retraces >= g_shot_at) {
            write_shot();
            g_quit = 1;
            exit(0);
        }
        return;
    }
    /* keep the card's rate whatever the monitor's (a fade is so many retraces) */
    {
        static Uint64 next;
        Uint64 now = SDL_GetPerformanceCounter(), f = SDL_GetPerformanceFrequency();
        Uint64 step = f / retrace_rate();
        if (!next || now > next + f / 10)
            next = now;
        next += step;
        while ((now = SDL_GetPerformanceCounter()) < next) {
            Uint64 left_ms = (next - now) * 1000 / f;
            if (left_ms > 1)
                SDL_Delay((Uint32)(left_ms - 1));
        }
    }
}

unsigned clock_ticks(void)
{
    if (g_shot)            /* g_virtual_ms counts 1/4200 s: 100 a second = 42 */
        return (unsigned)(g_virtual_ms / 42);
    return (SDL_GetTicks() - g_start_ms) / 10;
}

double clock_fine(void)
{
    if (g_shot)
        return g_virtual_ms / 42.0;
    return (double)(SDL_GetPerformanceCounter() - g_start_pc) * 100.0
           / (double)SDL_GetPerformanceFrequency();
}

/* 0xed0fc: one more at every tick of the 100 Hz timer (race_timer_tick 0x4c454), set to 0
   by the screens that time something with it; the menus' music is chosen by it */
static unsigned g_counter_from;

unsigned long timer_counter(void)
{
    return clock_ticks() - g_counter_from;
}

void timer_counter_set(unsigned long v)
{
    g_counter_from = clock_ticks() - (unsigned)v;
}

static unsigned g_timeout_at;

void settimeout(unsigned n)
{
    g_timeout_at = clock_ticks() + n;
}

int timedout(void)
{
    return (int)(clock_ticks() - g_timeout_at) >= 0;
}

int key_get(void)
{
    int c;
    poll();
    if (!g_nkeys)
        return 0;
    c = g_keys[0];
    memmove(g_keys, g_keys + 1, --g_nkeys * sizeof g_keys[0]);
    return c;
}

int scan_get(void)
{
    int c;
    poll();
    if (!g_nscans)
        return 0;
    c = g_scans[0];
    memmove(g_scans, g_scans + 1, --g_nscans * sizeof g_scans[0]);
    return c;
}

/* PC scan codes (set 1, as the game's keyboard handler sees them) to SDL's: the keys of
   the extended block give the same codes as the keypad's */
static const SDL_Scancode g_pc_keys[0x59][2] = {
    [0x01] = { SDL_SCANCODE_ESCAPE },
    [0x02] = { SDL_SCANCODE_1 }, [0x03] = { SDL_SCANCODE_2 }, [0x04] = { SDL_SCANCODE_3 },
    [0x05] = { SDL_SCANCODE_4 }, [0x06] = { SDL_SCANCODE_5 }, [0x07] = { SDL_SCANCODE_6 },
    [0x08] = { SDL_SCANCODE_7 }, [0x09] = { SDL_SCANCODE_8 }, [0x0a] = { SDL_SCANCODE_9 },
    [0x0b] = { SDL_SCANCODE_0 }, [0x0c] = { SDL_SCANCODE_MINUS }, [0x0d] = { SDL_SCANCODE_EQUALS },
    [0x0e] = { SDL_SCANCODE_BACKSPACE }, [0x0f] = { SDL_SCANCODE_TAB },
    [0x10] = { SDL_SCANCODE_Q }, [0x11] = { SDL_SCANCODE_W }, [0x12] = { SDL_SCANCODE_E },
    [0x13] = { SDL_SCANCODE_R }, [0x14] = { SDL_SCANCODE_T }, [0x15] = { SDL_SCANCODE_Y },
    [0x16] = { SDL_SCANCODE_U }, [0x17] = { SDL_SCANCODE_I }, [0x18] = { SDL_SCANCODE_O },
    [0x19] = { SDL_SCANCODE_P }, [0x1a] = { SDL_SCANCODE_LEFTBRACKET },
    [0x1b] = { SDL_SCANCODE_RIGHTBRACKET }, [0x1c] = { SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER },
    [0x1d] = { SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL },
    [0x1e] = { SDL_SCANCODE_A }, [0x1f] = { SDL_SCANCODE_S }, [0x20] = { SDL_SCANCODE_D },
    [0x21] = { SDL_SCANCODE_F }, [0x22] = { SDL_SCANCODE_G }, [0x23] = { SDL_SCANCODE_H },
    [0x24] = { SDL_SCANCODE_J }, [0x25] = { SDL_SCANCODE_K }, [0x26] = { SDL_SCANCODE_L },
    [0x27] = { SDL_SCANCODE_SEMICOLON }, [0x28] = { SDL_SCANCODE_APOSTROPHE },
    [0x29] = { SDL_SCANCODE_GRAVE }, [0x2a] = { SDL_SCANCODE_LSHIFT },
    [0x2b] = { SDL_SCANCODE_BACKSLASH }, [0x2c] = { SDL_SCANCODE_Z }, [0x2d] = { SDL_SCANCODE_X },
    [0x2e] = { SDL_SCANCODE_C }, [0x2f] = { SDL_SCANCODE_V }, [0x30] = { SDL_SCANCODE_B },
    [0x31] = { SDL_SCANCODE_N }, [0x32] = { SDL_SCANCODE_M }, [0x33] = { SDL_SCANCODE_COMMA },
    [0x34] = { SDL_SCANCODE_PERIOD }, [0x35] = { SDL_SCANCODE_SLASH, SDL_SCANCODE_KP_DIVIDE },
    [0x36] = { SDL_SCANCODE_RSHIFT }, [0x37] = { SDL_SCANCODE_KP_MULTIPLY },
    [0x38] = { SDL_SCANCODE_LALT, SDL_SCANCODE_RALT }, [0x39] = { SDL_SCANCODE_SPACE },
    [0x3a] = { SDL_SCANCODE_CAPSLOCK },
    [0x3b] = { SDL_SCANCODE_F1 }, [0x3c] = { SDL_SCANCODE_F2 }, [0x3d] = { SDL_SCANCODE_F3 },
    [0x3e] = { SDL_SCANCODE_F4 }, [0x3f] = { SDL_SCANCODE_F5 }, [0x40] = { SDL_SCANCODE_F6 },
    [0x41] = { SDL_SCANCODE_F7 }, [0x42] = { SDL_SCANCODE_F8 }, [0x43] = { SDL_SCANCODE_F9 },
    [0x44] = { SDL_SCANCODE_F10 }, [0x45] = { SDL_SCANCODE_NUMLOCKCLEAR },
    [0x46] = { SDL_SCANCODE_SCROLLLOCK },
    [0x47] = { SDL_SCANCODE_HOME, SDL_SCANCODE_KP_7 }, [0x48] = { SDL_SCANCODE_UP, SDL_SCANCODE_KP_8 },
    [0x49] = { SDL_SCANCODE_PAGEUP, SDL_SCANCODE_KP_9 }, [0x4a] = { SDL_SCANCODE_KP_MINUS },
    [0x4b] = { SDL_SCANCODE_LEFT, SDL_SCANCODE_KP_4 }, [0x4c] = { SDL_SCANCODE_KP_5 },
    [0x4d] = { SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_6 }, [0x4e] = { SDL_SCANCODE_KP_PLUS },
    [0x4f] = { SDL_SCANCODE_END, SDL_SCANCODE_KP_1 }, [0x50] = { SDL_SCANCODE_DOWN, SDL_SCANCODE_KP_2 },
    [0x51] = { SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_KP_3 },
    [0x52] = { SDL_SCANCODE_INSERT, SDL_SCANCODE_KP_0 },
    [0x53] = { SDL_SCANCODE_DELETE, SDL_SCANCODE_KP_PERIOD },
    [0x57] = { SDL_SCANCODE_F11 }, [0x58] = { SDL_SCANCODE_F12 },
};

int key_down(int scan)
{
    const Uint8 *k = SDL_GetKeyboardState(0);
    if (scan <= 0 || scan >= 0x59)
        return 0;
    if (g_script_held[scan] || (scan == 1 && g_pad_esc))
        return 1;
    for (int i = 0; i < 2; i++)
        if (g_pc_keys[scan][i] && k[g_pc_keys[scan][i]])
            return 1;
    return 0;
}

int mouse_read(int *x, int *y)
{
    if (x)
        *x = g_mouse_x;
    if (y)
        *y = g_mouse_y;
    return g_mouse_buttons | g_script_buttons;
}

void mouse_limits(int x0, int y0, int x1, int y1)
{
    g_lim_x0 = x0;
    g_lim_y0 = y0;
    g_lim_x1 = x1 < 639 ? x1 : 639;
    g_lim_y1 = y1 < 479 ? y1 : 479;
    mouse_clamp();
}

void mouse_relative(int on)
{
    if (on == g_mouse_rel)
        return;
    g_mouse_rel = on;
    if (!on)                               /* (the menus' whole screen again) */
        mouse_limits(0, 0, 640, 480);
    if (!g_shot) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
        SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
    }
}

/* the joystick: a game controller (its left stick, the left trigger the third axis (the
   brake), the right the fourth (the throttle), A B X Y the four buttons, the pad as the
   stick) or else a joystick (its first four axes, its hat as the stick, its first four
   buttons); and the port's own two (16, 32): a pad's right and left shoulder, a
   joystick's fifth and sixth buttons; RENFS_JOY=1 (four axes) or 2 (two) a test joystick played by RENFS_INPUT */
static SDL_GameController *g_pad;
static SDL_Joystick *g_joy;

static int joy_count(int v)                /* -32768..32767 as the game port's 0..511 */
{
    return (v + 32768) >> 7;
}

void *joy_sdl(void)
{
    if (getenv("RENFS_JOY") || g_shot)
        return 0;
    if (g_pad && SDL_GameControllerGetAttached(g_pad))
        return SDL_GameControllerGetJoystick(g_pad);
    return g_joy && SDL_JoystickGetAttached(g_joy) ? g_joy : 0;
}

int joy_device(int count[4], int *buttons)
{
    const char *test = getenv("RENFS_JOY");
    int mask = 0, hat = 0;

    count[0] = count[1] = count[2] = count[3] = 0;     /* (an axis it lacks) */
    *buttons = 0;
    if (test || g_shot) {
        int j = 0, jc[4] = { -1, -1, -1, -1 };
        if (!test || !atoi(test))
            return 0;
        for (int i = 0; i < g_nscript; i++)    /* (as the script has it at this retrace) */
            if (g_retraces >= g_script[i].at && g_retraces < g_script[i].until) {
                j |= g_script[i].joy;
                for (int a = 0; a < 4; a++)
                    if (g_script[i].jc[a] >= 0)
                        jc[a] = g_script[i].jc[a];
            }
        count[0] = jc[0] >= 0 ? jc[0] : j & 1 ? 0 : j & 2 ? 511 : 256;
        count[1] = jc[1] >= 0 ? jc[1] : j & 4 ? 0 : j & 8 ? 511 : 256;
        *buttons = j >> 8 & 0x3f;
        if (atoi(test) == 2)
            return 3;
        count[2] = jc[2] >= 0 ? jc[2] : j & 0x20 ? 0 : 511;
        count[3] = jc[3] >= 0 ? jc[3] : j & 0x10 ? 0 : 511;
        return 0xf;
    }
    if (g_pad && !SDL_GameControllerGetAttached(g_pad)) {
        SDL_GameControllerClose(g_pad);
        g_pad = 0;
    }
    if (g_joy && !SDL_JoystickGetAttached(g_joy)) {
        SDL_JoystickClose(g_joy);
        g_joy = 0;
    }
    /* the first game controller or joystick with two axes (a keyboard's media keys may
       be listed as a joystick too: one axis, many buttons); again when plugged in again */
    for (int i = 0; !g_pad && !g_joy && i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i))
            g_pad = SDL_GameControllerOpen(i);
        else if ((g_joy = SDL_JoystickOpen(i)) && SDL_JoystickNumAxes(g_joy) < 2) {
            SDL_JoystickClose(g_joy);
            g_joy = 0;
        }
    }
    if (g_pad) {
        static const SDL_GameControllerButton b[6] = {
            SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
            SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER };
        mask = 3;
        count[0] = joy_count(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX));
        count[1] = joy_count(SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY));
        /* (bound axes asked so, not SDL_GameControllerHasAxis: SDL 2.0.14's, newer than
           the systems the Linux release is for) */
        if (SDL_GameControllerGetBindForAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT).bindType
                != SDL_CONTROLLER_BINDTYPE_NONE
            && SDL_GameControllerGetBindForAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT).bindType
                != SDL_CONTROLLER_BINDTYPE_NONE) {
            /* the pedals: a pedal pressed reads low, as the game port's */
            count[2] = 511 - (SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 6);
            count[3] = 511 - (SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 6);
            mask = 0xf;
        }
        for (int i = 0; i < 6; i++)
            if (SDL_GameControllerGetButton(g_pad, b[i]))
                *buttons |= 1 << i;
        hat = (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_UP) ? SDL_HAT_UP : 0)
            | (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ? SDL_HAT_DOWN : 0)
            | (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? SDL_HAT_LEFT : 0)
            | (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? SDL_HAT_RIGHT : 0);
    } else if (g_joy) {
        int n = SDL_JoystickNumAxes(g_joy);
        for (int i = 0; i < 4 && i < n; i++) {
            count[i] = joy_count(SDL_JoystickGetAxis(g_joy, i));
            mask |= 1 << i;
        }
        for (int i = 0; i < 6 && i < SDL_JoystickNumButtons(g_joy); i++)
            if (SDL_JoystickGetButton(g_joy, i))
                *buttons |= 1 << i;
        if (SDL_JoystickNumHats(g_joy) > 0) {
            hat = SDL_JoystickGetHat(g_joy, 0);
            if (n < 2)
                count[0] = count[1] = 256;
            mask |= 3;
        }
    }
    if (hat & SDL_HAT_LEFT)
        count[0] = 0;
    if (hat & SDL_HAT_RIGHT)
        count[0] = 511;
    if (hat & SDL_HAT_UP)
        count[1] = 0;
    if (hat & SDL_HAT_DOWN)
        count[1] = 511;
    return mask;
}

void mouse_place(int x, int y)
{
    g_mouse_x = x;
    g_mouse_y = y;
    mouse_clamp();
    x = g_mouse_x;
    y = g_mouse_y;
#ifdef MISTER_HYBRID
    if (g_mister) {
        SDL_WarpMouseInWindow(g_win, x * g_fb_w / 640, y * g_fb_h / 480);
        return;
    }
#endif
    if (!g_shot && (SDL_GetWindowFlags(g_win) & SDL_WINDOW_MOUSE_FOCUS)) {
        /* the window's pixels from the picture's (the renderer keeps it 4:3, centred) */
        int ww, wh;
        float sx, sy;
        SDL_Rect vp;
        SDL_GetWindowSize(g_win, &ww, &wh);
        SDL_RenderGetScale(g_ren, &sx, &sy);
        SDL_RenderGetViewport(g_ren, &vp);
        SDL_WarpMouseInWindow(g_win, (int)((x + vp.x) * sx), (int)((y + vp.y) * sy));
    }
}

void mouse_pointer(const unsigned char *pixels, int w, int h)
{
    g_cursor = pixels;
    g_cursor_w = w;
    g_cursor_h = h;
}

void mouse_show(int on)
{
    g_cursor_on = on;
    if (!g_shot)
        SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}
