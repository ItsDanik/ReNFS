#!/usr/bin/env python3
"""The original's road painting loop (0x5a4f0 with 0x59d3c, after the objects' cursor
0x597e4) run on a snapshot, against the port's road_draw (port/src/roaddraw.c) given the
same road points: every call it makes must be the same, in the same order. The calls out
are caught on both sides and not followed: the quad cutter 0x35348, the two picture
painters 0x350d4 and 0x35090, the walls 0x59294, the 3D objects 0x66738 and the cars
0x679ec, 0x6837c.

    build/venv/bin/python tools/roaddraw_emu.py cy1 build/snap/d2.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_EDX, UC_X86_REG_EIP)
from snap_emu import Snap, game
from road_emu import Road, ROAD_SECTIONS
from poly_emu import Vec3

# (as tools/cars_emu.py's: video.c and the cars' files reach the rest)
SOURCES = ["ff.c", "ffdev.c", "roaddraw.c", "sky.c", "trackinfo.c", "road.c", "poly.c", "raster.c", "track.c", "files.c", "exe.c",
           "trackfam.c", "model.c", "racepal.c", "font.c", "video.c", "fsh.c", "unpack.c", "view.c", "config.c", "polyfill.c",
           "cardraw.c", "skid.c", "camera.c", "car.c", "dash.c", "hud.c", "tall.c", "crash.c", "smoke.c", "drive.c", "timing.c", "opponent.c",
           "sndlib.c", "racesnd.c", "audio.c", "sound.c", "eacs.c"]


def lib():
    out = "build/roaddraw_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["tools/timing_test.c", "tools/collide_test.c", "tools/replay_stub.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


class Scr2(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int)]


class Tex(ctypes.Structure):
    _fields_ = [("pixels", ctypes.c_void_p), ("lo", ctypes.c_void_p), ("hi", ctypes.c_void_p),
                ("rowbytes", ctypes.c_int), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("see_through", ctypes.c_int)]


class Window(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("w", ctypes.c_int), ("h", ctypes.c_int)]


I = ctypes.c_int
P = ctypes.c_void_p
QUAD = ctypes.CFUNCTYPE(None, ctypes.POINTER(Tex), I, I, I, I, ctypes.POINTER(P), ctypes.POINTER(Scr2), I)
PAINT = ctypes.CFUNCTYPE(None, P, ctypes.POINTER(Scr2))
WALL = ctypes.CFUNCTYPE(None, P, I, I, P, I, I, I)
MODEL = ctypes.CFUNCTYPE(None, I, ctypes.POINTER(Vec3), I)
CARROW = ctypes.CFUNCTYPE(None, I, I, ctypes.POINTER(Scr2), ctypes.POINTER(Scr2))
CARS = ctypes.CFUNCTYPE(None, I)


class Painters(ctypes.Structure):
    _fields_ = [("quad", QUAD), ("whole", PAINT), ("picture", PAINT), ("roadside", WALL),
                ("model", MODEL), ("car_row", CARROW), ("cars", CARS), ("cars_behind", CARS)]


def original(s, n, node):
    """the calls the original makes"""
    mu, out = s.mu, []
    fam = s.u32(0x145c50)
    regs = lambda: [mu.reg_read(r) for r in (UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX, UC_X86_REG_ECX)]
    sp = lambda: mu.reg_read(UC_X86_REG_ESP)
    stack = lambda k: struct.unpack("<%dI" % k, bytes(mu.mem_read(sp() + 4, 4 * k)))
    pts = lambda a, k: struct.unpack("<%di" % (2 * k), bytes(mu.mem_read(a, 8 * k)))
    tex = lambda a: a - fam if a else None
    sign = lambda v: v - (1 << 32) if v & 0x80000000 else v

    def quad():
        a, d, b, c = regs()
        vi, views, scr, near = stack(4)
        v = struct.unpack("<4I", bytes(mu.mem_read(views, 16)))
        return 4, ("quad", tex(a), sign(d), sign(b), sign(c), sign(vi),
                   tuple((x - game(0x138c58)) // 12 for x in v), pts(scr, 4), sign(near))

    def paint(kind):
        a, d, _, _ = regs()
        return 0, (kind, tex(a), pts(d, 4))

    def wall():
        a, d, b, c = regs()
        v, side, flag = stack(3)
        return 3, ("roadside", a - game(0xf61bc), sign(d), sign(b), c - game(0xe0000) + 0xe0000,
                   sign(v), sign(side), sign(flag))

    def model():
        a, d, b, _ = regs()
        return 0, ("model", (a - game(0x146f14)) // 0x138,
                   struct.unpack("<3i", bytes(mu.mem_read(d, 12))), sign(b))

    def car_row():
        a, d, b, c = regs()
        p0, p3 = stack(2)
        return 2, ("car_row", sign(d), sign(b), pts(p0, 1), pts(p3, 1))

    def cars():
        return 0, ("cars", sign(regs()[0]))

    caught = {0x35348: quad, 0x350d4: lambda: paint("whole"), 0x35090: lambda: paint("picture"),
              0x59294: wall, 0x66738: model, 0x679ec: car_row, 0x6837c: cars}
    def hook(mu, addr, size, f):
        k, rec = f()
        out.append(rec)
        ret = struct.unpack("<I", bytes(mu.mem_read(sp(), 4)))[0]
        mu.reg_write(UC_X86_REG_ESP, sp() + 4 + 4 * k)
        mu.reg_write(UC_X86_REG_EIP, ret)
    for at, f in caught.items():
        mu.hook_add(UC_HOOK_CODE, hook, f, begin=game(at), end=game(at))
    s.call(0x597e4)
    s.call(0x5a4f0, eax=n, edx=node)
    return out


def setup(s, name, L, n, node):
    """the port's road, track, family and view as the snapshot has them; returns the near
    plane and the window"""
    L.track_load(name.encode())
    L.track_keep_sections(node // 4 if node >= 0 else -((-node) // 4))
    if not L.trackinfo_load(name.encode(), s.i32(0x12c340), s.i32(0x143e2c)):
        sys.exit("no track info")
    if not L.trackfam_load(name.encode(), s.i32(0x12c340), s.i32(0x12da12)):
        sys.exit("no track family")
    # the pictures re-coloured through the original's inverse table of the race palette
    global _inv
    _inv = ctypes.create_string_buffer(s.raw(s.u32(0xf77c4), 32768), 32768)
    ctypes.c_void_p.in_dll(L, "g_inv").value = ctypes.addressof(_inv)
    L.pal_apply_remaps()
    L.pal_apply_pieces()
    famfile = ctypes.c_void_p.in_dll(L, "g_fam").value
    road = Road.in_dll(L, "g_road")
    ctypes.memset(ctypes.addressof(road), 0, ctypes.sizeof(road))
    sec = s.read(0x136620, 6 * ROAD_SECTIONS)
    for i in range(ROAD_SECTIONS):
        k, b, a, p = struct.unpack_from("<BBhh", sec, 6 * i)
        road.sec[i].kind, road.sec[i].behind, road.sec[i].a, road.sec[i].points = k, b, a, p
    count, xcount = s.i32(0x13bb78), s.i32(0x1365c0)
    road.sec_mask, road.drawn, road.count = s.u32(0x13bb90), n, count
    ctypes.memmove(road.world, s.read(0x137434, 12 * count), 12 * count)
    ctypes.memmove(road.view, s.read(0x138c58, 12 * count), 12 * count)
    ctypes.memmove(ctypes.byref(road.cam), s.raw(s.u32(0x143df4) + 0x10, 12), 12)
    ctypes.memmove(road.cam_m, s.raw(s.u32(0x143df8), 36), 36)
    ctypes.memmove(road.screen, s.read(0x13a47c, 8 * count), 8 * count)
    road.xsets, road.xcount, road.xfirst = s.i32(0x1365d8), xcount, s.i32(0x1365cc)
    ctypes.memmove(road.xworld, s.read(0x12f540, 12 * xcount), 12 * xcount)
    ctypes.memmove(road.xview, s.read(0x131f70, 12 * xcount), 12 * xcount)
    ctypes.memmove(road.xscreen, s.read(0x1349a0, 8 * xcount), 8 * xcount)
    ctypes.c_int.in_dll(L, "g_road_mirror").value = s.i32(0x12dd28)
    ctypes.c_int.in_dll(L, "g_road_steps").value = s.i32(0x145c94)
    L.raster_window(None, 0, s.i32(0xf88a4), s.i32(0xf88a8), s.i32(0xf88ac), s.i32(0xf88b0))
    view = s.u32(0x143dfc)
    near = struct.unpack("<i", s.raw(view + 4, 4))[0]
    win = Window(*struct.unpack("<4i", s.raw(struct.unpack("<I", s.raw(view + 0x1c, 4))[0] + 0x18, 16)))
    return near, win


def port(s, name, L, n, node):
    near, win = setup(s, name, L, n, node)
    famfile = ctypes.c_void_p.in_dll(L, "g_fam").value
    road = Road.in_dll(L, "g_road")
    out = []
    vbase = ctypes.addressof(road.view)
    exe_at = L.exe_at
    exe_at.restype = ctypes.c_void_p
    exe0 = exe_at(0x10000, 1) - 0x10000
    shape = lambda p: p - famfile if p else None
    pts = lambda sp, k: tuple(v for i in range(k) for v in (sp[i].x, sp[i].y))

    def quad(t, ush, lev, ui, vi, p, sp, near):
        out.append(("quad", shape(t.contents.pixels - 16), ush, lev, ui, vi,
                    tuple((p[i] - vbase) // 12 for i in range(4)), pts(sp, 4), near))

    def whole(sh, sp):
        out.append(("whole", shape(sh), pts(sp, 4)))

    def picture(sh, sp):
        out.append(("picture", shape(sh), pts(sp, 4)))

    def wall(pair, nd, first, tile, v, side, flag):
        out.append(("roadside", pair - exe0 - 0xf61bc, nd, first, tile - exe0, v, side, flag))

    def model(m, pos, angle):
        out.append(("model", m, (pos.contents.x, pos.contents.y, pos.contents.z), angle))

    def car_row(nd, rn, a, b):
        out.append(("car_row", nd, rn, pts(a, 1), pts(b, 1)))

    def cars(nd):
        out.append(("cars", nd))
    keep = Painters(QUAD(quad), PAINT(whole), PAINT(picture), WALL(wall), MODEL(model),
                    CARROW(car_row), CARS(cars))
    Painters.in_dll(L, "g_road_paint").__init__(*[getattr(keep, f) for f, _ in Painters._fields_])
    L.road_objects_start()
    L.road_draw(n, node, near, ctypes.byref(win))
    return out


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    for p in snaps:
        s = Snap(p)
        n = s.i32(0x136618)
        node = struct.unpack("<i", s.raw(s.u32(0x143df4) + 0x38, 4))[0]
        got = port(s, name, L, n, node)
        want = original(s, n, node)
        kinds = {}
        for w in want:
            kinds[w[0]] = kinds.get(w[0], 0) + 1
        first = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), None)
        if first is None and len(want) == len(got):
            print("%s: %d calls, all equal (%s)" % (os.path.basename(p), len(want),
                                                    ", ".join("%s %d" % kv for kv in sorted(kinds.items()))))
        else:
            first = first if first is not None else min(len(want), len(got))
            print("%s: %d calls, the port %d; first difference at %d:" % (
                os.path.basename(p), len(want), len(got), first))
            for i in range(max(0, first - 2), min(first + 3, max(len(want), len(got)))):
                print("  original", want[i] if i < len(want) else None)
                print("  port    ", got[i] if i < len(got) else None)


if __name__ == "__main__":
    main()
