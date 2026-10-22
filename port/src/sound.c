/* The menus' sound.
   The effects: FRONT_MW.BNK (SIMDATA/SOUNDBNK), 128 offsets of records with an EACS
   header at +0x28 (NOTES §3.8); played in the middle at the volume CFG_SFX_VOLUME.
   The music: an .AS4 stream (1SNh / 1SNd blocks, 1SNl the loop's start, 1SNe the end),
   read ahead half a second at every retrace as 0x71b6c reads it whenever the menus wait. */
#include "sound.h"
#include "audio.h"
#include "collide.h"
#include "config.h"
#include "eacs.h"
#include "files.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *g_bank;
static long g_bank_len;

static unsigned u16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned long u32(const unsigned char *p) { return u16(p) | (unsigned long)u16(p + 2) << 16; }

void fe_sound_bank_load(void)
{
    fe_sound_bank_free();
    g_bank = file_load("SIMDATA/SOUNDBNK/FRONT_MW.BNK", &g_bank_len);
}

void fe_sound_bank_free(void)
{
    free(g_bank);
    g_bank = 0;
}

/* the last of sound 0x4a (the audio screen's test of the effects' volume, 0x15662c): kept
   to be stopped or heard to its end */
static int g_test_sound = -1;

void fe_play_sound(int n)
{
    static short out[2 * 65536];
    unsigned long rec;
    eacs h;
    int s, frames;
    if (!g_bank || n < 0 || n >= 128 || g_bank_len < 512)
        return;
    rec = u32(g_bank + 4 * n);
    if (!rec || (long)rec + 0x48 > g_bank_len || !eacs_header(&h, g_bank + rec + 0x28, 0x20))
        return;
    if (h.data >= g_bank_len)
        return;
    /* the data is in the file, not in the record */
    frames = eacs_stream_block(&h, g_bank + h.data, g_bank_len - h.data, out,
                               h.samples < 65536 ? (int)h.samples : 65536);
    if (n == 0x4a)
        fe_test_sound_stop();
    s = audio_stream_open(h.rate, h.channels);
    if (s < 0)
        return;
    audio_stream_volume(s, config_word(CFG_SFX_VOLUME));
    audio_stream_put(s, out, frames);
    if (n == 0x4a)
        g_test_sound = s;
    else
        audio_stream_release(s);
}

int fe_test_sound_done(void)
{
    return g_test_sound < 0 || audio_stream_queued(g_test_sound) == 0;
}

void fe_test_sound_stop(void)
{
    audio_stream_close(g_test_sound);
    g_test_sound = -1;
}

void fe_sfx_volume(int v)
{
    if (g_test_sound >= 0)
        audio_stream_volume(g_test_sound, v);
}

/* F5's silence: the three volumes kept (0x15d148, 0x15d14a, 0x15d14c) and set to 0 */
static int g_muted;            /* 0xf748e */
static int g_kept[3];
static const int g_volume_at[3] = { CFG_MUSIC_VOLUME, CFG_MENU_VOLUME, CFG_SFX_VOLUME };

int fe_sound_muted(void)
{
    return g_muted;
}

int fe_sound_kept(int at)
{
    for (int i = 0; i < 3; i++)
        if (g_volume_at[i] == at)
            return g_kept[i];
    return 0;
}

void fe_sound_unmute(void)
{
    if (!g_muted)
        return;
    for (int i = 0; i < 3; i++)
        config_set_word(g_volume_at[i], g_kept[i]);
    g_muted = 0;
}

void fe_sound_mute(void)
{
    if (g_muted) {
        fe_sound_unmute();
        fe_sfx_volume(config_word(CFG_SFX_VOLUME));
        music_volume(config_word(CFG_MENU_VOLUME));
    } else {
        for (int i = 0; i < 3; i++) {
            g_kept[i] = config_word(g_volume_at[i]);
            config_set_word(g_volume_at[i], 0);
        }
        fe_sfx_volume(0);
        music_volume(0);
        g_muted = 1;
    }
    /* a third of a second for the key to be let go */
    settimeout(0x21);
    while (!timedout() && !g_quit)
        video_retrace();
    settimeout(12000);
}

static void sound_service(void);

/* the music */
static struct {
    unsigned char *d;
    long len, pos, loop;           /* loop: the 1SNl's block, -1 none */
    eacs h;
    int has_header, stream, fading, volume;
    unsigned fade_start;
} g_music = { .stream = -1, .volume = 127 };

/* the volume a stream starts at, as the service that starts it sets it: the menus'
   (fe_service_music 0x71c20, +0x77), the race music's (0x71cec, +0x75), or -1 the
   stream's own (127) */
