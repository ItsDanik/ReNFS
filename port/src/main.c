/* ReNFS: The Need for Speed Special Edition (1996), written again from its DOS program.
   main 0x4716c -> startup 0x42054 -> fe_outer_loop 0x47000 (NOTES §4.1). */
#include "joystick.h"
#include "ff.h"
#include "audio.h"
#include "collide.h"
#include "exe.h"
#include "fe.h"
#include "files.h"
#include "race.h"
#include "replay.h"
#include "smooth.h"
#include "video.h"
#include "mister.h"
#include "renfs_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char usage[] =
    "ReNFS " RENFS_VERSION ": The Need for Speed SE, for Linux and Windows.\n"
    "\n"
    "    renfs [-data <folder>] [-home <folder>] [-lang de] [--shot <file.ppm> <frames>]\n"
    "          [--race <track>] [--replay <file.rpl> [highlights]] [--version]\n"
    "\n"
    "  -data <folder>   the game: the CD's files as they are (else $RENFS_DATA, else\n"
    "                   ./NFSSE, else NFSSE beside the program)\n"
    "  -home <folder>   the port's own settings (else $RENFS_HOME, else ~/.config/renfs;\n"
    "                   on Windows %APPDATA%\\renfs)\n"
    "  -lang de         the German game (the CD holds both; else $RENFS_LANG)\n"
    "  --shot <f> <n>   no window: write the picture of retrace n (60 a second) to f\n"
    "  --race <track>   straight into a race on the track (cy1 .. al3, tr1 .. tr7,\n"
    "                   tr5 the rally) with the car, opponent and race type last\n"
    "                   chosen in Control Central; P holds it, Esc the pause menu\n"
    "  --replay <f>     a replay: the last race's (replay.rpl in the home folder) or the\n"
    "                   CD's (REPLAY1.RPL ..); with \"highlights\" only those\n"
    "\n"
    "  RENFS_LANG=de    the German art (GART, ...)\n"
    "  RENFS_NOSOUND=1  no sound\n"
    "  RENFS_SNDWAV=f   write what is heard to the file f (a WAV)\n"
    "  RENFS_THROW=s:x,y,z  a test: at race step s the car given that speed (m/s)\n"
    "  RENFS_RACE=t     a test: as --race t (RENFS_REPLAY=f: --replay f)\n"
    "  RENFS_PERF=1     every 5 seconds of a race: pictures a second and their time\n"
    "  RENFS_SEED=n     a test: the race's random numbers from n (else the clock)\n"
    "  RENFS_PLACE=s:a,c,v  a test: at race step s the opponent put a m ahead of the\n"
    "                   car and c m to the side, at v m/s\n"
    "  RENFS_LAPS=n     n laps of a circuit (tr1 ..; else the race length's, 8 by default)\n"
    "  RENFS_TOD=n      the track's time of day in --race: 0 morning, 1 midday, 2 evening\n"
    "  RENFS_MIRROR=1, RENFS_RALLY=1  the mirrored track or the rally in --race\n"
    "  RENFS_EXPLODE=1           EXPLODE in --race (the horn throws the cars near)\n"
    "  RENFS_VIEW=n     a test: the race watched from camera n (2..5, 8..12 outside)\n"
    "  RENFS_RES=n      a test: races at 640 (640x480) or 320 (320x200), whatever the\n"
    "                   graphics screen says\n"
    "  RENFS_SMOOTH=0|1 a test: the race's motion smoothed between its steps or not,\n"
    "                   whatever F11 last left (not kept)\n"
    "  RENFS_FF=0..100  force feedback and rumble's strength, 0 none (else renfs.cfg's\n"
    "                   \"forcefeedback n\", 100)\n"
    "  RENFS_FFLOG=f    every force-feedback command written to f with the race's step\n"
    "\n"
    "Keys: Space, Enter or Esc pass a movie or the title.\n"
    "In a race, F11: the motion smoothed between the race's steps (drawn at every\n"
    "retrace, 60 a second at 640x480; the port's own, on at first) or a new picture\n"
    "only at each step, as the original; kept in the home folder's renfs.cfg.\n"
    "\n"
    "A pad or joystick: calibrated the first time (or Options, Controls, Calibrate\n"
    "joystick). It drives even with the controls on Keyboard, but all or nothing; for\n"
    "analogue steering and pedals choose Joystick for Steering and Accelerate/brake in\n"
    "Options, Controls, as in the original. Pad: left stick steers, right trigger\n"
    "gas, left trigger brake, shoulders gear up / down, A horn, B map, X camera,\n"
    "Y handbrake, Start Esc.\n";

