/* The MiSTer hybrid core (branch mister, built with MISTER_HYBRID; the core is
   ~/MiSTer-ReNFS): the game runs on the MiSTer's ARM, the FPGA core shows the picture at
   15 kHz and has the sound and the input. SDL's "mister" drivers (the framework's,
   hybrid/sdl2) do most of it; this is the rest. Without the core every call here does
   nothing or answers as the PC would, and the program runs as the PC's does (which is how
   it is tested, against hybrid/tools/fakecore). */
#ifndef RENFS_MISTER_H
#define RENFS_MISTER_H

#ifdef MISTER_HYBRID

int mister_on(void);                   /* the core is there */
/* once a retrace, after the picture was handed over: until the core's next field
   (59.6 a second), whatever the picture's size; so a retrace is a field */
void mister_retrace_wait(void);

/* the game's data and the player's files: where the launcher runs the game (the data
   folder, games/dnk/ReNFS/ or on a USB drive), the CD in NFSSE/, the rest in saves/.
   0 when the CD's files are not all there, after the core's message saying which and
   where (it stays until the player leaves the core) */
int mister_start(void);
/* a message on the core's picture that stays until the player leaves the core */
void mister_fatal(const char *text);
/* what the process ends with: the launcher's contract */
int mister_exit_code(void);

#endif
#endif