static int g_music_level = CFG_MENU_VOLUME;

void music_level_from(int at)
{
    g_music_level = at;
}

static void music_stop(void)
{
    audio_stream_close(g_music.stream);
    free(g_music.d);
    memset(&g_music, 0, sizeof g_music);
    g_music.stream = -1;
    g_music.volume = 127;
}

/* the next block of sound, the loop followed; -1 at the end */
static int music_block(short *out, int max)
{
    for (int guard = 0; guard < 4096; guard++) {
        const unsigned char *b;
        long size;
        if (g_music.pos + 8 > g_music.len)
            goto end;
        b = g_music.d + g_music.pos;
        size = (long)u32(b + 4);
        if (size < 8 || g_music.pos + size > g_music.len)
            goto end;
        g_music.pos += size;
        if (!memcmp(b, "1SNh", 4)) {
            g_music.has_header = eacs_header(&g_music.h, b + 8, size - 8);
            if (g_music.has_header && size > 0x28)
                return eacs_stream_block(&g_music.h, b + 0x28, size - 0x28, out, max);
        } else if (!memcmp(b, "1SNd", 4) && g_music.has_header) {
            return eacs_stream_block(&g_music.h, b + 8, size - 8, out, max);
        } else if (!memcmp(b, "1SNl", 4) && size >= 12) {
            g_music.loop = (long)u32(b + 8);
        } else if (!memcmp(b, "1SNe", 4))
            goto end;
        continue;
end:
        if (g_music.loop < 0 || g_music.loop >= g_music.len)
            return -1;
        g_music.pos = g_music.loop;
    }
    return -1;
}

static void music_service(void)
{
    static short out[2 * 32768];
    if (!g_music.d)
        return;
    if (g_music.fading) {
        int v = g_music.volume - (int)(clock_ticks() - g_music.fade_start) * g_music.volume / 100;
        audio_stream_volume(g_music.stream, v);
        if (v <= 0) {
            music_stop();
            return;
        }
    }
    while (!g_music.has_header || audio_stream_queued(g_music.stream) < g_music.h.rate / 2) {
        int n = music_block(out, 32768);
        if (n < 0)
            break;
        if (g_music.stream < 0 && g_music.has_header) {
            if (g_music_level >= 0)
                g_music.volume = config_word(g_music_level);
            g_music.stream = audio_stream_open(g_music.h.rate, g_music.h.channels);
            audio_stream_volume(g_music.stream, g_music.volume);
        }
        if (g_music.stream < 0)
            break;
        audio_stream_put(g_music.stream, out, n);
    }
}

void music_start(const char *name)
{
    char path[256];
    if (!config_word(CFG_MENU_MUSIC))
        return;
    music_stop();
    snprintf(path, sizeof path, "FRONTEND/MUSIC/%s", name);
    g_music.d = file_load(path, &g_music.len);
    if (!g_music.d) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    g_music.loop = -1;
    g_retrace_hook = sound_service;
    music_service();
}

void music_choose(void)
{
    static const char *const names[12] = {
        "ROCK1.AS4", "ROCK2.AS4", "ROCK3.AS4", "ROCK4.AS4", "ROCK5.AS4", "ROCK6.AS4",
        "TECHNO1.AS4", "TECHNO2.AS4", "TECHNO3.AS4", "TECHNO4.AS4", "TECHNO5.AS4", "TECHNO6.AS4",
    };
    int mode = config_word(CFG_MENU_MUSIC), pick;
    unsigned long c = timer_counter();   /* 0xed0fc */
    if (mode < 0 || mode > 15)
        return;
    if (mode == 0)
        pick = 5;
    else if (mode == 1)
        pick = (int)(c % 12);
    else if (mode == 3 || mode >= 10)
        pick = (int)(c % 6) + 6;
    else
        pick = (int)(c % 6);
    /* 5 means none: so ROCK6 is never chosen here (as in the original) */
    if (pick != 5)
        music_start(names[pick]);
}

/* 0x52580: EA's next random number scaled to 0 .. n - 1 */
static int random_below(int n)
{
    return (int)((collide_rand() & 0xffff) * (unsigned)n >> 16);
}

