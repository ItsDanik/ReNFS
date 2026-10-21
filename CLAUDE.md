# ReNFS — *The Need for Speed SE* (1996, MS-DOS) for Linux and the MiSTer

Reverse engineering and a hand-written reimplementation of `NFS_DOS.EXE`, the DOS program
of *The Need for Speed Special Edition*. A native SDL2 port (Linux x86, then ARM), then a
MiSTer hybrid core (game on the ARM, the FPGA for 15 kHz video, audio and input).

**This file is the summary. `docs/NOTES.md` is the full record**: addresses, proofs,
formats, dead ends. New findings go there in detail and here as one line.
**`docs/HANDOVER.md` is where the work stands**: read it first in a new session.

---

## Working agreement — read this first

**Danik does not write code or decompile.** He knows the MiSTer and these ports, and he
does the verification and judges whether it plays like the original. Don't hand him code
to review, patches to apply or compiler errors to sort out.

- **Claude:** all code, tooling, decompilation, builds, debugging. This machine has
  Ghidra 12.1.2 (`/opt/ghidra`), python3, wine, objdump, docker. The `Read` tool opens
  PNGs: look at the pictures you make yourself.
- **Danik:** real hardware, whether it looks and feels like the original, and decisions
  about scope, licences and targets.

"Here is a build and a screenshot" beats "please check this function". Everything builds
with one command. Plain language, not jargon.

**The method is the skill `reverse-engineer-dos-game`** (`~/.claude/skills/`: the steps,
the toolkit, the decompiler's traps, checking against the original). What ReNFS learns that
holds for any game goes into the skill too.

The model project is **OpenScreamer** (`~/openscreamer`, its `CLAUDE.md` and
`docs/NOTES.md`): the same kind of program (Watcom C, DOS/4GW, LE), the same tools, the
same rules. Its "Hard-won rules" hold here too, above all: **when a format question
stalls, find the code that reads the field.**

---

## Decisions (Danik, 2026-10-18)

- Engine name **ReNFS** (`~/renfs`, the core `~/MiSTer-ReNFS`); game id
  `SE|Need for Speed SE` (menu entry `_Other/dnk-Need for Speed SE.mgl`).
- **Hand-written from the start.** The decompiled code is read, and clean C is written that
  does the same thing. Machine-converted code may exist only as an unpublished local
  reference for checking, under `build/`. OpenScreamer had to write every converted routine
  again by hand later; don't repeat that.
- **The player provides only the CD's files** (the folder `NFSSE/`, the ISO extracted).
  The installed copy (`NFSSE-Install/`) holds nothing the CD lacks, apart from `NFS.CFG`,
  `GAMEDATA/CONFIG/PATHS.DAT` and `NFS.EXE` (= `NFS_DOS.EXE`): the port makes its own
  settings and never needs an installation.
- **The menus on the MiSTer are 640x480** (interlaced on a 15 kHz screen, progressive with
  *HDMI Only* / VGA); races at 320x200 / 320x240.
- **Commit as the work goes** (each finished step, in `~/renfs` and `~/MiSTer-ReNFS`),
  never game data or decompiled code. Push `~/renfs` when a step is finished and checked.
- **After the options screen, the race comes next** (Danik, 2026-10-19); the screens
  behind the options, showcase, records and tournament wait.
- **A first start uses the game's built-in settings and records** (`cfg_defaults`
  `0x38ea8`, as an installed original), never the CD's CONFIG.DAT (Danik, 2026-10-19).
- GitHub (Danik, 2026-10-21): **`github.com/ItsDanik/ReNFS`, public, GPL-3**, branch `main`
  (local and remote). 0.6.0b released 2026-10-21 (Linux, Windows); ask before creating the
  MiSTer core's repository.

## Legal

The game is still EA's. **Never publish `NFSSE/`, `NFSSE-Install/` or anything made from
them**: `build/` (the flat image, the Ghidra project, `build/decomp`, PNGs, unpacked files)
is ignored. Decompiled text is never copied into `port/`. Check `git status` before every
commit.

---

## The binary and how to decompile it

`NFS_DOS.EXE`, 1,254,591 bytes, Watcom C/C++32 (1988–1995 runtime), DOS/4GW stub (11,392
bytes) + **LE** image. Objects: 1 code `0x10000` (698,877 bytes), 2 code `0xC0000` (60),
3 data `0xD0000` (24), 4 data + BSS + stack `0xE0000` (563,728). Entry `0x94444` (a jump
over the Watcom banner to `0x944bc`); **`main` = `0x4716c`**. No symbols.

