#!/usr/bin/env python3
"""The player's car driven, the original's code against the port's, step by step: on a
snapshot of a race, the original's chain of a race step for the player's car (every
second step the controls, 0x10400; then its kind's routine: drive_player 0x382d0
(car_physics_player 0x408e0, car_check_lap 0x5056c, the car on the road 0x38808) or,
tumbling after a hard hit on a wall, drive_crashed 0x1cfe0; then car_road_offset 0x51598
and cars_meets_world 0x1c878, its traffic_police_tick 0x7a040 cut down to the player's
0x786d4) run in the emulator, and the port's drive_controls, drive_step and crash_world
from the same car record and globals, given the same control words. After every step
every field of the car the port keeps is compared (its tumbling body too), the globals
the driving and the crash write and the smoke's block. The sounds, the replay's
highlights, the acceleration test's times, the force feedback and the roadside objects
are left out on the original's side (LEFT_OUT); the cars meet each other on both sides
(collide.c, from the snapshot's random numbers and order).

    build/venv/bin/python tools/drive_emu.py [snapshot ..]     (default: all d*, e*, r*)
"""
import ctypes, glob, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap, game, flat
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

SOURCES = ["ff.c", "ffdev.c", "drive.c", "cardraw.c", "skid.c", "model.c", "polyfill.c", "camera.c", "car.c", "dash.c", "hud.c", "tall.c",
           "config.c", "view.c", "sky.c", "roaddraw.c", "trackinfo.c", "road.c", "poly.c",
           "raster.c", "track.c", "files.c", "exe.c", "trackfam.c", "racepal.c", "font.c",
           "video.c", "fsh.c", "unpack.c", "timing.c", "smoke.c", "opponent.c", "sndlib.c",
           "racesnd.c", "audio.c", "sound.c", "eacs.c"]
# (crash.c is compiled into drive_test.c, which reaches its parts)
# sounds (0x52d2c, 0x52e40, 0x52ea8: two words on the stack each), highlights, the
# acceleration test's times, speech, force feedback, the wall's sound by the camera,
# EXPLODE's push; the crash's sounds (0x53214, 0x531dc)
LEFT_OUT = {0x52d2c: 2, 0x52e40: 2, 0x52ea8: 2, 0x5c6d4: 0, 0x2d71c: 0, 0x2d778: 0,
            0x722b4: 0, 0x72438: 0, 0x4d8c0: 0, 0x4d878: 0, 0x7fb58: 0, 0x78c30: 0,
            0x53214: 0, 0x531dc: 0}
KIND = {2: 0x382d0, 4: 0x1cfe0}
CAR, SPEC = 0x800, 0x800

# the driving's globals, in drive_test.c's order: (address, size)
GLOBALS = [(0x145c94, 4), (0x12f1bc, 1), (0xf6b74, 4), (0x12da12, 4), (0x12da0e, 4), (0x12dd2c, 4),
           (0x12dd10, 4), (0x12dd54, 4), (0x129ecc, 4), (0x13c160, 4), (0x13c164, 4), (0x12cca0, 4),
           (None, 0), (0xed100, 4), (0xec31c, 4), (0xec314, 4), (0xec318, 4), (0x13c170, 4),
           (0x12da28, 4), (0x12ba24, 4), (0x12dd58, 4), (0x12da30, 4), (0xec288, 4), (None, 0)]
GLOBALS += [(0x12d954 + 4 * p, 4) for p in range(8)] + [(0x12d974 + 12 * p, 4) for p in range(8)]
GLOBALS += [(0x12d978 + 12 * p, 4) for p in range(8)] + [(0x12ccac + 4 * p, 4) for p in range(8)]
GLOBALS += [(0x14ea80 + 0x1d2 * p + 0x1c4, 4) for p in range(8)]
WRITTEN = {17: "first_move", 19: "air_shake", 22: "spinning", 56: "top_speed"}
# the crash's, in drive_test.c's order; the last are only read
CRASH_GLOBALS = [(0xea5d0, "hit_wait"), (0xea5d4, "hit_level"), (0xea66c, "impulse"),
                 (0xea670, "depth"), (0x12b4c4, "point_vel.x"), (0x12b4c8, "point_vel.y"),
                 (0x12b4cc, "point_vel.z"), (0x129fac, "refresh"), (0x145c98, "view"),
                 (0xf6b70, "respawned"), (0xea700, "restart"), (0xf6dd8, "random")]
