/* What every menu screen shares. */
#include "menu.h"
#include "mister.h"
#include "files.h"
#include "fsh.h"
#include "joystick.h"
#include "sound.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

unsigned ticksince(void)
{
    static unsigned last;
    unsigned now = clock_ticks(), d = now - last;
    last = now;
    return d;
}

int g_fkeys;                   /* 0x12bd3c: F1-F4 read too (set by the race's options) */

int fe_read_keys(void)
{
    static const struct { int code, scan; } keys[] = {
        { 0x1b, 0x01 }, { 0x20, 0x39 }, { 0x0d, 0x1c }, { 0x4800, 0x48 }, { 0x5000, 0x50 },
        { 0x4b00, 0x4b }, { 0x4d00, 0x4d }, { 0x5200, 0x52 }, { 0x5300, 0x53 },
    };
    static const int fkeys[4] = { 0x3b, 0x3c, 0x3d, 0x3e };
    static const int raw[] = { 0x2a, 0x36, 0x1d, 0x38, 0x15, 0x31, 0x24 };
    if (key_down(0x44) && key_down(0x1d))  /* Ctrl+F10: the modem game's break */
        return 0;
    if (key_down(0x3f)) {                  /* F5: 0x72638, the sound off or on again */
        fe_sound_mute();
        return 0;
    }
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (key_down(keys[i].scan))
            return keys[i].code;
    for (int i = 0; g_fkeys && i < 4; i++)
        if (key_down(fkeys[i]))
            return fkeys[i] << 8;
    for (size_t i = 0; i < sizeof raw / sizeof raw[0]; i++)
        if (key_down(raw[i]))
            return raw[i];
    /* Tab: nothing; else the joystick's buttons, else its direction (0x4dfe0) */
    if (key_down(0x0f))
        return 0;
    return joy_menu_key();
}

void fe_flush_keys(void)
{
    while (key_get())
        ;
    while (scan_get())
        ;
}

void fe_draw_shape(const shape *s, int see_through)
{
    for (int y = 0; y < s->h; y++) {
        int sy = s->y + y;
        if (sy < 0 || sy >= g_fb_h)
            continue;
        for (int x = 0; x < s->w; x++) {
            int sx = s->x + x;
            unsigned char c = s->pixels[y * s->w + x];
            if (sx >= 0 && sx < g_fb_w && !(see_through && c == 255))
                g_fb[sy * g_fb_w + sx] = c;
        }
    }
}

/* the mouse */
int g_mouse_active, g_mouse_buttons;
static int g_saved_x, g_saved_y;
static fsh_file g_mouse_fsh;
static shape g_ptr[2];

void fe_mouse_load(void)
{
    char path[256];
    if (g_mouse_fsh.d)
        return;
    snprintf(path, sizeof path, "%s/MOUSE.FSH", art_dir());
    if (!fsh_load(&g_mouse_fsh, path) || !fsh_find(&g_mouse_fsh, "ptr0", &g_ptr[0]) ||
        !fsh_find(&g_mouse_fsh, "ptr1", &g_ptr[1])) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    mouse_pointer(g_ptr[0].pixels, g_ptr[0].w, g_ptr[0].h);
}

void fe_mouse_read(void)
{
    g_mouse_buttons = mouse_read(0, 0);
}

void fe_mouse_save(void)
{
    mouse_read(&g_saved_x, &g_saved_y);
}

void fe_mouse_moved(void)
{
    int x, y;
    mouse_read(&x, &y);
    if (!g_mouse_active && (g_mouse_buttons || x != g_saved_x || y != g_saved_y))
        g_mouse_active = 1;
}

void fe_mouse_hide(void)
{
    if (g_mouse_active) {
        mouse_show(0);
        g_mouse_active = 0;
    }
}

void fe_mouse_look(int pressed)
{
    fe_mouse_load();
    if (g_mouse_fsh.d)
        mouse_pointer(g_ptr[pressed != 0].pixels, g_ptr[pressed != 0].w, g_ptr[pressed != 0].h);
    mouse_show(1);
}

void fe_mouse_press(void)
{
    if (g_mouse_fsh.d)
        mouse_pointer(g_ptr[1].pixels, g_ptr[1].w, g_ptr[1].h);
    settimeout(20);
    while (!timedout() && !g_quit)
        video_retrace();
    if (g_mouse_fsh.d)
        mouse_pointer(g_ptr[0].pixels, g_ptr[0].w, g_ptr[0].h);
    settimeout(12000);
}