int main(int argc, char **argv)
{
    const char *data = 0, *home = 0, *shot = 0, *race = 0, *replay = 0, *lang = 0;
    int shot_frames = 0, highlights = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-data") && i + 1 < argc)
            data = argv[++i];
        else if (!strcmp(argv[i], "-home") && i + 1 < argc)
            home = argv[++i];
        else if (!strcmp(argv[i], "-lang") && i + 1 < argc)
            lang = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) {
            shot = argv[++i];
            shot_frames = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--race") && i + 1 < argc)
            race = argv[++i];
        else if (!strcmp(argv[i], "--replay") && i + 1 < argc) {
            replay = argv[++i];
            if (i + 1 < argc && !strcmp(argv[i + 1], "highlights"))
                highlights = ++i;
        }
        else if (!strcmp(argv[i], "--version")) {
            puts("ReNFS " RENFS_VERSION);
            return 0;
        } else {
            fputs(usage, strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? stderr : stdout);
            return strcmp(argv[i], "--help") && strcmp(argv[i], "-h") ? 1 : 0;
        }
    }
    if (!race && getenv("RENFS_RACE") && *getenv("RENFS_RACE"))
        race = getenv("RENFS_RACE");        /* (where there are no arguments: the MiSTer's tests) */
    if (!replay && getenv("RENFS_REPLAY") && *getenv("RENFS_REPLAY"))
        replay = getenv("RENFS_REPLAY");
#ifdef MISTER_HYBRID
    /* the MiSTer: the CD's files looked for, "Loading ..." on the core's picture; a
       message that waits for the player to leave the core when they are not all there */
    if (!shot && !mister_start())
        return mister_exit_code();
#endif
    if (!files_init(data, home)) {
        files_error(data || getenv("RENFS_DATA")
                    ? "The game's files are not in the folder given (FRONTEND/ART/TITLE.QFS is missing)."
                    : "The game's files were not found. Copy the whole CD of The Need for Speed SE "
                      "into a folder named NFSSE next to ReNFS, or name the folder with -data <folder>.");
        return 1;
    }
    if (lang)                  /* (else $RENFS_LANG, files_init) */
        g_german = (lang[0] == 'd' || lang[0] == 'D') && (lang[1] == 'e' || lang[1] == 'E');
    files_cfg_init();          /* renfs.cfg, the port's own settings, explained */
    smooth_load();             /* (the port's own setting: smooth.c) */
    if (!exe_load())           /* the game's tables, the built-in settings among them */
        return 1;
    collide_init();            /* EA's random numbers as the program starts */
    for (int i = 0; i < 9; i++)    /* every track's time of day midday (main 0x471b5) */
        rec_put(REC_TOD + 4 * i, 1);
    if (!video_init(shot, shot_frames))
        return 1;

    if (!shot && !getenv("RENFS_NOSOUND"))
        audio_init();
    srand(shot ? 1 : (unsigned)time(0));   /* pictures without a window: the same each time */
    if (race || replay)
        joy_startup();         /* (fe_outer_loop's, for the tests straight into a race) */
    if (race)
        race_test(race);
    else if (replay)
        race_replay_test(replay, highlights ? 2 : 1);
    else
        fe_outer_loop();

    if (shot)                  /* the program ended before the frame asked for */
        fprintf(stderr, "renfs: ended before frame %d\n", shot_frames);
    ff_close();                /* (main 0x47219) */
    audio_done();
    video_done();
#ifdef MISTER_HYBRID
    return mister_exit_code();
#else
    return 0;
#endif
}
