#!/usr/bin/env python3
"""The MiSTer's check of the CD (port/src/mister_files.c): every file of the English game
in the folders the game reads, made from the CD's own listing. The German ones (GART,
GSHOW, GSPEECH, GDASH, GSLIDES, GTRACKFM) are never opened on the MiSTer (English only).
Only names: nothing of the files' contents.

    tools/mister_files.py NFSSE > port/src/mister_files.c
    tools/mister_files.py NFSSE --check <log>   every path a RENFS_FILELOG asked for that
                                                is not in the list (should be none)
"""
import os, sys

FOLDERS = ["FRONTEND/ART", "FRONTEND/MOVIE", "FRONTEND/MUSIC", "FRONTEND/SHOW",
           "FRONTEND/SPEECH", "GAMEDATA/REPLAY", "SIMDATA/CARFAMS", "SIMDATA/CARSPECS",
           "SIMDATA/DASH", "SIMDATA/ETRACKFM", "SIMDATA/MISC", "SIMDATA/NTRACKFM",
           "SIMDATA/SLIDES", "SIMDATA/SOUNDBNK"]
TOP = ["NFS_DOS.EXE"]

def listing(cd):
    out = [t for t in TOP if os.path.exists(os.path.join(cd, t))]
    for f in FOLDERS:
        for root, dirs, files in os.walk(os.path.join(cd, f)):
            dirs.sort()
            rel = os.path.relpath(root, cd).replace(os.sep, "/")
            out += [rel + "/" + n for n in sorted(files)]
    return out

def main():
    cd = sys.argv[1]
    names = listing(cd)
    if len(sys.argv) > 3 and sys.argv[2] == "--check":
        have = {n.upper() for n in names}
        asked = {l.strip().replace("\\", "/").upper() for l in open(sys.argv[3]) if l.strip()}
        folders = {"FRONTEND/ART", "GAMEDATA/REPLAY"}      # asked for as folders
        real = sorted(a for a in asked if a not in have and a not in folders)
        for a in real:
            print(a)
        print("%d asked for, %d not in the list" % (len(asked), len(real)), file=sys.stderr)
        return
    print("/* The MiSTer's check of the CD (mister.c): the English game's files in the folders")
    print("   the game reads, %d of them. Made by tools/mister_files.py from the CD's listing:" % len(names))
    print("   names only. */")
    print("#include <stddef.h>\n")
    print("const char *const g_mister_files[] = {")
    for n in names:
        print('    "%s",' % n)
    print("    NULL\n};")

main()