CRASH_IN = [0x12dd08, 0x12da34, 0x12da2c, 0x12da38]
CRASHES = [0x14ec38 + 0x1d2 * p for p in range(8)]
SMOKE, SMOKE_LEN = 0x1464d0, 0x96c

# control words: steering bits 0..5 (32 straight, 0 full left, 63 full right), pedals
# 6..11 (30 neither, 0 the throttle, 63 the brake), 12..15 (1 up, 2 down, 4 horn, 8 the
# handbrake)
def word(steer=32, pedal=30, buttons=0):
    return steer | pedal << 6 | buttons << 12
GAS, BRAKE, COAST = 0, 63, 30


# into drive: past the start's 300 steps (shifts before it are only noted), the lever up
# (the snapshots at the start stand in neutral)
START = [(24, word()), (2, word(buttons=1)), (2, word())]


def script(name):
    """(steps, word) pieces"""
    return START + pieces(name)


def pieces(name):
    if name == "gas":
        return [(400, word(pedal=GAS))]
    if name == "weave":
        return [(60, word(pedal=GAS)), (20, word(0, GAS)), (25, word(63, GAS)), (20, word(0, GAS)),
                (30, word(pedal=COAST)), (15, word(63, COAST)), (40, word(pedal=GAS)),
                (25, word(pedal=BRAKE)), (10, word(0, BRAKE)), (40, word(63, GAS)), (30, word(0, GAS))]
    if name == "handbrake":
        return [(80, word(pedal=GAS)), (8, word(0, COAST, 8)), (30, word(0, GAS)), (12, word(63, COAST, 8)),
                (40, word(pedal=GAS)), (60, word(pedal=BRAKE)), (40, word(pedal=COAST))]
    if name == "manual":
        seq = [(30, word(pedal=GAS))]
        for k in range(5):
            seq += [(1, word(pedal=GAS, buttons=1)), (45, word(pedal=GAS))]
        seq += [(1, word(pedal=BRAKE, buttons=2)), (20, word(pedal=BRAKE)), (1, word(pedal=COAST, buttons=2)),
                (30, word(10, GAS)), (40, word(pedal=BRAKE)), (1, word(pedal=COAST, buttons=2)),
                (1, word(pedal=COAST, buttons=2)), (40, word(pedal=COAST)), (60, word(pedal=GAS))]
        return seq
    if name == "reverse":
        down = [(2, word(pedal=BRAKE, buttons=2)), (2, word(pedal=BRAKE))]
        return [(60, word(pedal=BRAKE))] + down + down + [(90, word(20, GAS)), (40, word(pedal=BRAKE)),
                                                         (60, word(50, GAS)), (2, word(buttons=1)),
                                                         (2, word()), (2, word(buttons=1)), (60, word(pedal=GAS))]
    if name == "joy":
        # a joystick's in-between steering and pedals
        seq = []
        for k, (st, pd) in enumerate([(32, 10), (40, 5), (24, 0), (50, 20), (14, 30), (32, 40),
                                       (44, 52), (32, 2), (20, 8), (60, 0), (32, 45), (5, 25)]):
            seq.append((22 + 3 * k, word(st, pd)))
        return seq
    if name == "tumble":
        return [(450, word(pedal=COAST)), (120, word(pedal=GAS))]
    if name == "tumble gas":
        return [(300, word(pedal=GAS)), (150, word(40, GAS)), (100, word(pedal=GAS))]
    raise ValueError(name)


