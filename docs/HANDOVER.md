# Handover — where ReNFS stands

Read this first in a new session, then `CLAUDE.md` (the summary and the rules) and the
sections of `docs/NOTES.md` for the area you work on. Keep this file current: what is done,
what is next, what is waiting for Danik.

Start a new session in `~/renfs` (the game) or `~/MiSTer-ReNFS` (the core).

**2026-10-19: `~/renfs` was lost and rebuilt.** Deleting the old link `~/nfsse` (which
pointed to `~/renfs`) emptied the project. Restored from Claude Code's file backups (notes,
handover, `CLAUDE.md`, our own tools), `~/openscreamer/tools` (the copied tools), the game's CD image (→ `NFSSE/`) and its installed copy (→ `NFSSE-Install/`), and `tools/rebuild.sh` (the
Ghidra project and `build/decomp`, one command). The git history was started again.

## Done (2026-10-18, first session)

- Names agreed with Danik: engine **ReNFS**, game `SE|Need for Speed SE`, hand-written
  reimplementation, local repositories only.
- `~/renfs`: git repository, `.gitignore` keeping out the game
  (`NFSSE/`, `NFSSE-Install/`) and `build/`. Tools copied from OpenScreamer (`unle.py`, the
  Ghidra scripts, `ghidra-ext/`) and new ones: `tools/fsh.py` (FSH/QFS pictures to PNG,
  RefPack in Python), `tools/errnames.py` + `tools/RenameFuncs.java` (function names from
  EA's error messages, `tools/names.txt`).
- Ghidra project `build/ghidra/renfs` (`nfs_flat.bin`), analysed, seeded from the
  relocation records, 1,953 functions; every function decompiled into `build/decomp/`
  (`f_<addr>.c`, `INDEX.txt` with callers and callees). Rebuild steps: `CLAUDE.md`.
- `main` = `0x4716c` found; the start-up and the two video modes (320x200, 640x480) seen.
  First map of the program (NOTES §4: the front end's state machine `0x42c30`), the
  `wwww` containers' layout (§3.2), the TRI loader `0x4f010` and what it reads (§3.3).
  `tools/calltree.py` prints the call tree under a routine.
- `~/MiSTer-ReNFS`: stamped with `hybrid/new_core.sh ReNFS "Need for Speed SE"` (hybrid
  at `68b4bd4`, host version 14), `package/games.txt` = `SE|Need for Speed SE`; the core
  builds as stamped (`./core/build_core.sh`, timing met). The game
  submodule is not added yet (there is no repository to point at: add `~/renfs` as the
  submodule `renfs/` on its branch `mister` once Danik agrees on GitHub, or from the local
  path meanwhile).
- The launcher's catalogue needs no change: it is made from `package/games.txt` by
  `distribution.py` (the skill's step 3 predates that).

## Done (2026-10-19, second session)

- The project rebuilt after it was lost (see the top). `tools/rebuild.sh` is now general:
  `--decomp` re-applies `tools/names.txt` and decompiles again in minutes, and it takes the
  entry, the code range and the BSS from the EXE's layout.
- **The skill `reverse-engineer-dos-game`** (`~/.claude/skills/`): the method of
  OpenScreamer and ReNFS, the toolkit, the decompiler's traps, checking against DOSBox.
  What ReNFS learns that holds for any game goes there too.
- **The program mapped, first pass** (NOTES §4): 85 of the game's routines named (start-up,
  the front end's screens, the race and its loading, HUD, speech, replays, saves). The race
  loop is `race_main` `0x6348c` steps + `race_draw_frame` `0x5bc40`. The race clock (the
  physics' rate) is 100 Hz divided in a 3-cycle: 33.3 steps a second by default, 30 to 60
  by a setting (NOTES §4.3). `tools/strmap.py` lists the strings each routine uses.

- (continued) The libraries the game uses, its fixed-point arithmetic (16.16, a rounding
  multiply, 65,536 a turn: the port can match it to the bit), the cars' six driving kinds,
  the frame's drawing (NOTES §4.4-4.8).
- **The original runs here, unseen** (`nfs-dos`, `tools/nfsdos.py`, NOTES §5): into a race
  with keys, its memory read. **The race's 33.3 steps a second is measured**, not only read.
- (third part) The race's rules mapped: what runs on which step (every step / 2nd / 4th,
  measured in the original with the new `peek` step; an earlier note had it one place off),
  how a race ends (finished, arrested, everyone turned back, quit), each racer's results
  record, cars against walls and each other, the traffic's driving, and the police: stop
  quickly when caught and it is a warning, after a long chase a ticket, the second ticket an
  arrest (NOTES §4.3, §4.8). 22 more routines named (215 names in all).

- **The track format is read** (`tools/tri.py`, NOTES §3.3): the road's nodes, lanes,
  heading, the speed limits and the opponents' and traffic's speeds a section, the roadside
  objects, and the road's 3D surface, all from the loader's code. A picture of every track
  from above (`build/tri/`). Left and right checked against the original running in DOSBox
  (the game's pointers now read through DOS/4GW's paging: NOTES §5).

- **The 3D models are read** (`tools/orip.py`, NOTES §3.2, §3.5): the `wwww` containers,
  the `ORIP` models (points, polygons, textures, the named parts: wheels, lights,
  steering), drawn textured from eight sides: the cars and the circuits' grandstands, water
  tower, arches look right. Units are 1/128 m (the NSX model is the real car's size).

- **A track drawn from its own files matches the original** (`tools/trackview.py`, NOTES
  §3.6): the view at the start of the City race, road, textures, roadside pictures and
  buildings, next to the DOSBox screenshot of the same spot (`build/tri/compare_cy1.png`).
  The track, model and texture formats are proved together.

- **Fonts and sound are read** (`tools/ffn.py`, `tools/eacs.py`, NOTES §3.7, §3.8): every
  font draws text; every speech, music and sound-bank file converts to WAV
  (`build/snd/`). The music's compression is standard IMA ADPCM, found in the mixer's own
  decoder; ROCK1 matches ffmpeg's EA decoder sample for sample. The music loops are
  understood (a marker block points back at the block the loop starts with).

- **EA's packers are read and the car physics files opened** (`tools/unpack.py`,
  `tools/carspec.py`, NOTES §3.9, §3.10): all 527 packed files on the CD unpack exactly as
  the game's own routine does (run in an emulator to check), which also opens 23 menu
  pictures the picture tool couldn't (German records screens, rally maps, police screens).
  The car files give the torque curves, the gears' top speeds (the real cars' figures),
  the rev limits and which cars have ABS and traction control (`build/cars/torque.png`).

- **The movies play** (`tools/tgv.py`, NOTES §3.14): all 26 (EA logo, title, attract
  loops, car showcases, police and winners' clips) decode frame for frame the same as
  ffmpeg's own decoder, sound included (`build/tgv/title.png`).

- **The replays are read** (`tools/rpl.py`, NOTES §3.15): a replay is the race's settings
  plus the driver's keys every second step and a snapshot of every car every 27 s; the
  game drives the race again from them. The CD's four (alpine, Burnt Sienna,
  Transtropolis, coast) drawn on their tracks (`build/rpl/*.png`). For the port this
  means the race must run exactly as the original's for a replay to play right.

- **CONFIG.DAT is read** (`tools/config.py`, NOTES §3.16): the player's name, the menus'
  last choices, the keys and the records of all nine tracks (best segment times or laps,
  top speeds, three tables of race times). The CD's records are not the ones built into
  the program (a revised set), so the port should start from the CD's file.

- **The port runs: the title picture on the screen** (`port/`, NOTES and `CLAUDE.md` "The
  port"). One command builds it; it finds the CD's folder, reads the game's own
  `TITLE.QFS`, fades it in and out as the original does and shows it for its 5 seconds
  (`build/shots/title_120.png`; `RENFS_LANG=de` the German one). The picture equals the
  reader's pixel for pixel; the port's unpacker equals the game's on all 527 packed files.

- **The port plays the movies with sound, as the attract mode does**: the EA logo, one of
  the three attract movies (chosen at random the first time, then in turn), the title
  movie, then the title picture; Space, Enter or Esc pass each one. The pictures equal the
  reader's at the right moments; the sound heard equals the movie's own (recorded with
  `RENFS_SNDWAV`, the same to within a sample all the way through); a whole run takes
  79 s, the movies' own length. Still to come in the attract mode: the replay race from
  the second round on (needs the race).

- **The port's game mode screen and exit dialog equal the original's** (screenshots
  compared by palette entry: 9 pixels, and 1 near-black pixel). Single player / Modem /
  Exit with the mouse and the keys, the move and choice sounds, the music chosen as the
  original does it, 45 s idle goes back to the movies, Esc asks "Exit to system ?".
  Single player asks for the name as the original does (typed, Backspace, Del, Esc;
  equal to the original's pictures), then the main menu. Modem is left out (no
  network). Under the hood: the menus' font and letter blending, the mouse pointer, the
  inverse colour table, CONFIG.DAT read from the CD.

- **The port's main menu ("Control Central") equals the original's** (five runs of
  keys compared screen by screen: 1 to 24 pixels each, DOSBox's colour noise). The
  player's car, the opponent, the race type and the track change with the arrows, the
  keys or the mouse; the mirrored and rally tracks, Lost Vegas and the Warrior unlock by
  their typed codes (SKCART checked against the original), End held on head to head shows
  "No mercy", Esc / Cancel go back, the right button asks "Exit game ?", 2 minutes untouched
  shows the title. (Race, the cars' showcase and the track's screen: since done.) The
  menu's choices are kept between runs (`~/.config/renfs/config.dat`).
- **The options screen equals the original's** (gearbox, ABS, traction control, Done,
  Cancel, Esc; six screens compared). Audio, graphics, controls, load / save and credits
  behind it are not written yet.

- **The race, under way (2026-10-19, third session)**. Ported and equal to the original
  run on snapshots of a City 1 race (Unicorn, the original's own code against the port's):
  the track file and its ring of sections, the sky settings and tint, the family's
  textures and their re-colouring (`trackinfo.c`, `trackfam.c`, `racepal.c`), the fixed
  tables read from the CD's `NFS_DOS.EXE` (`exe.c`), the frame's road points and their
  move into the view (`road.c`), the quad and triangle painters and the cutting of near
  pieces (`raster.c`, `poly.c`), and **the road painted far to near with its roadside
  signs (`roaddraw.c`): the same picture pixel for pixel on all 6 snapshots**
  and **the open roads' sky (`sky.c`, with EA's sin, cos and atan2 in `exe.c`)**:
  City 1's view, sky and road together, the same picture pixel for pixel on 5 snapshots
  (`tools/roadpic_emu.py`; `build/shots/road_d2_port.png` next to `road_d2_orig.png`).
  Then the views and lens (`view.c`), the starting grid, the node a car is on and the
  driver's camera (`car.c`, `camera.c`), and the race palette made alone with Thomas's
  inverse colormap (`racepal.c`, `font.c`, `dash.c`): all equal on the 6 snapshots.
  **`renfs --race cy1` draws City 1 from the starting grid by itself** (`race.c`): the
  512TR standing in slot 7, the driver's view, sky and road; the same picture by palette
  entry as the original's road and sky at the standing start (r1a).
  **The cars drawn** (`cardraw.c`, `model.c`: EA's 3D library, the rows a car is on, each
  car placed, turned and its polygon groups sorted and painted) and **their pictures in
  the race's colours** (`0x65020`, a shared colour map): equal by palette entry on e1, e3,
  e4, e5, r1a, r1b (`tools/cars_emu.py`), every car's pictures equal byte for byte on all
  11 snapshots (`tools/racepal_emu.py`). The e snapshots (`build/snap/e*.bin`) are a race
  at 320x200 taken with `NFS_DOS_DIR=$PWD/build/dosnfs` (its CONFIG.DAT +0x7f = 1: low
  detail); `~/dosgames/nfs` gives 640x480. `--race cy1` draws the racers too (at the
  standing start the rival is out of view, in the original as well).
  **The cars' shadows**: EA's polygon filler (`polyfill.c`, exact on 7,300 random
  polygons, `tools/polyfill_emu.py`), the shadow laid on the road's slope, the darkening
  table (equal); the cars test still 0 pixels differing with shadows in.
  **The racers' markers** (`0x66b98`: the small triangle over each rival, its outline
  drawn by EA's line routine, exact on 10,000 random lines; colours from `0x55abc`):
  0 pixels differing with markers in (8 pixels of marker on e3).
  **The dashboard** (`hud.c`, `dash.c`; NOTES §4.11): the whole picture as a view starts
  (`0x30094`: the dashboard, the map's dim box, the status bar with its record time) and
  every frame (`0x31684`): the view painted off the screen and copied through the
  windscreen's BAMF list, the mirror's picture, the wheel turned by the steering, the two
  gauges with their needles (EA's smoothed wide line `0x8b195`, exact on 10,000 random
  lines; its blend table equal), all equal to the original pixel for pixel on e1 and e3
  with the wheel, engine and speed set seven ways (`tools/dash_emu.py`), and the start's
  lights and the radar detector's (equal with steps 0..359, radar 0..7). `--race cy1`
  now shows the dashboard over the view, the lights counting down, then the status bar.
  **The map** (`0x31cb4`, `hud_map`): the road round the car turned to its heading, the
  segment's end, the racers' dots or arrows, the police's flashing dot; equal on e1, e3,
  e5 in 18 cases each (other nodes, mirrored, the police, from outside).
  **The mirror's view** (`road_gather_behind`, `road_transform_behind`, `road_draw_behind`,
  `sky_behind`, `cars_behind_at`; NOTES §4.7 "The mirror's view"): five sections behind,
  the horizon turned round, the road's quads and the cars behind, flipped left for right;
  equal pixel for pixel on all 11 snapshots and with a car put 2..10 nodes behind
  (`tools/mirror_emu.py`). `--race cy1` shows it. (The views' windows come from their
  layouts, not the program file's +0x10.)
  **The player's car drives** (`drive.c`; NOTES §4.6 "The player's car driven"): the
  keys sampled into a control word, the steering and pedals, the gearbox (the automatic
  starts in neutral as the original: A puts it in drive), the engine and its torque
  curve, the tyres' grip and sliding, the slope, the road's edges and walls, jumps and
  landings, the car's physics file and size loaded; **equal to the original step for
  step in 297 runs** on the 11 snapshots (`tools/drive_emu.py`: gas, weaving into the
  walls, handbrake, manual, reverse, ABS/TCS, rally, circuit, drag strip, joystick,
  revving, two players, jumps, NITRO). `--race cy1` drives: arrows, A / Z the gears,
  space the handbrake (`build/shots/drive_strip.png`).
  **The crash** (`crash.c`; NOTES §4.6 "The crash"): a hard wall hit throws the car about
  as a box (its own turn and spin, bouncing off the walls and the road) until it lands
  on its wheels or is put back in the first lane, a crash counted; the smoke's puffs
  moved (not drawn yet); the every-4th-step refresh of the car's place on the road.
  **Equal step for step in 396 runs** (drive_emu, 9 of them thrown at the walls).
  Found on the way: the game runs the x87 at double precision (0x127f); Unicorn starts
  at single, `snap_emu.py` now sets it. A hard hit is rare driving CY1 (the hardest
  seen 19 m/s across, 24 needed): `RENFS_THROW=200:48,0,32` shows one
  (`build/shots/crash_tumble.png`, `crash_back.png`).
  **The status bar's numbers and the race's timing** (`hud.c` `bar_numbers`, `timing.c`;
  NOTES §4.11, §4.12): after the start the speed, gear, place, time, the split against
  the rival (or the time to beat in a time trial) and the miles left or the lap; each
  racer's finish (or turning back), the race's two orders, the splits, the places, the
  best lap; the race ends when every player is in and stopped. **Equal step for step in
  143 runs** (`tools/timing_emu.py`, 13 kinds of race on all 11 snapshots) and the bar
  pixel for pixel in 27 cases on all 11 (`tools/dash_emu.py`). `--race cy1` ends at the
  finish line (no results screen yet).
  **The camera outside the car and the crash's smoke** (`camera.c`, `hud.c`
  `hud_outside_*`, `smoke.c`; NOTES §4.7, §4.10, §4.11): the car upside down or across
  the road turns the camera to the crash view (mode 13) for at least 60 steps, then back
  to the seat; the other outside modes (2..5, 8..12) too, the screen round the view (its
  tiles, border, status bar, small rev counter with its needle, map), and the smoke
  puffs added to the picture once the tumbling car slows. **Equal**: the camera step for
  step in 1280 steps outside on each of the 11 snapshots (`tools/camera_emu.py`), the
  outside screen pixel for pixel in 7 frames on each (`tools/dash_emu.py`), the smoke in
  88 rounds (`tools/smoke_emu.py`), its table and pictures (`tools/racepal_emu.py`).
  `RENFS_THROW=200:48,0,32 port/build/renfs --race cy1` shows it all
  (`build/shots/smoke560.ppm`).
  Not yet: the gear gate (`0x2e4f0`, map off), the
  HUD's keys (`0x2f644`), the map at 640x480, the replay's buttons (`0x5c5d0`), the doubled BAMF lists (detail),
  one-colour polygons (`0xaa990`), the police lights, the cameras from the car ahead and the police car (modes 6, 7), the dust
  (`smo1`.., `dir1`..), the traffic's driving, the race's setup from
  the front end.

- **The opponent drives** (`port/src/opponent.c`, NOTES §4.13): `drive_opponent`
  `0x7b34c` every step (the steering eased, the plan `0x7cf98` every 4th step: speeding
  up by its car's table and the catch-up factors, sliding across to its place, kept on the
  road), and `traffic_police_tick` `0x7a040` every 4th step (its target speed from the road's
  limits, its driver's record and how far it is from the leading player; its lane from the
  cars ahead, the road's edges and the player beside it), the settings read from the
  program (`0x63f50`), its driver by the race type, the random numbers (`RENFS_SEED`).
  **Equal** step for step in 220 runs on all 11 snapshots (`tools/opponent_emu.py`, 15
  settings, among them the player catching and passing it; `--coverage` lists what was
  run). `--race cy1` races one opponent: it starts beside the player on the left and
  pulls away (`build/shots/opp_grid.png`, `opp_race.png`). The racer's first step moves it
  from the grid's node 20 to 19, which sets its road axes: no road follow at set-up.
  Not yet: the traffic, the police, its sounds, EXPLODE.

- **Cars meet cars** (`port/src/collide.c`, NOTES §4.14): `cars_meets_world` `0x1c878`'s
  loop, the two bodies pushed apart as boxes (`box_separate` `0x2b864`, `box_contact`
  `0x26324`), the hit's push (`box_impulse` `0x2afb8`), the views shaken by EA's random
  numbers (`0x91e04`); a hard hit throws both cars about and knocks a player's over
  (`0x1e838`); a thrown computer's car is stood up and drives again (`0x1de8a`, kind 5
  `0x7d9e4`), put back with the player after its crash (`0x7c030`). **Equal**: 22,000
  random pairs on all 11 snapshots (`tools/collide_emu.py`, every live instruction run)
  and races with the opponent placed beside, ahead and behind the player, touching up to
  188 times and a hard hit (`tools/opponent_emu.py`). `RENFS_PLACE=s:a,c,v` places the
  opponent in `--race`. Not yet: the sounds.

- **Roadside objects knocked over** (`port/src/collide.c` `objects_collide`, NOTES §4.15):
  a player's car outside the lanes knocks over the signs and posts under it (`0x1ebf4`,
  the box test `0x1ee80` from the model's size `0x65160`), the road gathered again so the
  object lies flat (`0x5004c`). **Equal**: the fallen array compared every step in
  `tools/drive_emu.py` and `tools/opponent_emu.py` on all 11 snapshots. In `--race cy1`
  (`RENFS_SEED=7`, gas from retrace 420) the "65" sign falls at about retrace 1100;
  `RENFS_VIEW=n` (a test) watches the race from camera n (`build/shots/knock_sign.png`,
  camera 4). Its force feedback since (§4.26).

- **The traffic** (`port/src/opponent.c`, NOTES §4.16): six traffic cars wait off the road
  (kind 6, `0x7e0c8`) and come on ahead of the racers (on CY1, a one-way road, all going
  their way; on a two-way road some come the other way), at most four at a time (`car_place` `0x7cac8` and its parts), placed again when
  they fall out of the racers' reach (`0x77de8`, `0x77d3c`) and after the player's crash
  (`0x7c030`), the chase dropped (`0x781ac`); sized by their models (`0x512e8`).
  **Equal**: `tools/opponent_emu.py --full` keeps the snapshot's whole field (9 cars, the
  police car parked) and compares every car every step: 419 runs on all 11
  snapshots, the coming side through a "two-way" setting (every node given lanes both
  ways). `--race cy1` has the traffic (`build/shots/traffic_beside.png`,
  `traffic_passing.png`, `traffic_ahead.png`). Fixed on the way: the race's top speeds
  are kept for all 9 cars (the ninth wrote past the port's table), and the rival's size
  is its PDN's.

- **The police** (`port/src/opponent.c`, `hud.c`, `cardraw.c`, NOTES §4.17): the police
  car parked behind the grid comes out ahead at the road's police marks (`0x79500`, by
  chance), chases a player who speeds past within 5 nodes, catches up and pulls over
  with the player (`0x79728`): a warning for a short chase, else a ticket, the player
  pulled to the right and held (kind 1, `0x7ea20`) while the full-screen picture shows
  (`TICKETDL`/`WARNDL`, `0x30424`), the police lights flashing in the palette under it;
  the second ticket ends the race (arrested, result 4). **Equal**: `opponent_emu.py
  --full` with both routines on (all 495 runs: 9 warnings, 32 tickets, 5 arrests); the police car drawn in view pixel for
  pixel (`CARS_POLICE=8,3 tools/cars_emu.py`). `RENFS_SEED=6 --race cy1` with the
  throttle held then the brake gives a ticket (`build/shots/police_*.png`). Fixed on the
  way: the race's step counter never reached the road's drawing (the animated objects
  stood still in `--race`). Not yet: the view from the police car (camera 7), the siren,
  the arrest's film.

- **The race's sounds** (`port/src/sndlib.c`, `racesnd.c`, `audio.c`'s voices, NOTES §4.18):
  EA's sound library (the CD's banks put in one table of 128, 16 voices handed out by mask
  and priority, the random spreads of pitch, volume and pan drawn from the collisions'
  random numbers, the pitch and the sides) and the game's 16 channels: the engine's two
  loops by throttle and revs, the road's noise, the tyres, the horns (the player's, the
  computer's: `0x80a60` now runs in every check, its random number is the game's), the
  radar's beep, the siren and its echo, the two cars heard loudest with their Doppler
  shift, the hits, landings, scrapes, knocked objects and the gear's clunk; the engine
  silenced while thrown about. **Equal**: `opponent_emu.py --full --sound` runs the
  original's whole sound path in Unicorn (`snap_emu.py` steps over the library's
  `push ds; call 0x94710 .. pop ds`) and compares every voice record, channel and sound
  global every step: 47 settings on the 11 snapshots, the camera following the race,
  from the driver's seat and again from outside (`SND_VIEW=3`), all 1,034 runs equal.
  What no check runs yet (two players, tunnels, the rally, mono, the siren's echo, the
  library's random spreads): NOTES §4.18's end. The other checks still equal.
  `--race cy1` is heard (`RENFS_SNDWAV=f` writes it). A voice's end comes from the clock
  (`snd_time`), not the mixer. The race's music and speech too.

- **The race keys** (`port/src/racekeys.c`, NOTES §4.19): C changes the camera
  (`camera_next` `0x1494c`; racing: the seat, behind, further back, higher, the seat
  again), and the views from the rival's and the police car (modes 6 and 7, reached when
  the race is not running) are in `camera_update`; Alt+Ctrl+1..8 watches another car (the
  status bar and the map follow it); F1 the outside view's size (three, inside a border),
  F2 the view's pixels doubled, F3 the road drawn 28, 14 or 9 sections ahead, F4 the sky
  plain, F5 the sounds off and on, F6 the racers' markers. The race starts from the
  settings' detail (+0x81, +0x83, +0x85, +0x89). **Equal**: `camera_emu.py` presses the
  view key in five more runs a snapshot (racing, through a tumble, watching the rival,
  not racing with the rival and the police close), every camera field equal each step, all
  14 modes. **Changed on the way**: the port's answer to the speed test was read the
  wrong way round (it gave the least detail: the road 13 sections ahead, the pixels
  doubled); it now answers as the snapshots' original (all detail 0), `config_emu.py`
  equal. Pictures: `build/shots/keys/` (keys.png, cams.png, watch.png); F5 heard in
  `build/snd_port/race_f5.wav` (silent from 14 to 17 s).

