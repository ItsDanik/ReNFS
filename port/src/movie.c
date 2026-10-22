/* The movies' player. movie_play 0x34ba0 sets VGA mode X, 320x240 (240 lines, 80 bytes a
   plane row), and runs movie_loop 0x346dc. The sound is read ahead of the pictures into
   its stream; a picture is shown when it is due, 15 a second, by the sound heard so far
   when there is sound (0x8cd42) or by the clock, a key frame's palette at the retrace.
   At the end it waits for the sound to finish (0x34a30). */
#include "movie.h"
#include "audio.h"
#include "tgv.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

#define SOUND_MAX 16384        /* frames in one sound block, at most */

/* keeps half a second of sound queued; the frames put in so far */
static long feed(tgv *t, int *stream, short *buf, long put, int *sound_ended)
{
    while (!*sound_ended && (*stream < 0 || audio_stream_queued(*stream) < t->sound.rate / 2)) {
        int n = tgv_next_sound(t, buf, SOUND_MAX);
        if (n < 0) {
            *sound_ended = 1;
            break;
        }
        if (*stream < 0)
            *stream = audio_stream_open(t->sound.rate, t->sound.channels);
        if (*stream < 0) {                 /* no sound card: the clock times the pictures */
            *sound_ended = 1;
            break;
        }
        audio_stream_put(*stream, buf, n);
        put += n;
    }
    return put;
}

int movie_play(const char *name)
{
    static short sound[2 * SOUND_MAX];
    unsigned char black[768] = { 0 };
    char path[256];
    tgv t;
    int stream = -1, shown = 0, stopped = 0, ended = 0, sound_ended = 0;
    long put = 0;
    unsigned start;

    snprintf(path, sizeof path, "FRONTEND/MOVIE/%s", name);
    if (!tgv_open(&t, path)) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return -1;
    }
    video_mode(320, 240);
    video_palette(black, 0, 256);
    put = feed(&t, &stream, sound, put, &sound_ended);
    start = clock_ticks();
    while (!ended && !stopped && !g_quit) {
        int due;
        if (stream >= 0 && sound_ended && audio_stream_queued(stream) == 0) {
            start = clock_ticks() - (unsigned)(shown * 100 / 15);
            audio_stream_close(stream);    /* the sound is over: the clock goes on */
            stream = -1;
        }
        if (stream >= 0)
            due = (int)((put - audio_stream_queued(stream)) * 15 / t.sound.rate);
        else
            due = (int)((clock_ticks() - start) * 15 / 100);
        while (shown <= due && !ended) {
            if (!tgv_next_frame(&t)) {
                ended = 1;
                break;
            }
            /* (0x34c4e) in the middle of the screen, at a multiple of 4: the 320x192
               films 24 rows down */
            {
                int x0 = ((g_fb_w - t.w) / 2 + 3) & ~3, y0 = ((g_fb_h - t.h) / 2 + 3) & ~3;
                if (x0 < 0)
                    x0 = 0;
                if (y0 < 0)
                    y0 = 0;
                for (int y = 0; y < t.h && y0 + y < g_fb_h; y++)
                    memcpy(g_fb + (y0 + y) * g_fb_w + x0, t.cur + y * t.w,
                           t.w < g_fb_w - x0 ? t.w : g_fb_w - x0);
            }
            if (t.new_palette) {
                video_palette(t.pal, 0, 256);
                t.new_palette = 0;
            }
            shown++;
        }
        video_retrace();
        put = feed(&t, &stream, sound, put, &sound_ended);
        {
            int k = key_get();
            if (k == 0x20 || k == 0x0d || k == 0x1b)
                stopped = 1;
        }
    }
    /* 0x34a30: the sound plays out (2 seconds at most) unless a key stopped it */
    if (!stopped) {
        settimeout(200);
        while (audio_stream_queued(stream) > 0 && !timedout() && !g_quit)
            video_retrace();
    }
    audio_stream_close(stream);
    tgv_close(&t);
    video_palette(black, 0, 256);
    return stopped;
}