# the settings tried on top of each snapshot's: name, and the globals, the car's fields
# and its physics' words changed
VARIANTS = [("as is", {}, {}, {}),
            ("abs tcs", {}, {0x48e: 1, 0x492: 1}, {}),
            ("manual", {}, {0x3e9: 0}, {}),
            ("rally", {0x12dd2c: 1}, {}, {}),
            ("circuit", {0x12da12: 4}, {}, {}),
            ("drag strip", {0x12da12: 3}, {}, {}),
            ("soft landings", {0x12da28: 0x20}, {}, {}),
            ("joystick", {0x12d974: 1, 0x12d978: 1}, {}, {}),
            ("past finish", {0x13c164: 30}, {}, {}),
            ("revving", {0x12ccac: 1}, {}, {}),
            ("segment 2", {0x12da12: 2, 0x12da0e: 2}, {}, {}),
            ("two players", {0x12da30: 2, 0x12dd10: 4}, {}, {}),
            ("top speed 16", {}, {}, {0x38: 0x100000}),
            ("rolled", {}, {0x14: 0x100000}, {}),
            ("jump", {}, {0x58: 0x80000}, {}),
            ("jump soft", {0x12da28: 0x20}, {0x58: 0xa0000}, {}),
            ("rally jump", {0x12dd2c: 1}, {0x58: 0x90000, 0x14: 0xff0000}, {}),
            ("nitro", {0x12cca0: 1}, {}, {}),
            # thrown at the walls: hard hits, the crash
            ("thrown right", {}, {0x54: 0x300000, 0x5c: 0x200000}, {}),
            ("thrown left", {}, {0x54: -0x300000, 0x5c: 0x280000}, {}),
            ("thrown spinning", {}, {0x54: 0x380000, 0x5c: 0x380000, 0x74: 0x300000}, {}),
            ("thrown up", {}, {0x54: -0x380000, 0x58: 0xc0000, 0x5c: 0x300000}, {}),
            ("thrown rally", {0x12dd2c: 1}, {0x54: 0x400000, 0x5c: 0x300000}, {}),
            ("thrown manual", {}, {0x3e9: 0, 0x54: -0x380000, 0x5c: 0x300000}, {}),
            ("thrown circuit", {0x12da12: 4}, {0x54: 0x380000, 0x5c: 0x100000}, {})]
# keys held through a run (the original's table 0xf7ac8 + scan code): N and B, NITRO's
KEYS = {"nitro": [0x31, 0x30]}
V = {v[0]: k for k, v in enumerate(VARIANTS)}
RUNS = [("gas", "as is"), ("weave", "as is"), ("handbrake", "as is"), ("weave", "abs tcs"),
        ("manual", "manual"), ("reverse", "as is"), ("weave", "rally"), ("handbrake", "rally"),
        ("weave", "circuit"), ("gas", "drag strip"), ("weave", "drag strip"), ("weave", "soft landings"),
        ("joy", "joystick"), ("joy", "abs tcs"), ("gas", "past finish"), ("weave", "revving"),
        ("weave", "segment 2"), ("gas", "two players"), ("gas", "top speed 16"),
        ("weave", "rolled"), ("gas", "jump"), ("weave", "jump soft"), ("weave", "rally jump"),
        ("reverse", "rally"), ("joy", "rally"), ("gas", "nitro"), ("weave", "nitro"),
        ("tumble", "thrown right"), ("tumble", "thrown left"), ("tumble", "thrown spinning"),
        ("tumble", "thrown up"), ("tumble", "thrown rally"), ("tumble", "thrown manual"),
        ("tumble", "thrown circuit"), ("tumble gas", "thrown right"), ("tumble gas", "thrown left")]


