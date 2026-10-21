# ReNFS — the full record

Everything read out of `NFS_DOS.EXE` and the data, with addresses and proofs. Sections
are numbered; `CLAUDE.md` has the one-line summaries. Addresses are flat image addresses
(`build/nfs_flat.bin`, made by `tools/unle.py --fixups`).

---

## 1. The program

- Watcom C/C++32 runtime banner at `0x94446`; entry `0x94444` jumps to `0x944bc`, which
  calls `0xa9b62` (Watcom's `__CMain` equivalent), which calls **`main` = `0x4716c`**.
- `main` (first look): sets a dozen globals, `0x89897(0x12dd30, 1, argv?, 9)`,
  `0x94010`, copies 7 bytes from `0xe2a1c` somewhere, **`0x42054`** (start-up: calls
  `initgraphics` twice, fonts, sound), `addtimer` (`0x80f58`), `0x1f07c`, **`0x47000`**
  (the front end's loop, to be confirmed), `0x4d6c8`, `0x80f90`, then a function pointer
  at `0xf8a84` (exit).
- `initgraphics` (`0x84d74`, EA library, stack arguments): `(320, 200, 0)` and
  `(640, 480, 8)` from `0x341a4`; also called from `0x1a5d8` and `0x1b6a0` (two big
  routines that call nearly the same things: race set-up for the two detail levels?),
  `0x3770c`, `0x3c0c8`, `0x42054`, `0x56338`.
- EA's libraries name themselves in their error messages: memory manager (`initmemman`,
  `getmemblock`, `purgememblock`, ...), shapes (`loadshape`, `convertshapetype`), file
  handles (`openhandle`, `loadfile`, `openblockhandle`), packed files (`unpack`
  `0xa7020`, `unpacksize` `0xa711c`: RefPack), CD-ROM (`cdromread`, `cdromseek`), streams
  (`setstreamspeed`, `delstream`, `streamreader`: the ADPCM music and video), network
  (`newnetgame`, `sendpacket`, ...), windows/text (`createwindow`, `setfont`,
  `textcolour`). Banner "(c)1994-95, EAC Inc. by DM, AS, PC, IM, KH, BP, et al." at
  `0xe6219`. Names: `tools/names.txt`.

## 2. The data

- The CD (`NFSSE/`) has everything: `FRONTEND/` (ART, SHOW, SPEECH, MOVIE, MUSIC and the
  German G-versions), `SIMDATA/` (CARFAMS, CARSPECS, DASH, ETRACKFM, NTRACKFM, MISC,
  SLIDES, SOUNDBNK and German G-versions), `GAMEDATA/` (CONFIG/CONFIG.DAT, MODEM,
  REPLAY/REPLAY1-4.RPL, SAVEGAME).
- The installation copies a subset of the CD unchanged (every file byte for byte), puts
  the eleven position words (`FIRST.EAS` .. `FINALLAP.EAS`, from `FRONTEND/SPEECH/`) in
  its root, and adds `NFS.CFG` ("SB16 VIDEO ENGLISH NOREMOTE ") and
  `GAMEDATA/CONFIG/PATHS.DAT` (fixed 80-byte strings: `gamedata/config/`,
  `gamedata/savegame/`, ...). The program finds the rest on the CD.
- File names in the program (strings at `0xe0104`..): `%s%s%s.qfs`, `%s%s16.eas`,
  `%s.tgv`, `%sattract%d.tgv`, `%stitle.tgv`, `win#%d.tgv`, `cop%d.tgv`, `%s%s.SAV`,
  `%s%s.rpl`, `%s%s.tri`, `%sjoystick.cfg`, `%s.vis`, the car sound banks
  (`Supra_sw.bnk`, `OSupmb.bnk`, `traffc.bnk`, `COLLSWWT.BNK`, ...), `nfs.cfg`,
  `%sconfig.dat`, `maskhi.fsh` / `masklo.fsh` (the cockpit for 640 / 320).

## 3. Formats

### 3.1 FSH / QFS (pictures) — read

`tools/fsh.py list|png`. "SHPI", u32 size, u32 count, 4-byte id (`LN32`, `GIMX`), then
`count` × (4-char name, u32 offset). A shape: u8 type, u24 offset to its next block
(0 = last), u16 width, height, x centre, y centre, x, y; pixels follow. Types: `0x7B`
8-bit indexed, `0x7D` 32-bit, `0x78` 565, `0x22` palette 6 bits a channel, `0x24`
palette 8 bits, `0x2A` 32-bit palette, `0x7C` an attached block of unknown use (comes
after most menu pictures, w×h often 0×0: a hot spot or a link?). A file's palette is a
shape named `!pal` / `!PAL`, or a palette block attached to the picture.
QFS = RefPack: `10FB` + big-endian u24 unpacked size (`11FB`: a compressed size first),
then the LZ77 commands (2/3/4-byte copies, literal runs, `FC..FF` end). The game's
unpacker is `unpack` (`0xa7020`).
Menu pictures are all 640x480 (TITLE.QFS `bgnd`, the slides `SCRN`).

### 3.2 `wwww` containers — read (`tools/orip.py list`)

"wwww", u32 count, `count` u32 offsets from the container's start (0 = an empty slot).
Entries nest. The reader is `wwww_resolve` `0x60134`: it turns every offset into a pointer
in place, nested containers too, and returns the address of the table (`+8`);
`wwww_count` `0x602a4` reads the count before it, `wwww_entry` `0x60028` one slot.
- `.CFM` (a car, `SIMDATA/CARFAMS`): [0] `ORIP` + [1] its `SHPI` textures (high detail),
  [2] `ORIP` + [3] `SHPI` (low detail: ANSX 16 points and 6 polygons against 103 / 56).
- `.FAM` (a track family, `<track>_M01` / `_R01` / `_T01`, read by `load_track_fam`
  `0x60078`, names `%s/%s_T%.2d.fam` / `_M` / `_R` / `%s_%.3d.fam`): [0] 17 `SHPI` texture
  banks (names `A000`, `A004`, `B000`, `C004` ..: the road's?), [1] 32 `SHPI` of one picture
  `0000` each (the flat roadside objects?), [2] one `SHPI` `horz` (the horizon), [3] a
  `wwww` of `wwww` (`ORIP`, `SHPI`) pairs = the track's 3D models (`load_track_models`
  `0x66118` → `load_track_model` `0x65224` each): 1-2 on the open roads, 4-8 on the
  circuits (TR1: a water tower, a pylon, grandstands, rock arches, a bridge). What T / M / R
  choose: to find.
- `.FMM` (a dashboard, `<car>DH` / `DL` = 640 / 320): 4 `BAMF`. `.MMB`: 12 `BAMF`.

### 3.3 TRI (track) — read (`tools/tri.py`, 2026-10-19)

`SIMDATA/MISC/<track><n>.TRI`: AL1-3 (alpine), CL1-3 (coast), CY1-3 (city) are the open
roads, in three segments each; TR1-7 the circuits; AL22 = AL2 + 12 bytes. Read by
`load_tri` `0x4f010` (from `race_load_track` `0x5ba1c`) in three parts, each in memory
under the name the loader gives it, and **the file stays open**: the road's 3D points are
read from it section by section as the car goes (`0x4ee30` → `0x4ed74` → `0x4eb00`).

1. **"RoadSection"**, 0x1621c bytes, at `0x136608`:

   | at | what | proof |
   |---|---|---|
   | `+0` | u32 0x11 | every file |
   | `+4` | u16 sections in the loop, 0 = an open road | `load_tri`: loop → node mask `0x13bb88` = 4 × n − 1, `0x13bb80` = 1 |
   | `+6` | u16 sections (4 road nodes each) | the speed loop runs to `+6` × 4 nodes |
   | `+8` | i32 node spacing, 6.0 (16.16) on every track | the nodes are 6.00 apart |
   | `+0xc` | 3 × i32 (?), small, different per track | |
   | `+0x18`, `+0x20` | copied into node 0's `+0x14` / `+0x18` | `load_tri` |
   | `+0x24` | u32 the size of part 3 | `allocmem("TrackSlices", +0x24)` |
   | `+0x28` | u32 33..63 (?) | |
   | `+0x2c` | 600 × u32: each section's offset in part 3 | `0x4ed74`, `0x4eb00` |
   | `+0x98c` | **2,400 road nodes** × 0x24 → `0x137430` | |
   | `+0x15b0c` | 600 × 3 bytes, one a section → `0xf74a0` | below |
   | `+0x16214` | u32 object kinds (64) | |
   | `+0x16218` | u32 the most placements (1000; `0x1c630` keeps 2 bytes of state each) | |

   **A road node** (0x24 bytes): `+0..3` four bytes (widths? `+1` / 8 picks a lane width in
   `cars_meet_world`), `+4` the lanes, a nibble a side (high = the left / negative side, low
   = the right), `+5..7` bytes (`+7` picks a row of `0xed284` in `0x4f7d4`),
   `+8` **x, y (up), z** as i32 16.16, `+0x14` u16 (the slope?), **`+0x18` u16 the heading**
   (0x4000 = a full turn; the code takes it × 4 for its 65,536: `car_road_offset`
   `0x51598` reads the dword at `+0x16` >> 16 << 10 >> 8), `+0x1a..0x23` (?).
   Proof of the heading: on all 17 tracks it is atan2(dx, dz) towards the next node within
   1-6 parts in 16,384 (one node of AL1 off by 43).
   **Left and right**: in the original (DOSBox, `nfsdos.py car:`), at the start of a race on
   CY1 the player (right-hand lane on the screen) is at x = +2.5 and the opponent at −2.5,
   both heading +z: +x is the driver's right when the heading is 0, so a picture with x to
   the right and z up is not mirrored.
   **The section bytes**: `[0]` scaled at load by a factor from `0xed12c` / `0xed1d4`
   (picked by two settings, `0x47b58` / `0x47b50`: the difficulty?), read by the
   opponents (`0x7cf98`): their speed; `[1]` **the speed limit** (`police_pull_over`: the
   chase starts when the player is faster than it × 65,536): 27 on the coast and the
   mountains, 33 in the city, 9 / 16 / 18 on the circuits, 2 or 9 or 10 in a few places;
   `[2]` read by the traffic (`0x786d4`, `0x7e64c`), about 0.6 × `[0]`, 5 on most circuits.
2. **"RoadObjects"** (`0x13bb84`): "SJBO", u32 size (with this 12-byte head), u32 0. Then
   64 **kinds** × 16 bytes (read by `0x59490` drawing, `tri_section_objects` `0x4f7d4`):
   `+0` flags (4 = animated: `+8` frames, `+9` steps a frame on the step counter), **`+1`
   the type = the 3D points it takes**: 1 = a 3D model (`+2` = its number among the
   family's models), 4 = one flat picture, 6 = two flat faces at a right angle (the second
   `+8` wide, going back from corner 3); `+2` the picture (`0x141ed8[+2 + variant]`, the
   variant 0..3 from `0xf4604` / `0xf458c` by distance; `/ 4` = the family's entry-1
   `SHPI`), `+3` the second face's picture, `+4` i32 width, `+0xc` i32 height (16.16). Then the **placements**
   (`0x13bb94`) × 16 bytes up to an i32 −1, in node order: `+0` i32 node, `+4` u8 kind,
   `+5` u8 angle (256 a turn, added to the node's heading), `+0xa` 3 × i16 x, y, z from the
   node's position (8.8, the world's axes). Read by `0x4f7d4` and `car_node_in_list`
   (walks them by node). A flat object's corners (`0x4f7d4`): 0 = (x − c, y + height,
   z − s), 1 = (x + c, y + height, z + s), 2 = (x + c, y, z + s), 3 = (x − c, y, z − s), with
   (c, s) = width / 2 × (cos, sin) of −(`+5` / 256 + heading / 16384) turns: at angle 0
   along the road's right, the picture facing the driver. **The picture's right edge goes
   on corner 0**: the other way the original's "MALL" sign on CY1 reads "LLAM" (§3.6). 584..998 placements on the open roads, 138..595 on the circuits.
3. **"TrackSlices"** (`0x136604`), one record a section: "TRKD", u32, u32, 12 bytes
   (copied as they are: the strips' textures?), then **4 rows (one a node) of 11 points ×
   3 × i16** in 1/128 units: point 0 from the node's position, points 1 and 6 from point 0,
   every other point from the one before it (1..5 one side, 6..10 the other). 288 bytes a
   section. `0x4eb00` turns one into 12 + 528 bytes (i32 points) in a ring of 64 at
   `0x1365e8` (0x8700 bytes); `0x4ee30` keeps the 30 sections round the car there.

**In memory** (snapshot of CY1, 2026-10-19): the head is the file's first 0x1621c bytes
unchanged (node 0's `+0x14` / `+0x18` from the head's `+0x18` / `+0x20` are the same values
on CY1; the speed scaling's factor was 1.0); `0x1365e0` = the objects after the 12-byte
"SJBO" head, the placements `0x13bb94` = that + kinds × 16; the stored sections follow the
objects in the file. `tri_keep_sections` `0x4ee30(sec)`: L = `0x13bb8c` (sections round a
circuit, 0x8000 on an open road), the head H `0x1365ec`, ahead A `0x1365f8` = 30, span
T `0x1365f4` = A + 5; fold(d) = L − d if d > L/2 (sic), d + L if d < −L/2. d = fold(H −
sec); e = fold(|sec − (H − A)|); e > T → H = (A + sec − T) & mask, d = A − T. While d < A:
load H, H + 1; while d > A: load H − 1 − T, H − 1 (d folded again each time). A section
goes to the ring slot (sec × 0x21c) % 0x8700. **Ported (`port/src/track.c`: the whole file
in memory, the same ring), equal to the original's ring in 3 snapshots
(`tools/track_emu.py`).**

Units: a node is 6 units from the next; TR1 is 3,066 units round; the speed limits 27 / 33
would be 60 / 74 mph in metres a second. **Probably metres**, not yet proved (the HUD shows
"5.2 MILES" at the start of CY1, 11,514 units long: the race may not run the whole file).
`tools/tri.py info|nodes|png`: the picture (`build/tri/`) shows the road strips (grey the
two inner ones a side, then green, then brown), the centre line, the start, and the
objects (red: pictures, blue: points).

### 3.5 ORIP (3D models) — read (`tools/orip.py`, 2026-10-19)

EA's 3D library (`0x94000`..`0x9a000`, every routine opening with `push n; call __chk`)
takes a model in place: `orip_fix` `0x978a8` turns its offsets into pointers,
`orip_instance` `0x95522` makes a drawable copy (room for the turned points, their screen
places, the sort list), `draw_model` `0x666b0` → `orip_transform` `0x98137`, `orip_sort`
`0x982e1` (by the z of each polygon's first two corners), `orip_draw` `0x98735` →
`orip_draw_list` `0x9868a` → `orip_draw_poly` `0x98482`. Offsets from the "ORIP":

| at | what | proof |
|---|---|---|
| `+4` | u32 size; `+8` 0x2bc; `+0xc` 0 (a use count at run time) | `orip_instance` adds 1 to `+0xc` |
| `+0x10` / `+0x18` | points / their offset: 3 × i32 x, y (up), z (forward), **1/128 units** | NSX 234 × 151 × 562 = the real car's 1.81 × 1.17 × 4.43 m at 7.8 mm a unit |
| `+0x14` | points of the body (ANSX 87 of 103; the rest the wheels?) | |
| `+0x1c` / `+0x20` | texture positions: u32 u, v in pixels (0 on most track models) | `0xab105` reads them through `0x1616cc`, u << 16 |
| `+0x24` / `+0x28` | polygons, 12 bytes: u32 flags, u32 corners, u32 texture positions (both counted in u32s into the index table) | `orip_fix`, `orip_sort`, `orip_draw_poly` |
| `+0x2c` | 12 bytes: the model's name (`_NSX`, `_TR1`..) | |
| `+0x38` / `+0x3c` | textures, 20 bytes: 8 bytes a part's name or 0, the `SHPI` picture's name (4), 8 bytes | names match the `SHPI` beside the model |
| `+0x40` / `+0x44` | named point groups, 20 bytes (`left_tur`, `right_tu`: count 8, first, ..) | |
| `+0x48` / `+0x4c` | named polygon groups, 28 bytes (`NON-SORT`, `inside`, `surface`, `outside`) | |
| `+0x50` | the index table (u32 each) | `orip_fix` |
| `+0x54` / `+0x58`, `+0x5c` / `+0x60` | named points, 12 bytes: 8-char name, u32 point (`smok`, the wheels `FL0`.. `RR1`, the lights `bklr` `bkll` `lt_frnt` ..) | |
| `+0x64` / `+0x68` | a count and a table (0 on the cars) | `orip_fix` |

**The polygon flags** (`orip_draw_poly`): bits 0-2 the corners (3 or 4); `0x80`
textured (else one colour, `0xaa990`); bits 3-6 pick the drawing routine from tables at
`0x1265d8` (triangles) and `0x1265e4` / `0x1265f0` / `0x1265fc` / `0x126608` (quads, by
`0x1000` / `0x2000` and the model's flag); `0x100` or `0x400` drawn from both sides, `0x200`
the other side faces out (`0xaa95b` decides); `0x800` the next polygon is drawn with it;
**`0x1000` the polygon has its own texture positions** (`0xab105` reads `+8`); without it
the whole picture is put on corners 0-3 = top left, top right, bottom right, bottom left
(`map_quad_whole` `0x91b90` gives them (0,0), (w,0), (w,h), (0,h)); bits 16-23 the
texture. Colour 255 is see-through in every palette seen (magenta, 206 28 198), and covers
the open parts of the grandstands. A texture with a part's name and no picture
(`frntires`, `rertires`) is the wheels: the game fills it in (`tyr1`..`tyr4`, turning).
Car tail lights are drawn in colours the second palette (`!xxx`) changes (to check).
`tools/orip.py png` draws a model from eight sides (`build/orip/`): the NSX and TR1's
models look right from all of them.

### 3.6 A track drawn from its files, against the original (2026-10-19)

`tools/trackview.py <NFSSE> <track> <node> <out> [across eye fov]` draws the view along a
track from the TRI (road points, strips, objects) and the FAM (road textures, roadside
pictures, models), textures in perspective. **At the start of CY1 it shows what the
original shows** (`build/tri/compare_cy1.png`: the DOSBox screenshot above, the drawing
below): the road and its lines, the bank and curved barrier on the right, the "MALL" sign
and building on the hill, the lamp posts, the trees and buildings on the left. What that
proves:
- **The road strips.** `draw_road_sections` `0x5aa84` draws from the quad lists at
  `0xf4880` (11 bytes: 4 point numbers, the strip, a drawing record, a flag), one list a
  detail level (`0xf61c0`: 32, 18, 10 quads a section). At full detail the inner strips
  (0-2 and 5-7: between points 0/1, 1/2, 2/3 and 0/6, 6/7, 7/8) go row by row and the outer
  ones (3, 4, 8, 9) over two rows. Strip s's texture is the section header's byte `2 + s`
  (bytes 0, 1: 0 so far).
- **The road textures.** The table `0x140cd8` (144 textures × 8 slots, filled by `0x5b34c`
  from the family's entry 0): texture t = bank t / 3, picture `A`/`B`/`C` by t % 3, slot
  digit 0..7 (`A000` .. `A007`; a missing slot repeats the one before, the families have 0
  = 128 × 256 near and 4 = 32 × 96 far). The picture's width runs along the road over two
  rows (the second drawing record, `[1]` = 0 / 1, picks the half), its height across the
  strip, top = the outer side: CY1's lane picture has its solid line at the top (the edge)
  and dashes at the bottom (the middle of the road).
- **The roadside pictures**: `0x141ed8`, 4 slots a picture, filled from entry 1 (one
  `SHPI` a picture named `0000`, or one `SHPI` of `0000`, `0100` ..).
- **Left and right** a second time: the bank and barrier are on the right in both.
The camera here is a guess (1.3 above the road, 70° wide); the game's own is still to
read, and the horizon (`horz`, entry 2) and the sky are not drawn yet.

### 3.7 FFN (fonts) — read (`tools/ffn.py`, 2026-10-19)

Read by `setfont` `0x91ed0`. "FNTF" (setfont also takes `FNT3` `FNTP` `FNTS` `FNTU`),
u32 size, u16 100, u16 characters (224), u16 `+0xc`, u16 `+0xe` (`& 3 == 2` picks the other
drawer), bytes `+0x10`..`+0x13` (`+0x12` + `+0x13` = the line's height), u32 `+0x14` the
character table, u32 `+0x1c` the bitmap. A character, 11 bytes: u16 code, u8 width, u8
height, u16 x, u16 y (in the bitmap), u8 advance, i8 x offset, i8 y offset (from the line's
top). The bitmap is an FSH shape (16-byte header, type `0x7a` = 4 bits a pixel, high nibble
first, rows of (4 × width + 7) / 8 bytes); a pixel is a level 0..15 (the edges' smoothing),
mixed with the text's colour when drawn. Checked by drawing a line of text with each font
(`build/ffn/`).

### 3.8 EA sound: EAS, ASF / AS4, BNK — read (`tools/eacs.py`, 2026-10-19)

One header, **`EACS`**, everywhere (read by `snd_stream_header` `0x8c8f3`, `snd_play_eacs`
`0xa1f11`, `0x95b4e`, `0xad887`): +4 u32 samples a second (16000, 16247, 22050), +8 u8 bytes
a sample, +9 u8 channels, +0xa u8 compression (0 PCM, 2 IMA ADPCM), +0xb i8 the voice a
stream plays on (0..15, records of 0x28 at `0x160fa4`; -1 in EAS / BNK), +0xc u32 samples,
+0x10 i32 loop start (-1 none), +0x14 u32 loop length, +0x18 u32 where the data starts.
- **`.EAS`** (193: speech, `SPEECH/`, `GSPEECH/`): the header at 0, the data at 0x20. 170 PCM
  (8-bit signed: the mixer's volume table `0xa06a5` maps byte 0x80 to -128), 23 IMA.
- **`.BNK`** (38: engines, collisions, sirens): 128 u32 offsets (0 = none) of 0x48-byte
  records; a record's +0x28 is the header, its +4 the header's file position, its data
  offset a file position too; +0, +0xc, +0x14..+0x27 (volumes, pitch?) still to read.
  All PCM.
- **`.ASF` / `.AS4`** (3 PCM 16 kHz, 12 IMA 22 kHz music): blocks of u32 tag + u32 size
  (the 8 included), handled by `snd_stream_block` `0x8cbf5` and queued for the mixer
  (`0x8c9fe`, the list at `0x15ece4`/`0x15ece8`): `1SNh` (the header at +8, data from +0x28),
  `1SNd` data, `1SNl` at the loop's end (u32: the file position of the block the loop starts
  with: checked on all 14, every loop starts on a block), `1SNe` the end (CREDITS.ASF has
  1.7 MB after it that is not part of the sound). The headers' sample counts equal the
  blocks' sums.
- **The IMA ADPCM** (`snd_voice_start` `0x9ff4c` sets the voice's mode 2 and its decoder
  `[voice+0x58]`: `ima_decode_mono` `0xadfdc` or `ima_decode_stereo` `0xae084`): standard
  IMA, the tables pre-multiplied in the data (`0x12669c`: 89 steps × 16 codes of ready
  differences, computed bit by bit: step/8 + step·b2 + step/2·b1 + step/4·b0; `0x127cdc`:
  the 8 index changes ×64; the index is kept ×64, clamped to 0..0x1600). That is why a search
  for the step table found nothing. High nibble first; stereo is one byte a sample, high
  nibble the first channel. A stream's data block (and the 1SNh's data) starts with 0x14
  bytes: u32 samples, u32 step index channel 0, 1, u32 value channel 0, 1 (the voice packs
  them as `+0x30` = index0·64 | index1·64 << 16, `+0x34` = value0 | value1 << 16). An IMA
  `.EAS` starts from 0, 0. **Checked against ffmpeg's EA decoder** (`adpcm_ima_ea_eacs`):
  with its rounding ((2n+1)·step/8) every sample of ROCK1.AS4 is equal; the tool keeps the
  game's bit-by-bit rounding (differences up to 72 of 32768).
- Not sound: `0x8cd60`, `0x8cdf0`, `0x8cf00` are the TGV video's (§3.14).

### 3.9 EA's packers: 10FB, 30FB/32FB/34FB, 46FB — read (`tools/unpack.py`, 2026-10-19)

`unpack` `0xa7020` (src, dst, complain; returns the size; `unpack_size` `0xa711c`) picks by
the first two bytes, `& 0xfe` on the first (an odd first byte = a header 3 bytes longer):
`10FB` RefPack (`0xaf2c8`; 473 `.QFS`), `30FB`/`32FB`/`34FB` Huffman (`0xaf440`; the `.PBS`,
`.PDN`, 15 `.QFS`), `46FB` byte pairs (`0xb0674`; 8 `.QFS`), `4AFB` (`0xb0890`; no file uses
it). **All 527 packed files on the CD unpack to the byte as the game's own routine does**:
`tools/unpack_emu.py` runs `0xa7020` itself in Unicorn (`build/venv`) and the two are
compared file by file. Before this, `fsh.py` could not open 23 menu pictures (the German
track records, the rally maps, the police screens); it now uses `unpack.py`.
- **Huffman** (`0xaf440`, bits read from the top of each byte): 16 bits the kind (bit 8:
  24 more bits to skip), 24 bits the size, 8 bits the escape byte. A "number": `1xx` = 4..7,
  or z zeros, a one, z + 2 bits b = 2^(z+2) + b. Per code length 1, 2, ..: a number − 4 =
  the codes of that length (canonical), until the code space is full. The symbols in code
  order: each a number − 3 = how many unused byte values to step on from the last (from
  255, wrapping). Then codes: a symbol that is not the escape is a byte; the escape is
  followed by a number − 4 = n: n > 0 repeats the last byte n times, n = 0 then 1 bit: 1 the
  end, 0 then 8 bits a byte as it is. `32FB` then sums the bytes (deltas), `34FB` twice.
  (The routine decodes codes up to 8 bits through two 256-byte tables, longer ones through
  left-aligned 16-bit limits.)
- **Pairs** (`0xb0674`, `47FB` has 3 bytes more): 24-bit size, the escape byte, the number
  of pairs, then (code, first, second) each; a code stands for its two bytes, each maybe a
  code again (`0xb0630`, recursive). A byte that is no code is itself; escape + 0 ends,
  escape + b is b.

### 3.10 Car physics: PBS and PDN (`tools/carspec.py`; every field the game reads, 2026-10-21)

Both Huffman-packed, all i32, mostly 16.16. Read first from the files (2026-10-19), then
field by field from the code that uses them as the driving was ported (`port/src/drive.c`,
equal to the original step for step, NOTES §4.6). `build/cars/torque.png`.
- **`.PBS`** (CARSPECS: the 8 player cars and TRAFFC; 1912 bytes), `load_car_pbs` `0x50c24`:
  used in place (the car's `+0x479`). **A checksum**: the sum of bytes 0..0x757 = u32
  `+0x774`, otherwise `+0x3c` is set to 0 (an edited file gives a car without power).
  Every field the game reads (names from what the code does with them):
  - mass: `+0x0`, `+0x4` the front and rear shares, × `+0x14` the mass; `+0x34` a
    resistance (× the mass at loading); `+0x24` × `+0x44` the weight thrown forward and
    back; `+0x40` the wheelbase (the turn's and the yaw's damping, `0x324` · it²).
  - the engine: `+0x3c` a power factor (0.5..0.84; ×248/256 with the automatic gearbox,
    `0x18990`; TRAFFC 2.0); the torque curve `+0x88` points from `+0xb4` rpm every 200 rpm,
    `+0xb8 + 8i` each (in N·m it seems: the NSX peaks at 286 at 5600), made 16.16 at loading
    with `+0x5c`, `+0x64`, `+0x14`; `+0xac` the red line, `+0xb0` the idle; the revs' rise
    `+0x35c`, fall `+0x360`, rise in neutral `+0x364` (× the gear's `0xec270`), drop on
    letting go `+0x358`; `+0x368` how hard the engine and the wheels pull each other's revs.
  - the gearbox: `+0x58` the gears + 3, `+0x54` the final drive, `+0x70 + 4g` each gear's
    ratio, `+0x2b8 + 4g` each gear's force factor, the automatic's shift points down
    `+0x290 + 4g` and up `+0x294 + 4g`, `+0x298` the revs for a spinning start, `+0x354` a
    shift's time in steps.
  - the drive and brakes: `+0x18` the front's share of the drive (0 = rear-wheel drive),
    `+0x1c` the front's share of the braking, `+0x28` / `+0x2c` the front's and rear's limits,
    `+0x38` the top speed (not for traffic), `+0x33c` / `+0x340` how fast the throttle opens
    and closes; `+0x334` ABS, `+0x338` traction control (1.0 = the car has it: the NSX, ZR-1,
    Supra both, 512TR, RX-7, 911 ABS only, Viper and Diablo neither; on only with the
    player's setting, `0x12da5f` / `0x12da5b` → car `+0x492` / `+0x48e`).
  - the tyres: `+0x2dc` / `+0x2e0` the front's and rear's grip, `+0x31c` the grip across's
    limit, `+0x320` / `+0x328` with their inverses `+0x324` / `+0x32c` the engine's and the
    brakes' strength, `+0xa4` the slip's limit, and **the grip by slip angle, 512 bytes an
    axle: `+0x374` the front, `+0x574` the rear**.
  - the steering's speed: `+0x2ec`, `+0x300`, `+0x30c`, `+0x310` (less with speed),
    `+0x314` / `+0x318` its shifts.
  - the body: `+0x2d4` / `+0x2d8` how far it leans and nods; `+0x2e4`, `+0x2e8` → car
    `+0x78`, `+0x7c`; `+0x36c` the driver's eyes' height, `+0x370` the horizon in the view.
  - **The rally (EARALLYE, `0x12dd2c`)**: the loader copies the front's grip curve over the
    rear's, halves `+0x320`, `+0x328` (their inverses again), `+0x2dc`, `+0x2e0`, `+0x28`,
    `+0x2c`, makes `+0x24` 1.5 times, and `+0x3c` × 0x14c/256 (`0x12da12` = 3) or 0x133/256.
  (`By_R&T`, a 4-byte file beside them, is opened for writing in a debug case: not needed.)
- **`.PDN`** (CARFAMS: all 22 cars; 460 bytes), loaded in `load_car_cfm` `0x65874` into
  `0x1493a4[car]`, applied by `car_apply_pdn` `0x1e9cc` (`crash.c`, `opponent.c`): `+0x0`,
  `+0x4`, `+0x8` the car's size (half its width 2 · `+0x0`, half its length 2 · `+0x8`;
  `car_setup_body` `0x512e8`, §4.6); `+0xc`, `+0x10` the inertia and
  mass of the body that tumbles (`0x2c920`, `+0xc0`); `+0x14` → car `+0x168` and 1/x `+0x16c`
  (10, 15, 16; 255 for traffic): how much a bend slows a computer's car; `+0x18` → `+0x170`:
  the computer's top speed on its driver's plan (× the driver's record `+0x55`); 93 values from
  `+0x1c` → car `+0x178..`: the speed a computer's car gains a step at each speed; six from
  `+0x1ac` → `+0x308..` with 1/x at `+0x320..`: the gears' top speeds in m/s (NSX 20, 35.6,
  50.2, 63.5, 74.7 = 269 km/h; Diablo 323, 512TR 307: the real cars'), for a computer's
  revs; `+0x1c4` the rev limit (NSX 8000) → `+0x338`; `+0x1c8` → `+0x469` the horn's sound.
  Traffic cars have 0 in the head, gears and rev limit (sized from their model instead).

### 3.11 Track INFO (sky and horizon) — read (`tools/trackinfo.py`, 2026-10-19)

`SIMDATA/MISC/<track>INFO`, text with its own labels, read by `load_track_info` `0x49dc0`:
a line at a time (`0x38c50`: lines starting `#` or empty skipped), one value a line. Eight
decimals into 16.16 (`0x41b68`): vert offset near/far → `0x12ef44`/`48`, zoffset near/far →
`4c`/`50`, height near/far → `58`/`60`, radius near/far → `54`/`5c`; colour top and bottom,
three numbers on a line packed a<<10 | b<<5 | c → `0x12ef64`/`68` (all tracks: 0 1 2 and
3 4 5); then `atoi`: ring type (1 on CL3, AL3) → `6c`, ring y offset ("bigger places the
horizon further down", -60 / -15 / -63) → `70`, ring height (112) → `74`; "RGB TIME OF DAY
VALUES": three groups of five → `0x12ef78 + 4g + 12k` (the fifth ×256; CY1: 400 202 80 2572
216 / 255 255 255 0 255 / 296 52 52 0 255: colours above 255 and an angle?, no direct reader
found), and one last value → `0x12efb4` (1 on CL3, AL3). Users: `load_horizon`, `0x4a9d0`,
`0x4ace8`, `race_menu_options`. CY3INFO has a stray "o" line that `atoi` reads as 0.
**The files win**: the flag `0xec5d8` is 1; only when a file is missing (or `0x12c340` = 1)
does the game take the built-in table (33 values a track from `0xec5dc`, 0x84 apart, picked
by the names cl1 .. tr1), which holds older values than the files.

### 3.12 INV (the menus' inverse palettes) — read (`tools/inv.py`, 2026-10-19)

32768 bytes beside a menu picture, loaded with it (e.g. the audio screen `0x10853`:
`option/audio.fsh`, its `!pal` through `pal_from_shape` `0x718a4`, `option/audio.inv` into
`0xf77c4`). Read by EA's `nearestcolour` `0x80c80` in 8-bit modes: a colour `0xRRGGBB` →
the palette entry at `(R>>3)<<10 | (G>>3)<<5 | B>>3`; with no table it searches the palette
at `0x1607a4` itself (only the entries flagged at `0xf77c8`); in 15/16/24/32-bit modes it
packs the colour instead. 30 routines call it (menus, fonts, the 3D library).
**36 of the 51 tables are their screen's nearest-colour table** (90–93 % the nearest entry,
the rest ties); **15 fit no palette on the CD** (all the installer's `IART` screens, `AUDIO`,
`OPTIONS`, `MODEMSET`, `FINSTND`, and the bonus tracks' `BRUN`/`TROP` screens that share
City's tables): made for older art, so the original picks odd colours through them. The port
reads the files as they are, and that stays faithful.

### 3.13 BAMF (what of the 3D view reaches the screen) — read (`tools/bamf.py`, 2026-10-19)

EA's blit lists, copied by `0x8b8f0` (the off-screen view `0xf4578`, the list) by VESA
64 KB bank (`0x9d7f8` sets the bank): "BAMF", u32 size, u16 banks, u16 the longest span,
u32 `+0xc` and `+0x10` (0x10000, or 0x8000 = the source read at half the rate across /
down: pixels doubled), then per bank a u16 count of spans, each u32 offset in the source,
u16 offset in the bank, u16 length. Every file's spans end exactly at its size.
- **The HUD** (`load_cockpit_mask` `0x5f7c8`, drawn by `0x30b64`): `MASKHI.B<s><n>` /
  `MASKLO..` into `0x145a58`, and `MASKHI.MMB` (a `wwww` of 12) into `0x145a24`; s = the
  view's size (full screen, medium, small window), n = how it is copied (1:1; doubled
  across; doubled both ways; doubled with every other line: the detail settings). The
  spans leave out the status bar on top, the tachometer bottom left and (the `.B` set
  only) the map's box bottom right; `0x12b988` picks the set. `MASKHI.FSH` / `MASKLO.FSH`
  (`MASKHIG` German) hold the HUD's pieces (digits, bars, the tachometer, labels).
- **The dashboards** (`SIMDATA/DASH/<car>DH.FMM`, `DL.FMM`: a `wwww` of 4, the same four
  ways of copying): the windscreen and side window, A-pillar and mirror cut out
  (`build/bamf/ansxdh.png`); the dashboard picture is `<car>DH.FSH`.

### 3.14 TGV (the movies) — read (`tools/tgv.py`, 2026-10-19)

The 26 `FRONTEND/MOVIE/*.TGV`: the EA logo, the title, three attract loops, a showcase a
car, three police clips (and German `GCOP`), seven winners' clips. Played by `movie_play`
`0x34ba0` → `movie_loop` `0x346dc` (from `fe_attract_mode` `0x34284`, `fe_winner_movie`,
`fe_cop_ticket_movie`); a frame decoded by `tgv_frame` `0x8d4ac`. All 320x240, 15 frames a
second, the sound an EA stream (16 kHz 8-bit stereo PCM in all checked).
- Blocks of u32 tag + u32 size (the 8 included): `1SNh`/`1SNd` the sound (§3.8), `kVGT` a
  key frame, `fVGT` a delta frame.
- **`kVGT`** (`tgv_key` `0x8d190`): `+8` u16 width, `+0xa` height, `+0xc` u16 (0 in all),
  `+0xe` u16 colours (256), `+0x10` u32 flags (bit 0 = the picture unpacked; 0 in all
  1,236), `+0x14` the palette, 3 bytes a colour, **8-bit** (values to 255), copied as it is
  to the decoder's `+0x44`; then the picture, RefPack (`10FB`, §3.9). A new size frees and
  allocates the two picture buffers (`+0x28` current, `+0x2c` previous).
- **`fVGT`** (`tgv_delta` `0x8cf00`): swaps the two buffers, then `+8` u16 vectors, `+0xa`
  raw blocks, `+0xc` packed blocks, `+0xe` bits an index. From `+0x14`: the vectors (x, y:
  pairs of 10-bit signed fields, least significant bit first, `bits_signed` `0x8cd60`; the
  run rounded up to 32 bits); the raw 4x4 blocks (16 bytes, row by row); the packed blocks
  (8 bytes: 4 colours, then a u32 LE of sixteen 2-bit codes, the top two bits first,
  `tgv_packed_blocks` `0x8cdf0`); then one index a 4x4 block of the picture
  (`bits_unsigned` `0xa6ec8`, least significant bit first). `tgv_build_frame` `0xa6f6c`,
  left to right, top to bottom: an index below the vectors' number copies the 4x4 at
  (this place + that vector) from the previous picture (a row table makes dy·width);
  any other is block (index − vectors) of the raw blocks followed by the packed ones.
- **Checked against ffmpeg's `tgv` decoder**: all 26 movies, 11,833 frames, equal pixel for
  pixel; TITLE's sound equal sample for sample.

### 3.15 RPL (replays) — read (`tools/rpl.py`, 2026-10-19)

A replay is not a film: it is the race's settings, every driver's controls and snapshots of
every car, from which the race is driven again. 100,374 bytes, five blocks of memory
written by `replay_file` `0x5c3b0` (`fwrite`, `<path>Replay.rpl`, when `0x12dd5c` is set and
the mode `0x15d136` is not 1) and read back in the same order: `replay_settings` `0x5c340`
(from `race_run`: the first block only, from the name at `0x145558`; no file → `0x14554c`
= 6, `0x13c17c` = 0x20) and `replay_load` `0x5c4a4` (all of it, keeping the player's own
three settings in its entry). The attract mode (`fe_attract_mode` `0x34284`) puts
`REPLAY%d.RPL` in `0x156c4f`, sets `0x156cc3` = 3 and runs a race.
- `0x00000` 0x348 bytes: **the race record `0x12da0c`** (filled by `race_setup_record`
  `0x36c5c`): `+2` (0 in all), `+6` the track (`0x41b68`'s order: city coast alpine
  springs avalley bsienna vridge tpolis lvegas moon), `+0xa` its files' stem (`al1`,
  `tr6`, ..), `+0x14` 1 / 2 (head to head / a race of 8?), `+0x18` network, `+0x1c` flags
  (`0x15d12a`), `+0x20` racers, `+0x24` drivers (people), `+0x28` (1 with traffic), `+0x2c`
  cars in all, `+0x30` traffic cars, `+0x38` **the time** (seconds from 1970: the CD's were
  made 15–16 June 1996), from `+0x3c` two 0x4b-byte entries (the player, the head-to-head
  rival: `+2` the name, `+0xb` the car in `0xf6e40`'s order: TSupra LDiabl P911 CZR1
  F512TR DVIPER ANSX MRX7 .. Traffc, the police and traffic cars; `+0x13` / `+0x17` the
  two driving aids), .. `+0x300` the clock setting `0x12dd0c`, `+0x310` the player
  `0x12dd1c`.
- `0x00348` 0x9a4 bytes from `0x144ba8` (the highlights `0x144da8`, §4.8, among them).
- `0x00cec` 0x15e00 bytes, **the recording** (`UsrCtrlBuff`, allocated by `replay_start`
  `0x5dd68` and **never cleared**: what the race did not reach is whatever the memory held,
  so a reader must stop at the end below). `replay_record_step` `0x5d888` runs on every
  second step (flag 4, §4.3); `0x144ba4` = step counter `0x145c94` mod 28,800:
  - `+0`: the drivers' control words, one slot a call (`replay_record_controls` `0x5cac4`,
    played back by `replay_play_controls` `0x5cb58`): driver k's low bytes at k·7200 + t,
    its high bytes at 14,400 + k·7200 + t, t = `0x144ba4` / 4. Two drivers at most; a
    ring of 7,200 slots = 14,400 steps = 7.2 minutes. During the first 120 counts every
    word is `0x7a0` (no steering, no pedals).
  - `+0x7080`: every 450 calls (900 steps, 27 s; `0x143ee8` counts down) a snapshot
    s = `0x144ba4` / 1800 of 16: each driver's car, 0x1a4 bytes at `+0x7080 + s·0x1a4 +
    k·0x1a40` (`replay_pack_car` `0x5cbd0` + `replay_record_car` `0x5d0e4`; back by
    `replay_restore_car` `0x5ceb0`);
  - `+0xa500`: the other cars at the same moment, 0x172 bytes at `+0xa500 + s·0xb90 +
    j·0x172` (8 at most; the count at `0x143f3c + s·0xa2`).
  - A car's snapshot: `+0` / `+4` car `+0x50e` / `+0x512`, `+8` its index (`+0x516`) << 24
    | `+0x51a` << 8, `+0xc..` car `+0..+0x20` (the position, the angles), `+0x54` car
    `+0x48` (the road node), .. The positions lie on their road nodes (checked on all four).
- `0x16aec` 0xcc8 bytes from `0x143ed8`: `+4` (`0x143edc`) **the step counter at the last
  step recorded** (2 a step: REPLAY.RPL 11,632 = 5,816 steps = 2:54.5), from `+0x14` 16 ×
  0xa2 bytes, what each snapshot needs besides the cars (the cars' order `0x13c298` /
  `0x13c274`, the random seed `0xeab70`, ..).
- `0x177b4` 9 × 0x1d2 bytes from **`0x14ea80`**: the racers' results (§4.3), whose records
  start at `0x14ea7c`: the game's copy is 4 bytes late (each record's first 4 bytes are
  missing, the next one's follow). REPLAY.RPL: the Diablo finishes at 11,198 (+300 =
  2:52.5).
- **The control word** (`controls_read` `0x49248` builds it from the keys or joystick;
  `car_steer` `0x483e4` / `car_steer_2` `0x4853c` and `car_pedals` `0x48990` /
  `car_pedals_2` `0x48a80` use it, chosen by the entry's `0x12d974` / `0x12d978`): bits
  0–5 the steering (32 = straight; 1 to 15 either way a slope, beyond it full lock),
  6–11 the pedals (30 = neither; below 15 `car_throttle` `0x48600` ramps `+0x3b1` to 255,
  45 and above `car_brake` `0x48658` the brake `+0x3b9`), 12 a gear down, 13 a gear up
  (`car_shift` `0x48714`), 14 car `+0x4ba`, 15 car `+0x3f5`.
- The CD's four: REPLAY alpine (al1) Diablo against a 911 with traffic, 2:54; REPLAY1
  Burnt Sienna (tr6) a 512TR in a race of 8, 1:53; REPLAY2 Transtropolis (tr7) a Diablo in
  a race of 8, 2:03; REPLAY3 coast (cl1) Viper against a Supra with traffic, 1:44. All
  one driver at the keyboard (`build/rpl/*.png`).

### 3.16 CONFIG.DAT (the front end's state, the records) — read (`tools/config.py`, 2026-10-19)

`GAMEDATA/CONFIG/CONFIG.DAT`, exactly 24,402 bytes (0x5f52), is copied as it is to
`0x156c14` by `cfg_load_config_dat` `0x39c40` (another size or no file: `cfg_defaults`
`0x38ea8` builds it) and written back by `cfg_save_config_dat` `0x3a134`. Offsets from
`0x156c14`:
- `+0` the player's name ("Player" / "Spieler"); `+0x3b` (`0x156c4f`) the replay's name;
  from `+0x6b` the menus' choices as words, read by `race_setup_record` `0x36c5c`: `+0x93`
  the segment (→ `0x12da0e`), `+0x95` the track (→ `0x12da12`), `+0xb3` the race length
  (which table of race times), the car, the opponents, the detail settings (`0x156c93`..
  set from a speed test `0x74ca0` in the defaults), ..; `+0x5f36` (`0x15cb4a`..) the keys.
- From `+0xb5`, **the records**: 9 tracks (`0x41b68`'s order) × 0xa6b bytes, 0x27 bytes a
  record: `+0` the name (15), `+0xf` the car (the menu's order, `0xf7464`: RX-7 NSX SUPRA
  911 ZR-1 VIPER 512TR DIABLO), `+0x13` 0/1 (?), `+0x17` 1 = filled, `+0x1b` the time in
  **1/60 s** (`fmt_time` `0x74914`: m:ss.t; 99,999 / 999,999 = "N/A"), `+0x1f` a speed
  (16.16 m/s; `fmt_speed` `0x74854`: × 2.24 "mph", German × 3.6 + 1 km/h; below
  0x9fff6 "N/A"), `+0x23` the kind of race (0 time trial "t.t." / German "z", 1 head to
  head "h.h." / "d", 2 a race: the track select screen `0x75288`).
  - records 0–2: city, coast, alpine: the best time of segments 1–3 (`records_segment`
    `0x69608`, from the result's `0x15cbc1`, its top speed `0x15cbc9`); a circuit: the best
    lap in record 0 (`records_best_lap` `0x69438`, from the 20 lap times at `0x15cb71`);
    records 3–5: the best top speed of segments 1–3.
  - records 6–35: three tables of ten race times (`+0xb3` picks one; on a circuit each
    length twice the one before: Rusty Springs 2:43, 5:23, 10:34).
  - record 36 (`+0x57c`): the track's top speed.
  The players' results they come from: 0x95 bytes a player from `0x15cb68` (`+0` the name,
  `+9` 20 lap times, `+0x59` the time, `+0x61` the top speed, `+0x6d` ..).
- **The CD's copy is not the built-in defaults**: `cfg_defaults` takes its records from
  text tables (`0xeab80` .., six strings a record: name, time, car, speed, `+0x13`, kind);
  the first five race times on City agree, the slower five are faster on the CD (Slayer
  7:54.5 against 8:09.5 built in, ..). The CD's city segment times add up to its best
  race time (7,332 + 7,596 + 11,706 = 26,634). **Danik decided (2026-10-19): a first start
  uses the built-in defaults**, as an installed original does (it never reads the CD's).
- **`cfg_defaults` `0x38ea8`** (ported: `config.c`, all 24,402 bytes equal to the
  original's in English and German, `tools/config_emu.py`): the name "Player" / "Spieler"
  (`0x15d140`); words +0x6d 1 (head to head), +0x6f 0, +0x71 0, +0x7d 1, +0x73 1, +0x77
  100, +0x75 100, +0x7b 1, +0x79 0x7f, +0x8d 1, +0x95 0 (City), +0x99 0, +0xaf 0, +0x8b 0,
  +0x8f 2, +0x93 0, +0x97 0, +0xb3 1, +0xb1 4, +0x91 7 (Diablo), +0x9b 6 (512TR). Then
  `0x8ee80` (the CPU check: `0xf7924` a 486, `0xf7928` a Pentium) and **the speed test
  `0x74ca0`** sets the detail: below 300 all 0; below 0x1a4 +0x83 1, +0x85 1; below 600
  +0x83 2, +0x85 2, +0x81 1; from 600 **+0x7f 1 = races at 320x200** (the graphics
  screen's `640l`/`320l` by +0x7f; `0x1af78` sets 640x480 again after a replay only when
  +0x7f ≠ 0; an earlier reading here had it the wrong way round) with +0x85 0, 1, 2 and
  +0x81 0, 0, 1 below 0x2d0, 900 and
  above. +0x87, +0x89 0. The detail words are the graphics screen's (`fe_graphics_options`
  `0x32d08`: `win` `ima` `van` `car` `hor`). The test's number is the frames drawn in a
  given time (more: faster), yet 0 is the most detail in every word: +0x83 → `0x12d9d9`
  (the view's pixels 1:1, doubled across, doubled both ways), +0x85 → `0x12d9d5` (the road
  drawn 28, 18 or 13 sections ahead at the start, 28, 14, 9 by F3), +0x81 → `0x12d9d4`
  (the outside view's size), +0x89 → `0x12d9d6` (the sky's picture off) (§4.19). **The
  port answers below 300** (all 0: 320x200, the most detail), as the snapshots' installed
  original has them (an earlier answer, 0x1a4..599, was read the wrong way round: it gave
  the least detail). Races stay 320x200 for now, Danik's choice for the MiSTer.
  The records from text tables of six strings (name, time, car, speed, +0x13, kind; each
  number by `atoi` `0x89903`; +0x17 = 1): each track's record 36 from `0xeab80` + 0x18·t;
  City, Coast, Alpine records 6..15 from `0xeae98`, `0xeaf88`, `0xeb078` and 0..5 from
  `0xeac58`, `0xeace8`, `0xead78`; the circuits (tracks 3..8) records 6..35 from `0xeb168`,
  `0xeb438`, `0xeb9d8`, `0xeb708`, `0xebca8`, `0xebf78` (0xf0 a table of ten) and record 0
  from `0xeae08` + 0x18·t. The three arrays of 100 words from +0x5bb a track cleared; the
  open roads' six numbers at +0x5a3 a track written as constants. The keys from +0x5f36
  (0, 0, 0, 0x4b00, 0x4d00, 0x4800, 0x5000, 0x41, 0x5a, 0x20, 0x43, 0x48, 0x4d; German
  0x59 and 0x4b), the modem's "Custom" +0x5e78, "ATZ" +0x5eaa, "ATD" +0x5ee6, +0x5ea0 9,
  +0x5ea2 0, +0x5ea4 4, +0x5ea6 1, +0x5f50 0. Nothing else: the rest stays as the BSS
  leaves it (0). `cfg_load_config_dat` returns 1 when it made them (the front end then
  goes to the name), and after either corrects joystick keys (+0x5f36.. between 1 and 3,
  with `0x12f1d8` = 0: back to kind 0 and the arrows / A, Z; ported, `config.c`).

### 3.4 Still to read

Every file type on the CD has a reader, and every field the game reads is used by the
port and checked: the PBS/PDN (§3.10, the driving equal step for step), the replay's
block (§4.22, the CD's four replays equal at every snapshot), CONFIG.DAT (`config.c`, the
defaults equal byte for byte). Nothing open.

## 4. Map of the program (2026-10-19)

Tools: `tools/strmap.py` (the strings each function uses; the full lists
`build/strs_by_site.txt`, `build/strs_by_func.txt`), `tools/calltree.py`. Names are in
`tools/names.txt` (prefixes: `fe_` front end, `race_`, `load_`, `snd_`, `cfg_`, `modem_` /
`net_`, `hud_`). The evidence for a name is the files and texts the routine uses (listed
here) unless said otherwise. Layout of the code object: game `0x10000`..`0x80000`, EA's
libraries `0x80000`..`0x94000` and `0x95000`..`0xb0000`, Watcom's runtime `0x94000`.. and
`0xa9000`.. (mixed; not separated yet).

**What is left unnamed (counted 2026-10-21)**: of the game's 810 routines (`0x10000` ..
`0x80000`, 428 KB), 142 (22.6 KB, 5 %) are neither in `tools/names.txt` nor named by address
in these notes or the port; with EA's network library (`0x80000` .. `0x88000`, 108 of 191)
and the runtime the rest. Gone through one by one (their callers, callees and texts): the
network race, its modem and serial screens, phone book and chat (`0x1f168` .. `0x20d04`,
`0x54c80` .. `0x5575c`, `0x602b0` .. `0x60e5c`, `0x70d70`, `0x714b4`, the race's sync
`0x37000`, `0x496e8` .., `0x51498`), the I-Force driver's serial side and the serial port
(`0x4b3bc` .. `0x4c2dc`, `0x60930` .. `0x60bf4`: the port plays its commands on SDL, §4.26),
`joyopt.cfg`'s remapping (`0x4dcc0`, `0x4de9c`, §4.23), the debug `.vis` recorder (`0x35118` ..
`0x351c0` → `0x58870`), routines nothing calls (`0x5985c` under `0x5a0a8`, whose switch
`0x58c88` has no caller; EA's filler's mode 1, §4.10), and small helpers inside routines
ported (memory freed at a race's end, the INFO file's line readers `0x38c40` .., a file's
writing `0x74ef4`, empty routines of one byte). None is a part of the game the port lacks.

### 4.1 Start-up and the outer loop

- `main` `0x4716c`: `0x89897` (memset-like, `0x12dd30`), `0x94010`, copies "NFS SE"
  (`0xe2a1c`), `startup` `0x42054`, `addtimer(race_timer_tick 0x4c454)`, `0x1f07c`,
  `fe_outer_loop` `0x47000`, `0x4d6c8`, `0x80f90(0x4c454)` (removes the timer), exit
  through the pointer at `0xf8a84`.
- `startup` `0x42054`: `cfg_read_nfs_cfg` `0x41d74` (`nfs.cfg`: SBPRO SB16 GUS AWE WSS,
  YESSOUND NOSOUND, NOVIDEO LOWVIDEO, GERMAN, NOREMOTE), the XMS / Windows 256-colour /
  "NFS SE CD not present..." errors, `cfg_init_paths` `0x39db8` (`gamedata\config\paths.dat`
  or the built-in list `gamedata/config/` .. `simdata/ntrackFm/`, English and G-versions),
  `initgraphics`, `fonts/graver18.ffn`, `%stitle.tgv`, the stream speed (`setstreamspeed`).
  `0x93090` (called from it) sets the PC timer: `inittimer` `0x81078`, divisor
  `0x1234f0 / TIMERHZ`; **TIMERHZ = 100** (`0xf7940`, initialised data).
- `fe_outer_loop` `0x47000`: `cfg_load_config_dat` `0x39c40`, `fe_title_screen` `0x46f20`
  (`title.qfs`), `fe_main_menu` `0x42c30`, `cfg_save_config_dat` `0x3a134`, `0x550a0`
  (network shutdown?), `fe_attract_mode` `0x34284` (`%sea.tgv`, `%sattract%d.tgv`,
  `%stitle.tgv`, `eatitle.qfs`, and a race replay of `gamedata\replay\REPLAY%d.RPL`),
  `fe_game_mode_screen` `0x47274` (`control/gamemod2.qfs`: single / modem / exit),
  "Exit to system ?", "No network services found.".

### 4.2 The front end

- `fe_main_menu` `0x42c30` (17 KB, `control/central.qfs`, `control/ani.qfs`,
  `control/tourny1..3`, `showcase.asf`, "Quit tournament ?", "Track already completed"):
  the central screen and its state machine. It calls the race (`race_from_menu`), the
  options, the showcase, the chat, the track selection, the standings.
- Screens (one routine each): `fe_audio_options` `0x105dc` (`option/audio.fsh`; "Cannot
  change audio mode in Multi-player modes."), `fe_graphics_options` `0x32d08`
  (`option/graphics.qfs`, `check/graphics.fsh` = the same screen during a race),
  `fe_options_screen` `0x53a30` (`option/options.qfs`, `credits.asf`), `fe_controls_setup`
  `0x22bd0` (`option/cntrlset.qfs`, `check/chconset.qfs`), `fe_joystick_calibrate`
  `0x4c6b0` (`joychk.qfs`, `joycal.qfs`), `fe_credits` `0x25600` (`credits.fsh`, `bgn%d`),
  `fe_showcase` `0x17724` (`%s.tgv`, `showcase.asf`, pages vid / his / gen / per / eng)
  with `fe_showcase_specs` `0x15008` (`mec`) and `fe_showcase_history` `0x16290` (`his`,
  `gen`, `f1s%d` / `f2s%d`), `fe_chat_screen` `0x195d8` (`control/chat.qfs`),
  `fe_track_select` `0x75288` and `fe_track_select_tournament` `0x76380`
  (`tracksel/%s.qfs`, the lengths "16.0 miles" .. and the music names `city.eas` ..
  `loopx.eas`), the load / save screens `0x3aacc` `0x3c0c8` `0x3d4c4` (`*.SAV`, `*.RPL`,
  "Load replay ?", the four system replays REPLAY.RPL, REPLAY1-3.RPL may not be deleted:
  `is_system_replay` `0x3a9c4`), `fe_yes_no_dialog` `0x73b04`, `fe_enter_name` `0x743b4`,
  the standings `0x69f8c` .. `0x6f070` (`stats/standbst` / `standseg` / `standtrk` /
  `standlo1` / `standlo2` / `sgmtstnd` / `finstnd` / `trmtstnd`, the commentary
  `con<n>_<x>16.eas`), `fe_winner_movie` `0x37ff0` (`win#%d.tgv`, `winner.qfs`),
  `fe_cop_ticket_movie` `0x3823c` (`cop%d.tgv`, `ticket.qfs`).
- Modem / network: `modem_config_screen` `0x213b0`, `modem_settings_screen` `0x62314`,
  `modem_connect` `0x605e0`, `modem_phonebook` `0x61338` (`*.num`), `net_wait_players`
  `0x55104`. Not to be ported (Danik, 2026-10-18: network skipped).
- The block `0x71208`..`0x74dcc` (small routines called from every screen): the widgets
  (pictures, text, buttons, the mouse `%smouse.fsh`), key names (`0x73068` German,
  `0x733a0` English), `fmt_speed` `0x74854`, `fmt_time` `0x74914` (`%d:%02d.%d`),
  `screenshot_tga` `0x72ac4` (`nfs%02d.tga`). The opponents' names "Drifter Burnout Psycho
  Phreak Hacker Razor Mr. Ice" are at `0x74748`.

- **The menus' common routines, as the port has them (2026-10-19; checked against the
  original: the game mode screen to 9 pixels, the exit dialog to 1 near-black pixel of
  DOSBox's colour adjustment)**:
  - Keys `fe_read_keys` `0x727a0`: the key *held*, first of Ctrl+F10 / F5 (both 0), Esc
    0x1b, Space 0x20, Enter 0x0d, 0x4800 up, 0x5000 down, 0x4b00 left, 0x4d00 right,
    0x5200 Ins, 0x5300 Del, F1-F4 (only when `0x12bd3c`), then the raw scan codes 0x2a,
    0x36, 0x1d, 0x38, 0x15 (Y), 0x31 (N), 0x24 (J); Tab 0; the joystick after.
    `key_down` `0x84742` (by scan code), `key_scancode` `0x91d60`. **The menus read held
    keys**: a DOSBox test must hold a key (`hold:Return:0.2`), a tap is missed.
    A screen acts on a key once, then again only when it is let go or held over 25
    hundredths (not Esc, Space, Enter). `key_flush` `0x84358` (empties the typed-key queue; `key_get` `0x84304`).
  - Mouse: the driver `0x8498e` sets the poll `[0xf7964]` = `0x848af` (relative mickeys,
    x at half speed; buttons 1 left, 2 right), hide `[0xf795c]`, show `[0xf7960]`; place
    `0xf78e0` / `0xf78e4`. `0x156604` the mouse has been used, `0x156606` its buttons.
    `mouse_load` `0x52020` (`MOUSE.FSH`: ptr0, ptr1 pressed, ptr2; 15x15, 255
    see-through), `mouse_draw` `0x52288` (also sets 0x156604), `mouse_buttons` `0x522e8`,
    `mouse_press` `0x52304` (ptr1 for 0.2 s), `mouse_save` `0x52348`, `0x71ad4` place +
    save, `0x71b24` moved.
  - Sound: `fe_sound_bank_load` `0x724c4` (`SIMDATA/SOUNDBNK/FRONT_MW.BNK`),
    `fe_play_sound` `0x72510` (sound n, pan 0x40, volume CONFIG.DAT word +0x79),
    `fe_sound_bank_free` `0x725ac`. 0x47 a move, 0x48 a choice.
  - Music: `music_choose` `0x63ed0`: mode CONFIG.DAT +0x73 (0 none, 1 any of 12 in turn,
    3 and 10-15 techno, else rock), counter `0xed0fc`; pick 5 of a set means none, so
    ROCK6 never plays (kept). `music_start` `0x72010` (looping AS4), the feeding
    `fe_service_stream` `0x71b6c` / `fe_service_music` `0x71c20`, `music_fade_out`
    `0x720d8`, `music_wait_end` `0x72150`.
  - Text: `setfont` `0x91ed0` (FFN +0xa count, +0x10/+0x11 origin, +0x12/+0x13 line,
    +0x14 characters, +0x1c the 4-bit bitmap), `printstring` `0x94234` (characters
    <= 0x20 move by the space's advance, newline: x 0, y + line), `text_width` `0x84794`
    (through `0xa3e30`, at most 256 characters). `textcolour` `0x95e10` with `0xa87ec`
    makes a ramp of 16 ARGB: a = k*255/15, the colour * k / 15. `0xa857c` also makes a
    16x256 table of nearest entries, but the letter routine `0xa8080` uses it only when
    the window has **7** bits a pixel (`0xf88b4` = window +0x18); 8-bit menus always take
    the slow path: `0xb1430` for each pixel of level 1..15 (level 0 is left alone):
    channel = round(under * (65536 - 256a) / 65536) + the ramp's, at most 255, then
    `nearestcolour`; level 15 is the colour's nearest entry. (The table path would also
    skip letters one byte wide.) Styles `fe_styles_init` `0x71208` (white ffffff, beige
    e8d3b7, yellow ffce06, red ff0606, the chosen one, black for the shadow, saved at
    0x152f04 0x153f44 0x154fd4 0x150b84 0x14fb44 0x151bc4), `fe_style` `0x7135c` (3 white,
    2 red, 1 yellow, else beige), `fe_text` `0x71774` (shadow black at +2,+2, then the
    text), `fe_text_centre` `0x716fc`, `fe_button_text` `0x713d0` (the same into a window
    of width + 10 by 45 holding the screen there, pasted back: text drawn over text).
  - Colours: `nearestcolour` `0x80c80`: the inverse table `0xf77c4` when there is one, else
    the closest of entries 0..253 in the palette copy `0x1607a4` (dg^2+dr^2+db^2, strict <
    from 99999). `make_inverse_table` `0x8227c` reads the DAC back (`read_dac` `0xa3184`,
    6 bits x 4) and builds the exact 32x32x32 inverse map (`inverse_colormap` `0x819c0`:
    cell centres x*8+4, entries 0..253, the lowest wins a tie). **`pal_fade_up` `0x71918`
    calls it at its end**; checked: the original's table on the game mode screen equals
    this computation in all 32768 cells. **It is Spencer W. Thomas's inverse colormap**
    (Graphics Gems II, `inv_cmap.c`: `0x819c0` the colours, `invcmap_red_loop` `0x81bd8`,
    `_green_loop` `0x81d5c`, `_blue_loop` `0x82000`; distances unsigned, 0xffffffff at
    first, colours B G R 0 from `read_dac`, only those allowed in `0xf77c8`): each colour
    scans out from its own cell and stops at the first cell it cannot claim after some it
    could, so a region's ragged edge can leave a cell to a farther colour. A plain search
    gives the same table on the menus and on 4 race snapshots, but 2 cells differ on r1's
    palette; the port does Thomas's (`font.c`), equal in every cell on all 6 (2026-10-19).
    **The allowed entries `0xf77c8`**: the program's 0..253; the race's own (`race_colours`
    `0x2e74c`), never put back.
  - `fe_yes_no_dialog` `0x73b04`: two windows of 340x100 holding the screen at (150,190);
    the box darkened through `remap_table` `0x89f3c` (0x9000 a channel, 6-bit cap, weights
    2R 3G 1B) by `polygon` `0x8a0cf`, two red frames `frame` `0x8a0f0`, the question red
    centred at (170,20), "Yes" at (60,60), "No" at (250,60). **"changed" starts at 1**: on
    its first pass it plays sound 0x47 and draws both buttons again with `0x713d0` (so
    they come out brighter than drawn once). Mouse placed at x 225 / 420, y 260 only if
    the mouse has not been used. Hit boxes yes x 0xbe..0x104, no 0x17c..0x1cc, y
    0xf5..0x118. Esc / N no, Y (J in German) yes, left / right swap, Enter / Space /
    a click the one chosen (the right button: no).
  - `fe_text_field` `0x701cc` (text, max length, centre x, y, names flag on the stack):
    a window of max/2*30 + 5 each side by 35 holding the screen; typed keys from the
    queue (`key_get` `0x84304`); names (flag 1) let in space . * + [ ] : ; " < > , ?,
    file names (0) none of them and letters become capitals; % \ / | never; German adds
    0x81 0x84 0x94 0xe1 0xe6 0x8e 0x9a 0x99 0xf8 0x15 (and installs its own keyboard
    handler `0x8447c` / `0x843bc`). Appends while length < max and the width < 440.
    Backspace, Del clears all, Enter (refused when only spaces, `0x98aac` isspace)
    copies back, Esc not. Any key redraws without the cursor; "_" blinks every 15
    hundredths. The mouse buttons are read once before the loop (after they are let go),
    so a click in it does nothing. `0x4dfe0` reads the joystick (`0x12f25c`: 0x20/0x0d).
  - `fe_enter_name` `0x743b4` (the name, CONFIG.DAT +0 = 0x156c14): the screen at
    (80,190) 480x100 darkened by `remap_table` 0x6000 (darker than the dialog's), red
    frames, "Please Enter Name" / "Bitte Namen Eingeben" red centred at (240,10), the
    field (8, 320, 230, names); sound 0x48 either way; a new name clears 0x15d12a and
    0x15d12e (bits the main menu keeps). The pointer is hidden and stays hidden. Port
    against the original: equal (only DOSBox's near-black noise), typing too.
  - `fe_game_mode_screen` `0x47274`: `CONTROL/GAMEMOD2.QFS` (bgnd, sinl/modl/exil lit,
    sind/modd/exid dark), the mouse picks by y (<= 0xcd single, <= 0x118 modem, else
    exit), button centres (320, 167/242/315); 45 s without a choice: back to the movies
    (result 2); single → `fe_enter_name` → 0, exit → 1, Esc and the right button ask
    "Exit to system ?".
  - `fe_outer_loop` `0x47000`: state 2; when going round again: music fade and wait, the
    attract mode, the first time CONFIG.DAT (state 3 if it fails), `music_choose`; the
    title when state is 2 or 3; state = the game mode screen; 0: `0x7469c`, the main menu,
    the settings saved; until 1. The main menu's result is the next state: 0 the game mode
    screen, 2 (2 minutes untouched) the title then the game mode screen, 1 quit.
    `fe_new_session` `0x7469c` copies the name into the tournament's records and clears
    them (`tourn_clear_records` `0x745f4`, 0x15d0d8 / 0x15d0e8 / 0x15d0fa), 0x15d134 = 0.

- **`fe_main_menu` `0x42c30`, "Control Central" (2026-10-19; the port's
  `port/src/mainmenu.c` equals the original in every screen tried: the start, car, track
  and race type changes, SKCART, the opponent, the buttons, Esc — 1 to 24 pixels, all
  DOSBox's colour noise)**. Network parts (0x15d136) left out.
  - Art `CONTROL/CENTRAL.QFS` (bgnd, cham = Chat greyed, the frames Trb3 opponent, Tlb1
    player, Brb4 location, Blb2 race type, arrows Aro1..Aro8, buttons Ra1l optl chal canl,
    Coml/Netl/Modl network; `!pal`), its inverse table `CENTRAL.INV` (`fe_load_inv`
    `0x747b8`) but `pal_fade_up` rebuilds it anyway. Each panel is its own file
    `CONTROL/<name>.QFS`, shape "0000", loaded again only when its key changes (0xec500/4/6
    track, 0xec502 player, 0xec4fc opponent, 0xec4fe race type; 0x14 = none;
    `mm_pictures_free` `0x424c0`). Names: tracks 0x41c80 (city coast alpine springs
    avalley bsienna vridge tpolis lvegas moon), mirrored with `m` 0x41ca8, rally with `r`
    0x41ccc; player 0x41cf0 (rx71 nsx1 sup1 9111 vet1 vip1 5121 dia1 war1); opponent
    0x41d20 (rx72 .. dia2, slew = traffic, watch2 = the clock, tourny, war2); tournament:
    tourny3 / tourny2 / tourny1 by the player's car < 3 / < 6 / else; race type 0x41d50
    (watch1 headhead single tourny, nomercy).
  - Drawing, into the off-screen window 0x156658 and shown (`screen_show_begin` `0x8378c`,
    `drawshapeatpos`, `screen_show_end` `0x83740`), then the pointer (`mouse_draw`, always):
    bgnd, cham, track, `flad` (tournament track done), player, opponent, race type, then
    the chosen one: a panel's frame and its arrow under the mouse with colour 255
    see-through (`drawshape_see_through` `0x815e4`), a button opaque. The first time it
    draws twice, fades up, builds the inverse table, the styles, and starts the 2-minute
    timeout. A change of choice or arrow plays 0x47 (not for "none") and draws all again
    (the buttons it draws first are covered by that).
  - The choice (8 = none): 0 opponent, 1 player, 2 location, 3 race type, 4 Race,
    5 Options, 6 Chat, 7 Cancel, 9 network. The keys put the mouse at 0xec418 / 0xec42c
    (x 482 175 444 211 45 228 400 570, y 158 197 318 349 454 454 454 454), since the
    mouse's hit test runs every pass. Hit boxes (inclusive, `mouse_in_rect` `0x71a1c`),
    in this order: opponent 0x153..0x271 x 0x61..0xda (race types 1 and 2 only; arrows x
    0x155..0x16a and 0x25d..0x26f, y 0x91..0xa8), player 0x13..0x14a x 0x70..0x11a
    (arrows 0x15..0x28, 0x136..0x148, y 0xba..0xd1), location 0x153..0x225 x
    0xfc..0x17f (arrows 0x155..0x168, 0x20f..0x223, y 0x131..0x14e), race type
    0x5a..0x14c x 0x13a..0x180 (arrows 0x5c..0x6b, 0x136..0x145, y 0x154..0x16d), Options
    0xa5..0x125, Cancel 0x1f8..0x27f, Chat 0x16b..0x1b5 (network), Race 0..0x70, all y
    0x1ba..0x1d1. A click tests the right arrows one pixel wider.
  - Keys (the held-key rule of the menus; any key hides the pointer, `fe_mouse_hide`
    `0x71afc`, until the next drawing): Up 0→4, 1→4 (time trial, tournament) else 0,
    2..4 → one less, 5..7 → 3, 8 → 1. Down 0→1, 1→2, 2→3, 3→4, 4..7 → 1 (time trial,
    tournament) else 0, 8 → 1. Left / Right on a panel change its value (sound 0x48),
    on the buttons Race → Options → Cancel → Race. Enter / Space / a click: on an arrow
    the value, on a car picture the showcase (`fe_showcase` `0x17724`), the location the
    track's records (`fe_track_select_tournament` `0x76380`), Race the race (or "Track
    already completed" in a tournament), Options `0x53a30`, Cancel back (0). Esc back (0),
    the right button "Exit game ?" / "Spiel verlassen ?". 2 minutes untouched (not in a
    tournament): result 2.
  - Values (CONFIG.DAT): +0x6d race type (0 time trial, 1 head to head, 2 single race,
    3 tournament), +0x91 opponent (0-7 cars, 8 traffic, 9 clock, 10 tournament, 11
    Warrior), +0x95 track (8 Lost Vegas, 9 never offered), +0x9b player car (11
    Warrior), +0xb3 race length 0/1/2 → +0xb1 laps 4/8/16, or 2/6/12 on tracks 4..7.
    `car_setup` `0x73a80`: +0x99 = 0 or 2 and +0x8f = 1 or 2 by the car (tables in the
    port). The player's car steps 0..7 (11 after 7 once the Warrior is unlocked); in a
    tournament only within the track's class (tracks 0,3: 0-2; 1,4,5: 3-5; else 6-7) and
    the track sets the car (0xec440 = 0 3 6 0 3 3 6 6 7 3). The opponent steps 0..7, or
    0..8 (traffic) in a single race. `mm_race_type_changed` `0x425b0`: time trial opp 9,
    head to head the player's car if the opponent was 8..10, single race opp 8,
    tournament: no mirror / rally, track 9 or a locked 8 → 0, its car, opp 10, length 1,
    8 laps (6 on 4..7). `mm_race_type_step` `0x4235c` (a tournament running asks "Quit
    tournament ?" first). Tracks: `mm_track_next` `0x4287c` goes normal → mirrored (if
    unlocked, not a tournament) → rally (likewise) → `mm_track_plus_one` `0x42910` (the
    next, of 9 with Lost Vegas else 8); `mm_track_previous` `0x42984` the other way.
  - Unlocks: +0x5f50 (kept in CONFIG.DAT) ≥ 1 mirrored tracks, the Warrior and Lost Vegas;
    ≥ 2 rally tracks too. **The typed codes** (`cheat_code_step` `0x46e94`, one letter
    each time a letter key goes down, 0x12ccaa; the strings stored as letter + 99 from
    0xec48c, their positions 0xec4d8..): SKCART mirrored tracks (and shows one), EARALLYE
    rally tracks, EXPLODE 0xf74a4, CASINOVILLE Lost Vegas (silent), CONCEPTCAR the
    Warrior as the car, TURBO 0x15d12a |= 8, NITRO 0x12cca0, ROCKYROAD +0x5f50 between 1
    and 2 with its unlocks (silent). Not in a tournament; TURBO and NITRO not in a network
    game. **End held** on head to head: "nomercy" picture and 0x15d12a |= 4, cleared on
    release.
  - The CD's CONFIG.DAT has type 1, opponent 7, track 1, car 6, 8 laps, length 1; the
    DOSBox install has none, so the original there starts from `cfg_defaults` `0x38ea8`
    (track 0). The port tests take the original's live state (`nfsdos.py
    mem:156c14:5f52:...`, `RENFS_CONFIG=`).
  - Options (sel 5): fade down, pictures freed, `fe_options_screen`; back: the race type
    checked again (a loaded game may change it), `CENTRAL.QFS` and its palette loaded
    again, the mouse put on Options, drawn and faded up as the first time (0x4577a).

- **`fe_options_screen` `0x53a30` (2026-10-19; the port's `port/src/options.c` equals the
  original on six screens: 6 to 24 pixels of DOSBox's colour noise)**. `OPTION/OPTIONS.QFS`
  (`OPTIONS.INV`, rebuilt by the fade anyway). Rows 0 Transmission, 1 ABS, 2 Traction
  control, 3 Audio (`fe_audio_options` `0x105dc`), 4 Graphics detail (`0x32d08`), 5
  Controls setup (`0x22bd0`), 6 Load and save (`0x3c0c8`; not in a network game), 7
  Credits (`fe_credits` `0x25600` with `credits.asf`), 8 Done, 9 Cancel. Shapes: `bgnd`,
  the names lit `Tral Absl Trcl Audl Gral Conl Loal Crel Donl Canl` (dark ones are in
  bgnd), values `Autd/Mand`, `On1d/Of1d/N/1d`, `On2d/Of2d/N/2d` and their lit `..l`
  (tables 0x53978..). The values: CONFIG.DAT **+0x97 gearbox (0 automatic, 1 manual; +0x8d
  = 1 when automatic, set with it), +0x99 ABS, +0x8f traction control (0 on, 1 off, 2 the
  car has none: not switchable)**. Mouse by height: y <= 0x85, 0xa7, 0xcb, 0xec, 0x10e,
  0x132, 0x155, 0x184 the rows; below, x <= 0x141 Done, x >= 0x143 Cancel; elsewhere the
  choice stays. Keys put the mouse at x 240 (Done 40, Cancel 565), y 118 151 185 220 253
  287 324 363 450 450. Up: one less (Cancel → Credits), from the top to Done; Down: one
  more, from Done / Cancel to the top; Left / Right swap Done and Cancel. A choice plays
  0x48 then, as the screen is drawn again, 0x47. Esc and Cancel put the four words back
  (and `0x721ec` re-applies the volume +0x77); Done, the right button, 2 minutes or
  `0x13c3ca` leave keeping them, fitting the opponent to the race type when a game was
  loaded. Every key acts once a quarter second (Esc included here). Leaving: fade down,
  `0x15d13e` = 1 (the main menu draws again), result 0.

- **`fe_audio_options` `0x105dc(where)`** (2026-10-20; `port/src/audioopt.c`, equal to the
  original by palette entry from all three places: only DOSBox's 158-pixel scaling strip).
  `where` 0 the options screen, 1 the race menus' options, 2 the pause menu (mode and 3D
  greyed: `Audm`, the mode's dark value, `3dem`, and passed over by the mouse and the
  keys; also with a card that has no modes, `0x15d14e`, and the mode alone on card 5).
  `OPTION/AUDIO.FSH`. Rows 0 Song (CONFIG.DAT **+0x73**: 0 none, 1 shuffle all, 2 rock,
  3 techno, 4..15 one of the twelve; names `0x104e4` Nony Shuy Shuo Shug Ramo Toxo Scuo
  Oveo Rouo Powo Filg Odyg Aaeg Hidg Fung Chog), 1 Race music (**+0x75**), 2 Menu music
  (**+0x77**), 3 Sound effects (**+0x79**), 4 Speech (+0x7b, byte ^1; `Oful/Onul`, dark
  `Ofud/Onud`), 5 Audio mode (**+0x71**: 0 16-bit stereo, 1 8-bit stereo, 2 8-bit mono;
  `16bl 8bsl 8bil` / `..d`), 6 3D enhanced (+0x7d, byte ^1, read by nothing else in the
  program; `Ofdl/Ondl`, `Ofdd/Ondd`), 7 Done, 8 Cancel. Labels lit `Sonl Racl Menl Soul
  Spel Audl 3del Donl Canl`. The mouse by height: y ≤ 0x89, 0xb3, 0xde, 0x109, 0x135,
  0x15e, 0x18d, then x ≤ 0x141 Done / ≥ 0x143 Cancel; keys put it at x 240 (Done 40,
  Cancel 565), y 114 158 201 244 287 329 372 450 450 (`0x104c0`, `0x104d2`).
  - The bars: a window 198 × 29 (`newwindow(0xc6, 0x1d)`) gets `Bol<n>` (lit) or
    `Box<n>` at 0, 0, then the clip x 6..0xc8 and the red `Bor1` (186 × 17) at
    (pos − 0xb2, 6), and is copied to (300, 0x8d / 0xb8 / 0xe3). pos = volume · 0xb8 / 0x7f
    (from the kept volumes while F5 has the sound off); a volume = pos · 0x7f / 0xb8, pos 1
    giving 1. A click inside x 0x131..0x1ec, y 0x93 / 0xbe / 0xe9 + 0x11 puts the bar at
    mouse x − 0x132 (and ends F5's silence); Left / Right a step; the race music's and the
    menus' music's are heard at once (`0x721ec`), the effects' by `0x721c0`.
  - Over the race music's and the effects' rows the music is heard at +0x75 and served
    by `0x71cec` (a stream started then gets +0x75), over the menus' music's at +0x77 by
    `fe_service_music` `0x71c20` (+0x77): `0x72224` every pass. Over the effects' row
    sound 0x4a plays again whenever it has ended (`0x72578`; its voice `0x15662c`) and is
    stopped as the row is left (`0x72564`).
  - Entering with a song > 3 (or from the pause menu) the menus' music fades and the race
    music starts (`0x63d88`). Song: the music stopped (`0x7211c`, `0x72150`), +0x73 + 1 mod
    16, the new one started 100 ticks later (`0x63d88`) unless none. The key stays live:
    Enter or the button held goes on through the songs (each pass waits `mouse_press`'s
    fifth of a second), as it drags the bars.
  - Audio mode: + 1 mod 3 (0 skipped without 16 bits, `0x15d150`), the box "Changing
    audio mode" (`0x74234`: text_width + 20 by 60 at 320 − w/2, 210, darkened by the
    0x9000 table, two red frames, the text in red at y + 18), the music stopped, the bank
    freed, the card set up again (`0x71db8`), the song started again, the bank loaded.
  - Esc and Cancel put CONFIG.DAT +9..+0xb4 back (0xac bytes, taken as the screen began)
    and the music's volume (+0x77); Done, 2 minutes or the right button keep it. At the
    end: if the mode differs from the last one set up (Cancel put the old one back) the
    card is set up again and the menus' music chosen again (`0x63e2c`; the pause menu's
    `pause.asf`); from the pause menu `pause.asf` again anyway; then `0x721ec(+0x77)`.
- **`fe_graphics_options` `0x32d08(pause)`** (2026-10-20; `port/src/gfxopt.c`, equal to
  the original by palette entry from the options screen and the pause menu: 0 to 6 pixels
  of DOSBox's scaling on 21 pictures). `OPTION/GRAPHICS.QFS`, or `CHECK/GRAPHICS.FSH`
  while `0x15d126` is set (a race session, `race_from_menu`: the race menus and the pause
  menu). From the options screen and the race menus with 0, the pause menu with 1: the
  resolution's row greyed (`scrm` and `640m`/`320m`), passed over by the mouse and the
  keys, the screen starting on row 1. `0x12bd3c` = 1 meanwhile (`fe_read_keys` gives F1-F4).
  Rows (lit label; value pictures lit / dark from `0x32c10`..): 0 Screen resolution
  (**+0x7f**, byte ^1: **0 races at 640x480, 1 at 320x200**; `scrl`; `640l 320l`), 1
  Window size (+0x81 + 1 mod 3; `winl`; `full wmdl smal`), 2 Image quality (+0x83 + 1 mod
  4, at +0x7f = 1 back to 0 above 1; `imal`; `ihil imdl ilwl intl` (`atml atll` unused)),
  3 View distance (+0x85 + 1 mod 3; `vanl`; `farl vmdl clsl`), 4 Car detail (+0x87 ^1;
  `carl`; `chil clwl`), 5 Horizon (+0x89 ^1; `horl`; `honl offl`), 6 Done (`donl`), 7
  Cancel (`canl`); the dark ones `..d`, the dark labels the background's. Resolution to
  320x200 also brings the quality above 1 down to 1. The mouse by height: y 0..0x8c,
  0x91..0xc1, 0xc6..0xf2, 0xf7..0x127, 300..0x15c, 0x161..400, then x ≤ 0x140 Done (y
  0x1a4..0x1e0), ≥ 0x145 Cancel (..0x1df); keys put it at x 230 (Done 60, Cancel 565), y
  116 169 220 270 325 377 445 445 (`0x32bf0`, `0x32c00`). Enter, Space or a click steps
  the row chosen (0x48; the mouse put back on the row when not in use); **F1 window, F2
  quality, F3 distance, F4 horizon** step their setting from any row and put the mouse on
  it (the row follows the mouse at the next pass). Up: Cancel as Done, one less, round to
  Done (and past row 0 in the pause menu); Down: (+1) mod 7, from Cancel to the top;
  Left/Right swap Done and Cancel only. A change draws the two rows (0x47), then the whole
  screen at the next pass. Keys act once a quarter second (25 ticks) or once let go. Esc
  and Cancel put +9..+0xb4 back; Done, 2 minutes, the right button or `0x13c3ca` keep
  them. The end: `0x12bd3c` = 0, faded down, the art freed. After it the options screen
  puts the mouse on its row (`0x5458d`), the race menus only when the mouse is not in use
  (`0x12e1e`).
- **`fe_controls_setup` `0x22bd0(race_menu)`** (2026-10-20; `port/src/ctrlopt.c`, equal to
  the original by palette entry on 25 pictures from the options screen and the checkpoint
  menu's options, prompts and the conflict included: 15 pixels of DOSBox's scaling inside
  bgnd's labels, 29-30 when a pointer's line adds some). `OPTION/CNTRLSET.QFS`, from the
  race menus (`0x12e88`, 1) `CHECK/CHCONSET.QFS`. **The controls** (`0x15cb4a` = CONFIG.DAT
  +0x5f36, 26 bytes, a copy kept for Cancel and Esc): +0x5f36 the steering's kind (0 the
  keys, 1 the joystick across, 4 the mouse), +0x5f38 the pedals' (0, 2 the joystick up and
  down, 3 its buttons, 5 the mouse's buttons), +0x5f3a the gears' (0, 3, 5; the pedals and
  the gears never both on buttons of one device), then the keys as `fe_read_keys`'s codes
  (a character, an extended key's scan << 8): +0x5f3c left, +0x5f3e right, +0x5f40 faster,
  +0x5f42 brake, +0x5f44 up, +0x5f46 down, +0x5f48 handbrake, +0x5f4a camera, +0x5f4c horn,
  +0x5f4e the map (not on this screen). Rows: 0 Steering, 1 Accelerate/brake, 2 Shift
  up/down, 3 Camera, 4 Handbrake, 5 Horn, 6 Calibrate joystick (only with one, `0x12f1c0`;
  `fe_joystick_calibrate` `0x4c6b0(1)`), 7 Done, 8 Cancel; lit labels `strl accl shfl caml
  hndl hrnl call donl canl`; the kinds' pictures dark/lit by kind (0 k, 1-3 j, 4-5 m):
  `stkd stjd stmd`/`stkl ..`, `ackd ..`, `shkd ..`; rows 3-5 lit `cakl hbkl hnkl`. The
  values are **text** (`fe_text` at 400, y 0x68 0x90 0xb8 0xe0 0x108 0x130): keys "%s/%s"
  of their names (`0x73068`: Space Enter Insert Delete Home End Pgup Pgdown Up Down Left
  Right Backspace Tab Center, else the character in capitals; German Leertaste .. Pos 1
  Ende Nachste Vorherige Hoch Runter Links Rechts, the character as it is, umlauts too, none
  upper-cased), else the kind's ("Keyboard" "Left/Right" "Up/Down" "Button1/Button2"
  ..; German "Links/Rechts" "Hoch/Runter" "Taste1/Taste2"). The whole screen: style 0, bgnd,
  the three kinds dark, the six texts, then style 1 the row chosen's label, kind lit and
  its text again over the first. **On the first draw the palette is set black after the
  drawing** (`0x23767`), before the fade up (set before, the letters' blending would mix
  with black: 1,380 pixels off); before the loop the cleared screen (`0x72600`) has
  already been faded up once. A change draws the two rows (0x47; never seen: the whole
  screen follows at the next pass). The mouse by height: 0..0x86, 0x8a..0xae, 0xb2..0xd6,
  0xda..0xfe, 0x102..0x127, 300..0x14e, (joystick) 0x152..0x18b, then x ≤ 0x140 Done (y
  0x195..0x1e0), ≥ 0x145 Cancel (..0x1df); over x 0x186.., y 100..0x14a it is on the keys.
  Keys put it at x 200 (Done 50, Cancel 565), y 118 157 196 237 277 317 356 450 450
  (`0x229c0`, `0x229d2`). Up: Cancel as Done, one less, past row 6 without a joystick,
  round to Done; Down: (+1) mod 8, Cancel → 0, past 6; Left/Right swap Done and Cancel.
  **Enter, Space or a click** (0x48): rows 0-2, Space or a click on the name: the next kind
  the machine has (`0x240c1`, `0x2440d`, `0x247f2`; a joystick `0x12f1d8`, the mouse
  `0xec408`; DOSBox gives Keyboard ↔ Mouse for the steering); back to the keys a conflict
  asks for keys at once; Enter or a click on the keys with the keys chosen: **the two
  keys asked for**: the keyboard's picture lit, the row's text cleared (the window of 240
  by 40 kept from bgnd), the prompt (`ensl`/`ensr`, `enac`/`enbr`, `enup`/`endn`: "Enter
  key for left..") and `fe_button_text("_")`, the key, "%s/_" over it, the second key,
  "%s/%s"; rows 3-5 one key (`enhb` "Enter key...", single name). Then **`0x7390c`**: two
  controls of one kind, or two of nine keys equal (the kinds' places count as 0..5) →
  `conf` ("Control conflict / Please re-enter") two seconds (`settimeout(200)`), `conb`,
  asked again. The pointer is not seen meanwhile. **The key** (`0x733a0`): once every
  key is up and the queue emptied (`0x84358`; the German game takes the BIOS's keyboard
  meanwhile, `0x8447c` .. `0x843bc`, so the DOS driver's German layout), nothing while a
  Shift is held; refused: `> ? @ ^ _ p { | } ~ m M P ( ) * + : < - Tab ! " # $ % &` and
  anything without a name (German also `< > ^ \``); Esc or the right button keep the old
  key; letters (and umlauts) come in capitals. After each press: 2 minutes again, wait for
  the keys up, the mouse back on its row unless in use. Esc and Cancel put the 26 bytes
  back; Done, 2 minutes or the right button keep them. The end: faded down, freed.
  **In a race** (`race_run` → **`0x36a0c`**): the kinds to the player's entry
  (`0x12d974`..: 0 keys, < 4 → 1, else 2; the pedals and gears 3 for buttons), each key
  through **`0x36758`** (German only: the German layout's character to the US one of the
  same key, Y and Z swapped, a table built in code) and **`key_scancode` `0x91d60`** (its
  first place in EA's 5 × 0x5b character table `0xf7b74`, an extended key as scan | 0x80)
  into `0x12d92c` left, `0x930` right, `0x934` faster, `0x938` brake, `0x93c` up, `0x940`
  down, `0x944` handbrake, `0x948` horn, `0x94c` the camera, `0x950` the map. So the German
  game's default camera key is K (+0x5f4a 'K'). The port reads a key by its physical place
  (`scan_get`) and makes the code the BIOS would give with a US keyboard (EA's table without
  Shift), or the German one in the German game; a joystick or the mouse to drive with:
  not yet (the kinds stay the keys).
- **The pause menu and the detail**: as it opens CONFIG.DAT +9..+0xb4 is put back from
  the copy kept for the race (`0x565c1`), then the race's detail written in (`0x56b48`:
  `0x12d9d9` at most 1 at 320x200 (`0x12c308` = 0), +0x83 = `0x12d9d9`, +0x85 =
  `0x12d9d5`, +0x87 = `0x12d9d7`, +0x89 = `0x12d9d6`, +0x81 = the outside view's layout
  less its first (`0x144cc`)). At its end the copy is taken again (`0x5809d`), the detail
  read back (`0x5818d`: `0x12d9d4`, `0x12d9d9`, `0x12d9d5`, `0x12d9d7`, `0x12d9d6`), the
  copy taken once more, the camera's mode set (`0x14b00`) and the views set up
  (`0x1440c(+0x81, min(0x12d9d9, 2))`). The road's distance itself (`0xf6b48`) is only
  asked again by the next Esc or F3.
- **F5 in the menus** (`0x72638`, from `fe_read_keys` `0x727a0` and `0x72ac4`): +0x75,
  +0x77, +0x79 kept in `0x15d148` / `0x15d14a` / `0x15d14c` and set to 0, every voice's
  volume 0 (`0xf748e` = 1); again: put back and heard (the effects' voices +0x79, speech's
  +0x79 · 3 / 2, the music +0x77). Then a third of a second's wait (`settimeout(0x21)`) and
  the 2 minutes again. Before a race (`0x45b21`), the race menus' replays (`0x1ae60`,
  `0x1be94`), the pause menu's end (`0x58107`), the load screens' replays and the end
  (`0x4721e`) the three are put back without being set. (`fe_outer_loop` saves CONFIG.DAT
  before `0x4721e`: quitting with F5's silence on saves the volumes as 0.)
  `0x72ac4` is `0x727a0` with a screenshot key (`nfs%02d.tga`, when `0xf9474`); the audio
  screen reads its keys with it while the race music's service is in use.
- **`race_menu_options` `0x12760(finish)`** (CHECK/CHKOPTS.QFS; equal to the original by
  palette entry, 0 pixels): rows 0 Audio (`0x105dc(1)`), 1 Graphics detail (`0x32d08(0)`), 2
  Controls setup (`0x22bd0(1)`), 3 Load and save (`0x3c0c8(1)` from the checkpoint menu,
  `0x3d4c4(1)` from the finish menu; not in a network game, `loam` drawn then), 4 Done, 5
  Cancel; lit `audl gral conl loal donl canl`. The mouse: y 0..0x9b, 0x9d..0xe9,
  0xeb..0x136, 0x138..0x1a4, then x ≤ 0x141 / ≥ 0x143 from 0x1a6 (the gaps keep the
  choice); keys put it at (100, 119 196 271 345), (40, 450), (565, 450). Up: one less,
  round to Done; Down: (sel + 1) mod 5 (Cancel → Graphics); Left / Right swap Done and
  Cancel. After a screen the pointer is put back on the row only if the mouse is not in
  use; the pointer drawn (`mouse_draw` `0x52288`) marks it in use (`0x156604` = 1).
  Esc and Cancel put +9..+0xb4 back and `0x721ec(+0x77)`; the end saves CONFIG.DAT
  (`0x3a134`). Both race menus call it (the finish menu's Options is `0x12760(1)`, not
  the chat `0x195d8`).
- **Load and save: `fe_load_save_screen` `0x3c0c8(from_race_menu)` and `0x3d4c4(1)`**
  (2026-10-20; `port/src/loadsave.c`; equal to the original by palette entry on 34
  pictures: the screen, the list opening and closing, scrolling, the save box, the
  messages, the delete list and its question, a tournament loaded: 47 pixels everywhere,
  DOSBox's scaling on two lines, and the pointer where DOSBox put its own). `0x3c0c8(0)`
  from the options screen (`OPTION/LOADSAVE.QFS`), `(1)` from the checkpoint menu's
  options, `0x3d4c4(1)` from the finish menu's (both `CHECK/`, `0x12cbf4` = 1). Rows 0
  Save tournament, 1 Load, 2 Delete tournament, 3 Save replay / highlights, 4 View
  replay, 5 View highlights, 6 Delete replay, 7 Done; lit `savl loal dlgl sarl vwrl vwhl
  dlrl donl`, greyed `savm loam` (`vwrm vwhm` in a network game). **`0x15d134` "a game
  is on"**: set when Control Central starts a race (`0x45ab7`) and by Load; cleared by
  `fe_new_session` `0x7469c` and by "Quit tournament ?" (`0x423a3`). Save can be chosen
  from the options screen only while a game is on, from the finish menu only in a
  tournament not complete (raced[0..7] sum 8, or 9 with raced[8] when Lost Vegas is
  offered `0xec460`), never from the checkpoint menu; Load only from the options screen
  with no game on. Start: Load; 3 from the checkpoint menu; Save while a tournament is on;
  `0x3d4c4`: Save, else Delete tournament. Up/Down step past the rows that cannot be
  chosen, round. The mouse by height (English 0..0x87, 0x8c..0xaf, 0xb4..0xd7, 0xdc..0xff,
  0x104..0x127, 0x12c..0x14f, 0x154..0x190, 0x19a..0x1e0; German from row 3 0xdc..0x118,
  0x11d..0x140, 0x145..0x168, 0x16d..0x1a4, 0x1ae..); keys put it at x 155 (Done 40), y
  120 160 200 240 280 320 360 450 (German 120 160 200 251 302 340 390 450). **A change
  draws the whole screen again** (0x47): `0x3ca8d` also draws the old row dark and the new
  one lit into the menu's window, but its `mov [esp+0x730], 1` comes after a `push`, so it
  is the full redraw's flag `0x72c`, and the window is shown only by that redraw: the
  rows left show bgnd's own dark letters (the dark shapes differ from bgnd in ~1,800
  pixels each; seen in the original's off-screen window, `nfsdos.py snap:`). The finish
  menu's `0x3ddb7` the same (`0x6fc`). Enter (0x48): Save → **`0x3a2b0`**: a 400x100 box
  at (0x78, 0xbe), darkened (0x9000) with two red frames, "Save game as" / "Spiel
  speichern als" (style 2, centred, y 200), the text field (8 letters, centre 0x140, y
  0xfa) over CONFIG.DAT +9; the session gets the keys +0x5f36 (26 bytes), `0x15d0d6` Lost
  Vegas and +9..+0xb4; `gamedata/savegame/%s.SAV` = the session `0x15cb68`, 0x5b6 bytes
  (the racers 8 × 0x95, then kept settings 0x4a8, keys 0x554, Vegas 0x56e, points 0x570,
  won 0x580, raced 0x592, gave up 0x5a4, type 0x5b4); "Replace game ?" when it exists;
  "Game saved" / "Spiel gespeichert" (**`0x74dcc(text, ticks)`**: 440x60 at (100, 210)
  kept, the message box, until the ticks, a key held or a button, 10 more ticks, put
  back; a key still held from the Enter ends it at once, in the original as well). Save
  replay → **`0x3a628`**: a 480x100 box at (0x50, 0xbe), "Save replay as" /
  "Wiederholung speichern als", the name +0x3b cut at its '.'; REPLAY, REPLAY1..3 →
  "Cannot replace system replay"; "Replace replay ?"; replay.rpl copied (`0x3a218`);
  "Replay saved". Load, Delete tournament, View replay / highlights (`0xec248`), Delete
  replay → **the list `0x3aacc(kind)`** (0 view, 1 delete replay, 2 load, 3 delete game):
  `LOADANIM.QFS` (CHECK or OPTION), a box at (353, 108) 205x262 opening `opn8` with
  `opn1..7` see-through over it, a tenth of a second each (0x49), closing `opn7..1`
  (0x49). The files of `*.SAV` / `*.RPL` (100 at most, `0x12c424`, 20 bytes each),
  shown without the extension at (+0x23, +0x1c + 0x15 i), cut to 0x7d (`0x717d0`),
  style 0, the chosen style 1 from the 7th frame; `arul` (+0x5b, +7), `ardl` (+0x5b,
  +0xf1), `canl` (+0xb7, +0x55; German +0x4e) lit when chosen. **The first file is
  chosen when there is one, Cancel only when none** (`0x3ada3`/`0x3adaa`, again stores
  after two pushes: `0x134` back, `0x12c` the choice); then for kinds 2 and 0 the file
  that +9 ("%s.SAV") or +0x3b ("%s.RPL") names: its row, or with ten or more at the top.
  The mouse is kept in 0x165..0x22a × 0x6f..0x16e (`0x84885`); places x 455 (366 the up
  arrow's, 545 Cancel's), y 121 + ~21.2 a row. Left from Cancel goes back to the row
  left, Right to Cancel; Up/Down skip empty rows and scroll at the arrows (Up from the
  first row in the original runs the choice below 0 over the memory before `0x12c424`:
  nothing lit, the pointer in the box's corner; the port stops at the arrow). The right
  button is Cancel. Load: "Load game ?", the results cleared (time, distance, top, totals,
  laps), `0x15d134` = 1, the settings, keys and Vegas put back, type 3, "Game loaded
  successfully", and the screen's choice goes to Done; 1 back when an open road's later
  segment is next. View: "Load replay ?" / "Load highlight ?", +0x3b the name, played
  with +0xaf 1 or 2 through `race_run(2)` (the session not kept around it). Delete: "Delete
  game ?" / "Delete replay ?" (the system replays refused with a message). The DOSBox
  copy of the original loads a `.SAV` written by the port (`build/dosls`).
  **Control Central** with a game on: changing the race type or Cancel/Esc ask "Quit
  tournament ?" / "Wettkampf beenden ?" (yes: head to head, a new session); Race on a
  tournament's track already raced: "Track already completed" / "Strecke bereits
  fertiggestellt" (100 ticks), and that track's picture carries `flad` see-through
  (`0x43e3c`: type 3 and raced[+0x95]).
- **`fe_credits` `0x25600`** (2026-10-20; `port/src/credits.c`; the five pages equal to the
  original by palette entry, DOSBox's one scaling line only). From the options screen
  (`0x546db`): faded down, the music faded out and ended, `music_start("credits.asf")`
  (`0x72010`: nothing when +0x73 is 0), the credits, the music faded out and ended,
  `0x63e2c` a menu song again, the pointer back on the row, the screen drawn and faded
  up. The credits: `credits.fsh` of the language's art folder (`0x747ec` is
  `sprintf("%s%s", 0x1561a4, name)` then `0x83f18`), its `!pal`; pages `bgn1`..`bgn5`,
  each drawn into the menu window and shown, faded up, `0xed0fc` = 0, then until Space,
  Enter or the left button (the next page), Esc or the right button (the end; `0x13c3ca`
  too, the modem game's), or `0xed0fc` ≥ 2000; faded down. The buttons count only once
  they have been seen up (one flag for all five pages). No pointer is drawn. At the end
  the music faded out, a fade down, the end of the music, the file freed, `mouse_save`.
  **`0xed0fc` is a free count of the timer's ticks**: `race_timer_tick` `0x4c454`
  (installed at start-up, `0x47202`) adds one every tick, menus and races alike; the
  credits, the race menus (`0x1a703`, `0x1b7c0`), the pause menu (`0x56ece`),
  `controls_read` (`0x492b5`) and `0x6034c` set it to 0 to time something, and
  `music_choose` `0x63ed0` takes the song from it (in the port `timer_counter`; before it
  never moved and the song was always the first of the set). In this DOSBox both it and
  the system clock `0xf7980` count about 70 a second, so the credits' 2000 take 28 s there.
- **The cars' showcase `fe_showcase` `0x17724`** (2026-10-21; `port/src/showcase.c`; every
  page tried equal to the original by palette entry, DOSBox's near-black noise only: the
  main page, History, General and its next cars, Performance and its slides, Mechanical
  and its slides, back to Control Central).
  - From Control Central (`0x45189` the opponent's picture, `0x4545c` the player's; not
    with the player's car 11, the Warrior, nor with `0x15d142` from NFS.CFG, nor the
    player's in a network game `0x15d136`): `music_fade_out`, `pal_fade_down`,
    `music_wait_end`, `music_start("showcase.asf")`, `0xec4fc` = 0x14 (the opponent's
    picture loaded again). **The opponent's showcase shows the opponent's car as the
    player's** (+0x9b = +0x91, the traffic's 8 as car 0), and after it the car it ended on
    is the opponent's and the player's car is put back. After: `0x63e2c` (a menu song),
    the menu's palette, the pointer on the panel, the screen drawn and faded up.
  - Files: `FRONTEND/SHOW/` (GSHOW in German, `0x1565b4`) `<car>.qfs` the main page
    (names `0x14fb8`: rx7 nsx supra 911 vette viper 512tr diablo, by +0x9b),
    `<car3><kind>.qfs` the others (`0x14f2c` = `0x14eb8`: rx7 nsx sup 911 zr1 vip 512 dia;
    kinds his gen per mec), the film `FRONTEND/MOVIE/<car>.tgv` (`0x341a4`), the
    commentary `<car3><kind>16.eas` from `FRONTEND/SPEECH` (`0x72370` = `0x722b4` with the
    speech folder `0x1560b4` put in front; 8-bit 16 kHz, **up to a minute long**: the
    port's speech had room for 4 s and was cut).
  - The main page: choices 0 History, 1 Video, 2 General, 3 Performance, 4 Mechanical,
    5 next car, 6 previous car, 7 Done, 8 none; the keys' moves (shorts) up `0x14f4c`
    (3 3 3 5 2 1 4 4 7), down `0x14f5e` (5 4 4 1 6 3 2 2 7), left `0x14f70`
    (1 2 0 2 0 6 7 5 7), right `0x14f82` (4 0 1 0 1 7 5 6 7), the pointer's places x
    `0x14f94`, y `0x14fa6`. Mouse areas in the order tried: Video (0x134..0x1bc,
    0xd1..0x141), History (0x1c3..0x267, 0xf6..0x16e), General (0x86..0x12c,
    0xa4..0x119), Performance (0x134..0x212, 0x4e..0xcb), Mechanical (0x52..0x14f,
    0x146..0x19c), previous (200..0x104, 0x1ae..0x1e0), next (0x168..0x1a4), Done
    (0..0x82). Start: lit 1 (Video), the pointer there unless the mouse was used; the
    screen cleared (`0x72600`), the page's palette set at once (`0x83fe4`), the inverse
    table, the styles. A drawing: `bgnd`, the lit choice (`hisl vidl genl perl engl`
    see-through, `nexl prel donl` not; `vidl` only with the films `0x15d120`); the first
    after a load with the palette black (`0x84080`) then faded up, the inverse table, the
    styles. A new choice: sound 0x47, one retrace, the old one dark and the new lit (into
    the hidden screen: drawn over by the redraw that follows, so never seen). Enter on
    History / General / Performance / Mechanical: the file freed, faded down, the page,
    the file again, the pointer on the choice; Video: the music faded out, the film, the
    music again; next / previous car: `car_setup` `0x73a80`, faded down, the new car's
    file (no pointer move); Done or Esc (sound 0x48). Two minutes untouched or the right
    button: as Done. **In a tournament the cars go round inside the track's class** (+0x95:
    0 or 3 → cars 0..2; 1, 4, 5 → 3..5; else 6..7), else round all eight.
  - `fe_showcase_history` `0x16290` ("his": the text is the background's; "gen": the
    figures and two frames of five slides, `f2s0..4` small at the top, `f1s0..4` large,
    each with arrows): choices 0 frame 2, 1 frame 1, 2 Done, 3 previous, 4 next, 5 none;
    starts on frame 2 ("gen") or Done ("his", or no mouse: the pointer put there). Up /
    down (`0x14f14` 3 0 1 1 1 2, `0x14f20` 1 3 0 0 0 2) only on General, and from none to
    Done; left / right on a frame turn its slides (sound 0x48), else along Done → previous
    → next, round; over a frame the pointer on its arrows lights them (`f2lf f2rt f1lf
    f1rt`) and Enter or a click there turns the slides. The pointer's places `0x14efc`,
    `0x14f08`. In a tournament its next and previous cars go round the classes 0 or 3, 1
    or 4, else (track 5 with the last: unlike the main page's). The commentary starts as
    the page first fades up and stops as it is left or the car changes (`0x72454`,
    `0x72490`). The flag at `[esp+0x380]` that would light the arrows alone is never set.
  - `fe_showcase_specs` `0x15008` ("per": `acc1 bra1 han1` with their figures `accl brak
    hand`, German two of them, the slides turned with `^ 1`; "mec": `engn driv chas`):
    choices 0 the frame, 1 Done, 2 previous, 3 next, 4 none; up and down both `0x14ea4` =
    `0x14eae` (2 0 0 0 1); the pointer's places `0x14e90`, `0x14e9a`; Performance's Done
    area is narrower (0..0x69). Otherwise as the photo pages.
- **The track's screen `fe_track_screen` `0x76380`** (2026-10-21; `port/src/trackrec.c`;
  every page tried equal to the original's screenshots by palette entry, at most 50
  pixels of DOSBox's near-black, English and German). From Control Central's track
  picture (`0x456af`; not over a link, not track 9): the menu's music faded, showcase.asf,
  the opponent's picture let go (`0xec4fc` = 0x14), afterwards `0x63e2c` and Control
  Central's palette and inverse table again. `ART/TRACKSEL/<name>.QFS` by the names
  `0x750cc` (citysel coastsel alpsel rustsel autsel brunsel vertsel tropsel lostsel), its
  `.INV` (rebuilt by the fade anyway), the description `0x750f0` (city.eas coastal.eas
  alpine.eas loop1.eas loop2.eas goldrash.eas loop3.eas industri.eas loopx.eas) said by
  `0x72370` as it first fades up, stopped (`0x72454`, `0x72490`) on leaving or another
  track. Esc puts back the 0x2b dwords of CONFIG.DAT from +9 copied at the start.
  - Choices (the mouse areas, `mouse_in_rect` `0x71a1c`(x0, x1, y0, y1)): 0 Done (0..0x82,
    y 0x1b3..0x1e0), 1 previous track (200..0xfa), 2 next (0x172..0x1a4), 3 Best Times
    (0x1e0..0x280, y ..0x1df), 4..6 an open road's segment maps (x `0x75234`[t] ..
    `0x75246`[t], y `0x751c8`[3t+k] .. `0x751fe`[3t+k]; the circuits' are at y 500), 7 a
    circuit's laps (0..0x140, 0x15e..0x183; track > 2), 8 none, 9 the time of day (0..0x140,
    0x184..0x1a0; not on track 8 nor mirrored nor the rally). Keys: open roads up `0x75054`,
    down `0x75068`, left `0x7507c`, right `0x75090`; circuits `0x75004`, `0x75018`,
    `0x7502c`, `0x75040`; up or down onto 9 where there is no time of day → 7 / 4, or 0;
    the pointer's places `0x750a4`, `0x750b8` (placed at the start without a mouse).
  - Drawn: `bgnd`; the time of day `0x74ff8` (mord midd eved, see-through); style 0; a
    circuit's length `0x75168`[t−3] (kilometres `0x751b0` with `0x15d140`, German) at
    (0xc8, 0x153) and its laps +0xb1 "%d" at (0xdc, 0x16b) (style 1 when lit); an open
    road's segment length `0x75114`[4t + seg] and the road's `[4t]` (km `0x75180`) at
    (0xc8, 0x153), (0xaf, 0x16a), the segment's `bsg%d`, `ssg%d` (the segment a local,
    1..3, starting at 1); the lit one see-through: donl prel nexl besl ssg1..3 lapl, or
    `timl` + `0x74fec` (morl midl evel). A change of the lit choice: sound 0x47, the old
    one dark and the new lit drawn to the back screen and the whole page drawn again at
    the next pass (so only the sound shows; the dark laps' "%d" is formatted and never
    drawn).
  - Enter (sound 0x48): previous/next track (0..7, 8 with `0xec460`; a tournament: the
    car `0xec440`[t], opponent 10, `car_setup` `0x73a80`; the laps by +0xb3 again: 4 8 16,
    2 6 12 on tracks 4..7), the page loaded again; Best Times; a segment; the laps (not
    in a tournament: +0xb3 + 1 mod 3, the laps again); the time of day: 1 → `0x75144`[t]
    (0 2 2 2 0 2 2 2 2), else 1. Two minutes or the right button: leave.
  - **The time of day `0x12dd30`** (9 ints, main fills them with 1 by `0x89897`, a dword
    fill) is the race record's end (`0x12da0c` + 0x324): REPLAY.RPL keeps it, the CD's
    four have all 1. `race_run` `0x374ed`: the family switch `0x12c340` = 1 with the
    mirror or the rally, else `0x12dd30`[track] (below, `load_track_fam`).
  - **Best Times `fe_track_best` `0x75288`** (`ART/TRACKSEL/<name>BST.QFS`, names
    `0x74fa8`): the ten best totals of the track (+0xb5 + 0xa6b t + 0xea, a circuit's
    + 0x186 · race length), each name cut to 0x10e at (8, 0x73 + 0x16 i), time
    `fmt_time` at 0x118, car `0x74fcc`[+0xf] (RX-7 .. DIABLO: a car past 7 reads past the
    table) at 0x1e0, kind +0x23 at 0x24e (t.t. h.h., German "  z" "  d"); the top speed
    (record 36, when its +0x17 is not 0) at y 0x17c, `fmt_speed`. The page whole only at
    its first drawing; then only the lit choice (donl larl rarl desl, drawn whole), a
    change drawing the old one dark (dond lard rard, desd see-through) and the new lit
    (desl see-through). Choices 0 Done (returns 1: the track's screen leaves too, the
    music faded), 1/2 the previous/next track (as above, no speech), 3 Description
    (returns 0: the track's screen again), 4 none; left `0x74f80`, right `0x74f8a`, up or
    down from none → Done; the pointer `0x74f94`, `0x74f9e` (on Description at the start).
  - A trap for DOSBox tests: CONFIG.DAT is at `0x156c14`, so +0x7f (the races' 640x480 or
    320x200) is `0x156c93` and the segment +0x93 is `0x156ca7`: `poke:156c93:0100` runs
    the race at 320x200 (seen as a "different" rally status bar, 2026-10-21).

### 4.3 The race

```
race_from_menu 0x3770c (from fe_main_menu)
  race_run 0x37228 (also from fe_attract_mode, the load/save screens, the race menus)
    race_setup_record 0x36c5c   the race record (0x12da..), clock rate setting 0x12dd0c = 1
    race_clock_start 0x4c520, race_clock_rate 0x4c5f8
    race_main 0x6356c
      loading: load_slide 0x64d28 (the loading picture), snd_load_car_banks 0x52774,
        load_track_info 0x49dc0 (%s%sInfo), load_car_cfm 0x65874 (via 0x51388 "carObj"),
        load_smoke 0x645bc, race_load_track 0x5ba1c -> load_tri 0x4f010, load_vis 0x58bc4,
        load_traffic_cfg 0x7de84, load_dash 0x2e8d4, load_cockpit_mask 0x5f7c8
      the loop:  while steps done 0xf6d60 < steps due 0xf6d64:  race_step 0x6348c
                 then race_draw_frame 0x5bc40
  race_menu_checkpoint 0x1a5d8 (check/chkmenu.qfs, REPLAY.RPL, "End game ?")
  race_menu_finish 0x1b6a0 (check/finmenu.qfs)  -- both call race_menu_options 0x12760
  race_pause_menu 0x56474 (check/pause.fsh, pause.asf)
```

- **The race clock.** `race_timer_tick` `0x4c454` runs on every timer interrupt (100 a
  second). A countdown `0x12f19c` is reloaded with `0x12f1a8` twice and `0x12f1a4` once in
  a cycle of three (`0x12f1a0`). Each time it reaches 0 the race clock `0xed0f8` goes on by
  one (not while state `0xf6b74` = 3, paused), and on every second tick in state 1
  `0x48f6c(clock/2, 0, 2, ...)` is called. `race_clock_rate` `0x4c5f8` sets the pair from
  `0x12dd0c`:

  | `0x12dd0c` | reloads | interrupts per 3 ticks | clock rate |
  |---|---|---|---|
  | 0 | 3 3 4 | 10 | 30 Hz |
  | 1 | 3 3 3 | 9 | 33.3 Hz |
  | 2 | 3 3 2 | 8 | 37.5 Hz |
  | 3 | 2 2 3 | 7 | 42.9 Hz |
  | 4 | 2 2 2 | 6 | 50 Hz |
  | 5 | 2 2 1 | 5 | 60 Hz |

  `race_setup_record` sets 1; `race_run` sets 3 when bit 8 of `0x12da28` is set (what that
  bit is: to find). Single player: steps due `0xf6d64` = the clock, so **one `race_step`
  per clock tick**, then one frame drawn: the physics' rate is the clock's.
  **Measured in the original (DOSBox, §5, 2026-10-19): 33.31 race-clock ticks per 100
  system ticks** in a single race on City (setting 1, reloads 3 and 3), the steps done
  equal to the steps due all along, and the race time on the screen = clock / 33.33
  (1,449 ticks, "00:43.4"). The system clock runs at 100 a second. In the modes `0x15d134` = 1..3 (network, replay?)
  the steps due come from elsewhere (`0x12dd18`, `0xf6d68`).
- `race_step` `0x6348c`, in this order: the controls (`0x71cec` or `runsystemtasks`
  `0x83908`); `race_timed_calls` `0x1fa1c` (two timers at `0x129fb0` that call
  `race_pause_menu` `0x56474` / `0x63110` once their time comes); `race_check_pause`
  `0x63304`; `race_keys` `0x47c88`; then, unless the race is held (state 4, or 2 before the
  start), `race_update_cars` `0x519d8` with the step flags `0x12f1bc`, and the step counter
  `0x145c94` goes on by 2.
  - **The step counter and the flags.** `0x145c94` counts in twos (2 a step: it is
    always even and twice the race clock). The flags are 1 if `(n & 1) == 0`, | 4 if
    `(n & 3) == 0`, | 0x10 if `(n & 7) == 0`: **1 on every step, 4 on every second step,
    0x10 on every fourth**. Measured (§5, 2026-10-19, `peek`): over 10 s of a race the
    counter was never odd and ran at 2 × the clock; of 1,914 samples the flags were 1
    (910), 5 (508), 21 (496). (An earlier note here said every 2nd / 4th / 8th: wrong.)
  - The keyboard is a table of keys down by scan code at `0xf7ac8` (`0xf7ac8 + code`).
    `race_keys`: keys 1..8 (codes 2..9) put the camera on car 0..7 (`camera_set_car(0x129ecc,
    car, 4)`, `0x12dd54` = the car watched). `race_check_pause`: Esc (code 1) or P (`0x19`)
    after the first 60 steps opens the pause menu (`0x1f988`); in a network race
    (`0x15d134`) with a Shift (`0x2a` / `0x36`) held it ends the race instead (`0x15d138` = 1,
    `0x13c17c` = 2).
- **`race_update_cars` `0x519d8`** (the step's work on the cars): on flag 4 each car's
  sound routine (`+0x50e` → `0x13c1e0`); on flag 1 (every step), in state 1,
  `car_check_finish` `0x51760` for every racer, then the end-of-race test (below), then
  **every car's driving routine** (`+0x512` → `0x13c1b8`, in road order: the list
  `0x13c298`) and `car_road_offset`; then `cars_meet_world` `0x1c878`; then the camera
  (`camera_update`), and the HUD's sums.
- **Where a car is in its race:** `car_race_progress` `0x5162c` returns 1 before the start
  and 2 past the finish, 0 between. A point-to-point track (`0x13bb80` = 0) ends at road node
  `0x13c164` (`+0x48` is the car's node; below node 12 it is before the start); a circuit
  (`0x13bb80` ≠ 0) at lap `0x13c160` (`+0x50` is the car's lap). `car_past_finish_by`
  `0x51724`: how many nodes past.
- **The racers' results**, one record of `0x1d2` bytes a racer (car `+0x516` is its index)
  from `0x14ea7c`: lap times at `+0` / `+8` + lap × 4, the tickets `+0x1b4` (`0x14ec30`),
  the warnings `+0x1b8` (`0x14ec34`), **the finish time** `+0x1c0` (`0x14ec3c`: 0 = still
  racing, the step counter − 300 when past the finish, −999 = turned back past the start),
  `+0x1ce` (`0x14ec4a`) the place (counted from the times already in).
- **The end of a race: `0x13c178`** (a copy in `0x13c17c`), 0 while it runs, cleared by
  `race_init_car_kinds` and `0x506dc`. `race_update_cars` sets **1** once every racer has a
  finish time and is down to half a unit of speed (`+0x70` ≤ `0x8000`), **0x10** if none of
  those times is positive (all turned back); `police_pull_over` sets **4** (arrested,
  below). `race_run` copies it to `0x15d122` (and makes it 2 when `0x15d136` = 0, `0x12da24`
  = 1 and `0x156cc3` = 0: when that is, not yet known), and `race_from_menu` reads it: 4 →
  `fe_cop_ticket_movie` (`ticket.qfs`, `cop%d.tgv`), 1 → the winner's film and the
  standings. `0x68dcc` (the track records?) also tests it for 1 and 0x10.
- The cars' records: `0x12da30` cars, `0x18c` bytes each from `0x12cccc`.
- `race_draw_frame` `0x5bc40` ("sub:%4d cam:%c", a debug line): calls the 3D and HUD
  routines `0x59a74` `0x5a4f0` `0x5aa84` `0x5adbc` (`horz`), `0x31cb4` (`mpbd`: the map?),
  `0x3081c` (`b00t` ..: cockpit), `0x314e4`, `0x31684`.
- HUD / dashboard: `load_dash` `0x2e8d4` (`DH.fsh` / `DL.fsh`: `dash`, `tach`, `sped`, ...,
  and `%s%sDH.fmm` / `DL.fmm`), `0x2e100` (`lap#`), `0x2e1dc` (`splt`), `0x2df78` (`mile`),
  `0x30424` (the police: `ticketDH` / `tickegDH` / `warnDH` ..., the G = German),
  `0x30b64` (`tach`).
- Speech in the race: `snd_say_position` `0x501a0` (`first.eas` .. `eighth.eas`),
  `snd_say_best_time` `0x5056c` (`besttime.eas`, `bestlast.eas`, `finallap.eas`). The car
  sound banks `Supra_sw.bnk` .. (the player, by car), `O<car>mb.bnk` (opponents),
  `traffc.bnk`, `COLLSWWT.BNK` .. (collisions), `NFS_FMmb.bnk`: the table at `0x524b4`.
- Physics file: `load_car_pbs` `0x50c24` (`.PBS`, "By_R&T"); `.PDN` at `0x660bc` in
  `load_car_cfm`'s neighbour `0x65874`; the car's parts by name (`frntires`, `rertires`,
  `left_turn`, `right_turn`, `inside`, `outside`, `surface`, `convex`).
- Replays (§3.15): `save_replay` `0x3a628` ("Save replay as") copies the last race's
  `replay.rpl` under a new name through `replay_copy_as` `0x3a218` (named `load_replay`
  until 2026-10-19: it reads, checks the disk's free space, writes); `replay_file`
  `0x5c3b0` writes it at the race's end, `replay_start` `0x5dd68` allocates `UsrCtrlBuff`.
- Track names: `0x41b68` (`city coast alpine springs avalley bsienna vridge tpolis lvegas`
  and their `m` / `r` variants = mirrored? / reversed?), `0x68dcc` (four-letter: city coas
  alpi rust autu burt mont trop neod). (`0x563a4`: the pause menu's map names `cy cl al loop1 loop2 loop6 loop3 loop7 loopx`, not music; NOTES §4.20.)

### 4.4 The libraries the game leans on (read from their code)

- Memory: `allocmem` `0x828a8` (`reservemem` under a lock), `freemem` `0x82ebc`
  (`findmemblock` + `purgememblock`), `__chk` `0xa5ff4` (Watcom's stack check: `push <bytes>; call` opens every library routine; named `free` until 2026-10-19, wrongly), `memmove` `0x8ecc4`
  (stack arguments). `fatalerror` `0x92484`: prints, restores the system, exits through
  `0xf8a84`. `sprintf` `0x8198b` (stack, `__prtf`-like core `0xa23f3`).
- Time: the system clock `0xf7980` (TIMERHZ ticks). `ticksince` `0x81350`, `settimeout`
  `0x8136b`, `timedout` `0x81389`. `runsystemtasks` `0x83908`: runs every task given to
  `addsystemtask` whose time has come (`0x15db6c` routine, `+4` period, `+8` next time) and
  returns their results OR-ed. It is called before nearly everything; the keyboard, the
  sound and the music streams are such tasks (to confirm).
- Pictures: `findshape` `0x80e1d` (a shape in a SHPI by its 4-letter name, `fatalerror`
  if asked to and missing), `blitshape` `0x9d8d0` (shape, x, y, window), `drawshape`
  `0x81280`, `drawshapeatpos` `0x8129c` (at the shape's own x / y, `+0xa` / `+0xc` >> 16),
  `drawshapemove` `0x84248`. Windows: the current window is 13 dwords at `0xf889c`;
  `setwindow` `0x8118c` copies one in, `setscreenwindow` `0xba93c` puts the screen's
  (`0xf88d4`) back; `newwindow` `0x80c4c`. `waitretrace` `0xba95f` (port `0x3da` bit 8).
  The video driver's routines are pointers set by `initgraphics` (`0xf795c` show the page,
  `0xf7960`, `0xf7964`).
- Palette: `setpalette` `0x84080` (first, count, colours; out `0x3c8`, 6-bit DAC),
  `nearestcolour` `0x80c80`, `pal_clear` `0x7188c` (768 bytes), `pal_from_shape` `0x718a4`
  (palette shape type `0x24` = 8 bits a channel, else 6), `pal_fade_down` `0x7198c` /
  `pal_fade_up` `0x71918` (steps of `0x98abc` / `0x98aec`; which is in and which is out: to
  read).
- The front end's helpers: the mouse at `0xf78e0` / `0xf78e4` (`mouse_set` `0x71ad4`,
  `mouse_in_rect` `0x71a1c`, `mouse_moved` `0x71b24`), `fe_show_if_changed` `0x71afc`
  (shows the page when something changed), `fe_service_music` `0x71c20` (keeps the menu's
  music stream fed, called in every wait loop), `fe_play_sound` `0x72510`.

### 4.5 Numbers: fixed point, no floating point

- **The game computes in whole numbers.** The game code (`0x10000`..`0x80000`) holds 18
  x87 instructions in all; the physics is 16.16 fixed point, inline: `imul` then `shrd`
  by 16. **The multiply rounds**: `(p >> 16) + bit 15 of p` (`rotate2d` `0x3e918`,
  `car_road_offset` `0x51598`). So the port can match the original to the bit with plain
  integer C (64-bit products, the same rounding).
- Angles: **65,536 = a full turn**. `fixsin` `0x80b00` / `fixcos` `0x80b70`: a quarter-wave
  table of 256 entries at `0xf8448` (index = angle >> 6, the two bits above it the
  quadrant), plus a straight-line step between entries: `(cos(i) >> 2) * (((a & 0x3f) *
  0x6487e) >> 9) >> 21` (`0x6487e` = 2π × 65,536). The result is 16.16.
- `rotate2d` `0x3e918` (x, y, angle, &x', &y'): x' = x·cos − y·sin, y' = x·sin + y·cos.
- A random number generator: `0xeab70 = (0xeab70 × 0xeab78) & 0xffff`, the bits 8.. of the
  product used (in `drive_opponent_wait` `0x7e0c8`).

### 4.6 The cars

- The cars' objects: pointers at `0x13c24c` (`0x12da38` of them; the racers first,
  `0x12da30`; then traffic and police?). A second table, `0x12da30` records of `0x1d2`
  bytes from `0x14ec3c` (the racers' standings?), and the controls / players `0x18c` bytes
  from `0x12cccc`.
- A car's fields seen so far: `+0x0` `+0x4` `+0x8` a position (16.16; `+0x4` the height?),
  `+0xc` `+0x10` `+0x14` angles (`+0x10` the heading, kept in 24 bits), `+0x48` the road
  node it is on (`& 0x13bb88`, the track's node mask), `+0x60` / `+0x68` the speeds
  sideways / forwards?, `+0x70` / `+0x74` (speed; `0x960000` = 150 the cap), `+0x88` a
  pointer to the record that holds the kind, `+0x341`.. the last step's values, `+0x47d`
  the player number, `+0x50e` the sound kind, `+0x512` **the driving kind**, `+0x51a` flags
  (4 = active).
- **The driving kind** `+0x512` picks the routine called every step through the table
  `0x13c1b8` (filled by `race_init_car_kinds` `0x504c4`); `+0x50e` picks one called every
  second step (flag 4) through `0x13c1e0` (1 = `car_controls_player` `0x10400`, once called car_sound_player; 2 = nothing):

  | kind | routine | what | set by |
  |---|---|---|---|
  | 1 | `drive_player_wait` `0x7ea20` | counts down, then kind 2 | `police_pull_over` `0x79728`, `0x7e64c` |
  | 2 | `drive_player` `0x382d0` → `car_physics_player` `0x408e0` (4.4 KB) | the player | `load_car_pbs` |
  | 3 | `drive_opponent` `0x7b34c` (3 KB) | a computer's car | `load_car_pbs`, `0x51200`, kinds 5, 6 |
  | 4 | `drive_crashed` `0x1cfe0` | the crash: the car tumbling as a box (below) | `0x1ca9c`, `0x1cb14`, `car_car_impact` `0x1e314` |
  | 5 | `drive_kind5` `0x7d9e4` | slows by `0x13333` a step (`0x23333` with flag 4), runs the opponent's routine | `car_set_drive_kind` `0x1cd40` |
  | 6 | `drive_opponent_wait` `0x7e0c8` | held at (8, 200 + 10n, 8), counts down, then kind 3 | `0x7ccc0` |

  After its routine, every car goes through `car_road_offset` `0x51598` (its place
  across the road: the road node's position `0x137430 + node × 0x24`, turned by the node's
  angle).
- `car_physics_player` `0x408e0`: `0x3ea98` / `0x3e9c4` every second step (flag 4; the gearbox?),
  `0x3ed68` (the engine: reads `Carspecs` through `0x18a04`), `0x3f260` twice (front 0 and
  rear 1, from `+0x60` / `+0x68`), `0x3f7b8` twice (the axles' forces?), `0x4071c`,
  `0x40230`, `0x4008c`, `replay_mark_highlight` `0x5c6d4` (events 0x28, 0x57, 0x78),
  `race_stats_max` `0x2d71c` (the race's greatest values, capped at 1200).
- `replay_mark_highlight` `0x5c6d4`: for each slot of 60 half-steps (`0x145c94` counts 2
  a step) keeps the greatest event and its time (`0x144da8`): the replay's highlights
  ("Load highlight ?"). 60 half-steps = 0.9 s at the measured 33.3 steps a second (1 s at
  setting 0's 30: perhaps what it was made for).
- `car_check_lap` `0x5056c` (called by the opponents too; speaks `besttime` / `bestlast` /
  `finallap` for the player).
- **Making the cars** (`cars_create` `0x51388`, from `race_main`): for each of the
  `0x12da38` cars a record (`+0x516` / `+0x47d` its number), `car_setup` `0x51200`
  (racers: `load_car_pbs`; the others: driving kind 3, `+0x48a` from `0x12da53 + 0x4b·n`)
  → `car_reset` `0x50830` (everything cleared: position (1, 0, 1), the shakes `+0x427..`
  0, `+0x88` = itself, `+0xc0` = `+0xc4` its body, `+0x39d` = 10·n), then
  `car_setup_body` `0x512e8` (the PDN); `race_sort_order` `0x518c0` sorts the order
  tables `0x13c298` (by node, then `+0x39d`) and `0x13c274` (by `+0x4c`). `0x506dc` is not
  the grid: it clears the laps and times (`0x13c160..`, the standings `0x14ec18 + 0x1d2·n`
  by `0x685e4`).
- **The starting grid: `car_grid_place` `0x7c938`** (`port/src/car.c`; for the
  `0x12da30` racers at the end of `load_traffic_cfg` `0x7de84`). With g = `0x12dca0[+0x47d]`
  (the grid slot: 7 the player, 6 the opponent on CY1) and w = the lane width at node 10 (the
  table `0x15d15c`, made in `0x7de84`: row n of 40 shorts holds i / n in 8.8 for n = 1..3,
  read at row (node +4 & 15) [node +1 >> 3]) << 8: node = 32 − (`0x12da2c` > 2 ? 2g : g/2·4),
  x = −w/2 + (g % 2)·w (+ the node's x and its heading as the yaw when `0x12da12` > 2),
  one more lane left (x −= w) for even g when node 20 has 2 lanes left and 1 right; `+0x39d`
  = 0, z = the node's z. CY1: the player at (2.5, 0, node 20's z), node 20; the first step's
  `track_follow` makes it 19.
- **`track_follow` `0x4f534(pos, &node)`** (`port/src/track.c`): the midpoints m(n, n+1)
  of the nodes (x and z, sums `>> 1`); d = ((pos.z − m.z) >> 8)² + ((pos.x − m.x) >> 8)²
  in 32 bits, compared unsigned; one node ahead while d(n+1, n+2) < d(n, n+1), else one
  back while d(n−1, n) < d(n, n+1) (only for n > 0 or on a circuit), until it stays; then
  masked by `0x13bb88`; returns 1 if the node changed. −1 stays.
- Both checked by `tools/camera_emu.py` (every racer's place; every car's node from 12
  behind to 12 ahead, 149 to 193 searches a snapshot): equal on all 6 snapshots.

#### The player's car driven (2026-10-19; ported, `port/src/drive.c`)

Checked by `tools/drive_emu.py`: the original's own code run in Unicorn on the 11
snapshots against the port's, step for step, every field of the car and every global
compared after each step. 297 runs (18 variants × scripted keys: gas to 57 m/s, weaving
into the walls, the handbrake, the manual box, reverse, the joystick) all equal.
`--coverage` lists the original's instructions never run.

- **A step** (`race_main`): the flags from the step counter `0x145c94` (2 a step): 1
  every step, 4 when it is a multiple of 4, 0x10 of 8. On flag 4 the controls
  `car_controls_player` `0x10400`, then `drive_player` `0x382d0` =
  `car_physics_player` `0x408e0` + `car_check_lap` `0x5056c(car, 1)` + `car_on_road`
  `0x38808(car, 1)`, then `car_road_offset` `0x51598`.
- **The keys** are sampled by the timer at each 100 Hz tick into a ring (`0x48f6c` →
  `0x48bec` → `0x4918c`); `controls_read` `0x49248` gives one word a step. `0x48bec`:
  steering left alone 0, right alone 0x3c (swapped on a mirrored track), both or none
  0x20; pedal 5 accelerating, 0x3c braking (the brake wins), else 0x1e; gear up bit 1,
  else gear down bit 2; B+R+W (scan codes 0x30, 0x13, 0x11) with the automatic both;
  handbrake bit 8, horn bit 4; word = bits << 12 | pedal << 6 | steer. Both 0x3000 bits
  set = held at full revs (`0x12ccac[p]` = 1, the bits cleared). The keys' defaults
  (`0x12d92c`.., copied from the settings `0x15cb4e`..): left 0x4b, right 0x4d,
  accelerate 0x48, brake 0x50, up 0x1e (A), down 0x2c (Z), handbrake 0x39, horn 0x23 (H).
- **The gearbox** (`car_shift` `0x48714`): the automatic's lever `+0x3e9` 1 reverse, 2
  neutral, 3 drive (0 = by hand). **Every car starts in neutral** (`car_reset`, gear −1):
  the gear-up key puts the automatic into drive; nothing else writes `+0x3e9` in a race
  (the other writers: the crash and reset paths `0x1cfce` `0x1d536` `0x1db40`,
  `load_car_pbs`, the replay `0x5cf95`). Before step 300 (the lights) a request is only
  remembered (`+0x3d5`), never acted on. `0xec5d0` is written 0x100 or 0, always 0 in fact
  (its condition's register is never set).
- **`car_physics_player` `0x408e0`**: a step is 1/30 s (`+0x506` = 0x888, `+0x50a` = 30);
  with two humans and `0x12dd10` set, the leader's step is shortened by half its lead
  (`+0x4c`), up to `0x12dd10`. Then TCS/ABS; on flag 4 (or in neutral) the revs
  `0x3ea98` and the automatic's choice `0x3e9c4` / `0x188a0`; the engine `0x3ed68`
  (torque `0x18a04`: points every 200 rpm; NITRO and the full-revs start); the drive and
  brake split front/rear; the drags `0x3f260`; the axles `0x3f7b8` (locked `0x3f56c`,
  rolling `0x3f60c`); the weight shift and the grips; `0x4071c`; the integration, the
  world speed, the road-frame slip table `0xec320` / `0xec36c`; the body's lean; the yaw
  torque (another for the rally); the road's edges `0x40230` (`0x401d8` how far the car
  reaches, `0x3fed4` the wall); `0x4008c` the node's kind; the race's top speed.
- **`0x4071c` reads the node's +0x12 word** (its slope, high half << 10, signed), of this
  node and the next: the grip changes with the slope's change. Not the heading +0x16
  (the first port's mistake: the runs parted at step 287).
- **The wall** `0x3fed4`: a glancing touch turns the car off it; a hard one goes to
  `0x78204` → `0x1cb14`, the crash (driving kind 4, the tumbling body at `+0xc4`; below).
  The hard hit: across the wall ≥ 24 m/s (`0x180000`, 40 in the rally) and `+0x70` over
  35 m/s; s = v − 6 (at most 14), a = s/4 about the up axis, b (the hit's side) and
  q = (`+0x5c`'s whole part) · s / 64 · 3/4 across, signs by the wall's side.
- **`car_on_road` `0x38808`**: gravity g = (dt >> 2) · 0x9cf5c >> 14, and g / 2 more
  unless the options word `0x12da28` (the settings' `0x15d12a`) has 0x20 — softer
  landings after a long jump, except on "tr6" (strcmp `0x83f40` with the track's name
  `0x12da16`); in the air and landing (`+0x401` the steps in the air); on the road the
  slope's push (`fix_sin(pitch >> 14) >> 8` · 2511, by 0x2666 while rising) and the
  car tilted a quarter of the way to the road (`0x3871c`); a landing shakes the dashboard
  (`0x12ba24`).
- **`car_check_lap` `0x5056c`**: the finish of an open road `0x13c164` = the head's short
  at +6 × 4 − 0xb5 (CY1 1739); of a circuit its length × 4 (`0x13bb80` the loop flag,
  `0x13bb8c` the length). Past the finish, or before the start, the car drives itself
  (`0x10180`, towards the road six nodes ahead, `0x10010`).
- **Loading** (`load_car_pbs` `0x50c24`): SIMDATA/CARSPECS/<`0xede84[number]`>.PBS
  (unpacked); the sum of bytes [0, 0x758) must equal +0x774, else +0x3c = 0; the rally's
  scaling; `car_torque_load` `0x18990`: +0x3c × (automatic ? 0xf8 : 0x100) / 256, each
  torque point at 0xb8 + 8i = mulr(mulr(mulr(t << 16, +0x5c), mulr(+0x3c, +0x64)), +0x14);
  ABS and TCS from the player's choice and the car's +0x334 / +0x338; the downforce 0; the
  gearbox 2 for the automatic, 0 by hand; `physics_setup` `0x3fb90` (the axles' weights
  and grips); `car_reset` `0x50830`. The player's record `0x12da48 + 0x4b · slot`: +2 the
  name, +0xb the car, +0xf automatic (= CONFIG +0x8d), +0x13 TCS (= CONFIG +0x8f == 0),
  +0x17 ABS (= +0x99 == 0) (read off the snapshot).
- **The car's size** (`car_setup_body` `0x512e8`): half its length 2 · PDN word 8, half
  its width 2 · word 0, when words 0, 4, 8 are all set (SIMDATA/CARFAMS/<name>.PDN, the
  name from `0xf6b10` / `0xf6ca0` / `0xf6e70`, no M); else the CFM model's extents
  (+0x64a / +0x64e: the traffic's, whose PDN has none; ported, `crash.h`).
- `0x8bb7c` is EA's arctangent: atan(x = the first stack argument, y = the second), 1024
  a turn from the y axis (`ea_atan_1024`, `exe.c`).
- **Not yet**: a circuit's laps counted (CY1 is open); the acceleration test's
  figures; force feedback and the sounds (`0x52d2c`, `0x52e40`, `0x52ea8`); the replay
  marks (`0x5c6d4`) and the race's greatest values (`0x2d71c`); the police and the other
  cars pushing (`+0x3d9`..). `0x3ecc1` is never reached.

#### The crash (2026-10-19)

A hard wall hit throws the player's car about as a box until it lands on its wheels or,
its wait over, is put back on the road. Ported in `crash.c`; checked by
`tools/drive_emu.py` (the original's code in Unicorn, step for step, every field, the
crash globals, the smoke): **396 runs equal on all 11 snapshots**, 9 of them thrown at the
walls (`thrown right/left/spinning/up/rally/manual/circuit`, the car's world speed set at
the start) with tumbles of 48–594 steps, landings and a put-back counted.

- **The body** (`+0xc4`, `0x84` bytes): the turn `m[9]` (rows: across, up, along; z
  flipped against the car's), the place (lifted to the box's middle), the speed, the spin
  (radians a second, 16.16), the mass, the turn's resistance, their inverses, the radius,
  the half sizes (x, y, z), and the wait (steps). Set from the PDN by `0x2c920` (called from
  `car_apply_pdn` `0x1e9cc`); the car's half height `+0xbc` and reach `+0x84` with it.
- **The start**: `car_hard_hit` `0x78204` (car, a, b, s) → `car_crash` `0x1cb14`: the
  body from the car (`0x1cbb0`), `+0x4f2` = 1, kind 4, the smoke cleared, wait 300; then
  the hit as a spin s·across + b·along − a·up. A police chase is dropped (`0x781ac`).
- **A step** (`drive_crashed` `0x1cfe0`): the lap check (a new node → the road's frame
  `0x7dbd8`: across, up, along, the node's point); the body moved (`0x2c9c0`: a 64th
  slower, gravity `0x53b6` a step, turned by its spin through `0x2c894`, a turn about an
  axis); three contacts (`0x2a334`): the nearer wall (the node's +2 / +3 lanes × 0x2000,
  plus `0xea5c8` or `0xea5cc` by whether that side has lanes or the node is kind 5), a
  tunnel's roof (node kind 4, 7 m up), the road. A contact finds the box's deepest corner,
  pushes it out along the surface's normal (`0x2a918`: restitution `0xea684`, friction
  `0xea688`; **its second cross product reads a vector it has already overwritten**, kept
  in the port) and, with `0xea678`, lifts it out by the energy. `0xea66c` the push,
  `0xea670` the depth, `0x12b4c4` the corner's speed.
- The player's car meanwhile: its angles the body's (`0x2cb78`), the pedals off, the revs
  down 200 a step, the gearbox out of gear (the automatic in neutral). Nearly still and
  flat or on its side (`0xea5a4`, `0xea5a8`, `0xea5ac`): speeds halved. The smoke
  (`0x1464d0`, 100 puffs: `0x64588` clear, `0x64804` let out, `0x64978` rise and age) once
  the body is slower than 8 m/s (`+0x4a5` bit 2). `0x145c98` = 0x3c: the camera's crash
  view (`camera_check_mode` `0x6324c`, §4.7); the smoke drawn with the car (§4.10).
- **The end**: on its wheels and along the road (up · up > 0.966, along · along > 0.7) and
  still → `0x1ceb4` (kind 2 again, `car_set_drive_kind` `0x1cd40`); else the wait runs
  down (faster when still); at 0, unless another racer faster than 9 m/s is less than a
  second behind, `0x1d8a8` puts it back: the middle of the node's first lane, upright along
  the road, still, in neutral; `0xf6b70` = 1, `0xea700` = 0x1e, and **a crash counted**
  (results +0x1bc, `0x14ec38` + 0x1d2 · player) when `0x13c168` = 1 (a race).
- The tail of every step: the tyres' screech from the corner's speed, the hit's sound
  level (`0xea5d4`, held 10 steps by `0xea5d0`), the car's place from the body, the turn
  squared up every 4th step (`0x2c584`).
- **`cars_meet_world` `0x1c878`**, the part the port has: the hit's timers, and every 4th
  step `traffic_police_tick` `0x7a040` → `0x786d4` for the players' cars: the road's frame,
  the place across `+0x80`, the reach `+0x84`, the speed along the road `+0x15c`
  (`0x7b130`, `0x1e6cc`), the flags `&0xbe1c | 0x1e0`.
- **The x87 at double precision**: the game sets the control word 0x127f at start-up
  (`0xa8c95` loads `0x128b08`). `0x3e7fc` (1/x) gives other last bits at single precision:
  Unicorn starts with 0 (single!), so `snap_emu.py` sets 0x127f.
- **Not ported**: the other cars' crash (`0x1de8a`: `0x7bf94`, stood up `0x7daec`, kind
  5); the other cars put back with the player (`0x7c030`); the sounds (`0x53214`,
  `0x531dc`, `0x52e40`); the replay's mark; the dust's pictures (`smo1`..`smo4`,
  `dir1`..`dir4` of SMOKE.QFS, `0x646f0`). **Not reached by the runs**: the tunnel's roof, the `0xea5c8` wall branch,
  the fast landing (`0x1de73`), the large-vector halving in `0x2c2d8`/`0x2c3d8`/`0x2c490`,
  the smoke's index wrapping.
- In the port, `RENFS_THROW=s:x,y,z` gives the car a speed at step s to see a crash (a
  hard hit is rare driving CY1 normally: the hardest seen 19 m/s across).

### 4.7 Drawing a frame

- `camera_update` `0x136b0` (also in the step), `camera_set_car` `0x13530`
  (`0x129ecc` = the camera, a car, a mode: 3 seen). **The camera is the only one, at
  `0x129ecc`**: the decompiler shows its fields as globals (`0x129f04` is its `+0x38`, the
  node; `0x129f08` its `+0x3c`, the flags).
- **The camera (`port/src/camera.c`; the driver's modes 0 and 1, checked by
  `tools/camera_emu.py`: equal on all 6 snapshots, CY1 in-car, d3 with the body leaning
  and shaking).** Its record: `+0` mode, `+4` the mode wanted, `+0xc` frames left of a
  move, `+0x10` position, `+0x1c` the next position, `+0x28` → the car's position, `+0x2c`
  the eye from the car turned, `+0x38` node, `+0x3c` flags (1 the node changed), `+0x40`
  angles (pitch, yaw, roll), `+0x4c` the next angles, `+0x58` → the car's angles (its
  `+0xc`), `+0x5c` tilt (added to the car's angles), `+0x68` the mirror drawn, `+0x6c` the
  eye in the car, `+0x78` the yaw followed, `+0x88` / `+0x8c` the views, `+0x90` the car.
  The modes' settings `0xe80f0`, 0x58 bytes each, 14: `+0` the view (`0xe8de8 + 32·it`:
  0 the 85° one, 1 the 96° `0xe8e08`), `+4` the eye (mode 0: (−0.4, eye, 0), the eye the
  car spec's `+0x36c`, put into modes 0 and 10 by `0x147b2`), `+0x10` → the angle followed
  (set each frame), `+0x18` how it follows, `+0x2c` / `+0x30`, `+0x38` the mirror, `+0x3c`
  (0: the eye taken from `+4`), `+0x40` the mode next, `+0x44`, `+0x48` an eye to start
  from, `+0x54` (use it). `camera_set_target` `0x13450(cam, pos, angles, car, mode)`
  copies them in, clears `+0x2c` and the tilt, node = the car's, `+0x78` = the car's
  node's heading if the car is another, then `camera_set_mode` `0x14b00(cam, mode)`:
  modes < 4, 10, 11 become 13 unless `car_upright_on_road` `0x14de8` (the yaw within 30°
  (0x1e0000) of the node's heading, |pitch| and |roll| ≤ 0x240000); with `0x15d13a` or
  `0x145c90` 0 → 1 and 10 → 11; the views, `+0x68`, `+0xc`, the eye; tilt 0;
  `camera_update`; `view_set_layout`. **Modes 0, 1 in `camera_update`**: tilt pitch =
  `0x12ba20` + (`0xea70c` (1.0, never written) >> 8)·(car `+0x369` >> 8) −
  `shake_wave(car +0x42d, car +0x428)`, tilt roll = (`0xea70c` >> 9)·(car `+0x365` >> 8) −
  `shake_wave(car +0x429, car +0x427)` (`0x3e900`: the wave `0xec28c` (0, 0, −1, −2, −2,
  −1, 0, 1, 2, 3, 4, 5, 5, 4, ..) at the phase times the amount, / 16 truncated);
  `camera_turn_eye` `0x13544(eye, out, pitch, roll, yaw)`: the eye >> 8 turned by
  X(−pitch)·Z(−roll)·Y(−yaw) (`0x83a54`), << 8; position = car + that; the node and its
  flag from the car's; `+0x78` = the car's yaw; the eye from the mode (so the eye turned is
  the last frame's). The end of every mode: angles = (the car's + tilt) & 0xfffc00, the
  position and angles taken from the next ones; modes > 1 add `0xf6b8c` to the roll.
- **The modes outside the car** (2026-10-19; `camera.c`, checked by `tools/camera_emu.py`:
  1280 steps outside on each of the 11 snapshots, modes 2..5, 8..13, the crash thrown
  and put back, all equal). The table's fields by name: `+0` the view's kind, `+4` the eye,
  `+0x14` how far ahead the point looked at is, `+0x18` how the eye follows (1 across
  with the car ×1.5, 2 ×½, 3 round to the car's yaw and turned with the car, 4 round to
  the road's heading), `+0x2c` a shift, `+0x30` divide (each update 1/shift of the way
  instead of 1/2^shift, `0x13630`), `+0x3c` the frames a move takes, `+0x40` the mode
  after it (−1 none), `+0x44` where the eye's z ends it, `+0x48` / `+0x54` the eye to start
  from when taken from the driver's seat (not in a crash, the player's car upright).
  Mode 13 (the crash): kind 1 (96°), eye (0, 2.8, −10), the point 12 ahead, follows the
  road's heading by 1/16 a frame. Mode 10 the way back into the seat (follow 3, then 0),
  11 into 1. **`0x138e3`..** (modes 2..5, 8, 10..13): the camera put at the car when more
  than 16 nodes off; the road's way from the nodes 12 either side, weighted by the car's
  `+0x39d` (0..0x60000 along its node); the point = the car + that way made 1 long
  (`0x2c1c0`) × ahead; the eye eased and the yaw followed (`0x3e7ec` = `ea_atan2(z, x)` <<
  8), the eye turned (by the car with follow 3; for mode 13 pitched towards the point;
  else flat), raised by the slope under the car (halved downhill), held at most 6 above
  the road (`0x14c9c`: the plane through the node, the next and the across vector, from
  the **next** node's point) while the car tumbles, and kept inside the road's edges
  (`+2` / `+3` lanes × 0x2000). Mode 9 is fixed to the car. Modes 6 and 7 (from the car
  ahead, from the police car) not ported.
- **`camera_check_mode` `0x6324c`**, each step after the splits (then `camera_update`):
  `0x145c98` counted down; a mode ≥ 4 that is the one wanted (not 10) stays; the car
  watched not upright on its road (`0x14de8`) → `0x145c98` = 60 and mode 13 (from 0..3,
  10, 11); upright again and the 60 steps over → back to the mode wanted (0 by 10, 1 by
  11). `camera_set_mode` `0x14b00` then sets the view (`0x14210`, the layout for the
  detail) and `0x143e00` = 1: the screen round the view drawn again (§4.11).
- `draw_road_sections` `0x5aa84`: the track **from far to near** (a painter's order), a
  section = 4 road nodes (`0x13bb90` the sections' mask); per section 11 points, 3D at
  `0x13b728` (12 bytes each, `0x84` a section) and on the screen at `0x13b9bc` (8 bytes,
  `0x58` a section); its faces (11 bytes each: four point numbers, a texture number,
  flags) drawn by `draw_quad` `0x35b04` with the texture from `0x140cd8`; then the objects
  and the cars behind at that row: `cars_behind_at` `0x68430` (→ `0x66d08`). The section's kind
  is `0x136621 + section × 6` → a record of `0x4c` bytes at `0xf61c0`.
- `0x5a4f0` is the road ahead (the view); `0x5aa84` the mirror's. Read and ported: see
  "Painting the road" below.
- `draw_horizon` `0x5adbc` (`horz`), the HUD and dashboard `0x314e4` / `0x31684` /
  `0x3081c` (`b00t`.., the cockpit) / `0x31cb4` (`mpbd`, the map), `0x59a74`, `0x5868c`.

#### Painting a textured quad (2026-10-19; ported, `port/src/raster.c`)

- The filler pointer `0x12c0dc` = `0x34ed8` (set by `load_vis` `0x58bc4`; the other
  setters of `0x12c0d8..e4` are a debug `.vis` recorder). It calls `0x91360(shape, uv[4],
  xy[4])` (stack arguments): uv in 16.16 picture pixels, xy whole screen pixels, corners in
  order round the quad. A shape of type `0x45` goes to `0x90b30` instead (with its type
  set to `0x7b` for the call): the same routine with the row painter `0xa7610`, which
  leaves colour 255 out. **Type `0x45` is made at load** by `0x5b270`: every shape of a
  track's SHPI that holds a 255 and whose name does not start with `!` (all are `0x7b` on
  the CD).
- `0x91360`: the top corner = the first with the least y, the bottom = corner 2 unless
  another has a greater y (strictly). Which way round from the two edges' slopes at the top
  (`(dx << 16) / (dy + 1)`, the next edge's ≥ the previous one's → +1). The left and right
  sides walked from the top to the bottom corner into tables by absolute row (x, u, v each
  side; 0x300 rows at most, `fatalerror` over): `0xa7790` (left x), `0xa7844` (right x),
  `0xa78e4` / `0xa79a4` (u, v): slope `(x1 − x0 << 16) / n` (n = dy + 1, the end row
  included), a half step in on the first row (left: when the slope < 0, right: > 0; u/v
  by the x slope's sign), cut to the window `0xf88a4..0xf88b0` (x0, y0, x1, y1: ends
  outside). An edge going up (n ≤ 0) is skipped: the table keeps what was there (the
  original's stack).
- Each row: n = right − left + 1; **n ≤ 0 → once, the way round flipped and the sides
  built again, the same row again; a second time ends the quad** (`0x91917`). Rows outside
  the window's x are skipped; du = (u_r − u_l) / n, dv likewise (before the clipping), the
  left cut moves u, v by du, dv per pixel cut. The row painter `0xa7a4c` (`0xa7610`):
  source = picture + (v >> 16) × row bytes + (u >> 16); the fractions in 16 bits each and
  their carries step the source by a row and a pixel (`add bx,dx` / `adc esi,[step +
  row?]`): exactly u + k·du, v + k·dv, no wrapping. Row bytes `shaperowbytes` `0xa61f0`
  (bits a pixel from `0xf9888[type & 0x7f]`, 15 → 16, × width, rounded up to bytes);
  the screen's bits a pixel `0xf88b4`, its rows `0xf88c4` (a table of row addresses) +
  `0xf88c8`.
- `tools/raster_emu.py` runs `0x91360` / `0x90b30` in Unicorn against the port's
  `raster.c`: **equal on 22,246 random quads** (convex, crossed, clipped, see-through;
  2026-10-19).

#### Cutting a quad: `draw_quad` `0x35b04` (read, 2026-10-19)

`draw_quad(shape, ushift, level, uindex, vindex, P[4] (3D point pointers), S (4 screen
points, 8 bytes each), near)`, Watcom registers then the stack; `0x35a98` is its helper
that packs the four pointers and screen points of a child.
- Nothing when near > P2.z and near > P1.z.
- **Cut into four when P3.z < `0xeaae8[level]` >> `0xf6aa4` and level < `0xeaad8`**,
  else painted whole: `0x3521c(shape, ushift, level, uindex, vindex, S)` gives the corners'
  picture positions: w = width >> ushift, h = height >> level, u0 = ((1 << ushift) − 1 −
  uindex) · w (mirrored), v0 = ((1 << level) − 1 − vindex) · h, u1 = u0 + w − 1, v1 = v0 +
  h − 1, corners (u0,v0) (u1,v0) (u1,v1) (u0,v1) in 16.16; painted only if w > 1 and
  h > 1, through `[0x12c0dc]`.
- The cut: midpoints m01, m23, the centre c = mid(m01, m23), m12 (each coordinate
  `(a + b) / 2` toward zero), from the unclipped points. A midpoint nearer than `near` is
  moved for its projection only: t = (near − z) / `0x143e5c` in 16.16 (two divisions:
  whole, then the remainder << 16), x += t · `0x143e54`, y += t · `0x143e58` (rounded
  multiplies), z = near. The children (ushift + 1, level + 1): (m01, P1, m12, c) at
  (2u + 1, 2v + 1), (c, m12, P2, m23) at (2u + 1, 2v); then, **unless m23.z < near and
  m01.z < near**, m30 and (P0, m01, c, m30) at (2u, 2v + 1), (m30, c, m23, P3) at (2u, 2v).
- **The view's own cutter `0x35348`** (same arguments; `0x352dc` its helper) is the one
  for the road ahead (`0x5a4f0`); `draw_quad` draws only the mirror (`draw_road_sections`
  `0x5aa84`). Differences: nothing when near > P3.z and near > P0.z; cut when `0xeaadc[level]`
  >> `0xf6aa4` > P1.z **or** > P2.z (and level < `0xeaad8`); the midpoints m01, m23, m03,
  c; the children (P0, m01, c, m03) at (2u + 1, 2v + 1), (m03, c, m23, P3) at (2u + 1, 2v),
  then unless m23.z < near and m01.z < near, m12 and (m01, P1, m12, c) at (2u, 2v + 1),
  (c, m12, P2, m23) at (2u, 2v). A whole piece is painted only if `0x91cee(4, S)` & 7 = 0:
  the outcode of all corners together (1 left of x0, 2 at or right of x1, 4 above y0,
  8 at or below y1; **compared unsigned**, so x < 0 counts as right, and a piece below the
  window is still painted). `0xeaad8` = 2, `0xf6aa4` = 11, both tables 6.4, 4.8, 3.8.
- Each view has a record (camera `+0x88` the road ahead, `+0x8c` the mirror; `0x1486c(cam,
  mirror)` makes one current: `0x143dfc`): `+0` the field of view in degrees (85 ahead,
  96 a second view `0xe8e08`, 70 the mirror), `+4` the near plane, `+8` its layout
  (`0xe8b10` + 0x16c · resolution + 52 · `+8`, copied to `+0xc` and kept in `+0x18`),
  `+0x10` its window **as the layout's `+4` says** (`0x14630` hands that to `0x14154`: the
  program file's `+0x10` is not used; 0 ahead, 12 the mirror at 320x200),
  `+0x14` its reciprocal table (made current by `0x83ae0`, which changes only `0xf7a14`),
  `+0x1c` → the window. See "The views" below.
- **The views (`port/src/view.c`, checked by `tools/view_emu.py`: equal on all 6
  snapshots, 2026-10-19).** The windows `0xe85c0`, 17 a resolution (`0x2a8` bytes, 40
  each): +0 id, +4 squash (0xd999 or 0x1b333), +8 / +0xc the lens's centre moved, +0x10 /
  +0x14 what the lens covers (320 x 200), +0x18.. +0x24 the part of the screen painted.
  Layouts `0xe8b10` (+ `0x16c` a resolution, 52 bytes each): +4.. the windows of its
  modes. At the race's start (`views_init` `0x14630`'s end, `view_heights` `0x145d0`):
  windows 0 and 1 get the height `0xe8e4c[0x90·res + 12·car]` (car = its `+0x48a`; 127
  for the 512TR at 320x200), window 2 half; and the first layout's three windows of
  each resolution get +0xc = h · (spec `+0x370` − 108) / 200 (at 640x480 the difference
  · 480 / 200, then · h / 480): the car's PBS `+0x370` is its driver's horizon (87 the
  512TR: −21). `view_setup` `0x14154(rec, mode)`: rec +0x10 = mode, +0x1c = the window;
  the sky's tables (`0x49a78`: w, fov/2, short((fov<<8)/360·5), squash·w/320); the lens
  `0x14284` → `0x83c18` (origin x + dx, y + dy) and `0x83c5b`: half = fov/2 in a
  1024-a-turn (`<< 10 / 360`), centre `0xf7a28` = origin + w/2, `0xf7a2c` = origin + h/2,
  focal `0xf7a30` = cos·(w/2) / sin (the table `0xf8448`, `tab_sin` `0xa376c`, unsigned),
  `0xf7a34` = −cos·(h/2) / sin (only the dividing projector `0x83d4f` reads it); then
  `0x83af4` the reciprocals from `0xf7a30` into the table current. **One table for all
  views**: `0x14630` gives the mirror `0x128ec0` and the others the table current before,
  but it runs twice at a race's load (the second time `0x128ec0` is current), so all three
  share it, and the first frame's `view_set_mode` `0x14568` (`0x143e00` set) rebuilds it
  for the view ahead (focal 178): the mirror projects with that table, its own focal (46)
  only in `0xf7a30`. **Each frame after the view changed** (`race_draw_frame` `0x5bdaa`,
  after `view_select`): `0x14568(current, 0)` (the layout's 1:1 window whatever the
  detail; `0x143e04` kept) and the screen round it (`0x3081c`), `0x143e64` = 1; the next
  frame `0x14568(current, 0x143e04)` with `0x143e00` kept (`0x5bd71`); `0x143e00` is
  cleared at the frame's end (`0x5c137`: 1 again if it was 999). So whoever's view was set
  up last (`0x1440c` ends with the outside's), the table is the view ahead's again before
  anything is projected: measured in DOSBox after the pause menu (focal 178 with the
  pixels doubled at 640x480). Every `0x14210` marks the view changed. Ported 2026-10-20
  (race.c; the port's pause menu had left the outside's table). `view_select` `0x1486c` sets the centre and focal again; for the view
  ahead it adds `0x129ec0` / `0x129ec4` (set only by `0x14860`, which nothing calls) and
  shifts by `0xf456c` (never written): both nothing. **The camera's matrices**
  (`camera_matrix` `0x5868c`, after `0x1486c(cam, 0)`): with X, Y, Z the turns by the
  camera's `+0x40` (pitch), `+0x44` (yaw), `+0x48` (roll) (0x1000000 a turn, `>> 8`
  into EA's sin `0x80b00`; X = [1 0 0; 0 c −s; 0 s c], Y = [c 0 s; 0 1 0; −s 0 c], Z =
  [c −s 0; s c 0; 0 0 1]), S = diag(1, the current window's squash, 1), F = diag(±1, 1,
  1) (−1 on the mirrored tracks `0x12dd28`): camera +0x94 = Y·Z·X·S·F, +0xb8 (the mirror)
  = Y·Z·X·Y(half a turn). `mat_mul` `0x839f8`: each product `>> 16` on its own
  (truncated), summed in 32 bits.
- `port/src/poly.c` (projection, both cutters, the corners, the outcode in `raster.c`) is
  **checked by `tools/poly_emu.py`**: the original's cutters caught inside a real frame on
  a snapshot (every top-level call, the globals at that moment) and every piece they
  paint (picture, uv, xy) against the port's given the same quads: **equal on 663 pieces
  in 5 snapshots** (standing, and driving: 2026-10-19).
- **The road into the view: `0x59a74`** (once a frame, before the drawing). The near
  plane = `0xf6aa8` (62259) >> 11 = 30, kept in the view's `+4` (the `near` of the
  cutters). With t = (−camera position) >> 11 (camera `[0x143df4]` +0x10..0x18):
  `0x94db8(n, src, shift, t, dst)` dst = (src >> 11) + t for the `0x13bb78` road points
  `0x137434` → `0x142578`, and (if `0x1365d8` > 0) the `0x1365c0` other points `0x12f540` →
  `0x13d8f8`; `0x94e10(n, src, M, dst)` turns them by the view's matrix `[0x143df8]` (9
  ints) into `0x138c58` (and `0x131f70`) with `0x83a54(p, M, out)`: out.x = (x·M0 + y·M3 +
  z·M6) >> 16, out.y with M1 M4 M7, out.z M2 M5 M8, each sum in 64 bits, no rounding.
  EA's `memmove` `0x8ecc4` takes **(src, dst, n)**: the turned points copied back to
  `0x142578`. Then `0x587b8(node / 4)`: the near plane's direction = point k − point 0 of
  `0x138c58`, k = that section's points across (`0x136624 + 6·sec`, a word); |z| < 0x1999
  → z = 0x1999. Then of `0x142578`: the first n − 11·(`0x136618` − 3) points held to |x| ≤
  `0xf6ab0` (5376, a quarter when z < 2·near) and |y| ≤ `0xf6ab4` (5376) and moved onto
  the near plane, the rest onto 8 × near. Projected by `0x94c80(n, src, dst, 16)` (=
  `0x83b5c` with the starting shift an argument) into `0x13a47c` (and `0x131f70` →
  `0x1349a0`). A section's record `0x136620 + 6·sec`: +0 its kind (byte), +4 its points.
  **Ported (`port/src/road.c`), equal to the original on all points of 5 snapshots
  (`tools/road_emu.py`).**
- **The frame's points: `0x50070(camera)`** keeps the ring round the camera's section
  (`0x4ee30`) and, **only when the section changed** (`0x136610`, the node gathered at, /4
  ≠ the camera's, or < 0; `0x5004c` sets it to −1 to force it), `0x4ffbc(node, node/4,
  0x136618)`. (Then the cars' part, `[cam+0x3c]`, `0x4fed8`: not read yet.) `/4` is C's,
  towards 0.
  `0x4ffbc(node, sec, drawn)`: `0x13660c` = base = node/4, `0x136610` = node, `0x13bb74`
  (filled) = `0x13bb78` (count) = 0; `0x59998(base & mask)`; then `0x4f7d4(base, sec++, 4,
  1)` drawn − 1 times, `(base, sec, 4, 0)`, `(base, sec + 1, 1, 0)` (the far edge: one row).
  `0x59998(s)`: the first placement (16 bytes each, `[0x13bb94]`, `0x1365dc` of them) at
  node 4s by halves (lo 0, hi n−1; t > node → lo), then back over the run of equal nodes
  **and one more** (the first test always matches), then on while t > node and < n−1:
  `0x1365cc`, `0x1365d0` = next, `0x1365d4` = its node; `0x1365c4` (filled) = `0x1365d8`
  (objects) = `0x1365c0` (points) = 0.
  `0x4f7d4(base, sec, rows, objects)`: the detail kind = int `0xed284[node byte +7][sec −
  base]` (8 rows of 32: by the node's type and how far ahead), its record `0xf61bc +
  0x4c·kind`; at +0x18 a list for each row of the point numbers (0..10) kept, rising,
  0xff after each row. The kept points of the ring's section (`+12 + 0x84·row + 12·i`) are
  appended to `0x137434`. With `objects`, while the row's node (masked) = `0x1365d4`: the
  placement's corners into `0x12f540`, `0x1365d8`++, next placement (at the end: 0, and on
  a circuit its node unless it is this row's, else −1). A section's record `0x136620 +
  6·(sec & mask)`: kind byte, word first point, word points; `0x13bb78` += points.
  A placement: +0 node, +4 kind, +5 angle (256 a turn, negated), +0xa/+0xc/+0xe i16 offset
  << 8 from the node's position. Its kind (`[0x13bb84]` + 16·kind): +1 type (1: one point;
  6: a board and a side, 6 points; else a board, 4), +4 width, +8 depth (type 6), +0xc
  height. Angle a = (−angle << 16) − (node i16 +0x18 << 10), sin/cos of a >> 14 (the
  table `0xf8448`, 1024 a turn); c = (width/2 >> 8)·(cos >> 8), s likewise. Standing: (x−c,
  y+h, z−s), (x+c, y+h, z+s), (x+c, y, z+s), (x−c, y, z−s). **Knocked over** (byte
  `[0xea5d8][2·placement]` ≠ 0; the array, 2 bytes a placement, made at `0x1c630` from the
  head's `+0x16218`, cleared by the replay's `0x5f138`/`0x5f18c`/`0x5f1e0`): height 0, the
  top two moved by (cos·h, sin·h) of b + 0x400000 − (the row node's angle), b = −angle <<
  16 (+0x800000 if < −0x400000). Type 6 adds Q + (cos, sin)(a + 0x400000)·depth, at y + h
  and y, Q the third corner. **Ported (`road_gather`, `port/src/road.c`), equal on 6
  snapshots, standing and with every other object knocked over (`tools/gather_emu.py
  [--fallen]`).**
- **The game's fixed tables come from the player's `NFS_DOS.EXE`** (`port/src/exe.c`: the
  LE image laid out and relocated as `unle.py --fixups`, checked by an FNV-1a of the
  0x169a10 bytes = `0x76078ed2`, identical to `build/nfs_flat.bin`): the port holds none
  of the game's data, and reads `exe_i32(0xed284 + ..)` at the original's addresses.
- Projection `0x83b5c(p, out)`: z ≤ `0xf7938` → (0x80000000, 0x80000000) and `0xf7a40`
  |= 1. Else z normalised below 0x400 with a shift (16; ≥ 0x10000: 24 and z >> 8; again
  ≥ 0x10000: +8, >> 8; ≥ 0x1000: +4, >> 4; ≥ 0x800: +2, >> 2; ≥ 0x400: +1, >> 1), r =
  `[0xf7a14]`[z]; x' = `0xf7a28` + (x · r >> shift, rounded by the last bit out), y' =
  `0xf7a2c` − (y · r >> shift). The shift is taken mod 32 by the CPU (z ≥ 2^24: no shift).
  The table (`0x83af4`): r[i] = (`0xf7a30` << 16) / i, i < 10 as 10; 0x400 entries.

#### The sky: `sky_draw` `0x4a9d0(camera)` (2026-10-19; the open roads' kind ported, `port/src/sky.c`)

- Once: `0x12ef40` = `sky_ring_setup` `0x4a64c(0x12ef60, 0x12ef5c, 1)` (0x448 bytes; the
  circuits' ring of "HR_Ring" pictures). The heading `[0x143df4]+0x44` (2^24 a turn) >> 14
  → d = (sin << 8, 0, cos << 8) (`0xf8448`) >> `0xf6aa4`, turned by `[0x143df8]`
  (`0x83a54`) and projected (`0x94c80(1, .., 16)`): the screen y of the horizon (>>
  `0xf456c`, 0 in the snapshots). **`0x12ef6c` (the INFO's ring type) = 0** (CL, AL, CY:
  the open roads) → `0x4a8b4(y)`; else, if the ring has a height (`0x12ef60` > 0),
  `0x4ace8(ring, −camera >> 10, &screen point)` (TR1..7, and CL3, AL3 by the INFO).
- **The circuits' ring** (2026-10-20; ported, `sky.c` `ring_draw`). `sky_ring_setup`
  `0x4a64c(height far, radius far)`: +0 = height > 0; 32 points (+0x24, 12 bytes each) at
  angle k·0x1000 (65,536 a turn: 16 a turn, twice over): x = sin·radius, y = vert offset
  far, z = cos·radius + zoffset far (rounded 16.16 products); the horizon picture "horz"
  (family entry 2) in eight panels (+0x244, 0x20 each, 16.16 uv: u = (i·w >> 3) << 16 and
  + ((w/8 − 1) << 16), v 1 and h − 2) and the same eight turned round (+0x344: from
  ((7 − i)·w >> 3) + w/8 − 1 back). `sky_ring_draw` `0x4ace8`: the first point =
  ((heading `0x129f10` − 0x320000) / 2^20, toward zero) mod 16 (+16 when negative: 70°
  left of the heading); the eight points from it (`0x94db8`: each >> `0xece1c` (8) plus
  −camera >> 10 >> 8), turned (`0x94e10`, the view's matrix) and projected (`0x94c80`,
  16). scale = window +4 × window +0x10 / screen width `0x143e30`; y = scale × ring y
  offset, H = scale × ring height (rounded); roll = camera +0x48 (negated on a mirrored
  track), a = roll >> 8; each panel i of 7 (panel = first + i): corners 0, 1 = points i,
  i + 1 + (sin a·(H − y) >> 16, −(cos a·(H − y) >> 16)), corners 2, 3 = points i + 1, i −
  (sin a·(H + y) >> 16, −(cos a·(H + y) >> 16)); left out when both of its top x are right
  of the screen or both bottom x left of it, when facing away (x0 > x1 or x3 > x2;
  mirrored: the other way round and the sides swapped) or any x outside ±0x500; painted by
  `0x34f1c(shape, uv, corners)` (two triangles 0 1 2 and 2 3 0, uv >> 16, as `paint_whole`'s
  `0x905e0`/`0x8fad0`) with panel & 7's uv, the turned set when panel & 8 and `0x12efc4`.
  Before the panels (unless F4's plain sky `0x12d9d6`) **`0x4ab18`**: up = (H − y − 1) /
  cos a − 4, down = (H + y − 1) / cos a − 4 (EA's 16.16 division), right = window x + w −
  point x, dy = right·tan a·squash; four `fill_rect`s, the sky's colour (the picture's
  first pixel) and the ground's (its last) left and right of the horizon point, each edge
  up and down by dy as the roll turns (`ret 8`: the two colours on the stack). With F4:
  per panel two polygons (`0x8bc71`) from the window's top and from its bottom to the line
  of the two projected points. **Checked (`tools/sky_emu.py`): equal pixel for pixel on six
  TR1 snapshots t1..t6 (standing, then up to 116 mph), with F4 too; the city's still equal.**
- `0x4a8b4(y)`: the shape "horz" of the family's entry 2 (`0x143e50`); the picture's
  column at the middle = ((mirror ? −yaw : yaw) × 5, a rounded 16.16 multiply) >> 16, C
  remainder by its width; unless `0x12d9d6` (then h = cy = 0): top = y − (cy (+0xa) ·
  `0x12efbc` >> 16), bottom = top + ((h − 1) · `0x12efbc` >> 16). `fill_rect` `0x8a2a0(win
  x, win y, win w, top, the picture's first pixel)`, `sky_strip` `0x49bdc(shape, column, 1,
  win x, top)`, `0x8a2a0(win x, bottom, win w, win y + win h − bottom, its last pixel)`
  (`0x8a2a0` makes a one-colour shape for EA's blitter `0x9d8d0`: cut to the window).
- `sky_strip`: rows n = (h − 1) × `0x12efbc` (rounded), cut to the window's top (k rows
  skipped) and bottom; for the `0x12efb8` columns, those before the picture's end (or,
  when the first column is left of its start, those left of it, a row lower) and the rest
  wrapped round; screen row i reads picture row `0x12dd60[k + i]` + 1 (+1), column
  middle + `0x12df40[j]` (`0x94aed`, the rest from the same table position).
- **The tables `sky_tables` `0x49a78(width, half fov, scale, squash)`**, from `view_setup`
  `0x14154(view, mode)`: the view record +0 = its field of view in degrees (85 ahead, 70
  the mirror), +0x1c = `0xe85c0` + 680 · `0x143e2c` + 40 · mode, a window entry (+4 the
  squash 0xd999, +0x10 the width, +0x18.. x, y, w, h); arguments (+0x10, fov / 2, short((fov
  << 8) / 360 × 5), +4 × width / 320). a = (half fov << 16) / 360; t = tan a (`ea_sin` /
  `ea_cos` `0x80b00` / `0x80b70`, EA's 16.16 division: whole part and remainder apart; cos
  0 → 0x11); focal = (width/2 << 16) / t; per = (scale/2) / a; with half fov > 10:
  `0x12efbc` = squash, `0x12dd60[i]` = (i · (1/squash)) >> 16 for 480 rows, `0x12efb8` =
  width, `0x12df40[w/2 ± i]` = ±(`ea_atan2` `0x927b0(i << 16, focal)` × per, rounded).
  `ea_sin(a)` (65536 a turn) = table(a >> 6) + ((table cos >> 2) · ((a & 63) · 0x6487e >>
  9) >> 21); `ea_cos` the same with −sin. `ea_atan2(y, x)`: |y|, |x|, the smaller << 32 /
  the larger (unsigned), the eighth-turn table `0x125db8` (257) at >> 24 plus the next
  step × the 16 bits below, then the octant (x < 0: 2, y < 0: 4, swapped: 1); equal →
  0x2000. **The tables are made for the road ahead only** (d1 was taken with the mirror's
  view current and still holds the 320-wide ones).
- **Checked** (`tools/sky_emu.py`, and `tools/roadpic_emu.py` sky then road): the tables
  equal, **the sky and the road together pixel for pixel on the 5 snapshots with the road
  ahead's view current**.

#### Painting the road: `0x5a4f0` (2026-10-19; ported, `port/src/roaddraw.c`)

`race_draw_frame` calls `road_objects_start` `0x597e4` (`0x1365d8` > 180 → 0; if > 0:
next = `0x1365cc` + it − 1, less the placements' count if past it; `0x1365c8` (the points
not yet painted) = `0x1365c0`; `0x1365d4` = that placement's node; else −1), then after
the window and the horizon **`road_draw` `0x5a4f0(N = 0x136618, S = the camera's node)`**:
- M = min(N, 4). `0x58f30`: from the view's window record (x, y, w, h at +0x18..): left
  `0x143da4` = x, `0x143da8` = y, right `0x143da0` = x + w, bottom `0x143d9c` = y + h.
  Unless `0x143dcc` (0 in a race; with it `0x5a0a8` at the end instead, which also reads
  `0x143e60` and `0x143dd0`): **`road_draw_far` `0x59d3c(N, S, M)`** first.
- The near part: first point = `0x13bb78` − 11 − (N − M)·11; row r = 4M + 3 − (S & 3),
  the texture's size = `0xf4690[r]`; section = (S/4 + M − 1) & mask, row node = ((S & ~3)
  + 4M − 1) & node mask. Per section (M of them, the camera's last): first point −= the
  section's points (`0x136624 + 6·sec`); its kind's record `0xf61bc + 0x4c·kind` (+0 its
  points, +4 its quads, +8 and +0x10 four pairs of point bytes, a row each, for the walls,
  +0x48 its first quad); the quads (11 bytes at `0xf4880`: four corner numbers from the
  first point, the strip 0..9 across, at +6 a pointer to the tile {+0 ushift, +4 uindex,
  +8 the first size}, +10 the rows it closes) **from the last one back**; the section's
  ring data `0x4f48c(sec)` (= `track_section`): +1 flags, +2 + strip the texture row.
- A quad: corners from `0x13a47c` (screen) and `0x138c58` (view). **`0x58f70(S, strip)`**:
  not when all four x > right, or all < left, or bottom < y of corners 0 and 3; then
  visible if any of the triples (1,2,3), (3,0,1), (2,3,0) (`0xf6b08`) has `0x961f0` area
  < 0 (strip < 5) or > 0 (strip ≥ 5); area = (C.y − B.y)(A.x − B.x) − (C.x − B.x)(A.y −
  B.y), A, B, C = the triple's corners (32-bit products), reversed in the mirror
  (`0x12dd28`, `0x58eec`). Visible → texture `0x140cd8[row][tile+8 + size]` (0: nothing)
  to **`0x35348(tex, tile+0, 0, tile+4, 0, view points, S, near)`**.
- A quad closing c > 0 rows: the walls `0x59294(rec + 0x10 + 2·(row node & 3), row node,
  first point, tile, size, side = (flags & 0x1f) + 0x20, (flags & 0x20) == 0)` if flags &
  0x80, the same with rec + 8 if flags & 0x40 (no city track has any). Then c times:
  while the row node = `0x1365d4`, `draw_object` `0x59490(r, 1)`; `0x679ec(−999, S, row
  node, 0, &S0, &S3)`; `0x6837c((row node + 1) & mask)` (the cars); row node −1; while
  `0x1365d4` = row node and the next placement's byte +8 = 0, `0x59490(r, 1)`. Then r −=
  c, size = `0xf4690[r]`. At the end `0x6837c((row node + 1) & mask)`.
- **`0x59d3c`** the same over rows 4N − 4 down to 4M: first point = count − 11, −= the
  kind record's +0; r = 4N + 3 − (S & 3), size = `0xf4708[r / 4]`; section from S/4 + N −
  1, the next = row node >> 2; a quad painted when **`0x590b0`** (the outcode `0x91cee` of
  its corners = 0, then the triples (1,2,3), (3,0,1) at `0xf6b2c`) and view z of corner 3
  > 0, by **`[0x12c0e4]` = `paint_whole` `0x350d4(tex, S)`**; per row only the objects
  and `0x6837c`. Sets `0x143e60` = 1 << (S & 3) (<< 4 more when camera +0 ≠ 0), read only
  by `0x5a0a8`.
- **`draw_object` `0x59490(r, draw)`**: the placement `0x1365d0`, its kind. Type 1 (a 3D
  model): `0x1365c8` −1, `0x66738(0x146f14 + 0x138·kind[2], &that point of 0x12f540,
  (placement[5] << 16) + (node i16 +0x18 << 10))`. Else `0x1365c8` −= type; if draw and
  the four points' view z (`0x131f70`) ≥ 0x86: type 4, picture `0x141ed8[kind[2] + v]`, v
  = `0xf458c[r]` if placement byte +8 else `0xf4604[r]` (+ ((`0x145c94` / kind[9]) %
  kind[8])·4 when kind[0] & 4, animated), painted by `paint_whole` if `0x59430` (all four
  x within ±0x800); type 6 (also points 4, 5 ≥ 0x86): the board (corners 1,2,3,0 when
  kind[0] & 1) with `0x141ed8[kind[2] + 0xf4604[r]]` and the side (4,5,2,1 when kind[0] &
  2, else 1,4,5,2) with kind[3], both by **`[0x12c0d8]` = `paint_picture` `0x35090`**
  (→ `0x91bf0` / `0x91b90`: `0x91360` / `0x90b30` with uv (0,0), (w·65536 − 1, 0), ..,
  the whole picture); `0x143dd0` set during the side (read only by `0x58c5c`, `0x5a0a8`).
  No track has a type-6 kind with flags 1 or 2 (the original would read past its copy).
  Then the cursor back one: `0x1365d0` −1; below 0 → the last placement, its node on a
  circuit unless it is the one just drawn, else −1.
- **The far painter `paint_whole` `0x350d4(shape, S)`**: type `0x45` → `0x906d0` (type
  `0x7b` meanwhile) else `0x90900`: two triangles, picture points (1,1), (w−2,1), (w−2,h−2)
  on S0 S1 S2 and (w−2,h−2), (1,h−2), (1,1) on S2 S3 S0, each made 16.16 + 0x8000 by
  `0x905e0` / `0x8fad0` for **`raster_tri` `0x8fbd8` / `0x8f0c8(shape, uv, xy)`**: top =
  least y, bottom = greatest (the first test y1 > y0, then y2), middle = 3 − (top | bottom);
  dy = yb − yt < 1.0 → nothing. The long edge at the middle's row: x = xt + (xb − xt)(ym
  − yt) / dy (64-bit), width w = xm − that, |w| in [1.0, 0x600) else nothing; the picture
  there likewise; du, dv = (Δ << 16) / w. A table of a row's steps `0xa6240(0x15ed1c,
  0x8000, 0x8000, du, dv, width, (|w| >> 16) + 1)`: entry i = ((0x8000 + i·dv) >> 16)·width
  + ((0x8000 + i·du) >> 16) (a power-of-two width shifted; written in pairs). Rows from yt
  >> 16 to the middle's row then to yb >> 16 (each cut at the window's bottom), both sides
  from xt, the picture stepped down the left side (the short side when the middle is on
  the left, else the long one), every slope (Δ << 16) / Δy (Δ itself when Δy = 0); a row:
  n = (right >> 16) − (left >> 16), cut at the window's right, its left cut skips k
  pixels, painted only from the window's top: pixel i = picture[(v >> 16)·width + (u >>
  16) + steps[k] + steps[i]] (**steps[k] + steps[i], not steps[k + i]**), `0x94aed`
  (`0x9473e` leaves 255 out). A quicker path when the corners are inside compares their
  16.16 x with the window's pixels, so it is taken only for rows that paint nothing.
  **`tools/raster_emu.py --whole`: equal on 5,000 random quads and picture sizes.**
- **Checked**: `tools/roaddraw_emu.py` catches every call out of the original's
  `0x597e4` + `0x5a4f0` on a snapshot (0x35348, 0x350d4, 0x35090, 0x59294, 0x66738,
  0x679ec, 0x6837c) against the port's: **equal call for call on all 6 CY1 snapshots**
  (220..373 calls). `tools/roadpic_emu.py` lets both paint (cars, walls and models left
  out) into a 320 × 200 screen with the 3D view's window (0, 0, 320, 127): **equal pixel
  for pixel on all 6** (r1a after putting back the see-through mark of the picture it was
  paused painting). No CY1 frame reaches the walls or the 3D models.
- **The walls `road_wall` `0x59294`** (2026-10-20; ported, `roaddraw.c`): (pair, node,
  first point, tile; stack: variant, side, flag; `ret 0xc`). a, b = first + the pair's two
  bytes; nothing if view[b].z < the view's near plane ([0x143dfc] +4, 30). Height 0x20000
  with the flag (section flags & 0x20 clear), else 0xe666; the tops are the road's world
  points `0x137434` + (0, height, 0) taken into the view by **`0x591a4`**: (point −
  camera +0x10) >> `0xf6aa4` each (the difference shifted, not each shifted), turned by
  [0x143df8], and onto the screen (shift 16) from a copy moved onto the near plane when
  nearer (`move_to_near`'s EA division and rounded products); the quad's corners: top b,
  top a, a, b (screen `0x13a47c`, view `0x138c58` for the bottoms). Left out when the four
  corners' outcode (`0x91cee`) is not 0. Texture `0x140cd8`[side][tile +8 + variant] (0:
  nothing); tile +8 ≠ 0 → `paint_picture` `[0x12c0d8]`, else the quad cutter `0x35348`
  (tile +0, 0, tile +4, 0, the four view points, the screen corners, near).
- **The 3D objects `draw_model` `0x66738`** (2026-10-20; ported, `roaddraw.c` `road_model`,
  the models in `trackfam.c`): `load_track_models` `0x66118`, first thing in
  `load_track_textures` (`0x6018c(1, 0)`): the family's entry 3 is a `wwww` of objects,
  each a `wwww` of an ORIP (`orip_fix` `0x978a8`) and its SHPI; `0x65224` keeps them at
  `0x146f14 + 0x138·i` (+0 the SHPI, +4 its "!PAL" (8 bits as shape type 0x24: >> 2),
  +0xc the ORIP, +0x10 the instance `0x95522` with its pictures `0x9759a`), the palette
  tinted (`0x560ec(0, 0x100)`) and its pictures queued for re-colouring (`0x55e68`); on a
  mirrored track every polygon's 0x200 turned over. At most 8 (TR1, TR5). The drawing:
  turn = the library's y turn (`0x988c8`, angle >> 14); v = ((place − camera) >> 12,
  each) turned by [0x143df8]; nothing when v.z < −0x40 or > 0x1f40, or when |v.x| > v.z
  and |v.x| > 0x500 (on City 3, `0x12da12` = 0 and `0x12da0e` = 2, |v.x / 2| > v.z);
  instance flag 2 when v.z < 0x219; the turn times the view's (`0x839f8`) into `0x14e598`;
  `0x666b0`: the origin at v (`0x9803e` of −v), placed (`0x957ec`), transformed
  (`0x98137`), projected (`0x98090`), the whole list sorted (`0x983b4` → `0x982e1`) and
  drawn (`0x98735`): a car's far model's path.
- **Checked** (`tools/roadpic_emu.py --walls --models`, the sort orders taken from the
  snapshot): **the road with its walls and objects equal pixel for pixel on TR1's t1..t6
  and CY1's d2..d4, e1, e3, e5**; `roaddraw_emu.py` equal call for call on t1..t6 (its
  wall's tile pointer compared by the data object's place: it was taken by the code's).
- The grid on a circuit: `car_grid_place` `0x7c938`'s kind is the track (`0x12da12`):
  `--race` gave City's 0 on every track until 2026-10-20; now TR1's player stands at 4.5 m
  across, 121.6 m along, node 19, as in the original's t1.
- **The mirror's view (2026-10-19; ported: `road_gather_behind` / `road_transform_behind`
  in `road.c`, `road_draw_behind` in `roaddraw.c`, `sky_behind` in `sky.c`,
  `cars_behind_at` in `cardraw.c`).** `race_draw_frame` `0x5be17`: the camera's `+0x68`
  sets `0x143e08` bit 0; when `0x143e08` = 1 and `0xea708` < 2: `0x1486c(cam, 1)`,
  `0x5a950`, the window `0xf457c` (`0x8118c`), `0x5adbc`, `0x5aa84([0x13661c] = 4)`,
  `0x96240` (the picture's rows reversed, its cx = w − cx), `0x1486c(cam, 0)`. With the
  settings' `+0x87` (`0x12d9d7`) `0x143e08` ^= 2 each frame: painted every other frame,
  shown every frame. The picture (`0x5ad44`, sized by `0x301f4`) is the dashboard's places
  0..1 inclusive: 47x17 for the 512TR.
  - **The road behind** (`0x50070`'s end, when the camera's `+0x3c` is set): sec = ((node +
    3) / 4 − 4) & `0x13bb90`; if ≠ `0x136614`, kept there, `0x13bb7c` = 0, and five
    sections from it through `0x4fed8(sec)` (below 0 → 0 on an open road): its row 0 of
    the ring (`0x4f48c(sec)` + 0xc, 11 points), the kind = dword `0xedc84` + node(sec ·
    4).type · 24, the points the record `0xf61bc` + kind · 0x4c + 0x18 lists (ascending,
    0..10) appended at `0x13b494`, the kind kept at `0x136621` + 6 · sec.
  - **`0x5a950`**: near = `0xf6aa8` >> shift into the view's +4; the points less the
    camera (`0x94db8`, >> shift) into `0x1422e4`, turned by `[0x143df8]` (the mirror's
    matrix) into `0x13b728`, projected (shift 16) into `0x13b9bc`; those with z <
    0x23333 >> shift projected again at twice that z.
  - **`0x5aa84(n)`**: the window from `0x58f30` (the view's window +0x18..+0x24); for
    k = n−1..0 (the farthest section first) the section's record by its kept kind, its
    quads last to first, corners `0x13b9bc`[11i + q[j]], kept when `0x59134` says so (not
    all left, not all right, the bottom at or below a far corner's row), the texture
    `0x140cd8`[ring[2 + q4] · 32 + (tile + 8 + dword `0xf467c`[k]) · 4], painted by
    `draw_quad` `0x35b04`; a quad with q[10] set then draws the cars of 5 rows
    (`0x68430(max(row − 5, 0))`, row + 1 each); at the end `0x68430` at row − 2 and − 1.
  - **`0x68430(node)`**: d = the camera's node − node (wrapped); d < 11 and
    `0x14e5c5`[d] marked: every car in `0x13c224` not in state 6, at `+0x345` = node, not
    the player in camera mode 0: `0x67f1c(car, 1)` (only where it is seen, `+0x391`: no
    shadow) and `0x66d08(car, 1)` (not drawn beyond z 0x2100, far beyond 0xa00).
  - **The horizon `draw_horizon` `0x5adbc`**: on node (camera − 3) of type 4/7/9 the
    window filled with `0x13d5d4`, type 8 `0x13d5b0`; else across = −(((yaw · 2) >> 16) +
    0x100) & 0x1ff, row = (pitch << 2) >> 16 (− 0x400 above 0x200; × 480/200 at
    640x480) + the layout's +0x20 / 2 + the window's dy; step 0x1000000 / w (and across)
    halved at 320x200; tile = step·w, bottom = step·(h−1), row −= step·cy (16.16 rounded),
    −17 on track 5 (`+0x95`); above row the sky's colour (`horz`'s 16th), then the tiles
    right to left from across (turned every other one with `0x12efc4`) through
    `0x90900`, below them its last pixel.
  - **Checked** (`tools/mirror_emu.py`): the 55 points equal to the snapshot's, the
    mirror's picture equal pixel for pixel on all 11 snapshots, and with car 1 put in the
    camera's lane 3, 6, 9 nodes behind on d1, 2, 5, 10 on e3, 4 on d4.

### 4.8 Cars meeting cars, walls, traffic and the police

- **`cars_meet_world` `0x1c878`**, every step after the driving: every 4th call
  (`0x129fac` counts down from 4) `traffic_police_tick` `0x7a040`. Then for every racer
  `0x1c800`, and `car_objects_collide` `0x1ebf4` (the roadside objects, §4.15) when the car is further across the road than
  the node's lanes reach (node byte 4: lanes on each side, a nibble each; lane widths from
  `0x15d15a`). Then each car (not kind 6, flags `+0x51a` & 6 = 6) against the next ones in
  road order, as long as they are within two nodes: if both are within the sum of their
  sizes (`+0xc0` → `+0x70`) in x and in z, `car_car_collide` `0x1c6a4` →
  `car_car_impact` `0x1e314` (951 bytes: the push, `0x2afb8`, `0x2b864` (6 KB: the bodies'
  dents?), the replay's highlight `0x5c6d4`, and kind 4 through `car_set_drive_kind`).
- `car_set_drive_kind` `0x1cd40` (car, kind): sets `+0x512` and puts the car's position,
  angles and speeds back from its body record (`+0xc0`). Kind 4 = crashed; if the car is
  the one watched, `snd_play_event29` `0x53214` (sound 0x29 through `0x95a7a`, the sound
  library's play).
- **`traffic_police_tick` `0x7a040`** (3.7 KB, every 4th step = 8.3 times a second): every
  car that is not a racer (`+0x516` < 0 or ≥ `0x12da30`) and not crashed drives in its lane:
  `0x786d4` (939 b), the lane choices on the node's lanes (byte 4), `0x78c88` (2.2 KB, the
  steering), `0x78a80`, `0x7850c` (a lane change), `0x78370` (EXPLODE's push, below; not a slowing down). Cars with `+0x174` & 8 are **police**: `police_pull_over` `0x79728`. Cars with
  `+0x174` & 4: `ai_car_engine` `0x79e28` (past the finish they pull to the side; the gear
  from the speed against the table at `+0x308`, the engine's revs `+0x3c1` eased by 1/8).
- **`police_pull_over` `0x79728`** (in full: §4.17): (for a police car chasing, `+0x175` & 4) once it has
  caught the player (the player's `+0x176` & 1, set when the police car is more than two
  nodes ahead, or level and both slower than 3) and both are stopped (speeds `+0x15c` /
  `+0x70` under 1), the player gets **a warning** (`0x14ec34` + 1, `hud_warning_on` `0x2d2c8`) if the chase's
  counter `0x15d4b8` (`traffic_police_tick` adds to it) is still under the limit
  `0x15d2d8`, else **a ticket** (`0x14ec30` + 1, `hud_ticket_on` `0x2d290`). Either way
  the player's car goes to kind 1 (wait, then drive again), the chase ends (`0x15d4b8` =
  `0xf74a8` = 0) and the police car is put back (`0x7c030`, `0x7aeb8`). **On the second
  ticket the race ends: `0x13c178` = 4, arrested**, and the player's finish time is 0.
  The limits `0x15d2d8` / `0x15d2e0` / `0x15d2e4` are set by `drive_opponent_wait`.
- The message: `hud_ticket_on` sets `0xea81c`, `hud_warning_on` `0xea820`, both with a
  time of −1000 (`0xea824`); `hud_ticket_or_warning` `0x2d2ac` tells if one is up (the
  pause waits for it); `load_ticket_picture` `0x30424` loads `ticketDH` / `warnDH` ..
  (`tickeg..` / `gwarn..` German, `..DL` the low-detail screen) from a `.qfs`.

Still to name: `0x5efb0` (first in `race_update_cars` when the state is
not 0: the replay?), `0x2a918`, the second drawing pass `0x5a4f0`, the widget block
`0x71208`..`0x74dcc`, Watcom's runtime.

### 4.9 The race's palette and its textures (2026-10-19)

**Made by the port alone (2026-10-19; `tools/racepal_emu.py`: the palette 0..253 and the
inverse table equal to the original on all 6 snapshots, given the snapshot's cars).** The
cars' colours (`load_car_cfm` `0x65874`): the file `SIMDATA/CARFAMS/<name>.CFM` by the id
`0x12dce4[n]` (< 100 the traffic's names `0xf6e70` (Wagon, CRX, Vandura, BMW, SunBird,
JETTA, RODEO, axxess, probe, Jeep, Lemans, Pickup); 100..199 `M` + the racers' `0xf6ca0`;
200..399 the racers' `0xf6b10` (TSupra, LDiabl, P911, CZR1, F512TR, DVIPER, ANSX, MRX7..,
Traffc), `+0x48a` = id − 100 or 200; 400 and up `CopMust` `0xf6e60`); the container's
entry 1 (`+0x51e`, the near pictures) "!PAL" (`+0x522`) and "!xxx" (`+0x526`, not for the
police: `+0x174` & 8) and entry 3's (`+0x65a`) "!PAL": 8-bit ones (type 0x24) made 6-bit,
each tinted (`pal_tint_shape` `0x562e8`: `pal_tint(0, w·h/3)`). The dashboard's
(`load_dash` `0x2e8d4`): `SIMDATA/DASH/<car>DL.FSH` (`DH` at 640x480, `0x12c308`), "!pal"
(made 6-bit if 8) else the block after "dash"; not tinted before the pieces are set (only
after, by `0x2e74c`, for its own pictures). On CY1 (r1, d): the ids 204, 201, 400, then
the traffic (7, 5, 0, 11, 6, 4 in r1). `race_colours` `0x2e74c`: the DAC read back
(`0x13cb48`), the allowed entries 0..254 less 0x47, 0x5f (with `0x12da20` < 2), 0xfa..0xff
and the marks `0x14e5bc[i]`, the inverse table; then the needles' colour table `0x8a390` (car `+0x48a`'s
colour `0xea710`, §4.11), the dashboard re-coloured and tinted, the DAC again into `0x12ba30`.

**The port's first race frame (`race.c`, `renfs --race cy1`, 2026-10-19).** The loading
in `race_main`'s order: the track, its INFO and family, the nine cars' CFMs (r1's ids),
the 512TR's PBS (`+0x36c` 63569 the eyes' height, `+0x370` 87 the horizon), the
dashboard's colours, the pieces set, `race_colours`, the re-colouring; `view_init`,
`camera_init`, `view_setup` ahead, mirror, ahead again; the two racers on the grid (slots
7 and 6) and `track_follow`; the camera on car 0, mode 0. Then a frame per retrace in
`race_draw_frame`'s order: `camera_update`, the sections kept and gathered again when the
camera's quarter changes (`0x50070`), `view_select(0)`, `view_matrix`, `road_transform`,
the sky, `road_draw(28)`. **By palette entry the frame equals the original's road and sky
at the standing start (r1a)** (the port's `roadpic_emu` picture of r1a, which equals the
original's with the one picture mark the snapshot caught mid-paint put back). Two
globals the original shares that the port keeps twice: `0x13bb90` (the track's section
mask, also the road's) and `0x136618` (the sections drawn): `road_gather` now copies both
into `g_road`; left 0, the road painted its sections all into section 0 (a view that
looked turned). `0x136618` = 28 on every snapshot; its writer is not found yet.

One palette for everything in a race, made of **pieces**: `pal_queue_piece` `0x55ea0(start,
count, pal)` lists them (50 at most, 0x13c7f0), `0x55f58` sets them all (setpalette) after
the loading, once, in `race_main`. In CY1 (d2): 0x90+0x60 the road's palette, 0x30+0x17
and 0x48+0x17 the two racers' cars (`car+0x526`, `0x64e14`: starts and counts `0xf6ea0`,
`0xf6ec8`), 0x47 and 0x5f one entry each (`0xf6f5c` + 3 · car`+0x48a`: the cars'
indicators?), 0x60..0x8f six traffic cars of 8, 0xfa+4 (`0xf6de0`, the program's), 0+0x30
(`0x12b5ec`, `0x2e8bc`: the dashboard?), 0xf0+0xd (the program's `0xf6d18`). The pictures
that come with palettes of their own are **re-coloured**: `pal_queue_remap` `0x55e68(bank,
pal)` lists them (100 at most, 0x13ce48), `0x55fa8` does them after the pieces are set and
the race's inverse table is made (`0x2e74c`): `0x55ee0(bank, pal)` = `0x95d20(0, 0xff,
pal)` (6 bits to 8: (v·255 + 32) / 63, `0xa2f10`; then `nearestcolour` of each of entries
0..254 into `0xf9520`, `0xaa1f0`), 255 stays 255 (`0xf961f`), and every shape of the bank
not named `!..` has its w·h pixels mapped. Both lists stay in memory after use (the counts
go to 0): a snapshot shows every piece and every bank (14 pieces, 64 banks in d2).
**The tint**: every loaded palette goes through `pal_tint` `0x560ec(from, to, pal)` in place
(6-bit triples): c = c · scale / 256 (red `0x13d58c`, green `0x13d594`, blue `0x13d598`,
negative → 0), then c += (max − c) · towards >> 16 (`0x13d590`, ≤ 0x10000; max = the
brightest scaled channel), then c = c · bright >> 16 (`0x13d588`), both rounded by the
last bit out, clamped 0..63. `0x560bc` sets the five from the track's tints (§3.11):
group `0x12c340` (the family switch, below). CY1's usual family: 255 255 255 0 0xff00, so
every channel one lower (63 → 62).
**`load_track_info` `0x49dc0`** ends with that, and: at 320x200 (`0x143e2c` = 0) the ring's
y offset and height × 200 / 480; `0x12efc4` = `0x12efb4` (0 if the track is not a `tr` and
the family is 1). The file is read only when `0xec5d8` = 1 and the family is not 1 (else
the built-in rows, `0xec5dc` + 0x84 · (cl1 cl2 cl3 al1 al2 al3 cy1 cy2 cy3 tr1..tr7)).
Its decimals (`0x41b68`): spaces, '-', the whole part by atoi, the fraction's digits
counted, its zeros skipped and four digits kept after them, (atoi << 16) / 10^digits
unsigned. A line starting `#`, LF or CR is skipped; at the end of the file the last line
is read again. **Ported (`port/src/trackinfo.c`), equal on the snapshots; the file path
equal to `tools/trackinfo.py` on all 16 tracks.** `0x143e2c` is the race's resolution by
then: race_main `0x6356c` calls `0x5b984([0x12c308])` at `0x63664`, just before
`load_track_info` at `0x6366e`, so at 640x480 the ring keeps the file's numbers. (Until
2026-10-21 the port shrank them at 640x480 too: the circuits' horizon there sat low and
flat; the snapshots, all 320x200, could not show it.)
**The family switch `0x12c340`**: 1 if `0x12dd2c` or `0x12dd28`, else `0x12dd30[track]`
(main sets all nine to 1; the track screen `0x77254` toggles one between 1 and another
value). 1: `simdata\ntrackfm\<track>_M01.fam` with `0x12dd28`, `_R01` with `0x12dd2c` (not
track 3, whose rally is TR5's own `_001`), else `simdata\etrackfm\<track>_001.fam`
(`0x156334`; `g..` in German); any other family `simdata\ntrackfm\<track>_T01.fam`
(`load_track_fam` `0x60078`). The trackinfo's tint is the family's (0 morning, 2 evening).
**Ported**: the port had loaded `_001` for the mirrored and rally tracks too (fixed
2026-10-21; checked by eye against the original's mirrored City and rally Alpine, and
evening Alpine).
**`load_track_textures` `0x5b34c`** (entries: 0 the texture banks, 1 the roadside pictures,
2 the horizon): clears `0x140cd8` (144 × 8). One bank: `0x5b270` (see-through marks), the
palette its `!pal` (or `01A0`'s next block), texture 3t + k = "tt" + 'A'+k + '0'..'7' for
t < 48 (`0x5b2f8`: only if slot 0 is empty; the name's last digit counts up; a missing one
= the slot before, the first 0), then "ga<k>0".. replaces textures 3q + k (q = the TRI
head's i16 +0x28 / 3) where the bank has them. Several banks: bank i's "A000".., "B000"..,
"C000".. = textures 3i, 3i+1, 3i+2 (CY1's ETRACKFM has 17); the palette = bank 0's `!pal`.
A name is looked for in capitals, then in small letters (`0x5b1dc`, which rewrites the
name in place; findshape `0x80e24` compares the four bytes). An 8-bit palette (`!pal` type
0x24) is shifted to 6 bits in place. The roadside pictures `0x141ed8` (4 equal slots): one
bank's "<i>00" (i < 64, 0 if missing), or bank i's "0000". Then: tint the road palette,
piece (0x90, 0x60, it), the horizon's `!pal` tinted and its bank queued for re-colouring
with it, every road bank and every picture bank queued with the road palette.
**Ported (`port/src/trackfam.c`, `racepal.c`): every slot, both palettes and all 142
pictures' bytes equal to d2 when re-coloured through the original's inverse table
(`tools/setup_check.py`).** (In r1a picture 16's type is 0x7b: the snapshot paused inside
a drawing wrapper (`0x34dc0`..`0x350fe`) that sets a see-through shape to 0x7b to paint it
opaque and puts 0x45 back.)

### 4.10 The cars drawn (2026-10-19)

**Ported (`port/src/cardraw.c`, `model.c`), equal by palette entry to the original on
e1, e3, e4, e5, r1a and r1b (`tools/cars_emu.py`: the original's sky `0x4a9d0`, `0x597e4`
and `0x5a4f0` with the cars in, run on the snapshot, against the port's; left out on
both sides: the walls `0x59294` and the 3D roadside objects `0x66738`; the shadows and
the racers' markers are in since, still 0 pixels differing).**

**Which road rows have a car.** The race's step (`0x51f8f`) lists the cars on the road in
`0x13c224` (count `0x13c220`): for each car of the order `0x13c298` with `+0x51a` & 4 and
`+0x50e` ≠ 0, `+0x341` = `+0x345` = its node, `+0x349` its `+0x39d`, `+0x34d` its angles.
Each frame `0x678a4` clears `0x14e5d0[35]` and, for every car not in state 6 (`+0x512`),
takes node = `+0x341`, one less when `+0x39d` − 0x29999 < 0 (near the start of its node),
plus 4 · length below 0, **writes it back to `+0x341`** (a second frame between two steps
takes one more off) and marks `0x14e5d0[node − camera's node]` (wrapped) when < 35; then
`0x677f8` (`cars_mark_behind`) does the same for the mirror: it clears `0x14e5c5[11]` and,
for every car not in state 6, takes node = `+0x345`, one more when `+0x39d` + 0x29999 >
0x50000, less 4 · length past the end, writes it back to `+0x345`, and marks
`0x14e5c5`[camera's node − node] (wrapped) when < 11. (An earlier reading here, "the eleven
rows round each car for `0x679ec`", was wrong.)

**`cars_at_node` `0x6837c(node)`**, called by the road painter as each row is reached far
to near: d = node (+ 4 · `0x13bb8c` when below the camera's) − the camera's node; if d < 35
and `0x14e5d0[d]`, the list `0x13c224` is walked from its end, skipping state 6 and cars
whose `+0x341` ≠ node. The player's car in camera mode 0 goes to `0x6829c` (nothing drawn).
Any other: `0x68258` then `0x66d08(car, 0)`.
- `0x68258`: with its own matrix (`+0x20`, the physics' `+0x24`) `+0x351` = `ea_atan2(m[6],
  m[8]) << 8`; then `0x67f1c(car, 0)` and, if seen, the shadow `0x8a094(4, [0x14e594])`.
- **Without its own matrix `+0x351` is the heading the step left**: `race_update_cars`
  (`0x51fa3`..`0x51ffc`), each step after the driving, for every car in the order list
  that races (`+0x51a` & 4, `+0x50e` ≠ 0): `+0x341` = `+0x345` = its node (`+0x48`),
  `+0x349` = `+0x39d`, and pitch, yaw, roll (`+0xc`..`+0x14`) copied to `+0x34d`..`+0x355`.
  The player's car has no own matrix but in a crash's tumble, so this is what turns its
  body as it steers and slides. The port set `+0x351` only on the grid and while paused
  until 2026-10-21 (Danik: from outside the car "does not face where it's going", turning
  only in crashes).
- `0x67f1c`: `+0x391` = rotate((car − camera) >> 9, the matrix `[0x143df8]`); seen if
  0x20 ≤ z ≤ 0x1900 and |x| ≤ z. Then `0x38548(car, 1)` (the shadow's centre `+0x359`, its
  axes `+0x36d`, `+0x379`) and the shadow's four corners (−a+b, −a−b, a−b, a+b, each axis
  >> 10) rotated, offset and projected (`0x83b5c`) into `[0x14e594]`.
- `0x66d08`: none if z < 0x20, z > 0x5a00 or 2z < |x|. Far (`+0x78e`) = z > 0x1900 and slot
  (`+0x516`) ≠ 2. Its turn: `+0x24` if `+0x20`, else Z·X·Y of the library's turns X =
  (shake(`+0x42d`, `+0x428`) + pitch + `+0x369`) >> 14, Z = (shake(`+0x429`, `+0x427`) +
  roll + `+0x365` − `+0x365`/4) >> 14, Y = `+0x351` >> 14; times the camera's matrix into
  `0x14e598`. z < 0x219: instance flag 2 (polygons with a corner nearer than 30 left out)
  on both models, else cleared. Then `0x66880`, the marker `0x66b98` when `0x12da20` ≥ 1,
  and `0x64a74` for the player.
- `0x66880`: far: the model `+0x666` placed (`0x957ec`), transformed, projected (`0x98090`),
  sorted and drawn. Near: a racer (slot < `0x13c18c`, `0xea81c` = `0xea820` = 0) has its
  mark's entry `0x14e5bc[slot]` set (setpalette) to `0xf6f5c` + 3 · `+0x48a` (`0xf6f83`
  when braking, `+0x3b9` > 10); else the police lights flash in 0xfa... Then the model
  `+0x52e` placed and its groups in order: inside (`+0x5be`, count `+0x5c2`, list `+0x5c6`),
  surface (`+0x5d6`..), outside (`+0x5ca`..), convex (`+0x5e2`..), each one whose index is
  (unsigned) < 8 sorted then drawn.

**EA's 3D library** (`0x94000`..`0x9a000`, `0xaa000`..`0xac000`): its own camera `0xf9678` is
never turned (identity); the near plane `0xf9674` = 30 (`0x66101`). A point in the view =
rotate(pt, M) + pos (`orip_transform` `0x98137`); onto the screen `0x94c80` (shift 16). The
sort `orip_sort` `0x982e1`: key = z(corner 0) + z(corner 1), Shell's sort, the farthest
first, **in place** (the order is kept from frame to frame: a test must copy it).
`orip_draw_poly` `0x98482`: flag 2 and a corner z ≤ 30 → nothing (nor its chain); unless
flags & 0x500, (area > 0) must equal !(flags & 0x200); texture (flags >> 16) & 0xff; with
0x80 and a picture: routine bits 3..6 = 1 see-through; 3 corners → triangle, the picture's
points in whole pixels (`0x905e0`/`0x8fad0`); flags & 0x1000 → quad, points << 16
(`0x91360`/`0x90b30`); else the whole picture (0,0)..(w·65536 − 1, h·65536 − 1)
(`0x91bf0`/`0x91b90`). No picture: one colour (`0xaa990`, ported since, §4.10's player's wheels).
0x800: the polygon after the corners follows. The turns `0x98860`/`0x988c8`/`0x98930` are
the transpose of the port's `mat_turn`, `fix_sin`/`fix_cos` of (a & 0x3ff) without steps
between. Groups: names of 8 letters, any case; 28 bytes, +0x10 the first index into the
index table (+0x50), +0x14 the count. A car's instance (`0x95522`): every polygon listed
(`0x981f7` tests polygon 0's 0x400 each time: all or none); textures by their 4-letter
SHPI names (`0x9759a`); racers (id ≥ 200) have "frntires" and "rertires" showing the
"tire4" part's picture; mirrored tracks turn 0x200 over on every polygon.

**The cars' pictures in the race's colours: `0x65020`**, in `race_main` after
`race_colours` and before `pal_apply_remaps`: for each car i of `0x13c24c`, `0x55ff0(+0x51e,
+0x522, i)` and `0x55ff0(+0x656, +0x65a, i)` (the near and far pictures with their own
"!PAL"); cars 0 and 1 then `0x976c0(+0x52e)` (each texture's one colour `+0xc` matched
again: `nearestcolour`). `0x55ff0(bank, pal, i)`: `0x95d20(0, 0xfa, pal)` (entries 0..0xf9
of the map `0xf9520` only); 0xfe = the mark `0x14e5bc[i]` when there is one; 0xfa..0xfd =
themselves when `0x12dd08` and i = 2 (the police car's lights in a chase); then every
picture not named `!..` through the map. **The map is shared**: entries a re-colouring
does not fill keep what the last one left: for the first cars 0xfa..0xfe come from the
dashboard's palette (`race_colours` re-colours the dashboard just before), later cars
keep the police car's 0xfa..0xfd and the last mark in 0xfe. (`0x13ca48`, another map of
the race's colours made by `0x5591c`, gets the same five entries.) `0x12dd08` = 1 (set by
`0x653a4`) in the snapshots' races: one racer against the player with the police on
(`0x12da30` = 1, `0x12da34`). **Equal byte for byte on all 11 snapshots
(`tools/racepal_emu.py`)**: before this the port drew the cars in their own palette's
numbers (the rival nearly black).

**The shadow** (ported, `cardraw.c`, `polyfill.c`; in `tools/cars_emu.py` since
2026-10-19, still 0 pixels differing on all six). `0x38548(car, 1)`: `0x384bc` turns
(0, `+0x78`) and (−`+0x7c`, 0) by −`+0x351` (`0x3e918`: x' = cos·x − sin·z, z' = sin·x +
cos·z, `ea_cos`/`ea_sin` of angle >> 8, each product rounded) into the half axes `+0x36d`
and `+0x379` (y 0); the centre `+0x359` = the car's place. `0x382f0` lays them on the
road: A = the node's point (`+8`, `+0xc`, `+0x10` of node `+0x48` & mask), B = A + 2 · the
node's i16 at `+0x1c`, `+0x1e`, `+0x20`, C = the next node's point; n = (A − B) × (C − B)
(`0x3e880`, rounded products); inv = 0x10000 / n.y in 16.16 by two divisions, >> 8; then
each axis' y = ((−(x>>8)(n.x>>8) − (z>>8)(n.z>>8)) >> 8) · inv and the centre's y = A.y +
the same of (centre − A). In `0x67f1c`: the centre into the view, the corners −a+b, −a−b,
a−b, a+b (each axis >> 10) turned, offset and projected; `0x8a094(4, corners)`.
**The darkening table `0xf8118`** (`set_shade_table` `0x89ff0`), made in `0x55abc` after
the cars' pictures: `0x89f98(0x9999, the palette set, 0x13d488)` → `0x89f3c`: for each
entry whose `0xf8018` is set (all 256), each 6-bit channel × 0.6 (rounded by the last
bit, at most 63), the nearest of the allowed colours (`0xf77c8`) by 2·dr² + 3·dg² + db²,
the first of equals (`0x89da0`; with `0x15ec74` the three are averaged first: grey).
Equal on d2, r1a, e3. (`0x55abc` also finds the HUD's colours, `0x55790`, `0x13d5a4`..)

**EA's polygon filler** `0x8bc71` (entries `0x8a04c(mode, colour, n, points)` with the
shade row painter `0xa5dd4`, `0x8a094` its mode 0; the sky ring `0x4ace8` and the markers
`0x66b98` use it with the one-colour painter `0xa6e80`). Points: n × (x, y) ints. One point,
or every x the same: a line (`0x8bb3c` → `0xa6c38`, in the colour: a black line for a
shadow). Corners: the top one the *last* with the least y, the bottom one the *first* with
the most. Nothing if the height is 0, max x < x0, **min x ≥ x1 − 1**, max y < y0 or min y ≥
y1 (window `0xf88a4`..`0xf88b0`, the ends outside). Cut (the slower edges) if any corner
is outside. From the top corner on through the corners to the bottom one, each edge going
down (`next.y > y`) is made (`0xa631c`, cut: `0xa632b`) and walked (`0x8bf3c`, cut
`0x8be7e`): both tables get its x on each row. Then back the other way (`0x8c06e`, mode 0
the convex case `0x8c276`; mode 1 `0x8c09d` the general): row by row while its x lies
between left and right, then all its later rows go to the side it left by. Rows from
max(min y, y0) to min(max y, y1 − 1), none if that is one row or less (!), painted
left..right, both in. An edge (0x38 bytes): x in 32.32, y in 32.32, the bottom end, a
count, four counts of rows wholly left/right above/below, the slope (minor/major in 0.32:
majors < 50 from EA's table `0x127ff0` of floor(m · 65536 / M) << 16, `0x127fec` = 50;
else rounded, a half down), and its kind: 2 vertical, 3/4 diagonal, 5/6 steep (x rounded
each row), 7/8 shallow (each row the run of x, from y's rounding carry); odd ones go left.
Cut: above/below first (whole rows; the shallow kinds by dividing by the slope, rounded),
then the x ends again, then left and right (rows outside counted, filled with x0 / x0 − 1
or x1 / x1 − 1 so they are empty unless the other side widens them). **Ported
(`port/src/polyfill.c`), equal to the original on 7,300 random convex polygons from a
pixel to 2,000 wide, most crossing the window (`tools/polyfill_emu.py`).** Not ported,
as the game never uses them: mode 1 (its two entries `0x8a077` and `0x8a0b1` have no
callers and no pointer in the relocation records names them: EA's library's, dead in this
program), and the halving of coordinates past 31 bits (`0xa6b39`; the projection's
polygons never come near).
**Lines** `0x8bb3c(x0, y0, x1, y1, colour)`: the edge made cut (`0xa632b`; its ends taken
top first, swapped when y0 > y1), drawn by `0xa6c38` only if it returns 0 and its count
> 0. A flat edge (y0 = y1, `0xa6a79`): kind 1, 9 if x0 = x1 (a point), 0 if x0 > x1 (the
ends swapped); cut: y < y0 → code 4 (y = y0), y ≥ y1 → 8, all left (max < x0) → code 2,
y1 − 1, one row left (`+0x24`), all right (min ≥ x1) → code 1, `+0x2c`, each with count
0; else the ends clamped to x0 and x1 − 1, the count shortened. `0xa6c38`: x and y to
16.16 (whole << 16 | the fraction's top 16 bits) + 0x8000, their whole parts the rounded
x and y (16 bits); **only the slope's top 16 bits are stepped**. Kinds 0, 1: a run (x, y,
count, 1) through `0x8a2a0`; 2: (x, whole y, 1, count); 3/4: a pixel per row, x ∓ 1; 5/6:
pixel (x, row), the fraction += slope, the row on, on a carry x ∓ 1; 7/8: the fraction +=
slope, pixel (x, y), x ∓ 1, on a carry y + 1 (the pixel takes y before it); 9: one pixel.
The pixel `0xa6060` is cut to the window. One-colour rows (`0xa6e80`, `0x8bc71`'s):
`0x8a2a0(left, y, right − left + 1, 1, colour)` each. **Ported (`poly_line`,
`poly_solid_rows`), equal on 10,000 random lines (a third flat, upright, diagonal or
points) and 10,000 one-colour polygons (`tools/polyfill_emu.py … line|solid`).**

**The racers' markers** (ported, `cardraw.c`; in `tools/cars_emu.py`, still 0 pixels
differing, 8 pixels of marker on e3). After `0x66880` in `0x66d08`: the player's own car
(slot = `0x12dd58`) only with the word `0x15d13a` (0 in a race played; else `0x64a74` on
`0x1464d0` when `+0x175` has 2); any car when `0x12da20` (the race type, CONFIG.DAT +0x6d:
`0x36c5c` loads the word as dword `+0x6b` >> 16) ≥ 1 and slot ≤ `0x12da2c` − 1 (the cars
racing: 1 time trial, 2 head to head, 8 single race and tournament). Its place `0x591a4`:
(car − camera) >> `0xf6aa4` (11, never written) turned by `[0x143df8]`; a copy put on the
near plane (`[0x143dfc]+4`) when nearer, projected (`0x94c80`, shift 16). The point: x,
y − 0x3800 / z − 15 at 640x480 (`0x12c308`), y − 0x2000 / z − 7 at 320x200, z the turned
one's. `0x66b98(x, y, slot, z)`: nothing with the byte `0x12d9db` (a key toggles it,
`0x481d7`) or slot < 0; half width w and height h (3, 1) at 320x200, else by z: > 3000
(3, 1), > 2000 (5, 2), > 1000 (8, 3), else (9, 4). The triangle (x − w, y − h), (x + w −
1, y − h), (x, y) filled (`0x8bc71`) in `0x13d5e8[slot]`, then the lines (x − w, y − h)–
(x + w, y − h), (x + w, y − h)–(x, y), (x − w, y − h)–(x, y) in `0x13d5c8`; a second one
the same h higher when slot < `0x12da30` (the players' cars). The colours (`0x55abc`,
before the shade table): `0x13d5e8` = f5 f8 f0 fa, the nearest white (`0x13d5c4`), f9 fc
fb; `0x13d5c8` the nearest black: `0x55790(r, g, b)`, the entry 0..254 of the 6-bit
palette with the least |dr| + |dg| + |db|, the first of equals. (`0x13d5a4`..`0x13d5e4`,
`0x12b9fc`, `0x12ba08`: the HUD's other colours, the same way.) Equal on all 7 snapshots.

**The crash's smoke** (2026-10-19; `port/src/smoke.c`). `load_smoke` `0x645bc`, in
`race_main` after `cars_create`: `SIMDATA\MISC\SMOKE.QFS` (`0x156564` + `0xe3b7c`), the
puff `smok` (`0xf6db0`; 64x64, centre (30, 32)), `smo1`..`smo4` and `dir1`..`dir4` (the
dust, `0x1464bc`, `0x1460ac`: not used yet), its `!PAL` + 0x10 tinted (`pal_tint`) and
queued for re-colouring (`0x55e68`); the puffs' order `+0x640` = 0..99. **The table**
`0x64488`, at the end of `pal_apply_remaps` `0x55fa8`: the palette read back from the card
(`0x8a328`, 6 bits) made 8 (c·255/63, truncated); 64 KB at `[0x1464cc]`, `0x64360`: entry
[s][d] = `nearestcolour`(min(255, d + s − s/4) for red, green, blue) (the smoke colour's
three quarters added); its rows' pointers `0x1460bc` given to EA's `0x94731` (`0xf94cc`).
**Drawn** at the end of `car_draw` `0x66d08` for the player's car (slot `0x12dd58`) with
`0x15d13a` = 0, when `+0x175` & 2 (the crash slowed): the offset = the near model's point
`smok` on the screen (`+0x616` from `0x97afa`: the ORIP's +0x54/+0x58 named points; into
the instance's `+0x7c`) − the car's place (`0x591a4`), or (0, 0) with `+0x78e` (the far
model). **`0x64a74(smoke, offset)`**: the 100 puffs (place >> 11) − (camera >> 11) turned
(`0x94db8`, `0x94e10`), z held to 1..0x7fff; the order sorted again far first (insertion,
moving each past the nearer, so it stays nearly sorted from frame to frame); projected
(`0x587a8` = `0x94c80` shift 16); each alive (age ≥ 0) with z > 0x4ccc >> 11: size =
(`0xf7a30` << 5) / z in 16.16, f = (0x10000 − age) >> 8, ((size >> 9) · ((0x10000 − f² +
0xccc) >> 8) >> 8) · 0x300 >> 4, the puff at its point + the offset. **EA's stretched
shape** `0x96f70` → `0x96e40(scale, shape, x, y)`: w = scale · width >> 16, h = |scale| ·
height >> 16, the corner = (x, y) − the centre × scale (rounded, `imul shrd adc`; a scale
< 0 mirrors from the right) → `0x96aa8(w, h, shape, x, y)`: out if past the clip, steps
(width << 16) / w, (height << 16) / h, starting half a step in (w or h < 0 flips), cut to
the clip, a column table `0x15ed1c` (`0xa6240`), each row by `0x9498d`: **every pixel =
`0xf94cc`[shape][screen]** (no see-through colour: the table does it), `0x96a10` instead
for a banked screen (`0xf88c0`). A shape not of the screen's type (`0xf88b5`) is converted
first (`0x9e734`). **Equal on all 11 snapshots in 88 rounds of made-up puffs
(`tools/smoke_emu.py`), the table and the re-coloured file too (`tools/racepal_emu.py`).**

**In the port's race (`--race cy1`)**: the two racers on the grid, standing (`+0x351` = the
grid's yaw), the list made each frame (the step's job), the rows marked, the cars painted
as the road reaches them. At the standing start the rival is beside the player, out of
the windscreen's view, in the original too (e1).

**The player's car's wheels and one-colour polygons (2026-10-21; equal on h7, 0 pixels).**
`0x66880` calls, for the player's car's near model only, before its points are turned:
- **`0x66148`, the tyres' pictures.** At load (`0x65e55`, racers' ids ≥ 200) +0x5fe / +0x606
  the texture slots "frntires" / "rertires" (`0x97521`), +0x5ee.. the pictures of "tire1" ..
  "tire4" (`0xf6f48`), both slots set to the fourth, +0x602 / +0x60a = 3. Each frame:
  |speed +0x68| above 0x1e0000 → 3, below 0x1999 → 0, else: in the same step as the last
  frame (`0x14e618` = the counter `0x145c94`) or held (`0xf6b74` = 4) → 1, else
  `0xf6fe0` = 1 − it, below 0xa0000 (and `0xf6fe0` ≠ 0) `0xf6fe4` = 1 − it and the
  picture `0xf6fe4` + 1, above it `0xf6fe0` + 1. Front: tyres +0x425 & 1 → 0, & 2 → 3; for
  cars other than numbers 1 and 10 also 0 when |Δx| + |Δy| since the last frame (`0xf6fe8`,
  `0xf6fec`, the car's x and **y**) is below 0x1999. Rear +0x426 the same without the
  distance. A slot changed only when it is another picture (`0x977f3`: the slot's +0xc).
- **`0x6634c`, the front wheels.** s = +0x18 / 32 (towards 0). `0x662e0`: the angle of the
  camera to the car, `0x3e7ec(dz >> 8, dx >> 8)` (**z first**), less the drawn yaw +0x351,
  both and their difference to ±0x800000. The left part keeps s from 0xa0000 to 0x760000,
  the right from −0x760000 to −0xa0000, else 0; then `0x97eb8(model, +0x60e "left_turn",
  −s)`, `(+0x612 "right_turn", −s)` (found by `0x97cd0`, the ORIP's +0x44 records of 20
  bytes, +0x40 of them: name, points' count +8, first in the index table +0xc, first move
  among the points +0x10). **`0x97eb8`** sets each of the part's points in the instance's
  own copy (+0x70) to the ORIP's point + its move × s (`0xaa4d0`, rounded; 0 a copy). The
  instances are made with 1 (`0x95522`'s ebx at `0x65bff`): **every car model has its own
  copy of its points** (`0x97d49` copies them); the port's `model.points`.
- **One-colour polygons (`0x98482` → `0xaa990`)**: no 0x80 or no picture: two corners a
  line `0x8bb3c`, three or four filled solid (`0x8bc36`, `0x8bc71`'s entry with the corners
  by address), in the slot's +4: **the palette entry nearest the ORIP texture record's +0xc
  colour** (`0x97638` → nearestcolour `0x80c80`), matched as the instance is made
  (`0x976c0`) and again for cars 0 and 1's near models in `0x65020` (after the race's
  palette pieces `0x55f58` and `0x2e74c`). The 512TR's rear bumper is one; the port's own
  match gives the original's entries (82, the last 69) for both cars of h7. A slot's +0x10
  (a drawing routine of its own, called first) is zeroed at creation and never set: not
  ported. The cars' tyres on mirrored tracks (ids 100..199) have no slots: left alone.

**The tyres' marks on the road (2026-10-21; `port/src/skid.c`, checked by
`tools/skid_emu.py`: 600 cars recorded, 120 rounds of the road's rows, 600 lines painted,
10,360 lines compared, all equal; h7's own two lines in `cars_emu`, 0 pixels).** Only the
player's car leaves them, only as it is drawn, and none with the settings' +0x87 (the cars
in less detail, `0x12d9d7`).
- **The ring**: 125 records of 0xa0 at `0x149574` (`0xf6e2c` the one being written): +0 four
  corners about the centre (the front pair A'+B, A'−B, the rear −A'+B, −A'−B, with A' = A −
  A/4 towards 0, A the half length along and B the half width across the road, both
  `0x38548`'s >> 10), +0x30 the shadow's centre in the world, +0x3c the corners seen and
  +0x6c on the screen (written by the painter), +0x8c the node, +0x90 / +0x94 the front's /
  rear's mark from here back (the *last frame's* slide, `0xf6ff4` / `0xf6ff8`; 0 none),
  +0x98 the step counter `0x145c94`, +0x9c 2 on a surface whose `0x12cc00` row's 4th word
  is set, else 1. `0x65094` empties it (record 0's node 0x10000, the marks 0) at the race's
  start (`0x650c8`) and at a replay's jump (`0x5d690`).
- **`0x671dc(car; A, B, C on the stack)`**, from `0x67f1c` (outside, after the shadow's
  corners) and `0x6829c` (from the driver's seat: `0x38548(car, 1)` then the same). A tyre
  pair slides when +0x405 / +0x409 ≥ 0x3c0000 and +0x425 / +0x426 & 3. Neither: only the
  last slides kept (0). Else the record at `0xf6e2c` is written (a pair that does not slide
  gets 0 and keeps its old corners); d = |Δz| + |Δx| from the centre where the last record
  began (`0x14e620`). d ≤ 0xa666: the record moves on with the car, and when the rear slides
  `0x679ec(record, 3 records)` paints it. Else the ring moves on (the new record's marks
  0); with d ≥ 0x30000, when the record before (at the ring's start, **the one past its
  end**, `0x14e394`, never written: empty) left the same marks, the record is copied on and
  its corners moved half way back to the one before; the next record's marks 0, the centre
  kept, and the rear's painting from the record before (**at the start the one before the
  ring**, `0x1494d4`: never written but for its points seen, so node 0 and no marks).
- **`0x67784(camera node)`**, before the road (`0x5bdfe`): `0x14e5f3`[d] = 1 for each record
  with a mark d nodes from the camera's − 2, d < 15. The painter reads byte 15 too: the
  loading pictures' used flag of picture 0 (`0x14e602`).
- **`0x679ec(-999, camera node, row node, 0; the row's edge)`**, from the road's painter at
  each near row (`0x5a85d`, after its walls, before its cars): d = the row's from the
  camera's − 2 (round a circuit), none past 15 or when `0x14e5f3`[d] is 0. The records from
  `0xf6e2c` − 1 back, 123 of them; one more than 2 nodes from the row stops the joining.
  Each: the centre seen (`0x83a54`, >> 9), each corner turned and added; joined to the
  newer one before it when both left that pair's mark: either end nearer than 0x40 is cut to
  0x40 along the line (both: nothing, not even projected; a cut newer end projected again),
  the width 0x2580 / the farther z (z 0: 100), at most 15, halved at 320x200 and with F2's
  detail (`0x12d9d9`), at least 1; a line only when an end is beyond the row's edge
  (`0x67950`: (b.y − a.y)(x − a.x) − (b.x − a.x)(y − a.y) ≥ 0 for one end). A record's
  painting (`0x671dc`'s) the same without the edge.
- **`0x989a0(x0, y0, x1, y1, w)`**: a = `0x8bb7c(dx, dy + dy/4)` (the screen's y a quarter
  longer), u = w·(−cos a), v = w·sin a then v − v>>2 + v>>4, the quad (x0 + u/2, y0 + v/2),
  (x1 + u/2, y1 + v/2), (x1 − (u − u/2), ..), (x0 − .., ..), darkened as a shadow
  (`0x8a094`).

**The tyres' dust (2026-10-21; `skid.c`'s `skid_dust`, checked by `tools/skid_emu.py`: 600
cases, 520 puffs, the random numbers after each, and the dirt's stretch on pixels, all
equal).** `0x664bc(car, behind)` for the player's near model only: after its points are
placed (`0x66a18`, behind = 1) and after its groups are painted (`0x66b3d`, 0).
- None when both tyres' bytes +0x425/+0x426 are 0, both slides +0x405/+0x409 are 0, or the
  car is drawn far (+0x78e). a = `0x662e0` (the camera's angle round the car, as the
  front wheels'). base = 0x8000 behind (with `0xf6b8c` 0: only replays write it, always 0)
  else 0x10000; base = base / 256 · (0x38400 / seen z +0x399).
- A sliding pair: k = slide · (`0x52580(4)` + 2) / 2^18 (towards 0), 1 .. 0x180; amount =
  base / 256 · k. The left tyre (0 front, 2 rear) behind or with 0xc0000 < a < 0x740000,
  the right (1, 3) behind or with −0x740000 < a < −0xc0000.
- **`0x663e0(tyre, amount)`**: the records from `0xf6e2c` − 1 back, four of them, size =
  amount, + amount each: one whose pair's mark is set, less than 0x30 steps old (and more
  than 0), its screen point not 0x80000000: kind 1 `0x64714` (smo1 or smo3 by
  `0x52580(2)`·2, halved at 320x200, `0x96f70` = `0x96e40`: added as the crash's smoke),
  else `0x64768` (dir1..dir3 by `0x52580(3)`, halved at 320x200, at most 0x10000,
  `0x97500` → `0x973d0` → `0x97038`: `0x96aa8`'s copy whose rows `0x9473e` put the
  picture's pixels on the screen, 0xff see-through). The pictures: SMOKE.QFS's names at
  `0xf6db8` / `0xf6dc8` into `0x1464bc` / `0x1460ac` (`0x645bc`).
- **Its random numbers are the collisions' (`0x52580` → `0x91e04`)**: in the original how
  many frames are drawn changes how cars hit each other later. The port's dust draws from
  its own copy of the generator (taken from the race's at `skid_reset`), so the race stays
  the same whatever the frame rate (60 a second with smooth motion). The check seeds both
  equal.

### 4.11 The dashboard and the HUD (2026-10-19)

**The frame** (`race_draw_frame` `0x5bc40`): the 3D view is painted into an off-screen
picture (`0xf4578`, 320x200, its own window), then in the driver's view (camera mode 0,
`0x12ba2c` = 0) `0x31684(player's car)` puts it on the screen; other views `0x314e4`.
`0x2d284` (sets `0x12b8d4`) asks for the whole dashboard again, done by `0x31684` through
`0x30094`. With `0xeaafc` (0 in these races) all of it goes to a back buffer `0xf4588`
instead of the screen (`0xba93c` sets the screen's window, `0xf88d4` → `0xf889c`).

**EA's windows and blits.** A window record (13 words, the current one at `0xf889c`):
w, h, the clip x0 y0 x1 y1 (`0xf88a4`..), `+0x18` the kind (0x7b08: 8 bits), `+0x20` the
row in bytes, `+0x28` a table of row offsets, `+0x2c` the base. An off-screen picture is a
window record followed by an FSH shape (`+0x38`), its pixels after the shape's 16 bytes.
`setwindow` `0x8118c(record)`, `cliprect` `0x812ec(x0, x1, y0, y1)` (clamped to 0 and the
window's size), `0x5885c(x, y, w, h)` = `cliprect(x, x + w, y, y + h)`. The blitter
`0x9d8d0(shape, x, y, record)`: for an 8-bit shape on an 8-bit window a rectangle cut to
the clip, row by row with the record's row routine (`+4` for 8 bits): `0xf79a4` copies
(memmove), `0xf79cc` skips 255 (0x81434; kind 4 shapes skip 0), `0xf7a50` copies the other
way, the window into the shape (`0x84248(shape, x, y)`, which also sets the shape's x, y).
Entries: `0x81214(s, x, y)` copy, `0x81230(s)` copy at its own place (+0xc, +0xe, signed),
`0x81280(s, x, y)`, `0x8129c(s)`, `0x812bc(s, x, y)` by its centre (+8, +0xa); see-through
`0x815c8(s, x, y)`, `0x81580(s)` / `0x815e4(s)` at its own place, `0x81600(s, x, y)` by its
centre.

**The HUD's pictures** (`load_cockpit_mask` `0x5f7c8`, after `0x2e8bc` in `race_main`):
`D:\simdata\misc\` + `masklo.fsh` (`maskhi.fsh` at 640x480, `masklog`/`maskhig` German)
into `0x145604`, its `!pal` (>> 2 when 8-bit) queued for re-colouring (`0x55e68`) and the
program's 13 colours at 0xf0 (`0xf6d18`); `0x5f240` finds the pictures by name, many
from the program's tables of names: `0xf6c84` grey digits gry0..9, gryb (`0x1456b8`),
`0xf6cb0` yellow yel0..9, yelr, yeln, plus, mnus, yelb (`0x1456e8`), `0xf6cec` red
(`0x145728`), `0xf6bc0`/`0xf6be4`/`0xf6c08` the replay's buttons, `0xf6c2c`/`0xf6c4c` the
views', `0xf6c6c`/`0xf6c78` fuel?; then the `MMB` and `.B<s><n>` lists. Re-coloured, equal
byte for byte to the snapshot's (`tools/racepal_emu.py`).

**The whole dashboard** `0x30094`: the screen's window; `0x2d314` (the race running,
`0xf6b74` = 1, and the redraw due: the clip the whole screen); the dashboard's picture
`dash` (`0x12b4d8`) see-through at (0, 0) (`0x815c8`); `0x12ba0c` = 1; **the map's box**:
the screen at (0x104, 0x7f) taken into `0xf4580` (57x52, `0x84248`; 640x480: (0x208,
0x132)), which is then darkened twice through the shade table (`0x8a0cf` = `0x8a04c` mode 0
with `0xa5dd4`, corners (0, 0), (0x38, 0), (0x38, 0x3b), (0, 0x3b); 640x480 (0x70, 0x8b));
then `0x2f2a4` the status bar; `0x12b8d0` = 0x63 (the wheel drawn next), `0x12b8d4` = 0,
`0x2d150` (every number shown set to −99: drawn again).
**The status bar** `0x2f2a4`: with `0x12b940` = 0 or the race's steps `0x145c94` < 360
only a black strip (0, 0, 320, 16) in `0x13d5c8`. Else `stat` at its place, then by
`0x12b8cc`: 1 (the race timed) `rtet`, the time of record 6 (+ 10 · `0x12dd24` on a circuit,
`0x13bb80`) of the track `0x12da12`, `splt`, plus or minus (`0x14ea6c` ≥ 0); 0 (a segment)
`segt`, then with splits (`0x14ea70` > 0) `splt` and plus/minus, else `lap#` (circuit) or
`mile`, and the time: the best lap `0x13c20c` on a circuit, else the record `0x12da0e`
(the segment) of the track (CONFIG.DAT +0xb5 + track · 0xa6b + k · 0x27 + 0x1b); 2 `0x145780`
and `0x2d8c0` (not read yet). Then (< 2) `head` (race type `0x12da20` 0, 1) or `sing`.
A time (`0x2d6b4(t + 300, x, y, step, gap)`, 320x200: (0x11, 3, 6, 2)): t − 300 (≥ 0) split
by `0x2d3ac` into minutes, seconds, tenths (`0x12b8c4`, `0x12b8c0`, `0x12b8bc`), each drawn
by `0x2d47c(v, x, y, step, n, table)`: n digits, the most significant first, copied
(`0x81214`) step apart; minutes at x, seconds at x + 2·step + gap, tenths 2·step + gap on.
**Ported (`port/src/hud.c`): equal to the original on e1 (the strip) and e3 (the bar),
the screen and the map's background (`tools/dash_emu.py`).**

**Each frame** `0x31684(car)` (ported, `hud_frame`): nothing with `0x13c17c`; the
tickets (`0xea81c`, `0xea820`: `0x30424`, not read); `0x306e8` (the dashboard loaded again
when `0x12b5f0` = 0, else nothing); the redraw when `0x12b8d4` or `0x12b8d8`. The wheel's
turn by |steering| (`+0x18`): below 0x26666 0, 0x66666 1, 0x74ccc 2 (3 never), 0x154ccc 4,
0x1c6666 5, else 6 (640x480: 0x21999, 0x70000, 0xc8000, 0x133333, 0x1b0000 → 0..4, 5 above).
The screen's window; **the view through the windows**: `0x8b8f0(view's pixels, list
0x12b8a4[0x143e04])` (`0x12d9d9` = 3: another list, not read): a BAMF's spans, each from
its offset in the picture (÷ its width) to bank·65536 + its offset in the window (÷ the
window's width), going on at column 0 of the next row of both when the window's row ends
(the list's +0xc is the rate the picture is read at across: `0x9471c` gives `0x94aed`
the picture's pixels and the step table `0x15ed1c`, which `0xa6240` fills with (i · rate)
>> 16 for the longest span; each piece of a span, the first or one after a row's end,
reads the table from its start; the rows doubled down are the list's own spans; ported
2026-10-20, before that the doubled lists showed nothing: dash_emu's DETAILS and
OUT_DETAILS, equal on e1-e4).
**The mirror** (`0x5b040`, with `0x143e08`): its picture `0xf457c` copied at place 0
(`0x12b604`). Then the clip `0x5885c(0, 0, 0x102, 200)` (180 when `0xf6b74` ≠ 1) and the
wheel see-through at its place when the signed turn ≠ `0x12b8d0`: turned left
(`0x12b4e4`[i], `wstr wl06 wl12 wl18 wl26 wl34 wl45` at `0xea8b8`) when steering left, or
right on a mirrored track (`0x12dd28`), else `0x12b500`[i] (`0xea8d4`). Into the view's
picture (clip its whole): `tach` copied at its place, `sped` too (see-through for car 11),
**the needles** `0x2fc40`, the wheel's parts over the gauges see-through (`0x12b51c`/
`0x12b538`, or `0x12b554`/`0x12b570`: `0xea8f0`.. `0xea944`); back on the screen, the
two gauges' boxes (`0x12b884`, `0x12b894`: x, y, w, h of `tach` and `sped`) as clips and
the view's picture copied at (0, 0) through each. Then `0x2e4f0(car)`, `0x30214`,
`0x2f644`, `0x30d8c(car)` (the status bar's numbers: not yet), the map `0xf4584` at
(0x103, 0x7e) with `0x12b988`, `0x5c5d0` (the replay's buttons; racing, the watching bar `0x7ef14`, §4.22).
`0x83908` between the steps is EA's timer service, nothing drawn.
**Ported: every frame equal to the original's, on e1 and e3 with the wheel, engine and
speed set seven ways (`tools/dash_emu.py`, the routines not yet ported left out of both).**

**After the gauges, each frame** (`0x31684`'s end, on the screen's window):
`0x2e4f0(car)` the gear lever's gate, only with the map off (`0x12b988` = 0): `gate` at
its place, the knob (`nob1`, `lth3`..`lth1` at `0xea98c`, 4 when there are, `0x12b5e8`)
at one of the gate's 9 points (`0x12b83c`) by the gear (`+0x421`, `+0x41d`, `+0x3ed`,
`+0x3bd`), `gbak` without a gear. **Ported (2026-10-21, `hud.c` `gear_lever`):** the
state by `+0x421` (shown): < 11 with the wait `+0x3bd` < 1 → the new gear `+0x3ed`'s point;
≥ 14 the gear before `+0x41d`; 13 the gear before's x at neutral's y; 12 the new gear's x
at neutral's y; 11 with no wait → state 5; shown ≤ 0 → `gbak` once (when the last state
`0xea99c`, never reset, was not 0). Nothing drawn when the state is the last one. The
stick: one knob (`nob1`, the 512TR and the Diablo) centred on the point, or (`0x12b5e8` =
4: every other car) four parts from gate point 1 towards the point in quarter steps,
`lth1`, `lth2`, `lth3`, `nob1`, see-through. `0xea800` = 0, only read. `tools/dash_emu.py`'s
78 lever frames equal (512TR snapshots); the four-part stick seen equal in DOSBox (RX-7,
every gear). `0x30214` **the radar detector**: level
`0xea814` (5 at most): with any, `bglt` / `bgdk` (`0x12b9f8` flashing) at place 2; when
the level differs from `0x12b938`: as many `smlt` as the level at places 3..7, the rest
`smdk` (level 0: `bgdk` and five `smdk`). `0x2f644` the keys of the HUD (`0xf7b0a` the
status bar's mode: race / segment, or on City-like tracks 3 three ways; `0xf7b09` the
status bar on / off; the key `0x12d950` held the map on / off and the whole dashboard
again; `0xf7b0b` the view, `0x14b00`; each once per press, `0xea9a0`), and before them
the status bar drawn again when the steps pass `0x12b944`. `0x30d8c(car)` **the status
bar's numbers**: `0x12b948` (`wait`, network) aside; with the bar off or within the
first 360 steps only (< 360) **the start's lights** `0x2dbbc`: the strip blacked once
(`0x12b94c`), then five lights 0x41 apart at y 0: `srdd`/`srdl` (red, i < 3) and
`syld`/`syll` (yellow) dark while i > steps / 60, lit after, all `sgrn` from step 300 (a
sound each as it comes on, `0x5257c`, flags `0x145c9c`..), `0x12b944` = steps + 10 (so
the bar's picture comes back 10 steps after the last). After: the speed in red digits
`0x2ddc0` (x 0x7a; `+0x68`, or `+0x15c` for cars past the humans), the gear `0x2dee8`
(0xba), the place `0x2e368` (0x121, `0x516d4`), the time `0x2d5dc` (0x48: the player's
time from the lap table `0x14ea78 + 0x1d2·(+0x47d)`, + 300), the split `0x2e1dc` (0xe0),
laps `0x2e100` or miles `0x2df78`, the acceleration test `0x2d8c0`; `lost` (`0x145760`,
network) with `0x12b93c`. The HUD's pictures by name from `0x5f240`: `0x145758` tach,
`0x14575c` wait, `0x145760` lost, `0x145764` paus, `0x145784` sync, `0x145768` srdd,
`0x14576c` srdl, `0x145770` syld, `0x145774` syll, `0x145778` sgrn. **The lights, the
radar and the bar's return ported: equal on e1 and e3 with steps 0..359 and the radar
at 0..7 (`tools/dash_emu.py`); the numbers below.**

**load_dash's places** (`0x2e8d4` from `0x2ee4b`): a picture's attachment block is the
first of its chain of blocks (`+1`, 24 bits: the next) of type 0x7c: `+4` a count, from
`+8` pairs of 32-bit x, y. Each point on the screen = the picture's centre (`+8`, `+0xa`)
+ the pair + its corner (`+0xc`, `+0xe`; for the gauges their box's x, y). `dash`: 10
places `0x12b604` (0, 1 the mirror's corners; 8 and 9 then made relative to 2 and 3),
`tach` its count `0x12b5f8` of points `0x12b65c`, `sped` `0x12b5fc` at `0x12b74c` (then
point 12 the halfway of 11 and 13), `gate` 9 at `0x12b83c`. The lists:
`SIMDATA/DASH/<car>DL.FMM` (`DH`), a wwww of 4, into `0x12b8a4`.. Cars 3 and 11 show the
speed in digits (`0x12b5bc`, `num0`..`num9`, `numb` at `0xea960`).

**The needles** `0x2fc40(car)`: the engine's turns `+0x3c1`, at most the physics' `+0xac`
(the red line, 7240 the 512TR) + 0x3d4; point i = turns >> 10, t = turns & 1023: the end
= point i + (point i+1 − point i)·t >> 10, from the last point (the centre). The speed:
|`+0x68`| × 0x23d70 (mph; 0x39999 km/h with `0x15d140`), rounded, >> 16; per 10 mph (20
km/h) a point, the part between by `idiv` (truncated). In digits (cars 3, 11: at (0x99,
0x98) 4 apart, or (0x58, 0x62) 7 apart; see-through for 11): three, the two leading
noughts blank (`numb`). Each needle `0x8b195(centre, end (16.16), the colour record
0x12b5f4, widths 0xea71c, 0xea720 + 20·car, less 0x10000 at 320x200)`.
**The colour record** `0x8a390(r, g, b, palette)` (made by `race_colours` `0x2e74c` from
the car's `0xea710`: 0xff, 0, 0 for most, 0xff, 0x40, 0 for car 2): byte 0 the nearest
colour, 1..3 r g b, from +4 16 tables of 256: k + 1 sixteenths of the way (`0xa6040`:
a + (b − a)·t rounded) from each colour of the palette read back (`0x13cb48`, ·4) to
r g b, each the nearest (`0x80c80`, through the inverse table). (Called "the fog table"
before: it is the needles'.) **Equal to the original's on e1, e3, e5.**
**EA's smoothed wide line** `0x8b195(x0, y0, x1, y1, record, w0, w1)`: steep (|dy| >
|dx|) it steps a row at a time from the top end, the sides x ∓ w/2 of that end, their
steps the slope ∓ (w1 − w0) / (2·rows); shallow the same by columns. Cut to the bounds
`0x15ecc0`..`0x15ec90` across x and `0x15ecbc`..`0x15ec98` in y (0, and the view's size,
set in `0x2e74c`) by `0x8a507` / `0x8a76f` (a quirk: after a cut of the sides the steep
one checks its y against the x bounds). Each step: the side pixels blended through table
(16ths covered) − 1, the ones between the colour. Steps that may leave the window are
painted through the window's clip (`0x8a9d7`, `0x8adb2`; the near side only when inside
it, `0xa6060`, `0x96090`, the middle by `0x8a2a0`), the others straight into the rows
(`0x8ac1c`, `0x8aff7`). One pixel for both sides: blended by (both − 16), which below 1
reads before the table in the fast painter (never for the needles, a pixel wide or more).
**Ported (`poly_wide_line`): equal on 10,000 random lines of a pixel wide and more
(`tools/polyfill_emu.py wide`).**

**The map** `0x31cb4` (from `race_draw_frame` before `0x31684`; nothing with `0x12b988`
= 0, which `race_load_track` `0x5ba52` sets to 1, the mirror `0x143e08` to 0). At 320x200
into its 58x54 picture `0xf4584` (640x480: other places, `0x12c308`): the background
(the driver's seat, `*0x143df4` = 0 and `0x12ba2c` = 0: the map's dim background
`0xf4580` copied at (1, 1) once `0x12ba0c` says it was taken; else `mpbd` see-through),
a frame of `0x12ba08` (`0x8a0f0`: four fills), the clip (1..0x39, 1..0x35). Positions:
the nodes' x, z (`+8`, `+0x10`) >> 20 less the viewed car's node's (`0x13c24c[0x12dd54]
+0x48`), turned by that node's heading (`+0x18` >> 4: a turn of 1024, `0xf8448`), x
negated on a mirrored track; y up. **The road** in white (`0x13d5c4`) as lines (`0x8bb3c`)
from `0xea694`[track] nodes behind (480 for CY1) through 16 nodes apart for i =
`0xea6b8`..`0xea6dc`[track] (−29..30), round the map's centre (29, 29); on the City-like
track 3 i·13.5 apart; open roads (track < 3) kept to node 0 and at most 60 past the
segment's end `0xeaab4`[track·3 + segment], circuits wrapped by `0x12ba00`. **The
segment's end** (circuits: node 0): a red (`0x13d5a4`) bar 4 across the road, its angle
from the end and 8 nodes on by EA's arctangent `0x8bb7c` (the smaller over the larger as
a 32-bit fraction, rounded to 1/256, the byte table `0x12572c`; octants by sign and
which is larger). **The racers**, the last first (8 in the types above 1, 2 head to
head, 1 a time trial): within the range (the shorter way round) and |x| < 0x1c, |y| <
0x17 a dot `0x32ac0` (a 4x4 square of its marker colour `0x13d5e8`[i] at (x−2, y+1),
`pos1` over it at (x−3, y), clipped to rows 6..0x30) at (29 + x, 26 − y); else an
arrow at the edge: at x 0x2f head to head, else 0x32 − 7i; at the bottom (`aro1`, a
triangle under it) when the player's place `0x12b8c8` is 1 head to head / the racer's
`0x12bd33`[i] is behind it, else at the top (`aru1`). **The police** (car 2 `0x13c254`
with `+0x51a` & 4, `0x12dd08`, head to head): within 0x140 nodes and |y| < 0x1d a dot
flashing by `0x12ba04` (0..10: colour of marker 0 above 5, else marker 6).
**Ported (`hud_map`): equal on e1, e3, e5 at the snapshot's node and 8 others, mirrored,
with the police flashing and from outside (18 maps each, `tools/dash_emu.py`).**

**The status bar's numbers** (`0x30d8c` after the start's 360 steps; each routine draws
only when its value differs from its cache, all set to −99 by `hud_reset_shown` `0x2d150`, at every redraw of the dashboard). Digits by
`hud_digits` `0x2d47c(v, x, y, step, n, table)`, the most significant first, copied:
grey `0xf6c84` (→ `0x1456b8`), yellow `0xf6cb0` (yel0..9, `yelr` 10, `yeln` 11, `plus`
12, `mnus` 13, `yelb` 14 → `0x1456e8`), red `0xf6cec` (red0..9, `redb` → `0x145728`).
- `hud_speed_digits` `0x2ddc0(car, 0x7a, 1, 0xe)`: |speed| in mph (as the needle), three red
  digits, the leading noughts `redb`; cache `0x12b8e4`[viewed].
- `hud_gear` `0x2dee8(0xba, 1)`: the gear (−1 `yeln` neutral, below that `yelr`
  reverse, else gear + 1 as a digit); cache `0x12b92c`.
- `hud_place` `0x2e368(0x121, 1, 7)`: the place worked out (below), a yellow digit at
  0x121; the record's place written; cache `0x12b918`, the place `0x12b8c8`.
- `hud_race_time` `0x2d5dc(t, 0x48, 1, 8, 3)`: the time from the lap table (`0x14ea78 +
  0x1d2·player`) split by `hud_time_split` `0x2d3ac`, minutes, seconds, tenths; caches
  `0x12b8fc`/`f8`/`f4`. A new best lap (`0x13c208`) shows the bar again first (so the
  bar is blank for a frame, as in the original).
- `hud_split` `0x2e1dc(0xe0, 1, 8, 3)` while `0x14ea70` > 0: `plus` for a split below 0,
  `mnus` from 0 on (the opposite of the status bar's own picture), then the time of
  |split|; cache `0x12b8e0`.
- `hud_laps` `0x2e100(car, 0xde, 1, 8, 0xf5, 1)`: `lap#`, the lap (at most `0x12dd14`;
  one digit after `yelb`, or two), and the race's laps at (0xf5, 1); cache `0x12b928`.
- `hud_miles` `0x2df78(car, 0xd9, 1, 8, 2)`: `mile`, the road left to the finish node,
  48 units a node over 1600 a mile (1000 a km), + 1 tenth while short of it (else 0):
  the whole and the tenths 8 + 2 on; in km (`0x15d140`) the whole in two places, which
  overlap the `MILES` label, as in the original. Caches `0x12b91c`, `0x12b920`.
- Mode 2 (the acceleration test, `0x2d8c0`; 2026-10-20): `stat` then `acct`, then each
  figure only when it differs from its shown copy (`0x12b950`..`0x12b964`, cleared by
  `0x2d150`). The figures are made while driving Rusty Springs' time trial
  (`car_physics_player` `0x417a2`, §4.21): **`0x2d71c(t60, t100, quarter, speed)`** keeps
  each time above 0 (above `0x4b0` it becomes 0) in `0x12b968`, `0x12b96c`, `0x12b970`
  and `0x12b974` = speed · 224 / `0x640000` (mph); **`0x2d778(from60, from80)`** (capped
  at `0x1f4`) `0x12b97c` = from60 · `0xa76` / 12000 · 33 / 10 and `0x12b978` = from80 ·
  `0xdf3` / 12000 · 33 / 10 (feet). Times by `0x2d7f0`: seconds = v / 60, tenths =
  (v − seconds · 60) / 6, two digits at x and one at x + 2 · 8 + 2 (y 1), x `0x14`,
  `0x45`, `0x76`; mph 3 digits at `0xaa`, `0x12b97c` at `0x11f`, `0x12b978` at `0xee`;
  `0x2d50c` the yellow digits (`0x1456e8`), leading zeros blank (entry 14) but the last.
  All cleared when the car is loaded (`0x2e8d4(1)`, with `0x12ba2c` and `0xe80f0`).
**The dashboard's keys, `hud_keys` `0x2f644`** (2026-10-20), from both frames (`0x31ba1`,
`0x315ae`): first the status bar if due (steps > `0x12b944`); then, with the latch
`0xea9a0` (1 at the start; any of these keys clears it, none sets it): **F8** the bar's
mode, on Rusty Springs' time trial (track 3, type 0) (mode + 1) % 3, else 1 − mode, the
bar shown, drawn; **F7** the bar shown or not; **the map key** (settings `+0x5f4e`, `M`,
or the joystick's) the map on or off: off → `0x12b8d4` = 1, and from outside or without
the dashboard `0x2f84c` (the map corner's tiles and border again; the port draws the whole
screen round the view again instead); `0x12d9d9` == 3 → `0x12d9da` = 1 (not ported);
**F9** (not in a replay `0x15d13a`, not stopped `0x145c90`, camera mode ≤ 1):
`0x12ba2c` ^= 1, camera mode 0's kind `0xe80f0` = it (the bumper's eye), `camera_set_mode`
`0x14b00` again, `0x143e00` = 999 (the screen round the view drawn next frame).
`race_draw_frame` `0x5c008` uses the dashboard only with camera mode 0 and `0x12ba2c` 0;
coming back to it the whole dashboard is drawn (`0x143e68`).
**Ported (`hud.c` `hud_keys`, `accel_figures`; `race.c`).** Checked by eye against the
original (build/circ/h_*.png: F8's three modes, 5.1 / 10.7 / 13.4 at 111 mph, F7, the map
off, F9 there and back); not pixel for pixel, as those were taken in a 640x480 race.
**Ported (`hud.c` `bar_numbers`): equal pixel for pixel on e3 in 27 cases (speeds,
gears, places, times, splits both ways, laps, miles, a new best lap; `tools/dash_emu.py`).**

**The views from outside the car** (2026-10-19; `hud.c` `hud_outside_screen`,
`hud_outside_frame`). When `0x143e00` (the view changed): `0x14568(layout, 0)` and
**`0x3081c`**, into the view's picture: its four tiles `0000`..`0003` (32x32, names
`0xeaa20`[(i & 1) + 2j]) over the whole picture from row 13, the status bar (`0x2f2a4`),
the border round the view's box (layout `+0x24`; `b00t`/`b00b` along, `b00l`/`b00r` down
and their last pieces, the corners `b000`..`b003`, each drawn centred by `0x812bc`: x −
its `+8`, y − its `+0xa`), the small rev counter `tach` see-through at its own place, and
the numbers' caches reset (`0x2d150`). Each frame then **`0x314e4(car, redraw)`**:
`0x30b64`: with redraw the rev counter put in again and the whole picture copied to the
screen; else only the view's spans by a BAMF list: `MASKLO.B<l><d>` with the map on, else
the entries of `MASKLO.MMB`, by (layout − 1, detail); then the keys' bar `0x2f644` (ported
2026-10-20), the numbers `0x30d8c`, **the small needle `0x31168`**: the angle = min(rpm, red
line + 0x3d4) · 600 / red line + 0x1a9, the end = the centre (w/2 + `+0xc`, h/2 + `+0xe`)
+ r · (cos, sin), r = hw − hw/4 − hw/16, the start a quarter of the way out; the old
needle's two halves (`0xeaa98`..`0xeaaa4`) erased in clip boxes ±2 by drawing the `tach`
again, the new one by the smoothed line `0x8b195` (widths 1, the needles' record
`0x12b5f4`); then the map at (0x103, 0x7e) and `0x5c5d0`. Mode 0 again (`0x143e68` = 0):
the whole dashboard (`0x2d284`). **Equal pixel for pixel on all 11 snapshots in 7 frames
each (the crash's view, the revs from 300 to 9999; `tools/dash_emu.py`).** Not ported:
the tickets, `0xeaaa8` (the background drawn again), detail 3, the replay's buttons, 640x480. (Since: 640x480, detail 3 and the replay bar at both, §4.24.)

### 4.12 The race's timing (2026-10-19)

**Who is timed** (`race_laps_init` `0x506dc`): the result `0x13c178` = 0; by the race type
`0x12da20` (0 time trial, 1 head to head, 2 single race, 3 tournament): the racers
checked `0x13c18c` = 1 and the humans `0x13c190` = 1 in a time trial, 2 and the players
head to head, the field else. `timing_setup` `0x685e4`: each racer's record (0x1d2 bytes
from `0x14ea78`) cleared where the race writes it: the lap times (`+8` + 4k), finish
`+0x1c4` (`0x14ec3c`), place `+0x1d2` (`0x14ec4a`), top speed (`0x14ec44`). **The
splits**: 85 points (5 a lap, 17 laps) `0x14e62c`, point k of lap i = the table
`0xf701c` row (3·track + segment)·5 + k plus i · the finish node; passed: mine `0x14e794`
(times `0x14e79c`), theirs `0x14e798` (times `0x14e904`), both 0 in a race. A time trial
sets theirs passed to 85 and fills their times with the times to beat: an open road the
segment's record (CONFIG.DAT +0xb5 + track·0xa6b + k·0x27 + 0x1b) shared out by the row
3·track + segment of `0xf7238` (16.16 fractions, `mulr`), then the record; the fifth
point moved to the finish when within 0x27 nodes. A circuit: the first lap by the row
3t + 0 times the first lap's share (row 3t + 2), the others the rest of the race's record
(6 + 10·`0x12dd24`) shared over the lap limit − 1 by row 3t + 1; 300 added to each (the
start's wait). (The original first takes the other player's splits from the record's
`+0x44`, kept from the race before; the port keeps none yet.) The split `0x14ea6c`, its
shown steps `0x14ea70`, the camera switch `0xede80` = 0; the segment times `0x14ea74`[3]
cleared on segment 0.

**Every step** (`race_update_cars` `0x519d8`, in state 1): `car_check_finish` `0x51760`
for each racer with `+0x51a` & 4, by `car_race_state` `0x516d4` (circuit: lap < 1 →
1, lap > laps → 2; open road: node < 12 → 1, past the finish node → 2; the port's
`drive_race_progress`): 1 for a human: turned back, its lap time `+1` and finish −999;
2: finished, lap time `−1` and finish = steps − 300; the player's place (not in a
replay) one more than those finished before. Then **over** when every human has
finished (or turned back) and goes slower than 0x8000; two players: the first in and
stopped sets `0x145c90`, and the view goes to the one still racing (`0xede80`, camera
`0x13530` mode 3: not ported). Result 1 if any human finished, else 0x10 (all turned
back); `race_main` ends on it. Then the driving, the two orders sorted by `race_sort_order`
`0x518c0` (bubble sort, a swap when the first is further: `0x13c298` by node then
across `+0x60`, `0x13c274` by progress `+0x4c` then across; both the identity from
`cars_create` `0x51388`), the world, sorted again, and `splits_pass` `0x68d7c` (each a
split at most a step: mine when the player's progress passes it, theirs (a race only)
the rival's: the other car with two, else the leader, or the second when the player
leads) and `split_update` `0x68d00` (theirs − mine at the last split both passed;
when it changes, shown for 90 steps, counted down).

**The clock shown runs fast.** Times are in 1/60 s of the step counter `0x145c94`, which
goes on 2 a step; at the default 33.3 steps a second (§4.3) that is 66.7 a second, so a
race's "second" is 0.9 s of real time (the port does the same: 24.3 s after starting
`--race cy1` shows 00:21.9 with the 5 s of lights taken off).

**The place** (`hud_place` `0x2e368`): finished, one more than those in before; a time
trial 1 ahead of the time to beat (split > 0) else 2; else each car's place from
`0x13c274` (count − index) into `0x12bd33` (the map's arrows) and the records; head to
head 1 or 2 by the other car's index, else count − index.

**The best lap** (`car_check_lap` `0x5056c`): the player's lap 2..16 done, the lap's time
below `0x13c20c` (the record's, CONFIG +0xd0 + track·0xa6b, at the start) becomes it and
sets `0x13c208` (the status bar shows it). (Lap 1 is skipped in the port: the original
reads the word before the table.)

**Ported (`port/src/timing.c`): equal to the original step for step in 143 runs on all
11 snapshots, 13 kinds of race (won, lost, dead heat, turned back, three time-trial
segments, circuit, circuit time trial, single race, tournament, two players;
`tools/timing_emu.py`).** Not yet: the race-end screens and the records written, the
tickets and the record's other fields, the speech (`0x722b4`, `0x72438`), the network
branches, the old race's splits (`+0x44`), a computer car's gear from `+0x308`, km/h from the
settings.

### 4.13 The opponent's driving (2026-10-19)

A car the computer drives is kind 3 (`+0x512`); every step `drive_opponent` `0x7b34c`
moves it along the road, and every fourth step `traffic_police_tick` `0x7a040` chooses
its target speed (`+0x160`) and its place across the road (`+0x164`). A racer is a car
whose slot `+0x516` is in [humans, field); the traffic and the police are the cars after
the field (`+0x174` & 8 police, & 4 racer, `+0x51e` & 0x404 the wrong way).

**At the start** (`load_traffic_cfg` `0x7de84`'s part): the settings block `0x15d2bc`
(0x1ac bytes) is written by `traffic_defaults` `0x63f50` as a run of `mov [eax+d], imm32`
stores (one `mov edx,[eax+0x24]` scheduled among them), then +0x24 and +0x28 × 60 and
+0x48 = mulr(+0x48, 0x471c); the port reads the stores out of the program. +0x34 is 0 in a
time trial or a head-to-head on a circuit. **Each racer's driver** (`0x7dde8`): a set by
the race type (time trial 7; head to head 5, or 6 with option bit 4; single race 3, or 4
when cars 2 and 3 are the same model; tournament by the player's car {2,0,1,1,0,1,2,2}),
the letter at `0xf4038` + set·8 + grid place (`0x12dca0`, 7 − slot), the record at
`0xf4070` + 0x8d·(letter − 'A') into `0x15d490`[slot]: its +1 + 4i the catch-up factors
(i = 0..20, by progress behind the leading player / 10 + 10), +0x55 a top-speed factor,
+0x59 the bends' slowing, +0x65 / +0x69 the sliding limit and rate. The stretches of road
round the two racers (`0x15d29c`, `0x7814c`, `0x77f70`, `0x77cfc`): progress −12 .. +43
(the inner window), widened by 8 behind and 11 ahead, met half way when they overlap;
re-ranked leader first each tick. **The random numbers** (`0x38c10`): multiplier = seed |
1, state = 3·seed + (seed even), seeded by `time()` `0x91d9b`; a number is the product's
bits 8..23, the low 16 bits kept as the next state. Braking a step `0x15d470` = 0x5999.
**No road follow is done for a racer before its first step**: its first `drive_check_lap`
moves it from the grid's node 20 to 19, and that sets its road axes (`car_road_frame`
`0x7dbd8`). The port did one at set-up and the car never moved.

**A step** (`0x7b34c`): a racer's speed |v| = max + min/4; `drive_check_lap`; on a new
node the road's frame (`0x7dbd8`), the car's turn = the road's axes (negated the wrong
way) turned by its steering. The steering (`+0x18`) eases to `+0x1c` through a yaw rate
(`+0x74`; ±0x23333 a step for a racer, 0x13333 the traffic; else −9 × the steering,
snapped within 0x30000); turned beyond 0x160000 (0x200000 the wrong way) it spins out
(`0x1ca9c`, kind 4); before progress 150 it does not steer. Every `+0x4` (body wait)
steps — 4 a racer, 8 traffic, 6 police — **the plan** `0x7cf98`: the acceleration from
the car's table `+0x178`[speed + 1] (made from its PDN at load, `0x1e9cc`) times the
catch-up factor (settings +0x104/+0x158 + 4i, head to head / other), none before step
0x136 or in the air; towards the target speed by it, down by the braking; slowed by a big
steering error; the race's top speed noted where the road's limit allows it; the
velocity along the road (or along the car while it turns); the slide across towards its
place, limited by the driver's record (0x7cf04) and the speed's steering table
(`0x7d815`), adds the road's side axis; put down on the road (or left in the air over a
crest near a player above 0x1b0000). Then: on an open road in an edge lane past progress
0x46, × 255/256 a step; off the road's sides it is turned back (`0x7b178`) or stopped;
moved by 1/32 + 1/512 + 1/128 + 1/2048 of its velocity (x the other way).

**Every fourth step** (`0x7a040`, unless `0xf74ac`): the ticks `0x15d4b8` += 4; the
stretches; the leading player (most progress) `0x15d48c`; each car refreshed (`0x786d4`:
its road frame, place across, reach; for a computer's car the target speed, below); the
lanes each car covers (`lane_of` on its two sides). Then for each computer's car not
crashed: three scores, the lane to the left, its own and to the right: a start's
push to keep its lane (single race and tournament before its grid place's turn), a head
to head's pull to the right side, the racer's engine (`0x79e28`: past the finish it pulls
over by its grid place; gear and revs from `+0x308`), staying put alone in its lane on a
straight road, minus for the edge lanes (more when the road beside is narrower than the
car) and much more outside the road, plus away from a narrowing; **the cars ahead in each
lane** (`0x78c88`: the nearest within the stretches, the speeds they leave it, 0x50000 a
node between) scored by how much they fall short of the target (`0x78a80`); near a player
the lanes it blocks scored down (`0x79b60`). The best lane's middle, kept on the road,
moved by the car's own offset `+0x33c`; a lane change beside a car cuts the target to that
lane's speed unless it can slide over first (`0x7850c`). **The target speed** (`0x786d4`'s
part, `0x7e64c`): a racer the leading player's speed, then the road's limit
(the TRI's speeds, 3 bytes each four nodes) × its top factor (+0x170 ×
the driver's +0x55) × (1 − the bend's turn) × the catch-up (settings +0x5c/+0xb0 + 4i ×
the driver's), held between 0.8 and 1.2 near a player, slowed before progress 0x41, 0 past the finish
by its grid place; the traffic by its grip (`0x7e548`), the oncoming slower the faster its
stretch's racer (`0x78660`). The traffic sometimes picks another place in its lane
(`0x7dd33`, 1 in `0xf74f0` = 0x28).

**Ported (`port/src/opponent.c`): equal to the original step for step in 220 runs on all
11 snapshots** (374 with the cars meeting, §4.14) (`tools/opponent_emu.py`: the race cut to the player and the opponent,
three driving scripts, 15 settings: single race, no traffic option, the lane rule off,
fast, steering, off its lane, off the road, in the air, changing gear, past the finish,
before the start, and the player catching and passing a slowed opponent), and the
settings read from the program equal to the snapshots' blocks. With two cars, the branches for
several cars in the lanes (`0x78c88` 241 of 618 instructions run, `0x7850c` 24 of 106,
`0x78660`, `0x7e548`, `0x7e5e4` none) wait for the traffic to be checked. `--race cy1` races one
opponent (`RENFS_SEED=n` for its random numbers). The traffic's coming and going: §4.16.
The police: §4.17. The horns and engines' sounds: §4.18.

**EXPLODE** (2026-10-21, ported). Typed in Control Central it sets `0xf74a4` (cleared in a
tournament). Only one place reads it while racing: the lane planning, `0x7ac03`, between
`0x78c88` and `0x78a80`: with one player (`0x12da30` = 1), the player's horn held (car 0
`+0x4ba` = 1) and the car within 10 of the player's progress `+0x4c` (either way), `0x78370(car,
0x190000, 0x190000, 0x190000)`: `car_crash` `0x1cb14`, the spin `+0x3c..` given `s·m[0..2] +
b·m[6..8] − a·m[3..5]` (the port's `crash_hard_hit`), the chase dropped (`0x781ac(0)`) for a
car with `+0x174` & 4, then the knock `0x1e838`. So every 4th step the horn throws the
traffic and police around the player into the air. Dead: `0x78c30` (the handbrake's, needs
`0xf74f8` = 4, never written) and `0x79ffc` (never called, the other caller of `0x78370`). **Checked**: `tools/opponent_emu.py`'s runs "explode" and
"explode beside" (`0xf74a4` = 1, the horn in bursts or the throttle) equal on e1 and e3,
with and without `--full`; the opponent beside thrown (it ends at node 113 / 88 against 186 /
192 without the code). `RENFS_EXPLODE=1` in `--race`.

### 4.14 Cars meeting cars (2026-10-19)

After every race step's driving, `cars_meets_world` `0x1c878` (the hit's wait `0xea5d0`
counted down, else the hit level `0xea5d4` back to 0x6666; the tick every fourth step)
goes through the cars by node (`0x13c298`). A player's car: caught by the police
(`+0x176` & 1) it steers towards car 2, the police car (`police_steer` `0x1c800`: the
angle to it less its heading, half of it added to half its steering, held within
±0x140000, the throttle off); outside the lanes (`lanes right × width < +0x80`, or the
left's) it meets the roadside objects (`0x1ebf4`, §4.15). A car not of kind 6 with
`+0x51a` & 6 = 6 is tried against the cars after it in that order (round past the last
when within 4 nodes of the loop's end) until one is more than 2 nodes ahead (only while
the loop's end is more than 4 away), each with `+0x51a` & 5 = 5 and both |Δx| and |Δz|
under the sum of their radii (`+0xc0`→+0x70).

**`car_car_collide` `0x1c6a4`**: both bodies made from the cars (unless of kind 4,
`0x1cbb0`), `car_car_impact`; a hit at or above the hit level sets the level to it +
0x8000, the wait to 10, and shakes both views: s = min(hit, 0xa0000), negated if
`rand_below(10)` > 5, roll shake ±s (`+0x429`, opposite for the two), pitch shake
(`+0x42d`) ±s by another draw, all four phases `0x1c`. (Its sound `0x7fb58`, `0x52e40`:
not ported.) **EA's random numbers** `0x91e04`: six words `0xf8a8c`.. s0..s5, the sum
s5 + s4 + s3 + s2 + s1 + s0 with carries, each partial sum stored back (s4, s3, s2, s1),
then s5 counted up with its carry running up through s4..s1 into the result, s0 = the
result. Its seeding `0x91e6a` is never called: the image's words are the start.
`rand_below(n)` `0x52580` = (low 16 bits × n) >> 16.

**`car_car_impact` `0x1e314`**: `box_separate` `0x2b864` and `box_impulse` `0x2afb8`;
nothing if either fails. The limit is 0xd0000 (`0xea5b0`), 0x100000 (`0xea5b4`) with a
player in it. **A light hit** (the push `0xea668` ≤ the limit, and no player in it
already thrown): each car not of kind 4 driven again by its own kind from its body
(`0x1cd40`; the second is asked **by the first car's kind**, as the program does), both
`+0x15c` from their bodies (`0x1e6cc`). (A replay highlight `0x5c6d4` when both go faster
than 16: not ported.) **A hard hit**: both kind 4, a body moving faster than 0x8000 on any
axis waits 300 steps, and a player's car not yet thrown gets its road frame, `+0x4f2` = 1
and **the knock** `0x1e838`: spin = spin/256 × `0xea5b8`/256 + `0xea5bc`/256 × the road's
across/256 + `0xea5c0`/256 × along/256, the speed += `0xea5c4`/256 × its own up axis/256.

**`box_separate` `0x2b864`** (the live path; the switches `0xea62c`..`0xea64c` are never
written: 0 0 0 1 0 0 0 1 0): the turn between the boxes M[r][c] = A's axis r · B's axis c
(p8 products), with B's half sizes and A's (P5 = M·hB, P6 = M·hA). Up to 32 times: where
B is from A, in A's and in B's frame; **`box_contact` `0x26324`**: the first of (1) the
middles of B's six faces inside A (+ then −, axes 0..2), the normal ±B's axis; (2) A's
inside B, the normal ∓A's axis; (3) B's eight corners inside A (signs +++, ++−, +−+, +−−,
−++, …), (4) A's inside B, the face for a corner: in a fast hit (|B.vel − A.vel| > 0xf0000
on an axis) the box's axis most along the moving body's speed, else the face it is least
deep behind, turned to the corner's side. "Inside" includes the faces. No contact: done.
At the first contact the push is chosen: **along their speeds** when they close faster
than 0x140000 on an axis and either are within 0x3333 of each other or the closing
direction is within 0xd999 of the line between their middles (both made unit: the speeds
by `0x2bf9c`, halved first while too big; the line by `0x2c25c`): −1/|ΔV| (`0x3e7fc` of
the length `0x2c2d8`) / 256 × each body's own speed / 256, / 32; **else apart along the
line**, each by the other's share of the weight (`0x2c1c0` unit, fix_div shares, / 4,
then >> 14 and >> 5, or >> 19). Each pass adds the pushes. **`box_impulse` `0x2afb8`**:
rA, rB from the middles to the point, cA = rA × n, cB = rB × n (`0x3e880`, rounded);
closing = B.vel·n − A.vel·n − A.spin·cA + B.spin·cB; resistance = half of each 1/mass
and of each 1/inertia × |c|²; j = fix_div; nothing if j < 0; j × `0xea680` (the bounce)
is the hit `0xea668`; the speeds ± j·n/mass, the spins ± (r × j·n)/inertia. (The force
feedback `0x4d7a4` when `0x15d144`: not ported.) Closing faster than about 180 m/s on an
axis overflows the length and `0x3e784`'s square root never ends: the original hangs;
racing never comes near.

**A computer's car thrown about** (kind 4, `0x1cfe0` from `0x1de8a`, also a player's car
of kind 4 not crashing): t = its up axis · the road's up. Waited out (`+0x80` = 0) and
upright (t > 0xf70a) or out of every player's stretch (`0x7bf94`): **stood up**
(`car_stand_up` `0x7daec`: up = the road's, across = up × along made unit, along =
across × up), a police car's chase dropped (`0x781ac`(0)), kind 5 (`0x1cd40`), the car
from its body. Else, waiting: one step less; still moving (0x18000 on any speed or spin)
or neither on its wheels, roof nor side (|t| between 0x8f5 and 0xf70a): the wait =
(wait & 3) + 60 (police: chase dropped). A car longer than 3 m from middle to end not
crashing is stood up every step, its spin kept about the up axis only. Then the touching
tail and the car from its body as for a player's car. **Kind 5** (`drive_kind5`
`0x7d9e4`): its yaw rate eased to 0 by 0x13333 (0x23333 a racer); near a player it drives
with target speed 0 (`0x7b34c`); out of their sight: kind 3, standing along the road (the
turn the road's axes, no speed, on the ground). **A player put back after a crash**
(`0x7c030` from `0x1d8b3`): the chase dropped (`0x781ac`(1)); for each car after the
players (the field on a circuit, all cars on an open road): its chase flags (`+0x174`
& 0xfff7dbff, `0xf74a8` 0); a racer of kind 4 or 5 within the road's edges (−node+2 ×
0x2000 .. node+3 × 0x2000) is put back as kind 5 does (kind 4 through `0x1cd40`(3) first);
the traffic, and cars of kind 3 out of the stretches or police, are placed again by
`0x7cac8` (§4.16).

**Ported (`port/src/collide.c`, `crash.c`, `opponent.c`): equal to the original.**
`tools/collide_emu.py`: 22,000 random pairs of the snapshots' two cars (turned, placed and
moving at random up to 48 m/s an axis), the separation's touch, point, normal and both
places, the push's hit, speeds and spins, and 1000 of EA's random numbers, on all 11
snapshots; every instruction the switches leave live in `0x26324`, `0x2b864`, `0x2afb8`,
`0x2bf9c` and `0x2c1c0` run. `tools/opponent_emu.py` with the cars' meeting on (EA's
random numbers and the hit compared every step), 374 runs on all 11 snapshots: the
opponent put beside the player, ahead of it standing (the player ramming it: hard hits on
d2, d4), behind it closing (188 touches as it pushes a braking player), behind it fast (a
hard hit, both thrown, the opponent stood up, the player put back and the opponent with
it), and started thrown about (tumbling, on its roof, rolling; near the player or far
ahead; with the player's car thrown at a wall too, so that the player is put back while
the opponent still lies there). Not run: the police's parts, the long cars (traffic) and
kind 6. `RENFS_PLACE=s:a,c,v` puts the opponent so in `--race`. The traffic's placing: §4.16. Not yet: the police's parts, the sounds.

### 4.15 The roadside objects knocked over (2026-10-19)

Signs, lamp posts and the like (the TRI's placements, 16 bytes each at `0x13bb94`, `+0` the
node as an i32, `+0xa/+0xc/+0xe` the place from the node's point as i16 << 8) fall when a
player's car drives over them. Only humans: `cars_meets_world` `0x1c878` takes
`w = lane_width(nd[4] & 15, nd[1] >> 3) << 8` and calls `car_objects_collide` `0x1ebf4` when
`(nd[4] & 15) × w < +0x80` (the car further right than the lanes) or
`(nd[4] >> 4 & 15) × −w > +0x80` (further left).

- **`0x1ebf4`** keeps the car's place in the list: `+0x4ee` the index, `+0x4ea` its node
  (both 0 from `car_reset` `0x50830`). First, while the car's node equals `+0x4ea` and the
  index is not 0, it steps back (index − 1, `+0x4ea` = that placement's node). Then, if the
  car's node ≥ `+0x4ea` and the next placement's node is not −1, it walks forward: the
  current one is tried (its node the car's, its byte in the fallen array `0xea5d8` [2i]
  still 0, and under the car); a hit sets that byte to the step counter `0x145c94` / 1800
  + 1, calls `0x5004c` and returns 1; else index + 1, `+0x4ea` = its node, and the walk ends
  (0) once the car's node < `+0x4ea` or the next node is −1. Otherwise, the index not 0, the
  same walk backwards, ending once the car's node > `+0x4ea` or the index is 0.
- **`car_over_object` `0x1ee80`** (x, y, z on the stack, `ret 0xc`): dx, dz from the car's
  place, a = yaw >> 8; across = mulr(cos a, dx) − mulr(sin a, dz) within ±`+0x64e` / 2,
  along = mulr(sin a, dx) + mulr(cos a, dz) within ±`+0x64a` / 2 (EA's sin `0x80b00`, cos
  `0x80b70`). A hit also starts the wheel's force feedback (`0x4d730`) and, near the camera,
  a sound (`0x52d2c`): neither ported yet.
- **`0x5004c`** (`road_force_gather`): `0x136610` = `0x136614` = −1 and the camera's flags
  `0x129f08` |= 1, so the road ahead and the mirror's are gathered again next frame and the
  object is drawn lying down (`0x4f7d4`'s fallen branch, already in `road.c`).
- **`car_model_extents` `0x65160`** (called at `0x65e39` after the model's polygon groups):
  the near ORIP's body points (count `orip+0x14`, offset `orip+0x18`), min and max on each
  axis starting from 0; `+0x64e` = (max x − min x) << 9, `+0x652` y, `+0x64a` z: the box
  `0x1ee80` uses.

**Ported (`port/src/collide.c` `objects_collide`, `car.c` `car_extents`, `road.c`
`road_refresh`): equal to the original.** `tools/drive_emu.py` compares the fallen array
every step (and counts the knocks) on all 11 snapshots: 408 runs equal, 83 of them knocking
an object over; `0x1ebf4` 203 of 213 instructions run (not: the forward walk reaching the
list's end), `0x1ee80` all but the force feedback. `tools/opponent_emu.py` too (386 equal);
`tools/cars_emu.py` checks the box against the snapshots' `+0x64e/+0x652/+0x64a`. In
`--race cy1` (`RENFS_SEED=7`, gas from retrace 420) the "65" sign at node 74 falls at about
retrace 1100; `RENFS_VIEW=4` shows it from outside.

### 4.16 The traffic's coming and going (2026-10-19)

The snapshots hold the whole field: the player (car 0), the rival (1), the police car (2,
`+0x174` & 8, `+0x51a` = 3: not driven until it comes, §4.8) and six traffic cars (3..8,
`0x12da3c` = 6). **At the start** (`cars_create` `0x51388`): every car slot = index;
`car_setup` `0x51200` gives a computer's car kind 3, `+0x51a` 3 (then | 4), its z 1 m, node
and progress 0; `car_setup_body` `0x512e8` its size from the PDN, or for the traffic (a PDN
without the three halves) from the near model (`+0x78` = `+0x64a`, `+0x7c` = `+0x64e`, the
body's halves the model's / 2, `+0xbc`, `+0x84`, the radius `0x2c2d8`); so the rival's
`+0x78/+0x7c` are its PDN's, not `0x51200`'s 0x50000/0x28000. `load_traffic_cfg` `0x7de84`:
`0x15d480` (waiting) 0, `0x15d154` 0, `0x15d158` = the traffic, `0xf74a8` 0.

- **`car_move_on` `0x77de8`** (from `drive_opponent` before `drive_check_lap`, for a car not
  a racer or a racer before progress 2): progress < 2 → placed with a wait (`car_place`
  (1); the traffic's `+0x158` stretch 0 first); else (field ≤ 2) out of both racers' outer
  windows (`in_outer_window` `0x77c78`: `[zone+2, zone+1)`) → `+0x158` + 1 and
  `traffic_place_again` `0x77d3c`: placed in stretch `+0x158` (`car_place(0)`); while it
  lands inside an inner window (`0x77cfc`) the next stretch; past both: stretch 0 and
  placed to wait (`car_place(1)`). (A police car: its own branch, §4.8.)
- **`car_place` `0x7cac8`** (car, wait): `+0x405/+0x409` 0 (the player's chase bits off);
  `place_where` `0x7c5b4`: the side (1 the racers' way, 0 coming): behind the player's car
  (node − 12, the police − 18) for `+0x175` & 0x24 or the police on a one-way road (node
  0's left lanes 0); else at random (> 0x7fff), 1 on a one-way road, the traffic's count of
  its old side (`0x15d154` coming, `0x15d158` going) down, the side flipped when the new
  one already holds ≥ traffic − 1, its count up; the node: the stretch's inner window's
  start − (r & 15) when the side is 1 and the stretch's racer stands (`+0x70` < 1), else its
  end + (11·r >> 16); at least 0, masked. The place across: a random lane on that side,
  its middle (`+0x164` = w·((r·lanes >> 16) + 1) − w/2, or the left's negated), plus the
  car's own `+0x33c`; a racer on its grid place instead (`place_racer_grid` `0x7c7e8`,
  unless the rival is a traffic car, `0x12da9e` = 9). Then **`place_at` `0x7ccc0`** (`ret
  4`): node, progress, `+0x90` grip `0x15d2f4`[r & 3], a new lane place (`car_lane_pick`
  `0x7e5e4`), waits and steering 0, `+0x88` 100 / 160; `place_flags` `0x7c3f8`;
  **`place_speed` `0x7c4d4`**: going: (settings +0x48 + the stretch's racer's `+0x70`) / 2
  by its grip (`0x7e548`; a racer's `+0x15c` 0; a `+0x174` & 0x404 car the player's road
  speed), `+0x174`/`+0x175` & 0x10 off; coming: −`0x78660`, `+0x175` | 0x10; `+0x164`;
  **`place_on_road` `0x7c294`**: `0x7dbd8`, the node's point, z + r, + across × the road's
  side axis, `drive_check_lap` (and the frame again on a new node), turned with the road
  (rows 0 and 2 negated coming); the grip again; with the wait (not a racer unless the
  rival is traffic, not police, not a human): `+0x84` = (settings +0x30 · r >> 16) + 1,
  kind 6, `0x15d480` + 1.
- **`drive_opponent_wait` `0x7e0c8`** (kind 6): the traffic's `+0x158` 0 (1 at random while
  the leading racer's node < 0x78), held at node 1 at (8, 200 + 10·slot, 8) m; `+0x84`
  counted down; at 0 it comes (`0x15d480` − 1, kind 3, `0x77d3c`) once the furthest racer's
  node ≥ slot·16 + 15, fewer than settings +0x34 cars are on the road (`0x15d480` >
  traffic − +0x34; 4 on CY1, 0 in a time trial or on a circuit's head to head) and the
  race has traffic (`0x12da28` & 4 off); else it waits another step.
- **`0x7c030`'s loop** (the player put back): every car's chase bits off and `0xf74a8` 0;
  a car not a racer that is thrown about, police or out of the inner windows, outside the
  field, goes back to waiting (`car_place(1)`; a waiting one's count first taken back).
  **`chase_drop` `0x781ac`** (p): with police on, the police car's `+0x175` & 0xdb and
  `+0x176` & 0x87 (with p & 0xf7 first), the player's `+0x175` & 0xdb, `+0x176` & 0xfe,
  `0xf74a8` 0; called by the put back (p 1, with a chase), a hard hit of a racer (0), a
  police car's spin-out (0), its stand-up after a crash (0) and kind 5 (1).
- The race's top speed (`0x14ec44` + 0x1d2·car) is kept for every car, the traffic too:
  `0x14ea80` holds 9 results (the port's table had 8, and the ninth car wrote past it).

**Ported (`port/src/opponent.c`, `crash.c`, `car.c`, `race.c`): equal to the original.**
`tools/opponent_emu.py --full`: the whole field of the snapshot (9 cars, the police car
parked; its coming and chase since added, §4.17), every
car stepped by its kind (6 = `0x7e0c8`) and compared every step, the counts `0x15d480`,
`0x15d154/8`, `0x12da9e`, `0xf74a8` too; 419 runs (the 34 settings and 3 "two-way")
on all 11 snapshots, up to 26 placings in a run. **CY1 is one-way** (every node's byte 4 =
0x02: no lanes the other way), so all its traffic goes the racers' way; the "two-way"
settings give every node two lanes each way (byte 4 = 0x22, in the snapshot and the
port alike) to run the coming side too (`place_at`'s 0x7cbe2.., `place_speed`'s 0x7c576..).
Never run yet: `place_racer_grid` (the rival a traffic car).
In `--race cy1` the six traffic cars wait off the road, come on ahead of the racers from
about 25 s (at most 4 at a time), and go back to wait when they fall out of reach
(`build/shots/traffic_beside.png`, `traffic_passing.png`, `traffic_ahead.png`). The horns:
§4.18. The police: §4.17.

### 4.17 The police (2026-10-20)

The police car is car 2 (`+0x174` & 8; `0x13c254`), with the police on (`0x12dd08`,
`0x12da34`). **At the start** it is a computer's car like the traffic (kind 3, part 7); its
first step (`car_move_on`, progress < 2) places it behind the player's car (node − 18, on a
one-way road), not to wait, and `place_flags` takes it out of the race (part & ~4: not
driven, not in the tick, not drawn). The snapshots hold it so, at node 2.

- **`police_come` `0x79500`** (in `traffic_police_tick` after the lead, unless the traffic
  is off, `0x12da28` & 4): only with settings +0x34 (`0x15d2f0`, the traffic on the road:
  0 in a time trial and a circuit's head to head) and not over a link (`0x12da24` = 1).
  Past an open road's finish (`0x13c164`) `chase_drop(1)`. When the player's node passes
  `0xf7510` (the last looked at) and the police car is off the road, the nodes from
  `0xf7510` + 0x32 to the player's + 0x31 (below the head's short at +6 × 4) are looked
  at: the first marked for the police (node `+0x1a` & 1) is tried by EA's random numbers
  (`0xeab70`): bits 8..14 of the next > `0xf7500`[(+0x1a & 0x18) >> 3] (32, 64, 96, 128
  in 128) and nothing more until the player moves on; else the police car is placed there
  (`place_at`, wait 0) at the road's edge the mark names (& 2 the left: half its width
  in from the left edge, else from the right), facing the racers' way unless the left of
  a two-way road; `+0x176` & 0x87, its stretch the player's, the chase bits off,
  `0xf74a8` 0, part | 4, and parked (`+0x176` | 2: its target speed 0, `0x786d4`'s end)
  unless the mark has 4. `0xf7510` = the player's node. CY1's marks: nodes 170 (0xd), 379,
  682, 920, 1008 (0xb, the only parked one), 1112, 1333, 1450.
- **The chase** (`police_pull_over` `0x79728`, in the tick for a police car before its lane
  scores; edx the lane 0..7, ebx the scores): `0xf74a8` (**chasing**) starts when the
  police car is within 5 nodes of the player's and the player is faster than the section's
  limit (`0xf74a0` byte 1), the ticks `0x15d4b8` (4 a tick) then from 0. While chasing
  and not yet on it: `+0x175` | 4 if it is behind the player and going the racers' way,
  else | 0x20 (it turns first); its parking dropped once the player is past. The player's
  `+0x175` | 4 while it chases (0x24), | 8 within 10 nodes (the radar, the map's dot). A
  chasing police car (`+0x175` & 4) behind the player sets its own `+0x176` | 8; more than
  2 nodes ahead, or level with both slower than 3, it has **caught** the player
  (`+0x176` | 1 on the player's car: its steering `0x1c800` turns it towards the police).
  Chasing or caught, its lane scores lean to the right edge (s0 + 0x2e, s2 ∓ 0x2e by the
  lanes, `0x2e0000`) unless a roadside object is at its node or the next two
  (`car_object_at_node` `0x1eb54`: the car's place in the placements walked, not onto the
  last nor back onto the first). Its target speed (`0x786d4`): the leader's + settings
  +0x18, and the player's `+0x70` when 4 nodes ahead, or level (−2..3) in the first 900
  ticks with the player over the chase limit (byte 2).
- **Pulled over**: with the chase on (`+0x175` & 4), the player not thrown about, both
  under 1 m/s and caught: a **warning** if the ticks are under settings +0x1c (400), else
  a **ticket** (`0x14ec34` / `0x14ec30` + 0x1d2 · slot + 1 while racing, `hud_warning_on`
  `0x2d2c8` / `hud_ticket_on` `0x2d290`: `0xea820` / `0xea81c` 1, `0xea824` −1000); the
  player's body `+0x84` = settings +0x24 / +0x28 (60 steps each, 1 × 60 in the stores) and
  kind 1 (through its `+0x88`, its own pointer; the crash's sound after it tests kind 4
  and never plays); the police car's `+0x175` & ~4, `+0x176` & ~8, the player's `+0x176`
  & ~1, `0xf74a8` = `0x15d4b8` = 0; `0x7c030` (the chase dropped, the cars put back: the
  police car placed again behind, out of the race); `player_pull_aside` `0x7aeb8` (the lane
  ≥ 0: always the right): moved across to half a lane past the right lanes (at most the
  edge less half its width; the left's mirror image), turned with the road (+0x16, +0x12
  wrapped and negated, +0x14 << 10), stopped. **The second ticket** while racing:
  `0x13c178` = `0x13c17c` = 4 (arrested), the finish `0x14ec3c` and `0x14ec40` 0.
  `0x15d468` + 1 for each chase still on as the ticks pass +0x1c (only counted).
- **`drive_player_wait` `0x7ea20`** (kind 1): `+0x84` counted down; at 0 kind 2,
  `hud_ticket_off` `0x2d2e4` (if one was up, `0xea824` = the step counter; both off), the
  clutch 0, the gear −1, the automatic in neutral (`+0x3e9` 2 unless by hand), `0xf6b70` 1.
- **`police_come`'s twin in `0x77d3c`**: a police car fallen out of the racers' reach is
  placed again ahead once (`+0x176` bits 4..6 count it: (`+0x174` dword >> 20) + 1); the
  second time `chase_drop(1)` and it stays.
- **The picture** (`0x31684` in the seat, `0x314e4` outside, while `0xea81c` or `0xea820`):
  `load_ticket_picture` `0x30424` instead of the dashboard (and the view): once,
  `SIMDATA/MISC/` + `TICKET`/`WARN` (`TICKEG`/`GWARN` with kilometres, `0x15d140`) + `D`
  racing / `R` the replay + `H` 640x480 / `L` (`0x12c308`) + `.qfs`, the race's palette read
  back (`0x12ba30`), its `!pal` set at 0..253 (6 bits); its "scrn" (320x200) drawn at its
  place every frame. The race's frame still runs underneath: while it is up the police
  car (slot 2, `+0x176` & 0xc) swaps palette entries 0xfa..0xfd between `0xf6de0` and
  `0xf6dec` every 16 steps (`0x66971`, `0xf6ff0`, `0x14e61c`) instead of the racers'
  marks, over the picture's own colours there. Once both are off (`ticket_picture_off`
  `0x306e8`) the file is let go, the race's palette set back (unless the race is over)
  and the dashboard drawn again.

**Ported (`port/src/opponent.c`, `drive.c`, `hud.c`, `cardraw.c`, `race.c`): equal to the
original.** `tools/opponent_emu.py --full` now runs `0x79500` and `0x79728` on both sides
(the player's kind 1 too) and compares the counts (`0xf7510`, `0x15d468`, `0xea81c/20/24`,
the tickets, warnings, finish and `0x13c178`): all 495 runs on all 11 snapshots equal (45 settings, among them chases with a stop, a long one, two stops, every stop a ticket and one ticket already: 9 warnings, 32 tickets, 5 arrests). The police drawn in view
(`CARS_POLICE=8,3 tools/cars_emu.py`: car 2 moved ahead in the snapshot and put in the
order and `0x14e5d0`) pixel for pixel on all 11. In `--race cy1` (`RENFS_SEED=6`, the
throttle held, then the brake) the police comes out at node 170, chases, pulls up and
gives a ticket (`build/shots/police_*.png`). Not yet: the view from the police car
(camera mode 7) and the camera sent back to the player as it is placed, the siren and
the horns, the arrest's film (`fe_cop_ticket_movie`). The replay's palette: in a replay
(`0xf6b74` ≠ 1) `0x30424` leaves the race's colours alone and re-colours the ticket's
pictures to them, `0x55ee0(ticket, !pal + 0x10)` (ported 2026-10-21, `hud_ticket` with
`pal_remap`; before, a replay's ticket showed in the race's colours unconverted).
The pause menu with a ticket up: `0x30340` (at `0x56577`) frees the mask and the ticket and
puts the race's palette back; after the menu `0x306e8` loads the dashboard again and the
next frame the ticket afresh. The port keeps the ticket and the palette it had over the
menu instead: the same picture and colours after it.

### 4.18 The race's sounds (2026-10-20)

**EA's sound library** (the voices, the banks; all its routines take registers):

- 16 voice records at `0x160fa4`, 0x28 bytes: +0 the sound's record, +4 its +8 less 1, +0xa
  its +0x1c (1: one of a pair n, n+1), +0xb kind (1 a bank's sound, 2 a stream: voice 0
  holds the race's music, priority 101), +0xc cents, +0x10 the bend it started at, +0x11
  the record's +0x17 (semitones a bend of 0x40 moves), +0x12 index, +0x13 priority, +0x16
  playing, +0x17 the sound's number (0xff none), +0x24 volume, +0x25 the record's volume,
  +0x26 level = master (`0xf99e6`, 127) · record's · asked / 0x3f01 (`0x9ea7d`), +0x27 pan.
  `0x9eaae` frees one (+0x16 0, +4 −1, +0 0, +0x17 0xff, +0x18 0, pan 0x40, both
  volumes 0x7f).
- **The banks** (`snd_bank_add` `0x958f5`, `snd_bank_remove` `0x959b4`; at most 5,
  `0xf950c`): a BNK's 128 offsets into one table of 128 sounds (`0x161224`); a sound
  already there fails the bank. Adding turns the record's +4 and its header's +0x18 into
  pointers. The record (0x48): +0 the voices it may take (a mask), +4 the EACS header,
  +0xc a pitch spread, +0x10 cents, +0x14 priority, +0x17 reach, +0x18 pan, +0x19 volume,
  +0x1a a volume spread (signed), +0x1c 1 a pair, +0x1d a pan spread.
- **`snd_play` `0x95a7a`** (eax n, edx pan or −1 the record's, ebx volume, ecx bend):
  `snd_voice_find` `0x98bf5` (a free voice the mask allows from `0xf973c` on, else the
  first whose priority ≤ the sound's, unsigned; `0xf973c` = it + 1); EA's random numbers
  (`0x91e04`, the collisions' generator) in this order: the pitch if +0x10 or +0xc
  (`((r >> 16) · s) >> 15` logical, less s), the volume if +0x1a (clamped 0..127), the
  pan if −1 and +0x1d (a first draw only decides > 0x7f → 0x7f, else a second + 0x40).
  The pitch `(bend − 0x40) · reach · 100 / 64` + cents; the frequency `0xa062c`
  (rate · table `0x9eaec`[x − top] >> shift, x = cents + 0x2000, top from 0x40d0 down by
  0x4b0 an octave); the sides `0xa1fbb`/`0xa203a` (pan over 0x7f mirrored; under 0x40
  left 0x7f right 2p; above, left (0x7f − p) · 0x7e / 0x3e; each × level / 127). A pair
  (`0xa20a8`): two voices, the volume split near the edges by `0x12412c` {150, 140, 130,
  120, 110}; returns the second << 16 | the first.
- `snd_set_volume` `0x8169b`, `snd_set_pan` `0x8171b` (0..0xff), `snd_set_bend`
  `0x8179e` (kind 1 only, not stored): only on a playing voice. `snd_idle` `0x8193c`: 0
  playing, 1 not, −8 no voice. `snd_stop` `0x81814`. A voice played to its end is freed
  by the mixer (the timer's interrupt); the game never waits for it.
- The mixer's own records (`0x9ff4c` starts one, `0xa007f` its sides, `0xa00b9` its rate,
  `0xa005e` stops it) are the port's `audio.c`. `0x9eaae` and `0x8ca7b` start with
  `push ds; call 0x94710` (DOS/4GW's data selector, as the interrupt needs it) and end
  with `pop ds`: Unicorn steps over both (`snap_emu.py`), it cannot load a selector.

**The game's channels** (`0x13c2dc`: 16 pairs of voice and sound, −1 none):
7 the engine on the throttle (sound 1; the channel is `0xee352` >> 16), 1 off it (2), 5
the road's noise (0x29), 4 the front tyres, the one-off hits and a computer's horn, 0xc
the rear tyres, 2 and 3 the two cars heard loudest, 0xe the player's horn (3), 9 the siren
(0x62), 0xb under a bridge or in a tunnel (0x3e; 0x3d on Transtropolis) and the siren's
echo (100), 0xf the rally's stones (0x50). The settings (`0x12da53` + 0x4b · player): +0x14
the sound's volume (CONFIG +0x79), +0x18 the music's (+0x75), +0x1c stereo (+0x71, 2 mono:
every angle 0), +0x20 a card (`0x15d132`; without one, `0x529f4` sets both volumes 0).

- `channel_play` `0x52ecc` (ch, sound, bend, volume, pan): nothing without the sound's
  volume; all 0 muted (`0xeded0`); bend, volume, pan ≤ 0x7f. The channel's own sound is
  moved (volume · sfx / 127, bend, pan); another is started, unless it is a one-off (0x20
  .. 0x25, and the stones 0x50, always started anew) or the channel's last was a one-off
  still playing. `channel_level` `0x53084`: the volume through the curve `0xedf50`
  (129 words), the pan from an angle (`0x52ac8`: 0 ahead the middle, a quarter turn
  either way an edge). `near` (`0x7fb2c`): 127 at 0 down to 0 at 0x200000.
- `channel_event` `0x52d2c` (ch, kind, sub, how, dist, angle; not at the pause, state 3):
  ch −1 a one-off on channel 4 (`0x52c38`: 0xd the gear's clunk 0x20; 4,1 a landing 0x25;
  2,9 along a wall 0x23; 2,10 an object knocked over 0x21; 2,1 a body thrown about by
  how hard 0x24/0x22/0x23/0x21; two cars 0x24/0x22/0x21 above 0x6e/0x3c), at near(dist);
  else stopped when how ≤ 0, or the channel's sound by the kind (`0x52b10`: tyres 4,1 on
  the road, 4,8 off it, 2,1 a body), its bend how >> 16 (ch 4) or kept from its start
  (`0xeded8`), the volume near (ch 4: how · near >> 7). The wrappers: `0x52e40` how × 20,
  `0x52e80` how >> 4, `0x52ea8` and `0x52e1c` as it is.
- **The engine on and off**: `0x52670` at the start (sounds 1, 2, 0x29 silent on 7, 1, 5);
  `0x53214` (sound 1 on 7 again if it is not, 0x29 on 5 always, `0xeded4` 1) when the car
  heard lands from a crash (`0x1ceb4`, then again in `0x1cd40`: twice), as the two-player
  view turns to the other player, at the start, and from the race keys; `0x531dc` (7, 5,
  4, 3 stopped, `0xeded4` 0) as the car heard crashes (`0x1cb14`), spins out (`0x1ca9c`)
  or is thrown by a hard hit (`0x1e314`). The police's two calls (`0x798b2`, `0x7ea19`)
  test kind 4 just after setting kind 1: never.
- **The events**: a car meeting a car hard enough to shake the views (`0x1c6a4`: heard
  from the car that is not the one heard, 2,2, the hit × 20); a body touching the road or
  a wall (`0x1e1e7`, its mass under 8: 4,1 or 2,1 by how upright, × 20); a wall's scrape
  (`0x40564`, the watched player's car, a wall of kind 0, across faster than 6 m/s: from
  the seat at 0xd70000 / 0x280000 by the side, dist 1); an object knocked over within
  −8..9 nodes of the camera (`0x1ef94`); the gear's clunk from the seat as the shown gear
  reaches 0xb with the clutch in (`0x40cc3`); the player's landing (`0x38b44`, |vel.y|).
- **`horn_want` `0x80a60`** (slot, kind): only when no horn sounds (`0xf7778` 0): a racer's
  slot → car 1, pitch 1; else that car and kind; `0xf7778` = rand_below(15) + 5 (EA's
  random numbers: **the game's, with the sound off too**; it only counts down in
  `0x80974`, which runs with the sound on). Called from `0x79b60` (a racer stuck behind a
  player, after a few tries, kind 0; and a racer 5 nodes behind a driving player or 2
  behind the first car, above 15 m/s) and `0x7ad0f` (the traffic coming the other way,
  random byte < 0x23, moving, the radar quiet, its PDN's horn +0x469).
- **Each step** (`race_update_cars` after the camera and `key_get`, not at the pause, with
  the sound's volume): `0x80974` (the horn: where, Doppler `0x7ff88`, on channel 4 at
  `0x7fa80`[car]'s sound and `0x7faa8`[kind]'s pitch; stopped after), `0x7fef8` (the
  engine `0x53338` of the car heard: from the seat angle 0 dist 1, else where `0x7fb58`),
  `0x7fcbc` (its tyres and horn), `0x7fe88` (two players: the other's horn, 999 steps),
  `0x800a0` (the radar `0xea814` from the police within 64 nodes, halved when it is
  behind, its light `0x12b9f8` flashing every 4th step; the siren within 0x400000 with
  its pitch Doppler-shifted, its echo under a bridge every 3 steps; the two cars heard
  loudest, near · their speed against the camera's (`0x8001c`) / 64, kept on the channel
  they had: their engine Doppler-shifted (`0xf7798`[slot] + the shift), or their tyres
  while they slide).
- `engine_sound` `0x53338`: the radar's beep (`0x530b0`, sound 0x61, spaced by
  `0x52560`[radar] {1, 20, 15, 10, 5, 3, 1}); under a bridge/tunnel (node flags 3/4) the
  echo up by 5 to 0x7a, down by 5 after; the road's noise by 2 · speed (+0x14 on the
  node's flag 0, + a quarter off the road; pitch 0x5e, 0x40 or 0x18; the rally's stones
  and gravel by three random draws a step on track 5), × 6/10 heard from outside; the
  revs of the car watched against the player's red line + 2000 as the pitch; the throttle
  eased (half way up, an eighth down) mixing the two engine loops through the curve,
  times (revs + 0x14) · (near + 1) / 128; the player's horn's volume with them.
- Distances are EA's approximations (the larger plus a quarter of the smaller, in 2D then
  with the height); angles from the camera's yaw (made positive in place: `0x7fb58`
  writes it back).

**Ported (`port/src/sndlib.c`, `racesnd.c`, `audio.c`'s voices): equal to the original.**
`tools/opponent_emu.py --full --sound` runs the original's whole sound path (the library
and the device's records in Unicorn, no voice ever ending) and the port's from the
snapshot's state (the banks loaded from the CD and compared record by record, the voices,
the channels, the camera) and compares every step the 16 voice records, the 16 channels,
the 39 sound globals, the radar, the camera's yaw, the cars' Doppler fields (+0x3a1,
+0x3ad) and EA's random numbers: all 47 settings (with "horn": the player's horn in
bursts) on all 11 snapshots equal, 4 to 6 voices playing a step. The camera follows the
race (the original's `0x6324c` and `0x136b0` run, the port given its camera, its yaw
compared): the port's driving hears the camera as the step began, its sounds the one
just moved, as in the original (the camera's own check, `camera_emu.py`). `SND_VIEW=3`
puts the camera outside behind the car (`camera_set_car` `0x13530`, as the race keys do):
all 517 runs equal again. Without `--sound` the horns' random numbers are still compared
(`0x80a60` runs on both sides). **Not run by any check yet** (`--coverage`, e1): the
two-player routine `0x7fe88`; the tunnel's and the rally's parts of `0x53338`, its mono
pans and its sounds heard from outside far off; the siren's echo and channel 2's police
path in `0x800a0`; the wrapper `0x52ea8`; the library's random spreads (`0x95aba`..,
`0x95cfb`: no sound of these banks has a spread) and the stereo pairs (`0xa20a8`).
In the port a voice's end is worked out from the clock (`snd_time`), not by the mixer, so
that the game's side stays deterministic. Not yet: the speech.

**The race's music** (2026-10-20): the menus' own streamed music (`music_start` `0x72010`,
EA's stream on voice 0), started as the loading's last act in `race_main` (`0xf749a` = 1,
`0x63d88`, `fe_service_stream`) when CONFIG.DAT +0x73 is not 0: mode 1 any of the twelve,
2 a rock one, 3 a techno one, chosen by `0x52580(n)` = (EA's random number `0x91e04` &
0xffff) · n >> 16 (**the cars' random numbers**: one drawn here, after the seeding), 4..15
that one (`0xf6d7c`'s names; here ROCK6 can play). Volume: the stream's own (127) at the
start; `0x72224(v)` sets it at once (0 under P, F5 and the menu's start, `0x52618`),
`0x721ec(v)` moves to it over 0x32 ticks: `0x52640` / `0x525ec` give back the player's
music volume `0x12da6b` (CONFIG.DAT +0x75) unless F5 has the sound off (`0xeded0`). The
pause menu stops it (`0x7211c`, `0x72150`) for `pause.asf` and, on Continue, starts
another (`0x63d88`, a random number again); `race_run` `0x37228` fades it out
(`0x720d8`, 100 ticks) and waits (`0x72150`). **Ported** (`sound.c` `race_music_start`,
`music_volume`; the half-second move made at once).

**The race's speech** (2026-10-20): only `car_check_lap` `0x5056c` speaks in a race, on a
circuit, when the player's car (the car watched in a replay) starts a new lap: its lap
better than the best shown (`0x13c20c`, which it becomes; `0x13c208` = 1) → `besttime.eas`
while laps remain (lap < `0x13c160`), `bestlast.eas` as the last lap starts; else
`finallap.eas` as the last starts. `0x722b4(name)`: with the sound on (`0x15d128`) and
CONFIG.DAT +0x7b, the file is loaded by its bare name (the installed game's root holds the
eleven words; on the CD `FRONTEND/SPEECH/`, German `GSPEECH/`), EA's speech queue emptied
and set up (`0x98f66(4, 4, volume)`: the sounds' volume +0x79 × 3 / 2, 127 at most; 0 if
the menus' F5 has it off, `0xf748e`) and the word queued (`0x9901f`); `0x72438` starts the
queue (`0x990a6`). The race's F5 does not silence it. An `.EAS` in IMA has no block header:
its decoder starts at 0 (`eacs_sound`, equal to `tools/eacs.py` on the three words).
**Ported** (`sound.c` `speech_say`, `drive.c`; heard on TR1 with `RENFS_LAPS`).
The place words `first.eas` .. `eighth.eas` (a table of pointers at `0x503f0`) are read by
nothing (no relocation record points into the table): the original never says the places.
So a one-lap race says nothing, and a race of more laps says `finallap` only as the last
lap starts, or `besttime` when a lap beats the track's lap record (Rusty Springs 38.2 s
built in).
**The laps** (`0x437fc`, Control Central, each time round its loop): +0xb3 the race length
0/1/2 → +0xb1 4/8/16 laps, 2/6/12 on tracks 4..7; the built-in defaults hold +0xb3 1 and
+0xb1 4, so a race from the menu has 8 laps on Rusty Springs. `--race` takes them so
(`RENFS_LAPS` for fewer) and the track's number and segment (`0x12da12`, `0x12da0e`) from
the name asked for: before 2026-10-20 it kept City 1's (0) on every track, so a circuit
compared its laps with City's record and spoke "best time" every lap.

### 4.19 The race keys (2026-10-20)

`race_keys` `0x47c88`, once a race step after `race_check_pause` `0x63304` and before
`race_update_cars`. The keys are the table of keys held (`0xf7ac8` + scan code).

- **The view key** (`0x12d94c`: the settings' +0x5f4a, 'C' by default, made a scan code
  by `0x36758`; or the joystick's button, `0x12f1d8` / `0x12f274` & 4): once a press
  (`0xec5c8` set while it is up), and only while a player's car is watched (`0x12dd54` <
  `0x12da30`; in a replay not in its mode 5, `0x145544`), **`camera_next` `0x1494c`**
  then `0x5e93c` (the replay's `0x145530` = 0). camera_next: racing (`0xf6b74` = 1) the
  next mode is the mode's +0x24 when the player's car is watched (no replay, `0x145c90`
  = 0), else its +0x1c; not racing, its +0x20 (7 becomes 0 without the police,
  `0x12da34`), but first, once after mode 5 was chosen (`0xe8f68`), the police camera (7)
  when `0x50428` (the police chasing, `0x13c254` +0x174 & 0x42400, from just ahead of the
  player to 12 nodes behind) or the rival's (6) when `0x50478` (the rival not a traffic
  car, `0x12da9e` ≠ 9, from 10 nodes ahead to 12 behind) and the mode is not 10. Mode 6
  or 7 with its car not close: the mode taken as the current one and the next looked up
  again. The driver's modes (≤ 3, 10, 11) refused while the tumble's view waits
  (`0x145c98`). Then `camera_set_car(0x129ecc, car, mode)` and `0x14b00` again. Racing,
  the player's car: 0 → 1 → 2 → 3 → 10 (→ 0); another car: 4 → 5 → 11 → .. **In
  `camera_update` `0x136b0`** modes 6 and 7 keep the rival's (car 1) or the police car
  while it is close and upright (`0x14de8`), else `camera_set_car(the player's, 13)`.
- **Alt+Ctrl and 1 .. 8** (codes 2..9; 3..8 only with `0x12da20` ≥ 2 or the right
  Shift): `0x12dd54` = the car, `camera_set_car(.., 4)`, the engine heard again
  (`0x53214`), `0x15d71c` = 1. The dials stay the player's (`0x31171`), the status bar's
  numbers and the map follow the car watched (`0x31e87`).
- **Shift+End** (no replay; past 100 steps of the race clock `0xed0f8`, 400 in a network
  race): `0x1f0b4(player)`, the player gives up: unless a time trial (+0x6d = 0), `0x12a150[car]`
  = 1, `0x12a14e` = 10 (the network's message), with `0x12dd5c` (1 in a race on one
  machine, measured) the finish time 999,999; not once the player is in or has given up.
  From then `keys_control_word` `0x48bec` gives the player 0x87a0: straight on, no pedals,
  the handbrake (bit 15); the race ends as any other once the car stops (`0x13c178` = 1),
  the results showing DNF and N/A (measured in DOSBox, `build/dos/giveup/`: from 56 mph
  over in under 8 s). **Ported** (`racekeys.c`, `race.c`): from 52 mph stopped and over
  in about 6 s (`build/shots/pause/giveup*.png`). **Alt+M**
  (`0xf4568` ^= 1): the frame prints the camera's node and mode letter (`0x5bfa9`, a
  developer's display).
- **Ctrl held: no F key.** Each F key once a press (`0xec5cc`):
  F1 `0x14324` the view's next layout (the layout's +0: outside 1 → 2 → 3 → 1, 320x160,
  256x128, 204x101 inside a border; the driver's has none), `0x12d9da` = 1 at detail 3;
  F2 `0x12d9d9` + 1 mod 4 (at 320x200, `0x12c308` = 0, back to 0 above 1) through
  `0x14370`: `0x143e04` = min(it, 2), the view's window the layout's +4 + 4·detail (1:1,
  half width doubled, half both); F3 `0x12d9d5` + 1 mod 3, `0xf6b48` = 28, 14, 9: the frame
  (`0x5bcf5`) takes it into `0x136618` (the sections drawn ahead) and gathers the road
  again (`0x5004c`); F4 `0x12d9d6` (the sky's colours only); F5 `0xec528`: `0x525b8` every
  channel's volume 0, the music's 0 (`0x72224`), `0xeded0` = 1, or `0x525ec` the music's
  volume back and `0xeded0` = 0; F6 `0x12d9db` (the racers' markers, `0x66b98`).
- **At the race's start** (`0x37290`): `0x12d9d4` .. `0x12d9d6` from CONFIG.DAT +0x81,
  +0x83, +0x85, +0x89 (+0x87 the mirror's `0x12d9d7`); `0x5ba1c`: `0xf6b48` = 28, 18 or 13
  by `0x12d9d5`, `0x1440c(0x12d9d4, 0x12d9d9)`: the detail min(`0x12d9d9`, 2), the view
  ahead in its own layout, the outside one in its own + `0x12d9d4`. After the race at
  320x200 `0x12d9d9` is kept at most 1 (`0x375e8`).
- `0x12da04`: the keys End, Ins, the arrows, PgUp, PgDn, Space, Enter, Tab as bits (End
  0x800000, Ins 0x1000000, up 0x40000000, down 0x80000000, left 0x10000000, right
  0x20000000, PgUp 0x400000, PgDn 0x200000, Space 0x8000000, Enter 0x4000000, Tab
  0x2000000). Read once only, `0x5efb6`: Ins alone, the result `0x13c178` set, not running
  fast, steps played `0x14552c` > 0 → `0x13c17c` = the result and the replay's step skipped.
  `0x13c178` and `0x13c17c` are set together everywhere, and the loop ends on `0x13c17c`
  between frames: only a step after the end within the same frame of a replay sees it.
  Nothing to port (2026-10-21).

**Ported (`port/src/racekeys.c`, `camera.c`'s `camera_next` and modes 6 and 7, `view.c`'s
`view_layout`): the view key equal to the original.** `tools/camera_emu.py` presses it
every 13 steps in five more runs (racing, racing through a tumble, watching the rival,
not racing with the rival and the police placed ahead, the same without a chase) and
compares the camera record each step: all 14 modes reached, all equal on the 11
snapshots. The F keys, Alt+Ctrl+n and F5 are seen and heard in `--race` (pictures
`build/shots/keys/`). Not yet: Shift+End, Alt+M, the joystick's view button, the replay's
keys, the pause (`0x63304`, its menu `0x1f988`), the music's volume under F5.

### 4.20 The pause (2026-10-20)

`race_step` `0x6348c` starts with the calls due (`race_timed_calls` `0x1fa1c`: the table
`0xea61c`, each call's step in `0x129fb0[i]`, set by `0x1f988(step, i, 1)`; [0] the pause
menu `0x56474`, [1] P's hold `0x63110`), then `race_check_pause` `0x63304` (the step
counter `0x145c94`, 2 a step), then `race_keys`.

- `0x63304`: below 0x3c on the counter, `0x145c7c` (a pause asked) and `0x145c80` (the
  counter when last asked) are cleared. **Esc** (held, racing (`0xf6b74` = 1), the counter
  above 0x78, none asked): in a network race with a Shift held the race is given up
  (`0x15d138` = 1, `0x13c17c` = 2); else, at least 0xb4 since the last, the menu is due at
  the next step (none in a replay) and the road's distance asked again as F3 left it
  (`0x5b19c`: 28, 14, 9). Else **P** (no network, the counter above 0x78, none asked, no
  ticket or warning shown (`0x2d2ac`), at least 0x78 since the last): the hold due next step.
- `0x63110`, P's hold: `0x12f1b8` = 1 (the race clock `0xed0f8` stops in `0x4c454`),
  MASKLO's `paus` drawn at its own place (`0x2dac4`: "PAUSED, Press 'P' to continue",
  centred over the view), every channel's volume and the music's 0 (`0x52618`); it waits
  for P up, then down; the dashboard drawn again (`0x2d284`) and the camera's mode taken up
  again (`0x14b00`); waits for P up; (network: `0x2db34`, `0x1fa94`, `0x1fb30`); the
  music's volume back unless F5 has it off (`0x52640`); none asked, the last = now, and
  `0x4d700`; `0x12f1b8` = 0.

**Ported (`port/src/pause.c`): P's hold.** The race stands still while held: a race paused
300 retraces from retrace 600 gives at retrace 1200 the same picture, byte for byte, as an
unpaused race at 900 (`build/shots/pause/`); the original's own pause in DOSBox looks the
same (`build/dos/ppause/`).

**The pause menu, `race_pause_menu` `0x56474`.** `0x12d9e4` and `0x12f1b8` = 1 (the race
clock held); every channel stopped (`0x52ab4`); the race's palette kept (getpalette), faded
down; at 320x200 the mode changed to 640x480 (and back after; at 640x480 the race's own
off-screen window is used). The mouse put on the row chosen (x 100, y `0x5632c`: 120 170
217 265 311 360); FRONT_MW.BNK, `pause.asf` played; `check/pause.fsh` (its `!pal` the
menu's), `check/pause.inv`, and the map `check/<name><segment + 1>.qfs` for the open
roads, `check/<name>.qfs` for the circuits (names `0x563a4`: cy cl al loop1 loop2 loop6
loop3 loop7 loopx; the circuits' files by track tr1 tr2 tr6 tr3 tr7 tr4, `0x3629c`).
- Drawn: `bgnd`, the map's `0000` at its own place, `cham` (the chat dimmed; in a network
  race `repm`, the replay dimmed, and `chal` blinking with messages), the markers, then the
  row chosen lit (`conl repl audl gral chal quil`; dark `cond repd audd grad chad quid`).
- **The markers** (`drawshape_centred` `0x81600`, by the shape's centre, 255 left out): a
  car's point is its node / 4 into the track's points (pairs of words, x and z in metres,
  every 4 nodes: CY `0xf1340 0xf1ac0 0xf21a0`, CL `0xefd60 0xf04e0 0xf0c10`, AL `0xee3c0
  0xeebe0 0xef4e8` a segment each; the circuits `0xf2a38 0xf2c38 0xf3838 0xf3038 0xf3c38
  0xf3438` by track 3..8; the counts `0x563e8`), x / 25 and −z / 25 (circuits / 6), plus
  the map's place `0x56338[3·track + segment]`, `0x5636e[..]`. Race type (`0x12da20`) 2 or
  3: `p1`..`p8` on every car, `youl` on the player's, `lear` on the race order's last
  (`0x13c290` = `0x13c274[7]`, the leader); else the rival's `p<n>` (head to head), the
  player's, and `youl`. The image's point tables are never written.
- The rows by the mouse's height: 0..0x8c continue, 0x98..0xbe replay (not in a network
  race), 0xc5..0xec audio, 0xf7..0x11b graphics, 0x125..0x149 chat (network only),
  0x153..0x1e0 quit, else none lit. Up and down go round (past the chat, or the replay in
  a network race), the mouse moved to the row; sounds 0x47 a move, 0x48 a choice; keys
  repeat after 25 ticks. The right button = Quit chosen. Esc closes it as Continue, once
  Esc was let go after the menu opened (and waits for it to be let go again).
- Continue; Replay `0x5e4c4(1)`; Audio `fe_audio_options(2)`; Graphics
  `fe_graphics_options(1)` (the art freed and loaded again round them); Quit: "End game ?"
  / "Spiel beenden ?" (`fe_yes_no_dialog`), yes → `0x13c17c` = 2 (the race ends).
- Leaving: faded down, music faded out and waited for, the bank freed; 320x200 again, the
  race's palette set straight back (no fade); the config's detail (+0x81 .. +0x89, put
  there from the race's globals before the menu, `0x144cc` the outside view's size) read
  back; the dashboard due again (or `0xe80f0` from outside), `camera_set_mode`,
  `views_start`; the engine started again (`0x52670`) and heard if the player's car is
  driven (+0x512 1 or 2); the ticket's picture put back (`0x306d0`/`0x306e8`/`0x306dc`);
  the inverse table, the race font; none asked, the last = now (`0x632f8`); the clock
  goes on.

**Ported: the menu**, against the original's pictures (`build/dos/pause/`, compared by
palette entry: each entry one colour): continue, replay and graphics chosen, 156 pixels of
307,200 differ in each, the mouse pointer and single pixels of DOSBox's scaling. Continue
brings the race back where it stood (`build/shots/pause/back.png`), Quit with Yes ends it.
Replay, Audio and Graphics since (§4.22, §4.2; the detail handed to the settings and back,
§4.2 "The pause menu and the detail"). Not yet: the network's chat and messages, the
ticket's picture put back after the menu, the audio settings' per-player volumes
(`0x12da67`, `0x12da6b`).

### 4.21 The race from Control Central (2026-10-20)

**The Race button** (`fe_main_menu` `0x42c30`, case 4): a tournament's segment already
raced is refused (a message, `0x74234`); in a tournament EXPLODE (`0xf74a4`), the rally
(`0x12dd2c`), TURBO (`0x15d12a` & 8) and the Warrior (`0xf7494`) are cleared; the pictures
freed, the music faded out, the sound bank freed, faded down, the music waited for, then
`race_from_menu`. Back: "No mercy" (`0xf7492`) off, `0x15d12a` & ~4, the art, the inverse
table and the sound bank loaded again, the menu drawn and faded in.

**`race_from_menu` `0x3770c`**: the track names (`0x36290`: cy cl al tr1 tr2 tr6 tr3 tr7
tr4); CONFIG.DAT +9..+0xb4 (`0x156c1d`, 0xac bytes) and +0x5f36 (`0x15cb4a`, 26 bytes)
kept at `0x15d010` / `0x15d0bc` before each race and **put back after it** (what the race
changes in the settings is not kept). An open road (`0x15d09c` the track < 3): the name
"%s%d" with the segments done `0x15d09a` + 1, the track's records (+0x670.. by track,
`0x157284` + track · 0xa6b) copied to `0x12c0e8` (100 words; a circuit's from + race
length · 400). `race_run(2, names)`, then black, 640x480. The result `0x15d122`: 4 → the
police's film; 1 → the records written back when beaten, the winner's film
(`fe_winner_movie`) after a third segment or a circuit, the standings (`fe_standings_*`),
then `race_menu_checkpoint` `0x1a5d8` between an open road's segments (0 = quit) or
`race_menu_finish` `0x1b6a0` after the last; otherwise (given up, quit) the standings and
the finish menu. A tournament's end (`0x689b0`, `0x42a84`) and `fe_new_session`. **Still
to port** (the next step): the segments in a row, the standings, both menus, the records.

**`race_run` `0x37228`**: `race_setup_record` `0x36c5c` (below); the detail settings
(+0x81 .. +0x89) to `0x12d9d4..`; **EA's random numbers from the clock**: `0x91d9b` the
time in seconds (rounded at 500 ms), Watcom's `srand` `0x91df4` (state `0xf8a88`), each of
the six words `0xf8a8c..` = `rand() · rand()` (`0x91dd0`: × 0x41c64e6d + 0x3039, bits
16..30) (the port had them only from the image); the keys (`0x36a0c`); the grid places
(`0x370bc`); the track's name to `0x12da16` (`0x12da16` = "tr5" with the rally on track 3,
`0xe11d8`); the time again into `0x12da44` and `0x38c10` seeds the race's random numbers
(`0xeab70`) from it; **`0x653a4` the cars**; `race_clock_rate` with TURBO (`0x12da28` & 8:
`0x12dd0c` = 3, 42.9 steps a second); `race_main`. After it: the detail settings back into
CONFIG.DAT (+0x81 the outside view's size `0x144cc`), `0x15d122` = `0x13c178`, the music
faded out.

**`race_setup_record` `0x36c5c`**: `0x12da0e` = +0x93 the segment, `0x12da12` = +0x95 the
track, `0x12da20` = +0x6d the race type, `0x12dd14` = +0xb1 the laps, `0x12dd24` = +0xb3
the race length, `0x12dd0c` = 1 the clock's rate, `0x12da28` = `0x15d12a` the codes;
`0x12da30` = 1 the humans, `0x12dd58` = 0 the player. **`0x12da2c` the racers: 1 a time
trial, 2 head to head, 8 a single race or tournament. Head to head, with no "No mercy"
(`0x15d12a` & 4), on an open road (track < 3): `0x12da34` = 1 the police, `0x12da3c` = 6
the traffic; else none.** The machine's speed (`0xec410`, `0xec414`) lowers detail on a
slow PC (+0x7f, +0x83, +0x87). The racers' records (0x4b bytes from `0x12da48`): +2 the
name (+0 of CONFIG.DAT), **+0xb the car: `0x36c1c` maps the menu's number (+0x9b the
player's, +0x91 the opponent's) through the words at `0x36250`: 7 6 0 2 3 5 4 1 8 9 10 11
..**, so the menu's 0 is the RX-7 (7) and the game's 0 the Supra; the opponent's record a
copy of the player's with its car. `0x12dd10` = 1 (the leader waits) unless "No mercy".

**The cars `0x653a4`** (`0x12dce4[n]`, the ids `car_load` reads): base 200, or 100 when
`0x12d9d7` (+0x87, less detail) is set, for the computer's racers in races of 8 (the
humans always 200 + their car). A single race: the opponent 8 (the mixed field) gives car
n `0xf6df8`[n] (7 6 0 3 2 5 1 4) + base, else seven of the opponent's car. A tournament:
by the player's car, three cars in turn (n % 3): RX-7, NSX, Supra (0, 6, 7): 0 7 6;
Diablo, 512TR (1, 4): 4 1 4; 911, ZR1, Viper (2, 3, 5): 5 3 2. Head to head and time
trial: car 1 = the opponent + 200; with the police (`0x12dd08` = 1, one human and
`0x12da34`) car 2 = 400; then the traffic: each `((rnd & 0xffff00) >> 8) · 12 >> 16`
from `0xeab70`'s numbers (`0xeab70` × `0xeab78`), drawn again while already taken.
(Seeded with a small number, `0xeab78` small, the products stay below 2^16 and every
draw is 0: the clock's seconds never are; the port gives up after 10,000 draws.)
**The grid `0x370bc`** (`0x12dca0`, a racer's place): a single race, or a tournament's
first race: 7 6 2 3 4 5 1 0; a tournament's later races by its points (`0x15d0d8`) or the
last segment's order (`0x12c276`); else 7 6.

**The loading picture `load_slide` `0x64d28`** (from `race_main` right after
`initgraphics(640, 480, 8)`): `SIMDATA/SLIDES/<car>S<n>.QFS` (`GSLIDES` in German; the
car's name from `0xf6e30`, i.e. the names `0xf6b10` + 200; n = `0x52580(7)` from EA's
random numbers, drawn again while used, the used flags `0x14e602` cleared by each race
(`race_main` passes 1)): "SCRN", 640x480 with its palette set straight (no fade), left on
the screen while the race loads; then `0x5b984` sets the race's mode.

**Ported** (`port/src/race.c`: `race_run`, `race_from_menu`, `race_test`;
`mainmenu.c`'s Race): the race as the settings and the codes ask (track, segment, type,
the player's car, the opponent, the field of 1, 2 or 8, the police and traffic by type,
the grid, the cars and the far models by +0x87, the laps +0xb1, "No mercy", the mirrored
and rally tracks, NITRO), EA's and the race's random numbers seeded from the clock first,
the loading picture (held a second: the port loads at once), back to Control Central after
the race (the standings and menus between: next). Seen: Control Central → Race → the
picture → City 1 → Esc, Quit → Control Central; a single race of 8 and a tournament's
field on CY1 and TR1 for 85 s each; time trial; TURBO's clock (2 2 3). Not yet: the network's records, the detail settings back after a race (the original puts them
back from its copy anyway).

**After a race** (2026-10-20). **The session `0x15cb68`** (0x5b6 bytes, a saved game's
first part): 8 racers' records of 0x95 bytes: +0 the name, +9 a circuit's 20 lap times,
+0x59 the race's time, +0x5d its distance, +0x61 its top speed, +0x65 the total (times
added up over the segments, 0 once one is missed), +0x69 the total distance, +0x6d the
best top speed, +0x71 tickets, +0x75 warnings, +0x79 (only cleared), +0x7d..+0x91 the
race record's statistics `0x14ec18`, `1c`, `24`, `20`, `2c`, `28`. Then `0x15d010` the
kept settings, `0x15d0bc` the kept keys, `0x15d0d8` the tournament's points[8],
`0x15d0e8` won[9], `0x15d0fa` raced[9], `0x15d10c` given up[8], `0x15d11c` the race type,
`0x15d120` the films (2), `0x15d122` the result, `0x15d136` network, `0x15d140` German.
**`fe_new_session` `0x7469c`** (in `fe_outer_loop` before the menu and at
`race_from_menu`'s end): the keys kept, `tourn_clear_records` `0x745f4` (all but the
names; +0x93 the segment = 0), the points, won and raced cleared; `0x74748` the names:
CONFIG.DAT +0 the player's, then 8 bytes each from `0xe43e0` (Drifter Burnout Psycho
Phreak Hacker Razor "Mr. Ice").

**`0x68ab0`** (race_run, after `0x15d122` = `0x13c178`; a quit from the pause menu sets
only `0x13c17c` = 2, so the result stays 0): given up |= `0x12a150`; the humans' distance
`0x14ec40` = last node · 0x30 / 10; a computer's racer not in: finish = (last node · laps
· steps − 300) / ((lap − 1) · last node + node + 1); the order `0x13c194` bubble-sorted
(swapped when a's time ≤ 0 or b's < a's); the segment's time; the splits to `0x14eac4`
(read by nothing); with +0xaf = 0 (a race played, not a replay) **`0x68dcc`**: finished
(1): each racer's time, top speed, total (0 when the time < 1), best top; else these four
cleared for all 8. `0x14fae2` the player's place, `0x14faea` its rank by totals (how
many less), the rival `0x14fae8` (from player == 0, the other racer of the least total),
`0x14fae6` its rank, `0x14faec` its car (`0x36c3c`, the words `0x36270`), `0x14fae4` its
place; `0x12c278` the order (the next segment's grid); a circuit's player laps from
`0x14ea84` (differences, < 0x3c → 0); the tickets, warnings, distance and statistics.
race_run's end also writes the detail settings back (+0x81..+0x89) into CONFIG.DAT and
the kept copy.

**`race_from_menu`'s loop**: the settings kept, race_run, black, 640x480; kept +0x93 + 1;
the player's total and top after each segment (`0x12c324`, `0x12c318`), total distance
(open road added, circuit laps · distance), best top; the settings put back. Result 4: the
police's film (`0x3823c`: COP/GCOP 1..3). Result 1: after a third segment or on a circuit,
+0x93 = 0 and the winner's film (`0x37ff0`); the sound bank, the menu's music
(`0x63e2c`: EA's random number `0x52580`; mode 1 any of 12, rock or techno by the music
setting); an open road before its third: `fe_standings_segment(1)` (and
`fe_standings_final(1)` in a single race or tournament), then the checkpoint: 0 → +0x93 =
0, done. Otherwise the standings for the kind of race (segment, track `0x6b938`, loop1
`0x6cb24`, final `0x6f070`, `0x689f8`), then `race_menu_finish` `0x1b6a0`, done. Not
finished (given up, quit, arrested): the music, the bank, the standings, the finish menu,
+0x93 = 0. Done: the tournament's end (`0x689b0`, `0x42a84`, `0xec460`), then
`fe_new_session`. (The record splits `0x12c0e8` copied out and back: no change.)

**The winner's film `0x37ff0`**: won when the total ≥ 1, not given up, and no other racer's
total is less (a missed segment's 0 counts as less); raced[track] = 1. A tournament:
won[track] = 1; nine won with Lost Vegas (`0xec460`) or eight without → WIN#7, else by
track 0→WIN#2, 1→4, 2→6, 3→1, 4/5→3, else 5. A time trial: none. Head to head, single
race: n = 0 or 1 (`0x74aec`: Watcom's srand from the clock, the first rand / 0x2aaa, 3 →
0; 2 drawn again), the player's car < 3 → n + 1, < 6 → n + 3, else n + 5. `0x341a4`
plays a film: black, the keys flushed, 320x240, back to 640x480.

**`fe_standings_segment` `0x6a9d0`** (STATS/STANDSEG.QFS): the segment = +0x93 − 1 (0 →
the third); `records_segment` `0x69608` first if asked: the record +0xb5 + track · 0xa6b
+ i · 0x27 (+0 name, +0xf car, +0x13 gearbox, +0x17 set, +0x1b time, +0x1f speed, +0x23
race type, from the kept settings), the time i (0 < t < record) = bit 1, the speed i + 3
(record < top) = bit 2; never with the Warrior (car 11). The page: the player's column at
x 0x131, the rival's (a time trial: the record so far, +0x5a3 / +0x658) at 0x1d6; rows y
0x5a name, 0x6e car, 0x89 standing, 0x9b time, 0xad total, 0xcb average, 0xdd top speed;
"SEGMENT %d"/"ETAPPE %d" at (8, 0x5a); the records at y 0x136 and 0x17c (x 7, 0x131,
0x1d6, 0x23f the race kind), white when just taken; the commentary con4_B16 (both),
con1_A16 (speed), con2_A16 (time). Done x 0..0x82, y 0x1b3..0x1e0 (donl/dond), the mouse
put at (40, 450); 12,000 hundredths, Enter, Space, Esc or the right button. Formats: time
`0x74914` "%d:%02d.%d" / "%d.%d" / "N/A" (≤ 0, 99999, 999999); speed `0x74854` ("N/A" ≤
0x9fff6; mph i · 224 / 100 + 1, km/h i · 360 / 100 + 1, the tenth the m/s value's);
average `0x74a14` d · 1344 / t (2160 German); names cut by `0x717d0`; car names `0xf7454`.

**`race_menu_checkpoint` `0x1a5d8`** (CHECK/CHKMENU.QFS): rows conl repl higl gmol gmsl
chal quil (dark `*d`), the mouse's rows y 0..0x91, 0x9c..0xbe, 0xcb..0xec, 0xf7..0x11b,
0x125..0x149, (chat 0x153..0x177, a network race only), 0x180..0x1e0; the keys put the
mouse at x 320, y 128 173 219 266 312 358 404, up and down past the chat; Esc = Continue.
Replay and Highlights run race_run again in replay mode (+0xaf 1 / 2), Options `0x12760`,
Standings (`0x1b12e`, no network) the segment's (0), then with several racers or a
tournament (+0x6d 2 or 3) the standings so far `0x6f070(1)` (ported 2026-10-21,
`standings_checkpoint`), Quit "End game ?" → 0. (The flag that fades
the music is the stack's, set only by Quit: Continue fades it.)

**`race_menu_finish` `0x1b6a0`** (CHECK/FINMENU.QFS): rows conl repl higl gmol gmsl
chal; up wraps 0 → 4 past the chat, down (sel + 1) % 6, the chat → 0; Esc, two minutes or
the right button = done; Replay / Highlights +0xaf 1 / 2, Options `0x12760(1)` (`0x195d8` is the chat), Standings =
the standings again as after a race not finished (0, last); the music carries on into
Control Central; returns 1. Both race menus (and every standings page) draw everything
again on the pass after a change (the pointer shown again).

**`fe_standings_track` `0x6b938`** (STANDTRK): the top ten `0x69980` (the player's total
into record 6 + i, a circuit's 6 + race length +0xb3 · 10 + i, the rows below moved down;
an open road's +0x658 / +0x664 its segment totals and tops, the time trial's reference;
the place, 20 none) and the top speed `0x69e00` (record 36 by the best top; 0/1) taken
first; rows y 0x9d total, 0xbc average, 0xd0 top; the records at 0x11d and 0x177 (the
speed row's English race kind "t.t." whatever it is); the commentary: rank 0 and not a
time trial: a tournament complete (nine raced with Lost Vegas open, else eight) and most
points → con8_D/con6_C/con7_D (records) or con5_B (taken), once heard con8_H/con6_F/
con7_H or con5_D; else con8_A/con6_A/con7_A or con5_A (once heard G/E/G, con5_C);
otherwise records only: con4_F/con1_B/con2_B (heard: con4_C/con1_C/con2_D). The arrows
move the pointer between Done (40, 450) and the middle (320, 240). Then, records taken,
**`fe_standings_best` `0x69f8c`** (STANDBST): the ten (rows y 0x82 + i · 0x15, names cut
at 0x11d), the player's white, the top speed under them; con4_D/A (first) or con4_G/E
when the player is among them.

**`fe_standings_final` `0x6f070`**: the race so far (SGMTSTND; "SEGMENT %d" with 'segt' on
open roads, "%d Lap race" on circuits, at (8, 0x5a) in style 2; rows y 0xa6 + i · 0x1d:
name 0x7b (style 3), total 0x135, an open road's segment time 0x1ef, by the totals
`0x691f8`), or the tournament's (TRMTSTND, FINSTND when all raced: nine, or eight with
+0x5f50 = 0): by the points, most first, the points at 0x1ef, this race's (0x14ec48 in the
race records, "0" not finished) at 0x13e. Black at once; buttons "Donl"/"Dond". The
order after a race: open road: segment; final(1) in a single race or tournament; the last
segment: track (and best), then a tournament's points (finished) and final(0). Circuit:
loop1; final(1); a tournament's points and final(0). **A tournament's points `0x689f8`**:
by the totals, 20 15 12 10 8 7 6 5 (`0xf6ffc`), none to a racer who gave up. Its end
(`race_from_menu`, type 3): the records cleared, then if all raced and `0x689b0` (racer
0's points the most) +0x5f50 = 2 (1 if < 2) and `fe_unlock` `0x42a84` (Lost Vegas; level
> 0 the Warrior and mirrored tracks, > 1 the rally); otherwise the session is kept.

**A circuit: `fe_standings_loop1` `0x6cb24`** (STATS/STANDLO1): g_said `0x14fb38` = 0;
taken if finished: top ten, top speed and **the lap record `records_best_lap` `0x69438`**
(the least positive of the 20 laps, the first if equal, into record 0 when below its time:
time the lap, speed the best top; not with the Warrior). Buttons Done (x 0..0x82) and Laps
(0x190..0x280, lapl/lapd), y 0x1b3..0x1e0, sel 2 = neither; the mouse at (40 | 600, 450)
(`0x691bc`); left/right go from one to the other, up/down only from neither onto Done.
The standing: types 2/3 by the finishing place `0x14fae2` ("%d / 8", the rival's
`0x14fae4`; won = place 0), type 1 by the totals, a time trial against the top ten's
best of the race length (≤); the art by track 3..8 rust autu burn mont trop neod;
"%d Lap race"/"%d Rundenrennen" (+0xb1) at (8, 0x5f); the rival column: a time trial the
record's time, average and speed, type 1 the other racer, types 2/3 the totals' order
[won] (the best other than a winner); not finished or given up N/A. Records at y 0x11d
(the top ten's best) and 0x177. The commentary (not when "again", = not finished): won or
a tournament: g_said = 1; complete and most points (`0x6926c`: the player's points the
greatest, unlike `0x689b0`) → con8_D/con6_C/con7_D or con5_B (taken); else won →
con8_A/con6_A/con7_A or con5_A; neither: records → con4_f/con1_B/con2_B. Laps: the
commentary stopped (and not again), faded down, the count + 1, **the lap page
`0x6e1ac`** (STANDLO2): rows "%d", the time, the average (the lap's distance +0x5d) at x
7/0x68/0xc7, from nine laps in two columns ((laps + 1) / 2, the second at 0x148/0x1a6/
0x205), y 0x7d + row · 0x14, the best lap white; the lap record (record 0) at y 0x144;
'no5w' over the art but on Rusty Springs' time trial, where (not the rally) **the car's
measurements**: top speed, 0–60 and 0–100 mph (time), the quarter mile "time / mph"
(`0x692a8` "%d.%d", no unit), 60–0 and 80–0 (`0x69318`/`0x693a8`: steps · 3000 or 3857 /
12000 metres, "%d Ft" · 33 / 10 in English, N/A below 100 or 999); German three rows
only. Buttons Done (→ 1: the circuit's page ends too, no fade of its own) and Final
standings (finl/find, → 0, Esc and the time too: back to the circuit's page, redrawn, the
pointer on Laps, the keys let go first); con2_C16 the first visit with the lap record
taken. At the end the ten best when taken.

**Rusty Springs' measurements** (`car_physics_player` `0x417a2`, after the top speed):
in a time trial on track 3, nodes ≤ 0x61 or ≥ 0x1d1: below 0x3333 the clock starts (the
steps `0x145c94`, the progress, the bests 99,999); throttle > 0x32 and 0x64 < dt < 0x5dc:
past 0x1ac937 (60 mph) / 0x2ca666 (100 mph) the best steps into the race record +0x198 /
+0x19c; 0x53 of progress on (a quarter mile): steps < 1000 → +0x1ac (and the speed +0x1a8);
braking > 0x32: the last step above 0x23b5c2 (80 mph) / 0x1ac937, standing (< 0x1999):
the steps since into +0x1a0 / +0x1a4. Shown at once by `0x2d71c`/`0x2d778` (HUD values
`0x12b968`.., ported since: `hud.c`'s `0x2d8c0`). The clocks are statics kept from race to race.

**After a race the menus get six colours back**: `0x31bf4` (race_main's end, the HUD
freed) allows 0x5e, 0x5f, 0xfa..0xfd again; 0x47 and the racers' marks stay out of the
nearest colours (the blending of white letters picks other entries otherwise).
It also lets go of the dashboard's mask (`0x12b5f0`), the needles' table (`0x12b5f4`) and
**the ticket's picture (`0x12b4d0`)**: an arrest ends the race with the ticket still up
(`0x306e8` is not reached once the race is over). The port forgot the ticket until
2026-10-21 (Danik: Alpine evening, arrested, then Alpine midday "all the textures looked
corrupt", a restart or the replay right): the next race's first frame found the ticket
still on, let it go and set back the palette saved under it, the previous race's (evening
colours on midday pictures). `hud_free` now lets it go as `0x31bf4` does.

**Ported** (`session.c`, `standings.c`, `racemenu.c`, `race.c`, `drive.c`): all of the
above (the finish and checkpoint menus' Replay, Highlights and Options, and the
measurements on the status bar, came later: §4.22, §4.2, §4.11). **Equal** to the original's screens by palette entry: an open road's
segment, track and best standings, checkpoint and finish menus (all six rows); a circuit's
page and lap page (Rusty Springs' time trial with its measurements, 16 laps in two
columns; Autumn Valley's single race), the race's and the tournament's standings, all
0 pixels. Checked with the original's own numbers put in: `nfsdos.py poke:` writes them
into the game at the circuit's page (seen after Laps → back), `mem:` reads them out, and
`RENFS_SESSION` puts the same bytes into the port as its standings begin. (Shift+End does
not end a circuit race: the car stops; quit from the pause menu instead.)

### 4.22 The replay (2026-10-20; playing back equal to the original)

A replay drives the race again from its controls (§3.15). The state `0xf6b74`: 1 racing,
2 playing back, 3 running fast to a point (`0x145538`) before showing it, 4 held (the
bar's buttons waiting); `0x145c94` the step counter (2 a step), `0x144ba4` = it mod
28,800 (the ring), `0x143edc` the counter at the last step recorded.

**Recording** (`race_update_cars` `0x519d8` first calls `0x5efb0` when the state is not
0): every second step (flag 4) `0x144ba4` set, and racing `replay_record_step` `0x5d888`:
`controls_read` (the keyboard's word for the player: the port reads it every step), the
first 120 counts every racer's word `0x7a0`; every 450 calls (`0x143ee8`, 27 s, at
`0x144ba4` a multiple of 1800) a snapshot s = `0x144ba4` / 1800: the fallen objects'
bytes copied (`0x5f18c`: byte 1 = byte 0, 2 a TRI object at `0xea5d8`), into the 0xa2
bytes at `0x143eec + s · 0xa2`: the orders `0x13c298` (+0) and `0x13c274` (+0x24) a word
a car, +0x50 the cars kept (other than racers), +0x54 `0x129fac`, +0x58 `0xf74a8`,
+0x5c.. the two zones `0x15d29c` (12 bytes each: +2 of `0x15d29a`, `0x15d2a0`, `0x15d2a4`),
+0x74 `0x15d4b8`, +0x78 `0xf7770`, +0x7c `0xeab70` (EA's random), +0x80 `0xf7510`, +0x84
`0x15d154`, +0x88 `0x15d158`, +0x8c `0x15d480`, +0x90 `0x13c21c`, +0x98 byte `0xeab6c`,
+0x99 byte `0x15d468`, +0x9a `0x14e794`, +0x9e `0x14e798`; then the controls, then each
car `replay_record_car` `0x5d0e4` (a racer, slot < `0x12da30`: 0x1a4 bytes at `+0x7080 +
s·0x1a4 + slot·0x1a40`; the others 0x172 bytes at `+0xa500 + s·0xb90 + j·0x172`, j
counting them) and `0x5d2e4(0x144ba4, -1)` (the snapshot put back at once, as a check of
what it keeps). Otherwise the controls only (`0x5cac4`: low byte at k·7200 + t, high at
14,400 + k·7200 + t, t = `0x144ba4` / 4).
- **A car's snapshot** (`0x5cbd0`, back `0x5cd2c`): +0 car +0x50e (the routine of the
  table `0x13c1e0`, 0 none), +4 +0x512, +8 +0x516 << 24 | +0x51a << 8, +0xc..+0x2c car
  +0..+0x20, +0x30 the turn +0x24 (9), +0x54 +0x48, +0x58 +0x50, +0x5c +0x54 .. +0x68
  (4), +0x74 +0x70 .. +0x84 (6), +0x8c +0x8c .. +0xb8 (12 words), +0xbc +0x174, +0xc0
  +0xbc, +0xc4 the body (0x94 bytes at the pointer +0xc0), +0x158 +0x15c, +0x15c +0x158,
  +0x160 +0x33c, +0x164 +0x4f2. Back: also +0x405 = +0x409 = 0 and +0x4c = (lap − 1) ·
  `0x13bb8c` · 4 + node. A racer's adds (`0x5d0e4`): +0x168..+0x16a bytes +0x3b1, +0x3b5,
  +0x3b9; +0x16b +0x3bd + 100; +0x16c +0x3c5 << 16 | +0x3c1; +0x170 +0x3c9, +0x174 +0x3cd,
  +0x178 +0x3d1, +0x17c +0x6c, +0x180 +0x40d, +0x184 byte +0x3e9, +0x185 +0x3ed + 2, +0x186
  bits +0x3fd (1), +0x3f9 (2), +0x3f5 (4), +0x3f1 (8), +0x426 & 2 (0x20); +0x188 +0x401,
  +0x18c +0x3d5, +0x190 +0x3d9, +0x194 +0x3dd, +0x198 +0x3e1, +0x19c +0x3e5, +0x1a0 +0x4a2;
  back also +0x425 = 0, `0xf7768` = `0xf776c` = −1. The others' adds: +0x168 +0x160,
  +0x16c +0x164, +0x170 byte +0x3fd, +0x171 byte +0x340.
- **Highlights**: `replay_mark_highlight` `0x5c6d4(event)` racing only: slot `0x144ba4`
  / 60 of `0x144da8` (480 words: the counter / 60 << 8 | event + 0x80), the greatest
  event of each slot kept; 11 callers (`0x1cb9e`, `0x1e534`, `0x1e560`, `0x38a8f`,
  `0x40576`, `0x41704`, `0x4173b`, `0x41764`, `0x419e1`, `0x77f59`, `0x7b921`).
  `0x5c768` makes the list `0x144ba8` (start, end, event; `0x144d88` of them): 30 times
  the greatest slot above 0x80 (and not before the ring's start) taken, sorted by time,
  start (slot − 2) · 60, end (slot + 5) · 60, both down to a multiple of 4, overlapping
  ones joined (the greater event kept); none → one from 0 to the end, event 200.
- **The file** `replay_file` `0x5c3b0` at the race's end (`replay_end` `0x5de88`, not
  when `0x13c17c` is 0x20 or 0x40: a replay's end): §3.15's five blocks.

**Playing back.** `race_run`: CONFIG +0xaf (`0x156cc3`) 0 racing (`0x5e4c4(0)`), 1 the
replay → `0x14554c` = 4, 2 the highlights → 5, 3 (the attract mode) → 8, and
`replay_settings` `0x5c340` reads the file's race record (no file: `0x14554c` = 6, the
race ends at once with 0x20). `0x5e948` each step: 4 → `replay_load` `0x5c4a4`, then mode
3; 5 or 8 (8: `0x1455f8` = 1, the attract mode's) → load, mode 2; modes 1..3 →
`0x5e118`: 1 the pause menu's (back 600 counts, 9 s), 3 the whole (back 27,000), both to
a multiple of 4 by `0x5d690(counter, 0x143edc)`; 2 the highlights (`0x5c768`, the first
one by `0x5d690(start, end)`). Mode 7 (Done): with `0x145554` (a replay from the menus:
modes 4, 5, 8 set it) the race ends (0x20; 0x40 the attract mode's), else `0x5e020`: run
fast to the last step recorded − 8 and race on (state `0xf6b78` = 1 after).
- **`0x5d690(to, end)`**: `0x145534` = end (at most `0x143edc`); to a snapshot's count
  (a multiple of 1800): put back there (state 2); else from the snapshot below, state 3
  until `0x145538` = to. Racing before: the clocks kept (`0x144b94` race clock, `0x144b98`
  / `0x144b9c` steps done / due). Then the flags from the counter, the smoke cleared
  (`0x64588`), `0x2d2e4`, the sounds (`0x65094`, `0x133d4`).
- **`0x5d2e4(count, k)`** puts snapshot count / 1800 back: k ≥ 0 the counter from
  `0x144d98`[k]; the fallen objects (`0x5f1e0` byte 0 = byte 1, `0x5f138(s)`: fallen at
  or after s → 0), the 0xa2 bytes, every car's +0x4ea = +0x4ee = 0, the racers (`0x145540`
  = 1 when k ≥ 0) then the others; the camera's car again (modes 6, 7 the opponent's view
  `0x50428`, `0x50478`).
- **Each step played** (`0x5ee14`, state 2): speed `0xf6b7c` 0 with `0xf6b80` < 1 (held):
  only the flags; else below `0x145534` `0x5dc54` (a snapshot's count: put back; the
  controls from the ring `0x5cb58`; when `0x14553c`, the camera's car, the director
  `0x5dac4` for highlights); at the end (`0x145534` = `0x143edc`, no highlights): racing
  on (state `0xf6b78`) with the clocks kept, or else back to `0x144d98` and on; with
  highlights the next one (`0x5ec04`; after the last, again from the first, or the attract
  mode's end).
- **The bar** (`0x5e4ec`): buttons 1..8 (rectangles at `0x145618`..): Done (mode 7), |<
  (1), << (3), >> (4), >| (2), play (6: the speeds `0xf6b58`[1..3] in turn), pause (7: 5),
  camera (8: 10); the mouse, left/right keys `0x4b00`/`0x4d00` move, Space/Return press.
  `0x5decc(button, held)`: |< to `0x143edc` − 27,900, >| to − 300, << and >> by held / 16
  · 4 (4 at first), state 4; `0x5e308` the speeds: 6 full (`0xf6b7c` 4), 7 (3, wait 2),
  8 (2, 4), 9 (1, 8), 5 pause.
- **The bar drawn** (`0x5c5d0` at the end of both HUD frames, `0x5fcf0`; ported
  2026-10-20): MASKLO.FSH's `repl` strip at (0, 183), clip (0, 320, 180, 200); the
  buttons from the name tables `0xf6bc0` (pressed, `…d`), `0xf6be4` (lit, `…i`; their
  boxes are the buttons' rectangles), `0xf6c08` (normal, `…l`), the camera's labels
  `0xf6c2c`/`0xf6c4c` by the camera's mode 0..7 (13 shows 3), the speed's `0xf6c6c`/
  `0xf6c78`; pressed labels at +1, +1, Done's English pictures +10 across. The time by
  `0x2d6b4`(count, 0x2d, 0xba, 6, 2) and the line at (0x2d, 0xc3), count · 0x7a / (last
  + 1) long, 3 high, in the HUD's dark green over black; only when the counter moved or
  `0x1456ac` asks for all. All blits COPY. The pointer (`0x52394`, MOUSE.FSH ptr0, ptr1
  while a button is held) shown while not racing. **`0x2d314`**, the clip of every whole
  redraw of the dashboard: (0, w, 0, 0xb4) while not racing, so the bar's rows are kept;
  while racing the whole screen with the ticket's flag `0x12b8d4` (`0x2d284`), the bottom
  band (0xb4..h) with `0x12b8d8` (`0x2d278`). The ticket's end (`0x306e8`) asks for the
  whole bar again in a replay. Slower speeds: race_main runs one step per wait + 1 due
  (`0x145ca8`). Compared with the original only at 640x480 (DOSBox's machine is fast:
  races there are 640x480 though +0x7f is 0; `0x36e09` changes it only on slow
  machines, so another place decides): the same buttons, the arrow keys' light, the
  camera's and the speed's turns (FULL → 1/2 → 1/4), the pause menu's replay starting
  again at its first count after its last.
- **The watching bar** (2026-10-21; ported, `hud.c` `watch_draw`, `replay.c` `watch_step`):
  racing (`0xf6b74` = 1) with the car watched `0x12dd54` not the player's `0x12dd58`, or
  the player stopped `0x145c90` (or `0x15d13a`, written only as 0 in this build: a
  network game's), `0x5c5d0` draws it (`0x5c636`) and `0x5e948`'s end reads it (`0x7f010`).
  The pictures (`0x7ed30`, from `0x5f240`, MASKLO/MASKHI.FSH): `repl` `0x15d4c0`, `bard`
  `0x15d4c4`, the name's box `namd` `0x15d4c8`; 12 buttons k = 1..12 (normal `0x15d4cc`+4k
  from `0xf7604`, lit `0x15d500` from `0xf766c`, pressed `0x15d534` from `0xf7638`; button
  1's normal is `donm` (greyed) unless a network game; the boxes `0x15d628`+16k, x0 y0 x1 y1
  inclusive, from the pressed pictures); 1 Done, 2 `<`, 3..10 the cars 1..8, 11 `>`, 12
  the camera; its labels `0x15d568`/`0x15d580` + 4j (normal / lit, j 1..5 `bumd taid held
  skyd chad`, from `0xf76a0`/`0xf76b8`); the letters `0x15d598` (36: `00d 1d .. 9d 0a .. 0z`,
  `0xf76d0`). The state: `0x15d6f8` pressed, `0x15d6fc` lit, `0x15d700` the camera's
  label (1 at load), `0x15d704` the car named, `0x15d70c` last pressed, `0x15d710` lit as
  drawn, `0x15d718` redraw, `0x15d71c` all (`replay_start` `0x5dd68`: all, pressed = lit =
  last = 3, lit drawn 1, named 0; set again by `camera_set_mode` `0x14b00`, every Alt+Ctrl
  car, F1 `0x14324`, the ticket's end `0x306e8` racing, back from a replay `0x5e020`; two
  players with the first in and stopped, `0x519d8`: pressed = lit = the other + 3).
  **Drawn** (`0x5c65a`: the label = the camera's mode 1..5 (redraw), 13 → 3, 0 → 1; then
  `0x7ef14`): only with redraw or all, clip (0, 320, 180, 200) / (0, 640, 448, 480):
  `0x7f1d8` all: redraw, `repl`, `bard`, Done's `donm`; pressed: its pressed picture, lit =
  pressed; else when lit ≠ lit drawn or redraw: lit drawn's normal, lit drawn = lit, its lit
  picture; `0x7f998` the 8 cars' marker colours `0x13d5e8` (fill_rect at the lit picture of
  button 3+i x + 3 / 4, its y + 3; 2 x 1 / 5 x 6; the pressed one x + 1, at 640x480 y + 1
  too); the label (pressed 12: the lit one at +1, +1; lit 12: lit; else normal); `0x7f310`
  when the car named ≠ the watched or all: `namd` again (not with all), the session's name
  `0x15cb68` + 0x95 · car (1..8 characters, else none): digits `c − '0'`, letters `c − 'a'`
  (+0x20 below) + 10, others a gap of 5 / 10; centred (x = box x + w/2 − total/2), each at
  its own y. All copies. The pointer (`0x52394`) pressed while a button is.
  **Read** (`0x7f010`): the mouse (`0x7f470`: limits (0, 180..200 / 450..480); once moved
  (`0x15d724`, `0x15d720`) or pressed: each button in its box, but 4..10 past the racers
  `0x12da2c`, 12 unless a player's car is watched (`< 0x12da30`), 1 unless a network
  game; over one: unpressed → lit (pressed 0), pressed → pressed (lit 0)); else the keys
  (`0x7f5e8`): with no menu key (`0x72f28`) held ',' → 2, '.' → 11, '1'..'8' → 3..10 if that
  many race (the first of them held decides); left / right (a new key): lit − 1 (Done
  passed: below 1 → 12 or 11, 10 → racers + 2) / lit + 1 (past 12, or 12 not allowed → 2;
  4..10 past the racers → 11); up: 2 → 3, 11 → racers + 2; down: 3 → 2, racers + 2 → 11;
  Space / Enter: pressed = lit. Nothing: the pressed one lit, pressed 0. Pressed: 1 Done
  `0x15d138` = 1 (network); 2 / 11 once a press: the car before / after round the racers,
  `camera_set_car(.., 4)` (no engine); 3..10: that car if not watched, mode 4 and the
  engine heard `0x53214`; 12 once a press, a player's car watched: `camera_next`. Then
  last = pressed, lit = pressed if any; after a key the mouse at the lit button's normal
  picture's middle (`0x52480`; the port rounds up into 640x480 so that it reads back that
  pixel at 320x200, the replay bar's too). Esc is the pause menu's, not the bar's.
  **Checked** (`tools/watch_emu.py`, in `checks.sh`): random states, cars, racers, names,
  camera modes, mouse places and buttons, keys held, on e1, e3, h1, h7 (CY1) and t1, t4
  (TR1): 2,000 steps (the fields, the car watched, the key kept, the mouse, the calls
  out) and 2,000 drawings on a screen kept between them (every pixel), all equal.
- **The highlights checked** (2026-10-20): the original's attract mode read in DOSBox
  (`peek:` of `0x144d88` and the table `0x144ba8`): REPLAY1's 720..1500, 1620..2820,
  4800..6960 (0xdc), 7080..7524 (0xd7) and REPLAY2's 180..600 (0xf8), 1320..2820 (0xa8),
  3540..4980 (0xd4), 5700..6540 (0xd4), 6900..7560 (0xd7): the port's lists
  (`RENFS_REPLAYCHECK=1` prints them) are the same.
- **Replay / Highlights in the race menus** (`0x1add8` in `0x1a5d8`, the same in
  `0x1b6a0`): the music faded, the menus let go, `REPLAY.RPL` into +0x3b (`0xe0630`),
  +0xaf = 1 Replay or 2 Highlights; the volumes back first when the audio screen's F5
  silenced the menus (`0xf748e`: `0x15d148..` → +0x75, +0x77, +0x79); each racer's
  word `0x14ec48` + 0x1d2 · p (this race's points) and the session `0x15cb68` (0x5b6
  bytes, to `0x15d11e`) kept, `race_run(2)`, both put back, `0x15d122` = 1 again if it
  was 1 (the rest the replay's `0x68ab0` left: the order, places, ranks); +0xaf 0; the
  640x480 mode, a black palette, the menus' bank, music `0x63e2c`, art, the inverse
  table; the menu drawn again and faded up, the mouse on the row.
- **The pause menu's Replay** (`0x577ea`): `0x5e4c4(1)` and out as Continue. The first
  jump from racing (`0x5d690`, state 1) keeps the race clock `0xed0f8`, the steps done
  `0xf6d60` and due `0xf6d64` in `0x144b94..`; the replay goes back 600 counts and loops
  (`0xf6b78` 4) until Done (`0x5e020`): run fast to the last count − 8, `0xf6b78` 1, the
  camera asked for back (`0x144d94`, kept only by the highlights), the driver's seat
  given up for mode 4 at once while car 0 (`0x13c248`) is rolled 0x1e0000..0xdc0000;
  at the end (`0x5eed6`) state 1, `0x2d278`, `0x14260` (the view's window), the clocks
  put back, the engine heard again. The pause menu leaving while not racing sends Done
  (`0x58234`: `0x5e4c4(7)`).
- **The attract mode's replays** (`fe_attract_mode` `0x34284`, films on `0x15d120`):
  `0xeaaca` low word a round played, high word the replay (0 in the image). From the
  second round: the replay (high + 1) % 3, `REPLAY%d.RPL` (+1) into +0x3b, +0xaf 3,
  `gamedata\replay\` + the name tried (`0x8c8b0`), and only with `0xec414` > 5,400,000
  (the start-up's speed test, larger faster: a slow machine skips it) `race_run(2)`; then
  +0xaf 0. race_run returns 1 only for +0xaf 3 ended by Done (`0x13c17c` 0x40, `0x376e2`);
  then the films (EA, ATTRACT, TITLE) are skipped. The original here played REPLAY2 first.
- **Cameras** (`0x5dac4`, highlights): the event decides: two racers near → 6 or 7 (the
  opponent's views), events 0xd6..0xf7 with a random 10 ≥ 8 → 2, 0xdb..0xe3 → random 5,
  else random 4 + 1.
- **The controls' kinds go with the replay** (found 2026-10-20): race_run copies the
  entry's +0x33 / +0x37 / +0x3b into `0x12d974` / `0x12d978` / `0x12d97c` (the template's
  from `0x36a0c`: CONFIG.DAT +0x5f36 steering 0 keys, 1..3 → 1, else 2; +0x5f38 and
  +0x5f3a pedals 0, 3 → 3, 1..2 → 1, else 2). `0x10400` steers with `car_steer_2` when
  `0x12d974` ≠ 0 and uses `car_pedals_2` when `0x12d978` = 1. **The CD's four replays were
  driven with a joystick's steering** (+0x33 = 1, +0x37 = 3): with the keys' routines
  the port drifted from count ~350 (a few hundred units of speed, then off the road).
  The original's own recordings here: +0x33 = 0.
- **Checked** (`RENFS_REPLAYCHECK=1`: each snapshot reached by playing on is compared
  instead of put back; every byte of each car's snapshot but the body's +0x48..+0x5f,
  the human's +0x187 (nothing writes them) and the others' +0x171 (not in the port);
  the 0xa2 bytes but +0x90..+0x99 (+0x99 the long chases, counted only while racing)):
  **the CD's REPLAY, REPLAY1, REPLAY2, REPLAY3 and a replay the original recorded here
  (cy1) are equal at every snapshot to their ends**, all cars. The original's attract
  mode on REPLAY2 (`nfsdos.py follow:`) reaches node 212 at count 1804, as the port.
- **The highlight marks** (`replay_mark_highlight` `0x5c6d4`, racing only; the slot
  pos / 60 keeps its first event of that time): `0x1cb14` a crash begins 0x5c;
  `0x1e314` two cars hit, by the table `0xea5dc`[hard·8 + a player·4 + car 1·2 + car 2]
  (soft only with both above 16 m/s along the road); `0x38808` landing after more than 10
  steps in the air 0x54; `0x40230` a wall hit faster than 20 m/s across 0x32; `0x408e0`
  (every 2nd step) sliding sideways faster than half the speed 0x57, a tyre skidding with
  a slide at over 20 m/s 0x28, the first move 0x78, the rear sliding while turning
  faster than 0x320000 0x57; `0x77ef4` a change of places 0x50; `0x7b34c` a police car
  spinning out near a player 0x52.

### 4.23 The joystick and the mouse (2026-10-20; `port/src/joystick.c`, equal)

- **EA's game port reader `0x94fa8`** times port 0x201's four one-shots: `0xf9504` the
  mask of the axes that answered, `0xf94f0` / `0xf94f4` / `0xf94f8` / `0xf94fc` their
  counts (bit 0 X, 1 Y, 2 the third, 3 the fourth), `0xf9500` the buttons (inverted).
  **`0x4e3e8` (the poll)**: `0x12f278` X, `0x12f27c` Y, `0x12f280` the fourth axis (the
  throttle), `0x12f284` the third (the brake), `0x12f290` the buttons held (1 2 4 8).
  `joyopt.cfg`'s remapping (`0x4e580`, `0x12f254`), turned-round axes (`0x4e680`,
  `0x12f224`) and force feedback (`0x4d670`, `0x15d144`) work only with that file, which
  nothing makes: the remapping not ported; the force feedback ported (§4.26) and on
  whenever something can play it.
- **Start-up `0x4d1fc`** (`startup` at `0x4228c`, after the fonts): mask 0 none; 3 a
  stick; 0xc the throttle and pedals alone; else a stick, a throttle if the fourth count
  is above 0, pedals if the third is. `0x12f1c0` found, `0x12f1c4` X, `0x12f1c8` Y,
  `0x12f1cc` throttle, `0x12f1d0` pedals, `0x12f1d8` on (calibrated). Then
  **`cfg_load_joystick` `0x4d4a0`**: `GAMEDATA/CONFIG/joystick.cfg` (`0x156014`), 0x5c
  bytes to `0x12f294`, kept if the stick let go is within ±0x4b of both middles and
  inside the menus' thresholds; a file of another size is removed; otherwise the
  calibration screen (`0x4c6b0(0)`). Save `0x4d5ec`. The port keeps it in the home
  folder, the same 0x5c bytes (the original's file and the port's are equal byte for
  byte).
- **The calibration, 23 ints at `0x12f294`**: X least, middle, most, the left and right
  scales, the menus' low and high thresholds; Y the same; the throttle least, middle,
  most, its two scales and its pedal scale; the brake least, most, scale.
- **`fe_joystick_calibrate` `0x4c6b0(from_menu)`**: `JOYCHK.QFS` (green) in a race's
  session (`0x15d126`), else `JOYCAL.QFS` (gold): `bgnd`, `knob`, `pedl`, `sldr`, `!pal`.
  Every redraw: `pedl` copied at (0x217, 0x60) with a throttle; clip (0x9b..0x1db,
  0x8c..0x190), `bgnd`, the knob see-through (`0x815c8`) with its top left at (x, y),
  from (0x10e, 0xdc); the bars clip (0x218..0x229, 0x93 + (100 − bp)·0xba/100 .. 0x14d)
  and (0x242..0x253, … tp …), `sldr` at (0x218, 0x93) and (0x242, 0x93). **The clip
  `0x812ec` takes (x0, x1, y0, y1)**, the last push its first. The knob: right
  v·0x69/(max − mid), left v·0x6c/(min − mid), up and down v·0x4d/(…), snapped to the
  middle within ±5 across and ±3 up and down. The ends widen as the stick goes. Bars: with
  pedals bp = 100 − (brake − least)·100/(most − least), tp the same, each 0 under 5;
  with a throttle only, bp its travel past the middle, tp before it. Button 1 without left
  Shift (0x2a) and the stick within a quarter of its middle: the middles taken, done.
  Esc: done and `0x12f1d8` = 0. At the end the thresholds half way between each end and
  the middle, the ranges at most 0x7ffa, the scales floor(N·65536/(range + 1)) for
  ranges above 1 (N 0x23, the throttle's right half 0x26, the pedal scales 0x24 only
  with pedals or a throttle), a fade down, `cfg_save_joystick` (after Esc too).
- **`0x4e0b8` in a race** (each tick): steering = the scaled X clamped −0x20..0x1f, dead
  ±3; the pedal from Y ±0x1e; with pedals each = 0x1f − (count·scale >> 16), at least 0,
  pedal = brake − throttle; with a throttle alone it centred, dead 3 and 4. Into
  `0x12f260` / `0x12f264`, the buttons `0x12f274`.
- **`0x48bec`, the control word by kinds** (`0x12d974` steering, `0x12d978` pedals,
  `0x12d97c` gears, from `0x36a0c`): steering 0 keys, 1 the joystick (mirrored 0x20 − v,
  0x1f and −0x20 first one more), else the mouse `0x47b78`; pedals 0 keys, 1 the joystick
  + 0x1e, 3 the buttons (1 → 5, 2 → 0x3c, the brake wins), else the mouse's buttons (1 →
  5, 2 → 0x3c); gears 0 keys, 3 the buttons (1 up, else 2 down), else the mouse's. The
  handbrake: its key or button 8; the horn: its key or button 1 when neither pedals nor
  gears are buttons. `race_keys` `0x47ca2`: button 4 the view key; `hud_keys` `0x2f71d`:
  button 2 the map (pedals and gears not buttons).
- **`0x47b78`, the mouse steering** (not in a replay, the player watched, not stopped
  `0x145c90`): `mouse_limits(0, 0, 256, 64)`, steering x >> 2, mirrored 0x40 − s, 0x1e..0x21
  → 0x20, above 0x3c → 0x3c, below 0 → 0; else 0x20. race_run sets the limits (0x20,
  0xb4, 0x120, 0xc8) at 320x200, (0x40, 0x1cc, 0x240, 0x1e0) at 640x480. The port takes
  the mouse relative in a race (SDL's relative mode), absolute again in the menus.
- **`0x4dfe0`, the joystick in the menus**: button 1 Space (0x20), 2 Enter (0x0d), Y below
  its low threshold Up (0x4800), above the high Down, X Left / Right. Read by `fe_read_keys`
  (Tab not held), the replay bar's held key `0x72f28`, `0x72ac4`, `0x72d94`, the text field
  `0x701cc` (a button Enter, `0x70426`), `0x6fac0`, the chat `0x195d8`.
- **Without a joystick** (`cfg_load_config_dat` `0x39c40` at `0x39d08`) kinds 1..3 go back
  to the keys: steering 0 with Left / Right, gears +0x5f3a 'A' / 'Z', pedals +0x5f38 Up /
  Down. The controls screen's Calibrate row needs `0x12f1c0`, the joystick kinds
  `0x12f1d8`; after calibrating (`0x25145`) the screen is faded down, calibrated, the
  pointer placed if unused, the inverse table reloaded, `0x72600`, the palette, all drawn.
- **Checked** (DOSBox with a 4-axis timed joystick, `~/dosgames/nfs-joy.conf` /
  `nfs-joy.map`, the launcher's copy with `NFS_DOS_LAUNCHER`): the calibration screen with
  the stick let go and held right equal by palette entry, `joystick.cfg` equal byte for
  byte, the race's control words equal (let go 1952, left 1920, right 1983, throttle 800,
  brake 3104, button 1 18336, button 2 1952), the controls screen's joystick kinds equal
  (`build/shots/joy/`). The port: SDL game controllers (left stick, right trigger the
  throttle, left the brake, A B X Y buttons 1 2 4 8, the d-pad over the stick) or
  joysticks of two axes or more (a keyboard's "System Control" device has one: refused).
  Counts 0..511. `RENFS_JOY=1|2` a test stick of 4 or 2 axes driven by `RENFS_INPUT`.
- **The port's own, not the original's** (Danik, 2026-10-20: "gas, brake and shifting does
  not work on the controller"; his settings had all three controls on the keys, which the
  original reads alone, and the automatic starts in neutral): with the joystick on, a
  control left on the keys takes the joystick too when none of its keys is held (the
  steering and pedal values its own kind would give; `car_steer` / `car_pedals` then read
  them as keys: full past half way), and a pad's right / left shoulder (a joystick's
  fifth / sixth button) are a gear up / down whatever the kinds. Only the control word
  changes, so replays and the race record stay the original's.
  A pad's Start is Esc (held for `key_down(1)` and typed): the pause menu in a race, back
  in the menus; the original's joystick has no Esc. The analogue steering and pedals
  (`car_steer_2` `0x4853c`, `car_pedals_2` `0x48a80`) come only with the Joystick kinds,
  as in the original: the keys' kinds' routines know full or nothing.

### 4.24 Races at 640x480 (2026-10-21; the port's default, as the original's)

Settings +0x7f = 0 (the built-in default) races at 640x480: `0x12c308` = 1 (the hires flag,
0 at 320x200), `0x143e2c` the view's resolution (the window table `0xe85c0` + 680 · it).
`0x36e09` drops to 320x200 only on a slow machine; the port never does. The port: `g_view_res`
and `H(lo, hi)` (`view.h`); `RENFS_RES=320|640` overrides the settings for tests.

- **Files**: `MASKHI.FSH` / `MASKHIG`, `MASKHI.MMB`, `MASKHI.B00..` (`0x5f7c8`), the dash
  `<car>DH.FSH`, the tickets `TICKETxH` / `WARNxH`. Pictures 640x480, the view picture
  `0x143e30` too.
- **The status bar** (`0x30d8c`): digits at y 6, step 0x13; the accelerations at 0x14f /
  0x1cc / 0x234; the start lights a 640x32 band, lights 0x82 apart at y 1 (their beeps
  `0x5257c`: an empty routine, nothing to port).
- **The map `0x31cb4`**: corner (2, 2) (320: (1, 1)); node shift 19 (20); the road's centre
  (+0x38, +0x45) (320: +0x1c, +0x1c), the dots' (+0x38, +0x46) (+0x1c, +0x19); the frame
  drawn twice, (ox−1..ox+0x70, oy−1..oy+0x8a) then (ox−2..ox+0x71, oy−2..oy+0x8b); clip
  (2, 0x72, 2, 0x8c); dots inside |x| < 0x38, |y| < 0x3d, the police |y| < 0x44; the
  arrows at x = race type 1 ? ox+0x5b : ox+0x60 − 0xd·k, their triangles 4 wide (2), `aro1`
  at oy+0x83, `aru1` at oy. A dot (`map_dot`): clip (2, 0x72, 9, 0x85), fill (x−3, y−3, 7, 5),
  lines (x−1..x+1) at y−4 and y+2, `pos1` at (x−4, y−5), the clip put back. The map's
  background taken at (0x208, 0x132), 0x71x0x8c; from outside the map at (0x206, 0x130)
  (320: (0x103, 0x7e)).
- **From outside**: the tiles from row 0x20 (320: 0xd) (`0x3081c`); the outside box from
  `0xe8b10` + 0x16c (the 640x480 set) + 52 · layout + 0x24.
- **Detail 3, F2's fourth state, only at 640x480** (`0x12d9d9` = 3): the view drawn at
  detail 2 (`0x143e04` = min(it, 2)) and copied with list [3], every other line. The
  "clear" flag `0x12d9da` is set at the race's start with detail 3 (`0x5bc0f`), F1 at
  detail 3 (`0x4801c`), F2 reaching 3 (`0x480bd`), the map turned off (`0x2f7a3`) and after
  a view change (`0x5bfef`); while set the frame fills the view picture with the marker
  colour `0x13d5c8`, copies it with list [2] (dash `0x12b8ac`/`0x12b8b0`; outside
  `0x145a28`/`0x145a58` + layout · 16 + 8 / + 0xc) and clears it. In the frame after a view
  change from outside, the window's odd rows are blacked in the view picture (`0x5c053`).
  At 320x200 F2 goes 0, 1 (`0x4808e`'s other test is Ctrl, which no F key passes). Values
  4 and 5 (the detail by speed, `race_update_cars` `0x51b95`) need settings above 3:
  never.
- **The replay bar**: clip (0, 0x280, 0x1c0, 0x1e0); the time `0x2d6b4(count, 0x41, 0x1c5,
  0xd, 7)`; the line at (0x41, 0x1d8), 0xf1 long, 6 high; Done without the +10 offset, its
  plain look always blitted when not lit; the mouse kept to (0, 0x1c2, 0x280, 0x1e0)
  (`0x5e4f7`; 320: (0, 0xb4, 0x140, 0xc8)). The port's mouse is in 640x480 space, scaled.
- **The pause menu** keeps the mode (`0x56ce7`, `0x580a9`: 320x200 switches to 640x480 and
  back). The mouse limits of `0x55a00` and `race_run` don't matter: `0x47b78` sets its own
  (0, 0, 0x100, 0x40) at both resolutions.
- **Checked** on the snapshots h1 (the driver's view), h6, h7 (outside), with the
  original's code in Unicorn (`Snap.vesa()` follows EA's bank switch `0x9d7f8`,
  `vesa_screen()` reads the 640x480 screen): the dashboard, map, outside frames
  (`dash_emu`), the road, walls and 3D objects (`roadpic_emu --walls --models`), the cars
  (`cars_emu`), 0 pixels off, and the 320x200 ones still equal (`tools/checks.sh`).
- **Not ported at either resolution** (found on the way; h7 shows the first: 1,201 pixels
  of the player's own car from outside):
  - ~~the player's wheels~~: done 2026-10-21, §4.10's end ("The player's car's wheels");
  - ~~the tyres' dust~~: done 2026-10-21, §4.10's end ("The tyres' dust");
  - ~~the skid marks~~: done 2026-10-21, §4.10's end ("The tyres' marks on the road");
  - ~~the bar while racing but watching another car~~: done 2026-10-21, §4.22 ("The
    watching bar");
  - ~~the start lights' beeps `0x5257c`~~: nothing to port, `0x5257c` is a bare `ret` (the
    calls stay, the sound was taken out of this build).

### 4.25 Smooth motion, 60 frames a second (2026-10-21; the port's own, `port/src/smooth.c`)

Danik asked (2026-10-20) for a 60 fps mode: the engine stepping twice as fast, or the
motion smoothed between the steps, whichever is cheaper.

- **Stepping faster is not the way.** The original's own faster clocks (`0x12dd0c` 2..5,
  37.5 to 60 steps a second; TURBO is setting 3) make the whole game faster: every step's
  sums assume their 1/30 s (`+0x506` dt, `+0x50a` rate), the gearbox and sounds every
  2nd step, the traffic and police every 4th, the replay a word every 2nd step. Faster
  steps would mean new physics, other replays and no checks against the original.
- **So the steps stay the original's, and only the picture is in between.** Before each
  step (`race_step`) the port keeps what a frame is drawn from: every car's place, pitch,
  yaw, roll, drawn yaw (`+0x351`), body lean and nod (`+0x365`, `+0x369`), its own matrix
  (`+0x20`, `+0x24`), the camera's place and angles, and the player's revs and steering for
  the dials. Before the frame, all of it is set between the kept values and the step's by
  t = (the fine clock − the tick the last step was due) / the ticks to the next (3, or
  2 2 3 with TURBO; a replay played slower: its wait); after the frame it is put back as
  the step left it. Angles go the short way round (0x1000000 a turn); a matrix element by
  element (a car tumbling 0.07 turn a step shrinks by 2 % at most in between).
- **Not smoothed (a jump, drawn as the step left it):** a car put more than 16 m, an
  eighth of a turn or a quarter of a matrix's unit away in one step (put back on the
  road, the traffic placed ahead, a replay's jump), one whose own matrix came or went, and
  the camera on another car or in another mode. The nodes (the road drawn from, the cars'
  places in the road's order) stay the step's: no gap showed at the view's bottom from
  inside or outside at 180 km/h.
- The frame is shown every retrace (60 a second at 640x480, 70 at 320x200), as before; t
  lags the step by at most one step (30 ms), as any smoothing between known states.
- **The race underneath is unchanged**: the same race driven with it off and on leaves
  the same REPLAY.RPL byte for byte, and the CD's four replays still match every one of
  their snapshots with it on (`RENFS_REPLAYCHECK=1`). Without it 2 frames of 5 repeat at
  640x480 (33.3 steps on 60 retraces); with it each frame is new and the camera moves
  evenly (62,437 to 63,137 units a frame while speeding up).
- **F11 in a race** turns it on and off, kept in the home folder's `renfs.cfg` (`smooth 1`,
  the port's file); on at first. The original reads no F10, F11 or F12 (no access to
  `0xf7b0c`, `0xf7b1f`, `0xf7b20`). `RENFS_SMOOTH=0|1` for tests (not kept).

### 4.26 Force feedback (2026-10-21; `port/src/ff.c`, `ffdev.c`, equal)

Danik asked (2026-10-21) for the groundwork of force feedback and a pad's rumble, to be
used blindly on the MiSTer (he has no wheel). The original drives an **I-Force** device
(Immersion's, the wheels and sticks of 1996) on a serial port, through a small driver of
its own linked in.

- **The driver's packets** (`0x4c258`): 0x2b, the command, the length, the bytes, an XOR of
  them all; the serial port's routines `0x60b8c`, `0x60c10`, `0x60c30`; ready `0x145c6c`.
  The commands, sizes in percent as signed bytes:
  - **0xa1 a kick** [x, y, length / 10 ms] from `0x4ba10(strength ≤ 100, direction°, ms ≤
    2500)`; its two parts from `0x4b948` and the quarter tables `0xecf8c` / `0xece20`
    (100 · sin and cos of each degree). x is the wheel's axis: 90° → −s, 270° → +s, 45° →
    −71 %, 315° → +71 %, 0° and 180° the other axis alone.
  - **0xa4, 0xa5 two shakes** [a, b, rate ≤ 50] (`0x4bbdc`/`0x4bc70`, `0x4bcbc`/`0x4bd50`;
    the second takes its two sizes the other way round); [0, 0, 0] stops one.
  - **0xa6 the rumble** [s] (`0x4bd9c`); **0xa7 a steady push** [x, y] ±100 (`0x4bf1c`).
  - **0x82** every effect stopped, **0x83** the forces on, **0x84** held off.
- **The game's side** (`0x4d670` .. `0x4dcb6`; on `0x15d144`):
  - `0x4d730` **an object hit**: strength min(mph/2 + 70, 100), length max(200 − mph, 80)
    (both compared unsigned), 45° if the hit is to the right (across > 0), else 315°.
  - `0x4d7a4` **cars meeting**: at most one every 25 ticks of `0xed100` (the last `0xed114`);
    by the impulse j: < 0x2000 100 ms 80 %, < 0x5000 150/85, < 0x10000 200/90, < 0x100000
    240/95, else 280/100; 45° if the push's x > 0 and the first car is not the one watched,
    else 315°.
  - `0x4d878` **a wall**: 70 %, 90° on the right, 270° on the left, 180 ms.
  - `0x4d8c0` **a gear change** (none if the gear is −1): 12/22/32/42 % below revs 0x1000,
    0x1300, 0x1700 and above; 180° if the gear is below the one engaged, else 0°; 80 ms.
  - `0x4d9a8` **the road's shake**, each step: mph = mulr(|v|, 0x23d70) >> 16 (v the speed
    while a player's car is watched, else the speed along the road); rate 5..13 and size
    15..24 by brackets at 5, 10, 15, 20, 30, 40, 80 and 150 mph; by the surface `+0x4a2`:
    0 none, 1 the first shake (size − 2, size − 2, rate − 2), 2 the first (size, size,
    rate), 3 the first (size, size, 1) and the second (size − 4, 0, rate), above 3 none;
    sent only when the rate or size changes (`0xed10c`, `0xed110`, `0xed120`, `0xed124`).
  - `0x4d928` **the steady push**: the car's sideways and forward forces (`+0x411`, `+0x415`)
    >> 13, clamped; not while the crash rumbles; sent when it changes (`0xed118`, `0xed11c`).
    The original's slip: a forward force below −100 becomes +100.
  - `0x4dc40` **the crash's rumble**: on → everything stopped, then 0xa6 80 (`0x12f1e0` = 1);
    off → everything stopped. `0x4d714` stop (0x82, the rumble flag 0); `0x4d6ec` / `0x4d700`
    off / on; `0x4d670` open, `0x4d6c8` close.
- **Where they are called**: the walls in the driving (`0x4037f`, `0x4042a`, the car
  watched), the gear change (`0x40c26`), an object under the car (`0x1ef40`), the boxes'
  hit (`0x2b131`, when either car is the one watched), the knock (`0x1e9a4`: the rumble
  on), the engine started again after a crash (`0x53295`: off), each step for the player's
  car unless the race is in state 3 (`0x51f22`); P's hold off/on (`0x6312a`, `0x63238`);
  the pause menu off at its start (`0x56572`), its Replay row a stop (`0x5780e`), on after
  Continue, a stop after Quit; the race's start on and a stop (`0x63628`), its end a stop
  and off (`0x63b61`); the program's end the close (`0x47219`).
- **The port**: `ff.c` is the game's side and the driver's commands, step for step;
  `ffdev.c` plays the commands on what is there. A wheel or stick with force feedback
  (SDL's haptics: the joystick's own, else the first haptic device with a constant force,
  since a wheel on the MiSTer may not be the joystick the game reads): kicks and the steady
  push as constant forces, the shakes and the rumble as sine waves (the I-Force's rumble
  rate is unknown: 20 Hz). Else a pad's rumble (`SDL_JoystickRumble`): the strong motor
  the kicks and the crash's rumble, the weak one the shakes (and half a kick), held off by
  0x84, refreshed every 100 ms; the steady push has no place on a pad. `renfs.cfg`
  "forcefeedback 0..100" (`RENFS_FF`) sets the strength, 0 none; `RENFS_FFLOG=f` writes
  every command with the race's step (`--race cy1`: 422 commands, the gear kicks
  `a1 0 -42 8` and the steady pushes).
- **Equal**: `tools/ff_emu.py` runs the original's routines in Unicorn (the packet sender
  hooked, the serial port stepped over) against `ff.c` on random cars, globals and states,
  40 calls a sequence with the state carried: **0 of 2000 calls differ** (packets and
  state); two mistakes put in on purpose gave 206 of 1000. The race underneath is
  unchanged (the CD's replays equal at every snapshot).
- **The MiSTer**: the hybrid's SDL 2.32.10 is built with `-DSDL_HAPTIC=OFF`, so there only
  the rumble works (Danik felt `SDL_JoystickRumble` on a DualSense through it). A wheel's
  forces need that SDL built with haptics on; `ffdev.c` then finds it unchanged. Untried:
  no wheel here.

### 4.27 The MiSTer's race sizes (2026-10-21; the port's own, `video.c`, `tall.c`)

Danik's five on the core's Resolution line: 320x200, 320x240, 640x200, 640x240, 640x480
(`mister_resolution()`; `RENFS_RES=320x240`, `640x240`, `640x200` try them on a PC).
- **640x240 and 640x200**: the 640x480 race as it is, shown with 240 or 200 of its rows,
  each the row at the middle of those it stands for (`video_rows`: of 480, every second for
  240, one in 2.4 for 200). The 3D painters skip the rows not shown (`raster_rows`, a byte a
  row, consulted by the quads, the far road's triangles, the fills, the shaded rows, the sky
  and the smoke; only for the main view, never with detail 2 whose lists double the view's
  rows): those rows of the picture are equal to the full 640x480 one's, checked on Replay 1.
  The PC's drawing a picture: 640x480 0.75 ms, 640x240 0.47, 640x200 0.45, 320x200 0.27.
  At 640x200 the smallest digits lose a row (the status bar's clock); 640x240 reads well.
- **320x240**: the 320x200 race's screen stretched to 240 rows (one row in five twice: the
  dashboard and HUD are the 320x200 artwork) and the 3D view drawn at 240 rows of its own.
  The cockpit's dashboard frames the windscreen, so a taller view with the artwork moved
  apart (OpenScreamer's way) would break the roof and pillars; this keeps every picture of
  the game where it is. The views ahead and outside use windows 6/5 as tall (`view.c`
  `g_windows_tall`: y, the height, the horizon's dy, the painted height, and the squash
  6/5; not the mirror's, nor detail 2's whose rows the lists double; two of the seven
  320x200 layout records are empty, all 0). The 3D is painted into its own 320x240 picture
  (`tall.c`); a 320x200 copy of it (each row from the 240's row at its middle) goes into the
  game's view picture, so that everything drawn into it afterwards (the gauges' boxes, the
  ticket, the markers) is as at 320x200. The lists' copy to the screen (`bamf_copy`) notes
  for each screen pixel which view pixel it took; when a row is shown, a pixel that still has
  that colour takes the 240-row picture's pixel instead, from the row as far from the shown
  row as the list moved the view (6/5 of it: the cockpit's view sits 17 rows down). Without
  a seam at the windscreen's edges; the pause menu (640x480) and the race after it right.
  0.2 ms more a picture than 320x200 on the PC.
- **HUD (Bottom)** (Danik, 2026-10-22: a CRT's overscan hid the replay's bar; 0 by default,
  HUD (Top) always greyed: the status bar stays): in the 240- and 480-line sizes the
  bottom HUD is lifted by that many lines of a 240-line screen (`hud_lift`, `video_lift`):
  the replay's or the watching bar, and from outside the small rev counter (its own pixels,
  not its box) and the map. Done as the rows go out, after the game has drawn: the pieces
  are on the screen only (the lists leave their holes in the view), so each piece's pixels
  are taken from the rows below, and where it was the view's picture shows (the road),
  black under the bar, and under the rev counter below the view the border it covered
  (kept when the outside screen is drawn: the view's picture still has the dial there).
  The pointer is drawn lifted over the bar and the mouse's rows there are the bar's own,
  so its buttons work where they are seen. Not from the driver's seat but the bar (the
  dashboard is one picture). `RENFS_HUDBOTTOM=n` tries it on a PC.
- **The movies' colours on the MiSTer**: the framework switches a new palette at once while
  a picture drawn ahead waits a field, so every palette change of a movie (30 to 44 in a
  movie, at key frames) showed the picture before in the new colours for a field (Danik:
  "a corrupt frame about twice a second"). A picture with new colours is now sent at once,
  not ahead (`retrace_mister`); the police's ticket had the same field.

## 5. The original in DOSBox (2026-10-19)

- The game for DOSBox is a COPY of the installed game in `~/dosgames/nfs` (`NFSSE/` +
  `UNIVBE.EXE`, from the download's `TNFSSE/`); the CD image is read where it was
  downloaded (`.../TNFSSE/cd/NFSSE.iso`, `NFS_ISO` overrides). `nfs-dos` (`~/.local/bin`)
  starts it in DOSBox Staging (flatpak) with `~/dosgames/nfs.conf` (Screamer's settings:
  S3, 60000 cycles, Pentium, SB16): `mount C ~/dosgames/nfs`, `imgmount D <iso> -t iso`,
  `C:\NFSSE\NFS`. UNIVBE is not needed (DOSBox's VESA is enough).
- **The game's pointers are its own addresses, not DOSBox's.** DOS/4GW runs with paging:
  the data object is at its own address 0x2a4000 but physically at 0xea7000. Its own
  address comes from any relocated pointer in it (`0xea828` holds image `0xe0afc`), and the
  game's memory is one run, so host = where the data object was found + (pointer − its own
  address). Checked: the road nodes' pointer `0x137430` read that way gives CY1's node 0
  byte for byte. `nfsdos.py` does this (`dos()`, the `car:` step).
- **A snapshot to run the original's code on** (2026-10-19): `snap:<f>` in a plan pauses
  DOSBox and writes 32 MB of the game's memory by its own addresses (`<f>.json`: the
  data object's offset). `tools/snap_emu.py` loads it in Unicorn and calls any routine by
  its flat address. **Each object was loaded at its own place**: the code object at flat
  + `0x1e9000`, object 2 + `0xdd000`, object 3 + `0xce000`, the data object and its BSS +
  `0x1c4000` (from the 27,294 relocated pointers, all agreeing; snapshot `r1a`). Calling
  `race_draw_frame` `0x5bc40` on a race snapshot runs (1.4 million instructions, no
  interrupts, no ports) and leaves the whole 320x200 frame (cockpit, mirror, map) at the
  game's `0xa0000`; the 3D view is drawn first into a 320x127 window at `[0xf88c8]`.
  The palette is the card's, not in memory.
- **Driving the original under XTEST**: the gearbox starts in neutral even when automatic:
  `a` (gear up) first, then the keypad's arrows (`KP_8` throttle, `KP_4` / `KP_6` steer);
  `down:<key>` / `up:<key>` keep a key held across other steps. A snapshot is taken
  whenever DOSBox is paused: it may be mid-frame (in `d1` the mirror's globals were
  current), so tests catch the globals at each call.
- **`tools/nfsdos.py <prefix> "<plan>"`** runs it unseen on `Xvfb :77` as its own child,
  presses keys (XTEST), takes pictures, finds the data object in DOSBox's memory by the
  car sound banks' names (`0xe2d14`) and reads the clocks (the header lists what).
  `peek:<s>:<a>+<a>..` samples any words (flat addresses) every 5 ms and prints the
  values each took: the way to check a rate or a set of flags in the running original.
- The way into a single race with the defaults (Ferrari 512TR against the Diablo,
  head to head, City), in seconds from the start: `wait:20` (the EA logo, Pioneer, the
  intro, the title), `key:Return:5` ×3 (title, game mode SINGLE PLAYER, the name
  "Player"), `key:Return:7` (into CONTROL CENTRAL), `key:Down:1.5` ×3 (to RACE),
  `key:Return`. The race's clock starts about 2 s later.
- **Rusty Springs** (snapshots t1..t6, 2026-10-20): after `key:Return:7` into Control
  Central, `key:Down:1.5` (to the location), `key:Right:2` ×3 (City → Coastal → Alpine →
  Rusty Springs), `key:Down:1.5` ×2, `key:Return:9`; then `key:a:0.5`, `down:KP_8` and a
  snapshot every 3–4 s. It saves the track in the copy's CONFIG.DAT: put the City one back
  afterwards (`build/snap/CONFIG_cy.DAT`) for the CY1 plans.
- **A joystick in DOSBox**: `[joystick] joysticktype=4axis, timed=true` and a mapper file
  of keys for the axes (KP4/KP6 axis 0, KP8/KP2 axis 1, KP7/KP1 axis 2, KP9/KP3 axis 3,
  KP0 and KP. buttons 1 and 2). **The mapper file must hold every key's binding**: one
  holding only the joystick's leaves the keyboard unbound. Counts run 48..2706, the
  middle about 1377.
- The race in 640x480: the cockpit view, the HUD line on top (best lap? 02:02.2, the race
  time, the gear, the distance 5.2 miles, the lap 2/2), a mirror, the map at the bottom
  right.