**One command: `tools/rebuild.sh`** (about an hour, logs in `build/logs/`;
`tools/rebuild.sh --decomp` = names from `tools/names.txt` + decompile again, minutes).
What it does:

```bash
tools/ghidra-ext/install.sh   # once per Ghidra install: x86:LE:32:watcom (already done on this machine)
python3 tools/unle.py NFSSE/NFS_DOS.EXE -o build/nfs_flat.bin --map build/nfs_layout.json --fixups
G=/opt/ghidra/support/analyzeHeadless
$G build/ghidra renfs -import build/nfs_flat.bin -processor x86:LE:32:watcom -loader BinaryLoader \
   -loader-baseAddr 0x0 -noanalysis -scriptPath tools -postScript CreateEntry.java 0x94444
$G build/ghidra renfs -process nfs_flat.bin -analysisTimeoutPerFile 3600
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools \
   -postScript DecompRange.java build/decomp 0x10000 0xC0100      # a first INDEX.txt
# seeds: code addresses that pointers take (relocation records), then analyse again
python3 tools/seeds.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt > build/seeds.txt
$G build/ghidra renfs -process nfs_flat.bin -scriptPath tools -postScript SeedFuncs.java build/seeds.txt
$G build/ghidra renfs -process nfs_flat.bin -analysisTimeoutPerFile 3600
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools -postScript RenameFuncs.java tools/names.txt
$G build/ghidra renfs -process nfs_flat.bin -noanalysis -scriptPath tools \
   -postScript DecompRange.java build/decomp 0x10000 0xC0100      # one file a function + INDEX.txt
# (with DECOMP_BSS=0x129000: globals from there up that hold addresses are shown as ints)
```

- `--fixups` applies the LE relocation records, so every pointer in the image is a real
  address (no `RelocateRefs.java` needed). `build/nfs_layout.json` lists every fixup site.
- 1,953 functions, 89% of the code object (the rest is data in code).
- Calling conventions are mixed. The game and the Watcom runtime use `__watcall` (EAX, EDX,
  EBX, ECX, then the stack). **EA's libraries take their arguments on the stack**
  (`initgraphics(640, 480, 8)` is `push 8; push 0x1e0; push 0x280`): Ghidra drops them,
  so read call sites with `objdump -D -b binary -m i386 -M intel --start-address=... build/nfs_flat.bin`.