/* the text */
font g_fe_font;

int fe_font_load(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/FONTS/GRAVER18.FFN", art_dir());
    if (g_fe_font.d)
        return 1;
    if (!font_load(&g_fe_font, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return 0;
    }
    return 1;
}

/* the slots: white, beige, yellow, red, the chosen one, black (0x152f04, 0x153f44,
   0x154fd4, 0x150b84, 0x14fb44, 0x151bc4) */
static text_style g_white, g_beige, g_yellow, g_red, g_chosen, g_black;

void fe_styles_init(void)
{
    /* textcolour(nearestcolour(rgb), 0) each, copied out (0x8ecc4 is source first) */
    text_colour(nearest_colour(0xffffff));
    g_white = g_text;
    text_colour(nearest_colour(0xe8d3b7));
    g_beige = g_text;
    text_colour(nearest_colour(0xffce06));
    g_yellow = g_text;
    text_colour(nearest_colour(0xff0606));
    g_red = g_text;
    text_colour(nearest_colour(0xffce06));
    g_chosen = g_text;
    text_colour(nearest_colour(0x000000));
    g_black = g_text;
}

void fe_style(int n)
{
    g_chosen = n == 3 ? g_white : n == 2 ? g_red : n == 1 ? g_yellow : g_beige;
}

void fe_text(const char *s, int x, int y)
{
    g_text = g_black;
    text_draw(&g_fe_font, s, x + 2, y + 2);
    g_text = g_chosen;
    text_draw(&g_fe_font, s, x, y);
}

void fe_button_text(const char *s, int x, int y)
{
    /* drawn in a window of the text's width + 10 by 45 holding the screen there */
    text_clip(x, y, x + text_width(&g_fe_font, s) + 10, y + 45);
    fe_text(s, x, y);
    text_clip_off();
}

void fe_text_centre(const char *s, int x, int y)
{
    fe_text(s, x - text_width(&g_fe_font, s) / 2, y);
}

/* 0x89f3c with 0x89da0: each colour scaled (16.16, rounded) and capped at 63 (the
   library thought in 6-bit colours; the menus' palettes are 8-bit, so everything comes
   out dark), then the nearest of entries 0..253, green weighed 3, red 2, blue 1 */
static void darken_table(unsigned char out[256], const unsigned char *pal, long scale)
{
    int last = 0;
    for (int i = 0; i < 256; i++) {
        long t[3];
        long best = 9999;
        for (int c = 0; c < 3; c++) {
            long p = pal[3 * i + c] * scale;
            t[c] = (p >> 16) + (p >> 15 & 1);
            if (t[c] > 63)
                t[c] = 63;
        }
        for (int j = 0; j < 254; j++) {
            long d = 2 * (t[0] - pal[3 * j]) * (t[0] - pal[3 * j]);
            if (d >= best)
                continue;
            d += 3 * (t[1] - pal[3 * j + 1]) * (t[1] - pal[3 * j + 1]);
            if (d >= best)
                continue;
            d += (t[2] - pal[3 * j + 2]) * (t[2] - pal[3 * j + 2]);
            if (d < best) {
                best = d;
                last = j;
            }
        }
        out[i] = (unsigned char)last;      /* none near: the one before (the stack's) */
    }
}

/* 0x8a0f0: a frame of one pixel */
static void frame(int x0, int y0, int x1, int y1, int colour)
{
    for (int x = x0; x <= x1; x++)
        g_fb[y0 * g_fb_w + x] = g_fb[y1 * g_fb_w + x] = (unsigned char)colour;
    for (int y = y0 + 1; y < y1; y++)
        g_fb[y * g_fb_w + x0] = g_fb[y * g_fb_w + x1] = (unsigned char)colour;
}

void fe_message_box(const char *text)
{
    unsigned char remap[256];
    int w = text_width(&g_fe_font, text) + 20, x;

    x = 320 - w / 2;
    darken_table(remap, video_palette8(), 0x9000);
    for (int y = 210; y < 270 && y < g_fb_h; y++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < g_fb_w)
                g_fb[y * g_fb_w + i] = remap[g_fb[y * g_fb_w + i]];
    frame(x + 2, 212, x + w - 3, 267, nearest_colour(0xff0000));
    frame(x + 3, 213, x + w - 4, 266, nearest_colour(0xff0000));
    fe_styles_init();
    fe_style(2);
    fe_text_centre(text, x + w / 2, 228);
    video_retrace();
}

void fe_box(int bx, int by, int w, int h)
{
    unsigned char remap[256];

    darken_table(remap, video_palette8(), 0x9000);
    for (int y = by; y < by + h && y < g_fb_h; y++)
        for (int x = bx; x < bx + w && x < g_fb_w; x++)
            g_fb[y * g_fb_w + x] = remap[g_fb[y * g_fb_w + x]];
    frame(bx + 2, by + 2, bx + w - 3, by + h - 3, nearest_colour(0xff0000));
    frame(bx + 3, by + 3, bx + w - 4, by + h - 4, nearest_colour(0xff0000));
}

void fe_timed_message(const char *text, unsigned ticks)
{
    enum { X = 100, Y = 210, W = 440, H = 60 };
    static unsigned char saved[W * H];

    for (int y = 0; y < H; y++)
        memcpy(saved + y * W, g_fb + (Y + y) * g_fb_w + X, W);
    fe_message_box(text);
    settimeout(ticks);
    g_mouse_buttons = 0;
    while (!timedout() && !fe_read_keys() && !g_mouse_buttons && !g_quit) {
        video_retrace();
        if (g_mouse_active)
            fe_mouse_read();
    }
    settimeout(10);
    while (!timedout() && !g_quit)
        video_retrace();
    for (int y = 0; y < H; y++)
        memcpy(g_fb + (Y + y) * g_fb_w + X, saved + y * W, W);
}

void fe_text_cut(const char *s, int x, int y, int w)
{
    char cut[50];
    int n;

    memset(cut, 0, sizeof cut);
    for (n = 7; n < 50; n++) {
        strncpy(cut, s, n);
        if (text_width(&g_fe_font, cut) > w)
            break;
    }
    fe_text(cut, x, y);
}

int fe_yes_no_dialog(const char *question)
{
    enum { BX = 150, BY = 190, BW = 340, BH = 100 };
    static const int mouse_x[2] = { 225, 420 }, mouse_y[2] = { 260, 260 };
    static unsigned char saved[BW * BH];
    const char *yes = g_german ? "Ja" : "Yes", *no = g_german ? "Nein" : "No";
    unsigned char remap[256];
    int sel = 1, changed = 1, ready = 0, released = 0, done = 0, result = 0;

    for (int y = 0; y < BH; y++)           /* what was under it */
        memcpy(saved + y * BW, g_fb + (BY + y) * g_fb_w + BX, BW);
    if (!g_mouse_active) {
        mouse_place(mouse_x[sel], mouse_y[sel]);
        fe_mouse_save();
    } else
        fe_mouse_read();
    /* the box: the screen under it darkened (0x8a0cf through the table), two red frames */
    darken_table(remap, video_palette8(), 0x9000);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            unsigned char *p = &g_fb[(BY + y) * g_fb_w + BX + x];
            *p = remap[*p];
        }
    frame(BX + 2, BY + 2, BX + 337, BY + 97, nearest_colour(0xff0000));
    frame(BX + 3, BY + 3, BX + 336, BY + 96, nearest_colour(0xff0000));
    fe_styles_init();
    fe_style(2);
    fe_text_centre(question, BX + 170, BY + 20);
    fe_style(0);
    fe_text(yes, BX + 60, BY + 60);
    fe_style(1);
    fe_text(no, BX + 250, BY + 60);
    video_retrace();
    mouse_show(1);
    while (fe_read_keys() && !g_quit) {
        fe_mouse_read();
        video_retrace();
    }
    fe_flush_keys();
    while (!done && !g_quit) {
        int mx, my, k;
        mouse_read(&mx, &my);
        if (mx >= 0xbe && mx <= 0x104 && my >= 0xf5 && my <= 0x118) {
            if (sel) {
                sel = 0;
                changed = 1;
            }
        } else if (mx >= 0x17c && mx <= 0x1cc && my >= 0xf5 && my <= 0x118 && sel != 1) {
            sel = 1;
            changed = 1;
        }
        if (changed) {                     /* at the start too: drawn twice, brighter */
            fe_play_sound(0x47);
            fe_style(sel ? 0 : 1);
            fe_button_text(yes, BX + 60, BY + 60);
            fe_style(sel ? 1 : 0);
            fe_button_text(no, BX + 250, BY + 60);
            changed = 0;
        }
        if (g_mouse_active)
            fe_mouse_read();
        if (!g_mouse_buttons)
            released = 1;
        fe_mouse_moved();
        k = fe_read_keys();
        if (k == 0x20 || k == 0x0d || (g_mouse_buttons && released)) {
            int ok = ready;
            fe_mouse_press();
            if (ok) {
                fe_play_sound(0x48);
                result = g_mouse_buttons != 2 && !sel;    /* the right button: no */
                done = 1;
            }
        } else if (k && ready) {
            if (k == 0x1b || k == 0x31) {                  /* Esc, N */
                fe_play_sound(0x48);
                done = 1;
            } else if ((k == 0x15 && !g_german) || (k == 0x24 && g_german)) {  /* Y, J */
                fe_play_sound(0x48);
                result = done = 1;
            } else if (k == 0x4b00 || k == 0x4d00) {
                g_mouse_active = 0;
                sel ^= 1;
                mouse_place(mouse_x[sel], mouse_y[sel]);
                fe_mouse_save();
                changed = 1;
            }
            ready = 0;
        } else if (!k)
            ready = 1;
        video_retrace();
    }
    for (int y = 0; y < BH; y++)
        memcpy(g_fb + (BY + y) * g_fb_w + BX, saved + y * BW, BW);
    video_retrace();
    return result;
}

