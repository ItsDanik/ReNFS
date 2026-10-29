ReNFS @VERSION@ for macOS
=========================

The Need for Speed Special Edition (Electronic Arts Canada, 1996, MS-DOS),
written again by reading the original program. The whole single-player game:
every car and track, head to head, single race, time trial, tournaments, the
rally, police and traffic, replays, the cars' showcase, records, saved
tournaments, the original's codes, music and speech, English and German;
races at 640x480 or 320x200, smoothly at 60 frames a second. Not in it: the
network and modem race.

Beta. Please report problems at https://github.com/ItsDanik/ReNFS/issues

YOU NEED THE GAME'S CD
  ReNFS contains no part of the game. First move ReNFS.app out of Downloads
  (to Applications, or any folder of your own). Then copy everything on your
  Need for Speed SE CD (or in an image of it) into a folder named "NFSSE"
  next to ReNFS.app:

      ReNFS.app
      NFSSE/NFS_DOS.EXE
      NFSSE/FRONTEND/...
      NFSSE/GAMEDATA/...
      NFSSE/SIMDATA/...

  Or put the NFSSE folder in ~/Library/Application Support/renfs/ (in the
  Finder: Go -> Go to Folder...). Upper or lower case does not matter;
  nothing has to be installed or unpacked.

RUN
  Double-click ReNFS.app. macOS 10.13 or later, Apple silicon or Intel.
  ReNFS is not signed by Apple, so the first time macOS says it cannot be
  opened: click Done, then System Settings -> Privacy & Security, and
  "Open Anyway" next to ReNFS (on macOS 14 and older: right-click ReNFS.app
  -> Open -> Open). Or in Terminal:
      xattr -dr com.apple.quarantine ReNFS.app
  For the German game, in Terminal:
      open ReNFS.app --args -lang de

KEYS
  Menus:  the mouse, or the arrows, RETURN and ESC.
  Option+Return or Cmd+Return: window or the whole screen. Cmd+Q quits.
  On a Mac keyboard F1..F11 may need fn held (macOS may keep F11 for
  Show Desktop: System Settings -> Keyboard -> Keyboard Shortcuts), and
  End is fn+Right.
  Driving (Options -> Controls changes them):
          LEFT / RIGHT steer, UP / DOWN accelerate / brake,
          A / Z gear up / down (the automatic starts in neutral: one up),
          SPACE handbrake, H horn, C next camera, M map.
  Race:   ESC pause menu, P pause, F1 view size, F2 detail, F3 road distance,
          F4 sky, F5 sound, F6 markers, F7 / F8 the top bar, F9 bumper view,
          F11 smooth motion on / off, Shift+End give up,
          Ctrl+Option+1..8 watch another car.
  Game pad: left stick steers, right / left trigger accelerate / brake,
          shoulders gear up / down, A horn, B map, X camera, Y handbrake,
          START = ESC. Calibrated the first time. For analogue steering and
          pedals choose Joystick in Options -> Controls, as in the original.
          It rumbles; a force-feedback wheel gets the original's effects.

FILES IT KEEPS
  ~/Library/Application Support/renfs/
                     the game's settings and records, saved tournaments,
                     replays, the joystick's calibration, and renfs.cfg:
                       smooth 1           smooth motion (F11); 0 the original's
                       fullscreen 0       1 the whole screen (Option+Return)
                       forcefeedback 100  rumble's strength, 0..100
  (or -home <folder>)

LICENCE
  GPL-3.0 (LICENSE.txt). SDL2 (inside ReNFS.app): zlib licence
  (LICENSE-SDL2.txt).
  Source: https://github.com/ItsDanik/ReNFS
  The Need for Speed is a trademark of Electronic Arts; ReNFS is not connected
  with Electronic Arts.