def lib():
    import subprocess
    out = "build/drive_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-fwrapv", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["tools/drive_test.c", "tools/collide_test.c", "tools/replay_stub.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


def fields(L):
    out, i = [], 0
    at, size = ctypes.c_int(), ctypes.c_int()
    while L.drive_test_field(i, ctypes.byref(at), ctypes.byref(size)):
        out.append((at.value, size.value))
        i += 1
    return out


def rd(s, a, n):
    v = int.from_bytes(s.read(a, n), "little", signed=n == 4)
    return v


def wr(s, a, n, v):
    s.write(a, (v & ((1 << 8 * n) - 1)).to_bytes(n, "little"))


# the ported routines, for --coverage: (first, last instruction + 1)
PORTED = [(0x10010, 0x10428), (0x483e4, 0x48b80), (0x5162c, 0x51690), (0x3e918, 0x3e9c4),
          (0x3e9c4, 0x41a2b), (0x188a0, 0x18a50), (0x38434, 0x384ba), (0x38548, 0x38c0f),
          (0x5056c, 0x506da), (0x51598, 0x5162b),
          # the crash's
          (0x3fed4, 0x4008a), (0x78204, 0x7836e), (0x1cb14, 0x1cbb0), (0x1cbb0, 0x1cd3d),
          (0x1cd40, 0x1ceb2), (0x1ceb4, 0x1cfdf), (0x1cfe0, 0x1e314), (0x2a334, 0x2a917),
          (0x2a918, 0x2afb7), (0x2c25c, 0x2c2d6), (0x2c2d8, 0x2c3d7), (0x2c3d8, 0x2c48d),
          (0x2c490, 0x2c584), (0x2c584, 0x2c71c), (0x2c894, 0x2c91f), (0x2c9c0, 0x2ca92),
          (0x2cb78, 0x2cc5d), (0x3e784, 0x3e7e9), (0x3e7fc, 0x3e845), (0x64588, 0x645b9),
          (0x64804, 0x64977), (0x64978, 0x64a73), (0x7dbd8, 0x7dd45), (0x786d4, 0x788b5),
          (0x78a19, 0x78a7f), (0x7b130, 0x7b175), (0x1e6cc, 0x1e719), (0x1c878, 0x1ca99),
          # the cars meeting
          (0x1c6a4, 0x1c7ff), (0x1c800, 0x1c875), (0x1e314, 0x1e6cb), (0x1e838, 0x1e9ca),
          (0x2afb8, 0x2b863), (0x2b864, 0x2bf9b), (0x26324, 0x2a331), (0x2bf9c, 0x2c0f1),
          (0x2c1c0, 0x2c259), (0x52580, 0x52599), (0x91e04, 0x91e6a),
          # the roadside objects
          (0x1ebf4, 0x1ee57), (0x1ee80, 0x1efa8)]
HIT = set()


def set_collide(L, s):
    """the cars meeting: EA's random numbers and the cars by node as the original's"""
    st = (ctypes.c_uint * 6)(*struct.unpack("<6I", s.read(0xf8a8c, 24)))
    L.collide_test_random(st, 1)
    L.collide_test_set_impulse(s.i32(0xea668))
    n = s.i32(0x12da38)
    L.collide_test_order((ctypes.c_int * n)(*struct.unpack("<%di" % n, s.read(0x13c298, 4 * n))), n)
    s.write(0x15d144, b"\0\0\0\0")           # (no force feedback)
    # the roadside objects knocked over
    n = 2 * L.collide_test_placements()
    L.collide_test_fallen(ctypes.create_string_buffer(s.raw(s.u32(0xea5d8), n), n), n, 1)


def fallen_differ(L, s):
    """the roadside objects knocked over: the first that differs, else None"""
    n = 2 * L.collide_test_placements()
    mine = ctypes.create_string_buffer(n)
    L.collide_test_fallen(mine, n, 0)
    theirs = s.raw(s.u32(0xea5d8), n)
    if mine.raw == theirs:
        return None
    i = next(i for i in range(n) if mine.raw[i] != theirs[i])
    return "object %d knocked %d/%d" % (i // 2, theirs[i], mine.raw[i])


def run(L, F, path, scr, var, coverage=False):
    s = Snap(path)
    mu = s.mu
    if coverage:
        for lo, hi in PORTED:
            mu.hook_add(UC_HOOK_CODE, lambda mu, a, n, u: HIT.add(flat(a)), None, begin=game(lo), end=game(hi) - 1)

    def hook(mu, addr, size, k):
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp, 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, k in LEFT_OUT.items():
        mu.hook_add(UC_HOOK_CODE, hook, k, begin=game(at), end=game(at))
    car = s.u32(0x13c24c + 4 * s.i32(0x12dd58))

    def refresh(mu, addr, size, u):
        # traffic_police_tick cut down to the player's car (0x786d4), as the port has it
        if s.i32(0xf74ac) == 0:
            mu.reg_write(UC_X86_REG_EAX, car)
            mu.reg_write(UC_X86_REG_EIP, game(0x786d4))
        else:
            hook(mu, addr, size, 0)
    mu.hook_add(UC_HOOK_CODE, refresh, None, begin=game(0x7a040), end=game(0x7a040))
    vname, gset, cset, sset = VARIANTS[var]
    for a, v in gset.items():
        wr(s, a, 4, v)
    for o, v in cset.items():
        mu.mem_write(car + o, struct.pack("<i", v))
    # the surfaces for the track's kind, the original's and the port's
    s.call(0x3f364)
    for k, (a, n) in enumerate(GLOBALS):
        L.drive_test_global(k, 1, rd(s, a, n) if a else 0)
    L.drive_test_global(12, 1, 1 if s.i32(0xf74f8) == 4 else 0)
    L.drive_test_track(s.read(0x12da16, 8).split(b"\0")[0])
    for k in KEYS.get(vname, []):
        s.write(0xf7ac8 + k, b"\1")
    L.drive_test_global(23, 1, 1 if vname in KEYS else 0)
    L.drive_surfaces()
    for k, (a, nm) in enumerate(CRASH_GLOBALS):
        L.crash_test_global(k, 1, rd(s, a, 4))
    for k, a in enumerate(CRASH_IN + CRASHES):
        L.crash_test_global(len(CRASH_GLOBALS) + k, 1, rd(s, a, 4))
    smoke = ctypes.create_string_buffer(bytes(s.read(SMOKE, SMOKE_LEN)), SMOKE_LEN)
    L.crash_test_smoke(smoke, 1)
    mine = (ctypes.c_int * 16).in_dll(L, "g_surfaces")
    theirs = struct.unpack("<16i", s.read(0x12cc00, 64))
    if list(mine)[:12] != list(theirs)[:12]:
        return "surfaces differ: %s / %s" % (list(mine)[:12], theirs[:12])
    raw = bytes(mu.mem_read(car, CAR))
    if struct.unpack("<I", raw[0xc0:0xc4])[0] != car + 0xc4:
        return "the body is not at +0xc4"
    spec_at = struct.unpack("<I", raw[0x479:0x47d])[0]
    for o, v in sset.items():
        mu.mem_write(spec_at + o, struct.pack("<i", v))
    spec = ctypes.create_string_buffer(bytes(mu.mem_read(spec_at, SPEC)), SPEC)
    rc = ctypes.create_string_buffer(CAR * 2)
    L.drive_test_in(rc, raw, spec)
    out = ctypes.create_string_buffer(CAR)
    player = s.i32(0x12dd58)
    # the other cars' records, for the two players' catching up (their progress only)
    others = []
    for k in range(s.i32(0x12da38)):
        a = s.u32(0x13c24c + 4 * k)
        if k == player:
            o = rc
        else:
            o = ctypes.create_string_buffer(CAR * 2)
            L.drive_test_in(o, bytes(mu.mem_read(a, CAR)), spec)
            others.append(o)
        L.drive_test_car(k, o)
    set_collide(L, s)
    knocked = [0]

    def knock(mu, addr, size, u):
        knocked[0] += 1
    for at in (0x1ed0a, 0x1ee16):     # (0x5004c after an object is knocked over)
        mu.hook_add(UC_HOOK_CODE, knock, None, begin=game(at), end=game(at))
    n = tumbling = 0
    for steps, w in script(scr):
        for _ in range(steps):
            st = s.i32(0x145c94)
            fl = (1 if st & 1 == 0 else 0) | (4 if st & 3 == 0 else 0) | (0x10 if st & 7 == 0 else 0)
            wr(s, 0x12f1bc, 1, fl)
            wr(s, 0x12d954 + 4 * player, 4, w)
            L.drive_test_global(0, 1, st)
            L.drive_test_global(1, 1, fl)
            L.drive_test_global(24 + player, 1, w)
            if fl & 4:
                s.call(0x10400, eax=car)
                L.drive_controls(rc)
            kind = struct.unpack("<i", bytes(mu.mem_read(car + 0x512, 4)))[0]
            if kind not in KIND:
                return "step %d: kind %d, not in the port" % (n, kind)
            tumbling += kind == 4
            s.call(KIND[kind], eax=car)
            s.call(0x51598, eax=car)
            s.call(0x1c878)
            L.drive_step(rc)
            L.crash_world()
            theirs = bytes(mu.mem_read(car, CAR))
            out.raw = theirs
            L.drive_test_out(rc, out)
            diff = []
            for at, size in F:
                a = int.from_bytes(theirs[at:at + size], "little", signed=True)
                b = int.from_bytes(out.raw[at:at + size], "little", signed=True)
                if a != b:
                    diff.append("+%#x %d/%d" % (at, a, b))
            for k, nm in WRITTEN.items():
                a = rd(s, *GLOBALS[k + (player if k == 56 else 0)])
                b = L.drive_test_global(k + (player if k == 56 else 0), 0, 0)
                if a != b:
                    diff.append("%s %d/%d" % (nm, a, b))
            for k, (a, nm) in enumerate(CRASH_GLOBALS):
                if rd(s, a, 4) != L.crash_test_global(k, 0, 0):
                    diff.append("%s %d/%d" % (nm, rd(s, a, 4), L.crash_test_global(k, 0, 0)))
            k = len(CRASH_GLOBALS) + len(CRASH_IN) + player
            if rd(s, CRASHES[player], 4) != L.crash_test_global(k, 0, 0):
                diff.append("crashes %d/%d" % (rd(s, CRASHES[player], 4), L.crash_test_global(k, 0, 0)))
            L.crash_test_smoke(smoke, 0)
            theirs_smoke = bytes(s.read(SMOKE, SMOKE_LEN))
            if smoke.raw != theirs_smoke:
                at = next(i for i in range(SMOKE_LEN) if smoke.raw[i] != theirs_smoke[i])
                diff.append("smoke from +%#x (%s / %s)" % (at, theirs_smoke[at - at % 4:at - at % 4 + 16].hex(),
                                                            smoke.raw[at - at % 4:at - at % 4 + 16].hex()))
            fd = fallen_differ(L, s)
            if fd:
                diff.append(fd)
            if diff:
                return "step %d (word %#x, steps %d, kind %d): %s" % (n, w, st, kind, ", ".join(diff[:12]))
            wr(s, 0x145c94, 4, st + 2)
            # the 100 Hz clock, three ticks a step
            clock = s.i32(0xed100) + 3
            wr(s, 0xed100, 4, clock)
            L.drive_test_global(13, 1, clock)
            n += 1
    f = lambda o: struct.unpack("<i", bytes(mu.mem_read(car + o, 4)))[0]
    crash = ", %d tumbling, %d crashes" % (tumbling, rd(s, CRASHES[player], 4)) if tumbling else ""
    if knocked[0]:
        crash += ", %d objects knocked over" % knocked[0]
    return "equal, %d steps (to node %d, %.1f m/s, gear %d, %d rpm%s)" % (
        n, f(0x48), f(0x68) / 65536, f(0x3ed), f(0x3c1), crash)


def report_coverage():
    """the instructions of the ported routines the original never ran, as ranges"""
    import subprocess
    for lo, hi in PORTED:
        out = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                              "--start-address=%#x" % lo, "--stop-address=%#x" % hi,
                              "build/nfs_flat.bin"], capture_output=True, text=True).stdout
        addrs = [int(l.split(":")[0], 16) for l in out.splitlines()[7:] if l.strip() and l.strip()[0] in "0123456789abcdef" and ":" in l]
        miss, run_ = [], None
        for a in addrs:
            if a not in HIT:
                run_ = run_ or [a, a]
                run_[1] = a
            elif run_:
                miss.append(run_)
                run_ = None
        if run_:
            miss.append(run_)
        print("%#x..%#x: %d of %d run; not: %s" % (lo, hi, len([a for a in addrs if a in HIT]), len(addrs),
              " ".join("%x-%x" % (a, b) if a != b else "%x" % a for a, b in miss)))