int fe_ask_exit(void)
{
    char q[64];
    if (g_german)
        snprintf(q, sizeof q, "Zur%cck zum system ?", 0x81);
    else
        snprintf(q, sizeof q, "Exit to system ?");
    return fe_yes_no_dialog(q);
}

#ifdef MISTER_HYBRID
/* (the MiSTer's) a name typed with the pad (`j` the pad as the menus read it): up and down
   the letter at the end through the letters, right one more letter, left one less; held,
   again after 0.4 s and then every 0.1 s. Returns a key for the field: 8 one less, Esc,
   a letter to add, or 0x4800 for the last one changed in buf (drawn again); 0 none */
static int pad_letters(int j, char *buf, int len, int maxlen, int names)
{
    static const char name_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.";
    static const char file_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
    static int last;
    static unsigned long next;
    const char *set = names ? name_set : file_set;
    unsigned long now = clock_ticks();
    const char *at;

    if (j == 0x1b)
        return j;
    if (j != 0x4800 && j != 0x5000 && j != 0x4b00 && j != 0x4d00) {
        last = 0;
        return 0;
    }
    if (j == last && now < next)
        return 0;
    next = now + (j == last ? 10 : 40);
    last = j;
    if (j == 0x4b00)
        return len > 0 ? 8 : 0;
    if (j == 0x4d00)
        return len < maxlen ? (len > 0 ? buf[len - 1] : 'A') : 0;
    if (len == 0)
        return 'A';
    at = strchr(set, buf[len - 1]);
    if (!at || !buf[len - 1])
        at = set;
    else if (j == 0x4800)
        at = at[1] ? at + 1 : set;
    else
        at = at > set ? at - 1 : set + strlen(set) - 1;
    buf[len - 1] = *at;
    return 0x4800;
}
#endif