void race_music_start(void)
{
    static const char *const names[12] = {
        "ROCK1.AS4", "ROCK2.AS4", "ROCK3.AS4", "ROCK4.AS4", "ROCK5.AS4", "ROCK6.AS4",
        "TECHNO1.AS4", "TECHNO2.AS4", "TECHNO3.AS4", "TECHNO4.AS4", "TECHNO5.AS4", "TECHNO6.AS4",
    };
    int mode = config_word(CFG_MENU_MUSIC), pick;

    if (mode == 1)
        pick = random_below(12);
    else if (mode == 2)
        pick = random_below(6);
    else if (mode == 3)
        pick = random_below(6) + 6;
    else if (mode >= 4 && mode <= 15)
        pick = mode - 4;
    else
        return;
    music_start(names[pick]);
}

void music_random(void)
{
    static const char *const names[12] = {
        "ROCK1.AS4", "ROCK2.AS4", "ROCK3.AS4", "ROCK4.AS4", "ROCK5.AS4", "ROCK6.AS4",
        "TECHNO1.AS4", "TECHNO2.AS4", "TECHNO3.AS4", "TECHNO4.AS4", "TECHNO5.AS4", "TECHNO6.AS4",
    };
    int mode = config_word(CFG_MENU_MUSIC);

    /* (0xf749a cleared: the race's music not chosen yet) */
    if (mode == 1)
        music_start(names[random_below(12)]);
    else if (mode == 2 || (mode >= 4 && mode <= 9))
        music_start(names[random_below(6)]);
    else if (mode == 3 || (mode >= 10 && mode <= 15))
        music_start(names[6 + random_below(6)]);
}

void music_volume(int v)
{
    if (!config_word(CFG_MENU_MUSIC))
        return;
    g_music.volume = v < 0 ? 0 : v > 127 ? 127 : v;
    if (g_music.stream >= 0 && !g_music.fading)
        audio_stream_volume(g_music.stream, g_music.volume);
}

/* the race's spoken word (0x9901f's queue on its own voice): one at a time. Read whole
   and handed to the sound device half a second ahead at every retrace, as the music: the
   showcase's commentaries last a minute, and converting a minute at once to the device's
   rate took seconds on the MiSTer, with the sound held meanwhile */
static int g_speech = -1;
static struct {
    short *out;
    int n, pos, rate, channels;
} g_say;

static void speech_service(void)
{
    while (g_say.out && g_say.pos < g_say.n && audio_stream_queued(g_speech) < g_say.rate / 2) {
        int k = g_say.n - g_say.pos < g_say.rate / 8 ? g_say.n - g_say.pos : g_say.rate / 8;
        audio_stream_put(g_speech, g_say.out + (long)g_say.pos * g_say.channels, k);
        g_say.pos += k;
    }
    if (g_say.out && g_say.pos >= g_say.n) {
        free(g_say.out);
        g_say.out = 0;
    }
}

static void sound_service(void)
{
    music_service();
    speech_service();
}

void speech_say(const char *name)
{
    short *out;
    char path[256];
    unsigned char *d;
    long len;
    eacs h;
    int n, v;

    if (!config_word(CFG_SPEECH))
        return;
    snprintf(path, sizeof path, "FRONTEND/%s/%s", g_german ? "GSPEECH" : "SPEECH", name);
    d = file_load(path, &len);
    if (!d) {
        fprintf(stderr, "renfs: %s is missing\n", path);
        return;
    }
    if (eacs_header(&h, d, len) && h.data > 0 && h.data < len && h.samples > 0 &&
        (out = malloc(sizeof *out * (size_t)h.samples * (size_t)h.channels))) {
        n = eacs_sound(&h, d + h.data, len - h.data, out, (int)h.samples);
        /* (0x98f66 empties the queue) the sounds' volume and a half, 127 at most */
        speech_stop();
        g_speech = audio_stream_open(h.rate, h.channels);
        if (g_speech >= 0 && n > 0) {
            v = config_word(CFG_SFX_VOLUME) * 3 / 2;
            audio_stream_volume(g_speech, v > 127 ? 127 : v);
            g_say.out = out;
            g_say.n = n;
            g_say.pos = 0;
            g_say.rate = h.rate > 0 ? h.rate : 22050;
            g_say.channels = h.channels;
            g_retrace_hook = sound_service;
            speech_service();
        } else
            free(out);
    }
    free(d);
}

void speech_stop(void)
{
    audio_stream_close(g_speech);
    g_speech = -1;
    free(g_say.out);
    g_say.out = 0;
}

void music_stop_now(void)
{
    music_stop();
}

void music_fade_out(void)
{
    if (g_music.d && !g_music.fading) {
        g_music.fading = 1;
        g_music.fade_start = clock_ticks();
    }
}

void music_wait_end(void)
{
    while (g_music.d && g_music.fading && !g_quit)
        video_retrace();
    music_stop();
}
