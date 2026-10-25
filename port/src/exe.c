/* NFS_DOS.EXE: a DOS/4GW stub, then an LE image (NOTES §2). Its objects are runs of
   4096-byte pages; each page has relocation records, the places that hold one of the
   program's own addresses. Only "32-bit offset" records (source type 7) are flat
   addresses; the port applies those, as tools/unle.py --fixups does. */
#include "exe.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_LEN 0x169a10u            /* code 0x10000.., data + BSS 0xe0000..0x169a10 */
/* FNV-1a of the image, for the two builds known, both of 1,254,591 bytes: 19 June 1996
   and 17 June 1996 (the 1997 EA CD). They differ only in the give-up branch of 0x48bec and
   one German text (0xe3814), neither read here (NOTES §1) */
static const unsigned image_sums[] = { 0x76078ed2u, 0x21ef7c33u };
#define MAX_PAGES 1024

static unsigned char *g_image;

static unsigned u32(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static unsigned u16(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static unsigned fnv(const unsigned char *p, unsigned n)
{
    unsigned h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

/* the objects' pages at their addresses */
static int place_pages(const unsigned char *d, unsigned long len, unsigned le,
                       unsigned char *img, unsigned *page_va, unsigned *base)
{
    unsigned npages = u32(d + le + 0x14), objtab = le + u32(d + le + 0x40),
             nobj = u32(d + le + 0x44), datapages = u32(d + le + 0x80), i, p;
    if (nobj > 7 || npages > MAX_PAGES || u32(d + le + 0x28) != 4096)
        return 0;
    for (i = 0; i < nobj; i++) {
        const unsigned char *o = d + objtab + i * 24;
        unsigned b = u32(o + 4), first = u32(o + 0xc), n = u32(o + 0x10);
        base[i + 1] = b;
        for (p = 0; p < n; p++) {
            unsigned page = first - 1 + p, at = datapages + page * 4096, va = b + p * 4096;
            if (page >= npages || va + 4096 > IMAGE_LEN + 4096)
                return 0;
            page_va[page] = va;
            if (at < len) {                            /* (the last page is short) */
                unsigned k = at + 4096 <= len ? 4096 : (unsigned)(len - at);
                if (va + k > IMAGE_LEN)
                    k = IMAGE_LEN - va;
                memcpy(img + va, d + at, k);
            }
        }
    }
    return 1;
}

static int relocate(const unsigned char *d, unsigned long len, unsigned le,
                    unsigned char *img, const unsigned *page_va, const unsigned *base)
{
    unsigned npages = u32(d + le + 0x14), fpt = le + u32(d + le + 0x68),
             frt = le + u32(d + le + 0x6c), page;
    for (page = 0; page < npages; page++) {
        unsigned o = frt + u32(d + fpt + 4 * page), end = frt + u32(d + fpt + 4 * page + 4);
        while (o < end && end <= len) {
            unsigned src = d[o], flags = d[o + 1], n = 1, obj, target = 0, list, k;
            int one = 0;
            o += 2;
            if (flags & 3)
                return 0;                              /* not a kind this program has */
            if (src & 0x20)
                n = d[o++];
            else {
                one = (short)u16(d + o);
                o += 2;
            }
            if (flags & 0x40) {
                obj = u16(d + o);
                o += 2;
            } else
                obj = d[o++];
            if ((src & 0x0f) != 2) {
                if (flags & 0x10) {
                    target = u32(d + o);
                    o += 4;
                } else {
                    target = u16(d + o);
                    o += 2;
                }
            }
            list = o;
            if (src & 0x20)
                o += 2 * n;
            if ((src & 0x0f) != 7 || obj > 7)
                continue;
            for (k = 0; k < n; k++) {
                int at = (src & 0x20) ? (short)u16(d + list + 2 * k) : one;
                unsigned va = page_va[page] + (unsigned)at, v = base[obj] + target;
                if (at < 0 || va + 4 > IMAGE_LEN)      /* (at < 0: done from the page before) */
                    continue;
                img[va] = (unsigned char)v;
                img[va + 1] = (unsigned char)(v >> 8);
                img[va + 2] = (unsigned char)(v >> 16);
                img[va + 3] = (unsigned char)(v >> 24);
            }
        }
    }
    return 1;
}

int exe_load(void)
{
    static unsigned page_va[MAX_PAGES];
    unsigned base[8] = {0}, le;
    long len;
    unsigned char *d, *img;
    int ok;

    if (g_image)
        return 1;
    d = file_load("NFS_DOS.EXE", &len);
    if (!d) {
        files_error("NFS_DOS.EXE is missing from the game's folder. Copy the whole CD of The Need for Speed SE, every file and folder.");
        return 0;
    }
    le = len > 0x40 ? u32(d + 0x3c) : 0;
    if (d[0] != 'M' || d[1] != 'Z' || le + 0xb0 > (unsigned long)len || d[le] != 'L' || d[le + 1] != 'E') {
        files_error("NFS_DOS.EXE in the game's folder is not the game's program.");
        free(d);
        return 0;
    }
    img = calloc(1, IMAGE_LEN);
    ok = img && place_pages(d, (unsigned long)len, le, img, page_va, base) &&
         relocate(d, (unsigned long)len, le, img, page_va, base);
    free(d);
    if (ok) {
        unsigned sum = fnv(img, IMAGE_LEN), i;
        for (i = 0, ok = 0; i < sizeof image_sums / sizeof image_sums[0]; i++)
            ok |= sum == image_sums[i];
    }
    if (!ok) {
        files_error("NFS_DOS.EXE in the game's folder is not the version ReNFS knows: it needs the CD of The Need for Speed Special Edition (DOS).");
        free(img);
        return 0;
    }
    g_image = img;
    return 1;
}

const unsigned char *exe_at(unsigned addr, unsigned len)
{
    if (!g_image || addr > IMAGE_LEN || len > IMAGE_LEN - addr)
        return 0;
    return g_image + addr;
}

int exe_i32(unsigned addr)
{
    const unsigned char *p = exe_at(addr, 4);
    return p ? (int)u32(p) : 0;
}

/* 257 entries, a quarter wave: the second quarter read backwards, the second half negated */
int fix_sin(int a)
{
    int i = a & 0xff, v;
    if (a & 0x100)
        i = 0x100 - i;
    v = exe_i32(0xf8448 + 4 * i);
    return a & 0x200 ? -v : v;
}

int fix_cos(int a)
{
    return fix_sin(a + 0x100);
}

/* EA's 0x80b00: between two entries of the table, the slope from the cosine there
   (2 pi in 16.16 = 0x6487e, a step of the table = 64 of a's) */
int ea_sin(int a)
{
    int i = a >> 6, f = (a & 0x3f) * 0x6487e >> 9;
    return fix_sin(i) + ((fix_cos(i) >> 2) * f >> 21);
}

/* 0x80b70 */
int ea_cos(int a)
{
    int i = a >> 6, f = (a & 0x3f) * 0x6487e >> 9;
    return fix_cos(i) + ((-fix_sin(i) >> 2) * f >> 21);
}

/* 0x927b0: the smaller over the larger as a 32-bit fraction, into the table of an
   eighth of a turn (0x125db8, 257 entries), then the octant */
int ea_atan2(int y, int x)
{
    int oct = 0, a;
    unsigned small, large, ratio, i, f;
    if (x < 0) {
        oct = 2;
        x = (int)(0u - (unsigned)x);
    }
    if (y < 0) {
        oct |= 4;
        y = (int)(0u - (unsigned)y);
    }
    if (y == x)
        a = 0x2000;
    else {
        small = (unsigned)y;
        large = (unsigned)x;
        if (x < y) {
            oct |= 1;
            small = (unsigned)x;
            large = (unsigned)y;
        }
        ratio = (unsigned)(((unsigned long long)small << 32) / large);
        i = ratio >> 24;
        f = ratio >> 8 & 0xffff;
        a = exe_i32(0x125db8 + 4 * i);
        a += (int)((unsigned)(exe_i32(0x125dbc + 4 * i) - a) * f >> 16);
    }
    switch (oct) {
    case 1: return 0x4000 - a;
    case 2: return 0x8000 - a;
    case 3: return a + 0x4000;
    case 4: return -a;
    case 5: return a - 0x4000;
    case 6: return a - 0x8000;
    case 7: return -a - 0x4000;
    }
    return a;
}

/* EA's arctangent (0x8bb7c): a turn of 1024, from the y axis towards x; the smaller over
   the larger as a 32-bit fraction, rounded to 1/256, into the table of an eighth of a
   turn (0x12572c) */
int ea_atan_1024(int x, int y)
{
    unsigned ax = (unsigned)(x < 0 ? -x : x), ay = (unsigned)(y < 0 ? -y : y), r;
    int f = (y < 0 ? 8 : 0) | (x < 0 ? 0x10 : 0), t;

    if (ax == ay)
        t = 0x80;
    else {
        if (ax > ay) {
            unsigned w = ax;
            ax = ay;
            ay = w;
            f |= 4;
        }
        r = (unsigned)(((unsigned long long)ax << 32) / ay);
        t = *exe_at(0x12572c + (r >> 24) + (r >> 23 & 1), 1);
    }
    switch (f) {
    case 4:
        return 0x100 - t;
    case 8:
        return 0x200 - t;
    case 0xc:
        return t + 0x100;
    case 0x10:
        return -t;
    case 0x14:
        return t - 0x100;
    case 0x18:
        return t - 0x200;
    case 0x1c:
        return -t - 0x100;
    }
    return t;
}