/* 0x701cc: a line typed in, centred on cx at y, in a window of maxlen / 2 * 30 + 5 each
   side by 35 lines that holds what was there. For a name (names = 1) space . * + [ ] :
   ; " < > , ? are let in; for a file name none of them, and letters become capitals;
   % \ / | never. In German also ü ä ö ß µ Ä Ü Ö ° §. Backspace, Del (all of it); Enter
   (not when only spaces) keeps it, Esc not. The cursor "_" blinks every 15 hundredths. */
int fe_text_field(char *text, int maxlen, int cx, int y, int names)
{
    enum { H = 35 };
    static unsigned char saved[640 * H];
    char buf[42];
    int half = maxlen / 2 * 30, left = cx - half - 5, w = 2 * half + 10;
    int len = (int)strlen(text), tw, blink = 0, result = -1;
    unsigned waited = 0;

    if (w > 640)
        w = 640;
    if (len > 41)
        len = 41;
    memset(buf, 0, sizeof buf);
    memcpy(buf, text, (size_t)len);
    fe_flush_keys();
    if (g_mouse_active)
        fe_mouse_read();
    while ((fe_read_keys() || g_mouse_buttons) && !g_quit) {
        if (g_mouse_active)
            fe_mouse_read();
        video_retrace();
    }
    for (int r = 0; r < H; r++)
        memcpy(saved + r * w, g_fb + (y + r) * g_fb_w + left, (size_t)w);
    fe_style(1);
    tw = text_width(&g_fe_font, buf);
#define FIELD_DRAW(s)                                                       \
    do {                                                                    \
        for (int r = 0; r < H; r++)                                         \
            memcpy(g_fb + (y + r) * g_fb_w + left, saved + r * w, (size_t)w); \
        text_clip(left, y, left + w, y + H);                                \
        fe_text(s, left + w / 2 - tw / 2, y);                               \
        text_clip_off();                                                    \
    } while (0)
    FIELD_DRAW(buf);
    while ((fe_read_keys() || g_mouse_buttons) && !g_quit) {
        if (g_mouse_active)
            fe_mouse_read();
        video_retrace();
    }
    fe_flush_keys();
    ticksince();
    /* the mouse buttons are read once before the loop, after they were let go: a click
       in it does nothing (the original's code would take left as Enter, right as Esc) */
    while (result < 0 && !g_quit) {
        int j = joy_menu_key(), k = key_get();
        if (!k && (j == 0x20 || j == 0x0d))
            k = 0x0d;                      /* (a joystick's button, 0x70426) */
#ifdef MISTER_HYBRID
        if (!k && mister_on())
            k = pad_letters(j, buf, len, maxlen, names);
#endif
        if (k == 0x0d) {
            int i = 0;
            while (i < len && buf[i] == ' ')
                i++;
            if (i == len)
                k = 0;                     /* nothing but spaces */
        }
        if (k == 0x0d) {
            memset(text, 0, (size_t)maxlen + 1);
            memcpy(text, buf, (size_t)(len < maxlen ? len : maxlen));
            result = 1;
        } else if (k == 0x1b) {
            result = 0;
        } else if (k) {
            static const char banned[] = " .\\/%*+[]|:;\"<>,?";
            if (k < 0x80 && strchr(banned, k) &&
                (!names || k == '%' || k == '\\' || k == '/' || k == '|'))
                ;
            else if (k == 8 && len > 0)
                buf[--len] = 0;
            else if (k == 0x5300 && len > 0) {
                len = 0;
                memset(buf, 0, sizeof buf);
            } else if (len < maxlen && text_width(&g_fe_font, buf) < 440) {
                static const unsigned char german[] = { 0x81, 0x84, 0x94, 0xe1, 0xe6, 0x8e, 0x9a, 0x99, 0xf8, 0x15 };
                if (k > 0x1f && k < 0x7f) {
                    if (!names && k > 0x60 && k < 0x7b)
                        k -= 0x20;
                    buf[len++] = (char)k;
                } else if (g_german && k < 0x100 && memchr(german, k, sizeof german))
                    buf[len++] = (char)k;
            }
            /* any key draws it again, without the cursor */
            tw = text_width(&g_fe_font, buf);
            FIELD_DRAW(buf);
            ticksince();
        } else if ((waited += ticksince()) >= 15) {
            char shown[44];
            blink ^= 1;
            snprintf(shown, sizeof shown, "%s_", buf);
            FIELD_DRAW(blink ? shown : buf);
            waited = 0;
        }
        video_retrace();
    }
#undef FIELD_DRAW
    for (int r = 0; r < H; r++)
        memcpy(g_fb + (y + r) * g_fb_w + left, saved + r * w, (size_t)w);
    return result > 0;
}

