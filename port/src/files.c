/* The game's files: the data folder is the CD's files as they are (NFSSE/); DOS names
   are upper case but the program asks for them in any case, so every part of a path is
   matched against the folder's entries without regard to case. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "files.h"
#include "mister.h"
#include <SDL.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(d, mode) _mkdir(d)
#endif

int g_german;

static char g_data[1024], g_home[1024];

const char *files_data(void) { return g_data; }

/* dir/name, with name's real case in dir; 0 if not there */
static int find_in(const char *dir, const char *name, char *out, size_t size)
{
    int n;
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, name) == 0) {
            n = snprintf(out, size, "%s/%s", dir, e->d_name);
            found = n > 0 && (size_t)n < size;
            break;
        }
    }
    closedir(d);
    return found;
}

/* the real path of rel inside root */
static int resolve(const char *root, const char *rel, char *out, size_t size)
{
    char part[256], at[1024];
    snprintf(at, sizeof at, "%s", root);
    while (*rel) {
        size_t n = strcspn(rel, "/\\");
        if (n >= sizeof part)
            return 0;
        memcpy(part, rel, n);
        part[n] = 0;
        rel += n;
        while (*rel == '/' || *rel == '\\')
            rel++;
        if (n == 0)
            continue;
        if (!find_in(at, part, out, size))
            return 0;
        snprintf(at, sizeof at, "%s", out);
    }
    snprintf(out, size, "%s", at);
    return 1;
}

static int holds_game(const char *dir)
{
    char p[1024];
    return resolve(dir, "FRONTEND/ART/TITLE.QFS", p, sizeof p);
}

int files_init(const char *data_arg, const char *home_arg)
{
    const char *tries[4];
    char beside[1024] = "";
    int n = 0;

    /* a folder asked for is that folder or none */
    int asked = data_arg || (getenv("RENFS_DATA") && *getenv("RENFS_DATA"));
    if (data_arg)
        tries[n++] = data_arg;
    else if (getenv("RENFS_DATA") && *getenv("RENFS_DATA"))
        tries[n++] = getenv("RENFS_DATA");
    else
        tries[n++] = "NFSSE";
    if (!asked) {              /* else NFSSE beside the program */
        char *base = SDL_GetBasePath();
        if (base) {
            snprintf(beside, sizeof beside, "%sNFSSE", base);
            SDL_free(base);
            tries[n++] = beside;
        }
    }
    g_data[0] = 0;
    for (int i = 0; i < n; i++) {
        if (holds_game(tries[i])) {
            snprintf(g_data, sizeof g_data, "%s", tries[i]);
            break;
        }
    }

    if (home_arg)
        snprintf(g_home, sizeof g_home, "%s", home_arg);
    else if (getenv("RENFS_HOME"))
        snprintf(g_home, sizeof g_home, "%s", getenv("RENFS_HOME"));
#ifdef _WIN32
    else if (getenv("APPDATA"))
        snprintf(g_home, sizeof g_home, "%s/renfs", getenv("APPDATA"));
#endif
    else if (getenv("HOME"))
        snprintf(g_home, sizeof g_home, "%s/.config/renfs", getenv("HOME"));
    else
        snprintf(g_home, sizeof g_home, ".");

    if (getenv("RENFS_LANG") && strncasecmp(getenv("RENFS_LANG"), "de", 2) == 0)
        g_german = 1;
    return g_data[0] != 0;
}

const char *files_home(void)
{
    mkdir(g_home, 0755);           /* ~/.config (%APPDATA%) is there already; one level is enough */
    return g_home;
}

/* RENFS_FILELOG=<file>: every path of the CD asked for written there, a line each (to
   know the files the game opens: the MiSTer's check of the CD, tools/mister_files.sh) */
static void file_logged(const char *rel)
{
    static FILE *log;
    static int tried;
    if (!tried) {
        tried = 1;
        if (getenv("RENFS_FILELOG"))
            log = fopen(getenv("RENFS_FILELOG"), "a");
    }
    if (log) {
        fprintf(log, "%s\n", rel);
        fflush(log);
    }
}

int file_path(const char *rel, char *out, size_t size)
{
    file_logged(rel);
    return resolve(g_data, rel, out, size);
}

FILE *file_open(const char *rel)
{
    char path[1024];
    file_logged(rel);
    if (!resolve(g_data, rel, path, sizeof path))
        return 0;
    return fopen(path, "rb");
}

unsigned char *file_load(const char *rel, long *len)
{
    FILE *f = file_open(rel);
    unsigned char *d;
    long n;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(n + 16);            /* a little more: readers may look a few bytes past */
    if (d && fread(d, 1, n, f) != (size_t)n) {
        free(d);
        d = 0;
    }
    fclose(f);
    if (d) {
        memset(d + n, 0, 16);
        if (len)
            *len = n;
    }
    return d;
}

static void cfg_path(char *path, size_t n)
{
    snprintf(path, n, "%s/renfs.cfg", files_home());
}

int files_cfg_get(const char *name, int dflt)
{
    char path[1100], line[256], fmt[80];
    FILE *f;
    int v = dflt;

    cfg_path(path, sizeof path);
    if (!(f = fopen(path, "r")))
        return dflt;
    snprintf(fmt, sizeof fmt, "%s %%d", name);
    while (fgets(line, sizeof line, f))
        sscanf(line, fmt, &v);
    fclose(f);
    return v;
}

void files_cfg_set(const char *name, int value)
{
    char path[1100], line[256], all[8192];
    size_t n = 0, len = strlen(name);
    int done = 0;
    FILE *f;

    cfg_path(path, sizeof path);
    all[0] = 0;
    if ((f = fopen(path, "r"))) {
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, name, len) && line[len] == ' ') {
                if (done)
                    continue;
                snprintf(line, sizeof line, "%s %d\n", name, value);
                done = 1;
            }
            if (n + strlen(line) < sizeof all) {
                strcpy(all + n, line);
                n += strlen(line);
            }
        }
        fclose(f);
    }
    if (!done)
        snprintf(all + n, sizeof all - n, "%s %d\n", name, value);
    if ((f = fopen(path, "w"))) {
        fputs(all, f);
        fclose(f);
    }
}

void files_cfg_init(void)
{
    char path[1100];
    FILE *f;

    cfg_path(path, sizeof path);
    if ((f = fopen(path, "r"))) {
        fclose(f);
        return;
    }
    if ((f = fopen(path, "w"))) {
        fputs("# ReNFS's own settings\n"
              "# smooth 1: the race drawn at every retrace, its motion between the\n"
              "# race's steps (F11 in a race); 0: as the original, a new picture a step\n"
              "smooth 1\n"
              "# fullscreen 1: the whole screen; 0: a window (Alt+Enter switches)\n"
              "fullscreen 0\n"
              "# forcefeedback 0..100: the strength of a pad's rumble and a wheel's\n"
              "# force feedback; 0 none\n"
              "forcefeedback 100\n", f);
        fclose(f);
    }
}

void files_error(const char *text)
{
    const char *drv = SDL_getenv("SDL_VIDEODRIVER");

    fprintf(stderr, "renfs: %s\n", text);
#ifdef MISTER_HYBRID
    if (mister_on()) {                 /* on the core's picture, until the player leaves */
        mister_fatal(text);
        return;
    }
#endif
    if (!drv || strcmp(drv, "dummy"))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ReNFS", text, 0);
}

const char *art_dir(void)
{
    return g_german ? "FRONTEND/GART" : "FRONTEND/ART";
}
