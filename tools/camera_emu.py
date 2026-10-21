#!/usr/bin/env python3
"""The starting grid (0x7c938), the road node a position is on (0x4f534) and the driver's
camera (camera_update 0x136b0, modes 0 and 1) run on snapshots in Unicorn, against the
port's car_grid_place, track_follow and camera_update (port/src/car.c, track.c,
camera.c) given the same car and camera.

    build/venv/bin/python tools/camera_emu.py cy1 build/snap/r1a.bin [...]
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snap_emu import Snap
from poly_emu import Vec3

SOURCES = ["ff.c", "ffdev.c", "camera.c", "car.c", "model.c", "polyfill.c", "dash.c", "hud.c", "tall.c", "config.c", "view.c", "sky.c", "roaddraw.c", "trackinfo.c", "road.c", "poly.c",
           "raster.c", "track.c", "files.c", "exe.c", "trackfam.c", "racepal.c", "font.c", "video.c",
           "fsh.c", "unpack.c", "crash.c", "smoke.c", "drive.c", "cardraw.c", "skid.c", "timing.c", "opponent.c", "collide.c",
           "sndlib.c", "racesnd.c", "audio.c", "sound.c", "eacs.c"]
I = ctypes.c_int
U8 = ctypes.c_ubyte


def lib():
    out = "build/camera_test.so"
    subprocess.check_call(["cc", "-O1", "-g", "-shared", "-fPIC", "-o", out] +
                          ["port/src/" + f for f in SOURCES] + ["tools/camera_test.c", "tools/replay_stub.c", "-Iport/src"] +
                          subprocess.check_output(["sdl2-config", "--cflags", "--libs"]).decode().split())
    return ctypes.CDLL(os.path.abspath(out))


class Car(ctypes.Structure):
    _fields_ = [("pos", Vec3), ("pitch", I), ("yaw", I), ("roll", I), ("node", I), ("body_roll", I),
                ("body_pitch", I), ("dist", I), ("roll_phase", U8), ("pitch_phase", U8),
                ("roll_shake", I), ("pitch_shake", I), ("player", I), ("number", I)]


class Camera(ctypes.Structure):
    _fields_ = [("mode", I), ("wanted", I), ("frames", I), ("pos", Vec3), ("off", Vec3), ("node", I),
                ("flags", I), ("pitch", I), ("yaw", I), ("roll", I), ("tilt", I * 3), ("eye", Vec3),
                ("mirror_on", I), ("yaw_follow", I), ("step", Vec3), ("ahead", ctypes.c_void_p),
                ("mirror", ctypes.c_void_p), ("car", ctypes.POINTER(Car)), ("m", I * 9), ("mm", I * 9)]


# the camera record's fields compared, (name, offsets in the original's, the port's)
CAM_FIELDS = [("mode, wanted, frames", (0, 4, 0xc), lambda k: (k.mode, k.wanted, k.frames)),
              ("pos", (0x10, 0x14, 0x18), lambda k: (k.pos.x, k.pos.y, k.pos.z)),
              ("off", (0x2c, 0x30, 0x34), lambda k: (k.off.x, k.off.y, k.off.z)),
              ("node, flags", (0x38, 0x3c), lambda k: (k.node, k.flags)),
              ("angles", (0x40, 0x44, 0x48), lambda k: (k.pitch, k.yaw, k.roll)),
              ("tilt", (0x5c, 0x60, 0x64), lambda k: tuple(k.tilt)),
              ("mirror", (0x68,), lambda k: (k.mirror_on,)),
              ("eye", (0x6c, 0x70, 0x74), lambda k: (k.eye.x, k.eye.y, k.eye.z)),
              ("yaw follow", (0x78,), lambda k: (k.yaw_follow,)),
              ("step", (0x7c, 0x80, 0x84), lambda k: (k.step.x, k.step.y, k.step.z))]


def car_of(s, a):
    g = lambda o: struct.unpack("<i", s.raw(a + o, 4))[0]
    b = lambda o: s.raw(a + o, 1)[0]
    c = Car()
    c.pos = Vec3(g(0), g(4), g(8))
    c.pitch, c.yaw, c.roll, c.node = g(0xc), g(0x10), g(0x14), g(0x48)
    c.body_roll, c.body_pitch, c.dist = g(0x365), g(0x369), g(0x39d)
    c.roll_phase, c.pitch_phase, c.roll_shake, c.pitch_shake = b(0x427), b(0x428), g(0x429), g(0x42d)
    c.player, c.number = g(0x47d), g(0x48a)
    return c


def check(path, L, bad, stats):
    s = Snap(path)
    name = os.path.basename(path)
    cars = [s.u32(0x13c24c + 4 * k) for k in range(s.i32(0x12da38))]
    racers, kind = s.i32(0x12da2c), s.i32(0x12da12)

    # the grid: every racer placed by both
    for a in cars[:racers]:
        o = Snap(path)
        o.call(0x7c938, eax=a)
        want = struct.unpack("<3i", o.raw(a, 12)) + struct.unpack("<i", o.raw(a + 0x10, 4)) + \
            struct.unpack("<i", o.raw(a + 0x48, 4)) + struct.unpack("<i", o.raw(a + 0x39d, 4))
        c = car_of(s, a)
        L.car_grid_place(ctypes.byref(c), s.i32(0x12dca0 + 4 * c.player), racers, kind)
        got = (c.pos.x, c.pos.y, c.pos.z, c.yaw, c.node, c.dist)
        if want != got:
            bad.append("%s grid, car %d: the original %s, the port %s" % (name, c.player, want, got))

    # the node of every car's position, from 12 nodes behind to 12 ahead
    for a in cars:
        c = car_of(s, a)
        for d in range(-12, 13):
            start = c.node + d
            if start < 0 and not s.i32(0x13bb80):
                continue
            o = Snap(path)
            o.mu.mem_write(a + 0x48, struct.pack("<i", start))
            r = o.call(0x4f534, eax=a, edx=a + 0x48)
            want = (struct.unpack("<i", o.raw(a + 0x48, 4))[0], r & 0xffffffff)
            n = I(start)
            r = L.track_follow(ctypes.byref(c.pos), ctypes.byref(n))
            got = (n.value, r & 0xffffffff)
            stats["follow"] += 1
            stats["moved"] += want[0] != start
            if want != got:
                bad.append("%s follow, car %d from %d: the original %s, the port %s" % (
                    name, c.player, start, want, got))

    # the camera
    cam = s.u32(0x143df4)
    g = lambda o: struct.unpack("<i", s.raw(cam + o, 4))[0]
    if g(0) > 1:
        bad.append("%s: camera mode %d not ported" % (name, g(0)))
        return
    car_at = struct.unpack("<I", s.raw(cam + 0x90, 4))[0]
    if car_at != cars[s.i32(0x12dd58)]:
        bad.append("%s: the camera on another car" % name)
    c = car_of(s, car_at)
    k = Camera()
    k.mode, k.node, k.flags = g(0), g(0x38), g(0x3c)
    k.tilt[1] = g(0x60)
    k.eye = Vec3(g(0x6c), g(0x70), g(0x74))
    k.car = ctypes.pointer(c)
    L.camera_init(struct.unpack("<i", s.raw(struct.unpack("<I", s.raw(car_at + 0x479, 4))[0]
                                            + 0x36c, 4))[0])
    ctypes.c_int.in_dll(L, "g_cam_pitch_bias").value = s.i32(0x12ba20)
    o = Snap(path)
    o.call(0x136b0, eax=cam)
    h = lambda o_: struct.unpack("<i", o.raw(cam + o_, 4))[0]
    L.camera_update(ctypes.byref(k))
    fields = [("pos", (0x10, 0x14, 0x18), (k.pos.x, k.pos.y, k.pos.z)),
              ("off", (0x2c, 0x30, 0x34), (k.off.x, k.off.y, k.off.z)),
              ("node, flags", (0x38, 0x3c), (k.node, k.flags)),
              ("angles", (0x40, 0x44, 0x48), (k.pitch, k.yaw, k.roll)),
              ("tilt", (0x5c, 0x60, 0x64), tuple(k.tilt)),
              ("eye", (0x6c, 0x70, 0x74), (k.eye.x, k.eye.y, k.eye.z)),
              ("yaw follow", (0x78,), (k.yaw_follow,))]
    for what, offs, got in fields:
        want = tuple(h(x) for x in offs)
        if want != got:
            bad.append("%s camera %s: the original %s, the port %s" % (name, what, want, got))


# the outside cameras run step by step (camera_check_mode 0x6324c, then 0x136b0, as
# race_update_cars does): (name, the mode set at the start or None, steps, the car's
# moves); the car goes along the road from its node, tumbling (state 4) in the crash
VIEWS = [("crash", None, 220), ("crash, put back", None, 160)] + \
        [("mode %d" % m, m, 90) for m in (2, 3, 4, 5, 8, 9, 10, 11, 12, 13)] + \
        [("keys", None, 240), ("keys, crash", None, 220), ("keys, the rival watched", None, 200),
         ("keys, not racing", None, 300), ("keys, not racing, no chase", None, 300)]
# the view key (camera_next 0x1494c) pressed every KEY_EVERY steps in the "keys" runs; not
# racing, the rival and the police put ahead so the car passes them
KEY_EVERY = 13


def node_xyz(L, n):
    return [L.node_i32(n, o) for o in (8, 12, 16)]


def car_at(L, what, k, n0):
    """where the car is at step k: (x, y, z, pitch, yaw, roll, node, dist, state)"""
    n = n0 + k // 2
    x, y, z = node_xyz(L, n)
    head = ((L.node_i32(n, 0x16) >> 16) << 10) & 0xffffff
    across = [0, 0x30000, -0x50000, 0x180000, -0x200000, 0x400000][(k // 15) % 6]
    hx, hz = node_xyz(L, n + 1)[0] - x, node_xyz(L, n + 1)[2] - z
    x += across * (hz >> 8) // 0x600 >> 8 << 8 if hz else 0
    z -= across * (hx >> 8) // 0x600 >> 8 << 8 if hx else 0
    dist = (k % 2) * 0x30000 + 0x1000 * (k % 7)
    tumble = (what.startswith("crash") or what == "keys, crash") and k < (120 if what != "crash, put back" else 70)
    if tumble:
        p, yw, r = (k * 0x54321) & 0xffffff, (head + k * 0x123456) & 0xffffff, (k * 0x234567 + 0x400000) & 0xffffff
        y += (k % 9) * 0x18000
        return x, y, z, p, yw, r, n, dist, 4
    wob = [0, 0x10000, -0x30000, 0x80000, -0x140000][(k // 7) % 5]
    return x, y, z, (k * 0x1000) & 0xffffff if k % 3 else 0, (head + wob) & 0xffffff, 0, n, dist, 2


def views(path, L, bad, stats):
    from cars_emu import RaceCar
    name = os.path.basename(path)
    s0 = Snap(path)
    cam = s0.u32(0x143df4)
    car = s0.u32(0x13c24c + 4 * s0.i32(0x12dd58))
    if struct.unpack("<I", s0.raw(cam + 0x90, 4))[0] != car:
        bad.append("%s: the camera on another car" % name)
        return
    spec = struct.unpack("<I", s0.raw(car + 0x479, 4))[0]
    L.view_init(0, struct.unpack("<i", s0.raw(car + 0x48a, 4))[0],
                struct.unpack("<i", s0.raw(spec + 0x370, 4))[0])
    n0 = struct.unpack("<i", s0.raw(car + 0x48, 4))[0] + 30
    for what, mode, steps in VIEWS:
        s = Snap(path)
        g = lambda o: struct.unpack("<i", s.raw(cam + o, 4))[0]
        gc = lambda o: struct.unpack("<i", s.raw(car + o, 4))[0]
        rc = RaceCar()
        rc.body_roll, rc.body_pitch = gc(0x365), gc(0x369)
        rc.roll_phase, rc.pitch_phase = s.raw(car + 0x427, 1)[0], s.raw(car + 0x428, 1)[0]
        rc.roll_shake, rc.pitch_shake = gc(0x429), gc(0x42d)
        rc.player, rc.number = gc(0x47d), gc(0x48a)
        (ctypes.c_void_p * 2).in_dll(L, "g_cars")[0] = ctypes.addressof(rc)
        ctypes.c_int.in_dll(L, "g_player").value = 0
        crash = ctypes.addressof(ctypes.c_int.in_dll(L, "g_crash"))
        ctypes.c_int.from_address(crash + 32).value = s.i32(0x145c98)
        ctypes.memmove(ctypes.addressof(ctypes.c_int.in_dll(L, "g_cam_modes")), s.read(0xe80f0, 14 * 0x58),
                       14 * 0x58)
        ctypes.c_int.in_dll(L, "g_cam_pitch_bias").value = s.i32(0x12ba20)
        k = Camera()
        k.mode, k.wanted, k.frames = g(0), g(4), g(0xc)
        k.pos = Vec3(g(0x10), g(0x14), g(0x18))
        k.off = Vec3(g(0x2c), g(0x30), g(0x34))
        k.node, k.flags = g(0x38), g(0x3c)
        k.pitch, k.yaw, k.roll = g(0x40), g(0x44), g(0x48)
        for i in range(3):
            k.tilt[i] = g(0x5c + 4 * i)
        k.eye = Vec3(g(0x6c), g(0x70), g(0x74))
        k.mirror_on, k.yaw_follow = g(0x68), g(0x78)
        k.step = Vec3(g(0x7c), g(0x80), g(0x84))
        k.car = ctypes.cast(ctypes.pointer(rc), ctypes.POINTER(Car))
        keys = what.startswith("keys")
        others = []
        if keys:
            # the other racer and the police, standing; not racing, put ahead of the car
            racing = "not racing" not in what
            viewed = 1 if "rival" in what else 0
            chase = 0 if "no chase" in what else 1
            for i, ahead in ((1, 40), (2, 70)):
                a = s.u32(0x13c24c + 4 * i)
                o = RaceCar()
                n = n0 + ahead if not racing else struct.unpack("<i", s.raw(a + 0x48, 4))[0]
                x, y, z = node_xyz(L, n)
                head = ((L.node_i32(n, 0x16) >> 16) << 10) & 0xffffff
                for off, v in ((0, x), (4, y), (8, z), (0xc, 0), (0x10, head), (0x14, 0), (0x48, n)):
                    s.mu.mem_write(a + off, struct.pack("<i", v))
                o.pos, o.yaw, o.node, o.state = Vec3(x, y, z), head, n, 2
                ga = lambda off: struct.unpack("<i", s.raw(a + off, 4))[0]
                o.body_roll, o.body_pitch, o.dist = ga(0x365), ga(0x369), ga(0x39d)
                o.roll_phase, o.pitch_phase = s.raw(a + 0x427, 1)[0], s.raw(a + 0x428, 1)[0]
                o.roll_shake, o.pitch_shake = ga(0x429), ga(0x42d)
                o.number, o.player = struct.unpack("<i", s.raw(a + 0x48a, 4))[0], i
                bits = bytearray(s.raw(a + 0x174, 4))
                if i == 2:
                    bits[1] |= 0x20
                    s.mu.mem_write(a + 0x174, bytes(bits))
                o.bits[:] = list(bits)
                (ctypes.c_void_p * 3).in_dll(L, "g_cars")[i] = ctypes.addressof(o)
                others.append(o)
            for a, v in ((0xf6b74, 1 if racing else 4), (0x12dd54, viewed), (0x145c90, 0), (0x12da34, 1),
                         (0x12dd08, chase), (0x12da9e, 0), (0xe8f68, 1)):
                s.write(a, struct.pack("<i", v))
            L.camera_test_set(1 if racing else 4, viewed, 0, 1, chase, 0, 1)
            if viewed:
                s.call(0x13530, eax=cam, edx=s.u32(0x13c250), ebx=4)
                L.camera_set_car(ctypes.byref(k), ctypes.byref(others[0]), 4)
        ok = True
        for step in range(steps):
            x, y, z, p, yw, r, n, dist, state = car_at(L, what, step, n0)
            for o, v in ((0, x), (4, y), (8, z), (0xc, p), (0x10, yw), (0x14, r), (0x48, n),
                         (0x39d, dist), (0x512, state)):
                s.mu.mem_write(car + o, struct.pack("<i", v))
            rc.pos = Vec3(x, y, z)
            rc.pitch, rc.yaw, rc.roll, rc.node, rc.dist, rc.state = p, yw, r, n, dist, state
            if step == 0 and mode is not None:
                s.call(0x13530, eax=cam, edx=car, ebx=mode)
                L.camera_set_car(ctypes.byref(k), ctypes.byref(rc), mode)
            if keys and step % KEY_EVERY == KEY_EVERY - 1:
                s.call(0x1494c, eax=cam)
                L.camera_next(ctypes.byref(k))
                if os.environ.get("CAM_DEBUG"):
                    print(what, step, "after the key: the original", g(0), g(4), hex(g(0x90)),
                          "viewed", s.i32(0x12dd54), "the port", k.mode, k.wanted)
                stats["keys"] += 1
                if L.camera_test_look_first() != s.i32(0xe8f68):
                    bad.append("%s %s, step %d, the rival or police next: the original %d, the port %d" % (
                        name, what, step, s.i32(0xe8f68), L.camera_test_look_first()))
                    ok = False
            watched = s.u32(0x13c24c + 4 * s.i32(0x12dd54))
            s.call(0x6324c, eax=watched)
            s.call(0x136b0, eax=cam)
            L.camera_check(ctypes.byref(k), ctypes.byref(others[0] if keys and s.i32(0x12dd54) else rc))
            L.camera_update(ctypes.byref(k))
            stats["steps"] += 1
            for field, offs, port in CAM_FIELDS:
                want, got = tuple(g(o) for o in offs), port(k)
                if want != got:
                    bad.append("%s %s, step %d, camera %s: the original %s, the port %s" % (
                        name, what, step, field, want, got))
                    ok = False
            v = ctypes.c_int.from_address(crash + 32).value
            if v != s.i32(0x145c98):
                bad.append("%s %s, step %d, the crash view's wait: the original %d, the port %d" % (
                    name, what, step, s.i32(0x145c98), v))
                ok = False
            stats["modes"].add(g(0))
            if not ok:
                break


def main():
    name, snaps = sys.argv[1], sys.argv[2:]
    L = lib()
    L.files_init(b"NFSSE", None)
    if not L.exe_load():
        sys.exit("no NFS_DOS.EXE")
    if not L.track_load(name.encode()):
        sys.exit("no track")
    for p in snaps:
        bad, stats = [], {"follow": 0, "moved": 0, "steps": 0, "modes": set(), "keys": 0}
        check(p, L, bad, stats)
        views(p, L, bad, stats)
        print("%s: the grid, %d node searches (%d moved), the camera, %d steps outside (modes %s, "
              "the view key %d times): %s" % (
            os.path.basename(p), stats["follow"], stats["moved"], stats["steps"],
            sorted(stats["modes"]), stats["keys"], "all equal" if not bad else "%d differ" % len(bad)))
        for b in bad[:12]:
            print("  " + b)


if __name__ == "__main__":
    main()