# the fields the loader sets (load_car_pbs 0x50c24 .. car_reset 0x50830), compared
# with a snapshot's car before its first step would be no use (they change as it drives):
# only those that stay
LOADED = [0x43d, 0x44d, 0x451, 0x455, 0x459, 0x461, 0x465, 0x48e, 0x492, 0x49a, 0x49c, 0x49e,
          0x4a0, 0x48a]


def check_load(L, path):
    """the port's loader against the car the snapshot's game loaded: its physics as
    load_car_pbs left them, and the fields that stay as it drives"""
    s = Snap(path)
    player = s.i32(0x12dd58)
    car = s.u32(0x13c24c + 4 * player)
    raw = bytes(s.mu.mem_read(car, CAR))
    spec_at = struct.unpack("<I", raw[0x479:0x47d])[0]
    theirs = bytes(s.mu.mem_read(spec_at, 0x778))
    rec = 0x12da48 + 0x4b * player
    auto, tcs, abs_ = s.i32(rec + 0xf), s.i32(rec + 0x13), s.i32(rec + 0x17)
    spec = ctypes.create_string_buffer(0x800)
    out = ctypes.create_string_buffer(CAR)
    if not L.drive_test_load(s.i32(rec + 0xb), player, auto, tcs, abs_, s.i32(0x12dd2c),
                             s.i32(0x12da12), spec, out):
        return "the port could not load the car"
    diff = [k for k in range(0x778) if spec.raw[k] != theirs[k]]
    if diff:
        return "the physics differ at %d bytes from +%#x" % (len(diff), diff[0])
    bad = []
    for at in LOADED:
        size = 2 if 0x49a <= at < 0x4a2 else 4
        a, b = (int.from_bytes(x[at:at + size], "little", signed=True) for x in (raw, out.raw))
        if a != b:
            bad.append("+%#x %d/%d" % (at, a, b))
    return "loaded equal" if not bad else "loaded: " + ", ".join(bad)


def main():
    args = sys.argv[1:]
    coverage = "--coverage" in args
    args = [a for a in args if a not in ("--coverage", "--load")]
    paths = args or sorted(glob.glob("build/snap/[der]*.bin"))
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    if not L.track_load(b"cy1"):
        sys.exit("no track")
    F = fields(L)
    bad = 0
    for p in paths:
        r = check_load(L, p)
        bad += r != "loaded equal"
        print("%s %s" % (os.path.basename(p), r))
    if "--load" in sys.argv:
        return
    for p in paths:
        for scr, var in RUNS:
            r = run(L, F, p, scr, V[var], coverage)
            bad += not r.startswith("equal")
            print("%s %-9s %-13s %s" % (os.path.basename(p), scr, var, r))
    print("all equal" if not bad else "%d runs differ" % bad)
    if coverage:
        report_coverage()


if __name__ == "__main__":
    main()
