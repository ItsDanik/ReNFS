/* mister.c - see mister.h */
#include "mister.h"
#include "files.h"
#include <SDL.h>
#include <mister_hybrid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define EXIT_TO_MENU  0
#define EXIT_RESTART  42    /* danik_hybrid_launch.sh starts the game again: the core's list */
#define EXIT_RELOADED 43    /* the same, once the core has settled after being loaded again */

#define TITLE "ReNFS"

int mister_on(void)
{
    return MH_Open();
}

static uint32_t g_field;           /* the field of the last retrace */
static int g_field_ok;

void mister_retrace_wait(void)
{
    if (!MH_IsOpen())
        return;
    if (!g_field_ok) {
        g_field = MH_FieldCounter();
        g_field_ok = 1;
    }
    /* the next field after the last retrace's; one that took longer than a field goes on
       at once from the field it is in */
    if (MH_FieldCounter() == g_field)
        MH_WaitField(g_field);
    g_field = MH_FieldCounter();
}

/*
 * THE GAME'S FILES. The CD in NFSSE/ in the game's folder; the files the game opens
 * there (English: the German ones, the G folders, are never opened on the MiSTer) are
 * looked for, so that one missing is said at the start and not found out in a race.
 */
static const char *const g_found[] = {   /* the game is there (MH_PickGame) */
    "NFSSE/NFS_DOS.EXE", "NFSSE/FRONTEND/ART/TITLE.QFS", "NFSSE/SIMDATA/MISC/*", NULL
};

#define NO_GAME "The Need for Speed SE was not found. Copy everything on its CD, all the " \
    "files and folders as they are, to %s/NFSSE/."

static int g_listed;

/* `want` (a path from the CD's top, any case) is in `dir` */
static int has_file(const char *dir, const char *want)
{
    char at[1100], part[256];
    const char *p = want;
    snprintf(at, sizeof at, "%s", dir);
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : strlen(p);
        DIR *d;
        struct dirent *e;
        int found = 0;
        if (n >= sizeof part)
            return 0;
        memcpy(part, p, n);
        part[n] = 0;
        if (!(d = opendir(at)))
            return 0;
        while ((e = readdir(d)))
            if (!strcasecmp(e->d_name, part)) {
                size_t l = strlen(at);
                snprintf(at + l, sizeof at - l, "/%s", e->d_name);
                found = 1;
                break;
            }
        closedir(d);
        if (!found)
            return 0;
        p += n + (slash ? 1 : 0);
    }
    return 1;
}

int mister_start(void)
{
    static const mh_game game = { "SE", "Need for Speed SE", g_found, NULL, NO_GAME };
    if (!MH_Open())
        return 1;
    if (MH_PickGame(&game, 1, NULL, NO_GAME, &g_listed) < 0)
        return 0;
    if (g_listed)
        setenv("RENFS_MISTER_LISTED", "1", 1);
    {
        /* every file the game opens, the first missing ones named */
        extern const char *const g_mister_files[];
        char text[600] = "", path[1100];
        int missing = 0;
        for (int i = 0; g_mister_files[i]; i++) {
            if (has_file("NFSSE", g_mister_files[i]))
                continue;
            if (missing++ < 3) {
                size_t l = strlen(text);
                snprintf(text + l, sizeof text - l, "%s%s", l ? ", " : "", g_mister_files[i]);
            }
        }
        if (missing) {
            char msg[900];
            if (missing > 3) {
                size_t l = strlen(text);
                snprintf(text + l, sizeof text - l, " and %d more", missing - 3);
            }
            snprintf(path, sizeof path, "%s/NFSSE", MH_GamePath());
            snprintf(msg, sizeof msg, "Files of the CD are missing in %s: %s. Copy everything on "
                     "the CD, all the files and folders as they are.", path, text);
            mister_fatal(msg);
            return 0;
        }
    }
    MH_UI_Message(TITLE, "Loading Need for Speed SE...", 0);
    return 1;
}

void mister_fatal(const char *text)
{
    printf("mister: %s\n", text);
    fflush(stdout);
    if (MH_Open())
        MH_UI_Message(TITLE, text, MH_UI_WAIT | MH_UI_ERROR);
}

int mister_exit_code(void)
{
    if (MH_CoreReloaded())
        return EXIT_RELOADED;
    /* a game picked from the core's list: the list again (not when another core was loaded) */
    if (getenv("RENFS_MISTER_LISTED") && MH_Open())
        return EXIT_RESTART;
    return EXIT_TO_MENU;
}