- `tools/strmap.py build/nfs_layout.json build/nfs_flat.bin build/decomp/INDEX.txt [addr..]`:
  the strings each function uses (the map's first draft; the full list `build/strs_by_site.txt`).
- `tools/errnames.py` names functions after EA's error messages ("openhandle - %s FILE
  ERROR") → `tools/names.txt` → `RenameFuncs.java`. Names given by hand go into
  `tools/names.txt` too, so a rebuilt project gets them back.
- Pitfalls carried over from OpenScreamer: never import the EXE itself (Ghidra has no LE
  loader); headless scripts are Java and need `-scriptPath tools`; log headless output to a
  file, never pipe it through `tail`.

---

## The program (details: NOTES §4)

`main` `0x4716c` → `startup` `0x42054` → `fe_outer_loop` `0x47000` → `fe_main_menu`
`0x42c30` → `race_from_menu` `0x3770c` → `race_run` `0x37228` → `race_main` `0x6356c`:
`race_step` `0x6348c` once per race-clock tick, then `race_draw_frame` `0x5bc40`. The timer
runs at 100 Hz; the race clock divides it in a 3-cycle (`race_timer_tick` `0x4c454`):
33.3 Hz by default (measured in DOSBox), 30–60 Hz by `0x12dd0c`. Names: `tools/names.txt`.
Arithmetic: 16.16 fixed point, the multiply rounds, 65,536 = a full turn (NOTES §4.5).
A step: the cars' driving every step, their sounds and the gearbox every 2nd, traffic and
police every 4th (flags `0x12f1bc`, measured). Timing (NOTES §4.12): `race_update_cars` `0x519d8` checks each racer's finish, sorts two
orders (`0x518c0`), passes splits (`0x68d7c`); the status bar's numbers `0x30d8c`.
A race ends through `0x13c178`: 1 finished,
4 arrested (the second police ticket), 0x10 everyone turned back (NOTES §4.3, §4.8).
The original in DOSBox: `nfs-dos`, `tools/nfsdos.py` (unseen, keys, memory; NOTES §5).
Cars meeting (NOTES §4.14): `0x1c878` after the driving, the boxes `0x2b864`, the hit
`0x2afb8`, hard above `0xea5b0` (`0xea5b4` with a player); a thrown computer's car stood up,
kind 5 `0x7d9e4`; EA's random numbers `0x91e04` start from the image. Roadside objects
(NOTES §4.15): a player outside the lanes `0x1ebf4`, under the car `0x1ee80` (the model's box
`0x65160`), fallen bytes `0xea5d8`, the road gathered again `0x5004c`.
The traffic (NOTES §4.16): kind 6 waits `0x7e0c8`, placed `0x7cac8` (where `0x7c5b4`, at
`0x7ccc0`), again out of reach `0x77de8`/`0x77d3c`; `0x15d480` waiting, at most settings
+0x34 on the road; `opponent_emu.py --full` checks the whole field.
The police (NOTES §4.17): out ahead at the road's marks `0x79500`, the chase and the
pull-over `0x79728` (warning, ticket; arrested at the second), the player held `0x7ea20`,
the picture `0x30424`.
The race's sounds (NOTES §4.18): EA's library (voices `0x160fa4`, `snd_play` `0x95a7a`, its
random numbers the collisions' `0x91e04`), 16 channels `0x13c2dc`, each step `0x80974`,
`0x7fef8`, `0x7fcbc`, `0x7fe88`, `0x800a0` (the radar `0xea814` too); `horn_want` `0x80a60`
is game logic (a random number) with the sound off as well.
Cars drawn: `cars_at_node` `0x6837c` → `0x66d08` → `0x66880` (NOTES §4.10); their
pictures re-coloured by `0x65020` through a colour map `0xf9520` that keeps what the last
re-colouring left. Snapshots e1..e5: a 320x200 race, `NFS_DOS_DIR=$PWD/build/dosnfs`.
The x87 runs at double precision (CW 0x127f): Unicorn must be set so (NOTES §4.6, the crash).
Graphics screen `0x32d08` (NOTES §4.2): +0x7f 0 races at 640x480, 1 at 320x200; +0x81 window,
+0x83 quality, +0x85 distance, +0x87 car detail, +0x89 horizon; the dashboard's lists double the
view's pixels by their +0xc rate (`0x8b8f0`); the frame sets the view up again after a change
(`0x5bdaa`).
Main menu `0x42c30` (NOTES §4.2): the choices in CONFIG.DAT (+0x6d type, +0x91 opponent,
+0x95 track, +0x9b car, +0x5f50 unlocks); typed codes SKCART EARALLYE CASINOVILLE
CONCEPTCAR ROCKYROAD TURBO NITRO EXPLODE (`0x46e94`), End held = no mercy. EXPLODE: the horn throws the cars
near into the air (`0x7ac03` → `0x78370`, NOTES §4.13's traffic). Options
`0x53a30`: +0x97 gearbox, +0x99 ABS, +0x8f traction (0 on, 1 off, 2 none).
Driving (`0x10400` controls every 2nd step, `drive_player` `0x382d0`): the automatic
starts in neutral (gear up = drive); `0x4071c` reads the node's slope +0x12 (NOTES §4.6).
The opponent (NOTES §4.13): kind 3, a step `0x7b34c`, every 4th its lane and target
speed (`0x7a040`), its driver's record `0xf4070` + 0x8d a letter, the settings `0x63f50`.
The race from Control Central (NOTES §4.21): `race_from_menu` `0x3770c`, `race_run`'s
`race_setup_record` `0x36c5c` (racers 1/2/8 by type; police and 6 traffic head to head on
open roads), the cars `0x653a4` (menu car → game car `0x36250`), the grid `0x370bc`, the
loading picture `0x64d28` (SIMDATA/SLIDES), EA's random numbers re-seeded from the clock.
After a race (NOTES §4.21): the session `0x15cb68` (`fe_new_session` `0x7469c`), `0x68ab0`/`0x68dcc`
the results and ranks, `fe_standings_segment` `0x6a9d0`, `race_menu_checkpoint` `0x1a5d8`, the films
`0x37ff0`/`0x3823c`, `race_menu_finish` `0x1b6a0`, the track's `0x6b938` and ten best `0x69f8c`, the
final/tournament standings `0x6f070` (points `0x689f8`), a circuit's `0x6cb24` with its laps `0x6e1ac`
(lap record `0x69438`; Rusty Springs' time trial measures the car, `0x417a2`); after a race the
menus get six colours back (`0x31bf4`).
The replay (NOTES §4.22): the race record `0x12da0c` (`race_setup_record` `0x36c5c`), recording
`0x5d888` (controls every 2nd step, snapshots every 1800 counts), REPLAY.RPL `0x5c3b0`/`0x5c4a4`,
playing `0x5efb0`→`0x5e948`/`0x5ee14`/`0x5dc54`, jumps `0x5d690`, highlights `0x5c768` from the
marks `0x5c6d4`, the bar `0x5e4ec`/`0x5fcf0`; the controls' kinds (+0x33/+0x37 → `0x12d974`) go
with the file (the CD's were driven with a joystick); the race menus' Replay/Highlights keep
the session (`0x1add8`), the pause menu's keeps the clocks and races on after Done (`0x5e020`),
watching another car while racing its own bar (`0x7ef14`, `0x7f010`),
the attract mode plays REPLAY1..3 from its second round (`0x34284`).
Menus: held keys, letters blended per pixel, an inverse colour table after every fade up
(NOTES §4.2).
The race menus' Game options `0x12760`; the audio screen `0x105dc(where)` (+0x73 song, +0x75
race music, +0x77 menu music, +0x79 effects, +0x7b speech, +0x71 the card's mode, +0x7d 3D);
the menus' F5 silences all (`0x72638`, kept in `0x15d148..`, `0xf748e`).
Load and save (NOTES §4.2): `0x3c0c8`/`0x3d4c4`, the list `0x3aacc`, save `0x3a2b0`/`0x3a628`, the
message `0x74dcc`; a game on `0x15d134` (Quit tournament ?, Track already completed, `flad`).
**A store to `[esp+N]` after a `push` is the frame's `N-4`** (`tools/espdepth.py`).
The cars' showcase `0x17724` (NOTES §4.2, `showcase.c`): pages `0x16290` (his/gen) and `0x15008` (per/mec),
the opponent's leaves its last car as the opponent; the commentary `0x72370` up to a minute long.
The track's screen `0x76380` (NOTES §4.2, `trackrec.c`): segments, lengths, laps, the time of day
`0x12dd30` (the race record's end: the family `0x12c340`, `_T01`), Best Times `0x75288`.
Credits `0x25600` (CREDITS.FSH, credits.asf); `0xed0fc` counts every timer tick, screens zero it to time
things, the menu music's choice reads it (`timer_counter` in the port).
The controls screen `0x22bd0` (NOTES §4.2): +0x5f36.. kinds and keys (menu key codes), conflicts `0x7390c`,
the key `0x733a0`; the race's scan codes `0x36a0c` (`0x36758` German → US, `key_scancode` `0x91d60`, table `0xf7b74`).
`tools/fecompare.py`: a port menu picture against DOSBox's, by palette entry.
Races at 640x480 (NOTES §4.24): the default (+0x7f 0, `0x12c308` 1), `H(lo, hi)` in the port; detail 3
every other line (`0x12d9da` the clear); the start beeps `0x5257c` are an empty routine; the circuits'
sky ring unshrunk at 640x480 (`0x5b984` before `load_track_info`).
The MiSTer's race sizes (NOTES §4.27): 640x240/640x200 the 640x480 race's rows shown (`video_rows`, the
painters skip the others, `raster_rows`); 320x240 the 320x200 screen stretched with the 3D at 240 rows
of its own (`tall.c`, tall windows in `view.c`, the lists' copy noted in `bamf_copy`).
The watching bar (NOTES §4.22): another car watched or the player stopped, drawn `0x7ef14` (`hud.c`
`watch_draw`), read `0x7f010` (`replay.c` `watch_step`): the cars 1..8, < >, the camera, the driver's name.
Skid marks (NOTES §4.10's end, `skid.c`): 125 records `0x149574` written as the player's car is drawn
`0x671dc`, painted at the road's near rows `0x679ec(-999)` as shadows `0x989a0`; none with +0x87. The dust
`0x664bc` at their newest wheel points (smo/dir of SMOKE.QFS): its random numbers the port's own (the
original's are the collisions', so frames drawn would change the race).
The player's wheels (NOTES §4.10): tyre pictures `0x66148`, front wheels `0x6634c` (`0x97eb8` moves a part's points in
the model's own copy); one-colour polygons `0xaa990` in the slot's nearest colour (`0x976c0`).
The joystick and the mouse (NOTES §4.23): EA's game port `0x94fa8`, the poll `0x4e3e8`, start-up
`0x4d1fc`, `joystick.cfg` `0x4d4a0`/`0x4d5ec` (`0x12f294`, 23 ints), the calibration `0x4c6b0`, the race
`0x4e0b8`, the control word by kinds `0x48bec`, the mouse `0x47b78`, the menus `0x4dfe0`.
A pad drives on the Keyboard kinds too (all or nothing; the shoulders shift, Start is Esc);
**analogue steering and pedals need Joystick chosen in Options → Controls**, as the original
(Danik, 2026-10-20: no automatic switch). The player's controls: `README.md`.
`tools/checks.sh`: every check in one command (logs `build/logs/chk`).
Force feedback (NOTES §4.26, `ff.c`/`ffdev.c`): the original's I-Force commands (packets `0x4c258`; kick
`0x4ba10`, shakes, rumble, steady push), the game's side `0x4d670..0x4dcb6` (walls `0x4d878`, objects `0x4d730`,
cars `0x4d7a4`, gears `0x4d8c0`, the road `0x4d9a8`, steady `0x4d928`, crash `0x4dc40`); played on SDL haptics or
a pad's rumble; equal, `tools/ff_emu.py`. On the MiSTer a wheel's forces go through the framework's `MH_Force*` (`mister.c`), a pad rumbles.
Smooth motion (NOTES §4.25, the port's own, `smooth.c`): the steps stay the original's 33.3 a second;
each retrace's frame is drawn with the cars, camera and dials between the last two steps, then put back
(F11, `renfs.cfg`); stepping faster (`0x12dd0c`) would make the game faster, not smoother.

## File formats (details in `docs/NOTES.md`, "Formats")

| Ext | What | Status |
|---|---|---|
| `.FSH` | EA shapes, "SHPI": directory of 4-char names, 8-bit pictures + palettes (`!pal`) | read (`tools/fsh.py`) |
| `.QFS` | packed FSH (RefPack `10FB`, 23 Huffman or pairs) | read (`tools/fsh.py`) |
| `.CFM` `.FAM` | `wwww` containers of `ORIP` 3D models (1/128 units) + `SHPI` textures: cars, track families | read (`tools/orip.py`) |
| `.TRI` | track: road nodes (x y z, heading, lanes), speeds a section, objects, the road's 3D points | read (`tools/tri.py`) |
| `xxxINFO` | a track's sky and horizon settings, text | read (`tools/trackinfo.py`) |
| `.PBS` `.PDN` | car physics (torque, gears, ABS/TCS), Huffman-packed `30FB` | read, every field the game uses (`tools/carspec.py`, NOTES §3.10) |
| `xxFB` | EA's packers: RefPack `10FB`, Huffman `30FB`/`32FB`/`34FB`, pairs `46FB` | read (`tools/unpack.py`, equal to the game's own on all 527) |
| `.EAS` `.ASF` `.AS4` | EA sound/music (`EACS`, `1SNh` streams): PCM or IMA ADPCM | read (`tools/eacs.py`) |
| `.BNK` | EA sound bank (engines, collisions): 128 offsets, `EACS` records | read (`tools/eacs.py`) |
| `.TGV` | EA video: `kVGT` key frames (RefPack), `fVGT` 4x4 blocks + motion vectors, 320x240 at 15 a second | read (`tools/tgv.py`, equal to ffmpeg) |
| `.FFN` | fonts (`FNTF`): 4-bit smoothed bitmap + character table | read (`tools/ffn.py`) |
| `.INV` | the menus' inverse palettes (15-bit colour → entry), 32 KB | read (`tools/inv.py`) |
| `.B00`.. `.MMB` `.FMM` | `BAMF` blit lists: which spans of the 3D view reach the screen (HUD, dashboards) | read (`tools/bamf.py`) |
| `.RPL` | replays: the race record, every driver's controls (a word every 2nd step), car snapshots every 27 s | read (`tools/rpl.py`) |
| `CONFIG.DAT` | the front end's state and the records (9 tracks, times in 1/60 s, speeds 16.16 m/s), copied whole to `0x156c14` | read (`tools/config.py`) |

Language: English art is `ART`, `SHOW`, `SLIDES`, `DASH`, `SPEECH`, `NTRACKFM`/`ETRACKFM`;
German has a `G` in front (`GART`, `GSHOW`, ...).

---

## The port (`port/`)

```bash
cmake -S port -B port/build && cmake --build port/build
port/build/renfs                                   # the game (movies, title, game mode screen, for now)
port/build/renfs --shot build/shots/title.ppm 120  # no window: the picture of retrace 120
port/build/renfs --race cy1                        # straight into City 1 with the saved car, opponent and race type (tr5 = the rally)
port/build/renfs --help
```

C, CMake, SDL2 linked dynamically. A framebuffer of palette numbers at the game's mode
(640x480 the front end, 320x200 a race), shown 4:3 through the palette at each vertical
retrace (60 a second at 640x480, 70 at 320x200, kept whatever the monitor); the palette is
8 bits a channel as the game keeps it and 6 on the card, as the VGA's DAC (so fades are
palette fades, step for step). The 100 Hz clock is real time, or the retraces counted
under `--shot`. The game's blocking loops stay loops that wait for a retrace.
Data folder: `-data`, else `$RENFS_DATA`, else `./NFSSE`, else `NFSSE` beside the program
(a folder asked for is used or refused). Home: `-home`, `$RENFS_HOME`, `~/.config/renfs`
(the port's own formats, never the game's files). `RENFS_LANG=de`: the German art;
`RENFS_NOSOUND=1`: no sound; `RENFS_SNDWAV=f`: what is heard written to a WAV (checking);
`RENFS_JOY=1|2`: a test joystick of 4 or 2 axes moved by `RENFS_INPUT`'s jl jr ju jd (the stick), jg jk
(throttle, brake), jb1..jb6 (5/6 a pad's shoulders: the port's gear up/down), jx/jy/jz/jw<n> (exact counts 0..511);
`RENFS_INPUT="<retrace>:<key> .."`: keys and mouse given at those retraces (esc enter
space up down left right tab y n j f1-f9 bksp del ctrl alt lshift rshift end click
rclick m<x>,<y>, t<text> typed, k<letter> or k<digit> held; `<from>-<to>:<key>` holds a key through those retraces, several at
once; for `--shot` tests);
`RENFS_SHOTRAW=f`: `--shot` also writes the palette numbers (comparing by entry);
`RENFS_THROW=s:x,y,z`: the car given a speed at race step s (to see a crash);
`RENFS_PLACE=s:a,c,v`: the opponent put a m ahead and c m beside at v m/s at race step s;
`RENFS_VIEW=n`: the race watched from camera n (outside: 2..5, 8..12);
`RENFS_RES=320|640`: races at that size whatever the settings say (`320x240`, `640x240`, `640x200`: the MiSTer's sizes);
`RENFS_FF=0..100`: the force feedback's strength (else `renfs.cfg` `forcefeedback`, 100); `RENFS_FFLOG=f`:
every force-feedback command written with the race's step;
`RENFS_SMOOTH=0|1`: the motion smoothed between steps or not (F11's setting, `renfs.cfg`, untouched);
`RENFS_TOD=n`: the track's time of day in `--race` (0 morning, 1 midday, 2 evening);
`RENFS_MIRROR=1`, `RENFS_RALLY=1`: the mirrored track or the rally in `--race`;
`RENFS_EXPLODE=1`: the EXPLODE code in `--race`;
`RENFS_LAPS=n`: n laps of a circuit in `--race` (else the settings' race length, as Control Central `0x437fc`: 8 on TR1 by default);
`RENFS_CONFIG=f`: a CONFIG.DAT for the test (the original's, `nfsdos.py mem:156c14:5f52:f`).
`RENFS_SEED=n`: the race's random numbers seeded with n (else the time, as the original).
`RENFS_RACE=t`, `RENFS_REPLAY=f`: as `--race t`, `--replay f` (the MiSTer's launcher gives no arguments); `RENFS_PERF=1`: every 5 s of a race,
the pictures and steps a second, each picture's time, the slowest, the steps' share.
`RENFS_REPLAYCHECK=1`: a replay played from the start compared with each of its snapshots instead of put back (2: every word that differs).
`RENFS_SESSION=f`: the original's results (`nfsdos.py mem:` of `0x15cb68`, `0x12a150`, `0x14fae0`)
put in as the standings begin (comparing the pages; `nfsdos.py poke:` writes test numbers in).
Sound: SDL's device at 44.1 kHz 16-bit stereo, each stream converted by an
`SDL_AudioStream` and added in the callback. Movies: the sound read ahead (half a second
queued), the pictures timed by the sound heard, as the original with sound on.
`src/files.c` (case-insensitive paths), `unpack.c` (all of EA's packers on the CD),
`fsh.c`, `video.c` (screen, clock, keys), `audio.c`, `eacs.c` (EA sound blocks), `tgv.c`
and `movie.c` (the movies), `fe.c` (front end screens), `mainmenu.c` (Control Central), `options.c` (the options screen, the race menus' Game options), `menu.c` (what the menus share:
keys, mouse, text styles, the yes/no dialog), `font.c` (FFN, letters blended pixel by
pixel, `nearest_colour`, the inverse table), `sound.c` (the front end's sound bank and
music, the race's music `0x63d88` and speech `0x722b4`), `audioopt.c` (the audio screen), `gfxopt.c` (the graphics screen `0x32d08`), `ctrlopt.c` (the controls screen `0x22bd0`: kinds, keys redefined, conflicts; the race's keys from them, `drive_keys_from_config` `0x36a0c`), `joystick.c` (the joystick: found, calibrated, `joystick.cfg` in the home folder, read in races and menus), `smooth.c` (the port's smooth motion between the race's steps, F11), `tall.c` (the MiSTer's 320x240 races: the view at 240 rows in the stretched 320x200 screen), `ff.c` (force feedback: the original's
game side and I-Force commands, equal, `tools/ff_emu.py`) and `ffdev.c` (those commands on SDL's haptics or a
pad's rumble, or written down), `skid.c` (the tyres' marks: the ring `0x671dc`, the painter `0x679ec`, the line `0x989a0`; their dust `0x664bc`; equal, `tools/skid_emu.py`), `showcase.c` (the cars' showcase `0x17724`: its pages, film and commentary), `trackrec.c` (the track's screen `0x76380` and its Best Times `0x75288`), `credits.c` (the credits `0x25600`: five pages of CREDITS.FSH, 20 s each by the timer's count `0xed0fc`), `loadsave.c` (load and save `0x3c0c8`/`0x3d4c4`, the file list `0x3aacc`: tournaments as `<name>.sav` in the home folder, replays saved, played, deleted; a game on `0x15d134`), `config.c` (CONFIG.DAT; the built-in defaults, equal byte for byte,
`tools/config_emu.py`), `main.c`.
The race so far: `raster.c` (the quad and triangle painters), `poly.c` (projection, quad cutting),
`road.c` (the frame's points, into the view; the mirror's five sections behind), `roaddraw.c` (the road painted far to near,
the roadside objects, the walls `0x59294`, the 3D objects `0x66738`, the mirror's road behind (`tools/mirror_emu.py`, NOTES §4.7); pixel-exact on the snapshots, `tools/roadpic_emu.py`), `model.c`
(EA's 3D library: ORIP models placed, sorted, painted), `cardraw.c` (the cars as the road
reaches their rows, their shadows, the racers' markers; pixel-exact, `tools/cars_emu.py`;
NOTES §4.10), `smoke.c` (the crash's smoke: SMOKE.QFS, the add-table `0x64488`, the
puffs `0x64a74` stretched by EA's `0x96aa8`; exact, `tools/smoke_emu.py`), `polyfill.c` (EA's polygon filler `0x8bc71`, lines `0x8bb3c`, the smoothed
wide line `0x8b195`; exact, `tools/polyfill_emu.py`), `hud.c` (the dashboard over the view:
`0x30094`, `0x31684`, the needles, the map `0x31cb4`; from outside `0x3081c`, `0x314e4`, the small rev counter `0x31168`; the keys F7 F8 M F9 `0x2f644`, the measurements `0x2d8c0`;
pixel-exact, `tools/dash_emu.py`; NOTES §4.11), `track.c`
(the TRI and its ring of sections), `trackinfo.c`, `trackfam.c`, `racepal.c`, `sky.c` (the sky, the circuits' ring `0x4ace8`, the mirror's horizon), `view.c` (windows, lens,
matrices), `car.c` (cars' files, the grid), `camera.c` (the driver's eyes; outside, modes 2..5 and 8..13, the crash's view
`0x6324c`; step for step, `tools/camera_emu.py`), `dash.c`
(the dashboard's file, colours, places and BAMF lists), `drive.c` (the player's car driven: controls, gearbox, engine, tyres, walls, jumps;
equal step for step, `tools/drive_emu.py`; NOTES §4.6), `crash.c` (a hard wall hit: the car
tumbling as a box, landing or put back, its smoke; the 4th-step refresh 0x786d4; equal step
for step, drive_emu), `timing.c` (the finish, the racers' order and places, the splits,
the race's end; equal step for step, `tools/timing_emu.py`; NOTES §4.12), `opponent.c` (the opponent's driving: `drive_opponent` `0x7b34c`, its plan `0x7cf98`,
`traffic_police_tick` `0x7a040`'s lanes and target speeds; equal step for step,
`tools/opponent_emu.py`; the traffic's coming and going, NOTES §4.16; the police, §4.17), `race.c` (`race_run`: the race as the settings ask, its loading and
frame, a step every 3 ticks (TURBO 2 2 3); `race_from_menu`, `--race`),
`sndlib.c` (EA's sound library: banks, voices by mask and priority, the random spreads,
pitch and sides; NOTES §4.18) and `racesnd.c` (the race's 16 channels: engine, road, tyres,
horns, siren and radar, the two nearest cars, the hits; equal step for step,
`tools/opponent_emu.py --full --sound`), the samples mixed by `audio.c`'s voices,
`session.c` (what a race leaves: results, totals, ranks, records taken, a tournament's points),
`standings.c` (every standings page after a race; equal by palette entry), `racemenu.c` (the
checkpoint and finish menus, the winner's and the police's films),
`pause.c` (P's hold `0x63110`, the check `0x63304`, the pause menu `0x56474` with its map; NOTES §4.20), `racekeys.c` (`race_keys` `0x47c88`: C the camera `camera_next` `0x1494c`, Alt+Ctrl+1..8
the car watched, F1..F6 view size, pixels, road distance, sky, sound, markers, Shift+End giving up `0x1f0b4`; the view
key equal, `tools/camera_emu.py`; NOTES §4.19),
`replay.c` (the race record, recording, REPLAY.RPL, playing back, the highlights; equal to the
CD's four replays at every snapshot, `RENFS_REPLAYCHECK=1`; `--replay <file> [highlights]`),
`collide.c` (cars meeting cars: `0x1c878`'s loop, the boxes pushed apart `0x2b864`/`0x26324`,
the hit `0x2afb8`, EA's random numbers `0x91e04`; equal, `tools/collide_emu.py`, NOTES §4.14;
the roadside objects knocked over `0x1ebf4`, NOTES §4.15),
`exe.c`: **the game's fixed tables are read from the CD's `NFS_DOS.EXE`** at the
original's addresses (checked by a sum of the image), never copied into the port. The menus' look is checked against the
original's screenshots by palette entry (DOSBox Staging changes colours slightly).
**Headless tests set `RENFS_HOME` to a scratch folder** (once the port writes settings).

---

## Repository, version, releases

`github.com/ItsDanik/ReNFS` (public, GPL-3): `port/`, `tools/`, `docs/`, this file, `README.md`
(for players first: what it is, the CD, download, keys, settings; then building and the method).
Version = `VERSION` + one per commit since it changed (`tools/version.sh`, `renfs --version`);
nobody edits a number to commit; a new series: write e.g. `0.7.0b` into `VERSION`.
Releases: `tools/release_linux.sh` (the `mister-hybrid-tc` Debian 11 container: glibc 2.14, SDL
2.0.9: **use no SDL function newer than 2.0.9**, check with `objdump -T`) and
`tools/release_windows.sh` (MinGW-w64, SDL2.dll beside it; tested under wine: the pictures and
the replay checks equal to Linux's); packages in `dist/` (ignored), the players' READMEs
`tools/release_*_readme.txt`; a GitHub pre-release `v<version>` while beta. The port's Windows
differences are in `files.c` (the program's folder by `SDL_GetBasePath`, home `%APPDATA%\renfs`,
`_mkdir`); `long` is 32 bits there (lengths only). Errors a player must see go through
`files_error` (a message window). `renfs.cfg` (`files_cfg_get`/`_set`): smooth, fullscreen
(Alt+Enter), forcefeedback; `-lang de`.

## Video

The game uses two modes: **320x200** (low detail, `initgraphics(320, 200, 0)`) and
**640x480x8** (VESA, `initgraphics(640, 480, 8)`). **The front end exists only at 640x480**
(every menu picture is 640x480, there is no low-resolution set). On the MiSTer the menus
stay 640x480 (Danik, 2026-10-18).