- **The circuits drawn whole** (NOTES §4.7): the sky ring round a circuit (`0x4ace8`,
  set up by `0x4a64c`: seven panels of the horizon picture ahead, tilted with the car, the
  sky's and ground's colours above and below; F4 too), the roadside walls (`0x59294`:
  raised from the road's edge, textured by the track's tiles) and the 3D roadside objects
  (`0x66738`: the family's models, e.g. Rusty Springs' gantries and towers, turned,
  sorted with the rest of the road and painted). `--race` takes the track's number from
  its name, so the grid, records, laps, map and sounds are the track's own. **Equal**
  pixel for pixel on six Rusty Springs snapshots (t1..t6, new, the way to make them in
  NOTES §5) and the city's (`tools/sky_emu.py`, `tools/roadpic_emu.py --walls --models`,
  `roaddraw_emu.py` call for call); the other checks still equal. Picture:
  `build/shots/walls/vs.png`.

- **The race from Control Central** (NOTES §4.21, `race.c`'s `race_run`,
  `race_from_menu`): the Race button starts the race the menu's choices ask for: the
  track and its first segment, the race type (time trial alone; head to head with the
  police and six of the traffic on the open roads; a single race of eight, on one model
  or the mixed field; a tournament's class of three cars), the player's car, the
  opponent, the laps, the codes ("No mercy", mirrored, rally, NITRO, TURBO's faster
  clock). The car's loading picture (SIMDATA/SLIDES) first; EA's and the race's random
  numbers seeded from the clock as the original does each race (the port had EA's only
  from the image). After the race, back to Control Central. Seen in pictures: the whole
  path from the title, Esc/Quit and giving up; single races and tournaments of eight on
  CY1 and TR1 driven 85 s. `--race` now uses the saved choices too.

- **After a race, first part** (NOTES §4.21, `session.c`, `standings.c`, `racemenu.c`):
  each racer's results kept between segments (times added up, ranks, the rival), the
  segment's records taken, the segment standings screen, the checkpoint menu between an
  open road's segments (Continue, Standings, Quit), the three segments in a row, the
  winner's and the police's films, the menu music after a race. **Equal** by palette
  entry to the original's standings and checkpoint screens (`build/after/`).

- **After a race, the rest** (NOTES §4.21): the finish menu (Continue, Standings, Esc,
  two minutes), the track's standings with its ten best totals and top speed taken, the
  ten best page, the race's standings so far and a tournament's points (20 15 12 10 8 7
  6 5) carried from race to race, its end opening Lost Vegas and the rest; a circuit's
  page (the best total of its race length, the lap record) with its laps page (two
  columns from nine laps) and, on Rusty Springs' time trial, the car's measurements
  (0–60, 0–100, the quarter mile, 60–0, 80–0) now made while driving; the commentary on
  every page. After a race the menus get six of the race's colours back (`0x31bf4`).
  **Equal** by palette entry to the original on every page met: a head to head's and a
  single race's open road, Rusty Springs' time trial (laps page with test numbers),
  Autumn Valley's single race, Burnt Sienna's tournament race (`build/circ/`; the
  original's numbers put in with `nfsdos.py poke:` and `RENFS_SESSION`).

- **The replay, first part** (NOTES §4.22): the race recorded as the original records it
  (the controls every second step into a ring, a snapshot of every car and of the race
  every 27 s) and written to `replay.rpl` in the home folder at the race's end; a replay
  file played back (`--replay REPLAY2.RPL [highlights]`), the highlights picked from the
  events the race marks (crashes, hits, jumps, slides, places changed, the police
  spinning). **Equal**: the CD's four replays and one the original recorded here play
  back byte for byte like the files' own snapshots to their ends (`RENFS_REPLAYCHECK=1`);
  the original's attract mode in DOSBox reaches the same road node (`nfsdos.py follow:`).
  The CD's were driven with a joystick: the steering and pedal kinds now come from the
  race record. **The replay bar** (second part): Done, |<, <<, >>, >|, play at full, a
  half and a quarter speed, pause, the camera, the time and the line along the bottom,
  by mouse (the pointer) or arrows and Space/Enter, Esc for Done; the highlights keep it
  across their jumps (their lists equal the original's, read in its attract mode). The
  race menus' Replay and Highlight reel play the race just driven and come back to the
  menu; the pause menu's Replay shows the last seconds and races on after Done, the
  time watched not counted. The attract mode plays REPLAY2, 3, 1 .. as highlights from its
  second round, Esc skipping the films. F5's silence ends before a replay (`0xf748e`).
  Open (older, found 2026-10-20): `tools/timing_emu.py` puts the second racer's finish
  time one lap slot earlier than the original in 7 of its 13 runs (the game code writes
  `0x14ea7c` + racer · 0x1d2 + lap · 4 on both sides: look at the check's set-up of the
  second car's lap first).

**Two bugs Danik found (fixed 2026-10-21)**: from outside, his car always pointed where it
stood on the grid (the heading drawn, `+0x351`, now copied from the heading every step as
`0x51fa3` does); after an arrest, the next race drawn in the previous race's colours (the
ticket still up as the race ended: `hud_free` now lets it go, as `0x31bf4`). Both in NOTES
(§4.10 `0x68258`, the six colours after a race).

**What was left of the PC game (Danik 2026-10-21: everything but the network)**: the game
mode screen's Modem/Serial greyed out with the file's own dimmed button; the dashboard's
gear lever (`0x2e4f0`, map off: gate, knob or four-part stick, `tools/dash_emu.py`); EXPLODE
(the horn throws the cars within 10 of the player into the air, `0x78370`; `RENFS_EXPLODE=1`
in `--race`); the checkpoint menu's Standings with the standings so far (`0x1b12e`); the
police ticket in a replay re-coloured to the race's colours (`0x55ee0`). Found to need
nothing: the replay's keys `0x12da04` (read at a moment that does not come), the start
lights' sound `0x5257c` (an empty routine). Left out on purpose: the network race, the chat.

**Force feedback and rumble** (Danik 2026-10-21: groundwork for the MiSTer, no wheel to try;
`port/src/ff.c`, `ffdev.c`, NOTES §4.26): the original's I-Force side ported whole (kicks
for walls, objects, cars and gear changes, the road's shakes by speed and surface, the
steady push, the crash's rumble; on/off with the pause), **equal** to the original's
routines (`tools/ff_emu.py`, 0 of 2000 calls differ, in `tools/checks.sh`). Played on SDL's
haptics (a wheel: untried) or a pad's two motors; `renfs.cfg` `forcefeedback 0..100`,
`RENFS_FFLOG`. For the MiSTer: the hybrid's SDL (`~/MiSTer-ReNFS/hybrid/sdl2/build.sh`) has
`-DSDL_HAPTIC=OFF`, so only the rumble works there until it is built with haptics on. Next: the MiSTer (memory `order-of-work`), once Danik has tried the PC game.

**The PC release** (Danik 2026-10-21): `github.com/ItsDanik/ReNFS`, public, GPL-3, version
0.6.0b (`VERSION`, `tools/version.sh`), a Linux and a Windows package on the releases page
(`tools/release_linux.sh`, `tools/release_windows.sh`; CLAUDE.md "Repository, version,
releases"). For it: the Windows build (`files.c`), error messages in a window
(`files_error`), Alt+Enter full screen, `renfs.cfg` written with every setting explained
(`files_cfg_*`), `-lang de`, `--version`, no SDL function newer than 2.0.9. The README is
for players first. Windows tried under wine only (the same pictures as Linux, a replay
equal at every snapshot): Danik's real Windows is the open check.

**Progress page for Danik**: https://claude.ai/artifact/VZdxTpYC3oLXYeBVJpoDoM (every step,
its percent done and weight). Update its steps file after each finished step.

## The MiSTer (branch `mister`, started 2026-10-21)

The core is `~/MiSTer-ReNFS` (the framework `hybrid/` and this repository as `renfs/`,
branch `mister`). Danik's answers to the interview: `~/MiSTer-ReNFS/CLAUDE.md`, "Decisions
from the interview". **The real MiSTer is shared with other agents: ask Danik before every
use of it.**

Done on the branch: `MISTER_HYBRID` builds (`./build.sh host` against fakecore, `./build.sh
mister` for the ARM); `mister.c` (the core, the CD's files looked for at the start against
`mister_files.c`, the pad as the original's joystick, Menu OK/Back, Start = Esc, R3 nitro,
the OSD's resolution, smooth motion, rumble and the Cheats page; the race runs on while
the OSD is open, as on Danik's other cores, so HUD (Bottom) is seen moving); names typed with the d-pad (`menu.c`); no calibration screen (a fixed one);
the steering and pedals given as a joystick's whenever the core is there. `RENFS_FILELOG`
lists every path the game asks for; `tools/mister_files.py NFSSE --check <log>` found none
outside the list. The ARM build plays the CD's replays equal under qemu.

The framework's wheel forces (`MH_Force*`, `hybrid/hps/mister_triggers.c`, branch
`wheel-force` of the submodule, not in Hybrid_MiSTer yet: ask Danik) and
`hybrid/tools/ff_wheel.c`, a fake force-feedback wheel through uinput that prints what it
is given.

Measured on the MiSTer (2026-10-21, `~/MiSTer-ReNFS/CLAUDE.md`): 320x200 with smooth motion
59.6 pictures a second with drawing ahead (`mister_present_at`); 640x480 13-24 ms a picture,
so there smooth motion is off (greyed) and each step is shown once. Danik's decisions: five
race sizes on the OSD (320x200 default, 320x240, 640x200, 640x240, 640x480), Smooth On.
Fixed after his first try: the d-pad and mouse kept to the replay bar after the attract
mode's replay (the mouse limits, also on `main`).

**The five sizes are written (NOTES §4.27) and measured on the MiSTer** (all near 60 with
smooth motion; 640x240 drops to 57 in busy scenes; numbers in `~/MiSTer-ReNFS/CLAUDE.md`): 640x240/640x200
are the 640x480 race with only the shown rows painted; 320x240 the 320x200 screen stretched
with the view at 240 rows of its own (`tall.c`). The core's FPGA picture rebuilt for the
new Resolution line. At 640x200 the smallest digits lose a row (OpenScreamer merged a
glyph's rows for that; not done here).

Danik tried them all (2026-10-22: everything looks right, HUD (Bottom) and the movies
included); the core's README has screenshots of the new sizes. The wheel forces are in
Hybrid_MiSTer's main (merge 4fc0233, on GitHub).

Fixed after his tries the same day, and tried by him on the MiSTer (all good): the pause
menu after its Replay and Done (the mouse's limits, 0x56d45); the race runs on while the
OSD is open (as on his other cores); L3 the horn and Y the handbrake (they were swapped);
the speech fed half a second ahead at every retrace (a showcase commentary converted at
once held the game and the music 2 to 4 s); the films centred as the original (0x34c4e,
the 320x192 ones 24 rows down). The last two also on `main`, pushed.

**Next (Danik, 2026-10-22): make the rendering as fast as possible**, in a new session.
The numbers to start from are in `~/MiSTer-ReNFS/CLAUDE.md` (Replay 1, `RENFS_PERF=1`;
640x240 drops to 57 in busy scenes, 640x480 13-24 ms a picture). Every change must keep
`tools/checks.sh` equal. Not yet: pushing the `mister` branch and creating
ItsDanik/ReNFS_MiSTer (Danik: after the optimisations).

## Waiting for Danik

- Nothing open. (Decided 2026-10-20: 640x480 races for the PC, then a 60 fps mode, then
  the rest of the PC game, then the MiSTer: memory `order-of-work`.)
  (Decided 2026-10-19: a first start uses the game's built-in defaults, `cfg_defaults`,
  ported and equal byte for byte.)

Decided 2026-10-18: menus 640x480 on the MiSTer (interlaced at 15 kHz),
races 320x200 / 320x240; commit as the work goes (pushed to GitHub since 2026-10-21).

## Next, in order

1. **Finish the map** (NOTES §4.8, "Still to name"): `0x5efb0` (the replay?), the second drawing pass `0x5a4f0`, the
   widgets `0x71208`.., the Watcom runtime. Small enough now to leave for when the port
   reaches them; the formats are the better next step.
2. **Formats: done.** Every file type on the CD has a reader in `tools/` (TRI, `wwww`,
   `ORIP`, textures, FFN, EA sound, the packers, PBS/PDN first reading, INFO, INV, BAMF,
   TGV, RPL, CONFIG.DAT). What is still open inside them is listed in NOTES §3.4.
3. **The port's skeleton: done** (movies, title, game mode screen, exit dialog). Name entry
   the main menu, the settings kept and the options screen done too. Next: the screens
   behind the options (credits, audio, graphics, controls, load / save), Danik chose
   (2026-10-19): **the race next**, the remaining menus later. In the race, the
   port now makes a first frame of its own (`--race cy1`), the cars, their shadows,
   the racers' markers, the dashboard, the map and the mirror in it, and the player's car
   drives and crashes (equal step for step), and the status bar's numbers and the race's
   timing, the camera outside and the crash's smoke, the opponent's driving, and cars
   meeting cars, the roadside objects knocked over, the traffic, the police and the
   sounds, and the race's keys, P's pause and the pause menu (NOTES §4.20), Shift+End's giving up, the race's music and its speech (NOTES §4.18's end). , the circuits' sky ring, walls and 3D objects, the race started from Control
   Central, the segments in a row, every standings page, the checkpoint and finish
   menus, a tournament from race to race, the replays. **The race menus' Game options
   (`0x12760`) and the audio screen (`0x105dc`, from the options screen, the race menus
   and the pause menu; F5's silence `0x72638`): done**, equal to the original's pictures
   (NOTES §4.2). **The graphics screen (`0x32d08`, from the same three places): done**,
   equal by palette entry; the pause menu now hands the race's detail to the settings and
   back, the frame sets the view up again after a change (`0x5bdaa`), and the dashboard's
   doubled-pixel copy lists are ported (image quality Medium/Low showed no view before).
   Its first row, the races' resolution (+0x7f: **0 = 640x480**, 1 = 320x200; the
   built-in default 0, as the original on a fast PC), is shown but the port still races
   at 320x200 (Danik's MiSTer choice; 640x480 races are a step of their own: ask him).
   The camera check had stopped building since the replay (fixed: `tools/replay_stub.c`,
   the check's copy of the car record brought up to date) and is equal on all 11
   snapshots again. **The controls screen (`0x22bd0`, from the options screen and the race
   menus): done**, equal by palette entry on 25 pictures (the prompts and "Control
   conflict" too); the race now drives with the keys chosen there (`0x36a0c`: every
   driving key, the camera's and the map's; the German game's camera key is K). Keys are
   read by their place on the keyboard (`scan_get`), whatever the computer's layout. A
   joystick or the mouse to drive with: not yet (the screen offers only the keyboard, as
   the original on a PC without them; `fe_joystick_calibrate` `0x4c6b0` waits for the
   joystick). Drive, timing, opponent and collide checks had stopped building since the
   replay (`g_replay`): `tools/replay_stub.c` added to them too. `tools/fecompare.py`
   compares a menu picture with DOSBox's by palette entry. **Load and save (`0x3c0c8`,
   `0x3d4c4`, the list `0x3aacc`): done**, equal by palette entry on 34 pictures: saving
   and loading a tournament (`<name>.sav` in the home folder, the original's 0x5b6-byte
   session; the original loads the port's file), saving, viewing and deleting replays
   (the home folder's and the CD's listed together). Control Central now knows a game
   is on (`0x15d134`): "Quit tournament ?", "Track already completed" and the raced
   track's mark (`flad`). Two traps found on the way: stores to `[esp+N]` right after a
   `push` (a row change redraws the whole screen; the list starts on its first file),
   `tools/espdepth.py` lists them. **The credits (`0x25600`): done**, the five pages
   equal by palette entry, with CREDITS.ASF; the timer's free count `0xed0fc` now runs
   in the port (`timer_counter`), so the menu music's choice varies as the original's.
   Every screen behind the options is now in. **The joystick and the mouse (NOTES
   §4.23): done.** A pad or joystick found at start-up (`0x4d1fc`, SDL game controllers
   or joysticks of two axes or more), its calibration in the home folder's
   `joystick.cfg` (the original's bytes) or made on the calibration screen (`0x4c6b0`,
   gold from the menus, green from the race menus), the controls screen's joystick and
   mouse kinds, the race driven by them (`0x48bec`, `0x4e0b8`, the mouse `0x47b78`
   relative), the buttons' horn, handbrake, view and map, and the stick in the menus
   (`0x4dfe0`). Equal to the original in DOSBox with a 4-axis joystick (pictures,
   `joystick.cfg`, the control words; `build/shots/joy/`). `RENFS_JOY=1|2` a test
   stick. A pad drives without the controls set to it too (the port's: a control on the
   keys takes the stick, the shoulders shift; NOTES §4.23) Start is Esc. **Analogue steering and pedals need
   Joystick chosen in Options → Controls**, as in the original (Danik, 2026-10-20: keep it
   so, no automatic switch; documented in `README.md` and `--help`). `tools/checks.sh` runs every check in one command. Next: the cars' showcase
   (`fe_showcase` `0x17724`) and the track records from Control Central, then the
   MiSTer core. The dashboard's keys F7 / F8 / M / F9 and Rusty Springs' measurements on the
   status bar: done (NOTES §4.11). **Races at 640x480 (2026-10-21, NOTES §4.24): done**,
   the default as in the original (settings +0x7f = 0; Options → Graphics gives 320x200):
   the dashboard, the map, the outside view, the road, the cars, detail 3 (every other
   line), the replay bar and the pause menu, equal pixel for pixel on the 640x480
   snapshots h1 h6 h7 (`tools/checks.sh` runs them too). Found on the way, not ported at
   either resolution: the player's wheels turning and its tyre pictures, the tyres' dust,
   the skid marks, the bar while watching after the finish, the start lights' beeps
   (NOTES §4.24's end). **Smooth motion, 60 frames a second (2026-10-21, NOTES §4.25):
   done**, the port's own: the race's steps stay the original's 33.3 a second, each
   retrace's frame drawn with the cars, the camera and the dials between the last two
   steps (`smooth.c`), on at first, F11 in a race off and on (kept in `renfs.cfg`). The
   race underneath unchanged (the same REPLAY.RPL byte for byte off and on; the CD's
   replays equal to their snapshots). **Next: the rest of the PC game** (Danik's order):
   the gaps above, the cars' showcase (`fe_showcase` `0x17724`), the track records from
   Control Central; then the MiSTer. **The player's car's wheels from outside (2026-10-21):
   done**, the tyres' pictures by speed and the front wheels turned (NOTES §4.10's end), and
   the one-colour polygons; equal on every snapshot (h7 0 pixels, was 1,201). **The skid
   marks (2026-10-21): done** (`skid.c`, NOTES §4.10's end: the ring written as the player's
   car is drawn `0x671dc`, painted with the road's rows `0x679ec`; `tools/skid_emu.py`:
   random rings and cars on h7 and e3, 10,360 lines all equal). **The tyres' dust
   (2026-10-21): done** (`skid_dust`, 520 puffs equal; its random numbers the port's own,
   the original's are the collisions'). The start beeps (`0x5257c`): an empty routine,
   nothing to port. **The watching bar (2026-10-21): done** (Alt+Ctrl+a number while
   racing: the cars 1..8, before / after, the camera, the driver's name; NOTES §4.22;
   `tools/watch_emu.py`: 2,000 random steps and drawings on six snapshots, all equal).
   **The cars' showcase (2026-10-21): done** (`showcase.c`, NOTES §4.2: Enter or a click
   on a car's picture in Control Central; the main page, History, General with its slides,
   Performance, Mechanical, the film, the next and previous cars, the commentary; every
   page tried equal to the original by palette entry; the opponent's showcase leaves the
   car it ends on as the opponent, as the original). The front end's spoken words were cut
   after 4 s (fixed: the commentaries last a minute). **The track's screen and its Best
   Times (2026-10-21): done** (`trackrec.c`, NOTES §4.2: Enter or a click on the track's
   picture in Control Central; the segment maps, the lengths in miles or kilometres, a
   circuit's laps, the time of day, the previous and next track, the spoken description;
   Best Times with the ten best and the top speed; 23 pages equal to the original's by
   palette entry). The time of day is the race record's `0x12dd30` and now picks the
   track's look in the race (morning, evening: NTRACKFM's `_T01` and the sky's tint); the
   mirrored and rally tracks now load their own families (`_M01`, `_R01`: the port had
   loaded the normal one). Next: what is left of the PC game (see below), then the MiSTer.
   (`0x136618`, the sections drawn: the settings' +0x85 and F3, NOTES §4.19.)
   Open: the race's sound reaches full scale at the hits and the horn (0.2 to 0.7 % of
   the samples in `build/snd_port/`): how EA's mixer sums the voices is still to compare. Model polygons of one colour (`0xaa990`): ported 2026-10-21 (the 512TR's rear bumper).
4. Front end (menus, car and track choice), then the race (track, cars, physics, HUD,
   sound), then the rest (replays, championship/tournament, cops, the showcase).
5. MiSTer (`~/MiSTer-ReNFS`): the skill's steps 5–8 — `MISTER_HYBRID`, `mister_port.[ch]`,
   `CONF_STR`, the cheats (look for them in the program: NFS SE has typed codes on the
   menus, e.g. the hidden Warrior car and the Lost Vegas track: find the code that checks
   them), 240 lines and HUD margins, `tools/test_on_mister.sh`.
