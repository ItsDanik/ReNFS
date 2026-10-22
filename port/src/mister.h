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
/* before the picture is handed over: shown at the second field after the one its drawing
   began in (the framework's drawing ahead, MH_PresentAtField), so that one taking a little
   longer than a field is still shown in time; a field later than at once. Not with
   MISTER_HYBRID_AHEAD=0 (the framework's switch, for comparing) */
void mister_present_at(void);

/* the game's data and the player's files: where the launcher runs the game (the data
   folder, games/dnk/ReNFS/ or on a USB drive), the CD in NFSSE/, the rest in saves/.
   0 when the CD's files are not all there, after the core's message saying which and
   where (it stays until the player leaves the core) */
int mister_start(void);
/* the core's joystick 1 as the original's joystick (joy_device): the stick, the brake,
   the throttle (the triggers, or A and B all the way), its buttons 1 horn (L3), 2 map,
   4 camera, 8 handbrake and the port's 16 gear up, 32 gear down */
int mister_joy(int count[4], int *buttons);
void *mister_pad_sdl(void);            /* its SDL joystick (the rumble's) */
/* in the menus: Menu OK as Enter, Menu Back as Esc (0 for neither) */
int mister_menu_key(void);
int mister_pause_held(void);           /* Start: Esc */
int mister_nitro_held(void);           /* R3: NITRO's keys */
/* while a race is driven the pad's Menu OK and Back are no keys (A accelerates) */
void mister_racing(int on);
int mister_is_racing(void);

/* the core's OSD options (CONF_STR in core/ReNFS.sv, status bits 24..): the races'
   resolution (320x200 without the core); */
enum { MR_320x200, MR_320x240, MR_640x200, MR_640x240, MR_640x480 };
int mister_resolution(void);
/* the CRT Options' HUD (Bottom): lines of a 240-line screen the bottom HUD is lifted by
   (hud_lift), 0..24; 0 without the core */
int mister_hud_bottom(void);
/* Smooth Motion on (1) or off (always at 640x480); -1 without the core */
int mister_smooth(void);
/* Rumble: the force feedback's strength, 100 On, 160 Strong, 0 Off; -1 without the core */
int mister_rumble_strength(void);
/* the Cheats page: bit 0 mirrored tracks (SKCART), 1 rally tracks (EARALLYE), 2 Lost
   Vegas (CASINOVILLE), 3 the Warrior (CONCEPTCAR), 4 all unlocked (ROCKYROAD), 5 TURBO,
   6 NITRO, 7 EXPLODE, 8 No Mercy (End held) */
int mister_cheats(void);
enum { MC_MIRROR = 1, MC_RALLY = 2, MC_VEGAS = 4, MC_WARRIOR = 8, MC_UNLOCK = 0x10,
       MC_TURBO = 0x20, MC_NITRO = 0x40, MC_EXPLODE = 0x80, MC_NO_MERCY = 0x100 };

/* a wheel's forces (hybrid/hps MH_Force*; ffdev.c): levels -255 (left) .. 255 (right),
   ms 0 no end (kept by mister_force_keep() once a frame) */
int mister_force_available(void);
void mister_force_constant(int slot, int level, int ms);
void mister_force_sine(int slot, int magnitude, int period_ms);
void mister_force_stop(int slot);
void mister_force_keep(void);

/* a message on the core's picture that stays until the player leaves the core */
void mister_fatal(const char *text);
/* what the process ends with: the launcher's contract */
int mister_exit_code(void);

#endif
#endif