unsigned long g_cheat_flags, g_cheat_flags_b;

int fe_enter_name(char *name)
{
    enum { BX = 80, BY = 190, BW = 480, BH = 100 };
    static unsigned char saved[BW * BH];
    unsigned char remap[256];
    int r;

    mouse_show(0);                         /* hidden, and left so (0xf795c) */
    for (int y = 0; y < BH; y++)
        memcpy(saved + y * BW, g_fb + (BY + y) * g_fb_w + BX, BW);
    darken_table(remap, video_palette8(), 0x6000);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            unsigned char *p = &g_fb[(BY + y) * g_fb_w + BX + x];
            *p = remap[*p];
        }
    frame(BX + 2, BY + 2, BX + 477, BY + 97, nearest_colour(0xff0000));
    frame(BX + 3, BY + 3, BX + 476, BY + 96, nearest_colour(0xff0000));
    fe_styles_init();
    fe_style(2);
    fe_text_centre(g_german ? "Bitte Namen Eingeben" : "Please Enter Name", BX + 240, BY + 10);
    fe_style(1);
    r = fe_text_field(name, 8, 320, 230, 1);
    fe_play_sound(0x48);
    if (r)
        g_cheat_flags = g_cheat_flags_b = 0;
    for (int y = 0; y < BH; y++)
        memcpy(g_fb + (BY + y) * g_fb_w + BX, saved + y * BW, BW);
    return r;
}
